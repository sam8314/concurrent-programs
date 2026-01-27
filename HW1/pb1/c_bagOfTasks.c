/* matrix summation and min and max search using pthreads

   features: uses a barrier; the Worker[0] computes
             the total sum min and max from partial sums computed by Workers
             and prints them to the standard output

   usage under Linux:
    gcc -o bagOfTasks c_bagOfTasks.c -lpthread && ./bagOfTasks 9 3

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
int row_counter = 0;          // bag of tasks : next row to process (atom f&i)
int global_sum = 0;           // global sum protected by result_mutex
int global_min = INT_MAX; // global min protected by result_mutex
int global_max = INT_MIN; // global max protected by result_mutex
bool all_workers_done = false; 

int numWorkers, size;
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

  /* initialize mresult mutex */
  pthread_mutex_init(&result_mutex, NULL);

  /* read command line args if any */
  size = (argc > 1)? atoi(argv[1]) : MAXSIZE;
  numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
  if (size > MAXSIZE) size = MAXSIZE;
  if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;

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
  printf("Bag of tasks : row_counter = %d\n\n", row_counter);  

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
  printf("The global min is %d\n", global_min);
  printf("The global max is %d\n", global_max);
  printf("The execution time is %g sec\n", end_time - start_time);
  printf("=====================\n");

  pthread_mutex_destroy(&result_mutex);

  /* SEQUENTIAL VERIFICATION OF RESULTS*/
    start_time = read_timer();
    int seq_sum = 0;
    int seq_min = INT_MAX;
    int seq_max = INT_MIN;
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            int val = matrix[i][j];
            seq_sum += val;
            if (val < seq_min)
                seq_min = val;
            if (val > seq_max)
                seq_max = val;
        }
    }
    end_time = read_timer();
    printf("\n======SEQUENTIAL VERIFICATION OF RESULTS======\n");
    printf("The total is %d\n", seq_sum);
    printf("The global min is %d\n", seq_min);
    printf("The global max is %d\n", seq_max);
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

    printf("\nWorker %ld (pthread id %ld) has started\n", myid, pthread_self());
    while (true){
        int row = __sync_fetch_and_add(&row_counter, 1);
        if (row >= size) {
            printf("Worker %ld : no more rows (row_counter=%d)\n", myid, row_counter);
            break;
        }

        /*new row*/
        int row_sum = 0;
        int row_min = INT_MAX;
        int row_max = INT_MIN;
        int col;

        /* process row */
        printf("Worker %ld processing row %d\n", myid, row);
        for (col = 0; col < size; col++) {
            int val = matrix[row][col];
            row_sum += val;
            if (val < row_min)
                row_min = val;
            if (val > row_max)
                row_max = val;
        }
        printf("Worker %ld done row %d, sum=%d, min=%d, max=%d\n", myid, row, row_sum, row_min, row_max);

        // atomic update of global results CRITICAL SECTION
        pthread_mutex_lock(&result_mutex);
        global_sum += row_sum;
        if (row_min < global_min)
            global_min = row_min;
        if (row_max > global_max)
            global_max = row_max;
        pthread_mutex_unlock(&result_mutex);
    }
    printf("Worker %ld exiting\n", myid);
    pthread_exit(NULL);
}

