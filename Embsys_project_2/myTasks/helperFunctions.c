/*
 * helperFunctions.c
 *
 *  Created on: 16 Nov 2019
 *      Author: zedd
 */

#include "board.h"
#include "pin_mux.h"
#include "helperFunctions.h"

struct userDate_t date;
struct userTime_t time;

void allLedsOFF()
{
	GPIO_PinWrite(BOARD_LED1_GPIO, BOARD_LED1_PIN, 0 );
	GPIO_PinWrite(BOARD_LED2_GPIO, BOARD_LED2_PIN, 0 );
	GPIO_PinWrite(BOARD_LED3_GPIO, BOARD_LED3_PIN, 0 );
	GPIO_PinWrite(BOARD_LED4_GPIO, BOARD_LED4_PIN, 0 );
	GPIO_PinWrite(BOARD_LED5_GPIO, BOARD_LED5_PIN, 0 );
	GPIO_PinWrite(BOARD_LED6_GPIO, BOARD_LED6_PIN, 0 );
}

struct userDate_t getDate(char* stringDate)
{
	struct userDate_t date = {};

	date.year = ((stringDate[0] - 48) * 1000) +
				((stringDate[1] - 48) * 100) +
				((stringDate[2] - 48) * 10) +
				((stringDate[3] - 48));
	date.month = ((stringDate[5] - 48) * 10) + ((stringDate[6] - 48));
	date.day = ((stringDate[8] - 48) * 10) + ((stringDate[9] - 48));

	return date;
}

struct userTime_t getTime(char* stringTime)
{
	struct userTime_t time;

	time.hour = ((stringTime[0] - 48) * 10) + ((stringTime[1] - 48));
	time.minute = ((stringTime[3] - 48) * 10) + ((stringTime[4] - 48));

	return time;
}

void busyWait(int ticks)
{
	for(int i = 0; i < ticks; i++);
}

