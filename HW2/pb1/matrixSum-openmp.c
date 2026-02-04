/* matrix summation using OpenMP

   usage with gcc (version 4.2 or higher required):
     gcc -O -fopenmp -o m matrixSum-openmp.c 
     ./m [size] [numWorkers] for full config and see results
      ./m for storing results in results.txt

*/

#include <omp.h>

double start_time, end_time;

#include <stdio.h> // for printf
#include <stdlib.h> // for atoi, rand
#include <stdio.h> // for FILE operations
#include <stdbool.h> // for bool type
#include <time.h> // for time
#include <sys/time.h> 
#include <limits.h> // for INT_MAX, INT_MIN
#define MAXSIZE 10000  /* maximum matrix size */
#define MAXWORKERS 8   /* maximum number of workers */

int numWorkers;
int size; 
int matrix[MAXSIZE][MAXSIZE];

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

/* WORK ON MATRIX*/
double sequential(bool print, int matrix[MAXSIZE][MAXSIZE], int size){
  /* SEQUENTIAL VERIFICATION OF RESULTS*/
  int i, j;

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
  if(print){
    printf("\n==============SEQUENTIAL RESULTS==============\n");
    printf("The total is %d\n", seq_sum);
    printf("The global min is %d at (%d,%d)\n", seq_min, seq_min_row, seq_min_col); 
    printf("The global max is %d at (%d,%d)\n", seq_max, seq_max_row, seq_max_col); 
    printf("The execution time is %g sec\n", end_time - start_time);
    printf("==============================================\n"); 
  }
  return end_time - start_time;
}
double parallel(bool print, int matrix[MAXSIZE][MAXSIZE], int size, int numWorkers){
  /* PARALLELE WORK*/
  int global_min = INT_MAX;
  int global_max = INT_MIN;
  int global_min_row = 0, global_min_col = 0; 
  int global_max_row = 0, global_max_col = 0;
  int total = 0;
  int i, j;
  omp_set_num_threads(numWorkers);
  start_time = omp_get_wtime();

  #pragma omp parallel for reduction(max:global_max) reduction(min:global_min) reduction(+:total) private(j) collapse(2)
      for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            int val = matrix[i][j];
            total += val;

            #pragma omp critical // to avoid race conditions
            {
              if (val > global_max) {
                  global_max = val;
                  global_max_row = i;
                  global_max_col = j;
              }
              if (val < global_min) {
                  global_min = val;
                  global_min_row = i;
                  global_min_col = j;
              }
            }
        }
    }
  // implicit barrier
  end_time = omp_get_wtime();

  if(print){  
    printf("\n==============PARALLEL RESULTS================\n");
    printf("The total is %d\n", total);
    printf("The global min is %d at (%d,%d)\n", global_min, global_min_row, global_min_col); 
    printf("The global max is %d at (%d,%d)\n", global_max, global_max_row, global_max_col);
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
    /* initialize the matrix sequentially*/
    srand(time(NULL));
    for (i = 0; i < size; i++) {
      //printf("[ ");
      for (j = 0; j < size; j++) {
        matrix[i][j] = rand()%999;
        //printf(" %d", matrix[i][j]);
      }
      //printf(" ]\n");
    }
    
    double par = parallel(true, matrix, size, numWorkers);
    double seq = sequential(true, matrix, size);
  }

  /* store runtime time, speedup in output file with different sizes and number of workers
    run it 5 times and take median values for runtimes
    matrix size follows 10, 100, 1000, ..., MAXSIZE
    number of workers follows 1, 2, 3, ..., MAXWORKERS
  */
  else{
    int matrixSize[] = {1000,2000,3000,4000,5000,6000,7000,8000,9000,10000};
    FILE *fp = fopen("results.txt", "w");
    fprintf(fp, "Size \t NumWorkers \t MedParTime \t MedSeqTime \t Speedup\n");
    printf("opened file\n");

    //loop on matrix sizes
    for (int sizeIdx = 0; sizeIdx < sizeof(matrixSize)/sizeof(matrixSize[0]); sizeIdx++){
      size = matrixSize[sizeIdx];

      double par_times[5];
      double seq_times[5];

      //loop on numworkers
      for (numWorkers = 1; numWorkers <= MAXWORKERS; numWorkers=numWorkers*2){
      for (int run = 0; run < 5; run++){
        printf("Running size %d, run %d\n", size, run+1);
        /* initialize the matrix sequentially*/
        srand(time(NULL));
        for (i = 0; i < size; i++) {
          for (j = 0; j < size; j++) {
            matrix[i][j] = rand()%999;
          }
        }
        double par_time = parallel(false, matrix, size, numWorkers);
        double seq_time = sequential(false, matrix, size);
        par_times[run] = par_time;
        seq_times[run] = seq_time;
      }
      double med_seq_time = findMedian(seq_times, 5);
      double med_par_time = findMedian(par_times, 5);

      double speedup = med_seq_time / med_par_time;
      fprintf(fp, "%d & %d & %g & %g & %g \\\\ \n", size, numWorkers, med_par_time, med_seq_time, speedup);

      }
      fprintf(fp, "\\hline \n");
    }
    fclose(fp);
    printf("closed file\n");
  }

  return 0;
}

