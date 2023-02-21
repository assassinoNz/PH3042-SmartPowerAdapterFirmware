/*
 * Call setup_Analog() in setup
 * sets pinMode of mux pins, relay pin and switch pin 
 * 
 * int Voltage = getV()
 * returns RMS voltage
 * 
 * int Current = getI()
 * returns RMS current
 * 
 * bool success = relayOn(state)
 * state = true to turn on | false to turn off
 * will return true if relay mechanically action successful, else returns false
 * 
 * int r = switchAct() 
 * returns 0 if switch not pressed
 * returns 1 if medium press(>5s-toggle)
 * returns 2 if long press(>10s-reset)
 * 
 * bool s = getState();
 * returns true if relay on
 * returns false if relay off
 * 
 */

#ifndef SENSORREAD_H
#define SENSORREAD_H

#include <Arduino.h>

#define vOn D1
#define iOn D2
#define sw D7

#define relayPin LED_BUILTIN //D5
#define count 10000
#define relay_active LOW    //change depending on active high/low

void setup_Analog();
float getV();
float getI();
bool relayOn(bool b);
int switchAct();
void stable();
bool getState();

#endif
