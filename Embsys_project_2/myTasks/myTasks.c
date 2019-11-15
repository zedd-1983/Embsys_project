/*
 * myTasks.c
 *
 *  Created on: 14 Nov 2019
 *      Author: zedd
 */
#include "board.h"
#include "FreeRTOS.h"
#include "semphr.h"

#include "fsl_debug_console.h"
#include "pin_mux.h"

#include "myDefines.h"

SemaphoreHandle_t sw2Semaphore = NULL, sw3Semaphore = NULL, encoderSemaphore = NULL;

void ledTask(void* pvParameters)
{
	for(;;)
	{
		if(xSemaphoreTake(sw2Semaphore, 0) == pdTRUE)
		{
			GPIO_PortToggle(BOARD_LED_RED_GPIO, 1 << BOARD_LED_RED_GPIO_PIN);
			PRINTF("\n\rRED led toggled\n\r");
		}
		else if(xSemaphoreTake(sw3Semaphore, 0) == pdTRUE)
		{
			GPIO_PortToggle(BOARD_LED_GREEN_GPIO, 1 << BOARD_LED_GREEN_GPIO_PIN);
			PRINTF("\n\rGREEN led toggled\n\r");
		}
	}
}

void encoderRead(void* pvParameters)
{
	uint16_t count = ENCODER_MIN;
	uint16_t previousState;
	uint16_t currentState;
	uint8_t diff = 5;

	// initial read of the encoder
	previousState = GPIO_PinRead(BOARD_ENC_CLK_GPIO, BOARD_ENC_CLK_PIN);

	for(;;)
	{
		// read encoder as it moves
		currentState = GPIO_PinRead(BOARD_ENC_CLK_GPIO, BOARD_ENC_CLK_PIN);

		// encoder has moved
		if(currentState != previousState) {

			if(GPIO_PinRead(BOARD_ENC_DT_GPIO, BOARD_ENC_DT_PIN) != currentState) {
				count -= diff;
				if(count < ENCODER_MIN) {
					count = ENCODER_MIN;
					PRINTF(RED_TEXT"\rLower limit reached\n\r"RESET_TEXT);
				}
			}
			else {
				count += diff;
				if(count > ENCODER_MAX) {
					count = ENCODER_MAX;
					PRINTF(RED_TEXT"\rUpper limit reached\n\r"RESET_TEXT);
				}
			}

			uint8_t mins, secs;
			mins = count / 60;
			secs = count % 60;
			PRINTF("\rEncoder value: %03d\tTime value: %02d:%02d", count, mins, secs);
		}

		previousState = currentState;

		// if semaphore from encoder button is given, save the state of it
		// and notify pit for countdown (send the value to it via queue or
		// notification)
		if(xSemaphoreTake(encoderSemaphore, 0) == pdPASS)
		{
			uint16_t finalValue = count;
			PRINTF("\n\rFinal value for timer: %d\n\r", finalValue);

			// send value to PIT (PIT will send value to DisplayTask)

			// delete task as it is no longer needed
			//vTaskDelete(NULL);
		}
	}
}

