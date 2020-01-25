#ifndef MORSEMODELORA_H_
#define MORSEMODELORA_H_

#include <Arduino.h>
#include <WString.h>

#include "MorseMode.h"

class MorseModeLoRa: public MorseMode
{
    public:
        boolean menuExec(String mode);
        boolean loop();
        boolean togglePause();
        void onPreferencesChanged();
};

extern MorseModeLoRa morseModeLoRa;

#endif /* MORSEMODELORA_H_ */
