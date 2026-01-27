/* matrix summation using pthreads
features: uses a mutex lock to protect shared variables; the Worker[0] computes
the total sum from partial sums computed by Workers
and prints the total sum to the standard output
usage under Linux:
gcc matrixSum.c -lpthread
a.out size numWorkers
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
#define MAXSIZE 10000 /* maximum matrix size */
#define MAXWORKERS 10 /* maximum number of workers */
pthread_mutex_t lock; /* mutex lock to protect shared globals*/
int numWorkers; /* number of workers */

/* timer */
double read_timer() {
    static bool initialized = false;
    static struct timeval start;
    struct timeval end;
    if( !initialized ) {
        gettimeofday( &start, NULL );
        initialized = true;
        }
    gettimeofday( &end, NULL );
    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
    }

double start_time, end_time; /* start and end times */
int size, stripSize; /* assume size is multiple of numWorkers */
int globalSum;
int globalMin, globalMinRow, globalMinCol;
int globalMax, globalMaxRow, globalMaxCol;

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

    /* initialize mutex before creating threads*/
    pthread_mutex_init(&lock, NULL);

    /* read command line args if any */
    size = (argc > 1)? atoi(argv[1]) : MAXSIZE;
    numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
    if (size > MAXSIZE) size = MAXSIZE;
    if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;
    stripSize = size/numWorkers;

    /* initialize the matrix */
    srand(time(NULL));
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            matrix[i][j] = rand() %99; //rand()%99;
        }
    }
    /* print the matrix */
    #ifdef DEBUG
    for (i = 0; i < size; i++) {
        printf("[ ");
        for (j = 0; j < size; j++) {
            printf(" %d", matrix[i][j]);
        }
    printf(" ]\n");
    }
    #endif
 /* intialized shared global variables before any thread updates them*/
    globalSum = 0;
    globalMin = matrix[0][0];
    globalMax = matrix[0][0];
    globalMinRow = 0; 
    globalMinCol = 0;
    globalMaxRow = 0;
    globalMaxCol = 0;
    start_time = read_timer();
    for (l = 0; l < numWorkers; l++)
        pthread_create(&workerid[l], &attr, Worker, (void *) l);
    for (l=0; l< numWorkers;l++)
        pthread_join(workerid[l], NULL); /* instead of wait condition variable, the main thread waits after having finished creatinf all worker threads with their respective worker id, so that it can print the final results only after everyoen is finished*/
    end_time= read_timer();

    printf("The total is %d\n", globalSum);
    printf("Min = %d at %d,%d,\n", globalMin, globalMinRow,globalMinCol);
    printf("Max = %d at %d,%d,\n", globalMax, globalMaxRow,globalMaxCol);

    printf("The execution time is %g sec\n", end_time - start_time);
    return 0;
}


/* Each worker sums the values in one strip of the matrix.
After a barrier, worker(0) computes and prints the total */
void *Worker(void *arg) {
    long myid = (long) arg;
    int total, i, j, first, last;
    #ifdef DEBUG
    printf("worker %d (pthread id %d) has started\n", myid, pthread_self());
    #endif

    /* determine first and last rows of my strip. computes which rows of the matrix it is responsible for based on its workerId and stripSize;*/
    first = (int)myid*stripSize;
    last = (myid == numWorkers - 1) ? (size - 1) : (first + stripSize - 1);
    /* sum values in my strip */
    total = 0;
    int localMin = matrix[first][0];
    int localMax = matrix[first][0];
    int localMinRow = first, localMinCol = 0;
    int localMaxRow = first, localMaxCol = 0;
/*iterates over the assigned rows and calculates local sum, local minimum and local maximum, including their idexes;*/
    for (i = first; i <= last; i++) {
    for (j = 0; j < size; j++) {
        int val = matrix[i][j];
        total += val;
        if (val < localMin) {
            localMin = val;
            localMinRow = i;
            localMinCol = j;
        }
        if (val > localMax) {
            localMax = val;
            localMaxRow = i;
            localMaxCol = j;
        }
    }
}
        pthread_mutex_lock(&lock); /* placed so that shared global variabels are updated atomically, ie 2 theeads cant access them at the same time*/
        /*store the partial results in shared global variables inside a critical section.
*/
    globalSum += total;
    if(localMin < globalMin) {
        globalMin = localMin;
        globalMinRow = localMinRow;
        globalMinCol = localMinCol;
        }
    if(localMax > globalMax) {
        globalMax = localMax;
        globalMaxRow = localMaxRow;
        globalMaxCol = localMaxCol;
    }
    pthread_mutex_unlock(&lock); /*unlock mutex to allow other threads to proceed */

    pthread_exit (NULL);

}