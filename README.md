# CyBorg-Northbridge
[![Build Status](https://github.com/cyrusbuilt/CyBorg-Northbridge/actions/workflows/ci.yml/badge.svg)](https://github.com/cyrusbuilt/CyBorg-Northbridge/actions?query=workflows%3APlatformIO)

Firmware for the [CyBorg](https://github.com/cyrusbuilt/CyBorg) ViCREM chip (ATMEGA32A) (Formerly known as the "Northbridge")

## ViCREM (**Vi**rtual I/O, **C**lock, **R**OM/BIOS, **E**xpansion Peripheral I/O, **M**emory Managment)

## What does it do?
ViCREM is a sort of all-in-one chip that replaces most of the glue logic and peripheral chips you find in a more traditional Z80 system. I'm not familiar enough with CPLDs, PALs, GALs, or FPGAs, so something Arduino-based seemed like a suitable solution for me, and keeps the chip count low. ViCREM provides the functions:

- ROM/BIOS - iLoad Intel HEX Loader embedded and can load boot images from SD card.
- System Clock (4/8 MHz user selectable)
- Virtual I/O engine
- Memory Management (banked RAM access)
- Peripheral I/O
- - SPI bus for SD card reader
- - I2C bus
- - - Interface with Hardware RTC
- - - Interface with GPIO expander
- - RS-232 Serial UART

## How do I use it?
This project was built using [PlatformIO](https://platformio.org), so you'll need to follow the instructions to download and install it if you haven't already. Then clone this repo and in a terminal cd to the project directory. Now to build and flash it:

1) On a blank ATMEGA32A chip, you'll first need to set the fuses and flash the bootloader. To do this you'll need a programmer. Personally, I went the Arduino-as-ISP route using an Arduino Uno. You can follow [this tutorial](https://www.instructables.com/Programming-ATMEGA32-or-Any-Other-AVR-Using-Arduin/) on how to flash an Uno to be a programmer and wire up the ATMEGA32A on a breadboard for flashing, but skip the part where you actuall flash the ATMEGA32A. Next, in the `env:Upload_ArduinoISP` section of `platformio.ini`, make sure the `upload_port` parameter reflects the device name of the Arduino Uno. Now run the following command:

```sh
> pio run -e fuses_bootloader -t bootloader
```

This should set the fuses and flash the bootloader to the chip. **NOTE** this only needs to be done *ONCE*.

2) Once the ATMEGA32A has a bootloader or if you happened to get a chip that already has a bootloader, then you need to build and upload the actual firmware. To do this, we still the Arduino Uno we're using as a programmer in step 1. Making sure CyBorg is powered off, insert the ATMEGA32A chip into it's socket on the CyBorg board. **IMPORTANT NOTE** This procedure assumes you've already installed and flashed the KAMVA (Southbridge) chip and is functioning normally. Now connect the arduino the ICSP header on CyBorg:

| Arduino Pin | ->   | ICSP Pin |
| :---        | :--- | :---     |
| 13          | ->   | SCK      |
| 12          | ->   | MISO     |
| 11          | ->   | MOSI     |
| 10          | ->   | RST      |
| GND         | ->   | GND      |

3) With the programmer hooked up, make sure a power supply is connected to the ATX connector on CyBorg. The red 'standby' LED on KAMVA should be lit. Press the 'ON/OFF' button on CyBorg and the green PWR_OK LED should light up. Now run the following command:

```sh
> pio run -e Upload_ArduinoISP -t upload
```

When the command finishes successfully, the firmware should be uploaded and immediately begins running. If the integrated console is enabled or you are connected using an FTDI cable (see step 4), then you should be able to just press the RESET button on CyBorg to see ViCREM boot. By default, ViCREM plays a startup jingle via the onboard beeper when it boots successfully.

4) To monitor/debug ViCREM, you can either use a serial cable to connect to the DB-9 serial port on CyBorg or, use an FTDI cable (like [this one](https://www.adafruit.com/product/70?gclid=CjwKCAjwscGjBhAXEiwAswQqNB2296-7Bv-Y7F56Dpd_glp_bBPHJRNljWBkb1Cpdc8x13ulUFtHbBoCCFYQAvD_BwE)) and connect it to pin header H4. Now in the `env:Upload_UART` section of `platformio.ini`, make sure the `upload_port` parameter reflects your serial port or FTDI cable device name and then run the following command:

```sh
> pio run -t monitor
```

NOTE: Ideally, if all is working correctly step 4 won't even be necessary since as long as jumpers JP3 and JP4 (CONSOLE ENABLE) are jumpered on and the KAMVA (Soutbridge) is working correctly, you should be able to interract with the ViCREM using the integrated console.

ALSO NOTE: At this time, uploading firmware updates over serial is not yet supported, but should be in the near future. For now though, you MUST use the ICSP header.