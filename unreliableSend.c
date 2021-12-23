//
// File: unreliableSend.c
//
// Author: Hamza Sultan Khan Niazi
//
// Description: Implementation of functions that implement an unreliable
// UDP connection.
//
#include <stdlib.h> // rand
#include <sys/types.h>
#include <sys/socket.h>
#include "unreliableSend.h"
#include <time.h> 
#include <string.h> // memmove

// define state variables

static int US_FailureProb = 0;  //prob of failure, initially 0

// define probabilities of various errors.  This could be more general (i.e.,
// allow the user to set these), but these should suffice for now. The
// probabilities should add up to 100.
#define US_DROP_PROB            20
#define US_BURST_ERROR_PROB     20
#define US_ONE_BIT_ERROR_PROB   20
#define US_TWO_BIT_ERROR_PROB   20
#define US_THREE_BIT_ERROR_PROB 20

// prototypes for local functions
static int US_garble (char *msg, int len);

///////////////////////////////////////////////////////////////////////////////
//
// US_SetFailureProb
//
///////////////////////////////////////////////////////////////////////////////
void US_SetFailureProb (int newProb)
{
  // set failure to newProb
  US_FailureProb = newProb;

  // start a new random seed
  srand (time(0));
}

///////////////////////////////////////////////////////////////////////////////
//
// US_send
//
///////////////////////////////////////////////////////////////////////////////
int US_send(int s, const char *msg, int len, int flags)
{
  char garbledMsg[2048];

  if (rand()%100 >= US_FailureProb)
    // we're not causing an error in this packet so send it off normally
    return send (s,msg,len,flags);

  // copy the message to a temporary buffer, then garble it  and send it,
  // unless it was completely dropped
  memmove (&garbledMsg,msg,len);
  if (US_garble(garbledMsg,len))
    send (s,garbledMsg,len,flags);

  // return as if everything was sent off
  return len;
}

///////////////////////////////////////////////////////////////////////////////
//
// US_sendto
//
///////////////////////////////////////////////////////////////////////////////
int US_sendto(int s, const char *msg, int len, int flags,
	       struct sockaddr *to, int tolen)
{
  char garbledMsg[2048];

  if (rand()%100 >= US_FailureProb)
    // we're not causing an error in this packet so send it off normally
    return sendto(s,msg,len,flags,to,tolen);

  // copy the message to a temporary buffer, then garble it  and send it,
  // unless it was completely dropped
  memmove (&garbledMsg,msg,len);
  if (US_garble(garbledMsg,len))
    sendto (s,garbledMsg,len,flags,to,tolen);

  // return as if everything was sent off
  return len;
}

///////////////////////////////////////////////////////////////////////////////
//
// US_garble
//
///////////////////////////////////////////////////////////////////////////////
static int US_garble (char *msg, int len)
{
  // garble the message.  Probabilities of various errors are defined
  // by global constants.

  int randNum = rand()%100;
  int burstStart,burstEnd;
  int randByte,randBit;
  int numBits;
  int i;

  // dropped packet
  if (randNum < US_DROP_PROB)
    {
      // simulate a dropped packet.  Return indication that nothing should
      // be sent out.
#ifdef DEBUG
      printf ("dropped packet\n");
#endif
      return 0;
    }
  randNum -= US_DROP_PROB;

  // burst error
  if (randNum < US_BURST_ERROR_PROB)
    {
      // simulate a random length burst error.  All bits in the burst are
      // set to 1
      burstStart = rand()%len;
      burstEnd = rand()%(len-burstStart)+burstStart+1;
      for (i=burstStart;i<=burstEnd;i++)
	msg[i] = 0xff;
#ifdef DEBUG
      printf ("burst error\n");
#endif
      return 1;
    }
  randNum -= US_BURST_ERROR_PROB;

  // it must be a 1, 2, or 3 bit error
  if (randNum < US_ONE_BIT_ERROR_PROB)
    numBits = 1;
  else
    {
      randNum -= US_ONE_BIT_ERROR_PROB;
      if (randNum < US_TWO_BIT_ERROR_PROB)
	numBits = 2;
      else
	numBits = 3;
    }

  for (i=0;i<numBits;i++)
    {
      randByte = rand()%len;
      randBit = rand()%8;
      msg[randByte] ^= (0x01 << randBit);
    }
#ifdef DEBUG
  printf ("%d bit error\n",numBits);
#endif
  return 1;
}
