#ifndef BITWISE_H /* guard */
#define BITWISE_H

/*
    This file contain two bitwise operations
*/

/*
  get the i-th(i) bit of a number(num)
  i is from right to left(0 to 7 for a 8 bit number)
*/
#define ith_bit(num, i) ((num >> (i)) & 1)

/*
  modify the bit of number n at p-th bit to value b, b is 0 or 1
  i is from right to left(0 to 7 for a 8 bit number)
*/
int modify_bit(int n, int p, int b);

#endif //ID_STORAGE_H
