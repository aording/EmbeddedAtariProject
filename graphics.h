/*
 * graphics.h
 *
 *  Created on: Apr 3, 2021
 *      Author: Thoma
 */

#ifndef GRAPHICS_H_
#define GRAPHICS_H_

#include "Grlib/grlib/grlib.h"      // Graphics Library Import
#include "LcdDriver/lcd_driver.h"
#include "math_utils.h"
#include <msp430fr5994.h>
#include <stdio.h>

#define PADDLE_WIDTH   20
#define PADDLE_HEIGHT   5
#define PADDLE_Y      110
#define BALL_RADIUS     2

/*
 * Initialize the default settings for our graphics context and screen
 */
void Initialize_Graphics(Graphics_Context * context);
void Draw_Playspace(Graphics_Context *context, Graphics_Rectangle *blocks, int32_t *colors, char* bindings, int numBlocks, int lives, int levelNumber);
void Draw_Paddle(Graphics_Context *context, int paddle_x, int32_t color);
void Draw_Ball(Graphics_Context *context, int ball_x, int ball_y, int32_t color);
void Draw_EdgedBox(Graphics_Context *context, Graphics_Rectangle *rect, int inset, int32_t backgroundColor, int32_t foregroundColor);
void Draw_Points(Graphics_Context *context, unsigned int points);
void Draw_ModalBox(Graphics_Context *context, const char* line1, const char* line2, char showPoints, int points);

#endif /* GRAPHICS_H_ */
