#ifndef _RTC_H
#define _RTC_H

#include <Arduino.h>

const byte DAYS_OF_MONTH[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

struct DateTime {
	byte seconds;
	byte minutes;
	byte hours;
	byte s;
	byte day;
	byte month;
	byte year;
	byte tempC;
};

class RtcClass {
public:
	RtcClass();
	void update();
	void save();
	bool autoSet();
	void printDateTime(bool readDone);
	bool isLeapYear(byte year);
	void print2Digit(byte data);
	DateTime* dateTime;
};

extern RtcClass RTC;
#endif