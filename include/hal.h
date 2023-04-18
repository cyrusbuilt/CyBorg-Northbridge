#ifndef _HAL_H
#define _HAL_H

#include <Arduino.h>
#include "PetitFS.h"

/**
 * @brief Hardware definitions for base system.
 */

#define SERIAL_BAUD_RATE 115200  // BAUD rate for the Serial port.

// Z80 data bus
#define PIN_D0 24      // PA0 pin 40
#define PIN_D1 25      // PA1 pin 39
#define PIN_D2 26      // PA2 pin 38
#define PIN_D3 27      // PA3 pin 37
#define PIN_D4 28      // PA4 pin 36
#define PIN_D5 29      // PA5 pin 35
#define PIN_D6 30      // PA6 pin 34
#define PIN_D7 31      // PA7 pin 33

// Control and memory
#define PIN_AD0 18     // PC2 pin 24 - Z80 A0
#define PIN_WR 19      // PC3 pin 25 - Z80 WR
#define PIN_RD 20      // PC4 pin 26 - Z80 RD
#define PIN_MREQ 21    // PC5 pin 27 - Z80 MREQ
#define PIN_RESET 22   // PC6 pin 28 - Z80 RESET
#define PIN_BANK1 11   // PD3 pin 17 - RAM memory bank address (high)
#define PIN_BANK0 12   // PD4 pin 18 - RAM memory bank address (low)
#define PIN_INT 1      // PB1 pin 2 - Z80 control bus
#define PIN_RAM_CE2 2  // PB2 pin 3 - RAM chip enable (CE2). Active HIGH. Used only during boot.
#define PIN_WAIT 3     // PB3 pin 4 - Z80 WAIT
#define PIN_BUSREQ 14  // PD6 pin 20 - Z80 BUSREQ
#define PIN_CLK 15     // PD7 pin 21 - Z80 CLK

// SPI Bus
#define PIN_SS 4       // PB4 pin 5 - SD SPI - Chip select
#define PIN_MOSI 5     // PB5 pin 6 - SD SPI - Master Out/Slave In
#define PIN_MISO 6     // PB6 pin 7 - SD SPI - Master In/Slave Out
#define PIN_SCK 7      // PB7 pin 8 - SD SPI - Clock

// I2C Bus
#define PIN_SCL 16     // PC0 pin 22 - Clock
#define PIN_SDA 17     // PC1 pin 23 - Data

// Misc
#define PIN_IOS_LED 0  // PB0 pin 1 - IOS LED is ON if HIGH
#define PIN_WAIT_RES 0 // PB0 pin 1 - Reset the WAIT FF
#define PIN_USER 13    // PD5 pin 19 - USER LED and button (ON if LOW)
#define PIN_RUN 23     // PC7 pin 29 - RUN signal from Southbridge

/**
 * @brief Hardware definitions GPIO expansion
 */

#define GPIOEXP_ADDR 0x20   // MCP23017 address (see datasheet)
#define IODIRA_REG 0x00     // internal register IODIRA
#define IODIRB_REG 0x01     // internal register IODIRB
#define GPPUA_REG 0x0C      // internal register GPPUA
#define GPPUB_REG 0x0D      // internal register GPPUB
#define GPIOA_REG 0x12      // internal register GPIOA
#define GPIOB_REG 0x13      // internal register GPIOB

/**
 * @brief Hardware definitions for RTC
 */

#define DS1307_RTC 0x68     // DS1307 I2C address
#define DS1307_SECRG 0x00   // DS1307 seconds register
#define DS1307_STATRG 0x0F  // DS1307 status register
#define DS1307_OSC 0x08     // DS1307 Oscillator Stop Flag (32KHz output left enabled)

/**
 * @brief File names and starting addresses
 */
#define ZERO_ADDR 0x0000
#define BASICFN "BASIC47.BIN"
#define FORTHFN "FORTH13.BIN"
#define CPMFN "CPM22.BIN"
#define QPMFN "QPMLDR.BIN"
#define CPM3FN "CPMLDR.BIN"
#define FUZIXFN "FUZIX.BIN"
#define UCSDFN "UCSDLDR.BIN"
#define AUTOFN "AUTOBOOT.BIN"
#define Z80DISK "DSxNyy.DSK"
#define DS_OSNAME "DSxNAM.DAT"
#define BASSTRADDR ZERO_ADDR
#define FORSTRADDR 0x0100
#define CPM22CBASE 0xD200
#define CPMSTRADDR (CPM22CBASE - 32)
#define QPMSTRADDR 0x80
#define CPM3STRADDR ZERO_ADDR
#define UCSDSTRADDR ZERO_ADDR
#define FUZSTRADDR ZERO_ADDR
#define AUTSTRADDR ZERO_ADDR

#define MAX_TRACKS 512
#define MAX_SECTORS 32

#define KEY_CODE_CR 13
#define KEY_CODE_ESC 27

#define OS_MEM_BANK_0 0
#define OS_MEM_BANK_1 1
#define OS_MEM_BANK_2 2

/**
 * @brief Clock speed check
 */

#if F_CPU == 20000000
	#define CLOCK_LOW "5"
	#define CLOCK_HIGH "10"
#else
	#define CLOCK_LOW "4"
	#define CLOCK_HIGH "8"
#endif

/**
 * @brief 
 * 
 */
enum class DebugMode : uint8_t {
  OFF = 0,
  ON = 1,
  TRACE = 2
};

/**
 * @brief 
 * 
 * @param numPulse 
 */
void pulseClock(byte numPulse);

/**
 * @brief 
 * 
 */
void singlePulseResetZ80();

/**
 * @brief 
 * 
 * @param value 
 */
void loadHL(word value);

/**
 * @brief 
 * 
 * @param value 
 */
void loadByteToRAM(byte value);

/**
 * @brief 
 * 
 * @param value 
 */
void printBinaryByte(byte value);

/**
 * @brief 
 * 
 * @param intFlagUsed 
 */
void serialEvent(bool intFlagUsed);

/**
 * @brief 
 * 
 */
void flushSerialRXBuffer();

/**
 * @brief 
 * 
 * @param timestamp 
 */
void blinkIOSled(unsigned long *timestamp);

/**
 * @brief 
 * 
 * @param fatfs 
 * @return byte
 */
byte mountSD(FATFS* fatfs);

/**
 * @brief 
 * 
 * @param fileName 
 * @return byte 
 */
byte openSD(const char* fileName);

/**
 * @brief 
 * 
 * @param buffSD 
 * @param readBytes 
 * @return byte 
 */
byte readSD(void* buffSD, byte* readBytes);

/**
 * @brief 
 * 
 * @param secNum 
 * @return byte 
 */
byte seekSD(word sectNum);

/**
 * @brief 
 * 
 * @param buffSD 
 * @param numWrittenBytes 
 * @return byte 
 */
byte writeSD(void* buffSD, byte* numWrittenBytes);

/**
 * @brief 
 * 
 * @param opType 
 * @param errCode 
 * @param fileName 
 */
void printErrSD(byte opType, byte errCode, const char* fileName);

/**
 * @brief 
 * 
 */
void waitKeySD();

/**
 * @brief 
 * 
 */
void exitWaitState();

#endif