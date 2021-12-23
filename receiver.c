// File: server2.c

#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdlib.h>  // exit
#include <ctype.h>   // isprint
#include "SWP.h"
#include "unreliableSend.h"

#define BUF_SIZE 1024
#define MAX_PENDING 5
#define SERVER_PORT 50000
int main (int argc, char *argv[]) {
  char buf[BUF_SIZE];
  int len;
  int i,j;
  int port;
  int errorRate;
  int winSize;
  
  // get command line argument
  if (argc == 4)
    {
      port = atoi(argv[1]);
      winSize = atoi(argv[2]);
      errorRate = atoi(argv[3]);
    }
  else
    {
      printf ("usage:receiver <serverPort> <RecvWinSize> <errorRate>\n");
      exit (1);
    }
  
  // intialize receiver
  if(SWP_recvInit(port,winSize)<0)
    printf ("recvinit failed\n");
  
  // set failure probability for acks
  US_SetFailureProb (errorRate);
  
  // read in 1MB of data from client
  for (i=0;i<1024;i++)
    {
      // read in next packet
      SWP_recv (buf,&len);
      
      if ((i%100) == 0)
	printf ("Received packet %d\n",i);
      
      // verify that it is what we expect
      if (len != BUF_SIZE)
	{
	  printf ("length error.  Expected %d, received %d.\n",BUF_SIZE,len);
	  exit(1);
	}
      
      for (j=0;j<1024;j++)
	if (buf[j] != ('A'+i%26))
	  {
	    if (isprint(buf[j]))
	      printf ("Data error.  Expected '%c'(%2x), received '%c' (%2x).",
		      ('A'+i%26),('A'+i%26),buf[j],buf[j]);
	    else
	      printf ("Data error.  Expected '%c'(%2x), received '%c' (%2x).",
		      ('A'+i%26),('A'+i%26),' ',buf[j]&0xff);
	    
	    printf (" Position=%d.\n",j);
	    printf ("Rest of packet:\n");
	    for (;j<1024;j++)
	      {
		if (j%8 == 0)
		  printf("\n%4d:",j);
		if (isprint(buf[j]))
		  printf ("'%c'(%2x) ",buf[j],buf[j]);
		else
		  printf ("' '(%2x) ",buf[j]&0xff);
	      }
	    printf ("\n");
	    exit(1);
	  }
    }
  
  // delay a bit in case there are ACKs that still need sent back to client
  printf ("Please press enter.");
  fgets (buf,BUF_SIZE,stdin);
  
}

