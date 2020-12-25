#include "bitwise.h"

/*
  modify the bit of number n at p-th bit to value b, b is 0 or 1
  i is from right to left(0 to 7 for a 8 bit number)
*/
int modify_bit(int n, int p, int b) {
  int mask = 1 << p;
  return (n & ~mask) | ((b << p) & mask);
}
