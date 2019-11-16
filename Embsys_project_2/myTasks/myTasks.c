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
#include "fsl_rtc.h"

#include "myDefines.h"
#include "myTasks.h"
#include "helperFunctions.h"

TaskHandle_t 			encoderTaskHandle = NULL;
extern TaskHandle_t 	ledTaskHandle;
SemaphoreHandle_t 		sw2Semaphore = NULL, sw3Semaphore = NULL,
						startEncoderTaskSemaphore = NULL, encoderSemaphore = NULL;
extern TaskHandle_t 	startupTaskHandle;
extern QueueHandle_t 	queueForPIT;
extern uint32_t ledToOn;
extern rtc_datetime_t RTC_1_dateTimeStruct;



void startupTask(void* pvParameters)
{
#ifdef SHOW_MESSAGES
	PRINTF("\n\rStartup Task\n\r");
#endif
	for(;;)
	{
//TODO: run RTC
//
		RTC_GetDatetime(RTC, &RTC_1_dateTimeStruct);

		PRINTF("\r%04d:%02d:%02d:%02d:%02d:%02d\r",
			RTC_1_dateTimeStruct.year,
			RTC_1_dateTimeStruct.month,
			RTC_1_dateTimeStruct.day,
			RTC_1_dateTimeStruct.hour,
			RTC_1_dateTimeStruct.minute,
			RTC_1_dateTimeStruct.second
			);

		if(xSemaphoreTake(startEncoderTaskSemaphore, 0) == pdTRUE)
		{
#ifdef SHOW_MESSAGES
			PRINTF("\n\rCreating Encoder Task\n\r");
#endif
			if(xTaskCreate(encoderRead, "Encoder task", configMINIMAL_STACK_SIZE + 20, NULL, 4, &encoderTaskHandle) == pdFAIL)
			{
				PRINTF(RED_TEXT"\n\r\t***** ENCODER task creation failed *****\n\r"RESET_TEXT);
			}

			if(xTaskCreate(ledTask, "LED task", configMINIMAL_STACK_SIZE + 20, NULL, 3, &ledTaskHandle) == pdFAIL)
			{
				PRINTF(RED_TEXT"\n\r\t***** LED task creation failed *****\n\r"RESET_TEXT);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void ledTask(void* pvParameters)
{
#ifdef SHOW_MESSAGES
	PRINTF("\n\rLED task\n\r");
#endif
	for(;;)
	{
		if(xTaskNotifyWait(0,0, &ledToOn, 0) == pdTRUE) {
			switch(ledToOn) {
				case 1 : allLedsOFF(); LED_RED_ON(); break;
				case 2 : allLedsOFF(); LED_BLUE_ON(); break;
				case 3 : allLedsOFF(); LED_GREEN_ON(); break;
				case 4 : allLedsOFF(); LED_RED_ON(); LED_BLUE_ON(); break;
				case 5 : allLedsOFF(); LED_BLUE_ON(); LED_GREEN_ON(); break;
				case 6 : allLedsOFF(); LED_GREEN_ON(); LED_RED_ON(); break;
				default: allLedsOFF();
			}
		}
		vTaskDelay(pdMS_TO_TICKS(100));
	}
}

void encoderRead(void* pvParameters)
{
#ifdef SHOW_MESSAGES
	PRINTF("\n\rEncoder Read Task\n\r");
#endif
	uint16_t count = ENCODER_MIN;
	uint16_t previousState;
	uint16_t currentState;
	uint8_t mins, secs;
	const uint8_t diff = 5;

	PRINTF(GREEN_TEXT"\n\rPlease adjust the time interval as required\n\r"RESET_TEXT);
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

			mins = count / 60;
			secs = count % 60;
			PRINTF("\rTime value: %02d:%02d", mins, secs);
		}

		previousState = currentState;

		// if semaphore from encoder button is given, save the state of it
		// and notify pit for countdown
		if(xSemaphoreTake(encoderSemaphore, 0) == pdPASS)
		{
			uint16_t finalValue = count;
			PRINTF("\n\rTime selected: %02d:%02d\n\r", mins, secs);
#ifdef SHOW_MESSAGES
			PRINTF("\n\rFinal value for timer: %d\n\r", finalValue);
#endif

			if(xQueueSend(queueForPIT, &finalValue, (TickType_t)10) != pdPASS)
				PRINTF(RED_TEXT"\rFailed to send data to PIT\n\r"RESET_TEXT);

#ifdef SHOW_MESSAGES
			PRINTF("\n\rDeleting Encoder Task\n\r");
#endif
			vTaskDelete(NULL);
		}
	}
}


