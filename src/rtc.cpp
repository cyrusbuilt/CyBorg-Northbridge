#include <Wire.h>
#include "hal.h"
#include "iLoad.h"
#include "rtc.h"

byte decToBcd(byte val) {
	return ((val / 10 * 16) + (val % 10));
}

byte bcdToDec(byte val) {
	return ((val / 16 * 10) + (val % 16));
}

RtcClass::RtcClass() {
	dateTime = new DateTime {0, 0, 0, 0, 0, 0, 0};
}

void RtcClass::update() {
	// Set the seconds register.
	Wire.beginTransmission(DS1307_RTC);
	Wire.write(DS1307_SECRG);
	Wire.endTransmission();

	// Read from RTC and convert to binary.
	Wire.requestFrom(DS1307_RTC, 18);
	dateTime->seconds = bcdToDec(Wire.read() & 0x7f);
	dateTime->minutes = bcdToDec(Wire.read());
	dateTime->hours = bcdToDec(Wire.read() & 0x3f);
	Wire.read(); // Skip over DoW.
	dateTime->day = bcdToDec(Wire.read());
	dateTime->month = bcdToDec(Wire.read());
	dateTime->year = bcdToDec(Wire.read());
	
	// Skip over the next 10 registers.
	for (uint8_t i = 0; i < 10; i++) {
		Wire.read();
	}

	// Get tempurature.
	dateTime->tempC = Wire.read();
}

void RtcClass::save() {
	Wire.beginTransmission(DS1307_RTC);
	Wire.write(DS1307_SECRG);
	Wire.write(decToBcd(dateTime->seconds));
	Wire.write(decToBcd(dateTime->minutes));
	Wire.write(decToBcd(dateTime->hours));
	Wire.write(1);  // DoW not used (always set to 1 = Sunday)
	Wire.write(decToBcd(dateTime->day));
	Wire.write(decToBcd(dateTime->month));
	Wire.write(decToBcd(dateTime->year));
	Wire.endTransmission();
}

void RtcClass::print2Digit(byte data) {
	if (data < 10) {
		Serial.print(F("0"));
	}
	Serial.print(data);
}

void RtcClass::printDateTime(bool readDone) {
	if (readDone) {
		update();
	}

	print2Digit(dateTime->year);
	Serial.print(F("/"));
	print2Digit(dateTime->month);
	Serial.print(F("/"));
	print2Digit(dateTime->day);
	Serial.print(F(" "));
	print2Digit(dateTime->hours);
	Serial.print(F(":"));
	print2Digit(dateTime->minutes);
	Serial.print(F(":"));
	print2Digit(dateTime->seconds);
}

bool RtcClass::autoSet() {
	Wire.beginTransmission(DS1307_RTC);
	if (Wire.endTransmission() != 0) {
		return false;
	}

	#ifdef DEBUG
	Serial.print(F("INIT: boot3 - IOS: Found RTC DS1307 module ("));
	printDateTime(true);
	Serial.println(F(")"));

	Serial.print(F("INIT: boot3 - IOS: RTC DS1307 temperature: "));
	Serial.print((int8_t)dateTime->tempC);
	Serial.println(F("C"));
	#endif

	// Read oscillator stop flag.
	Wire.beginTransmission(DS1307_RTC);
	Wire.write(DS1307_STATRG);
	Wire.endTransmission();
	Wire.requestFrom(DS1307_RTC, 1);
	byte ocsStopFlag = Wire.read() & 0x80;

	// RTC oscillator stopped. RTC must be set to compile date/time.
	if (ocsStopFlag) {
		// Convert compile time strings to numeric values.
		dateTime->seconds = compTimeStr.substring(6, 8).toInt();
		dateTime->minutes = compTimeStr.substring(3, 5).toInt();
		dateTime->hours = compTimeStr.substring(0, 2).toInt();
		dateTime->day = compDateStr.substring(4, 6).toInt();
		switch (compDateStr[0]) {
			case 'J':
				dateTime->month = compDateStr[1] == 'a' ? 1 : dateTime->month = compDateStr[2] == 'n' ? 6 : 7;
				break;
			case 'F':
				dateTime->month = 2;
				break;
			case 'A':
				dateTime->month = compDateStr[2] == 'r' ? 4 : 8;
				break;
			case 'M':
				dateTime->month = compDateStr[2] == 'r' ? 3 : 5;
				break;
			case 'S':
				dateTime->month = 9;
				break;
			case 'O':
				dateTime->month = 10;
				break;
			case 'N':
				dateTime->month = 11;
				break;
			case 'D':
				dateTime->month = 12;
				break;
			default:
				break;
		}
		dateTime->year = compDateStr.substring(9, 11).toInt();

		// Ask to set RTC to compile time on failure.
		Serial.println(F("IOS: RTC failure!"));
		Serial.print(F("\nDo you want to set RTC to IOS compile time ("));
		printDateTime(false);
		Serial.print(F(")? [Y/N] >"));
		
		char inChar;
		unsigned long timestamp = millis();
		do {
			blinkIOSled(&timestamp);
			inChar = Serial.read();
		} while((inChar != 'y') && (inChar != 'Y') && (inChar != 'n') && (inChar != 'N'));
		Serial.println(inChar);

		// If yes, set the RTC to the compile date/time and print message.
		if ((inChar == 'y') || (inChar == 'Y')) {
			save();
			Serial.print(F("IOS: RTC set to compile time - Now: "));
			printDateTime(true);
			Serial.println();
		}

		// Reset oscillator stop flag.
		Wire.beginTransmission(DS1307_RTC);
		Wire.write(DS1307_STATRG);
		Wire.write(DS1307_OSC);
		Wire.endTransmission();
	}

	return true;
}

RtcClass RTC;