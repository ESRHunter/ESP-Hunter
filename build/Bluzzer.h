#ifndef BLUZZER_H
#define BLUZZER_H

#include <Arduino.h>
#include "Config.h"

extern bool buzzerEnabled; 
extern unsigned long buzzerStartTime;
extern int buzzerDuration;
extern int buzzerVolume; 

void initBuzzer();
void playBeep(int freq, int duration, int volume = -1); 
void updateBuzzer();

#endif