/* The program compares corresponding lines in 2 files. If the lines are different, both lines are printed. If one file is longer, all extra lines of such file are printed
run on terminal with gcc -Wall -Wextra -pthread diff.c -o diff
test1: identical files, print only elapsed time, test with echo -e "a\nb\nc" > t1.txt
echo -e "a\nb\nc" > t2.txt
./diff t1.txt t2.txt

test2: file2 longer, print file2 extra lines, test with echo -e "A\nB" > t5.txt
echo -e "A\nB\nC\nD" > t6.txt
./diff t5.txt t6.txt

test 3: file1 longer, print file1 extra lines ,test with echo -e "A\nB\nC\nD" > t3.txt
echo -e "A\nB" > t4.txt
./diff t3.txt t4.txt

test 4: lines in both files are differnt, all lines from files are printed(line1 file 1, line1 file2, etc),test with echo -e "one\ntwo\nthree" > t7.txt
echo -e "four\nfive\nsix" > t8.txt
./diff t7.txt t8.txt

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
#include <string.h>
#include <errno.h>
#define QUEUE_SIZE 256
#define MAX_LINE 8192
/*exhibits line reading from a file */
typedef struct {
    char *line;
} item_t;
/*every reader thread produces line into a queue and the comparator thread consumes lines from the queues*/
typedef struct {
    item_t *buffer; /*circular buffer*/
    int size, head, tail , count;
    int closed; /* it is set tp 1 when producer ends EOF*/
    pthread_mutex_t mutex;
    pthread_cond_t notEmpty;
    pthread_cond_t notFull;
} 
queue_t;

typedef struct { /* arguments passed to reader thread*/
    const char *filename;
    queue_t *q;
} 
reader_args_t;

typedef struct { /* args passed to comparator thread*/
    queue_t *q1;
    queue_t *q2;
}
cmp_args_t;


static void terminate(const char*msg);
static void queue_init(queue_t *q, int size);
static void queue_destroy(queue_t *q);
static void queue_close(queue_t *q);
static int queue_push(queue_t *q, char *line_ptr);
static int queue_pop(queue_t *q, char **out_line_ptr);

static char *read_line_duplicate(FILE *fp);
static void print_line(const char *string);

static void *reader_thread(void *arg);
static void *comparator_thread(void *arg);

/*measures thread creation, parallel exec, sync, and comparison*/ 
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

static void terminate(const char *msg) {
    perror(msg);
    exit(1);
}
/* it opens a file and reads it line by line; each line is dinamically allocated nd pushed into the shared queue. WHen EOF <reached, the queue is closed*/
static void *reader_thread(void *arg) {
    reader_args_t  *a = (reader_args_t *)arg;
    FILE *fp = fopen(a->filename, "r"); 
    if (!fp) {
        fprintf(stderr, "Can't open '%s': %s\n", a->filename, strerror(errno));
        queue_close(a->q); // no data produced
        return NULL;
    }
    while(true) {
        char *line = read_line_duplicate(fp); // reads one line from file and duplicades into heap mem
        if (!line) {
            if (ferror(fp)){
                fprintf(stderr, "Read error on '%s'\n", a->filename);
                clearerr(fp);
            }
            break;
        }
        if(!queue_push(a->q, line)) { // push line into shared queue, if queue closed, stop line production
            break;
        }
    }
    fclose(fp);
    queue_close(a->q);
    return NULL;

}
/* read one line from file using fgets func and returns a dinamically allocated copy. Extreme cases (EOF / error) handled by returning null */
static char *read_line_duplicate (FILE *fp) {
    char temp[MAX_LINE];
    if (!fgets(temp,(int)sizeof(temp), fp)) {
        return NULL;
    }
    size_t length = strlen(temp);
    char *s = (char *)malloc(length +1);
    if(!s) terminate ("malloc");
    memcpy(s,temp,length+1);
    return s;

}
/* continuously consumes lines from both quues and compares them*/
static void *comparator_thread(void *arg) {
    cmp_args_t *a = (cmp_args_t *)arg; 
    // flags that show if input file has terminated
    int done1 = 0;
    int done2 = 0;
    while (!done1|| !done2) { // as long as both file have processed
        char *l1 = NULL; // pointers to hold popped lines from each queue
        char *l2 = NULL;
        int got1 = 0; // flags for gotten lines
        int got2 = 0;
        if (!done1) { //try to get a line from queue1 if not done
            got1 = queue_pop(a->q1, &l1);
            if (!got1) done1 = 1; // if no line returned, q1 close & empty
        }
        if(!done2) { // get a line from queue2 if not done 
            got2 = queue_pop(a->q2, &l2);
            if (!got2) done2 = 1; // if no line returned, q2 close nd empty 

        }
        /* C1: both files still have lines, compare corresponding lines*/
        if(got1 && got2) {
            if(strcmp(l1,l2) != 0){ // if they differ, print both
                print_line(l1);
                print_line(l2);
            }
            free(l1); // free m alloc 
            free(l2);

        } else if (got1 && !got2) { // C2: file1 has lines, file2 finished
            print_line(l1);
            free(l1);

        } else if (!got1 && got2){ // C3: file1 finishd, file2 has lines
            print_line(l2);
            free(l2);
        } else {
            //C4, both queues emoty and closed, loop exits

        }
    }
    fflush(stdout); // writes output b4 terminating thread
    return NULL; 


}

static void print_line(const char *string) {
    fputs (string, stdout);
    size_t n = strlen(string);
    if (n ==0 || string[n-1] != '\n') fputc('\n', stdout);
}

static void queue_init(queue_t *q, int size) {
    
    q->buffer = (item_t*)calloc((size_t)size, sizeof(item_t));
    if (!q->buffer) {
        terminate("calloc");
    }
    q-> size = size; // sets max n elements for the queue
    q->head = q->tail = q->count = 0; // init head, tail and count indexes
    q->closed = 0; // queue open initiall
    pthread_mutex_init(&q->mutex, NULL); // init mutex to protect queue 
    pthread_cond_init(&q->notEmpty, NULL); // cond var that signals queue not empty 
    pthread_cond_init(&q->notFull, NULL); // cond var that signals queue not full
}

static int queue_push(queue_t *q, char *line_ptr) {

    pthread_mutex_lock(&q->mutex); // locks mutex b4 accessing share queue data
    while(q->count == q->size && !q->closed){ // while queue is full and open
        pthread_cond_wait(&q-> notFull, &q->mutex); // release atomically the mutex and wait for notFUll signal
    }

    if(q->closed) { // if queue closed, unlocks mutex and free line
        pthread_mutex_unlock(&q->mutex);
        free(line_ptr);
        return 0;
    }
    q->buffer[q->tail].line= line_ptr; // store line pointer at tail pos
    q->tail= (q->tail +1)% q->size; // circularly advances tail index
    q->count++;
    pthread_cond_signal(&q->notEmpty); 
    pthread_mutex_unlock(&q->mutex); // unlocks mutex after modifying shared state
    return 1;
}
static int queue_pop(queue_t *q, char **out_line_ptr){ 

    pthread_mutex_lock(&q->mutex); // lock mutex b4 access 
    while (q-> count == 0 && !q->closed) { // waits for notempty signal while queue is empty and open
        pthread_cond_wait(&q->notEmpty, &q->mutex);
    }
    if (q->count== 0 && q-> closed) {
       pthread_mutex_unlock(&q->mutex);
       *out_line_ptr = NULL;
       return 0;
    }
    char *line = q-> buffer[q->head].line; // fetch line pointer at head index
    q->buffer[q->head].line = NULL; // clears slot to avoid double'free during queue destruction. I didn't consider it at the beginning and it caused double free of object at queue destruction 
    q->head= (q->head +1)% q->size; // circularly advances head index
    q->count--; 
    pthread_cond_signal(&q->notFull);
    pthread_mutex_unlock(&q->mutex); // unlocks mutex after altering shared state 
    *out_line_ptr = line; // 
    return 1;


}

static void queue_destroy(queue_t *q) {

    for(int i=0; i< q->size; i++) { // loops through every slot in the buffer
        free(q->buffer[i].line); // frees heap mem for the line if slot contains line pointer
        q->buffer[i].line = NULL;
    }
    free(q->buffer); 
    pthread_mutex_destroy(&q->mutex); // after threads terminate
    pthread_cond_destroy(&q->notEmpty);
    pthread_cond_destroy(&q->notFull);

}

static void queue_close(queue_t *q) {

    pthread_mutex_lock(&q->mutex);
    q->closed = 1; // closed state flag, no more items produced
    pthread_cond_broadcast(&q->notEmpty); // wake consumer threads waiting for items
    pthread_cond_broadcast(&q->notFull); // wake producer threads waiting for space
    pthread_mutex_unlock(&q->mutex);

}

int main (int argc, char **argv) {
    if(argc !=3){
        fprintf(stderr, "Usage: %s filename1 filename 2\n", argv[0]);
        return 2;
    }
    queue_t q1, q2;
    queue_init(&q1, QUEUE_SIZE);
    queue_init(&q2, QUEUE_SIZE);

    reader_args_t reader1 = { .filename = argv[1], .q = &q1};
    reader_args_t reader2 = { .filename = argv[2], .q = &q2};
    cmp_args_t cmp = { .q1=&q1, .q2=&q2};

    pthread_t thread1, thread2, thread3;

   read_timer();

    if (pthread_create(&thread1, NULL, reader_thread, &reader1) !=0) terminate ("pthread_create reader1");
    if (pthread_create(&thread2, NULL, reader_thread, &reader2) !=0) terminate ("pthread_create reader2");
    if (pthread_create(&thread3, NULL, comparator_thread, &cmp) !=0) terminate  ("pthread_create comparator");

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    double elapsed = read_timer();
    fprintf(stderr, "ELAPSED TIME IS %.6f SECONDS\n", elapsed);

    queue_destroy(&q1);
    queue_destroy(&q2);
    return 0;
}