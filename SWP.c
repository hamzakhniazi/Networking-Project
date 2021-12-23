//
// File: SWP.c
//
// Author: Hamza Sultan Khan Niazi
//
// Description: Implements the sliding window protocol defined in SWP.h
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <signal.h>
#include <fcntl.h>
#include "calcCRC16.h"
#include "unreliableSend.h"
#include <sys/file.h>   // for FASYNC
#include <sys/time.h>   // timer
#include <errno.h>
#include <stdio.h>
#include <string.h>  // memmove
#include <stdlib.h> // exit
#include <unistd.h>     // getpid, pause
#include "SWP.h"

// define constants and structs

#define SWP_PAYLOAD_SIZE  1024 /* this value MUST be a multiple of 4*/
#define SWP_TIMEOUT_SECS 0
#define SWP_TIMEOUT_USECS 250000
#define SWP_MAX_TIMEOUTS 25

// buffer constants
#define SWP_BUFSIZE 256

// structures for data and ack messages
struct SWP_dataMsg {
  unsigned char seqNum;
  int length;
  unsigned char data[SWP_PAYLOAD_SIZE];
  unsigned int crc;
};

struct SWP_ackMsg {
  unsigned char ackNum;
  unsigned int crc;
};

// define state variables

// status of the module
static int SWP_sendWait;    // true iff sender must wait for buffer space
static int SWP_recvWait;    // true iff receiver must wait for message

// socket variables and addresses
static int SWP_sendDataSock, SWP_recvDataSock;
static struct sockaddr_in SWP_sendDataAddr, SWP_recvDataAddr;

// sliding window bounds
// window sizes
static int SWP_SWS;
static int SWP_SendSize;
static int SWP_RWS;
static int SWP_ReceiveSize;

static int SWP_LAR;    // Last Acknowledgement Received
static int SWP_LFS;    // Last Frame Sent
static int SWP_LFR;    // Last Frame Received
static int SWP_LAF;    // Last Acceptable Frame
static int SWP_sendSlotsAvail;  // number of available slots in send window
static int SWP_lastFrameConsumed; // last frame sent to application

// buffers for sending and receiving data and acks
static struct SWP_dataMsg SWP_sendBuffer[SWP_BUFSIZE];
static struct SWP_dataMsg SWP_receiveBuffer [SWP_BUFSIZE];
static int SWP_frameReceived [SWP_BUFSIZE];

// buffers for received data not consumed yet
#define Q_DATASIZE 1000
struct QStruct {
  struct SWP_dataMsg data [Q_DATASIZE];
  int front;
  int rear;
  int size;
};
struct QStruct Q;

// number of timeouts for each message
static int SWP_numTimeouts [SWP_BUFSIZE];

// indicates if a timeout is set, and if so, when it expires
static int SWP_sendTimeoutSet [SWP_BUFSIZE];
static struct timeval SWP_sendTimeout [SWP_BUFSIZE];

// define prototypes for asynchronous handlers
static void SWP_ackSIGIO (int signalType);
static void SWP_sendTimer(int signalType);
static void SWP_dataSIGIO (int signalType);

// define prototypes for utility routines
static void SWP_setSendTimeout (int seqNum);
static void SWP_clearSendTimeout (int seqNum);
static int SWP_inWindow (int left, int right, int seq);

///////////////////////////////////////////////////////////////////////////////
//
// SWP_sendInit
//
///////////////////////////////////////////////////////////////////////////////
int SWP_sendInit (char *hostname,short portNum,int winSize)
{
  struct hostent *hp;
  struct sigaction handler1;
  struct sigaction handler2;
  struct itimerval timeVal;

  int i;

  // set window and sequence sizes
  if (winSize<1 || winSize>128)
    {
      printf ("Send window size out of range\n");
      return -1;
    }
  SWP_SWS = winSize;
  SWP_SendSize = 2 * winSize;

  // translate hostname into host's IP address
  hp = gethostbyname(hostname);
  if (!hp){
    perror ("sendInit: gethostbyname");
    return -1;
  }

  // build address data structures
  memset (&SWP_sendDataAddr, 0, sizeof(SWP_sendDataAddr));
  SWP_sendDataAddr.sin_family = AF_INET;
  memmove (&SWP_sendDataAddr.sin_addr, hp->h_addr_list[0], hp->h_length);
  SWP_sendDataAddr.sin_port = htons(portNum);

  // create send socket
  if((SWP_sendDataSock = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0){
    printf ("sendInit: socket error\n");
    return -1;
  }

  // set up SIGIO handler for received acks
  handler1.sa_handler = SWP_ackSIGIO;
  if (sigfillset (&handler1.sa_mask) < 0){
    printf ("sendInit: segfillset1 error\n");
    return -1;
  }
  handler1.sa_flags = 0;
  if (sigaction(SIGIO, &handler1, 0) < 0){
    printf ("sendInit: sigaction1 error\n");
    return -1;
  }

  if (fcntl(SWP_sendDataSock, F_SETOWN, getpid()) < 0){
    perror("sendInit:fcntl1 ");
    return -1;
  }
  if (fcntl(SWP_sendDataSock, F_SETFL, O_NONBLOCK|FASYNC) < 0){
    printf ("sendInit: fcntl2 error\n");
    return -1;
  }

  // no send timeouts yet
  for (i=0;i<SWP_SendSize;i++)
    SWP_sendTimeoutSet[i] = 0;

  // set up timer handler so it ticks every tenth of a second
  if (sigfillset (&handler2.sa_mask) < 0){
    printf ("sendInit: segfillset2 error\n");
    return -1;
  }

  handler2.sa_handler = SWP_sendTimer;
  handler2.sa_flags = 0;
  if (sigaction(SIGALRM, &handler2, 0) < 0){
    printf ("sendInit: sigaction3 error\n");
    return -1;
  }

  timeVal.it_interval.tv_sec = 0;
  timeVal.it_interval.tv_usec = 100000;
  timeVal.it_value.tv_sec = 0;
  timeVal.it_value.tv_usec = 100000;
  if (setitimer (ITIMER_REAL,&timeVal,0) < 0) {
    perror ("SWP_sendInit: setitimer error");
    return -1;
  }
  
  // initialize sending window 
  SWP_LAR = SWP_LFS = 0;
  SWP_sendSlotsAvail = SWP_SWS;

  // we're not waiting for buffer space to become available
  SWP_sendWait = 0;

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// SWP_send
//
///////////////////////////////////////////////////////////////////////////////
void SWP_send (char *buf, int length)
{
  sigset_t oldsigset,sigset;

  // wait until it's OK to proceed (i.e., we're not waiting for an ACK
  while (SWP_sendWait)
    pause();

  // can't send more than payload size
  if (length > SWP_PAYLOAD_SIZE)
    length = SWP_PAYLOAD_SIZE;

  // increment LFS, which will be the seqnum for this message
  SWP_LFS = (SWP_LFS + 1) % SWP_SendSize;

  // copy data into message buffer
  memmove (&SWP_sendBuffer[SWP_LFS].data, buf, length);
  SWP_sendBuffer[SWP_LFS].length = length;
  SWP_sendBuffer[SWP_LFS].seqNum = SWP_LFS;

  // *** calculate crc and place in SWP_sendBuffer.crc ***
  SWP_sendBuffer[SWP_LFS].crc = 0;
  SWP_sendBuffer[SWP_LFS].crc = 
    htonl(calcCRC((char *)&SWP_sendBuffer[SWP_LFS],
		  sizeof(SWP_sendBuffer[SWP_LFS])));

  // block SIGIO and SIGALRM so that we can't get a signal between
  // the sendto and setting the timers.
  sigemptyset (&sigset);
  sigaddset (&sigset,SIGALRM);
  sigaddset (&sigset,SIGIO);
  sigprocmask (SIG_BLOCK,&sigset,&oldsigset);

  // send the message
  US_sendto(SWP_sendDataSock,(char *)&SWP_sendBuffer[SWP_LFS],
	    sizeof(SWP_sendBuffer[SWP_LFS]),0,
	    (struct sockaddr *)&SWP_sendDataAddr,sizeof(SWP_sendDataAddr));

  // set timeout
  SWP_setSendTimeout (SWP_LFS);

  // no timeouts yet for this message
  SWP_numTimeouts[SWP_LFS] = 0;

  // alter status
  SWP_sendSlotsAvail--;
  SWP_sendWait = (SWP_sendSlotsAvail <= 0);

  // restore signal mask
  sigprocmask (SIG_SETMASK,&oldsigset,0);
}

///////////////////////////////////////////////////////////////////////////////
//
// SWP_flush
//
///////////////////////////////////////////////////////////////////////////////
void SWP_flush(void)
{
  while (SWP_sendSlotsAvail < SWP_SWS)
    pause ();
}


///////////////////////////////////////////////////////////////////////////////
//
// SWP_ackSIGIO
//
///////////////////////////////////////////////////////////////////////////////
void SWP_ackSIGIO (int signalType)
{
  // SIGIO callback for received ack
  int ackAddrSize;
  int ackSize;
  struct sockaddr_in SWP_recvAckAddr;
  struct SWP_ackMsg SWP_recvAck;
  struct itimerval timeVal;

  // receive messages
  while (1)
    {
      ackAddrSize = sizeof(SWP_recvAckAddr);
      ackSize = recvfrom(SWP_sendDataSock,
			 (char *)&SWP_recvAck,sizeof(SWP_recvAck),0,
			 (struct sockaddr *)&SWP_recvAckAddr,&ackAddrSize);

      // exit loop if no more acks have arrived
      if (ackSize == -1 && errno==EAGAIN)
	break;

      // discard ack if it's not the expected size
      if (ackSize != sizeof(SWP_recvAck)) {
#ifdef DEBUG
	printf("SWP_ackSIGIO:received ack not correct size\n");
#endif
	continue;
      }
      
      // discard ack if error in transmission
      // *** calculate crc of SWP_recvAck.  it should be zero. ***
      if (calcCRC((char *)&SWP_recvAck,sizeof(SWP_recvAck)) != 0)
	{
#ifdef DEBUG
	  printf ("SWP_ackSIGIO:received ack has bad crc\n");
#endif
	  continue;
	}
      
      // ignore if we weren't expecting this ack
      if (!SWP_inWindow (SWP_LAR,SWP_LFS,SWP_recvAck.ackNum))
	continue;
      
      // ack received so cancel timeouts for messages acked and adjust send 
      // window
      while (SWP_LAR != SWP_recvAck.ackNum)
	{
	  SWP_LAR = (SWP_LAR + 1) % SWP_SendSize;
	  SWP_clearSendTimeout (SWP_LAR);
	  SWP_sendSlotsAvail++;
	}
      
      // we can't be waiting for buffer space now
      SWP_sendWait = 0;
    }
}
 
///////////////////////////////////////////////////////////////////////////////
//
// SWP_sendTimer
//
///////////////////////////////////////////////////////////////////////////////
void SWP_sendTimer(int signalType)
{
  int i,j;
  struct itimerval timeVal;
  struct timeval currTime;
  // timer ticked, which means one tenth of a second has passed.

  // get current time
  gettimeofday (&currTime,0);

  // examine all sequence numbers to see if any timeouts have expired
  for (j=0,i=(SWP_LAR+1)%SWP_SendSize;
       j<SWP_SendSize;
       j++,i=(i+1)%SWP_SendSize)
    {
      // go on to next seqNum if this timer not even set
      if (!SWP_sendTimeoutSet[i])
	continue;

      // still nothing to do unless the timeout time has passed
      if (timercmp(&currTime,&SWP_sendTimeout[i],<))
	continue;

      // timeout has occurred, so handle it
      // increment number of timeouts
      SWP_numTimeouts[i]++;
      
      // if too many timeouts we'll just give up
      if (SWP_numTimeouts[i] > SWP_MAX_TIMEOUTS) {
	printf ("Too many timeouts - giving up\n");
	exit(1);
      }
      
      // resend message
      US_sendto(SWP_sendDataSock,(char *)&SWP_sendBuffer[i],
		sizeof(SWP_sendBuffer[i]),0,
		(struct sockaddr *)&SWP_sendDataAddr,sizeof(SWP_sendDataAddr));
#ifdef DEBUG
      printf ("SWP_SendTimeout: Resent message %d\n",j);
#endif

      // reset timeout
      SWP_setSendTimeout (i);
    }
}

///////////////////////////////////////////////////////////////////////////////
//
// SWP_setSendTimeout
//
///////////////////////////////////////////////////////////////////////////////
static void SWP_setSendTimeout (int seqNum)
{
  // set the send timeout time to be the current time + SWP_TIMEOUT

  sigset_t oldsigset,sigset;

  // first, block SIGALRM and SIGIO so the timer and asynchronous input
  // can't happen while we're playing with the timeout structures
  sigemptyset (&sigset);
  sigaddset (&sigset,SIGALRM);
  sigaddset (&sigset,SIGIO);
  sigprocmask (SIG_BLOCK,&sigset,&oldsigset);
  
  // get the current time
  gettimeofday (&SWP_sendTimeout[seqNum],0);

  // add SWP_TIMEOUT constants to the time to get the time the timeout
  // expires
  SWP_sendTimeout[seqNum].tv_usec += SWP_TIMEOUT_USECS;
  if (SWP_sendTimeout[seqNum].tv_usec >= 1000000)
    {
      SWP_sendTimeout[seqNum].tv_sec++;
      SWP_sendTimeout[seqNum].tv_usec -= 1000000;
    }
  SWP_sendTimeout[seqNum].tv_sec += SWP_TIMEOUT_SECS;

  // the timeout is now set
  SWP_sendTimeoutSet[seqNum] = 1;

  // restore signal mask
  sigprocmask (SIG_SETMASK,&oldsigset,0);
}

///////////////////////////////////////////////////////////////////////////////
//
// SWP_clearSendTimeout
//
///////////////////////////////////////////////////////////////////////////////
static void SWP_clearSendTimeout (int seqNum)
{
  // clear the send timeout

  sigset_t oldsigset,sigset;

  // first, block SIGALRM and SIGIO so the timer and asynchronous input
  // can't happen while we're playing with the timeout structures
  sigemptyset (&sigset);
  sigaddset (&sigset,SIGALRM);
  sigaddset (&sigset,SIGIO);
  sigprocmask (SIG_BLOCK,&sigset,&oldsigset);
  
  // the timeout is not set anymore
  SWP_sendTimeoutSet[seqNum] = 0;

  // restore signal mask
  sigprocmask (SIG_SETMASK,&oldsigset,0);
}

///////////////////////////////////////////////////////////////////////////////
//
// SWP_recvInit
//
///////////////////////////////////////////////////////////////////////////////
int SWP_recvInit (short portNum,int winSize)
{
  int i;
  struct sigaction handler;

  // set receive window and sequence sizes
  if (winSize<1 || winSize>128)
    {
      printf ("Receive Window size out of range\n");
      return -1;
    }
  SWP_RWS = winSize;
  SWP_ReceiveSize = 2 * winSize;

  // build address data structures
  memset (&SWP_recvDataAddr, 0, sizeof(SWP_recvDataAddr));
  SWP_recvDataAddr.sin_family = AF_INET;
  SWP_recvDataAddr.sin_addr.s_addr = INADDR_ANY;
  SWP_recvDataAddr.sin_port = htons(portNum);

  // create socket for receiving data and bind it to port
  if((SWP_recvDataSock = socket(PF_INET,SOCK_DGRAM,IPPROTO_UDP)) < 0){
    perror("recvInit:socket");
    return -1;
  }

  if (bind (SWP_recvDataSock,(struct sockaddr *)&SWP_recvDataAddr,
	    sizeof(SWP_recvDataAddr)) < 0){
    perror("recvInit:bind");
    return -1;
  }

  // set up SIGIO handler for received data
  handler.sa_handler = SWP_dataSIGIO;
  if (sigfillset (&handler.sa_mask) < 0){
    perror("recvInit:sigfillset");
    return -1;
  }
  handler.sa_flags = 0;
  if (sigaction(SIGIO, &handler, 0) < 0){
    perror("recvInit:sigaction:SIGIO");
    return -1;
  }

  if (fcntl(SWP_recvDataSock, F_SETOWN, getpid()) < 0){
    perror("recvInit:fcntl1 ");
    return -1;
  }
  if (fcntl(SWP_recvDataSock, F_SETFL, O_NONBLOCK|FASYNC) < 0){
    perror("recvInit:fcntl2 ");
    return -1;
  }

  // initialize receive window
  SWP_LFR = 0;
  SWP_LAF = SWP_RWS;

  // initialize Q
  Q.front = Q.rear = Q.size = 0;

  // no frames are in the buffer
  for (i=0;i<SWP_ReceiveSize;i++)
    SWP_frameReceived[i] = 0;

  // we're waiting for data
  SWP_recvWait = 1;

  return 0;
}

///////////////////////////////////////////////////////////////////////////////
//
// SWP_recv
//
///////////////////////////////////////////////////////////////////////////////
void SWP_recv (char *buf, int *length)
{
  // wait for message to come in
  while (SWP_recvWait)
    pause();

  // remove item from Q
  memmove (buf,&Q.data[Q.front].data,Q.data[Q.front].length);
  *length = Q.data[Q.front].length;
  Q.front = (Q.front + 1) % Q_DATASIZE;
  Q.size--;

  // we must wait for next message if no more frames in the buffer
  SWP_recvWait = (Q.size==0);
}

///////////////////////////////////////////////////////////////////////////////
//
// SWP_dataSIGIO
//
///////////////////////////////////////////////////////////////////////////////
void SWP_dataSIGIO (int signalType)
{
  // SIGIO callback for received data
while(1)
{
  int addrSize;
  int dataSize;
 struct sockaddr_in fromAddr;
 struct SWP_ackMsg  ackMsg;
 struct SWP_dataMsg tempMsg;

// receive message
 addrSize = sizeof(fromAddr);
 dataSize= recvfrom(SWP_recvDataSock,(char*)&tempMsg,sizeof(tempMsg),0,(struct sockaddr*)&fromAddr,&addrSize);

 if (dataSize ==-1 && errno==EAGAIN)
	break; 

 if (dataSize != sizeof(tempMsg)){
  
	printf("SWP_dataSIGIO:received data not correct size\n");
	 continue;
  }

 if(calcCRC((char*)&tempMsg,sizeof(tempMsg)) !=0)
	continue;


 while(SWP_inWindow(SWP_LFR,SWP_LAF,tempMsg.seqNum)){

	SWP_receiveBuffer[tempMsg.seqNum]=tempMsg;
	SWP_frameReceived[tempMsg.seqNum]=1;
	while (SWP_frameReceived[SWP_LFR+1]==1 && SWP_LFR !=SWP_LAF){

	Q.data[Q.front]=SWP_receiveBuffer[SWP_LFR+1];
	SWP_LFR++;

	}
  }

	ackMsg.ackNum=SWP_LFR;
	ackMsg.crc=0;
	ackMsg.crc=calcCRC((char*)&ackMsg,sizeof(ackMsg));
	US_sendto(SWP_recvDataSock,(char*)&ackMsg,sizeof(ackMsg),0,(struct sockaddr*)&fromAddr,sizeof(fromAddr));

	if (Q.size==0)
	SWP_recvWait=1;
 }
}

///////////////////////////////////////////////////////////////////////////////
//
// SWP_inWindow
//
///////////////////////////////////////////////////////////////////////////////
int SWP_inWindow (int left, int right, int seqNum)
{
  // returns true iff seqNum is between > left and <= right
  if (left <= right)
    return left<seqNum && seqNum<=right;
  else
    return left<seqNum || seqNum<=right;
}
