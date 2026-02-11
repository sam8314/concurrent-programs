/*Usage
clang -Wall -Wextra -pthread unisex_bathroom.c -o unisex_bathroom

./unisex_bathroom 7 12
*/
#ifndef _REENTRANT
#define _REENTRANT
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>
#include <fcntl.h>      // O_CREAT
#include <sys/stat.h>   // mode constants
#include <errno.h>
//shared variables
static int men_in = 0, women_in = 0;
static int men_waiting = 0, women_waiting = 0;
static int next_turn = 0; // 0 -> women next when both waiting, 1 -> men next



static sem_t *entry;      // binary semaphore mutex, controls access to shared state and baton 
static sem_t *men_sem;    // men queue
static sem_t *women_sem;  // women queue

static int NUM_MEN, NUM_WOMEN;

static void think_or_work(void) {
    usleep(100000 + (rand() % 400000));
}
static void use_bathroom(void) {
    usleep(50000 + (rand() % 150000));
}

static void die(const char *msg) {
    perror(msg);
    exit(1);
}

/*
Baton Passing Rule:
- If the bathroom is empty, choose who enters next (alternate when both are waiting).
- If the bathroom is occupied by one gender, let the same gender continue to enter (batching),
but only switch when it's empty (equity via next_turn when empty).
- Otherwise, release the entrance.
IMPORTANT: When reporting a queue light, DO NOT also post the entrance.
The reported thread gets the baton.

*/
static void pass_baton(void) {
    // MUST be called while holding entry

    if (men_in == 0 && women_in == 0) { //bathroom empty
        if (men_waiting > 0 && women_waiting > 0) { //both men and women waiting 
            if (next_turn == 0) { //next turn acts as fairness toggle. 
                next_turn = 1; 
                sem_post(women_sem); //women go first if next turn = 0
                }
            else                { // if next turn = 1 men go next
                next_turn = 0; //flips turn back to women for next steps
                sem_post(men_sem); 
                }
            sem_post(entry); //releases the mutex entry so the next thread can proceed 
            return;
        }
        if (women_waiting > 0) { //if women waiting
            next_turn = 1; //toggle to 1 
            sem_post(women_sem); //wakes one woman
            sem_post(entry); //release mutex entry
            return; 
            }
        if (men_waiting > 0)   { //if men waiting
            next_turn = 0; //toggle to 0
            sem_post(men_sem);   //wakes one man
            sem_post(entry); 
            return; 
            }

        sem_post(entry);
        return;
    }

    // Bathroom occupied: allow batching of same gender
    if (men_in > 0) {
        if (men_waiting > 0) { 
            sem_post(men_sem); //wake one man
            sem_post(entry); 
            return; 
            }
        sem_post(entry); 
        return;
    }

    if (women_in > 0) {
        if (women_waiting > 0) { 
            sem_post(women_sem); //wake one woman
            sem_post(entry); //release mutex entry so one woken woman can take entry and update state in a safe way
            return; 
            }
        sem_post(entry); 
        return;
    }

    sem_post(entry);
}



static void *man(void *arg) {
    long id = (long)(intptr_t)arg;
    while (1) {
        think_or_work();

        sem_wait(entry); //mutex protecting shared variables men_in women_in men_waiting women_waiting next_turn
        //while women in or bathroom empty and women waiting and its womens turn, men must wait
        while (women_in > 0 || (women_waiting > 0 && men_in == 0 && women_in == 0 && next_turn == 0)) {
            men_waiting++;
            sem_post(entry);        // release mutex
            sem_wait(men_sem);      // sleep
            sem_wait(entry);        // reacquire mutex
            men_waiting--; //not waiting anymore     
        }
        men_in++; //man enters
        printf("man %ld ENTERS, men_in=%d (women_in=%d, mw=%d, ww=%d)\n",
               id, men_in, women_in, men_waiting, women_waiting);

        pass_baton();
        use_bathroom();

        sem_wait(entry);
        men_in--;
        printf("man %ld LEAVES, men_in=%d (women_in=%d, mw=%d, ww=%d)\n",
               id, men_in, women_in, men_waiting, women_waiting);
        pass_baton();
    }
    return NULL;
}

static void *woman(void *arg) {
    long id = (long)(intptr_t)arg;
    while (1) {
        think_or_work();
        sem_wait(entry);
  
        while (men_in > 0 || (men_waiting > 0 && men_in == 0 && women_in == 0 && next_turn == 1)) {
            women_waiting++; //register as waiting
            sem_post(entry); //release mutex so another thread can pass baton
            sem_wait(women_sem); //sleep on women queue
            sem_wait(entry); //relock mutex and wake
            women_waiting--;    //no longer waiting   
        }
        //entering procedure: update shared variable, print in protected state, pass baton
        women_in++; //woman enters
        printf("woman %ld ENTERS, women_in=%d (men_in=%d, mw=%d, ww=%d)\n",
               id, women_in, men_in, men_waiting, women_waiting);

        pass_baton(); //decides who wakes up next
        use_bathroom();
        //leaving procedure: lock state, update shared variable, print in protected state, pass baton
        sem_wait(entry); //leave
        women_in--;
        printf("woman %ld LEAVES, women_in=%d (men_in=%d, mw=%d, ww=%d)\n",
               id, women_in, men_in, men_waiting, women_waiting);
        pass_baton();
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: %s <numMen> <numWomen>\n", argv[0]);
        return 1;
    }
    NUM_MEN = atoi(argv[1]);
    NUM_WOMEN = atoi(argv[2]);
    srand((unsigned)time(NULL));//seed for random


    // macOS: use named semaphores
    sem_unlink("/ub_entry");
    sem_unlink("/ub_men");
    sem_unlink("/ub_women");

    entry = sem_open("/ub_entry", O_CREAT, 0600, 1);
    men_sem = sem_open("/ub_men", O_CREAT, 0600, 0);
    women_sem = sem_open("/ub_women", O_CREAT, 0600, 0);
    //error handling during semaphores creation
    if (entry == SEM_FAILED) die("sem_open entry");
    if (men_sem == SEM_FAILED) die("sem_open men_sem");
    if (women_sem == SEM_FAILED) die("sem_open women_sem");

    pthread_t *men_t = malloc((size_t)NUM_MEN * sizeof(*men_t));
    pthread_t *women_t = malloc((size_t)NUM_WOMEN * sizeof(*women_t));
    if (!men_t || !women_t) die("malloc");
    printf("pthreads allocated\n");


    for (long i = 0; i < NUM_MEN; i++) pthread_create(&men_t[i], NULL, man, (void*)(intptr_t)i);
        printf("men threads created\n");

    for (long i = 0; i < NUM_WOMEN; i++) pthread_create(&women_t[i], NULL, woman, (void*)(intptr_t)i);
    printf("women threads created\n");

    for (int i = 0; i < NUM_MEN; i++) pthread_join(men_t[i], NULL);
    for (int i = 0; i < NUM_WOMEN; i++) pthread_join(women_t[i], NULL);
    //memory cleanup
    free(men_t);
    free(women_t);
    //semaphore close & unlink
    sem_close(entry); sem_close(men_sem); sem_close(women_sem);
    sem_unlink("/ub_entry"); sem_unlink("/ub_men"); sem_unlink("/ub_women");
    printf("done\n");

    return 0;
}
