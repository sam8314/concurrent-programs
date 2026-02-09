/*
Suppose there is only one bathroom in your department. Any number of men or women can use it, but not simultaneously.
Develop and implement a multithreaded program that provides a fair solution to this problem using only semaphores for
 synchronization. Represent men and women as threads that repeatedly work (sleep for a random amount of time) and use 
 the bathroom. Your program should allow any number of men or women to be in the bathroom simultaneously. Your solution 
 should ensure the required exclusion, avoid deadlock, and ensure fairness, i.e., ensure that any person (man or woman) 
 waiting to enter the bathroom eventually gets to do so. Have the persons sleep for a random amount of time between 
 visits to the bathroom and have them sleep for a smaller random amount of time to simulate the time it takes to be 
 in the bathroom. Your program should print a trace of interesting simulation events.
*/

/*
A passing the baton reader-writer program using only semaphore to represente a unisex bathroom situation

    usage : 
    gcc -o b bathroom.c -lpthread
    b 7 12
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

#define SHARED 1

/* shared state */
int men_in = 0;
int women_in = 0;
int men_waiting = 0;
int women_waiting = 0;

/* semaphores */
sem_t entry; //controls access to shared state and the baton
sem_t men_sem; //queue for waiting men 
sem_t women_sem; //queue for waiting women

int NUM_MEN, NUM_WOMEN;

/*random sleep helpers */
static void think_or_work() {
    usleep(100000 + (rand() % 400000)); //0.1 to 0.5 s
}
static void use_bathroom() {
    usleep(50000 + (rand() % 150000)); //0.05 to 0.2 s
}

/*decide who gets the baton next */
static void pass_baton() {
    if (men_waiting > 0 && women_in == 0 && women_waiting == 0) {
        men_waiting--;
        sem_post(&men_sem);
    } else if (women_waiting > 0 && men_in == 0 && men_waiting == 0) {
        women_waiting--;
        sem_post(&women_sem);
    } else {sem_post(&entry);}
}

void *man(void *arg) {
    long id = (long)arg;
    while (1) {
        think_or_work();

        /* try to enter bathroom */
        sem_wait(&entry);
        if (women_in > 0 || women_waiting > 0) {
            /* someone of opposite sex inside or already queued => wait */
            men_waiting++;
            sem_post(&entry);
            sem_wait(&men_sem); //wait for baton
        }

        /* got permission, we own the baton here */
        men_in++;
        printf("man %ld ENTERS, men_in=%d\n", id, men_in);
        pass_baton();
        use_bathroom();

        /* leaving bathroom */
        sem_wait(&entry);
        men_in--;
        printf("man %ld LEAVES, men_in=%d\n", id, men_in);
        pass_baton();
    }
    return NULL;
}

void *woman(void *arg) {
    long id = (long)arg;
    while (1) {
        think_or_work();

        /* try to enter bathroom */
        sem_wait(&entry);
        if (men_in > 0 || men_waiting > 0) {
            women_waiting++;
            sem_post(&entry);
            sem_wait(&women_sem); //wait for baton
        }
        //got permission, we own the baton here
        women_in++;
        printf("woman %ld ENTERS, women_in=%d\n", id, women_in);
        pass_baton(); //let other women (if any) in, or release entry
        use_bathroom(); //in bathroom
        
        sem_wait(&entry); //leave bathroom
        women_in--;
        printf("woman %ld LEAVES, women_in=%d\n", id, women_in);
        pass_baton();
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 3) {exit(1);}
    NUM_MEN = atoi(argv[1]);
    NUM_WOMEN = atoi(argv[2]);

    srand(time(NULL));//seed for random

    sem_init(&entry, SHARED, 1);
    sem_init(&men_sem, SHARED, 0);
    sem_init(&women_sem, SHARED, 0);

    pthread_t *men_t = malloc(NUM_MEN * sizeof(pthread_t));
    pthread_t *women_t = malloc(NUM_WOMEN * sizeof(pthread_t));
    printf("pthreads allocated\n");

    for (long i = 0; i < NUM_MEN; i++)
        pthread_create(&men_t[i], NULL, man, (void *)i);
    printf("men threads created\n");

    for (long i = 0; i < NUM_WOMEN; i++)
        pthread_create(&women_t[i], NULL, woman, (void *)i);
    printf("women threads created\n");

    for (int i = 0; i < NUM_MEN; i++) pthread_join(men_t[i], NULL);
    for (int i = 0; i < NUM_WOMEN; i++) pthread_join(women_t[i], NULL);

    printf("done\n");
    return 0;
}
      

