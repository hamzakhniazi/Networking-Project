//
// File: unreliableSend.h
//
// Author: Hamza Sultan Khan Niazi
//
// Description: Define functions for an unreliable network.  The following
// functions are defined:
//
//    US_SetFailureProb (int newProb)
//    US_send (int s,const char *msg,int len,int flags)
//    US_sendto (int s, const char *msg, int len, int flags,
//               struct sockaddr *to, int tolen)
//
// The behavior of US_send and US_sendto are identical to send and sendto
// except that packets are randomly dropped.  These simulate unreilable links.
//
#ifndef _UNRELIABLE_SEND_H
#define _UNRELIABLE_SEND_H

void US_SetFailureProb (int newProb);
int US_send(int s, const char *msg, int len, int flags);
int US_sendto(int s, const char *msg, int len, int flags,
	      struct sockaddr *to, int tolen);
#endif
