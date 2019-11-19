/*
 * helperFunctions.c
 *
 *  Created on: 16 Nov 2019
 *      Author: zedd
 */

#include "board.h"
#include "pin_mux.h"

void allLedsOFF()
{
	GPIO_PinWrite(BOARD_LED1_GPIO, BOARD_LED1_PIN, 0 );
	GPIO_PinWrite(BOARD_LED2_GPIO, BOARD_LED2_PIN, 0 );
	GPIO_PinWrite(BOARD_LED3_GPIO, BOARD_LED3_PIN, 0 );
	GPIO_PinWrite(BOARD_LED4_GPIO, BOARD_LED4_PIN, 0 );
	GPIO_PinWrite(BOARD_LED5_GPIO, BOARD_LED5_PIN, 0 );
	GPIO_PinWrite(BOARD_LED6_GPIO, BOARD_LED6_PIN, 0 );
}

