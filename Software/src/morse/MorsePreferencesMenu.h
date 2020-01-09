#ifndef MORSEPREFERENCESMENU_H_
#define MORSEPREFERENCESMENU_H_

#include "MorsePreferences.h"

namespace MorsePreferencesMenu
{

    boolean menuExec(String mode);

    boolean setupPreferences(uint8_t atMenu);
    void displayKeyerPreferencesMenu(int pos);
    boolean adjustKeyerPreference(MorsePreferences::prefPos pos);
    boolean selectKochFilter();
}

#endif /* MORSEPREFERENCESMENU_H_ */
