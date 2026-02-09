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

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

sem_t bathroom; //semaphore to control access to bathroom
sem_t mutex; //semaphore to protect shared variables        

