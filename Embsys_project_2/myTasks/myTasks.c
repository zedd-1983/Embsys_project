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
TaskHandle_t			timeConfigTaskHandle = NULL;
extern TaskHandle_t 	ledTaskHandle;
SemaphoreHandle_t 		sw2Semaphore = NULL, sw3Semaphore = NULL,
						startEncoderTaskSemaphore = NULL, encoderSemaphore = NULL;
extern TaskHandle_t 	startupTaskHandle;
extern QueueHandle_t 	queueForPIT;
extern uint32_t 		ledToOn;
extern rtc_datetime_t 	RTC_1_dateTimeStruct;


void startupTask(void* pvParameters)
{
#ifdef SHOW_MESSAGES
	PRINTF("\n\rStartup Task\n\r");
#endif
	for(;;)
	{
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

		if(xSemaphoreTake(sw2Semaphore, 0) == pdTRUE) {
			// create timeConfig task
			if(xTaskCreate(timeConfig, "Time configuration task", configMINIMAL_STACK_SIZE + 100, NULL, 3, &timeConfigTaskHandle) == pdFAIL)
			{
				PRINTF(RED_TEXT"\n\r\t***** TimeConfig task creation failed *****\n\r"RESET_TEXT);
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
	NVIC_DisableIRQ(BOARD_SW2_IRQ);

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
			NVIC_EnableIRQ(BOARD_SW2_IRQ);
			vTaskDelete(NULL);
		}
	}
}

void timeConfig(void* pvParameters)
{
#ifdef SHOW_MESSAGES
	PRINTF("\n\rTime config\n\r");
#endif

	char stringDate[10] = "";
	char stringTime[5] = "";

	for(;;)
	{
		// disable ENC_BUTTON interrupt
		NVIC_DisableIRQ(PORTB_IRQn);
		//portENTER_CRITICAL();
		// configure time
		RTC_StopTimer(RTC);

		// get the new time
		PRINTF("\n\rEnter new date in format YYYY-MM-DD: ");
		SCANF("%s", stringDate);
		PRINTF("\n\rEnter new time in format HH-MM: ");
		SCANF("%s", stringTime);

		uint16_t newYear = ((stringDate[0] - 48) * 1000) +
				((stringDate[1] - 48) * 100) +
				((stringDate[2] - 48) * 10) +
				((stringDate[3] - 48));
		uint8_t newMonth = ((stringDate[5] - 48) * 10) + ((stringDate[6] - 48));
		uint8_t newDay = ((stringDate[8] - 48) * 10) + ((stringDate[9] - 48));

		uint8_t newHour = ((stringTime[0] - 48) * 10) + ((stringTime[1] - 48));
		uint8_t newMinute = ((stringTime[3] - 48) * 10) + ((stringTime[4] - 48));

		RTC_1_dateTimeStruct.year = newYear;
		RTC_1_dateTimeStruct.month = newMonth;
		RTC_1_dateTimeStruct.day = newDay;
		RTC_1_dateTimeStruct.hour = newHour;
		RTC_1_dateTimeStruct.minute = newMinute;

		RTC_SetDatetime(RTC, &RTC_1_dateTimeStruct);
#ifdef SHOW_MESSAGES
		PRINTF("\n\rYear: %d", RTC_1_dateTimeStruct.year);
		PRINTF("\n\rMonth: %d", RTC_1_dateTimeStruct.month);
		PRINTF("\n\rDay: %d", RTC_1_dateTimeStruct.day);
		PRINTF("\n\rHour: %d", RTC_1_dateTimeStruct.hour);
		PRINTF("\n\rMinute: %d", RTC_1_dateTimeStruct.minute);
#endif
		RTC_StartTimer(RTC);
		// enable ENC_BUTTON interrupt
		NVIC_EnableIRQ(PORTB_IRQn);
		//portEXIT_CRITICAL();
		// delete itself
		vTaskDelete(NULL);
	}
}

