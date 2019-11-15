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
TaskHandle_t encoderTaskHandle = NULL;
QueueHandle_t queueForPIT = NULL;

extern SemaphoreHandle_t sw2Semaphore, sw3Semaphore, encoderSemaphore;

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
	BaseType_t xHigherPriorityTaskWoken;
	GPIO_PortClearInterruptFlags(BOARD_ENC_BUTTON_GPIO, 1 << BOARD_ENC_BUTTON_PIN);

	xHigherPriorityTaskWoken = pdFALSE;
	xSemaphoreGiveFromISR(encoderSemaphore, &xHigherPriorityTaskWoken);
	//xSemaphoreGiveFromISR(encoderToPITSemaphore, &xHigherPriorityTaskWoken);
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
	BaseType_t xHigherPriorityTaskWoken;
	PIT_ClearStatusFlags(PIT, kPIT_Chnl_0, kPIT_TimerFlag);

	xHigherPriorityTaskWoken = pdFALSE;
	if(xQueueReceiveFromISR(queueForPIT, &receivedInterval, &xHigherPriorityTaskWoken) == pdTRUE) {
		PRINTF(GREEN_TEXT"\n\rReceived: %d\n\r"RESET_TEXT, receivedInterval);
		startCounting = true;
	}

	if(startCounting == true) {
		secondsCount++;
		if(secondsCount == (receivedInterval / 6)) {
			startCounting = false;
			secondsCount = 0;
			receivedInterval = 1;
			PRINTF(RED_TEXT"\n\r*********** Alarm ************\n\r"RESET_TEXT);
		}
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

    queueForPIT = xQueueCreate(5, sizeof(uint16_t));

    PRINTF(RED_TEXT"Embsys Project\n\r"RESET_TEXT);

//    if(xTaskCreate(ledTask, "LED task", configMINIMAL_STACK_SIZE + 20, NULL, 2, &ledTaskHandle) == pdFAIL)
//    {
//    	PRINTF(RED_TEXT"\n\r\t***** LED task creation failed *****\n\r"RESET_TEXT);
//    }

    if(xTaskCreate(encoderRead, "Encoder task", configMINIMAL_STACK_SIZE + 20, NULL, 2, &encoderTaskHandle) == pdFAIL)
    {
    	PRINTF(RED_TEXT"\n\r\t***** ENCODER task creation failed *****\n\r"RESET_TEXT);
    }

    sw2Semaphore = xSemaphoreCreateBinary();
    sw3Semaphore = xSemaphoreCreateBinary();
    encoderSemaphore = xSemaphoreCreateBinary();

    vTaskStartScheduler();

    while(1) {}

    return 0 ;
}

