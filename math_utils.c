/*
 * math_utils.c
 *
 *  Created on: Apr 3, 2021
 *      Author: Thoma
 */

#include "math_utils.h"

char IsCollidingAABB(Graphics_Rectangle *r1, Graphics_Rectangle *r2) {
    // Returns 0 if not colliding
    // If it is colliding, it'll return a char with bits set for what direction it is colliding in
    char result = 0;

    if(r1->yMin < r2->yMax &&
       r1->yMax > r2->yMin &&
       r1->xMin < r2->xMax &&
       r1->xMax > r2->xMin) {

        // We know we're colliding, now. We just need to find out how.
        if(r2->yMin <= r1->yMin && r1->yMin <= r2->yMax) result |= COLLIDING_NORTH;
        if(r2->yMin <= r1->xMax && r1->xMax <= r2->yMax) result |= COLLIDING_EAST;
        if(r2->yMax <= r1->yMax && r1->yMax <= r2->yMax) result |= COLLIDING_SOUTH;
        if(r2->xMin <= r1->xMin && r1->xMin <= r2->yMax) result |= COLLIDING_WEST;

    }

    return result;
}

/*
 * Returns a char of bitflags representing the collision status of the
 *  rectangle with respect to the screen's walls. If no collision, the
 *  result is zero. Otherwise, bitwise-and the result to find the collision
 *  direction
 */
char IsCollidingWalls(Graphics_Rectangle *ball) {
    char result = 0;

    if(ball->yMin < 0)   result |= COLLIDING_NORTH;
    if(ball->xMax > 127) result |= COLLIDING_EAST;
    if(ball->yMax > 117) result |= COLLIDING_SOUTH;
    if(ball->xMin < 0)   result |= COLLIDING_WEST;

    return result;
}

char IsNullBlock(Graphics_Rectangle *block) {
    if(block->xMin == 128) return 1;
    return 0;
}
