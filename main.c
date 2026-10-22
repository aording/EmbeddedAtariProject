#include <msp430fr5994.h>
#include <stdint.h>
#include "Grlib/grlib/grlib.h"
#include "LcdDriver/lcd_driver.h"
#include "graphics.h"

Graphics_Context g_sContext;

// Screen
#define SCREEN_WIDTH        128
#define SCREEN_HEIGHT       128
#define STATUS_LINE_Y       116
#define GAME_BOTTOM         115

// Paddle
#define PADDLE_WIDTH        24
#define PADDLE_HEIGHT       4
#define PADDLE_Y            108
#define PADDLE_MAX_X        (SCREEN_WIDTH - PADDLE_WIDTH)

// Ball
#define BALL_RADIUS         2

// Bricks
#define BRICK_ROWS          4
#define BRICK_COLUMNS       6
#define NUM_BRICKS          (BRICK_ROWS * BRICK_COLUMNS)
#define BRICK_WIDTH         18
#define BRICK_HEIGHT        6
#define BRICK_X_GAP         2
#define BRICK_Y_GAP         3
#define BRICK_START_X       5
#define BRICK_START_Y       12

// Levels
#define MAX_LEVEL           3

// Colors
#define BRICK_COLOR_1       GRAPHICS_COLOR_RED
#define BRICK_COLOR_2       GRAPHICS_COLOR_YELLOW
#define BRICK_COLOR_3       GRAPHICS_COLOR_GREEN
#define BRICK_COLOR_4       GRAPHICS_COLOR_PURPLE
#define PADDLE_COLOR        GRAPHICS_COLOR_WHITE
#define BALL_COLOR          GRAPHICS_COLOR_WHITE
#define STATUS_COLOR        GRAPHICS_COLOR_WHITE
#define BACKGROUND_COLOR    GRAPHICS_COLOR_BLACK

// Joystick
#define ADC_LEFT            3
#define ADC_RIGHT           4035

// BoosterPack S2 on P4.2
#define S2                  BIT2

// BoosterPack BUZ1 on P3.7
#define BUZZER              BIT7

// Game states
#define GAME_START          0
#define GAME_READY          1
#define GAME_RUNNING        2
#define GAME_OVER           3
#define GAME_WON            4

// Sounds
#define SOUND_NONE          0
#define SOUND_WALL          1
#define SOUND_PADDLE        2
#define SOUND_BRICK         3
#define SOUND_LEVEL         4
#define SOUND_GAME_OVER     5
#define SOUND_WIN           6

typedef struct
{
    Graphics_Rectangle rect;
    unsigned char active;
} Brick;

Brick bricks[NUM_BRICKS];

unsigned int paddle_x = 52;
unsigned int old_paddle_x = 52;

unsigned int Xoutput = 0;
unsigned int filteredX = 2048;

int ball_x = 64;
int ball_y = 104;
int ball_velocity_x = 1;
int ball_velocity_y = -1;

int lives = 3;

unsigned char currentLevel = 1;
unsigned char activeBricks = 0;
unsigned char gameState = GAME_START;
unsigned char lastButton = 0;
unsigned char speedPhase = 0;

volatile unsigned char soundType = SOUND_NONE;
volatile unsigned char soundNote = 0;
volatile unsigned char soundTicks = 0;

void initializeADC(void);
void readADC(void);

void initializeBuzzer(void);
void startTone(unsigned int period);
void stopTone(void);
void playSound(unsigned char sound);
void updateSound(void);

void initializeLevel(unsigned char level);
void startNewGame(void);
void advanceLevel(void);
void resetBall(void);
void loseLife(void);

unsigned char updateBallOneStep(void);
unsigned char getBallSteps(void);
void updatePaddlePosition(void);

void eraseOldPaddleArea(unsigned int oldX, unsigned int newX);
void drawPaddle(void);

void eraseOldBallArea(int oldX, int oldY, int newX, int newY);
void drawBall(int x, int y);

void drawStartScreen(void);
void drawGame(void);
void drawLives(void);
void drawLevel(void);
void drawEndMessage(const int8_t *message);

uint32_t getBrickColor(unsigned int brickIndex);


int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;

    // LCD power
    P5DIR |= BIT4;
    P5OUT |= BIT4;

    // Clock
    CSCTL0 = CSKEY;
    CSCTL3 &= ~(BIT4 | BIT5 | BIT6);
    CSCTL0_H = 0;

    // Joystick P1.2 / A2
    P1SEL0 |= BIT2;
    P1SEL1 |= BIT2;

    // Enable GPIO
    PM5CTL0 &= ~LOCKLPM5;

    initializeADC();

    // LCD
    Crystalfontz128x128_Init();
    Initialize_Graphics(&g_sContext);
    Graphics_setBackgroundColor(&g_sContext, BACKGROUND_COLOR);
    Graphics_clearDisplay(&g_sContext);

    // S2 on P4.2
    P4SEL0 &= ~S2;
    P4SEL1 &= ~S2;
    P4DIR &= ~S2;
    P4REN |= S2;
    P4OUT |= S2;

    // BUZ1 on P3.7
    initializeBuzzer();

    gameState = GAME_START;
    drawStartScreen();

    // Game update at about 30 Hz
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
    unsigned char button;
    unsigned char steps;
    unsigned char step;
    unsigned char screenChanged = 0;
    int old_ball_x;
    int old_ball_y;

    updateSound();

    button = ((P4IN & S2) == 0);

    if (gameState == GAME_START)
    {
        if (button && !lastButton)
        {
            startNewGame();
        }
    }
    else if ((gameState == GAME_READY) || (gameState == GAME_RUNNING))
    {
        updatePaddlePosition();

        old_ball_x = ball_x;
        old_ball_y = ball_y;

        if (gameState == GAME_RUNNING)
        {
            steps = getBallSteps();

            for (step = 0; step < steps; step++)
            {
                if (gameState != GAME_RUNNING)
                {
                    break;
                }

                if (updateBallOneStep())
                {
                    screenChanged = 1;
                    break;
                }
            }
        }
        else
        {
            ball_x = paddle_x + PADDLE_WIDTH / 2;
            ball_y = PADDLE_Y - BALL_RADIUS - 1;

            if (button && !lastButton)
            {
                gameState = GAME_RUNNING;
                ball_velocity_x = 1;
                ball_velocity_y = -1;
                ball_x += ball_velocity_x;
                ball_y += ball_velocity_y;
            }
        }

        if (!screenChanged)
        {
            if ((gameState == GAME_READY) || (gameState == GAME_RUNNING))
            {
                eraseOldBallArea(old_ball_x, old_ball_y, ball_x, ball_y);
            }

            if (old_ball_y + BALL_RADIUS >= STATUS_LINE_Y)
            {
                Graphics_setForegroundColor(&g_sContext, STATUS_COLOR);
                Graphics_drawLine(&g_sContext, 0, STATUS_LINE_Y, SCREEN_WIDTH - 1, STATUS_LINE_Y);
            }

            drawPaddle();

            if ((gameState == GAME_READY) || (gameState == GAME_RUNNING))
            {
                drawBall(ball_x, ball_y);
            }
        }
    }
    else if ((gameState == GAME_OVER) || (gameState == GAME_WON))
    {
        if (button && !lastButton)
        {
            startNewGame();
        }
    }

    lastButton = button;
}

// Update paddle
void updatePaddlePosition(void)
{
    uint32_t mappedPosition;

    old_paddle_x = paddle_x;

    readADC();

    filteredX = (unsigned int)(((uint32_t)3 * filteredX + Xoutput) / 4);

    if (filteredX < ADC_LEFT)
    {
        filteredX = ADC_LEFT;
    }

    if (filteredX > ADC_RIGHT)
    {
        filteredX = ADC_RIGHT;
    }

    mappedPosition = ((uint32_t)(filteredX - ADC_LEFT) * PADDLE_MAX_X) / (ADC_RIGHT - ADC_LEFT);

    paddle_x = (unsigned int)mappedPosition;

    if (paddle_x != old_paddle_x)
    {
        eraseOldPaddleArea(old_paddle_x, paddle_x);
    }
}


// Erase old paddle area
void eraseOldPaddleArea(unsigned int oldX, unsigned int newX)
{
    Graphics_Rectangle eraseArea;
    unsigned int oldRight;
    unsigned int newRight;

    oldRight = oldX + PADDLE_WIDTH - 1;
    newRight = newX + PADDLE_WIDTH - 1;

    eraseArea.yMin = PADDLE_Y;
    eraseArea.yMax = PADDLE_Y + PADDLE_HEIGHT - 1;

    Graphics_setForegroundColor(&g_sContext, BACKGROUND_COLOR);

    if (newX > oldX)
    {
        eraseArea.xMin = oldX;

        if (newX <= oldRight)
        {
            eraseArea.xMax = newX - 1;
        }
        else
        {
            eraseArea.xMax = oldRight;
        }

        Graphics_fillRectangle(&g_sContext, &eraseArea);
    }
    else if (newX < oldX)
    {
        if (newRight >= oldX)
        {
            eraseArea.xMin = newRight + 1;
        }
        else
        {
            eraseArea.xMin = oldX;
        }

        eraseArea.xMax = oldRight;

        Graphics_fillRectangle(&g_sContext, &eraseArea);
    }
}


// Draw paddle
void drawPaddle(void)
{
    Graphics_Rectangle paddle;

    paddle.xMin = paddle_x;
    paddle.xMax = paddle_x + PADDLE_WIDTH - 1;
    paddle.yMin = PADDLE_Y;
    paddle.yMax = PADDLE_Y + PADDLE_HEIGHT - 1;

    Graphics_setForegroundColor(&g_sContext, PADDLE_COLOR);
    Graphics_fillRectangle(&g_sContext, &paddle);
}


// Erase old ball
void eraseOldBallArea(int oldX, int oldY, int newX, int newY)
{
    Graphics_setForegroundColor(&g_sContext, BACKGROUND_COLOR);
    Graphics_fillCircle(&g_sContext, oldX, oldY, BALL_RADIUS);

    Graphics_setForegroundColor(&g_sContext, BALL_COLOR);
    Graphics_fillCircle(&g_sContext, newX, newY, BALL_RADIUS);
}


// Draw ball
void drawBall(int x, int y)
{
    Graphics_setForegroundColor(&g_sContext, BALL_COLOR);
    Graphics_fillCircle(&g_sContext, x, y, BALL_RADIUS);
}


// Move ball and check collisions
unsigned char updateBallOneStep(void)
{
    Graphics_Rectangle paddle;
    Graphics_Rectangle ball;
    unsigned int i;
    int new_x;
    int new_y;

    paddle.xMin = paddle_x;
    paddle.xMax = paddle_x + PADDLE_WIDTH - 1;
    paddle.yMin = PADDLE_Y;
    paddle.yMax = PADDLE_Y + PADDLE_HEIGHT - 1;

    new_x = ball_x + ball_velocity_x;
    new_y = ball_y + ball_velocity_y;

    ball.xMin = new_x - BALL_RADIUS;
    ball.xMax = new_x + BALL_RADIUS;
    ball.yMin = new_y - BALL_RADIUS;
    ball.yMax = new_y + BALL_RADIUS;

    // Left wall
    if (ball.xMin <= 0)
    {
        ball_velocity_x = 1;
        new_x = BALL_RADIUS;
        playSound(SOUND_WALL);
    }

    // Right wall
    else if (ball.xMax >= SCREEN_WIDTH - 1)
    {
        ball_velocity_x = -1;
        new_x = SCREEN_WIDTH - BALL_RADIUS - 1;
        playSound(SOUND_WALL);
    }

    // Top wall
    if (ball.yMin <= 0)
    {
        ball_velocity_y = 1;
        new_y = BALL_RADIUS;
        playSound(SOUND_WALL);
    }

    ball.xMin = new_x - BALL_RADIUS;
    ball.xMax = new_x + BALL_RADIUS;
    ball.yMin = new_y - BALL_RADIUS;
    ball.yMax = new_y + BALL_RADIUS;

    // Paddle collision
    if ((ball_velocity_y > 0) &&
        (ball.xMax >= paddle.xMin) &&
        (ball.xMin <= paddle.xMax) &&
        (ball.yMax >= paddle.yMin) &&
        (ball.yMin <= paddle.yMax))
    {
        ball_velocity_y = -1;
        new_y = PADDLE_Y - BALL_RADIUS - 1;

        if (new_x < paddle_x + PADDLE_WIDTH / 3)
        {
            ball_velocity_x = -1;
        }
        else if (new_x > paddle_x + (2 * PADDLE_WIDTH) / 3)
        {
            ball_velocity_x = 1;
        }

        playSound(SOUND_PADDLE);
    }

    ball.xMin = new_x - BALL_RADIUS;
    ball.xMax = new_x + BALL_RADIUS;
    ball.yMin = new_y - BALL_RADIUS;
    ball.yMax = new_y + BALL_RADIUS;

    // Brick collision
    for (i = 0; i < NUM_BRICKS; i++)
    {
        if (!bricks[i].active)
        {
            continue;
        }

        if ((ball.xMax >= bricks[i].rect.xMin) &&
            (ball.xMin <= bricks[i].rect.xMax) &&
            (ball.yMax >= bricks[i].rect.yMin) &&
            (ball.yMin <= bricks[i].rect.yMax))
        {
            if ((ball_y + BALL_RADIUS < bricks[i].rect.yMin) ||
                (ball_y - BALL_RADIUS > bricks[i].rect.yMax))
            {
                ball_velocity_y = -ball_velocity_y;
            }
            else
            {
                ball_velocity_x = -ball_velocity_x;
            }

            Graphics_setForegroundColor(&g_sContext, BACKGROUND_COLOR);
            Graphics_fillRectangle(&g_sContext, &bricks[i].rect);

            bricks[i].active = 0;

            if (activeBricks > 0)
            {
                activeBricks--;
            }

            playSound(SOUND_BRICK);
            break;
        }
    }

    // Ball fell below game area
    if (new_y - BALL_RADIUS > GAME_BOTTOM)
    {
        loseLife();
        return 1;
    }

    ball_x = new_x;
    ball_y = new_y;

    // Level complete
    if (activeBricks == 0)
    {
        advanceLevel();
        return 1;
    }

    return 0;
}


// Ball speed
unsigned char getBallSteps(void)
{
    if (currentLevel == 1)
    {
        return 1;
    }

    if (currentLevel == 2)
    {
        speedPhase ^= 1;

        if (speedPhase)
        {
            return 2;
        }

        return 1;
    }

    return 2;
}


// ADC setup
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


// Buzzer setup
void initializeBuzzer(void)
{
    P3SEL0 &= ~BUZZER;
    P3SEL1 &= ~BUZZER;

    P3DIR |= BUZZER;
    P3OUT &= ~BUZZER;

    TB0CCTL0 = 0;
    TB0CTL = TBSSEL__ACLK | MC__STOP | TBCLR;
}


// Start tone
void startTone(unsigned int period)
{
    P3OUT &= ~BUZZER;

    TB0CTL = TBSSEL__ACLK | MC__STOP | TBCLR;
    TB0CCR0 = period;
    TB0CCTL0 = CCIE;
    TB0CTL = TBSSEL__ACLK | MC__UP | TBCLR;
}


// Stop tone
void stopTone(void)
{
    TB0CTL = TBSSEL__ACLK | MC__STOP | TBCLR;
    TB0CCTL0 &= ~CCIE;

    P3OUT &= ~BUZZER;
}


// Request sound
void playSound(unsigned char sound)
{
    if ((soundType == SOUND_GAME_OVER) || (soundType == SOUND_WIN))
    {
        return;
    }

    stopTone();

    soundType = sound;
    soundNote = 0;
    soundTicks = 0;
}


// Update sound
void updateSound(void)
{
    if (soundType == SOUND_NONE)
    {
        return;
    }

    if (soundTicks > 0)
    {
        soundTicks--;

        if (soundTicks > 0)
        {
            return;
        }

        stopTone();
        soundNote++;
    }

    if (soundType == SOUND_WALL)
    {
        if (soundNote == 0)
        {
            startTone(13);
            soundTicks = 2;
        }
        else
        {
            stopTone();
            soundType = SOUND_NONE;
        }
    }
    else if (soundType == SOUND_PADDLE)
    {
        if (soundNote == 0)
        {
            startTone(17);
            soundTicks = 3;
        }
        else
        {
            stopTone();
            soundType = SOUND_NONE;
        }
    }
    else if (soundType == SOUND_BRICK)
    {
        if (soundNote == 0)
        {
            startTone(9);
            soundTicks = 2;
        }
        else
        {
            stopTone();
            soundType = SOUND_NONE;
        }
    }
    else if (soundType == SOUND_LEVEL)
    {
        if (soundNote == 0)
        {
            startTone(23);
            soundTicks = 3;
        }
        else if (soundNote == 1)
        {
            startTone(15);
            soundTicks = 3;
        }
        else if (soundNote == 2)
        {
            startTone(11);
            soundTicks = 5;
        }
        else
        {
            stopTone();
            soundType = SOUND_NONE;
        }
    }
    else if (soundType == SOUND_GAME_OVER)
    {
        if (soundNote == 0)
        {
            startTone(23);
            soundTicks = 4;
        }
        else if (soundNote == 1)
        {
            startTone(32);
            soundTicks = 4;
        }
        else if (soundNote == 2)
        {
            startTone(54);
            soundTicks = 8;
        }
        else
        {
            stopTone();
            soundType = SOUND_NONE;
        }
    }
    else if (soundType == SOUND_WIN)
    {
        if (soundNote == 0)
        {
            startTone(17);
            soundTicks = 3;
        }
        else if (soundNote == 1)
        {
            startTone(13);
            soundTicks = 3;
        }
        else if (soundNote == 2)
        {
            startTone(9);
            soundTicks = 3;
        }
        else if (soundNote == 3)
        {
            startTone(7);
            soundTicks = 7;
        }
        else
        {
            stopTone();
            soundType = SOUND_NONE;
        }
    }
}


// Buzzer waveform
#pragma vector = TIMER0_B0_VECTOR
__interrupt void BUZZER_UPDATE(void)
{
    P3OUT ^= BUZZER;
}


// Initialize level
void initializeLevel(unsigned char level)
{
    unsigned int row;
    unsigned int column;
    unsigned int i = 0;

    activeBricks = 0;

    for (row = 0; row < BRICK_ROWS; row++)
    {
        for (column = 0; column < BRICK_COLUMNS; column++)
        {
            bricks[i].rect.xMin = BRICK_START_X + column * (BRICK_WIDTH + BRICK_X_GAP);
            bricks[i].rect.xMax = bricks[i].rect.xMin + BRICK_WIDTH - 1;
            bricks[i].rect.yMin = BRICK_START_Y + row * (BRICK_HEIGHT + BRICK_Y_GAP);
            bricks[i].rect.yMax = bricks[i].rect.yMin + BRICK_HEIGHT - 1;

            if (level == 1)
            {
                bricks[i].active = 1;
            }
            else if (level == 2)
            {
                bricks[i].active = (((row + column) % 2) == 0);
            }
            else
            {
                if ((row == 0) ||
                    (row == BRICK_ROWS - 1) ||
                    (column == 0) ||
                    (column == BRICK_COLUMNS - 1) ||
                    (((row + column) % 2) == 0))
                {
                    bricks[i].active = 1;
                }
                else
                {
                    bricks[i].active = 0;
                }
            }

            if (bricks[i].active)
            {
                activeBricks++;
            }

            i++;
        }
    }
}


// Start new game
void startNewGame(void)
{
    stopTone();

    soundType = SOUND_NONE;
    soundNote = 0;
    soundTicks = 0;

    currentLevel = 1;
    lives = 3;
    speedPhase = 0;
    filteredX = 2048;

    initializeLevel(currentLevel);
    resetBall();
    drawGame();
}


// Advance level
void advanceLevel(void)
{
    if (currentLevel < MAX_LEVEL)
    {
        playSound(SOUND_LEVEL);

        currentLevel++;
        speedPhase = 0;

        initializeLevel(currentLevel);
        resetBall();
        drawGame();
    }
    else
    {
        gameState = GAME_WON;

        drawEndMessage((const int8_t *)"YOU WIN");
        playSound(SOUND_WIN);
    }
}

// Reset ball
void resetBall(void)
{
    gameState = GAME_READY;

    ball_velocity_x = 1;
    ball_velocity_y = -1;

    ball_x = paddle_x + PADDLE_WIDTH / 2;
    ball_y = PADDLE_Y - BALL_RADIUS - 1;
}


// Lose life
void loseLife(void)
{
    lives--;

    if (lives <= 0)
    {
        lives = 0;
        gameState = GAME_OVER;

        drawEndMessage((const int8_t *)"GAME OVER");
        playSound(SOUND_GAME_OVER);
    }
    else
    {
        resetBall();
        drawLives();
    }
}

// Start screen
void drawStartScreen(void)
{
    Graphics_setBackgroundColor(&g_sContext, BACKGROUND_COLOR);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_RED);
    Graphics_drawStringCentered(&g_sContext, (int8_t *)"BREAKOUT", AUTO_STRING_LENGTH, SCREEN_WIDTH / 2, 38, OPAQUE_TEXT);

    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_WHITE);
    Graphics_drawStringCentered(&g_sContext, (int8_t *)"MSP430", AUTO_STRING_LENGTH, SCREEN_WIDTH / 2, 58, OPAQUE_TEXT);

    Graphics_setForegroundColor(&g_sContext, GRAPHICS_COLOR_YELLOW);
    Graphics_drawStringCentered(&g_sContext, (int8_t *)"S2 TO START", AUTO_STRING_LENGTH, SCREEN_WIDTH / 2, 86, OPAQUE_TEXT);
}


// Draw game
void drawGame(void)
{
    unsigned int i;

    Graphics_setBackgroundColor(&g_sContext, BACKGROUND_COLOR);
    Graphics_clearDisplay(&g_sContext);

    for (i = 0; i < NUM_BRICKS; i++)
    {
        if (bricks[i].active)
        {
            Graphics_setForegroundColor(&g_sContext, getBrickColor(i));
            Graphics_fillRectangle(&g_sContext, &bricks[i].rect);
        }
    }

    drawPaddle();
    drawBall(ball_x, ball_y);

    Graphics_setForegroundColor(&g_sContext, STATUS_COLOR);
    Graphics_drawLine(&g_sContext, 0, STATUS_LINE_Y, SCREEN_WIDTH - 1, STATUS_LINE_Y);
    Graphics_drawString(&g_sContext, (int8_t *)"Lives:", AUTO_STRING_LENGTH, 2, 119, OPAQUE_TEXT);
    Graphics_drawString(&g_sContext, (int8_t *)"L:", AUTO_STRING_LENGTH, 106, 119, OPAQUE_TEXT);

    drawLives();
    drawLevel();
}


// Draw lives
void drawLives(void)
{
    Graphics_Rectangle area = {42, 118, 82, 127};

    Graphics_setForegroundColor(&g_sContext, BACKGROUND_COLOR);
    Graphics_fillRectangle(&g_sContext, &area);

    Graphics_setForegroundColor(&g_sContext, STATUS_COLOR);

    if (lives >= 1)
    {
        Graphics_fillCircle(&g_sContext, 50, 123, 2);
    }

    if (lives >= 2)
    {
        Graphics_fillCircle(&g_sContext, 61, 123, 2);
    }

    if (lives >= 3)
    {
        Graphics_fillCircle(&g_sContext, 72, 123, 2);
    }
}


// Draw level
void drawLevel(void)
{
    Graphics_Rectangle area = {118, 118, 127, 127};
    int8_t levelText[2];

    levelText[0] = '0' + currentLevel;
    levelText[1] = '\0';

    Graphics_setForegroundColor(&g_sContext, BACKGROUND_COLOR);
    Graphics_fillRectangle(&g_sContext, &area);

    Graphics_setForegroundColor(&g_sContext, STATUS_COLOR);
    Graphics_drawString(&g_sContext, levelText, AUTO_STRING_LENGTH, 119, 119, OPAQUE_TEXT);
}


// End screen
void drawEndMessage(const int8_t *message)
{
    Graphics_setBackgroundColor(&g_sContext, BACKGROUND_COLOR);
    Graphics_clearDisplay(&g_sContext);

    Graphics_setForegroundColor(&g_sContext, STATUS_COLOR);
    Graphics_drawStringCentered(&g_sContext, (int8_t *)message, AUTO_STRING_LENGTH, SCREEN_WIDTH / 2, 52, OPAQUE_TEXT);
    Graphics_drawStringCentered(&g_sContext, (int8_t *)"Press S2", AUTO_STRING_LENGTH, SCREEN_WIDTH / 2, 70, OPAQUE_TEXT);
}


// Brick color
uint32_t getBrickColor(unsigned int brickIndex)
{
    unsigned int row;

    row = brickIndex / BRICK_COLUMNS;

    if (row == 0)
    {
        return BRICK_COLOR_1;
    }
    else if (row == 1)
    {
        return BRICK_COLOR_2;
    }
    else if (row == 2)
    {
        return BRICK_COLOR_3;
    }
    else
    {
        return BRICK_COLOR_4;
    }
}
