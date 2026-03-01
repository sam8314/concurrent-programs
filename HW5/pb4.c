/*
mpicc pb4.c -o p
mpiexec -n 3 ./p
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // for usleep
#include <mpi.h>

#define TAG_VALUE 0
#define TAG_MATCH 1
#define TAG_DONE 99

void print_array(int rank, int arr[], int n, const char* name) {
    printf("Process %d (%s): [", rank, name);
    for (int i = 0; i < n; i++) {
        printf("%d", arr[i]);
        if (i < n-1) printf(", ");
    }
    printf("]\n");
    fflush(stdout);
}

int main(int argc, char** argv) {
    int my_rank, comm_sz;
    int local_array[10];
    int local_n = 5;
    int found_commons[10];
    int common_count = 0;
    MPI_Status status;
    
    // init MPI
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &my_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &comm_sz);
    
    if (comm_sz != 3) {
        if (my_rank == 0) {
            fprintf(stderr, "Error: we need exactly 3 processes; must put -n 3\n");
        }
        MPI_Finalize();
        return 1;
    }
    
    // init sorted local data
    const char* proc_name;
    if (my_rank == 0) { //IBM
        int temp[5] = {2, 5, 8, 12, 19};
        memcpy(local_array, temp, sizeof(temp));
        proc_name = "IBM";
    } else if (my_rank == 1) { //columbia
        int temp[5] = {3, 5, 7, 12, 18};
        memcpy(local_array, temp, sizeof(temp));
        proc_name = "Columbia";
    } else { // welfare
        int temp[5] = {1, 4, 5, 11, 12};
        memcpy(local_array, temp, sizeof(temp));
        proc_name = "Welfare";
    }
    
    // wait all processes are ready
    MPI_Barrier(MPI_COMM_WORLD);
    print_array(my_rank, local_array, local_n, proc_name);
    
    // ----- FIND COMMONS
    int i = 0; // index local array
    
    if (my_rank == 0) { // IBM
        while (i < local_n) {
            int current = local_array[i];
            MPI_Request requests[2];
            MPI_Status statuses[2];
            
            // send current value to G and H (non-blocking)
            MPI_Isend(&current, 1, MPI_INT, 1, TAG_VALUE, MPI_COMM_WORLD, &requests[0]);
            MPI_Isend(&current, 1, MPI_INT, 2, TAG_VALUE, MPI_COMM_WORLD, &requests[1]);
            MPI_Waitall(2, requests, statuses); //wait to finish both sends
            
            int match_count = 0;
            
            // from G
            int resp_g;
            MPI_Recv(&resp_g, 1, MPI_INT, 1, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == TAG_MATCH) match_count++;
            
            // from H
            int resp_h;
            MPI_Recv(&resp_h, 1, MPI_INT, 2, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
            if (status.MPI_TAG == TAG_MATCH) match_count++;
            
            // both matched >> common value
            if (match_count == 2) {
                found_commons[common_count++] = current;
                printf("Process %d (%s) found common value: %d\n", my_rank, proc_name, current);
                fflush(stdout);
            }            
            i++;
        }
        
        // termination signals
        int term = -1;
        MPI_Send(&term, 1, MPI_INT, 1, TAG_DONE, MPI_COMM_WORLD);
        MPI_Send(&term, 1, MPI_INT, 2, TAG_DONE, MPI_COMM_WORLD);
        
    } else {// colombia and welfare
        while (1) {
            int recv_val;
            MPI_Status probe_status;
            
            // probe for msg with timeout (avoid infinite blocking)
            int flag = 0;
            MPI_Iprobe(0, MPI_ANY_TAG, MPI_COMM_WORLD, &flag, &probe_status);
            
            if (flag) {
                // msg available >> get it
                MPI_Recv(&recv_val, 1, MPI_INT, 0, probe_status.MPI_TAG, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                
                if (probe_status.MPI_TAG == TAG_DONE || recv_val == -1) {break;} //terminator
                
                // find received value in local array
                int found = 0;
                for (int j = 0; j < local_n; j++) {
                    if (local_array[j] == recv_val) {
                        found = 1;
                        break;
                    }
                }
                
                // response to F (blocking fine here)
                if (found) {
                    MPI_Send(&recv_val, 1, MPI_INT, 0, TAG_MATCH, MPI_COMM_WORLD);
                    printf("Process %d (%s) confirmed value: %d\n", my_rank, proc_name, recv_val);
                    fflush(stdout);
                } else {MPI_Send(&recv_val, 1, MPI_INT, 0, TAG_VALUE, MPI_COMM_WORLD);}
            }
            usleep(1000); //delay for no busy waiting
        }
    }
        
    MPI_Barrier(MPI_COMM_WORLD);//wait all processes finish
    
    if (my_rank == 0) {
        printf("\n=== Final Results ===\n");
        printf("Process %d (%s) found %d common values: ", my_rank, proc_name, common_count);
        for (int j = 0; j < common_count; j++) {printf("%d ", found_commons[j]);}
        printf("\n");
        fflush(stdout);
    } else {
        printf("Process %d (%s) completed\n", my_rank, proc_name);
        fflush(stdout);
    }    
    MPI_Finalize();
    return 0;
}