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

#include "MorseGenerator.h"
#include "MorseDisplay.h"
#include "MorseMachine.h"
#include "MorsePreferences.h"
#include "MorseLoRa.h"
#include "MorseKeyer.h"
#include "MorseSound.h"
#include "decoder.h"
#include "MorsePlayerFile.h"
#include "MorseText.h"
#include "MorseMenu.h"
#include "MorseModeEchoTrainer.h"
#include "MorseModeHeadCopying.h"

using namespace MorseGenerator;

// we use substrings as char pool for trainer mode
// SANK will be replaced by <as>, <ka>, <kn> and <sk>, H = ch
// a = CWchars.substring(0,26); 9 = CWchars.substring(26,36); ? = CWchars.substring(36,45); <> = CWchars.substring(44,49);
// a9 = CWchars.substring(0,36); 9? = CWchars.substring(26,45); ?<> = CWchars.substring(36,50);
// a9? = CWchars.substring(0,45); 9?<> = CWchars.substring(26,50);
// a9?<> = CWchars;

//byte NoE = 0;             // Number of Elements
// byte nextElement[8];      // the list of elements; 0 = dit, 1 = dah

// for each character:
// byte length// byte morse encoding as binary value, beginning with most significant bit


const char* pppool[] = {
        "12",  // a    0
        "2111",  // b
        "2121",  // c
        "211",  // d
        "1",  // e
        "1121",  // f
        "221",  // g
        "1111",  // h
        "11",  // i
        "1222",  // j
        "212",  // k
        "1211",  // l
        "22",  // m
        "21",  // n
        "222",  // o
        "1221",  // p
        "2212",  // q
        "121",  // r
        "111",  // s
        "2",  // t
        "112",  // u
        "1112",  // v
        "122",  // w
        "2112",  // x
        "2122",  // y
        "2211",  // z  25
        // numbers
        "22222",  // 0  26
        "12222",  // 1
        "11222",  // 2
        "11122",  // 3
        "11112",  // 4
        "11111",  // 5
        "21111",  // 6
        "22111",  // 7
        "22211",  // 8
        "22221",  // 9  35
        // interpunct
        "121212",  // .  36
        "221122",  // ,  37
        "222111",  // :  38
        "211112",  // -  39
        "21121",  // /  40
        "21112",  // =  41
        "112211",  // ?  42
        "122121",  // @  43
        "12121",  // +  44    (at the same time <ar> !)
        // Pro signs
        "12111",  // <as> 45 S
        "21212",  // <ka> 46 A
        "21221",  // <kn> 47 N
        "111212",   // <sk> 48    K
        "11121",  // <ve> 49 E
        // German characters
        "1212",  // ä    50
        "2221",  // ö    51
        "1122",  // ü    52
        "2222",   // ch   53  H
        "111222111" // <SOS> 54 X
};


unsigned char MorseGenerator::generatorState; // should be MORSE_TYPE instead of uns char
unsigned long MorseGenerator::genTimer;                         // timer used for generating morse code in trainer mode

String MorseGenerator::CWword = "";
String MorseGenerator::clearText = "";
uint8_t MorseGenerator::wordCounter = 0;                          // for maxSequence

int rxDitLength = 0;                    // set new value for length of dits and dahs and other timings
int rxDahLength = 0;
int rxInterCharacterSpace = 0;
int rxInterWordSpace = 0;

boolean MorseGenerator::stopFlag = false;                         // for maxSequence

namespace MorseGenerator
{
    boolean active;
}

MorseGenerator::Config generatorConfig;

namespace internal
{
    String fetchNewWord();
    void dispGeneratedChar();

    void setStart2();

    String textToCWword(String symbols);

    unsigned long getCharTiming(MorseGenerator::Config *generatorConfig, char c);
    unsigned long getIntercharSpace(MorseGenerator::Config *generatorConfig);
    unsigned long getInterwordSpace(MorseGenerator::Config *generatorConfig);
    unsigned long getInterelementSpace(MorseGenerator::Config *generatorConfig);

}

void MorseGenerator::setup()
{
    MorseKeyer::setup();
}


Config* MorseGenerator::getConfig()
{
    return &generatorConfig;
}

void MorseGenerator::setStart()
{
    Serial.println("MG:sS() 1");
    generatorConfig.key = true;
    generatorConfig.printDitDah = false;
    generatorConfig.wordEndMethod = spaceAndFlush;
//    generatorConfig.printSpaceAfterWord = true;
    generatorConfig.printSpaceAfterChar = false;
    generatorConfig.timing = Timing::tx;
    generatorConfig.clearBufferBeforPrintChar = false;
    generatorConfig.printCharStyle = REGULAR;
    generatorConfig.printChar = true;
    generatorConfig.onFetchNewWord = &voidFunction;
    generatorConfig.onGeneratorWordEnd = &uLongFunctionMinus1;
    generatorConfig.onLastWord = &voidFunction;
//    generatorConfig.effectiveTrainerDisplay = MorsePreferences::prefs.trainerDisplay;

    MorseGenerator::handleEffectiveTrainerDisplay(MorsePreferences::prefs.trainerDisplay);

    internal::setStart2();
}

void internal::setStart2()
{
    CWword = "";
    clearText = "";
    genTimer = millis() - 1;  // we will be at end of KEY_DOWN when called the first time, so we can fetch a new word etc...
    wordCounter = 0;                             // reset word counter for maxSequence
}

void MorseGenerator::startTrainer()
{
    MorseGenerator::setStart();
    MorseMachine::morseState = MorseMachine::morseGenerator;
    MorseGenerator::setup();
    MorseDisplay::clear();
    MorseDisplay::printOnScroll(0, REGULAR, 0, "Generator     ");
    MorseDisplay::printOnScroll(1, REGULAR, 0, "Start/Stop:   ");
    MorseDisplay::printOnScroll(2, REGULAR, 0, "Paddle | BLACK");
    delay(1250);
    MorseDisplay::clear();
    MorseDisplay::displayTopLine();
    MorseDisplay::clearScroll();      // clear the buffer
}


void MorseGenerator::handleEffectiveTrainerDisplay(uint8_t mode)
{
    Serial.println("MorseGen::i::oPC 1");
    MorseGenerator::Config *generatorConfig = MorseGenerator::getConfig();
    switch (mode)
    {
        case DISPLAY_BY_CHAR:
        {
            Serial.println("MorseGen::oPC 1 c");
            generatorConfig->printChar = true;
            generatorConfig->wordEndMethod = MorseGenerator::space;
            MorseDisplay::getConfig()->autoFlush = true;
            break;
        }
        case DISPLAY_BY_WORD:
        {
            Serial.println("MorseGen::oPC 1 w");
            generatorConfig->printChar = true;
            generatorConfig->wordEndMethod = MorseGenerator::spaceAndFlush;
            MorseDisplay::getConfig()->autoFlush = false;
            break;
        }
        case NO_DISPLAY:
        {
            Serial.println("MorseGen::oPC 1 n");
            generatorConfig->printChar = false;
            generatorConfig->wordEndMethod = MorseGenerator::space;
            MorseDisplay::getConfig()->autoFlush = false;
            break;
        }
    }
}





void MorseGenerator::generateCW()
{          // this is called from loop() (frequently!)  and generates CW

    if (millis() < genTimer)
    {
        // if not at end of key up or down we need to wait, so we just return to loop()
        return;
    }

//    Serial.println("genCW() 1 cw: " + CWword + " t: " + clearText + " gs: " + String(generatorState));

    switch (generatorState)
    {                                             // CW generator state machine - key is up or down
        case KEY_UP:
        {
            // here we continue if the pause has been long enough

            if (CWword.length() == 0)
            {                                               // fetch a new word if we have an empty word
                Serial.println("genCW() 2 max: " + String(MorsePreferences::prefs.maxSequence) + " wc: " + String(wordCounter));

                String newWord = "";

                uint8_t max = MorsePreferences::prefs.maxSequence;
                if (max && MorseGenerator::wordCounter == (max - 1))
                {
                    // last word;
                    MorseText::setNextWordIsEndSequence();
                    generatorConfig.onLastWord();
                    Serial.println("penultimate word");
                }
                else if (max && MorseGenerator::wordCounter >= max)
                {
                    // stop
                    MorseGenerator::stopFlag = true;
                    MorseGenerator::wordCounter = 0;
//                    MorseEchoTrainer::echoStop = false;
                    Serial.println("stop");
                }

                if (!MorseGenerator::stopFlag)
                {
                    newWord = internal::fetchNewWord();
//                    newWord = "a" + String(wordCounter);
                    Serial.println("new word " + newWord);

                    MorseGenerator::clearText = newWord;
                    MorseGenerator::CWword = internal::textToCWword(newWord);
                    Serial.println("genCW() fetch cw: " + CWword + " t: " + clearText + " wc: " + String(wordCounter));
                }

                if (CWword.length() == 0)
                {
                    Serial.println("MorseGenerator::genrateCW() return because cwword empty");
                    // we really should have something here - unless in trx mode; in this case return
                    return;
                }

                MorseGenerator::wordCounter += 1;

//                if (generatorConfig.printSpaceAfterWord)
//                {
//                    MorseDisplay::printToScroll(REGULAR, " ");    /// in any case, add a blank after the word on the display
//                }

                switch (generatorConfig.wordEndMethod)
                {
                    case LF:
                    {
                        Serial.println("Generator: wordendmeth LF");
                        MorseDisplay::printToScroll(REGULAR, "\n");
                        break;
                    }
                    case spaceAndFlush:
                    {
                        Serial.println("Generator: wordendmeth flush");
                        MorseDisplay::printToScroll(REGULAR, " ");    /// in any case, add a blank after the word on the display
                        MorseDisplay::flushScroll();
                        break;
                    }
                    case space:
                    {
                        MorseDisplay::printToScroll(REGULAR, " ");    /// in any case, add a blank after the word on the display
                        Serial.println("Generator: wordendmeth shrug");
                        break;
                    }
                    case nothing:
                    {
                        Serial.println("Generator: wordendmeth nothing");
                        break;
                    }
                }
            }

            // retrieve next element from CWword; if 0, we were at end of character
            char c = CWword[0];
            CWword.remove(0, 1);
//            Serial.println("genCW() 3 cw: " + CWword + " t: " + clearText + " gs: " + String(generatorState));

            if ((c == '0'))
            {                      // a character just had been finished
                if (generatorConfig.sendCWToLoRa)
                {
                    MorseLoRa::cwForLora(0);
                }
            }
            else
            {
                genTimer = millis() + internal::getCharTiming(&generatorConfig, c);

                if (generatorConfig.sendCWToLoRa)
                {
                    // send the element to LoRa
                    c == '1' ? MorseLoRa::cwForLora(1) : MorseLoRa::cwForLora(2);
                }

                /// if Koch learn character we show dit or dah
                if (generatorConfig.printDitDah)
                {
                    MorseDisplay::printToScroll(REGULAR, c == '1' ? "." : "-");
                }

                if (generatorConfig.key && !stopFlag) // we finished maxSequence and so do start output (otherwise we get a short click)
                {
                    keyOut(true, (!MorseMachine::isMode(MorseMachine::loraTrx)), MorseSound::notes[MorsePreferences::prefs.sidetoneFreq],
                            MorsePreferences::prefs.sidetoneVolume);
                }
                generatorState = KEY_DOWN;                              // next state = key down = dit or dah
            }
            break;
        }
        case KEY_DOWN:
        {
            //// otherwise we continue here; stop keying,  and determine the length of the following pause: inter Element, interCharacter or InterWord?

            if (generatorConfig.key)
            {
//                Serial.println("Generator: KEY_DOWN keyOut false");
                keyOut(false, (!MorseMachine::isMode(MorseMachine::loraTrx)), 0, 0);
            }

//            Serial.println("Generator: KEY_DOWN CWword: " + CWword);

            if (CWword.length() == 1)
            {
                Serial.println("Generator: KEY_DOWN Word end");
                // we just ended the the word

//                MorseHeadCopying::onGeneratorWordEnd();

                Serial.println("Generator: KEY_DOWN Word end print char?");
                internal::dispGeneratedChar();

                Serial.println("Generator: KEY_DOWN Word end call MET::onGenWordEnd");

                unsigned long delta = generatorConfig.onGeneratorWordEnd();

                if (delta != -1)
                {
                    genTimer = millis() + delta;
                }
                else
                {
                    genTimer = millis() + internal::getInterwordSpace(&generatorConfig);
                    if (MorseMachine::isMode(MorseMachine::morseGenerator) && MorsePreferences::prefs.loraTrainerMode == 1)
                    {                                   // in generator mode and we want to send with LoRa
                        MorseLoRa::cwForLora(0);
                        MorseLoRa::cwForLora(3);                           // as we have just finished a word
                        //Serial.println("cwForLora(3);");
                        MorseLoRa::sendWithLora();                         // finalise the string and send it to LoRA
                        delay(MorseKeyer::interCharacterSpace + MorseKeyer::ditLength); // we need a slightly longer pause otherwise the receiving end might fall too far behind...
                    }
                }
            }
            else if (CWword[0] == '0')
            {
                // we are at end of character
                internal::dispGeneratedChar();
                genTimer = millis() + internal::getIntercharSpace(&generatorConfig);
            }
            else
            {                                                                                     // we are in the middle of a character
                genTimer = millis() + internal::getInterelementSpace(&generatorConfig);
            }
            generatorState = KEY_UP;                               // next state = key up = pause
            break;
        }
    }   /// end switch (generatorState)
}

unsigned long internal::getCharTiming(MorseGenerator::Config *generatorConfig, char c)
{
    long delta;
    switch (generatorConfig->timing)
    {
        case quick:
        {
            delta = 2;      // very short timing
            break;
        }
        case tx:
        {
            delta = (c == '1' ? MorseKeyer::ditLength : MorseKeyer::dahLength);
            break;
        }
        case rx:
        {
            delta = (c == '1' ? rxDitLength : rxDahLength);
            break;
        }
        default:
        {
            delta = 0;
            Serial.println("This should not be reached (getCharTiming)");
        }
    }
    return delta;
}

unsigned long internal::getIntercharSpace(MorseGenerator::Config *generatorConfig)
{
    long delta;
    switch (generatorConfig->timing)
    {
        case quick:
        {
            delta = 1;      // very short timing
            break;
        }
        case tx:
        {
            delta = MorseKeyer::interCharacterSpace;
            break;
        }
        case rx:
        {
            delta = rxInterCharacterSpace;
            break;
        }
        default:
        {
            delta = 0;
            Serial.println("This should not be reached (getCharTiming)");
        }
    }
    return delta;
}

unsigned long internal::getInterwordSpace(MorseGenerator::Config *generatorConfig)
{
    long delta;
    switch (generatorConfig->timing)
    {
        case quick:
        {
            delta = 2;      // very short timing
            break;
        }
        case tx:
        {
            delta = MorseKeyer::interWordSpace;
            break;
        }
        case rx:
        {
            delta = rxInterWordSpace;
            break;
        }
        default:
        {
            delta = 0;
            Serial.println("This should not be reached (getCharTiming)");
        }
    }
    return delta;
}

unsigned long internal::getInterelementSpace(MorseGenerator::Config *generatorConfig)
{
    long delta;
    switch (generatorConfig->timing)
    {
        case quick:
        {
            delta = 2;      // very short timing
            break;
        }
        case tx:
        {
            delta = MorseKeyer::ditLength;
            break;
        }
        case rx:
        {
            delta = rxDitLength;
            break;
        }
        default:
        {
            delta = 0;
            Serial.println("This should not be reached (getCharTiming)");
        }
    }
    return delta;
}

String fetchNewWordFromLoRa()
{
    MorseLoRa::Packet packet = MorseLoRa::decodePacket();
    Serial.println(packet.toString());
    if (!packet.valid) {
        return "";
    }

    //Serial.println("clearText: " + (String) clearText);
    //Serial.println("RX Speed: " + (String)rxWpm);
    //Serial.println("RSSI: " + (String)rssi);
    rxDitLength = 1200 / packet.rxWpm; // set new value for length of dits and dahs and other timings
    rxDahLength = 3 * rxDitLength; // calculate the other timing values
    rxInterCharacterSpace = 3 * rxDitLength;
    rxInterWordSpace = 7 * rxDitLength;
    MorseDisplay::vprintOnStatusLine(true, 4, "%2ir", packet.rxWpm);
    MorseDisplay::printOnStatusLine(true, 9, "s");
    MorseDisplay::updateSMeter(packet.rssi); // indicate signal strength of new packet

    String word = Decoder::CWwordToClearText(packet.payload);
    return word;
}

/**
 * we check the rxBuffer and see if we received something
 */
String fetchNewWord_LoRa()
{
    // we check the rxBuffer and see if we received something
    MorseDisplay::updateSMeter(0); // at end of word we set S-meter to 0 until we receive something again
    //Serial.print("end of word - S0? ");
    ////// from here: retrieve next CWword from buffer!
    String word;
    if (MorseLoRa::loRaBuReady())
    {
        word = fetchNewWordFromLoRa();
    }
    else
    {
        // we did not receive anything
        word = "";
    }
    return word;
}

String internal::fetchNewWord()
{
    String result = "";
//Serial.println("startFirst: " + String((startFirst ? "true" : "false")));
    if (MorseMachine::isMode(MorseMachine::loraTrx))
    {
        Serial.println("fetchNewWord mode: lora");
        result = fetchNewWord_LoRa();
    } // end if loraTrx
    else
    {
        Serial.println("fetchNewWord mode: !lora");
        generatorConfig.onFetchNewWord();

        // TODO: move into KochLearn.onFetchNewWord()
        if (MorseMenu::isCurrentMenuItem(MorseMenu::_kochLearn))
        {
            morseModeEchoTrainer.setState(MorseModeEchoTrainer::SEND_WORD);
        }

        result = MorseText::generateWord();
        Serial.println("fetchNewWord new: " + result);
    }
    return result;
}

void MorseGenerator::keyOut(boolean on, boolean fromHere, int f, int volume)
{
    //// generate a side-tone with frequency f when on==true, or turn it off
    //// differentiate external (decoder, sometimes cw_generate) and internal (keyer, sometimes Cw-generate) side tones
    //// key transmitter (and line-out audio if we are in a suitable mode)

    static boolean intTone = false;
    static boolean extTone = false;

    static int intPitch, extPitch;

// Serial.println("keyOut: " + String(on) + String(fromHere));
    if (on)
    {
        if (fromHere)
        {
            intPitch = f;
            intTone = true;
            MorseSound::pwmTone(intPitch, volume, true);
            MorseKeyer::keyTransmitter();
        }
        else
        {                    // not from here
            extTone = true;
            extPitch = f;
            if (!intTone)
            {
                MorseSound::pwmTone(extPitch, volume, false);
            }
        }
    }
    else
    {                      // key off
        if (fromHere)
        {
            intTone = false;
            if (extTone)
            {
                MorseSound::pwmTone(extPitch, volume, false);
            }
            else
            {
                MorseSound::pwmNoTone();
            }
            MorseKeyer::unkeyTransmitter();
        }
        else
        {                 // not from here
            extTone = false;
            if (!intTone)
            {
                MorseSound::pwmNoTone();
            }
        }
    }   // end key off
}

/////// generate CW representations from its input string

String internal::textToCWword(String symbols)
{
    int pointer;
    String result = "";

    int l = symbols.length();

    for (int i = 0; i < l; ++i)
    {
        if (i != 0) {
            result += "0";
        }
        char c = symbols.charAt(i);                                 // next char in string
        Serial.println("internal::textToCWword c " + String(c));
        pointer = MorseText::findChar(c);                                 // at which position is the character in CWchars?
        Serial.println("internal::textToCWword p " + String(pointer));
        if (pointer == -1)
        {
            result = "11111111"; // <err>
            break;
        }
        result += pppool[pointer];
    }
    result += "0";
    Serial.println("internal::textToCWword(): " + result);
    return result;
}

/// when generating CW, we display the character (under certain circumstances)
/// add code to display in echo mode when parameter is so set
/// MorsePreferences::prefs.echoDisplay 1 = CODE_ONLY 2 = DISP_ONLY 3 = CODE_AND_DISP

void internal::dispGeneratedChar()
{
    String charString = String(clearText.charAt(0));
    clearText.remove(0, 1);

//    charString.reserve(10);

    if (generatorConfig.printChar)
    {       /// we need to output the character on the display now
        Serial.println("Generator: dispGenChar " + charString);
        if (generatorConfig.clearBufferBeforPrintChar)
        {
            MorseDisplay::printToScroll(REGULAR, "");                      // clear the buffer first
        }
        MorseDisplay::printToScroll(generatorConfig.printCharStyle, MorseText::cleanUpProSigns(charString));
        if (generatorConfig.printSpaceAfterChar)
        {
            MorseDisplay::printToScroll(REGULAR, " ");                      // output a space
        }
    }
    else
    {
        Serial.println("Generator: dispGenChar no printChar - would have been " + charString);
    }

    MorsePreferences::fireCharSeen(true);
}

