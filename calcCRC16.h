// File: calcCRC16.h
//
// Author: Hamza Sultan Khan Niazi
//
// Description:  function to calculate a CRC using CRC-16 polynomial
//
//
#ifndef _CALCCRC_H
#define _CALCCRC_H

#define CRC_POLY  0x18005

int calcCRC (unsigned char *buf,int length)
{
  unsigned char curr;
  unsigned int rem = 0;
  int mask;
  int i;

  // process all bytes in buffer
  for (i=0;i<length;i++) {
    // store current byte
    curr = buf[i];

    // now process all bits in the current byte, from high to low
    for (mask=0x80; mask!=0; mask=mask>>1) {
      // shift current remainder over and add in new bit from buffer
      rem = rem << 1;
      if ((curr & mask) != 0)
	rem++;

      // subtract crc polynomial if it divides into remainder
      if ((rem & 0x10000) != 0)
	rem ^= CRC_POLY;
    }
  }

  // crc calculated in rem, so return it
  return rem;
}


#endif
