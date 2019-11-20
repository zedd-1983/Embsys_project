/*
 * helperFunctions.h
 *
 *  Created on: 16 Nov 2019
 *      Author: zedd
 */

#ifndef HELPERFUNCTIONS_H_
#define HELPERFUNCTIONS_H_

struct userDate_t {
	uint16_t year;
	uint8_t month;
	uint8_t day;
};

struct userTime_t {
	uint8_t hour;
	uint8_t minute;
};

void allLedsOFF();
struct userDate_t getDate(char*);
struct userTime_t getTime(char*);
void busyWait(int);

#endif /* HELPERFUNCTIONS_H_ */
