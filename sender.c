// File: client2.c

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/time.h>
#include <stdlib.h>  // exit
#include "SWP.h"
#include "unreliableSend.h"

#define SERVER_PORT 50000
#define BUF_SIZE 1024

int main (int argc, char *argv[]) {
  char *host;
  char buf[BUF_SIZE];
  int port;
  int errorRate;
  int winSize;
  int i,j;
  float xferTime;
  struct timeval startTime,stopTime;

  // get arguments from command line
  if (argc==5) {
    host = argv[1];
    port = atoi(argv[2]);
    winSize = atoi(argv[3]);
    errorRate = atoi(argv[4]);
  }
  else {
    printf("usage: sender <hostname> <ServerPort> <SendWinSize> <errorRate>\n");
    exit (1);
  }

  // initialize stopwait send
  if(SWP_sendInit(host,port,winSize)){
    printf("sendInit Failed\n");
    exit (1);
  }

  // set failure probability of outgoing packets
  US_SetFailureProb (errorRate);

  // send 1MB to server, keeping track of the time
  gettimeofday (&startTime,0);
  for (i=0;i<1024;i++)
    {
      // fill buffer
      for (j=0;j<BUF_SIZE;j++)
	buf[j] = 'A' + i%26;

      // send the buffer
      SWP_send (buf,BUF_SIZE);
    }

  // wait for all messages to be sent
  SWP_flush ();
  gettimeofday (&stopTime,0);

  // calculate and print the length of time it took
  xferTime = (stopTime.tv_sec+stopTime.tv_usec*0.000001) -
    (startTime.tv_sec+startTime.tv_usec*0.000001);
  printf ("The transfer took %6.3f seconds.\n",xferTime);

}

    
  
