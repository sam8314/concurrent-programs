/* Parallel tee command using Pthreads
   Usage: ./ptee filename
   Reads stdin → writes to stdout AND file simultaneously
   
    gcc -o ptee ptee.c -lpthread
    echo "Hello World" | ./ptee output.txt
    cat output.txt
*/

#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>     // for memset()
#include <unistd.h>     // for read() and write()
#include <fcntl.h>      // for open()
#include <errno.h>      // for errno
#include <stdbool.h>    // to use bool type

#define BUFFER_SIZE 4096
#define NUM_BUFFERS 3

typedef struct {
    char data[BUFFER_SIZE];
    int len;
    int buffer_id;
    bool filled;    
    pthread_mutex_t mutex;
    pthread_cond_t full;   /* signal that data is available */
    pthread_cond_t empty;  /* signal that buffer is free */
} Buffer;

Buffer buffers[NUM_BUFFERS];
pthread_t reader_tid, stdout_writer_tid, file_writer_tid;
int current_buffer = 0;  /* circular buffer index */
bool done_reading = false;
pthread_mutex_t done_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Initialize buffer */
void init_buffer(Buffer *b, int id) {
    b->len = 0;
    b->buffer_id = id;
    b->filled = false;
    pthread_mutex_init(&b->mutex, NULL);
    pthread_cond_init(&b->full, NULL);
    pthread_cond_init(&b->empty, NULL);
}

/* Reader thread: stdin to buffers */
void *reader_thread(void *arg) {
    int bytes_read;
    int local_buffer_idx;
    
    printf("[READER] reading from stdin...\n");
    
    while (1) {
        local_buffer_idx = current_buffer;
        
        /* Read data */
        bytes_read = read(STDIN_FILENO, buffers[local_buffer_idx].data, BUFFER_SIZE);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                /* EOF */
                pthread_mutex_lock(&done_mutex);
                done_reading = true;
                pthread_mutex_unlock(&done_mutex);
                
                /* Wake up all writers waiting on all buffers */
                for (int i = 0; i < NUM_BUFFERS; i++) {
                    pthread_mutex_lock(&buffers[i].mutex);
                    pthread_cond_broadcast(&buffers[i].full);
                    pthread_mutex_unlock(&buffers[i].mutex);
                }
                break;
            } else {
                perror("read error");
                exit(1);
            }
        }
        
        /* Fill buffer */
        pthread_mutex_lock(&buffers[local_buffer_idx].mutex);
        buffers[local_buffer_idx].len = bytes_read;
        buffers[local_buffer_idx].filled = true;
        
        printf("[READER] filled buffer %d (%d bytes)\n", 
               local_buffer_idx, bytes_read);
        
        /* Signal writers */
        pthread_cond_broadcast(&buffers[local_buffer_idx].full);
        
        /* Wait for buffer to be consumed by both writers */
        while (buffers[local_buffer_idx].filled) {
            pthread_cond_wait(&buffers[local_buffer_idx].empty, 
                             &buffers[local_buffer_idx].mutex);
        }
        
        pthread_mutex_unlock(&buffers[local_buffer_idx].mutex);
        
        /* Move to next buffer */
        current_buffer = (local_buffer_idx + 1) % NUM_BUFFERS;
    }
    
    printf("[READER] EOF reached\n");
    return NULL;
}

/* Writer thread base function */
void *writer_thread(void *arg) {
    int my_fd = *(int*)arg;
    free(arg);
    bool is_done = false;
    
    printf("[WRITER fd=%d] is waiting for data\n", my_fd);
    
    while (!is_done) {
        /* Check each buffer for data */
        for (int buf_idx = 0; buf_idx < NUM_BUFFERS; buf_idx++) {
            pthread_mutex_lock(&buffers[buf_idx].mutex);
            
            /* Wait for data or EOF */
            while (!buffers[buf_idx].filled && !done_reading) {
                pthread_cond_wait(&buffers[buf_idx].full, &buffers[buf_idx].mutex);
            }
            
            /* Check if we're done */
            pthread_mutex_lock(&done_mutex);
            is_done = done_reading && !buffers[buf_idx].filled;
            pthread_mutex_unlock(&done_mutex);
            
            if (is_done) {
                pthread_mutex_unlock(&buffers[buf_idx].mutex);
                break;
            }
            
            if (buffers[buf_idx].filled) {
                /* Write data */
                ssize_t bytes_written = write(my_fd, buffers[buf_idx].data, buffers[buf_idx].len);
                if (bytes_written != buffers[buf_idx].len) {perror("[WRITER] write error");}
                
                printf("[WRITER fd=%d] wrote buffer %d (%d bytes)\n", my_fd, buf_idx, buffers[buf_idx].len);
                
                /* Mark as consumed (only reader can reset this) */
                buffers[buf_idx].filled = false;
                
                /* Signal reader if both writers have consumed */
                pthread_cond_signal(&buffers[buf_idx].empty);
            }
            
            pthread_mutex_unlock(&buffers[buf_idx].mutex);
        }
    }
    
    printf("[WRITER fd=%d] EOF\n", my_fd);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s filename\n", argv[0]);
        return 1;
    }
    
    /* Initialize all buffers */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        init_buffer(&buffers[i], i);
    }
    
    /* Open output file */
    int file_fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0) {
        perror("open file");
        return 1;
    }
    
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
    
    /* Create writer threads first so they're ready to consume */
    int *stdout_fd = malloc(sizeof(int));
    *stdout_fd = STDOUT_FILENO;
    pthread_create(&stdout_writer_tid, &attr, writer_thread, stdout_fd);
    
    int *file_fd_ptr = malloc(sizeof(int));
    *file_fd_ptr = file_fd;
    pthread_create(&file_writer_tid, &attr, writer_thread, file_fd_ptr);
    
    /* Create reader thread */
    pthread_create(&reader_tid, &attr, reader_thread, NULL);
    
    /* Wait for all threads */
    pthread_join(reader_tid, NULL);
    pthread_join(stdout_writer_tid, NULL);
    pthread_join(file_writer_tid, NULL);
    
    /* Cleanup */
    for (int i = 0; i < NUM_BUFFERS; i++) {
        pthread_mutex_destroy(&buffers[i].mutex);
        pthread_cond_destroy(&buffers[i].full);
        pthread_cond_destroy(&buffers[i].empty);
    }
    pthread_mutex_destroy(&done_mutex);
    
    close(file_fd);
    printf("[MAIN] All threads done. Output written to '%s'\n", argv[1]);
    return 0;
}
