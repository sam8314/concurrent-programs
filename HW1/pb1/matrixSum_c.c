/* modified b part of the assignment with bag of tasks concept. a shared row counter is initialized and workers continuiously fetch/increment to dinamycally decide which row to process next */
#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>

#define MAXSIZE 10000 /* max matrix size*/
#define MAXWORKERS 10 /* max number of workers*/

/* shared input */
int size, numWorkers;
int matrix[MAXSIZE][MAXSIZE];

/* bag of tasks  */
int row_counter = 0;                 /* next row to process: each row is a task, and threads take tasks dinamically by incrementing the shared row counter under a lock */

/* shared results (same style as part b) */
pthread_mutex_t result_lock;
int globalSum;
int globalMin, globalMax;
int globalMinRow, globalMinCol;
int globalMaxRow, globalMaxCol;

/* timer */
double read_timer() {
    static bool initialized = false;
    static struct timeval start;
    struct timeval end;

    if (!initialized) {
        gettimeofday(&start, NULL);
        initialized = true;
    }
    gettimeofday(&end, NULL);

    return (end.tv_sec - start.tv_sec) + 1.0e-6 * (end.tv_usec - start.tv_usec);
}

double start_time, end_time;

void *Worker(void *arg);

int main(int argc, char *argv[]) {
    int i, j;
    long l;
    pthread_t workerid[MAXWORKERS];
    pthread_attr_t attr;

    /* read command line args */
    size = (argc > 1) ? atoi(argv[1]) : MAXSIZE;
    numWorkers = (argc > 2) ? atoi(argv[2]) : MAXWORKERS;
    if (size > MAXSIZE) size = MAXSIZE;
    if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;

    /* init matrix with random values (0..98) */
    srand((unsigned)time(NULL));
    for (i = 0; i < size; i++) {
        for (j = 0; j < size; j++) {
            matrix[i][j] = rand() % 99;
        }
    }

    /* init lock */
    pthread_mutex_init(&result_lock, NULL);

    /* init row counter + global results (done before threads start) */
    row_counter = 0;

    globalSum = 0;
    globalMin = matrix[0][0];
    globalMax = matrix[0][0];
    globalMinRow = 0; globalMinCol = 0;
    globalMaxRow = 0; globalMaxCol = 0;

    /* thread attributes */
    pthread_attr_init(&attr);
    pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

    /* timing: start after init, just before creating threads */
    start_time = read_timer();

    for (l = 0; l < numWorkers; l++) {
        pthread_create(&workerid[l], &attr, Worker, (void *)l);
    }

    /* main waits for workers to terminate before printing */
    for (l = 0; l < numWorkers; l++) {
        pthread_join(workerid[l], NULL);
    }

    /* timing ends after all workers terminated */
    end_time = read_timer();

    /* main prints final results */
    printf("The total is %d\n", globalSum);
    printf("Min = %d at (%d,%d)\n", globalMin, globalMinRow, globalMinCol);
    printf("Max = %d at (%d,%d)\n", globalMax, globalMaxRow, globalMaxCol);
    printf("The execution time is %g sec\n", end_time - start_time);

  
    pthread_mutex_destroy(&result_lock);
    return 0;
}

/* worker: repeatedly pull a row index from the bag and process it. */
void *Worker(void *arg) {
    (void)arg;

    while (1) {
        int row;
        pthread_mutex_lock(&result_lock);

        /* get a task (row number)*/
        row = row_counter;
        row_counter++;
        pthread_mutex_unlock(&result_lock);

        if (row >= size) break; /* bag emtpy, thread exits */

        /* compute local results for this row (no locks to maintain parallelism) */
        long long localSum = 0;
        int localMin = INT_MAX, localMax = INT_MIN;
        int localMinCol = 0, localMaxCol = 0;

        for (int col = 0; col < size; col++) {
            int val = matrix[row][col];
            localSum += val;

            if (val < localMin) {
                localMin = val;
                localMinCol = col;
            }
            if (val > localMax) {
                localMax = val;
                localMaxCol = col;
            }
        }

        /* update shared globals (critical section) */
        pthread_mutex_lock(&result_lock);

        globalSum += localSum;

        if (localMin < globalMin) {
            globalMin = localMin;
            globalMinRow = row;
            globalMinCol = localMinCol;
        }

        if (localMax > globalMax) {
            globalMax = localMax;
            globalMaxRow = row;
            globalMaxCol = localMaxCol;
        }

        pthread_mutex_unlock(&result_lock);
    }

    return NULL;
}
