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
#include <string.h> //for memcpy

#define MAXSIZE 10000  /* maximum list size */
#define MAXWORKERS 8   /* maximum number of workers */
//#define SEQ_CUTOFF 32 //does improve performance on macOS

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

int compare_double(const void *a, const void *b) { //compares double instead of int
  double da = *(double *)a;
  double db = *(double *)b;
  return (da > db) - (da < db);

}
double findMedian(double arr[], int n) {
    qsort(arr, n, sizeof(double), compare_double); //adapted to compare_double

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
void swap (int* a, int* b) {
    int t = *a;
    *a = *b;
    *b = t;
}
int split(int arr[], int low, int high) {
  int pivotIdx = low + rand()%(high-low +1); //randomized pivot index
  swap(&arr[pivotIdx], &arr[high]); //move pivot to end
    int pivot = arr[high]; 
    int i = (low-1);

    for (int j=low;j<=high-1; j++) {
        if (arr[j] < pivot) { //was <=
            i++; 
            swap(&arr[i], &arr[j]);
        }
    }
    swap(&arr[i+1], &arr[high]);
    return (i+1);
}

/* QUICKSORTS */
void seqQuickSort(int arr[], int low, int high) { //tail recursion elimination to prevent bus error/stack overflow on macOS for worst case partitions
    while (low < high) {
        int splitIdx = split(arr, low, high);//moved inside loop
        //recurse on smaller side, loop on larger side to achieve log(n) stack
        if(splitIdx-low<high-splitIdx){
        seqQuickSort(arr, low, splitIdx-1); //single recursive call
        low = splitIdx +1; //tail recursion eliminated
        } else{
        seqQuickSort(arr, splitIdx+1, high); //single recursive call
        high = splitIdx -1;
        }
    }
}
void parQuickSort(int arr[], int low, int high) { // with firstprivate to fetch indices for each task safely 
    if (low < high) {
     // if(high -low <SEQ_CUTOFF) { //avoid deep recurison on very small parts
       // seqQuickSort(arr,low,high);
        //return;
     // }
        int splitIdx = split(arr, low, high);

        #pragma omp task shared(arr) firstprivate(low, splitIdx) if(high-low>1000)
        parQuickSort(arr, low, splitIdx-1);

        #pragma omp task shared(arr) firstprivate(high, splitIdx) if(high-low>1000)
        parQuickSort(arr, splitIdx+1, high);

        #pragma omp taskwait // wait for both tasks to finish
    }
}

/* WRAPPERS WITH TIMERS */
double sequential(bool print, int list[], int size) {
  /* SEQUENTIAL VERIFICATION OF RESULTS*/
  start_time = omp_get_wtime();

  seqQuickSort(list, 0, size-1);

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
        parQuickSort(list, 0, size-1);
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

    int list_copy[MAXSIZE]; // added to compare same input for seq/par

    /* initialize the list sequentially*/
    srand(time(NULL));
    for (i = 0; i < size; i++) {
      //printf("[ ");
        list[i] = rand()%999;
    }
        //printf(" %d", list[i]);
        memcpy(list_copy, list, size * sizeof(int)); //keeps identical input for sequential
        double par = parallel(true, list, size); 
        double seq = sequential(true, list_copy, size); // we use the copy not the already sorted list
        printf("Speedup: %g\n", seq/par); //reports speedup in single run mode
        return 0;


    }
      //printf(" ]\n");
   
  

  /* store runtime time, speedup in output file with different sizes and number of workers
    run it 5 times and take median values for runtimes
    matrix size follows 10, 100, 1000, ..., MAXSIZE
    number of workers follows 1, 2, 3, ..., MAXWORKERS
  */
    int listSize[] = {1000,2000,3000,4000,5000,6000,7000,8000,9000,10000};
    FILE *fp = fopen("results.txt", "w");
    fprintf(fp, "Size \t NumWorkers \t MedParTime \t MedSeqTime \t Speedup\n");

    printf("opened file\n");
    fflush(fp); // ensures outputs appear even if it crashes

    srand(time(NULL)); //seed once not inside loops 

    //loop on list sizes
    for (int sizeIdx = 0; sizeIdx < 10; sizeIdx++){
      size = listSize[sizeIdx];
      if (size >MAXSIZE ) size = MAXSIZE; //safety 

      for (numWorkers =1; numWorkers<= 4; numWorkers++){
        omp_set_num_threads(numWorkers); //Specify the number of processors used by specifying a different number of threads by calling  omp_set_num_threads() as requested in the assignment

        double par_times[5];
        double seq_times[5];
        int base[MAXSIZE]; // store identical input across seq/par
        for ( int run = 0; run <5; run++) {

        
        printf("Running size %d, run %d\n", size, run+1);
        /* initialize the matrix sequentially*/
        for (i = 0; i < size; i++) {
          base[i] = rand()%999; //fill base
        }
        memcpy(list, base, size * sizeof(int)); //same input for parallel
        par_times[run] = parallel(false, list, size);
        memcpy(list, base, size * sizeof(int)); // same input for sequential 

        seq_times[run] = sequential(false, list, size);
      }
      double med_seq_time = findMedian(seq_times, 5);
      double med_par_time = findMedian(par_times, 5);
      fprintf(fp, "%d \t %d \t %g \t %g \t %g\n", size, numWorkers, med_par_time, med_seq_time, med_seq_time/med_par_time);
      fflush(fp); //flush each row

    }
    }
    fclose(fp);
    printf("closed file\n");
    return 0;
  
}