
#include <msp430fr5994.h>
#include <stdint.h>
#include "Grlib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"
#include "graphics.h"

Graphics_Context g_sContext;

#define SCREEN_WIDTH        128
#define STATUS_LINE_Y       116
#define GAME_BOTTOM         115

#define PADDLE_WIDTH        24
#define PADDLE_HEIGHT       4
#define PADDLE_Y            108
#define PADDLE_MAX_X        (SCREEN_WIDTH - PADDLE_WIDTH)

#define BALL_RADIUS         2

#define BRICK_ROWS          4
#define BRICK_COLUMNS       6
#define NUM_BRICKS          24
#define BRICK_WIDTH         18
#define BRICK_HEIGHT        6
#define BRICK_X_GAP         2
#define BRICK_Y_GAP         3
#define BRICK_START_X       5
#define BRICK_START_Y       12

#define ADC_LEFT            3
#define ADC_RIGHT           4035

// BoosterPack S2: J4.32 -> P4.2
#define S2                  BIT2

typedef struct {
    Graphics_Rectangle rect;
    unsigned char active;
} Brick;

Brick bricks[NUM_BRICKS];

unsigned int paddle_x = 52;
unsigned int Xoutput;

int lives = 3;
unsigned char activeBricks = NUM_BRICKS;

void initializeADC(void);
void readADC(void);
void initializeBricks(void);
void drawGame(void);
void drawLives(void);

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    // LCD power and clock
    P5DIR |= BIT4;
    P5OUT |= BIT4;

    CSCTL0 = CSKEY;
    CSCTL3 &= ~(BIT4 | BIT5 | BIT6);
    CSCTL0_H = 0;

    // Joystick P1.2 / A2
    P1SEL0 |= BIT2;
    P1SEL1 |= BIT2;

    // Enable GPIO
    PM5CTL0 &= ~LOCKLPM5;

    // ADC
    initializeADC();

    // LCD
    Crystalfontz128x128_Init();
    Initialize_Graphics(&g_sContext);
    Graphics_clearDisplay(&g_sContext);

    // BoosterPack S2 on P4.2
    P4SEL0 &= ~S2;
    P4SEL1 &= ~S2;
    P4DIR &= ~S2;
    P4REN |= S2;
    P4OUT |= S2;

    // Game setup
    initializeBricks();
    drawGame();

    // Timer_A at about 30 Hz using ACLK
    TA0CCR0 = 1092 - 1;
    TA0CCTL0 = CCIE;
    TA0CTL = TASSEL__ACLK | MC__UP | TACLR;

    __enable_interrupt();
    __low_power_mode_0();

    return 0;
}


// Game update
#pragma vector = TIMER0_A0_VECTOR
__interrupt void FIXED_UPDATE(void)
{
    static int velocity_x = 1;
    static int velocity_y = -1;
    static int pos_x = 64;
    static int pos_y = 104;
    static unsigned char isFree = 0;
    static unsigned char lastButton = 0;

    Graphics_Rectangle paddle;
    Graphics_Rectangle ball;

    unsigned char button;
    unsigned char i;

    int new_x;
    int new_y;

    // Read S2
    button = ((P4IN & S2) == 0);

    // Old paddle
    paddle.xMin = paddle_x;
    paddle.xMax = paddle_x + PADDLE_WIDTH - 1;
    paddle.yMin = PADDLE_Y;
    paddle.yMax = PADDLE_Y + PADDLE_HEIGHT - 1;

    // Erase old paddle
    Graphics_setForegroundColor(
        &g_sContext,
        GRAPHICS_COLOR_BLACK
    );

    Graphics_fillRectangle(
        &g_sContext,
        &paddle
    );

    // Read joystick
    readADC();

    if (Xoutput < ADC_LEFT)
        Xoutput = ADC_LEFT;

    if (Xoutput > ADC_RIGHT)
        Xoutput = ADC_RIGHT;

    // Map joystick to paddle
    paddle_x =
        ((uint32_t)(Xoutput - ADC_LEFT) * PADDLE_MAX_X) /
        (ADC_RIGHT - ADC_LEFT);

    // New paddle
    paddle.xMin = paddle_x;
    paddle.xMax = paddle_x + PADDLE_WIDTH - 1;

    // Erase old ball
    Graphics_setForegroundColor(
        &g_sContext,
        GRAPHICS_COLOR_BLACK
    );

    Graphics_fillCircle(
        &g_sContext,
        pos_x,
        pos_y,
        BALL_RADIUS
    );

    if (isFree)
    {
        // Calculate next ball position
        new_x = pos_x + velocity_x;
        new_y = pos_y + velocity_y;

        ball.xMin = new_x - BALL_RADIUS;
        ball.xMax = new_x + BALL_RADIUS;
        ball.yMin = new_y - BALL_RADIUS;
        ball.yMax = new_y + BALL_RADIUS;

        // Left wall
        if (ball.xMin <= 0)
        {
            velocity_x = 1;
            new_x = BALL_RADIUS;
        }

        // Right wall
        else if (ball.xMax >= SCREEN_WIDTH - 1)
        {
            velocity_x = -1;
            new_x = SCREEN_WIDTH - BALL_RADIUS - 1;
        }

        // Top wall
        if (ball.yMin <= 0)
        {
            velocity_y = 1;
            new_y = BALL_RADIUS;
        }

        // Update ball bounds
        ball.xMin = new_x - BALL_RADIUS;
        ball.xMax = new_x + BALL_RADIUS;
        ball.yMin = new_y - BALL_RADIUS;
        ball.yMax = new_y + BALL_RADIUS;

        // Paddle collision
        if ((velocity_y > 0) &&
            (ball.xMax >= paddle.xMin) &&
            (ball.xMin <= paddle.xMax) &&
            (ball.yMax >= paddle.yMin) &&
            (ball.yMin <= paddle.yMax))
        {
            velocity_y = -1;
            new_y = PADDLE_Y - BALL_RADIUS - 1;

            if (new_x < paddle_x + PADDLE_WIDTH / 3)
                velocity_x = -1;

            else if (new_x > paddle_x + (2 * PADDLE_WIDTH) / 3)
                velocity_x = 1;
        }

        // Update ball bounds
        ball.xMin = new_x - BALL_RADIUS;
        ball.xMax = new_x + BALL_RADIUS;
        ball.yMin = new_y - BALL_RADIUS;
        ball.yMax = new_y + BALL_RADIUS;

        // Brick collision
        for (i = 0; i < NUM_BRICKS; i++)
        {
            if (!bricks[i].active)
                continue;

            if ((ball.xMax >= bricks[i].rect.xMin) &&
                (ball.xMin <= bricks[i].rect.xMax) &&
                (ball.yMax >= bricks[i].rect.yMin) &&
                (ball.yMin <= bricks[i].rect.yMax))
            {
                if ((pos_y + BALL_RADIUS < bricks[i].rect.yMin) ||
                    (pos_y - BALL_RADIUS > bricks[i].rect.yMax))
                {
                    velocity_y = -velocity_y;
                }
                else
                {
                    velocity_x = -velocity_x;
                }

                // Erase brick
                Graphics_setForegroundColor(
                    &g_sContext,
                    GRAPHICS_COLOR_BLACK
                );

                Graphics_fillRectangle(
                    &g_sContext,
                    &bricks[i].rect
                );

                bricks[i].active = 0;

                if (activeBricks > 0)
                    activeBricks--;

                break;
            }
        }

        // Ball went below game area
        if (new_y - BALL_RADIUS > GAME_BOTTOM)
        {
            lives--;
            isFree = 0;

            velocity_x = 1;
            velocity_y = -1;

            pos_x = paddle_x + PADDLE_WIDTH / 2;
            pos_y = PADDLE_Y - BALL_RADIUS - 1;

            drawLives();

            // Game over
            if (lives <= 0)
            {
                lives = 3;
                activeBricks = NUM_BRICKS;

                initializeBricks();
                drawGame();
            }
        }
        else
        {
            pos_x = new_x;
            pos_y = new_y;
        }

        // All bricks destroyed
        if (activeBricks == 0)
        {
            isFree = 0;
            lives = 3;
            activeBricks = NUM_BRICKS;

            velocity_x = 1;
            velocity_y = -1;

            initializeBricks();
            drawGame();

            pos_x = paddle_x + PADDLE_WIDTH / 2;
            pos_y = PADDLE_Y - BALL_RADIUS - 1;
        }
    }
    else
    {
        // Ball follows paddle
        pos_x = paddle_x + PADDLE_WIDTH / 2;
        pos_y = PADDLE_Y - BALL_RADIUS - 1;

        // Launch ball when S2 is newly pressed
        if (button && !lastButton)
        {
            isFree = 1;

            velocity_x = 1;
            velocity_y = -1;

            pos_x += velocity_x;
            pos_y += velocity_y;
        }
    }

    // Save S2 state
    lastButton = button;

    // Draw paddle and ball
    Graphics_setForegroundColor(
        &g_sContext,
        GRAPHICS_COLOR_WHITE
    );

    Graphics_fillRectangle(
        &g_sContext,
        &paddle
    );

    Graphics_fillCircle(
        &g_sContext,
        pos_x,
        pos_y,
        BALL_RADIUS
    );
}


// ADC initialization
void initializeADC(void)
{
    ADC12CTL0 &= ~ADC12ENC;

    ADC12CTL0 = ADC12SHT0_2 | ADC12ON;
    ADC12CTL1 = ADC12SHP | ADC12SSEL_3;
    ADC12CTL2 = ADC12RES_2;

    ADC12MCTL0 = ADC12VRSEL_0 | ADC12INCH_2;

    ADC12CTL0 |= ADC12ENC;
}


// Read joystick
void readADC(void)
{
    ADC12CTL0 |= ADC12SC;

    while (ADC12CTL1 & ADC12BUSY)
    {
    }

    Xoutput = ADC12MEM0;
}


// Initialize bricks
void initializeBricks(void)
{
    unsigned int row;
    unsigned int column;
    unsigned int i = 0;

    for (row = 0; row < BRICK_ROWS; row++)
    {
        for (column = 0; column < BRICK_COLUMNS; column++)
        {
            bricks[i].rect.xMin =
                BRICK_START_X +
                column * (BRICK_WIDTH + BRICK_X_GAP);

            bricks[i].rect.xMax =
                bricks[i].rect.xMin +
                BRICK_WIDTH - 1;

            bricks[i].rect.yMin =
                BRICK_START_Y +
                row * (BRICK_HEIGHT + BRICK_Y_GAP);

            bricks[i].rect.yMax =
                bricks[i].rect.yMin +
                BRICK_HEIGHT - 1;

            bricks[i].active = 1;

            i++;
        }
    }
}


// Draw game
void drawGame(void)
{
    Graphics_Rectangle paddle;
    unsigned int i;

    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(
        &g_sContext,
        GRAPHICS_COLOR_WHITE
    );

    // Bricks
    for (i = 0; i < NUM_BRICKS; i++)
    {
        if (bricks[i].active)
        {
            Graphics_fillRectangle(
                &g_sContext,
                &bricks[i].rect
            );
        }
    }

    // Paddle
    paddle.xMin = paddle_x;
    paddle.xMax = paddle_x + PADDLE_WIDTH - 1;
    paddle.yMin = PADDLE_Y;
    paddle.yMax = PADDLE_Y + PADDLE_HEIGHT - 1;

    Graphics_fillRectangle(
        &g_sContext,
        &paddle
    );

    // Status separator
    Graphics_drawLine(
        &g_sContext,
        0,
        STATUS_LINE_Y,
        127,
        STATUS_LINE_Y
    );

    // Lives
    Graphics_drawString(
        &g_sContext,
        (int8_t *)"Lives:",
        AUTO_STRING_LENGTH,
        4,
        119,
        OPAQUE_TEXT
    );

    drawLives();
}


// Draw remaining lives
void drawLives(void)
{
    Graphics_Rectangle area = {
        70, 118, 105, 127
    };

    // Clear old lives
    Graphics_setForegroundColor(
        &g_sContext,
        GRAPHICS_COLOR_BLACK
    );

    Graphics_fillRectangle(
        &g_sContext,
        &area
    );

    // Draw lives
    Graphics_setForegroundColor(
        &g_sContext,
        GRAPHICS_COLOR_WHITE
    );

    if (lives >= 1)
        Graphics_fillCircle(&g_sContext, 77, 123, 2);

    if (lives >= 2)
        Graphics_fillCircle(&g_sContext, 87, 123, 2);

    if (lives >= 3)
        Graphics_fillCircle(&g_sContext, 97, 123, 2);
}
