// board setup
/*
 * Encoder:
 * PTB2 	- encoder button 	ENC_BUT
 * PTB3 	- encoder data 		ENC_DT
 * PTB10 	- encoder clock 	ENC_clk
 *
 * LEDs:
 * PTC5 	- top left led		LED1
 * PTC7		- bottom lef led	LED2
 * PTC0		- middle top led	LED3
 * PTC9		- middle bottom led	LED4
 * PTC8		- top right led		LED5
 * PTC5		- bottom right led	LED6
 *
 * Tap sensor:
 * PTB19	- tap sensor signal	TAP
 */
#include <stdio.h>
#include "board.h"
#include "peripherals.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "MK64F12.h"
#include "fsl_debug_console.h"
#include "fsl_rtc.h"

// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

// my added files
#include "myTasks.h"
#include "myDefines.h"
#include "helperFunctions.h"

// my variables
TaskHandle_t ledTaskHandle = NULL;
TaskHandle_t startupTaskHandle = NULL;
QueueHandle_t queueForPIT = NULL;

extern SemaphoreHandle_t 	sw2Semaphore, sw3Semaphore, encoderSemaphore,
							startEncoderTaskSemaphore, timeoutSemaphore,
							ledTaskDeleteSemaphore;

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

	// temporary solution to de-bouncing, far from ideal
	busyWait(3000000);

	GPIO_PortClearInterruptFlags(BOARD_ENC_BUTTON_GPIO, 1 << BOARD_ENC_BUTTON_PIN);

	xHigherPriorityTaskWoken = pdFALSE;

	// if it's a first press, send semaphore to startupTask to create encoder task
	if(++pressCount == 1) {
		xSemaphoreGiveFromISR(startEncoderTaskSemaphore, &xHigherPriorityTaskWoken);
	}
	else { // it's a time confirmation
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
	//static uint8_t timeout = 0;
	static uint8_t ledInterval = 0;
	BaseType_t xHigherPriorityTaskWoken;

	PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);

	xHigherPriorityTaskWoken = pdFALSE;
	if(xQueueReceiveFromISR(queueForPIT, &receivedInterval, &xHigherPriorityTaskWoken) == pdTRUE) {
#ifdef SHOW_MESSAGES
		PRINTF(GREEN_TEXT"\n\rReceived: %d\n\r"RESET_TEXT, receivedInterval);
#endif
		xTaskNotifyFromISR(ledTaskHandle, ++ledToOn, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
		ledInterval = receivedInterval / 6;
		startCounting = true;
	}

	if(startCounting == true) {
		secondsCount++;
		if(secondsCount == ledInterval) {	// if timer expires in 1/6th
			xTaskNotifyFromISR(ledTaskHandle, ++ledToOn, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
			secondsCount = 0;
			if(++loopCount == 6) {	// keep looping until 6th iteration, then stop the alarms
				PRINTF(GREEN_TEXT"\n\rFinished NOW\n\r"RESET_TEXT);
				startCounting = false;
				secondsCount = 0;
				receivedInterval = 1;
				loopCount = 0;
				ledToOn = 0;
				xSemaphoreGiveFromISR(ledTaskDeleteSemaphore, &xHigherPriorityTaskWoken);
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

    // encoder switch
    NVIC_SetPriority(PORTB_IRQn, 7);
    NVIC_ClearPendingIRQ(PORTB_IRQn);
    NVIC_EnableIRQ(PORTB_IRQn);

    queueForPIT = xQueueCreate(1, sizeof(uint16_t));

    PRINTF(GREEN_TEXT"FreeRTOS Project\n\r"RED_TEXT"Toothbrushing Application\n\n\r"RESET_TEXT);
    PRINTF("\nPress encoder button to select the interval,\n\rpress again to confirm\n\r");
    PRINTF("\nPress SW2 to configure time and date of the system\n\r");

    if(xTaskCreate(startupTask, "Startup task", configMINIMAL_STACK_SIZE + 50, NULL, 2, &startupTaskHandle) == pdFAIL)
    {
    	PRINTF(RED_TEXT"\n\r\t***** Startup task creation failed *****\n\r"RESET_TEXT);
    }


    sw2Semaphore = xSemaphoreCreateBinary();
    sw3Semaphore = xSemaphoreCreateBinary();
    encoderSemaphore = xSemaphoreCreateBinary();
    startEncoderTaskSemaphore = xSemaphoreCreateBinary();
    ledTaskDeleteSemaphore = xSemaphoreCreateBinary();

    vTaskStartScheduler();

    while(1) {}

    return 0 ;
}

