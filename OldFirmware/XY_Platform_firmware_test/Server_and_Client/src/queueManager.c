/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */
#include <pthread.h>
#include <stdio.h>

void *queueWatcher (void *dummy)
{
    while (1)
    {
        if (!isEmpty())
        {
            print("Running queue action");
            // TELECOMMAND_Parser(removeData());
        }
    }
}

void startThread ()
{
   pthread_t queueManagerThread; 
   int dummy = 1;
   if (pthread_create(&queueManagerThread, NULL, queueWatcher, &dummy)) 
   {

    printf("Error creating thread\n");

    }
}

