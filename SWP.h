//
// File: SWP.h
//
// Author: Hamza Sultan Khan Niazi
//
// Description: This is a simple implementation of reliable transmission
// that uses the sliding window protocol.
// UDP datagrams are used to send data packets and acknowledgements.
//
// The following functions are defined:
//    SWP_sendInit (char *hostname,int portNum)
//    SWP_send (char *buf, int length)
//    SWP_flush (void);
//
//    SWP_recvInit (int portNum)
//    SWP_recv (char *buf, int *length)

#ifndef _SWP_H_
#define _SWP_H_

int SWP_sendInit (char *hostname, short portNum, int WindowSize);
// initializes the SWP protocol so that messags subsequently sent using
// SWP_send will be sent to the SWP protocol running on hostname using UDP
// port portnum.  The sending window size is WindowSize, which must be
// between 1 and 128 (inclusive).
//
// A negative return value indicates an error.

void SWP_send (char *buf, int length);
// sends a message to the host specified when SWP_sendInit was called. length
// bytes will be sent, starting at buf.  SWP_send may return before the 
// message is sent, but SWP_send will make a copy of the message so the caller
// can change the buffer.  Currently there is no way for the caller to verify
// that the message was successfully sent.

void SWP_flush (void);
// does not return until all previously sent message have been successfully
// delivered

int SWP_recvInit (short portNum,int WindowSize);
// initializes the SWP protocol to receive messages on UDP port portnum.  The
// receive window size is WindowSize, which must be between 1 and 128 
// (inclusive).
//
// A negative return value indicates an error.

void SWP_recv (char *buf, int *length);
// receive a message using the SWP protocol.  On entry, buf is a pointer to
// a buffer of at least length bytes.  On return length contains the number
// of bytes actually read.
#endif
