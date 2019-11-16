// board setup
/*
 * PTB2 	- encoder button 	ENC_BUT
 * PTB3 	- encoder data 		ENC_DT
 * PTB10 	- encoder clock 	ENC_clk
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK64F12.h"
#include "fsl_debug_console.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

// my added files
#include "myTasks.h"
#include "myDefines.h"

// my variables
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t startupTaskHandle = NULL;
QueueHandle_t queueForPIT = NULL;

extern SemaphoreHandle_t sw2Semaphore, sw3Semaphore, encoderSemaphore, startEncoderTaskSemaphore;

uint32_t ledToOn = 0;

void PORTC_IRQHandler()
{
	BaseType_t xHigherPriorityTaskWoken;
	GPIO_PortClearInterruptFlags(BOARD_SW2_GPIO, 1 << BOARD_SW2_GPIO_PIN);

	xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(sw2Semaphore, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void PORTB_IRQHandler()
{
	static uint8_t pressCount = 0;
	BaseType_t xHigherPriorityTaskWoken;
	for(int i = 0; i < 2000000; i++);
	GPIO_PortClearInterruptFlags(BOARD_ENC_BUTTON_GPIO, 1 << BOARD_ENC_BUTTON_PIN);

	xHigherPriorityTaskWoken = pdFALSE;
	// if it's a first press, create encoder task and start it
	if(++pressCount == 1)
		xSemaphoreGiveFromISR(startEncoderTaskSemaphore, &xHigherPriorityTaskWoken);
	else	// otherwise it denotes confirmation of interval time
	{
		xSemaphoreGiveFromISR(encoderSemaphore, &xHigherPriorityTaskWoken);
		pressCount = 0;
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void PORTA_IRQHandler()
{
	BaseType_t xHigherPriorityTaskWoken;
	GPIO_PortClearInterruptFlags(BOARD_SW3_GPIO, 1 << BOARD_SW3_GPIO_PIN);

	xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(sw3Semaphore, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

void PIT0_IRQHandler()
{
	static uint8_t secondsCount = 0;
	static bool startCounting = false;
	static uint16_t receivedInterval = 1;
	static uint8_t loopCount = 0;
	BaseType_t xHigherPriorityTaskWoken;
	PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);

	xHigherPriorityTaskWoken = pdFALSE;
	if(xQueueReceiveFromISR(queueForPIT, &receivedInterval, &xHigherPriorityTaskWoken) == pdTRUE) {
#ifdef SHOW_MESSAGES
		PRINTF(GREEN_TEXT"\n\rReceived: %d\n\r"RESET_TEXT, receivedInterval);
#endif
		xTaskNotifyFromISR(ledTaskHandle, ++ledToOn, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
		startCounting = true;
	}

	if(startCounting == true) {
		secondsCount++;
		if(secondsCount == (receivedInterval / 6)) {	// if timer expires in 1/6th
			secondsCount = 0;
			// notify LED task to switch appropriate LED on
			xTaskNotifyFromISR(ledTaskHandle, ++ledToOn, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
			PRINTF(RED_TEXT"\n\r*********** Alarm ************\n\r"RESET_TEXT);
			if(++loopCount == 6) {	// keep looping until 6th iteration, then stop the alarms
				PRINTF("\n\rFinished NOW\n\r");
				startCounting = false;
				secondsCount = 0;
				receivedInterval = 1;
				loopCount = 0;
				ledToOn = 0;
			}
		}
//		else if ((secondsCount < receivedInterval) &&	// if timer gets interrupted by another press
//				(uxQueueMessagesWaiting(queueForPIT) > 0)) {
//				//(xQueueIsQueueEmptyFromISR(queueForPIT) == pdFALSE)) {
//				//(xQueueReceiveFromISR(queueForPIT, &receivedInterval, &xHigherPriorityTaskWoken) == pdTRUE)) {
//			PRINTF("\n\rRESET\n\r");
//			secondsCount = 0;
//		}
	}
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

int main(void) {

  	/* Init board hardware. */
    BOARD_InitBootPins();
    BOARD_InitBootClocks();
    BOARD_InitBootPeripherals();
  	/* Init FSL debug console. */
    BOARD_InitDebugConsole();

    // enable interrupts in NVIC for the buttons
    NVIC_SetPriority(BOARD_SW2_IRQ, 5);
    NVIC_ClearPendingIRQ(BOARD_SW2_IRQ);
    NVIC_EnableIRQ(BOARD_SW2_IRQ);

    NVIC_SetPriority(BOARD_SW3_IRQ, 6);
    NVIC_ClearPendingIRQ(BOARD_SW3_IRQ);
    NVIC_EnableIRQ(BOARD_SW3_IRQ);

    NVIC_SetPriority(PORTB_IRQn, 7);
    NVIC_ClearPendingIRQ(PORTB_IRQn);
    NVIC_EnableIRQ(PORTB_IRQn);

    queueForPIT = xQueueCreate(1, sizeof(uint16_t));

    PRINTF(GREEN_TEXT"FreeRTOS Project\n\r"RED_TEXT"Toothbrushing Application\n\r"RESET_TEXT);
    PRINTF("\nPress encoder button to select the interval,\n\rpress again to confirm\n\r");

    if(xTaskCreate(startupTask, "Startup task", configMINIMAL_STACK_SIZE + 20, NULL, 2, &startupTaskHandle) == pdFAIL)
    {
    	PRINTF(RED_TEXT"\n\r\t***** Startup task creation failed *****\n\r"RESET_TEXT);
    }

    sw2Semaphore = xSemaphoreCreateBinary();
    sw3Semaphore = xSemaphoreCreateBinary();
    encoderSemaphore = xSemaphoreCreateBinary();
    startEncoderTaskSemaphore = xSemaphoreCreateBinary();

    vTaskStartScheduler();

    while(1) {}

    return 0 ;
}

