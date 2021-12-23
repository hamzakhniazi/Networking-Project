# Makefile for the Sliding Window Protocol project
#

all : unreliableSend.o SWP.o sender receiver 

sender: sender.c SWP.o unreliableSend.o
	gcc sender.c SWP.o unreliableSend.o -o sender

receiver: receiver.c SWP.o unreliableSend.o
	gcc receiver.c SWP.o unreliableSend.o -o receiver

unreliableSend.o: unreliableSend.c unreliableSend.h
	gcc -c unreliableSend.c
	
SWP.o: SWP.h SWP.c calcCRC16.h
	gcc -c SWP.c
		
clean:
	rm -f *.o sender receiver 
