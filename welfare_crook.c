/*
COMPILE WITH mpicc welfare_crook.c -o welfare_crook
RUN WITH mpirun -np 3 ./welfare_crook
*/

/* this is an Single Program Multiple Data program where all processes execute the same program but control flow depends on the process rank inside the default communicator MPI_COMM_WOLRD

Each MPI process has its own provate address space, therefore no shared variables. The only way interaction happens is through message passing with MPI_Send, MPI_Recv, ect
Each process has its own assigned unique rank (ID). We use 3 processes in this assignment: Rank 0 mapped to F, rank 1 mapped to G, and rank 2 mapped to H
We assume each of the distributed processes have a local array of integers of size n. The integers represent personal numbers. We assume there is at least one person whose personal number is on all three lists.
In the developed program all three processes interact with each other until each process determines all names each of which is on all three lists. Each process prints the common names that is determined.The program is developed in C lang.
The restrictions are to not use shared variables and not send entire arrays in messages. We use message passing and pass one value at the time per message
ALGORITHM BREAKDOWN:
F → list of IBM employees Yorktown 
G → list of Columbia students
H → list of people on welfare in NY 
Problem: find common value/s in all three lists
Solution: filter through 3 steps to remove uncommon values 
1) Process F -----> takes each number in its array and sends it one by one to G, so G can see every value from F.
2) Process G -----> for each int received from F, if the number is in G, it will be sent to H, else ignored it. So at this point H receives only numbers in F AND G (INTERSECTION) F ∩ G
3) Process H -----> for each int received form G, if the number is in H, it is the found common value, else it is ignored.
when process H finds common value, it sends it to F and it sends it to G. so at this point F knows the value and so does G.
To stop the program F sends end of stream value END when done sending, G forwards it to H, and H sends a DONE back to F and G. at this point every process stops.
 */
#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define RANK_F 0
#define RANK_G 1
#define RANK_H 2

#define TAG_F_TO_G 5 // streaming vals from F to G
#define TAG_G_TO_H 15 // candidate int sent from G to H
#define TAG_H_TO_F 25 // confirmed intersection vals from H to F
#define TAG_H_TO_G 35 // confirmed intersection vals from H to G
#define TAG_DONE 40 // H indicates completion

// end of stream sentinel
static const int END = -1;
// payload value for DONE
static const int DONE = -1;

//qsort comparator for int
static int cmp_int(const void *a,const void *b){
    int x = *(const int*)a;
    int y = *(const int*)b;
    return (x>y) - (x<y);
} 
//binary search in sorted array ints (each array uses it to sort items)
static int binary_search(const int *arr, int n, int key) {
    int lo= 0;
    int hi = n-1;
    while (lo<=hi) {
        int mid = lo + (hi-lo)/2;
        int v = arr[mid];
        if (v == key) {
            return 1;
        }
        if (v<key){
            lo = mid +1;
        } else {
            hi = mid -1;
            }
    } 
    return 0;
}
//fill array with pseudo random int
static void fill_array (int *arr, int n, unsigned seed, int base, int range) {
    srand(seed);
    for( int i = 0; i<n; i++) {
        arr[i] = base + (rand()% range);
    }
}
//DEMO HELPER
// for the purpose od the problem statement we plant a few common vals
static void plant_values(int *arr, int n, const int *vals, int k, unsigned seed2){
    (void)seed2;
    if ( k > n) {
        k = n;
    }
    for(int i = 0; i<k; i++) {
       arr[i] = vals[i];
    }
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv); //init MPI environment. Must be called before any MPI function
    int rank = -1;
    int size = 0;
    //query this proc rank and tot number of processes in MPI_COMM_WORLD
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    if(size !=3) { //this sol is for 3 processes
        if (rank==0) {
            fprintf(stderr, "Run with 3 processes: mpirun -np3 ./welfare_crook [n] [seed]\n");
        }
        MPI_Finalize();
        return 1;
    }
    // Parsing n (array size) and seed (for random generation)
    int n = 50;
    unsigned seed = 1;
    if (argc >= 2) n = atoi(argv[1]);
    if (argc>= 3) seed = (unsigned)strtoul(argv[2], NULL, 10);
    if (n <= 0) n = 50;
    //allocating array --> in distributed memory each process allocates its own array 
    int *local = (int*)malloc((size_t)n * sizeof(int));
    if (!local) {
        fprintf(stderr, "Rank %d: malloc failed\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 2);
    }
    // for assignment pruposes we guarantee a small intersection for testing 
    const int planted_k = 3;
    int planted[planted_k];
    for(int i = 0; i < planted_k; i++){
        planted[i] = 100000 + i;
    }
    // diff seeds per rank so the local arrays differ
    if ( rank == RANK_F) {
        fill_array(local, n, seed + 10, 0, 200000);
        plant_values(local, n, planted, planted_k, seed + 101);
    } else if ( rank == RANK_G){
        fill_array(local, n, seed + 20, 0, 200000);
        plant_values(local, n, planted, planted_k, seed + 202);
    } else { //rank == RANK_H
        fill_array(local, n, seed + 30, 0, 200000);
        plant_values(local, n, planted, planted_k, seed + 303);
    }
    // sort local data 
    qsort(local, (size_t)n, sizeof(int), cmp_int);

    //DEBUG
    /*
    printf("RANK %d local array: [", rank);
    for(int i = 0; i <n; i++) {
        printf("%d", local[i]);
        if ( i < n-1) printf(", ");
    }
    printf("]\n\n");
    */
    
   //dynamic list of found comm values 
    int found_cap = 16;
    int found_len = 0;
    int *found = (int*)malloc((size_t)found_cap * sizeof(int));

    if ( !found) {
        fprintf(stderr, "Rank %d: failed malloc\n", rank);
        MPI_Abort(MPI_COMM_WORLD, 3);
    }
    /*===============================
    F is the producer of the pipeline (Rank 0). 
    it streams all values to G, one at a time (blocking send)
    ===============================*/
    if(rank == RANK_F) {
        for ( int i = 0; i < n ; i++) {
            int x = local [i];
            MPI_Send(&x, 1, MPI_INT, RANK_G, TAG_F_TO_G, MPI_COMM_WORLD);
        }
        //END sentinel to indicate F is finished sending values
        MPI_Send((int*)&END, 1, MPI_INT, RANK_G, TAG_F_TO_G, MPI_COMM_WORLD);
    /*F receives intersection values from H
    We use the wildcard MPI_ANY_TAG bcs H could send a control value (TAG_H_TO_F) or TAG_DONE to indicate termination
    when using wildcards we inspect the status of the tag*/
        while(1){
            MPI_Status status;
            int x;
            MPI_Recv(&x, 1, MPI_INT, RANK_H, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if(status.MPI_TAG ==TAG_DONE) break;

            if(found_len == found_cap) {
                found_cap *=2;
                found = (int*)realloc(found, (size_t)found_cap * sizeof(int));
                if (! found) MPI_Abort(MPI_COMM_WORLD, 4);

            }
            found[found_len++] = x; 
        }
    /*===============================
    G is the middle filter (rank 1). 
    it receives all values from F(blocking receive)
    if the value exists in G it is forwared to H, one at a time
    ===============================*/
    } else if (rank == RANK_G) {
        while(1) {
            MPI_Status status;
            int x;

            int flag = 0;
            // using MPI_Iprobe to see if H already sent a confirmed value to G. to avoid queue buildup
            MPI_Iprobe(RANK_H, TAG_H_TO_G, MPI_COMM_WORLD, &flag, &status);
            if(flag) {
                MPI_Recv(&x, 1, MPI_INT, RANK_H, TAG_H_TO_G, MPI_COMM_WORLD, &status);
                if(found_len == found_cap) {
                    found_cap *=2;
                    found = (int*)realloc(found, (size_t)found_cap *sizeof(int));
                    if ( ! found) MPI_Abort(MPI_COMM_WORLD,5);
                }
                found[found_len++] = x;
                continue;
            }
            //recv next val from F
            MPI_Recv(&x, 1, MPI_INT, RANK_F, TAG_F_TO_G, MPI_COMM_WORLD, &status);
            if( x == END) break;

            //forward only if x is in G
            if(binary_search(local, n, x)){
                MPI_Send(&x, 1, MPI_INT, RANK_H, TAG_G_TO_H, MPI_COMM_WORLD);
            }


        }
        //Let H know that G is done sending candidates
        MPI_Send((int*)&END, 1, MPI_INT, RANK_H, TAG_G_TO_H, MPI_COMM_WORLD);
        //After finishing the pipeline, G recv the rest of the common values and a DONE control message from H 
        while(1) {
            MPI_Status status;
            int x;
            MPI_Recv(&x, 1, MPI_INT, RANK_H, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if(status.MPI_TAG == TAG_DONE) break;

            if(found_len == found_cap) {
                found_cap *= 2;
                found = (int*)realloc(found, (size_t)found_cap * sizeof(int));
                if ( ! found) MPI_Abort(MPI_COMM_WORLD, 6);
            }
            found[found_len++] = x;

        }

        /*===============================
    H is the final filter
    it receives candidates from G, one at a time 
    if x is in H, then it is a confirmed common value as it reached the end of the filtering pipeline
    H sends x to F  and G one value per message
    ===============================*/
    } else { //rank == RANK_H
        // Track non blocking sends with MPI_Request object s 
        MPI_Request *req = NULL;
        int req_cap = 16;
        int req_len = 0;
        req = (MPI_Request*)malloc((size_t)req_cap * sizeof(MPI_Request));
        if( ! req) MPI_Abort(MPI_COMM_WORLD,7);

        while(1) {
            MPI_Status status;
            int x;
            // blocking receive from G 
            MPI_Recv(&x, 1, MPI_INT, RANK_G, TAG_G_TO_H, MPI_COMM_WORLD, &status);

            if(x == END) break;

            if(binary_search(local, n, x)) {

                if( found_len == found_cap) {
                    found_cap *= 2;
                    found = (int*)realloc(found, (size_t)found_cap * sizeof(int));
                    if ( ! found) MPI_Abort(MPI_COMM_WORLD, 8);
                }
                found[found_len++] = x;
                // MPI_Isend init a send and returns immediatly 
                //then, MPI:Waitall makes sure all unresolved sends are completeddd
                if(req_len +2 > req_cap){
                    req_cap *=2;
                    req = (MPI_Request*)realloc(req, (size_t)req_cap * sizeof(MPI_Request));
                    if (! req) MPI_Abort(MPI_COMM_WORLD, 9);
                }
                MPI_Isend(&x, 1, MPI_INT, RANK_F, TAG_H_TO_F, MPI_COMM_WORLD, &req[req_len++]);

                MPI_Isend(&x, 1, MPI_INT, RANK_G, TAG_H_TO_G, MPI_COMM_WORLD, &req[req_len++]);
            }
        }

    if(req_len >0) {
        MPI_Waitall(req_len, req, MPI_STATUSES_IGNORE); // all non blocking sends completed before DONE
        }
    // tell F and G there is no more results, via DONE control messages
    MPI_Send((int*)&DONE, 1, MPI_INT, RANK_F, TAG_DONE, MPI_COMM_WORLD);
    MPI_Send((int*)&DONE, 1, MPI_INT, RANK_G, TAG_DONE, MPI_COMM_WORLD);

    free(req);

    }
    
    MPI_Barrier(MPI_COMM_WORLD); // sync before printing results 
    // each process prints the common values determined
    if(rank == RANK_F) {
        printf("Process F (rank 0) common values determined (%d):\n", found_len);
        for(int i = 0; i < found_len; i++){
            printf(" %d\n", found[i]);
        }
    } else if ( rank == RANK_G) {
        printf("Process G (rank 1) common values determined (%d):\n", found_len);
        for(int i = 0; i < found_len; i++){
            printf(" %d\n", found[i]);
        }
    } else {
        printf("Process H (rank 2) common values determined (%d):\n", found_len);
        for(int i = 0; i < found_len; i++){
            printf(" %d\n", found[i]);
        }
    }
    free(found);
    free(local);

    MPI_Finalize(); // must be called before program exit
    return 0;

}