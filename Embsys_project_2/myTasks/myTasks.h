/*
 * myTasks.h
 *
 *  Created on: 14 Nov 2019
 *      Author: zedd
 */

#ifndef MYTASKS_H_
#define MYTASKS_H_


void startupTask(void* pvParameters);
void ledTask(void* pvParameters);
void encoderRead(void* pvParameters);
void timeConfig(void* pvParameters);
//void tapSensorTask(void* pvParameters);

#endif /* MYTASKS_H_ */
