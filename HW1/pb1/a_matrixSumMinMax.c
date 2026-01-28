/* matrix summation and min and max search using pthreads

   features: uses a barrier; the Worker[0] computes
             the total sum min and max from partial sums computed by Workers
             and prints them to the standard output

   usage under Linux:
gcc -o SumMinMax a_matrixSumMinMax.c -lpthread && ./SumMinMax 9 3

*/
#ifndef _REENTRANT 
#define _REENTRANT 
#endif 
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h> // for INT_MAX and INT_MIN

#define MAXSIZE 10000  /* maximum matrix size */
#define MAXWORKERS 10   /* maximum number of workers */

pthread_mutex_t barrier;  /* mutex lock for the barrier */
pthread_cond_t go;        /* condition variable for leaving */
int numWorkers;           /* number of workers */ 
int numArrived = 0;       /* number who have arrived */

/* a reusable counter barrier */
void Barrier() {
  pthread_mutex_lock(&barrier);
  numArrived++;
  if (numArrived == numWorkers) {
    numArrived = 0;
    pthread_cond_broadcast(&go);
  } else
    pthread_cond_wait(&go, &barrier);
  pthread_mutex_unlock(&barrier);
}

/* timer */
double read_timer() {
    static bool initialized = false;
    static struct timeval start;
    struct timeval end;
    if( !initialized )
    {
        gettimeofday( &start, NULL );
        initialized = true;
    }
    gettimeofday( &end, NULL );
    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
}

double start_time, end_time; /* start and end times */
int size, stripSize;  /* assume size is multiple of numWorkers */
int sums[MAXWORKERS]; /* partial sums */
int partial_min[MAXWORKERS]; /* partial mins */
int partial_max[MAXWORKERS]; /* partial maxs */
int minRow[MAXWORKERS]; 
int minCol[MAXWORKERS]; 
int maxRow[MAXWORKERS]; 
int maxCol[MAXWORKERS];
int matrix[MAXSIZE][MAXSIZE]; /* matrix */

void *Worker(void *);

/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {
  int i, j;
  long l; /* use long in case of a 64-bit system */
  pthread_attr_t attr;
  pthread_t workerid[MAXWORKERS];

  /* set global thread attributes */
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  /* initialize mutex and condition variable */
  pthread_mutex_init(&barrier, NULL);
  pthread_cond_init(&go, NULL);

  /* read command line args if any */
  size = (argc > 1)? atoi(argv[1]) : MAXSIZE;
  numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
  if (size > MAXSIZE) size = MAXSIZE;
  if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;
  stripSize = size/numWorkers;

  /* initialize the matrix WITH RANDOM VALUES*/
  srand(time(NULL)); // seed for the random values
  for (i = 0; i < size; i++) {
	  for (j = 0; j < size; j++) {
          matrix[i][j] = rand()%99;
	  }
  }

  /*print matrix*/
  for (i = 0; i < size; i++) {
	  printf("[ ");
	  for (j = 0; j < size; j++) {
	    printf(" %d", matrix[i][j]);
	  }
	  printf(" ]\n");
  }

  printf("Matrix size : %d x %d and has %d workers\n", size, size, numWorkers);
  printf("Decomposition size : %d x %d\n\n", stripSize, size);  

  /* SEQUENTIAL VERIFICATION OF RESULTS*/
    start_time = read_timer();
    int seq_sum = 0;
    int seq_min = INT_MAX;
    int seq_max = INT_MIN;
    int seq_min_row = 0, seq_min_col = 0; 
    int seq_max_row = 0, seq_max_col = 0;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            int val = matrix[i][j];
            seq_sum += val;
            if (val < seq_min) {
                seq_min = val;
                seq_min_row = i; 
                seq_min_col = j; 
            }
            if (val > seq_max) {
                seq_max = val;
                seq_max_row = i; 
                seq_max_col = j;
            }
        }
    }
    end_time = read_timer();
    printf("\n======SEQUENTIAL VERIFICATION OF RESULTS======\n");
    printf("The total is %d\n", seq_sum);
    printf("The global min is %d at (%d,%d)\n", seq_min, seq_min_row, seq_min_col); 
    printf("The global max is %d at (%d,%d)\n", seq_max, seq_max_row, seq_max_col); 
    printf("The execution time is %g sec\n", end_time - start_time);
    printf("=============================================\n");  

  /* do the parallel work: create the workers */
  start_time = read_timer();
  for (l = 0; l < numWorkers; l++)
    pthread_create(&workerid[l], &attr, Worker, (void *) l);

  pthread_exit(NULL);
}

/* Each worker sums the values in one strip of the matrix.
  They also compute min and max in one strip of the matrix.
   After a barrier, worker(0) computes and prints the total
   and also finds global min and max and prints them */
void *Worker(void *arg) {
  long myid = (long) arg;
  int total, i, j, first, last, strip_min, strip_max;
  int strip_min_row, strip_min_col;
  int strip_max_row, strip_max_col; 

  printf("\nWorker %ld (pthread id %ld) has started\n", myid, pthread_self());

  /*start printing strip matrix of this worker */
  printf("[");

 /* determine first and last rows of my strip */
  first = myid*stripSize;
  last = (myid == numWorkers - 1) ? (size - 1) : (first + stripSize - 1);

  /* find min and max of the strip*/
  strip_min = matrix[first][0]; 
  strip_max = matrix[first][0];
  strip_min_row = first; 
  strip_min_col = 0;     
  strip_max_row = first; 
  strip_max_col = 0;     

  for (i = first; i <= last; i++)
    for (j = 0; j < size; j++) {
      printf(" %d", matrix[i][j]);
      if (matrix[i][j] < strip_min) {
        strip_min = matrix[i][j];
        strip_min_row = i; 
        strip_min_col = j;
      }
      if (matrix[i][j] > strip_max) {
        strip_max = matrix[i][j];
        strip_max_row = i;
        strip_max_col = j;
      }
    }
  printf(" ]\n");

  partial_min[myid] = strip_min;
  partial_max[myid] = strip_max;
  minRow[myid] = strip_min_row; 
  minCol[myid] = strip_min_col; 
  maxRow[myid] = strip_max_row; 
  maxCol[myid] = strip_max_col;

  printf("Worker %ld: strip min is %d at (%d,%d)\n", myid, strip_min, strip_min_row, strip_min_col); 
  printf("Worker %ld: strip max is %d at (%d,%d)\n", myid, strip_max, strip_max_row, strip_max_col);

  /* sum values in my strip */
  total = 0;
  for (i = first; i <= last; i++)
    for (j = 0; j < size; j++)
      total += matrix[i][j];
  sums[myid] = total;

  /*stop all workers to be in sync*/
  Barrier();

  /* worker 0 computes the total sum and global min and max */
  if (myid == 0) {
    total = 0;
    int global_min = partial_min[0];
    int global_max = partial_max[0];
    int global_min_row = minRow[0];
    int global_min_col = minCol[0]; 
    int global_max_row = maxRow[0]; 
    int global_max_col = maxCol[0];

    /*compute global min and max*/
    for (i = 0; i < numWorkers; i++) {
      if (partial_min[i] < global_min) {
        global_min = partial_min[i];
        global_min_row = minRow[i]; 
        global_min_col = minCol[i]; 
      }
      if (partial_max[i] > global_max) {
        global_max = partial_max[i];
        global_max_row = maxRow[i]; 
        global_max_col = maxCol[i]; 
      }
    }

    for (i = 0; i < numWorkers; i++)
      total += sums[i];
    /* get end time */
    end_time = read_timer();


    /* print results */
    printf("\n======RESULTS======\n");
    printf("The total is %d\n", total);
    printf("The global min is %d at (%d,%d)\n", global_min, global_min_row, global_min_col); 
    printf("The global max is %d at (%d,%d)\n", global_max, global_max_row, global_max_col); 
    printf("The execution time is %g sec\n", end_time - start_time);
    printf("===================\n");
  }

  pthread_exit(NULL);
}
