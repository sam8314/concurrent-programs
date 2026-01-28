/* matrix summation and min and max search using pthreads

   features: uses a barrier; the Worker[0] computes
             the total sum min and max from partial sums computed by Workers
             and prints them to the standard output

   usage under Linux:
gcc -o noBarriersNoArray b_noBarriersNoArray.c -lpthread && ./noBarriersNoArray 9 3

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

pthread_mutex_t result_mutex; // mutex for global result variables
int global_sum = 0;           // global sum protected by result_mutex
int global_min = INT_MAX; // global min protected by result_mutex
int global_max = INT_MIN; // global max protected by result_mutex
int global_min_row = 0;   // row position of global min
int global_min_col = 0;   // column position of global min
int global_max_row = 0;   // row position of global max
int global_max_col = 0;   // column position of global max
bool all_workers_done = false; 

int numWorkers, size, stripSize;
int matrix[MAXSIZE][MAXSIZE];

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

void *Worker(void *);
/* read command line, initialize, and create threads */
int main(int argc, char *argv[]) {
  int i, j;
  long w; /// w for worker
  pthread_attr_t attr;
  pthread_t workerid[MAXWORKERS];

  /* set global thread attributes */
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  /* initialize mresult mutex */
  pthread_mutex_init(&result_mutex, NULL);

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

  /* do the parallel work: create the workers */
  start_time = read_timer();
  pthread_attr_init(&attr);
  pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

  for (w = 0; w < numWorkers; w++)
    pthread_create(&workerid[w], &attr, Worker, (void *) w);
  
    
  /*wait for children to finish their work*/
  printf("\nMain thread: waiting for %d workers to finish...\n", numWorkers);
  for (w = 0; w < numWorkers; w++)
    pthread_join(workerid[w], NULL);

  end_time = read_timer();
  printf("\n======RESULTS======\n");
  printf("The total is %d\n", global_sum);
  printf("The global min is %d at (%d,%d)\n", global_min, global_min_row, global_min_col);
  printf("The global max is %d at (%d,%d)\n", global_max, global_max_row, global_max_col);
  printf("The execution time is %g sec\n", end_time - start_time);
  printf("=====================\n");

  pthread_mutex_destroy(&result_mutex);

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

  pthread_exit(NULL);
}

/* Each worker sums the values in one strip of the matrix.
  They also compute min and max in one strip of the matrix.
   After a barrier, worker(0) computes and prints the total
   and also finds global min and max and prints them */
void *Worker(void *arg) {
    long myid = (long) arg;
    int my_sum = 0, my_min = INT_MAX, my_max = INT_MIN;
    int my_min_row = 0, my_min_col = 0;
    int my_max_row = 0, my_max_col = 0;
    int i, j, first, last;

    printf("\nWorker %ld (pthread id %ld) has started\n", myid, pthread_self());

    /* determine first and last rows of my strip */
    first = myid*stripSize;
    last = (myid == numWorkers - 1) ? (size - 1) : (first + stripSize - 1);

    /*print strip matrix of this worker */
    printf("[");
    for (i = first; i <= last; i++)
    for (j = 0; j < size; j++) {
        printf(" %d", matrix[i][j]);
    }
    printf(" ]\n");

    /* find min, max and sum of the strip*/
    for (i = first; i <= last ; i++){
        for (j = 0; j < size ; j++){
            int val = matrix[i][j];
            my_sum += val;
            if (val < my_min) {
                my_min = val;
                my_min_row = i;
                my_min_col = j;
            }
            if (val > my_max) {
                my_max = val;
                my_max_row = i;
                my_max_col = j;
            }
        }
    }

    /* print strip min and max*/
    printf("Worker %ld: strip min is %d at (%d,%d)\n", myid, my_min, my_min_row, my_min_col);
    printf("Worker %ld: strip max is %d at (%d,%d)\n", myid, my_max, my_max_row, my_max_col);
    printf("Worker %ld: strip sum is %d\n", myid, my_sum);

    /*atomically update global results CRITICAL SECTION*/
    pthread_mutex_lock(&result_mutex);
    global_sum += my_sum;
    if (my_min < global_min) {
        global_min = my_min;
        global_min_row = my_min_row;
        global_min_col = my_min_col;
    }
    if (my_max > global_max) {
        global_max = my_max;
        global_max_row = my_max_row;
        global_max_col = my_max_col;
    }
    pthread_mutex_unlock(&result_mutex);  

    pthread_exit(NULL);
}
