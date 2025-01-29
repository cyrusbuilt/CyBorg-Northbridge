#ifndef _BUSCTRL
#define _BUSCTRL

#include <Arduino.h>
#include "Adafruit_MCP23X17.h"

class BusControlClass {
public:
    BusControlClass();
    void init();
    bool hasIOEXP();
    bool hasBUSCTLR();
    void writeGPIOA(byte data);
    void writeGPIOB(byte data);
    void writeIODirA(byte data);
    void writeIODirB(byte data);
    void writeGPPUA(byte data);
    void writeGPPUB(byte data);
    byte readGPIOA();
    byte readGPIOB();
    void detectCards();
    bool card1Present();
    bool card2Present();
    bool card3Present();
    bool noCardsPresent() { return !_card1Present && !_card2Present && !_card3Present; }

private:
    bool _hasIOXEP;
    bool _hasBUSCTRL;
    bool _card1Present;
    bool _card2Present;
    bool _card3Present;
    Adafruit_MCP23X17 _busctlr;
};


extern BusControlClass BusControl;
#endif