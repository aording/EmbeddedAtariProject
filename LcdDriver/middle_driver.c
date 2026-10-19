// This code was ported from TI's sample code. See Copyright notice at the bottom of this file.

#include "msp430fr5994.h"
#include "LcdDriver/middle_driver.h"
#include "Grlib/grlib/grlib.h"
#include <stdint.h>

void HAL_LCD_PortInit(void)
{
    /////////////////////////////////////
    // Configuring the SPI pins
    /////////////////////////////////////
    // P5.2/UCB1CLK  ---------> SPI_CLK
    // P5.0/UCB1SIMO ---------> SPI_SI
    P5SEL1&=~(BIT0|BIT2);
    P5SEL0|=BIT0|BIT2;
    ///////////////////////////////////////////////
    // Configuring the display's other pins
    ///////////////////////////////////////////////
    // P5.3          ---------> RST
    P5DIR|=BIT3;
    // P4.1          ---------> RS
    P4DIR |= BIT1;
    // P8.3          ---------> SPI_CS
    P8DIR|=BIT3;

    return;
}

void HAL_LCD_SpiInit(void)
{
    //////////////////////////
    // SPI configuration
    //////////////////////////

    // Put eUSCI in reset state while modifying the configuration
    UCB1CTLW0 |= UCSWRST;

    // Set clock phase to "capture on 1st edge, change on following edge"
    UCB1CTLW0 |=  BITF;
    // Set clock polarity to "inactive low"
    UCB1CTLW0 &= ~BITE;
    // Set data order to "transmit MSB first"
    UCB1CTLW0 |=  BITD;
    // Set MCU to "SPI master"
    UCB1CTLW0 |=  BITB;
    // Set SPI to "3 pin SPI" (we won't use eUSCI's chip select)
    UCB1CTLW0 &= ~(BIT9 | BITA);
    // Set module to synchronous mode
    UCB1CTLW0 |=  BIT8;
    // Set clock to SMCLK
    UCB1CTLW0 |=  BIT7;

    // Set clock divider to 1 (SMCLK is from DCO at 8 MHz; we'll run SPI at 8 MHz)
    UCB1BRW = 0;

    // Exit the reset state at the end of the configuration
    UCB1CTLW0 &= ~UCSWRST;

    // Set CS' (chip select) bit to 0 (display always enabled)
    P8OUT &= ~BIT3;
    // Set DC' bit to 0 (assume data)
    P4OUT &= ~BIT1;

    return;
}


//*****************************************************************************
// Writes a command to the CFAF128128B-0145T.  This function implements the basic SPI
// interface to the LCD display.
//*****************************************************************************
void HAL_LCD_writeCommand(uint8_t command)
{
    // For command, set the DC' bit to low before transmission
    P4OUT &= ~BIT1;

    // Wait as long as the module is busy
    while (UCB1STATW & UCBUSY);

    // Transmit data
    UCB1TXBUF = command;

    // Set DC' bit back to high
    P4OUT |= BIT1;
}


//*****************************************************************************
// Writes a data to the CFAF128128B-0145T.  This function implements the basic SPI
// interface to the LCD display.
//*****************************************************************************
void HAL_LCD_writeData(uint8_t data)
{
    // Wait as long as the module is busy
    while (UCB1STATW & UCBUSY);

    // Transmit data
    UCB1TXBUF = data;
}






/* --COPYRIGHT--,BSD
 * Copyright (c) 2015, Texas Instruments Incorporated
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * *  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * *  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * *  Neither the name of Texas Instruments Incorporated nor the names of
 *    its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 * --/COPYRIGHT--*/
