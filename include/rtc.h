#ifndef _RTC_H
#define _RTC_H

#include <Arduino.h>

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
	DateTime* dateTime;

private:
	void print2Digit(byte data);
};

extern RtcClass RTC;
#endif