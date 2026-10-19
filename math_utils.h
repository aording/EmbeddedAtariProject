/*
 * math_utils.h
 *
 *  Created on: Apr 3, 2021
 *      Author: Thoma
 */

#ifndef MATH_UTILS_H_
#define MATH_UTILS_H_

#include <msp430fr5994.h>
#include "GrLib/grlib/grlib.h"

#define COLLIDING_NORTH BIT0
#define COLLIDING_EAST  BIT1
#define COLLIDING_SOUTH BIT2
#define COLLIDING_WEST  BIT3

char IsCollidingAABB(Graphics_Rectangle *r1, Graphics_Rectangle *r2);
char IsCollidingWalls(Graphics_Rectangle *ball);
char IsNullBlock(Graphics_Rectangle *block);

#endif /* MATH_UTILS_H_ */
