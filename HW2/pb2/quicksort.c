/*
The quicksort algorithm sorts the list of numbers by first dividing the list into two sublists so that all the numbers if one 
sublist are smaller than all the numbers in the other sublist. This is done by selecting one number (called a pivot) against 
which all other numbers are compared: the numbers which are less than the pivot are placed in one sublist, and the numbers which
 more than the pivot are placed in another sublist. The pivot can be either placed in one sublist or withheld and placed in its 
 final position.

Develop a parallel multithreaded program (in C/C++ using OpenMP tasks) with recursive parallelism that implements the quicksort 
algorithm for sorting an array of n values.

Run the program on different numbers of processors and report the speedup (sequential execution time divided by parallel execution 
time) for different numbers of processors (up to at least 4) and different workloads (at least 3 different lists of various sizes). 
Run each program several (at least 5)  times and use the median value for execution time. Try to provide reasonable explanations 
for your results. Measure only the parallel part of your program. Specify the number of processors used by specifying a different 
number of threads (set the OMP_NUM_THREADS environment variable or use a call to omp_set_num_threads(), see the OpenMP specification).
 To measure the execution time, use the omp_get_wtime function (see omp_get_wtimeLinks to an external site.).
*/

/* quick sort algorithm using OpenMP

   usage with gcc (version 4.2 or higher required):
     gcc -O -fopenmp -o q quicksort.c 
     ./q size numWorkers
*/

#include <omp.h>

double start_time, end_time;

#include <stdio.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#define MAXSIZE 10000  /* maximum list size */
#define MAXWORKERS 8   /* maximum number of workers */

int numWorkers;
int size; 
int list[MAXSIZE];

/* HELPER FUNCTIONS */
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
int compare(const void *a, const void *b) {
    return (*(int *)a - *(int *)b);  
}
double findMedian(double arr[], int n) {
    qsort(arr, n, sizeof(int), compare);

  	// If even, median is the average of the two
  	// middle elements
    if (n % 2 == 0) {
        return (arr[n / 2 - 1] + arr[n / 2]) / 2.0;
    }
  
  	// If odd, median is the middle element
  	else {
        return arr[n / 2];
    }
}

/* QUICKSORT ALGOs*/
double sequential(bool print, int list[], int size) {
  /* SEQUENTIAL VERIFICATION OF RESULTS*/
  start_time = omp_get_wtime();

  qsort(list, size, sizeof(int), compare); // TODO : actually rewrite sequential quick sort ???

  end_time = omp_get_wtime();

  if(print){  
    printf("\n==============SEQUENTIAL RESULTS================\n");
    printf("The execution time is %g sec\n", end_time - start_time);
    printf("===============================================\n"); 
  }
  return end_time - start_time;
}

double parallel(bool print, int list[], int size) {
  /* PARALLELE WORK*/
  start_time = omp_get_wtime();

  #pragma omp parallel
  {
    #pragma omp single
    {
      qsort(list, size, sizeof(int), compare); // TODO : actually rewrite parallel quick sort with tasks ???
    }
  }

  end_time = omp_get_wtime();

  if(print){  
    printf("\n==============PARALLEL RESULTS================\n");
    printf("The execution time is %g sec\n", end_time - start_time);
    printf("===============================================\n"); 
  }
  return end_time - start_time;
}


/* MAIN THREAD */
int main(int argc, char *argv[]) {
  int i, j, total=0;

  /* GIVE FULL CONFIG AND SEE ACTUAL RESULTS */
  if (argc > 2){
    size = atoi(argv[1]);
    numWorkers = (argc > 2)? atoi(argv[2]) : MAXWORKERS;
    if (size > MAXSIZE) size = MAXSIZE;
    if (numWorkers > MAXWORKERS) numWorkers = MAXWORKERS;
    /* initialize the list sequentially*/
    srand(time(NULL));
    for (i = 0; i < size; i++) {
      //printf("[ ");
        list[i] = rand()%999;
        //printf(" %d", list[i]);
    }
      //printf(" ]\n");
    
    double par = parallel(true, list, size);
    double seq = sequential(true, list, size);
  }

  /* store runtime time, speedup in output file with different sizes and number of workers
    run it 5 times and take median values for runtimes
    matrix size follows 10, 100, 1000, ..., MAXSIZE
    number of workers follows 1, 2, 3, ..., MAXWORKERS
  */
  else{
    int listSize[] = {1000,2000,3000,4000,5000,6000,7000,8000,9000,10000};
    FILE *fp = fopen("results.txt", "w");
    fprintf(fp, "Size \t NumWorkers \t MedParTime \t MedSeqTime \t Speedup\n");
    printf("opened file\n");

    //loop on list sizes
    for (int sizeIdx = 0; sizeIdx < sizeof(listSize)/sizeof(listSize[0]); sizeIdx++){
      size = listSize[sizeIdx];

      double par_times[5];
      double seq_times[5];
      for (int run = 0; run < 5; run++){
        printf("Running size %d, run %d\n", size, run+1);
        /* initialize the matrix sequentially*/
        srand(time(NULL));
        for (i = 0; i < size; i++) {
          list[i] = rand()%999;
        }
        double par_time = parallel(false, list, size);
        double seq_time = sequential(false, list, size);
        par_times[run] = par_time;
        seq_times[run] = seq_time;
      }
      double med_seq_time = findMedian(seq_times, 5);
      double med_par_time = findMedian(par_times, 5);

      double speedup = med_seq_time / med_par_time;
      fprintf(fp, "%d \t %d \t %g \t %g \t %g\n", size, numWorkers, med_par_time, med_seq_time, speedup);
    }
    fclose(fp);
    printf("closed file\n");
  }

  return 0;
}