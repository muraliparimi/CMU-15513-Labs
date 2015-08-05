/* 
 * CS:APP Data Lab 
 * 
 *
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#if 0
/*
 * Instructions to Students:
 *
 * STEP 1: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

INTEGER CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
  7. Use any data type other than int.  This implies that you
     cannot use arrays, structs, or unions.

 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }

FLOATING POINT CODING RULES

For the problems that require you to implent floating-point operations,
the coding rules are less strict.  You are allowed to use looping and
conditional control.  You are allowed to use both ints and unsigneds.
You can use arbitrary integer and unsigned constants.

You are expressly forbidden to:
  1. Define or use any macros.
  2. Define any additional functions in this file.
  3. Call any functions.
  4. Use any form of casting.
  5. Use any data type other than int or unsigned.  This means that you
     cannot use arrays, structs, or unions.
  6. Use any floating point data types, operations, or constants.


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. Use the BDD checker to formally verify your functions
  5. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.

/*
 * STEP 2: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the BDD checker to formally verify that your solutions produce 
 *      the correct answers.
 */


#endif
/*
 * bitOr - x|y using only ~ and &
 *   Example: bitOr(6, 5) = 7
 *   Legal ops: ~ &
 *   Max ops: 8
 *   Rating: 1
 */
int bitOr(int x, int y) {
  /* use only ~ and ~ to get | means
   * using a formula x & y = ~(~x | ~y)
   */
  return ~(~x & ~y);
}

/*
 * tmin - return minimum two's complement integer
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 4
 *   Rating: 1
 */
int tmin(void) {
  /* T_Min equals to 1 << 31 (word size = 32 bits)
   */
  return 1 << 31;
}
/*
 * negate - return -x
 *   Example: negate(1) = -1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int negate(int x) {
  /* negate a equals to (~a + 1)
   */
  return ~x + 1;
}
/*
 * getByte - Extract byte n from word x
 *   Bytes numbered from 0 (LSB) to 3 (MSB)
 *   Examples: getByte(0x12345678,1) = 0x56
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
int getByte(int x, int n) {
  /* byte n corresponds to bits range [8n, 8n + 7]
   * left shift x after getting the index of the first bit of byte n
   */
  x = x >> (n << 3);
  return x & 0xff;
}
/*
 * divpwr2 - Compute x/(2^n), for 0 <= n <= 30
 *  Round toward zero
 *   Examples: divpwr2(15,1) = 7, divpwr2(-33,4) = -2
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int divpwr2(int x, int n) {
    /* x/(2^n) = (x + 2^n - 1)/(2^n) if x < 0 and x/(2^n) if x > 0
     */
    int bias = (x >> 31) & ((1 << n) + ~1 + 1);
    return (x + bias) >> n;
}
/*
 * logicalShift - shift x to the right by n, using a logical shift
 *   Can assume that 0 <= n <= 31
 *   Examples: logicalShift(0x87654321,4) = 0x08765432
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 3
 */
int logicalShift(int x, int n) {
  /* if the first bit is 1,
   * try to change the new leading 1s to 0s.
   * use XOR to eliminate the extra 1s.
   */
  int shift = ((1 << 31) >> n) << 1;
  return ((x >> n) ^ shift) & (x >> n);
}
/*
 * isPositive - return 1 if x > 0, return 0 otherwise
 *   Example: isPositive(-1) = 0.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 8
 *   Rating: 3
 */
int isPositive(int x) {
  /* it is easy to use 1 to represent x > 0 and 0 otherwise
   * after changing x to -x
   */
  int bit = (x >> 31) & 1;
  x = ~x + 2 + ~bit;
  return (x >> 31) & 1;
}
/*
 * isLess - if x < y  then return 1, else return 0
 *   Example: isLess(4,5) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isLess(int x, int y) {
  /*
   *1. x < y , x and y have the same sign
   *2. x < 0 && y > 0
   */
  /* second method
  int bit1 = (x >> 31) & 1;
  int bit2 = (y >> 31) & 1;
  int diff = bit1 ^ bit2;
  int res = x + ~y + 1;
  int bit = (res >> 31) & 1;
  return (diff & bit1) + (bit & !diff);*/
  int bit1 = (x >> 31) & 1;
  int bit2 = (y >> 31) & 1;
  int flag1 = bit1 & !bit2;

  int res = x + ~y + 1;
  int bitOfRes = (res >> 31) & 1;
  int flag2 = ~(bit1 ^ bit2) & bitOfRes;
  return (flag1 | flag2);

}
/*
 * bang - Compute !x without using !
 *   Examples: bang(3) = 0, bang(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4
 */
int bang(int x) {
  /* only to judge if x is 0 which contains no 1s.
   * just to do or operations using all bits.
   */
  x = (x >> 16) | x;
  x = (x >> 8) | x;
  x = (x >> 4) | x;
  x = (x >> 2) | x;
  x = (x >> 1) | x;

  return (~x) & 1;
}
/*
 * isPower2 - returns 1 if x is a power of 2, and 0 otherwise
 *   Examples: isPower2(5) = 0, isPower2(8) = 1, isPower2(0) = 0
 *   Note that no negative number is a power of 2.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 20
 *   Rating: 4
 */
int isPower2(int x) {
  /* if x is a power of 2, then its bit representation
   * contains only a 1.
   * need to judge if x is negative or zero
   */
  int bit = (x >> 31) & 1;
  int y = x + ~1 + 1;
  int res = x & y;

  return !!x & !bit & !res;

}
/*
 * ilog2 - return floor(log base 2 of x), where x > 0
 *   Example: ilog2(16) = 4
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 90
 *   Rating: 4
 */
int ilog2(int x) {
  /* use binary_search to get all the 1s in bit representation
   * and add their positions.
   */
  int left = x >> 16;
  int isZero = !!left & 1;
  int count = 0;
  int tempShift = isZero << 4;
  count += tempShift;
  x = x >> tempShift;

  left = x >> 8;
  isZero = !!left & 1;
  tempShift = isZero << 3;
  count += tempShift;
  x = x >> tempShift;

  left = x >> 4;
  isZero = !!left & 1;
  tempShift = isZero <<  2;
  count += tempShift;
  x = x >> tempShift;

  left = x >> 2;
  isZero = !!left & 1;
  tempShift = isZero << 1;
  count += tempShift;
  x = x >> tempShift;

  left = x >> 1;
  isZero = !!left & 1;
  tempShift = isZero << 0;
  count += tempShift;
  x = x >> tempShift;

  return count;
}
/*
 * float_half - Return bit-level equivalent of expression 0.5*f for
 *   floating point argument f.
 *   Both the argument and result are passed as unsigned int's, but
 *   they are to be interpreted as the bit-level representation of
 *   single-precision floating point values.
 *   When argument is NaN, return argument
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_half(unsigned uf) {
  /* consider normalized, denormalized and special cases.
   */
  unsigned sign = (uf >> 31) & 1;
  unsigned newUf = uf & 0x7fffff;
  unsigned exponent = (uf >> 23) & 0xff;


  if (!newUf && !exponent) // all zeros
      return uf;
  if (exponent == 0xff) // NaN
      return uf;

  if (exponent > 1) {
    exponent --;
  } else if (exponent == 1) {
    exponent --;
    newUf += 1 << 23;
    if ((newUf & 0x3) == 0x3) // round to even
        newUf = (newUf >> 1) + 1;
    else //round to even and truncate directly
        newUf = newUf >> 1;
  } else { //round to even
    if ((newUf & 0x3) == 0x3)
        newUf = (newUf >> 1) + 1;
    else //round to even and truncate directly
        newUf = newUf >> 1;
  }

  return (sign << 31) | ((exponent << 23) & 0x7f800000) | newUf;

}
/* 
 * float_i2f - Return bit-level equivalent of expression (float) x
 *   Result is returned as unsigned int, but
 *   it is to be interpreted as the bit-level representation of a
 *   single-precision floating point values.
 *   Legal ops: Any integer/unsigned operations incl. ||, &&. also if, while
 *   Max ops: 30
 *   Rating: 4
 */
unsigned float_i2f(int x) {
  /* first convert x to unsigned in order to do logical right shift
   * then find the first 1 and get the exponent and mantissa
   * count the actual exponent and do truncation
   */
  int xx = x;
  unsigned sign, exp, mantissa;
  int first = 0;
  int temp;
  int left;
  unsigned tempM, tail;

  sign = (xx >> 31) & 0x1;

  if (sign == 1)
      xx = ~xx + 1;
  temp = xx;
  while (first != 32 && !(temp & 0x80000000)) { //find the first 1
      first ++;
      temp = temp << 1;
  }
  if (first == 32) { //can not find 1, means zero
      exp = 0x00;
      mantissa = 0x0;
  } else {
      left = 31 - first; //count how many bits are left
      exp = left +  127;
      mantissa = xx << (first + 1);
      tempM = mantissa >> 9;
      tail = mantissa & 0x1ff;
      if (tail > 0x100) { //round up
          mantissa  = tempM + 1;
      } else if (tail < 0x100){ //round down
          mantissa = tempM;
      } else {
          if ((mantissa & 0x200) >> 9) //round up
              mantissa = tempM + 1;
          else
              mantissa = tempM; //round down
      }
      if (mantissa >> 23) { //mantissa becomes 0 after round up
         exp ++;
         mantissa = 0;
      }
  }
  return (sign << 31) | (exp << 23) | mantissa;

}
