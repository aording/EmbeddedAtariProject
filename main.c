#include <msp430fr5994.h>
#include <stdint.h>
#include "Grlib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"
#include "graphics.h"

Graphics_Context g_sContext;

// SCREEN
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       128

// PADDLE
#define PADDLE_WIDTH        24
#define PADDLE_HEIGHT       4
#define PADDLE_Y            108
#define PADDLE_MAX_X        (SCREEN_WIDTH - PADDLE_WIDTH)

// BALL
#define BALL_X              64
#define BALL_Y              102
#define BALL_RADIUS         2

// BRICKS
#define BRICK_ROWS          4
#define BRICK_COLUMNS       6

#define BRICK_WIDTH         18
#define BRICK_HEIGHT        6

#define BRICK_X_GAP         2
#define BRICK_Y_GAP         3

#define BRICK_START_X       5
#define BRICK_START_Y       12

// STATUS AREA
#define STATUS_LINE_Y       116


// JOYSTICK CALIBRATION

#define ADC_LEFT            3
#define ADC_CENTER          2027
#define ADC_RIGHT           4035

unsigned int Xoutput;

int main(void)
{
    Graphics_Rectangle paddle;
    Graphics_Rectangle brick;

    unsigned int row;
    unsigned int column;

    int brickX;
    int brickY;

    int paddleX;
    int oldPaddleX;


    // STOP WATCHDOG TIMER
    WDTCTL = WDTPW | WDTHOLD;

    // POWER / CLOCK
    P5DIR |= BIT4;
    P5OUT |= BIT4;

    // Configure SMCLK for LCD SPI
    CSCTL0 = CSKEY;
    CSCTL3 &= ~(BIT4 | BIT5 | BIT6);
    CSCTL0_H = 0;

    // JOYSTICK X PIN
    // P1.2 = A2 = horizontal joystick
    P1SEL0 |= BIT2;
    P1SEL1 |= BIT2;

    // Disable GPIO high-impedance mode
    PM5CTL0 &= ~LOCKLPM5;

    // LCD INITIALIZATION
    Crystalfontz128x128_Init();
    Initialize_Graphics(&g_sContext);
    Graphics_clearDisplay(&g_sContext);

    // DRAW BRICKS

    for (row = 0; row < BRICK_ROWS; row++)
    {
        for (column = 0; column < BRICK_COLUMNS; column++)
        {
            brickX =
                BRICK_START_X +
                column * (BRICK_WIDTH + BRICK_X_GAP);

            brickY =
                BRICK_START_Y +
                row * (BRICK_HEIGHT + BRICK_Y_GAP);

            brick.xMin = brickX;
            brick.xMax = brickX + BRICK_WIDTH - 1;

            brick.yMin = brickY;
            brick.yMax = brickY + BRICK_HEIGHT - 1;

            Graphics_fillRectangle(
                &g_sContext,
                &brick
            );
        }
    }

    // DRAW BALL
    Graphics_fillCircle(&g_sContext, BALL_X,  BALL_Y,  BALL_RADIUS);


    // DRAW STATUS SEPARATOR
    Graphics_drawLine( &g_sContext, 0, STATUS_LINE_Y, 127, STATUS_LINE_Y);

    // DRAW LIVES
    Graphics_drawString(&g_sContext, (int8_t *)"Lives:", AUTO_STRING_LENGTH, 4, 119, OPAQUE_TEXT );
    Graphics_fillCircle(&g_sContext, 77, 123, 2);
    Graphics_fillCircle(&g_sContext, 87, 123, 2);
    Graphics_fillCircle(&g_sContext, 97, 123, 2);


    // ADC12 INITIALIZATION
    ADC12CTL0 &= ~ADC12ENC;
    ADC12CTL0 = ADC12SHT0_2 | ADC12ON;
    ADC12CTL1 =ADC12SHP | ADC12SSEL_3;
    ADC12CTL2 = ADC12RES_2;

    // Joystick X = P1.2 / A2
    ADC12MCTL0 = ADC12VRSEL_0 | ADC12INCH_2;
    ADC12CTL0 |= ADC12ENC;

    // INITIAL PADDLE POSITION
    paddleX = 52;
    oldPaddleX = 52;
    paddle.xMin = paddleX;
    paddle.xMax = paddleX + PADDLE_WIDTH - 1;
    paddle.yMin = PADDLE_Y;
    paddle.yMax = PADDLE_Y + PADDLE_HEIGHT - 1;
    Graphics_fillRectangle(&g_sContext, &paddle);

    // MAIN LOOP
    while (1)
    {
        // START ADC CONVERSION
        ADC12CTL0 |= ADC12SC;
        // WAIT FOR ADC
        while (ADC12CTL1 & ADC12BUSY)
        {
        }

        // READ JOYSTICK
        Xoutput = ADC12MEM0;
        // LIMIT ADC VALUE
        if (Xoutput < ADC_LEFT)
        {
            Xoutput = ADC_LEFT;
        }

        if (Xoutput > ADC_RIGHT)
        {
            Xoutput = ADC_RIGHT;
        }

        // MAP ADC TO PADDLE POSITION
        paddleX = ((uint32_t)(Xoutput - ADC_LEFT) * PADDLE_MAX_X) /  (ADC_RIGHT - ADC_LEFT);

        // ONLY REDRAW IF PADDLE MOVED
        if (paddleX != oldPaddleX)
        {
            // ERASE OLD PADDLE
            paddle.xMin = oldPaddleX;
            paddle.xMax = oldPaddleX + PADDLE_WIDTH - 1;
            paddle.yMin = PADDLE_Y;
            paddle.yMax = PADDLE_Y + PADDLE_HEIGHT - 1;

            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_BLACK);

            Graphics_fillRectangle( &g_sContext, &paddle);

            // DRAW NEW PADDLE
            paddle.xMin = paddleX;
            paddle.xMax =paddleX + PADDLE_WIDTH - 1;

            Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
            Graphics_fillRectangle(&g_sContext, &paddle);

            // SAVE CURRENT POSITION
            oldPaddleX = paddleX;
        }
        // Small delay
        __delay_cycles(20000);
    }
}
