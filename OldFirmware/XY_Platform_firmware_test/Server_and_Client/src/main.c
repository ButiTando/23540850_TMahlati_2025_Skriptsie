/// \file main.h
/// \brief iThemba RaPi Application entry point
///
/// \details iThemba RaPi Application entry point
///
/// \author Kevin Gema
/// \date 18-Apr-2016
/// \version $Id$
//-------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------
// Includes
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/time.h>
#include <fcntl.h>
#include <pthread.h>

#include "tcp_server.h"
#include "version.h"
#include "queueManager.h"
#include "types.h"

//-------------------------------------------------------------------------------------------------
// Defines

//-------------------------------------------------------------------------------------------------
// Function Definitions


void timer_handler (int signum)
{
	static int count = 0;
	printf ("timer expired %d times\n", ++count);
}

void error(char *msg) {
	perror(msg);
	exit(1);
}


/* Start a virtual timer. It counts down whenever this process is 
	executing. */

int main(int argc, char **argv)
{
	
	printf("%s v%d.%d Build: %s %s ", APP_NAME, MAJOR_VERSION, MINOR_VERSION, BUILD_DATE, BUILD_TIME);    
	
	//InitTimer(&sa, &timer, &timer_handler, 1, 0);	
	TCP_InitialiseServer();
	
    startThread();
        
       
	
    while (1)
    {           

		TCP_ServiceServer();

		TCP_ServiceClients();
		
		printf("Main running");
    }
	
    IO_Close();
    return 0;
}