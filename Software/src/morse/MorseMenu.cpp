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

#include <Arduino.h>

#include "MorseMenu.h"
#include "MorseSystem.h"
#include "koch.h"
#include "MorseDisplay.h"
#include "MorsePreferences.h"
#include "MorsePreferencesMenu.h"
#include "MorseKeyer.h"
#include "MorseUI.h"
#include "MorseRotaryEncoder.h"
#include "MorseGenerator.h"
#include "MorseLoRa.h"
#include "MorseEchoTrainer.h"
#include "decoder.h"
#include "MorseMachine.h"
#include "MorsePlayerFile.h"
#include "MorseWifi.h"
#include "MorseHeadCopying.h"
#include "MorseText.h"

using namespace MorseMenu;

#define setCurrentOptions(a) MorsePreferences::currentOptions = a; MorsePreferences::currentOptionSize = SizeOfArray(a);

namespace internal
{
    void menuDisplay(uint8_t ptr);
    boolean menuExec();
    boolean nothing(String mode);
}

////// The MENU

//////// variables and constants for the modus menu

enum navi
{
    naviLevel, naviLeft, naviRight, naviUp, naviDown
};

typedef struct MenuItem
{
        String text;
        menuNo no;
        uint8_t nav[5];
        MorseText::GEN_TYPE generatorMode;
        MorsePreferences::prefPos *options;
        boolean remember;
        boolean (*menufx)(String);
        String menufxParam;
} menuItem_t;

const menuItem_t menuItems[] = {
        {"", _dummy, {0, 0, 0, 0, 0}, MorseText::NA, MorsePreferences::allOptions, true, &internal::nothing, ""}, //

        {"CW Keyer", _keyer, {0, _goToSleep, _gen, _dummy, 0}, MorseText::NA, MorsePreferences::keyerOptions, true,
                &MorseKeyer::menuExec, "a"}, //

        {"CW Generator", _gen, {0, _keyer, _echo, _dummy, _genRand}, MorseText::NA, MorsePreferences::generatorOptions, true,
                &internal::nothing, ""}, //
        {"Random", _genRand, {1, _genPlayer, _genAbb, _gen, 0}, MorseText::RANDOMS, MorsePreferences::generatorOptions, true,
                &MorseGenerator::menuExec, "a"}, //
        {"CW Abbrevs", _genAbb, {1, _genRand, _genWords, _gen, 0}, MorseText::ABBREVS, MorsePreferences::generatorOptions, true,
                &MorseGenerator::menuExec, "a"}, //
        {"English Words", _genWords, {1, _genAbb, _genCalls, _gen, 0}, MorseText::WORDS, MorsePreferences::generatorOptions, true,
                &MorseGenerator::menuExec, "a"}, //
        {"Call Signs", _genCalls, {1, _genWords, _genMixed, _gen, 0}, MorseText::CALLS, MorsePreferences::generatorOptions, true,
                &MorseGenerator::menuExec, "a"}, //
        {"Mixed", _genMixed, {1, _genCalls, _genPlayer, _gen, 0}, MorseText::MIXED, MorsePreferences::generatorOptions, true,
                &MorseGenerator::menuExec, "a"}, //
        {"File Player", _genPlayer, {1, _genMixed, _genRand, _gen, 0}, MorseText::PLAYER, MorsePreferences::playerOptions, true,
                &MorseGenerator::menuExec, "player"}, //

        {"Echo Trainer", _echo, {0, _gen, _koch, _dummy, _echoRand}, MorseText::NA, MorsePreferences::echoTrainerOptions, true,
                &internal::nothing, ""}, //
        {"Random", _echoRand, {1, _echoPlayer, _echoAbb, _echo, 0}, MorseText::RANDOMS, MorsePreferences::echoTrainerOptions, true,
                &MorseEchoTrainer::menuExec, "a"}, //
        {"CW Abbrevs", _echoAbb, {1, _echoRand, _echoWords, _echo, 0}, MorseText::ABBREVS, MorsePreferences::echoTrainerOptions, true,
                &MorseEchoTrainer::menuExec, "a"}, //
        {"English Words", _echoWords, {1, _echoAbb, _echoCalls, _echo, 0}, MorseText::WORDS, MorsePreferences::echoTrainerOptions,
                true, &MorseEchoTrainer::menuExec, "a"}, //
        {"Call Signs", _echoCalls, {1, _echoWords, _echoMixed, _echo, 0}, MorseText::CALLS, MorsePreferences::echoTrainerOptions, true,
                &MorseEchoTrainer::menuExec, "a"}, //
        {"Mixed", _echoMixed, {1, _echoCalls, _echoPlayer, _echo, 0}, MorseText::MIXED, MorsePreferences::echoTrainerOptions, true,
                &MorseEchoTrainer::menuExec, "a"}, //
        {"File Player", _echoPlayer, {1, _echoMixed, _echoRand, _echo, 0}, MorseText::PLAYER, MorsePreferences::echoPlayerOptions,
                true, &MorseEchoTrainer::menuExec, "player"}, //

        {"Koch Trainer", _koch, {0, _echo, _head, _dummy, _kochSel}, MorseText::NA, MorsePreferences::kochEchoOptions, true,
                &internal::nothing, ""}, //
        {"Select Lesson", _kochSel, {1, _kochEcho, _kochLearn, _koch, 0}, MorseText::NA, MorsePreferences::kochEchoOptions, true,
                &MorsePreferencesMenu::menuExec, "selectKoch"}, //
        {"Learn New Chr", _kochLearn, {1, _kochSel, _kochGen, _koch, 0}, MorseText::KOCH_LEARN, MorsePreferences::kochEchoOptions,
                true, &Koch::menuExec, "learn"}, //
        {"CW Generator", _kochGen, {1, _kochLearn, _kochEcho, _koch, _kochGenRand}, MorseText::NA, MorsePreferences::kochGenOptions,
                true, &internal::nothing, ""}, //
        {"Random", _kochGenRand, {2, _kochGenMixed, _kochGenAbb, _kochGen, 0}, MorseText::RANDOMS, MorsePreferences::kochGenOptions,
                true, &Koch::menuExec, "trainer"}, //
        {"CW Abbrevs", _kochGenAbb, {2, _kochGenRand, _kochGenWords, _kochGen, 0}, MorseText::ABBREVS,
                MorsePreferences::kochGenOptions, true, &Koch::menuExec, "trainer"}, //
        {"English Words", _kochGenWords, {2, _kochGenAbb, _kochGenMixed, _kochGen, 0}, MorseText::WORDS,
                MorsePreferences::kochGenOptions, true, &Koch::menuExec, "trainer"}, //
        {"Mixed", _kochGenMixed, {2, _kochGenWords, _kochGenRand, _kochGen, 0}, MorseText::MIXED, MorsePreferences::kochGenOptions,
                true, &Koch::menuExec, "trainer"}, //

        {"Echo Trainer", _kochEcho, {1, _kochGen, _kochSel, _koch, _kochEchoRand}, MorseText::NA, MorsePreferences::kochEchoOptions,
                true, &internal::nothing, ""}, //
        {"Random", _kochEchoRand, {2, _kochEchoMixed, _kochEchoAbb, _kochEcho, 0}, MorseText::RANDOMS,
                MorsePreferences::kochEchoOptions, true, &Koch::menuExec, "echo"}, //
        {"CW Abbrevs", _kochEchoAbb, {2, _kochEchoRand, _kochEchoWords, _kochEcho, 0}, MorseText::ABBREVS,
                MorsePreferences::kochEchoOptions, true, &Koch::menuExec, "echo"}, //
        {"English Words", _kochEchoWords, {2, _kochEchoAbb, _kochEchoMixed, _kochEcho, 0}, MorseText::WORDS,
                MorsePreferences::kochEchoOptions, true, &Koch::menuExec, "echo"}, //
        {"Mixed", _kochEchoMixed, {2, _kochEchoWords, _kochEchoRand, _kochEcho, 0}, MorseText::MIXED,
                MorsePreferences::kochEchoOptions, true, &Koch::menuExec, "echo"}, //

        {"Head Copying", _head, {0, _koch, _trx, _dummy, _headRand}, MorseText::NA, MorsePreferences::headOptions, true,
                &internal::nothing, ""}, //
        {"Random", _headRand, {1, _headPlayer, _headAbb, _head, 0}, MorseText::RANDOMS, MorsePreferences::headOptions, true,
                &MorseHeadCopying::menuExec, "a"}, //
        {"CW Abbrevs", _headAbb, {1, _headRand, _headWords, _head, 0}, MorseText::ABBREVS, MorsePreferences::headOptions, true,
                &MorseHeadCopying::menuExec, "a"}, //
        {"English Words", _headWords, {1, _headAbb, _headCalls, _head, 0}, MorseText::WORDS, MorsePreferences::headOptions, true,
                &MorseHeadCopying::menuExec, "a"}, //
        {"Call Signs", _headCalls, {1, _headWords, _headMixed, _head, 0}, MorseText::CALLS, MorsePreferences::headOptions, true,
                &MorseHeadCopying::menuExec, "a"}, //
        {"Mixed", _headMixed, {1, _headCalls, _headPlayer, _head, 0}, MorseText::MIXED, MorsePreferences::headOptions, true,
                &MorseHeadCopying::menuExec, "a"}, //
        {"File Player", _headPlayer, {1, _headMixed, _headRand, _head, 0}, MorseText::PLAYER, MorsePreferences::headOptions, true,
                &MorseHeadCopying::menuExec, "player"}, //

        {"Transceiver", _trx, {0, _head, _decode, _dummy, _trxLora}, MorseText::NA, MorsePreferences::noOptions, true,
                &internal::nothing, ""}, //
        {"LoRa Trx", _trxLora, {1, _trxIcw, _trxIcw, _trx, 0}, MorseText::NA, MorsePreferences::loraTrxOptions, true,
                &MorseLoRa::menuExec, "trx"}, //
        {"iCW/Ext Trx", _trxIcw, {1, _trxLora, _trxLora, _trx, 0}, MorseText::NA, MorsePreferences::extTrxOptions, true,
                &MorseKeyer::menuExec, "trx"}, //

        {"CW Decoder", _decode, {0, _trx, _wifi, _dummy, 0}, MorseText::NA, MorsePreferences::decoderOptions, true, &Decoder::menuExec,
                "a"}, //

        {"WiFi Functions", _wifi, {0, _decode, _goToSleep, _dummy, _wifi_mac}, MorseText::NA, MorsePreferences::noOptions, false,
                &internal::nothing, ""}, //
        {"Disp MAC Addr", _wifi_mac, {1, _wifi_update, _wifi_config, _wifi, 0}, MorseText::NA, MorsePreferences::noOptions, false,
                &MorseWifi::menuExec, "mac"}, //
        {"Config WiFi", _wifi_config, {1, _wifi_mac, _wifi_check, _wifi, 0}, MorseText::NA, MorsePreferences::noOptions, false,
                &MorseWifi::menuExec, "startAp"}, //
        {"Check WiFi", _wifi_check, {1, _wifi_config, _wifi_upload, _wifi, 0}, MorseText::NA, MorsePreferences::noOptions, false,
                &MorseWifi::menuExec, "check"}, //
        {"Upload File", _wifi_upload, {1, _wifi_check, _wifi_update, _wifi, 0}, MorseText::NA, MorsePreferences::noOptions, false,
                &MorseWifi::menuExec, "upload"}, //
        {"Update Firmw", _wifi_update, {1, _wifi_upload, _wifi_mac, _wifi, 0}, MorseText::NA, MorsePreferences::noOptions, false,
                &MorseWifi::menuExec, "update"}, //

        {"Go To Sleep", _goToSleep, {0, _wifi, _keyer, _dummy, 0}, MorseText::NA, MorsePreferences::noOptions, false,
                &MorseSystem::menuExec, "sleep"}

};

boolean quickStart;                                     // should we execute menu item immediately?

void MorseMenu::setup()
{
    /// set up quickstart - this should only be done once at startup - after successful quickstart we disable it to allow normal menu operation
    quickStart = MorsePreferences::prefs.quickStart;
}


boolean MorseMenu::isCurrentMenuItem(menuNo test) {
    return MorsePreferences::prefs.menuPtr == test;
}


void MorseMenu::menu_()
{
    uint8_t newMenuPtr = MorsePreferences::prefs.menuPtr;
    uint8_t disp = 0;
    int t, command;

    //// initialize a few things now
    //Serial.println("THE MENU");
    ///updateTimings(); // now done after reading preferences
    MorseLoRa::idle();
    //keyerState = IDLE_STATE;
    MorseEchoTrainer::active = false;
    //startFirst = true;
    MorseMenu::cleanStartSettings();
    /*
     clearText = "";
     CWword = "";
     echoTrainerState = START_ECHO;
     generatorState = KEY_UP;
     keyerState = IDLE_STATE;
     interWordTimer = 4294967000;                 // almost the biggest possible unsigned long number :-) - do not output a space at the beginning
     genTimer = millis()-1;                       // we will be at end of KEY_DOWN when called the first time, so we can fetch a new word etc...
     */
    MorseDisplay::clearScroll();                  // clear the buffer
    MorseDisplay::clearScrollBuffer();

    MorseGenerator::keyOut(false, true, 0, 0);
    MorseGenerator::keyOut(false, false, 0, 0);
    MorseMachine::encoderState = MorseMachine::speedSettingMode;      // always start with this encoderstate (decoder will change it anyway)
    MorsePreferences::currentOptions = MorsePreferences::allOptions; // this is the array of options when we double click the BLACK button: while in menu, you can change all of them

    MorsePreferences::writeWordPointer();

    MorseDisplay::clear();

    while (true)
    {                          // we wait for a click (= selection)
        if (disp != newMenuPtr)
        {
            disp = newMenuPtr;
            internal::menuDisplay(disp);
        }
        if (quickStart)
        {
            quickStart = false;
            command = 1;
            delay(500);
            MorseDisplay::printOnScrollFlash(2, REGULAR, 1, "QUICK START");
        }
        else
        {
            MorseUI::modeButton.Update();
            command = MorseUI::modeButton.clicks;
        }

        switch (command)
        {                                          // actions based on enocder button
            case 2:
                if (MorsePreferencesMenu::setupPreferences(MorsePreferences::prefs.menuPtr)) // all available options when called from top menu
                    newMenuPtr = MorsePreferences::prefs.menuPtr;
                internal::menuDisplay(newMenuPtr);
                break;
            case 1: // check if we have a submenu or if we execute the selection
                    //Serial.println("newMP: " + String(newMenuPtr) + " navi: " + String(menuNav[newMenuPtr][naviDown]));
                if (menuItems[newMenuPtr].nav[naviDown] == 0)
                {
                    MorsePreferences::prefs.menuPtr = newMenuPtr;
                    disp = 0;
                    if (menuItems[newMenuPtr].remember)
                    {            // remember last executed, unless it is a wifi function or shutdown
                        MorsePreferences::writeLastExecuted(newMenuPtr);
                    }
                    if (internal::menuExec())
                        return;
                }
                else
                {
                    newMenuPtr = menuItems[newMenuPtr].nav[naviDown];
                }
                break;
            case -1:  // we need to go one level up, if possible
                if (menuItems[newMenuPtr].nav[naviUp] != 0)
                    newMenuPtr = menuItems[newMenuPtr].nav[naviUp];
                break;
            default:
                break;
        }

        if ((t = MorseRotaryEncoder::checkEncoder()))
        {
            //pwmClick(MorsePreferences::prefs.sidetoneVolume);         /// click
            newMenuPtr = menuItems[newMenuPtr].nav[(t == -1) ? naviLeft : naviRight];
        }

        MorseUI::volButton.Update();

        switch (MorseUI::volButton.clicks)
        {
            case -1:
                MorseUI::audioLevelAdjust(); /// for adjusting line-in audio level (at the same time keying tx and sending oudio on line-out
                MorseDisplay::clear();
                internal::menuDisplay(disp);
                break;
                /* case  3:  wifiFunction();                                  /// configure wifi, upload file or firmware update
                 break;
                 */
        }
        MorseSystem::checkShutDown(false);                  // check for time out
    } // end while - we leave as soon as the button has been pressed
} // end menu_()

void internal::menuDisplay(uint8_t ptr)
{
    //Serial.println("Level: " + (String) menuItems[ptr].nav[naviLevel] + " " + menuItems[ptr].text);
    uint8_t oneUp = menuItems[ptr].nav[naviUp];
    uint8_t twoUp = menuItems[oneUp].nav[naviUp];
    uint8_t oneDown = menuItems[ptr].nav[naviDown];

    MorseDisplay::printOnStatusLine(true, 0, "Select Modus:     ");

    //MorseDisplay::clearLine(0); MorseDisplay::clearLine(1); MorseDisplay::clearLine(2);                       // delete previous content
    MorseDisplay::clearScroll();

    /// level 0: top line, possibly ".." on line 1
    /// level 1: higher level on 0, item on 1, possibly ".." on 2
    /// level 2: higher level on 1, highest level on 0, item on 2
    switch (menuItems[ptr].nav[naviLevel])
    {
        case 2:
        {
            MorseDisplay::printOnScroll(2, BOLD, 0, menuItems[ptr].text);
            MorseDisplay::printOnScroll(1, REGULAR, 0, menuItems[oneUp].text);
            MorseDisplay::printOnScroll(0, REGULAR, 0, menuItems[twoUp].text);
            break;
        }
        case 1:
        {
            if (oneDown)
            {
                MorseDisplay::printOnScroll(2, REGULAR, 0, String(".."));
            }
            MorseDisplay::printOnScroll(1, BOLD, 0, menuItems[ptr].text);
            MorseDisplay::printOnScroll(0, REGULAR, 0, menuItems[oneUp].text);
        }
            break;
        case 0:
        {
            if (oneDown)
            {
                MorseDisplay::printOnScroll(1, REGULAR, 0, String(".."));
            }
            MorseDisplay::printOnScroll(0, BOLD, 0, menuItems[ptr].text);
        }
            break;
    }
}

///////////// GEN_TYPE { RANDOMS, ABBREVS, WORDS, CALLS, MIXED, KOCH_MIXED, KOCH_LEARN };           // the things we can generate in generator mode

boolean internal::menuExec()
{                                          // return true if we should  leave menu after execution, true if we should stay in menu
    //Serial.println("Executing menu item " + String(MorsePreferences::prefs.menuPtr));

    MorseDisplay::getConfig()->autoFlush = true;

    Koch::setKochActive(false);
    MorseText::start(menuItems[MorsePreferences::prefs.menuPtr].generatorMode);
    MorsePreferences::currentOptions = menuItems[MorsePreferences::prefs.menuPtr].options;

    if (menuItems[MorsePreferences::prefs.menuPtr].menufx != 0)
    {
        boolean (*fx)(String) = menuItems[MorsePreferences::prefs.menuPtr].menufx;
        String fxParam = menuItems[MorsePreferences::prefs.menuPtr].menufxParam;
        return fx(fxParam);
    }

    return false;
}   /// end menuExec()

void MorseMenu::cleanStartSettings()
{
//    MorseEchoTrainer::setState(MorseEchoTrainer::START_ECHO);
    MorseGenerator::generatorState = MorseGenerator::KEY_UP;
    MorseKeyer::keyerState = MorseKeyer::IDLE_STATE;
    Decoder::interWordTimer = 4294967000;   // almost the biggest possible unsigned long number :-) - do not output a space at the beginning
//    MorseGenerator::setStart();
    MorseDisplay::displayTopLine();
}

boolean internal::nothing(String mode)
{
    Serial.println("This should not be called: " + mode);
    return false;
}
