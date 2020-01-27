/******************************************************************************************************************************
 *  morse_3 Software for the Morserino-32 multi-functional Morse code machine, based on the Heltec WiFi LORA (ESP32) module ***
 *  Copyright (C) 2018  Willi Kraml, OE1WKL                                                                                 ***
 *
 *  This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 *  of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with this program.
 *  If not, see <https://www.gnu.org/licenses/>.
 *****************************************************************************************************************************/

#include "MorseModeKeyer.h"
#include "MorseKeyer.h"
#include "MorseMachine.h"
#include "MorsePreferences.h"
#include "MorseGenerator.h"
#include "decoder.h"
#include "MorseLoRa.h"
#include "MorseDisplay.h"
#include "MorseModeEchoTrainer.h"
#include "MorseSound.h"

MorseModeKeyer morseModeKeyer;

boolean MorseModeKeyer::menuExec(String mode)
{
    MorseKeyer::setup();

    if (mode == "a")
    {
        MorseMachine::morseState = MorseMachine::morseKeyer;
        MorseDisplay::clear();
        MorseDisplay::printOnScroll(1, REGULAR, 0, "Start CW Keyer");
        delay(500);
        MorseDisplay::clear();
        MorseDisplay::displayTopLine();
        MorseDisplay::printToScroll(REGULAR, "");      // clear the buffer
        MorseKeyer::clearPaddleLatches();
        MorseKeyer::keyTx = true;

        MorseModeKeyer::onPreferencesChanged();
    }
    else if (mode == "trx")
    {
        MorseMachine::morseState = MorseMachine::morseTrx;
        MorseDisplay::clear();
        MorseDisplay::printOnScroll(1, REGULAR, 0, "Start CW Trx");
        MorseKeyer::clearPaddleLatches();
        MorseKeyer::keyTx = true;
        Decoder::startDecoder();
    }
    return true;
}

boolean MorseModeKeyer::loop()
{
    return MorseKeyer::doPaddleIambic();
}

void MorseModeKeyer::onPreferencesChanged()
{
    if (MorseMachine::morseState == MorseMachine::morseKeyer)
    {
        unsigned char mode = MorsePreferences::prefs.keyTrainerMode;
        MorseKeyer::keyTx = (mode == 1 || mode == 2);
    }
}

boolean MorseModeKeyer::togglePause()
{
    return false;
}
