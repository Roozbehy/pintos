/*
 * Fixed_Point_Arithmetic.h
 *
 *  Created on: 11 Feb 2013
 *      Author: rz1511
 */

// X and Y are fixed-point numbers, N is an integer
#ifndef FIXED_POINT_ARITHMETIC_H_
#define FIXED_POINT_ARITHMETIC_H_

#define F (1 << 14) // (2^14)
#define IntToFloat(N) (N*F) //convert N to Fixed Point
#define FloatToIntZero(X) (X/F) //Convert x to integer (rounding toward zero)
#define FloatToIntNearest(X) ((X>=0) ? ( (X+F/2)/F ) : ( (X-F/2)/F) ) //Convert x to integer (rounding to nearest)
#define FloatPlusFloat(X,Y) (X+Y)
#define FloatMinusFloat(X,Y) (X-Y)
#define FloatPlusInt(X,N) (X+(N*F))
#define FloatMinusInt(X,N) (X-(N*F))
#define FloatMultFloat(X,Y) (((int64_t)X)*Y/F)
#define FloatMultInt(X,N) (X*N)
#define FloatDivFloat(X,Y) (((int64_t)X)*F/(Y))
#define FloatDivInt(X,N) (X/N)

#endif /* FIXED_POINT_ARITHMETIC_H_ */
