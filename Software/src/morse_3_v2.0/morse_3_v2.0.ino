
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

 /*****************************************************************************************************************************
 *  code by others used in this sketch, apart from the ESP32 libraries distributed by Heltec
 *  (see: https://github.com/Heltec-Aaron-Lee/WiFi_Kit_series)
 *
 *  ClickButton library -> https://code.google.com/p/clickbutton/ by Ragnar Aronsen
 * 
 * 
 *  For volume control of NF output: I used a similar principle as  Connor Nishijima, see 
 *                                   https://hackaday.io/project/11957-10-bit-component-less-volume-control-for-arduino
 *                                   but actually using two PWM outputs, connected with an AND gate
 *  Routines for morse decoder - to a certain degree based on code by Hjalmar Skovholm Hansen OZ1JHM
 *                                   - see http://skovholm.com/cwdecoder
 ****************************************************************************************************************************/

///// include of the various libraries and include files being used

#include <Wire.h>          // Only needed for Arduino 1.6.5 and earlier
#include "ClickButton.h"   // button control library
#include <SPI.h>           // library for SPI interface
#include <LoRa.h>          // library for LoRa transceiver
#include <WiFi.h>          // basic WiFi functionality
#include <WebServer.h>     // simple web sever
#include <ESPmDNS.h>       // DNS functionality
#include <WiFiClient.h>    //WiFi clinet library
#include <Update.h>        // update "over the air" (OTA) functionality
#include "FS.h"
#include "SPIFFS.h"

#include "morsedefs.h"
#include "MorseDisplay.h"
#include "wklfonts.h"      // monospaced fonts in size 12 (regular and bold) for smaller text and 15 for larger text (regular and bold), called :
                           // DialogInput_plain_12, DialogInput_bold_12 & DialogInput_plain_15, DialogInput_bold_15
                           // these fonts were created with this tool: http://oleddisplay.squix.ch/#/home
#include "abbrev.h"        // common CW abbreviations
#include "english_words.h" // common English words
#include "MorsePreferences.h"
#include "MorseRotaryEncoder.h"
#include "MorseSound.h"
#include "koch.h"
#include "MorseLoRa.h"
#include "MorseSystem.h"
#include "MorseMachine.h"
#include "MorseUI.h"
#include "MorseGenerator.h"
#include "MorsePlayerFile.h"
#include "MorseKeyer.h"
#include "decoder.h"


/// we need this for some strange reason: the min definition breaks with WiFi
#define _min(a,b) ((a)<(b)?(a):(b))
#define _max(a,b) ((a)>(b)?(a):(b))






// positions: [3] 1 0 2 [3] 1 0 2 [3]
// [3] is the positions where my rotary switch detends
// ==> right, count up
// <== left,  count down




//////// variables and constants for the modus menu


enum navi {naviLevel, naviLeft, naviRight, naviUp, naviDown };

enum menuNo { _dummy, _keyer,
              _gen, _genRand, _genAbb, _genWords, _genCalls, _genMixed, _genPlayer,
              _echo, _echoRand, _echoAbb, _echoWords, _echoCalls, _echoMixed, _echoPlayer,
              _koch, _kochSel, _kochLearn, _kochGen, _kochGenRand, _kochGenAbb, _kochGenWords,
              _kochGenMixed, _kochEcho, _kochEchoRand, _kochEchoAbb, _kochEchoWords, _kochEchoMixed,
              _head, _headRand, _headAbb, _headWords, _headCalls, _headMixed, _headPlayer,
              _trx, _trxLora, _trxIcw, _decode, _wifi, _wifi_mac, _wifi_config, _wifi_check, _wifi_upload, _wifi_update, _goToSleep };


typedef struct MenuItem {
  String text;
  menuNo no;
  uint8_t nav[5];
  MorseGenerator::GEN_TYPE generatorMode;
  boolean remember;
} menuItem_t;


const menuItem_t menuItems [] = {
  {"",_dummy, { 0,0,0,0,0}, MorseGenerator::NA, true},
  {"CW Keyer",_keyer, {0,_goToSleep,_gen,_dummy,0}, MorseGenerator::NA, true},
  
  {"CW Generator",_gen, {0,_keyer,_echo,_dummy,_genRand}, MorseGenerator::NA, true},
  {"Random",_genRand, {1,_genPlayer,_genAbb,_gen,0}, MorseGenerator::RANDOMS, true},
  {"CW Abbrevs",_genAbb, {1,_genRand,_genWords,_gen,0}, MorseGenerator::ABBREVS, true},
  {"English Words",_genWords, {1,_genAbb,_genCalls,_gen,0}, MorseGenerator::WORDS, true},
  {"Call Signs",_genCalls, {1,_genWords,_genMixed,_gen,0}, MorseGenerator::CALLS, true},
  {"Mixed",_genMixed, {1,_genCalls,_genPlayer,_gen,0}, MorseGenerator::MIXED, true},
  {"File Player",_genPlayer, {1,_genMixed,_genRand,_gen,0}, MorseGenerator::PLAYER, true},

  {"Echo Trainer",_echo, {0,_gen,_koch,_dummy,_echoRand}, MorseGenerator::NA, true},
  {"Random",_echoRand, {1,_echoPlayer,_echoAbb,_echo,0}, MorseGenerator::RANDOMS, true},
  {"CW Abbrevs",_echoAbb, {1,_echoRand,_echoWords,_echo,0}, MorseGenerator::ABBREVS, true},
  {"English Words",_echoWords, {1,_echoAbb,_echoCalls,_echo,0}, MorseGenerator::WORDS, true},
  {"Call Signs",_echoCalls, {1,_echoWords,_echoMixed,_echo,0}, MorseGenerator::CALLS, true},
  {"Mixed",_echoMixed, {1,_echoCalls,_echoPlayer,_echo,0}, MorseGenerator::MIXED, true},
  {"File Player",_echoPlayer, {1,_echoMixed,_echoRand,_echo,0}, MorseGenerator::PLAYER, true},

  {"Koch Trainer",_koch,  {0,_echo,_head,_dummy,_kochSel}, MorseGenerator::NA, true},
  {"Select Lesson",_kochSel, {1,_kochEcho,_kochLearn,_koch,0}, MorseGenerator::NA, true},
  {"Learn New Chr",_kochLearn, {1,_kochSel,_kochGen,_koch,0}, MorseGenerator::NA, true},
  {"CW Generator",_kochGen, {1,_kochLearn,_kochEcho,_koch,_kochGenRand}, MorseGenerator::NA, true},
  {"Random",_kochGenRand, {2,_kochGenMixed,_kochGenAbb,_kochGen,0}, MorseGenerator::RANDOMS, true},
  {"CW Abbrevs",_kochGenAbb, {2,_kochGenRand,_kochGenWords,_kochGen,0}, MorseGenerator::ABBREVS, true},
  {"English Words",_kochGenWords, {2,_kochGenAbb,_kochGenMixed,_kochGen,0}, MorseGenerator::WORDS, true},
  {"Mixed",_kochGenMixed, {2,_kochGenWords,_kochGenRand,_kochGen,0}, MorseGenerator::MIXED, true},

  {"Echo Trainer",_kochEcho, {1,_kochGen,_kochSel,_koch,_kochEchoRand}, MorseGenerator::NA, true},
  {"Random",_kochEchoRand, {2,_kochEchoMixed,_kochEchoAbb,_kochEcho,0}, MorseGenerator::RANDOMS, true},
  {"CW Abbrevs",_kochEchoAbb, {2,_kochEchoRand,_kochEchoWords,_kochEcho,0}, MorseGenerator::ABBREVS, true},
  {"English Words",_kochEchoWords, {2,_kochEchoAbb,_kochEchoMixed,_kochEcho,0}, MorseGenerator::WORDS, true},
  {"Mixed",_kochEchoMixed, {2,_kochEchoWords,_kochEchoRand,_kochEcho,0}, MorseGenerator::MIXED, true},

  {"Head Copying",_head, {0,_koch,_trx,_dummy,_headRand}, MorseGenerator::NA, true},
  {"Random",_headRand, {1,_headPlayer,_headAbb,_head,0}, MorseGenerator::RANDOMS, true},
  {"CW Abbrevs",_headAbb, {1,_headRand,_headWords,_head,0}, MorseGenerator::ABBREVS, true},
  {"English Words",_headWords, {1,_headAbb,_headCalls,_head,0}, MorseGenerator::WORDS, true},
  {"Call Signs",_headCalls, {1,_headWords,_headMixed,_head,0}, MorseGenerator::CALLS, true},
  {"Mixed",_headMixed, {1,_headCalls,_headPlayer,_head,0}, MorseGenerator::MIXED, true},
  {"File Player",_headPlayer, {1,_headMixed,_headRand,_head,0}, MorseGenerator::PLAYER, true},

  {"Transceiver",_trx, {0,_head,_decode,_dummy,_trxLora}, MorseGenerator::NA, true},
  {"LoRa Trx",_trxLora, {1,_trxIcw,_trxIcw,_trx,0}, MorseGenerator::NA, true},
  {"iCW/Ext Trx",_trxIcw, {1,_trxLora,_trxLora,_trx,0}, MorseGenerator::NA, true},

  {"CW Decoder",_decode, {0,_trx,_wifi,_dummy,0}, MorseGenerator::NA, true},

  {"WiFi Functions",_wifi, {0,_decode,_goToSleep,_dummy,_wifi_mac}, MorseGenerator::NA, false},
  {"Disp MAC Addr",_wifi_mac, {1,_wifi_update,_wifi_config,_wifi,0}, MorseGenerator::NA, false},
  {"Config WiFi",_wifi_config, {1,_wifi_mac,_wifi_check,_wifi,0}, MorseGenerator::NA, false},
  {"Check WiFi",_wifi_check, {1,_wifi_config,_wifi_upload,_wifi,0}, MorseGenerator::NA, false},
  {"Upload File",_wifi_upload, {1,_wifi_check,_wifi_update,_wifi,0}, MorseGenerator::NA, false},
  {"Update Firmw",_wifi_update, {1,_wifi_upload,_wifi_mac,_wifi,0}, MorseGenerator::NA, false},

  {"Go To Sleep",_goToSleep, {0,_wifi,_keyer,_dummy,0}, MorseGenerator::NA, false}

};


boolean quickStart;                                     // should we execute menu item immediately?

// defines for keyer modi
//

#define    IAMBICA      1          // Curtis Mode A
#define    IAMBICB      2          // Curtis Mode B (with enhanced Curtis timing, set as parameter
#define    ULTIMATIC    3          // Ultimatic mode
#define    NONSQUEEZE   4          // Non-squeeze mode of dual-lever paddles - simulate a single-lever paddle


//// for adjusting preferences





#define SizeOfArray(x)       (sizeof(x) / sizeof(x[0]))

int currentOptionSize;


///////////////////////////////////
//// Other Global VARIABLES ////////////
/////////////////////////////////

unsigned int lUntouched = 0;                        // sensor values (in untouched state) will be stored here
unsigned int rUntouched = 0;





//// not any longer defined in preferences:

  




///////////////////////////////////////////////////////////////////////////////
//
//  Iambic Keyer State Machine Defines
 
enum KSTYPE {IDLE_STATE, DIT, DAH, KEY_START, KEYED, INTER_ELEMENT };





  //CWword.reserve(144);
  //clearText.reserve(50);
boolean active = false;                           // flag for trainer mode






/////////////////// Variables for Koch modes


String kochChars;

////// variables for CW decoder

boolean keyTx = false;             // when state is set by manual key or touch paddle, then true!
                                   // we use this to decide if Tx should be keyed or not





////////////////////////////////////////////////////////////////////
// encoder subroutines
/// interrupt service routine - needs to be positioned BEFORE all other functions, including setup() and loop()
/// interrupt service routine

void IRAM_ATTR isr ()  {                    // Interrupt service routine is executed when a HIGH to LOW transition is detected on CLK
    MorseRotaryEncoder::isr();
}



int IRAM_ATTR checkEncoder() {
    return MorseRotaryEncoder::checkEncoder();
}


////////////////////////   S E T U P /////////////////////////////

void setup()
{
 
  Serial.begin(115200);
  delay(200); // give me time to bring up serial monitor

  // enable Vext
  #if BOARDVERSION == 3
  pinMode(Vext, OUTPUT);
  digitalWrite(Vext,LOW);
  #endif

  
  // set up the encoder - we need external pull-ups as the pins used do not have built-in pull-ups!
  pinMode(PinCLK,INPUT_PULLUP);
  pinMode(PinDT,INPUT_PULLUP);  
  pinMode(keyerPin, OUTPUT);        // we can use the built-in LED to show when the transmitter is being keyed
  pinMode(leftPin, INPUT);          // external keyer left paddle
  pinMode(rightPin, INPUT);         // external keyer right paddle

  /// enable deep sleep
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_0, (esp_sleep_ext1_wakeup_mode_t) 0); //1 = High, 0 = Low
  analogSetAttenuation(ADC_0db);


// we MUST reset the OLED RST pin for 50 ms! for the old board only, but as it does not hurt we do it anyway
//#if BOARDVERSION == 2
  pinMode(OLED_RST,OUTPUT);
  digitalWrite(OLED_RST, LOW);     // set GPIO16 low to reset OLED
  delay(50); 
  digitalWrite(OLED_RST, HIGH);    // while OLED is running, must set GPIO16 in high
//# endif

 // init display
  
  MorseDisplay::init();

  MorseSound::setup();
  
  //call ISR when any high/low changed seen
  //on any of the enoder pins
  attachInterrupt (digitalPinToInterrupt(PinDT), isr, CHANGE);   
  attachInterrupt (digitalPinToInterrupt(PinCLK), isr, CHANGE);
 
MorseRotaryEncoder::setup();

/// set up for encoder button
  pinMode(modeButtonPin, INPUT);
  pinMode(volButtonPin, INPUT_PULLUP);               // external pullup for all GPIOS > 32 with ESP32-LORA
                                                     // wake up also works without external pullup! Interesting!
  
  // Setup button timers (all in milliseconds / ms)
  // (These are default if not set, but changeable for convenience)
  modeButton.debounceTime   = 11;   // Debounce timer in ms
  modeButton.multiclickTime = 220;  // Time limit for multi clicks
  modeButton.longClickTime  = 350; // time until "held-down clicks" register

  volButton.debounceTime   = 11;   // Debounce timer in ms
  volButton.multiclickTime = 220;  // Time limit for multi clicks
  volButton.longClickTime  = 350; // time until "held-down clicks" register



  // to calibrate sensors, we record the values in untouched state
  initSensors();
  
  // read preferences from non-volatile storage
  // if version cannot be read, we have a new ESP32 and need to write the preferences first
  MorsePreferences::readPreferences("morserino");

  Koch::setup();


  MorseLoRa::setup();


  /// set up quickstart - this should only be done once at startup - after successful quickstart we disable it to allow normal menu operation
  quickStart = MorsePreferences::prefs.quickStart;

  
MorsePlayerFile::setup();
  MorseDisplay::displayStartUp();

    ///delay(2500);  //// just to be able to see the startup screen for a while - is contained in displayStartUp()

  ////

  menu_();
} /////////// END setup()











///////////////////////// THE MAIN LOOP - do this OFTEN! /////////////////////////////////

void loop() {
// static uint64_t loopC = 0;
   int t;

   boolean activeOld = active;
   checkPaddles();
   switch (MorseMachine::getMode()) {
      case MorseMachine::morseKeyer:    if (doPaddleIambic(MorseKeyer::leftKey, MorseKeyer::rightKey)) {
                               return;                                                        // we are busy keying and so need a very tight loop !
                          }
                          break;
      case MorseMachine::loraTrx:      if (doPaddleIambic(MorseKeyer::leftKey, MorseKeyer::rightKey)) {
                               return;                                                        // we are busy keying and so need a very tight loop !
                          }
                          MorseGenerator::generateCW();
                          break;
      case MorseMachine::morseTrx:      if (doPaddleIambic(MorseKeyer::leftKey, MorseKeyer::rightKey)) {
                               return;                                                        // we are busy keying and so need a very tight loop !
                          }  
                          Decoder::doDecode();
                          if (Decoder::speedChanged) {
                            Decoder::speedChanged = false;
                            displayCWspeed();
                          }
                          break;    
      case MorseMachine::morseGenerator:  if ((autoStop == stop1) || MorseKeyer::leftKey  || MorseKeyer::rightKey)   {                                    // touching a paddle starts and stops the generation of code
                          // for debouncing:
                          while (checkPaddles() )
                              ;                                                           // wait until paddles are released

                          if (effectiveAutoStop) {
                            active = (autoStop == off);
                            switch (autoStop) {
                              case off : {
                                  break;
                                  //
                                }
                              case stop1: {
                                  autoStop = stop2;
                                  break;
                                }
                              case stop2: {
                                  MorseDisplay::printToScroll(REGULAR, "\n");
                                  autoStop = off;
                                  break;
                                }
                            }
                          }
                          else {
                            active = !active;
                            autoStop = off;
                          }

                          //delay(100);
                          } /// end squeeze
                          
                          ///// check stopFlag triggered by maxSequence
                          if (stopFlag) {
                            active = stopFlag = false;
                          }
                          if (activeOld != active) {
                            if (!active) {
                               MorseGenerator::keyOut(false, true, 0, 0);
                               MorseDisplay::printOnStatusLine(true, 0, "Continue w/ Paddle");
                            }
                          else {
                               //cleanStartSettings();        
                               generatorState = KEY_UP; 
                               genTimer = millis()-1;           // we will be at end of KEY_DOWN when called the first time, so we can fetch a new word etc...          
                            }
                          }
                          if (active)
                            generateCW();
                          break;
      case MorseMachine::echoTrainer:                             ///// check stopFlag triggered by maxSequence
                          if (stopFlag) {
                            active = stopFlag = false;
                            MorseGenerator::keyOut(false, true, 0, 0);
                            MorseDisplay::printOnStatusLine(true, 0, "Continue w/ Paddle");
                          }
                          if (!active && (MorseKeyer::leftKey  || MorseKeyer::rightKey))   {                       // touching a paddle starts  the generation of code
                              // for debouncing:
                              while (checkPaddles() )
                                  ;                                                           // wait until paddles are released
                              active = !active;
             
                              cleanStartSettings();
                          } /// end touch to start
                          if (active)
                          switch (echoTrainerState) {
                            case  START_ECHO:   
                            case  SEND_WORD:
                            case  REPEAT_WORD:  echoResponse = ""; generateCW();
                                                break;
                            case  EVAL_ANSWER:  echoTrainerEval();
                                                break;
                            case  COMPLETE_ANSWER:                    
                            case  GET_ANSWER:   if (doPaddleIambic(MorseKeyer::leftKey, MorseKeyer::rightKey))
                                                    return;                             // we are busy keying and so need a very tight loop !
                                                break;
                            }                              
                            break;
      case MorseMachine::morseDecoder: doDecode();
                         if (speedChanged) {
                            speedChanged = false;
                            displayCWspeed();
                          }
      default:            break;
            
                        
  } // end switch and code depending on state of metaMorserino

/// if we have time check for button presses

    modeButton.Update();
    volButton.Update();
    
    switch (volButton.clicks) {
      case 1:   if (encoderState == scrollMode) {
                    if (morseState != morseDecoder)
                        encoderState = speedSettingMode;
                    else
                        encoderState = volumeSettingMode;
                    relPos = maxPos;
                    refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                    displayScrollBar(false);
                } else if (encoderState == volumeSettingMode && morseState != morseDecoder) {          //  single click toggles encoder between speed and volume
                  encoderState = speedSettingMode;
                  pref.begin("morserino", false);                     // open the namespace as read/write
                  if (pref.getUChar("sidetoneVolume") != MorsePreferences::prefs.sidetoneVolume)
                      pref.putUChar("sidetoneVolume", MorsePreferences::prefs.sidetoneVolume);  // store the last volume, if it has changed
                  pref.end();
                  displayCWspeed();
                  MorseDisplay::displayVolume();
                }
                else {
                  encoderState = volumeSettingMode;
                  displayCWspeed();
                  MorseDisplay::displayVolume();
                }
                break;
      case -1:  if (encoderState == scrollMode) {
                    encoderState = (morseState == morseDecoder ? volumeSettingMode : speedSettingMode);
                    relPos = maxPos;
                    refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                    MorseDisplay::displayScrollBar(false);
                }       
                else {
                    encoderState = scrollMode;
                    MorseDisplay::displayScrollBar(true);
                }
                break;
    }
   
    switch (modeButton.clicks) {                                // actions based on enocder button
       case -1:   menu_();                                       // long click exits current mode and goes to top menu
                  return;
       case 1:    if (morseState == morseGenerator || morseState == echoTrainer) {//  start/stop in trainer modi, in others does nothing currently
                  active = !active;
                  if (!active) {
                        //digitalWrite(keyerPin, LOW);           // turn the LED off, unkey transmitter, or whatever
                        //pwmNoTone(); 
                        MorseGenerator::keyOut(false, true, 0, 0);
                        MorseDisplay::printOnStatusLine(true, 0, "Continue w/ Paddle");
                  }
                  else {
                    cleanStartSettings();
                  }
                        
              }
              break;
       case 2:  setupPreferences(MorsePreferences::prefs.menuPtr);                               // double click shows the preferences menu (true would select a specific option only)
                MorseDisplay::clear();                                  // restore display
                displayTopLine();
                if (morseState == morseGenerator || morseState == echoTrainer) 
                    stopFlag = true;                                  // we stop what we had been doing
                else
                    stopFlag = false;
                //startFirst = true;
                //firstTime = true;
     default: break;
    }
    
/// and we have time to check the encoder
     if ((t = checkEncoder())) {
        //Serial.println("t: " + String(t));
        MorseUI::click();
        switch (encoderState) {
          case speedSettingMode:  
                                  changeSpeed(t);
                                  break;
          case volumeSettingMode: 
                                  MorsePreferences::prefs.sidetoneVolume += (t*10)+11;
                                  MorsePreferences::prefs.sidetoneVolume = constrain(MorsePreferences::prefs.sidetoneVolume, 11, 111) -11;
                                  //Serial.println(MorsePreferences::prefs.sidetoneVolume);
                                  displayVolume();
                                  break;
          case scrollMode:
                                  if (t == 1 && relPos < maxPos ) {        // up = scroll towards bottom
                                    ++relPos;
                                    refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                                  }
                                  else if (t == -1 && relPos > 0) {
                                    --relPos;
                                    refreshScrollArea((bottomLine + 1 + relPos) % NoOfLines);
                                  }
                                  //encoderPos = 0;
                                  //portEXIT_CRITICAL(&mux);
                                  MorseDisplay::displayScrollBar(true);
                                  break;
          }
    } // encoder 
    MorseSystem::checkShutDown(false);         // check for time out
    
}     /////////////////////// end of loop() /////////


void cleanStartSettings() {
    clearText = "";
    CWword = "";
    echoTrainerState = START_ECHO;
    generatorState = KEY_UP; 
    keyerState = IDLE_STATE;
    interWordTimer = 4294967000;                 // almost the biggest possible unsigned long number :-) - do not output a space at the beginning
    genTimer = millis()-1;                       // we will be at end of KEY_DOWN when called the first time, so we can fetch a new word etc... 
    wordCounter = 0;                             // reset word counter for maxSequence
    startFirst = true;
    displayTopLine();
}


////// The MENU


void menu_() {
   uint8_t newMenuPtr = MorsePreferences::prefs.menuPtr;
   uint8_t disp = 0;
   int t, command;
   
     //// initialize a few things now
     //Serial.println("THE MENU");
    ///updateTimings(); // now done after reading preferences
    LoRa.idle();
    //keyerState = IDLE_STATE;
    active = false;
    //startFirst = true;
    cleanStartSettings();
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

    keyOut(false, true, 0, 0);
    keyOut(false, false, 0, 0);
    encoderState = speedSettingMode;             // always start with this encoderstate (decoder will change it anyway)
    currentOptions = allOptions;                 // this is the array of options when we double click the BLACK button: while in menu, you can change all of them
    currentOptionSize = SizeOfArray(allOptions);
    pref.begin("morserino", false);              // open the namespace as read/write
    if ((MorsePreferences::prefs.fileWordPointer != pref.getUInt("fileWordPtr")))   // update word pointer if necessary (if we ran player before)
       pref.putUInt("fileWordPtr", MorsePreferences::prefs.fileWordPointer);
    pref.end(); 
    file.close();                               // just in case it is still open....
    MorseDisplay::clear();
    
    while (true) {                          // we wait for a click (= selection)
        if (disp != newMenuPtr) {
          disp = newMenuPtr;
          menuDisplay(disp);
        }
        if (quickStart) {
            quickStart = false;
            command = 1;
            delay(500);
            MorseDisplay::printOnScrollFlash(2, REGULAR, 1, "QUICK START");
        }
        else {
            modeButton.Update();
            command = modeButton.clicks;
        }

        switch (command) {                                          // actions based on enocder button
          case 2: if (setupPreferences(newMenuPtr))                       // all available options when called from top menu
                    newMenuPtr = MorsePreferences::prefs.menuPtr;
                  menuDisplay(newMenuPtr);
                  break;
          case 1: // check if we have a submenu or if we execute the selection
                  //Serial.println("newMP: " + String(newMenuPtr) + " navi: " + String(menuNav[newMenuPtr][naviDown]));
                  if (menuItems[newMenuPtr].nav[naviDown] == 0) {
                      MorsePreferences::prefs.menuPtr = newMenuPtr;
                      disp = 0;
                      if (menuItems[MorsePreferences::prefs.menuPtr].remember) {            // remember last executed, unless it is a wifi function or shutdown
                          pref.begin("morserino", false);             // open the namespace as read/write
                          pref.putUChar("lastExecuted", MorsePreferences::prefs.menuPtr);   // store last executed command
                          pref.end();                                 // close namespace
                      }
                      if (menuExec())
                        return;
                  } else {
                      newMenuPtr = menuItems[newMenuPtr].nav[naviDown];
                  }
                  break;
          case -1:  // we need to go one level up, if possible
                  if (menuItems[newMenuPtr].nav[naviUp] != 0)
                      newMenuPtr = menuItems[newMenuPtr].nav[naviUp];
          default: break;
        }

       if ((t=checkEncoder())) {
          //pwmClick(MorsePreferences::prefs.sidetoneVolume);         /// click
          newMenuPtr =  menuItems[newMenuPtr].nav[(t == -1) ? naviLeft : naviRight];
       }

       volButton.Update();
    
       switch (volButton.clicks) {
          case -1:  audioLevelAdjust();                         /// for adjusting line-in audio level (at the same time keying tx and sending oudio on line-out
                    MorseDisplay::clear();
                    menuDisplay(disp);
                    break;
          /* case  3:  wifiFunction();                                  /// configure wifi, upload file or firmware update
                    break;
          */
       }
       MorseSystem::checkShutDown(false);                  // check for time out
  } // end while - we leave as soon as the button has been pressed
} // end menu_() 


void menuDisplay(uint8_t ptr) {
  //Serial.println("Level: " + (String) menuItems[ptr].nav[naviLevel] + " " + menuItems[ptr].text);
  uint8_t oneUp = menuItems[ptr].nav[naviUp];
  uint8_t twoUp = menuItems[oneUp].nav[naviUp];
  uint8_t oneDown = menuItems[ptr].nav[naviDown];
    
  MorseDisplay::printOnStatusLine(true, 0,  "Select Modus:     ");

  //MorseDisplay::clearLine(0); MorseDisplay::clearLine(1); MorseDisplay::clearLine(2);                       // delete previous content
  MorseDisplay::clearScroll();
  
  /// level 0: top line, possibly ".." on line 1
  /// level 1: higher level on 0, item on 1, possibly ".." on 2
  /// level 2: higher level on 1, highest level on 0, item on 2
  switch (menuItems[ptr].nav[naviLevel]) {
    case 2: {MorseDisplay::printOnScroll(2, BOLD, 0, menuItems[ptr].text);
    MorseDisplay::printOnScroll(1, REGULAR, 0, menuItems[oneUp].text);
    MorseDisplay::printOnScroll(0, REGULAR, 0, menuItems[twoUp].text);
            break;
    }
    case 1: {if (oneDown) {
        MorseDisplay::printOnScroll(2, REGULAR, 0, String(".."));
    }
    MorseDisplay::printOnScroll(1, BOLD, 0, menuItems[ptr].text);
    MorseDisplay::printOnScroll(0, REGULAR, 0, menuItems[oneUp].text);
    }
            break;
    case 0: {
            if (oneDown) {
                MorseDisplay::printOnScroll(1, REGULAR, 0, String(".."));
            }
            MorseDisplay::printOnScroll(0, BOLD, 0, menuItems[ptr].text);
    }
            break;
  }
}

///////////// GEN_TYPE { RANDOMS, ABBREVS, WORDS, CALLS, MIXED, KOCH_MIXED, KOCH_LEARN };           // the things we can generate in generator mode




boolean menuExec() {                                          // return true if we should  leave menu after execution, true if we should stay in menu
  //Serial.println("Executing menu item " + String(MorsePreferences::prefs.menuPtr));

  uint32_t wcount = 0;

  effectiveAutoStop = false;
  effectiveTrainerDisplay = MorsePreferences::prefs.trainerDisplay;
  
  kochActive = false;
  switch (MorsePreferences::prefs.menuPtr) {
    case  _keyer:  /// keyer
                currentOptions = keyerOptions;                // list of available options in keyer mode
                currentOptionSize = SizeOfArray(keyerOptions);
                morseState = morseKeyer;
                MorseDisplay::clear();
                MorseDisplay::printOnScroll(1, REGULAR, 0, "Start CW Keyer" );
                delay(500);
                MorseDisplay::clear();
                displayTopLine();
                MorseDisplay::printToScroll(REGULAR,"");      // clear the buffer
                clearPaddleLatches();
                keyTx = true;
                return true;
                break;

     case _headRand:
     case _headAbb:
     case _headWords:
     case _headCalls:
     case _headMixed:      /// head copying
                setupHeadCopying();
                currentOptions = headOptions;
                currentOptionSize = SizeOfArray(headOptions);
                goto startTrainer;
     case _genRand:
     case _genAbb:
     case _genWords:
     case _genCalls:
     case _genMixed:      /// generator
                currentOptions = generatorOptions;                            // list of available options in generator mode
                currentOptionSize = SizeOfArray(generatorOptions);
                goto startTrainer;
     case _headPlayer:
                setupHeadCopying();
                currentOptions = headOptions;
                currentOptionSize = SizeOfArray(headOptions);
                goto startPlayer;
     case _genPlayer:  
                currentOptions = playerOptions;                               // list of available options in player mode
                currentOptionSize = SizeOfArray(playerOptions);
     startPlayer:
                file = SPIFFS.open("/player.txt");                            // open file
                //skip MorsePreferences::prefs.fileWordPointer words, as they have been played before
                wcount = MorsePreferences::prefs.fileWordPointer;
                MorsePreferences::prefs.fileWordPointer = 0;
                MorsePlayerFile::skipWords(wcount);
                
     startTrainer:
                generatorMode = menuItems[MorsePreferences::prefs.menuPtr].generatorMode;
                startFirst = true;
                firstTime = true;
                morseState = morseGenerator;
                MorseDisplay::clear();
                MorseDisplay::printOnScroll(0, REGULAR, 0, "Generator     ");
                MorseDisplay::printOnScroll(1, REGULAR, 0, "Start/Stop:   ");
                MorseDisplay::printOnScroll(2, REGULAR, 0, "Paddle | BLACK");
                delay(1250);
                MorseDisplay::clear();
                displayTopLine();
                MorseDisplay::clearScroll();      // clear the buffer
                keyTx = true;
                return true;
                break;
      case  _echoRand:
      case  _echoAbb:
      case  _echoWords:
      case  _echoCalls:
      case  _echoMixed:
                currentOptions = echoTrainerOptions;                        // list of available options in echo trainer mode
                currentOptionSize = SizeOfArray(echoTrainerOptions);
                generatorMode = menuItems[MorsePreferences::prefs.menuPtr].generatorMode;
                goto startEcho;
      case  _echoPlayer:    /// echo trainer
                generatorMode = menuItems[MorsePreferences::prefs.menuPtr].generatorMode;
                currentOptions = echoPlayerOptions;                         // list of available options in echo player mode
                currentOptionSize = SizeOfArray(echoPlayerOptions);
                file = SPIFFS.open("/player.txt");                            // open file
                //skip MorsePreferences::prefs.fileWordPointer words, as they have been played before
                wcount = MorsePreferences::prefs.fileWordPointer;
                MorsePreferences::prefs.fileWordPointer = 0;
                MorsePlayerFile::skipWords(wcount);
       startEcho:
                startFirst = true;
                morseState = echoTrainer;
                echoStop = false;
                MorseDisplay::clear();
                MorseDisplay::printOnScroll(0, REGULAR, 0, generatorMode == KOCH_LEARN ? "New Character:" : "Echo Trainer:");
                MorseDisplay::printOnScroll(1, REGULAR, 0, "Start:       ");
                MorseDisplay::printOnScroll(2, REGULAR, 0, "Press paddle ");
                delay(1250);
                MorseDisplay::clear();
                displayTopLine();
                MorseDisplay::printToScroll(REGULAR,"");      // clear the buffer
                keyTx = false;
                return true;
                break;
      case  _kochSel: // Koch Select 
                displayKeyerPreferencesMenu(posKochFilter);
                adjustKeyerPreference(posKochFilter);
                writePreferences("morserino");
                //createKochWords(MorsePreferences::prefs.wordLength, MorsePreferences::prefs.kochFilter) ;  // update the arrays!
                //createKochAbbr(MorsePreferences::prefs.abbrevLength, MorsePreferences::prefs.kochFilter);
                return false;
                break;
      case  _kochLearn:   // Koch Learn New .  /// just a new generatormode....
                generatorMode = KOCH_LEARN;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _kochGenRand: // RANDOMS 
                generatorMode = RANDOMS;
                kochActive = true;
                currentOptions = kochGenOptions;                          // list of available options in Koch generator mode
                currentOptionSize = SizeOfArray(kochGenOptions);
                goto startTrainer;
      case  _kochGenAbb: // ABBREVS - 2
                generatorMode = ABBREVS;
                kochActive = true;
                currentOptions = kochGenOptions;                          // list of available options in Koch generator mode
                currentOptionSize = SizeOfArray(kochGenOptions);
                goto startTrainer;
      case  _kochGenWords: // WORDS - 3
                generatorMode = WORDS;
                kochActive = true;
                currentOptions = kochGenOptions;                          // list of available options in Koch generator mode
                currentOptionSize = SizeOfArray(kochGenOptions);
                goto startTrainer;
      case  _kochGenMixed: // KOCH_MIXED - 5
                generatorMode = KOCH_MIXED;
                kochActive = true;
                currentOptions = kochGenOptions;                          // list of available options in Koch generator mode
                currentOptionSize = SizeOfArray(kochGenOptions);
                goto startTrainer;
      case  _kochEchoRand: // Koch Echo Random
                generatorMode = RANDOMS;
                kochActive = true;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo trainer mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _kochEchoAbb: // ABBREVS - 2
                generatorMode = ABBREVS;
                kochActive = true;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo trainer mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _kochEchoWords: // WORDS - 3
                generatorMode = WORDS;
                kochActive = true;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo trainer mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _kochEchoMixed: // KOCH_MIXED - 5
                generatorMode = KOCH_MIXED;
                kochActive = true;
                currentOptions = kochEchoOptions;                          // list of available options in Koch echo trainer mode
                currentOptionSize = SizeOfArray(kochEchoOptions);
                goto startEcho;
      case  _trxLora: // LoRa Transceiver
                currentOptions = loraTrxOptions;                            // list of available options in lora trx mode
                currentOptionSize = SizeOfArray(loraTrxOptions);
                morseState = loraTrx;
                MorseDisplay::clear();
                MorseDisplay::printOnScroll(1, REGULAR, 0, "Start LoRa Trx" );
                delay(600);
                MorseDisplay::clear();
                displayTopLine();
                MorseDisplay::printToScroll(REGULAR,"");      // clear the buffer
                clearPaddleLatches();
                keyTx = false;
                clearText = "";
                LoRa.receive();
                return true;
                break;
      case  _trxIcw: /// icw/ext TRX
                currentOptions = extTrxOptions;                            // list of available options in ext trx mode
                currentOptionSize = SizeOfArray(extTrxOptions);
                morseState = morseTrx;
                MorseDisplay::clear();
                MorseDisplay::printOnScroll(1, REGULAR, 0, "Start CW Trx" );
                clearPaddleLatches();
                keyTx = true;
                goto setupDecoder;

      case  _decode: /// decoder
                currentOptions = decoderOptions;                            // list of available options in lora trx mode
                currentOptionSize = SizeOfArray(decoderOptions);
                morseState = morseDecoder;
                  /// here we will do the init for decoder mode
                //trainerMode = false;
                encoderState = volumeSettingMode;
                keyTx = false;
                MorseDisplay::clear();
                MorseDisplay::printOnScroll(1, REGULAR, 0, "Start Decoder" );
      setupDecoder:
                speedChanged = true;
                delay(650);
                MorseDisplay::clear();
                displayTopLine();
                drawInputStatus(false);
                MorseDisplay::printToScroll(REGULAR,"");      // clear the buffer
                
                displayCWspeed();
                MorseDisplay::displayVolume();
                  
                /// set up variables for Goertzel Morse Decoder
                setupGoertzel();
                filteredState = filteredStateBefore = false;
                decoderState = LOW_;
                ditAvg = 60;
                dahAvg = 180;
                return true;
                break;
      case  _wifi_mac:
                WiFi.mode(WIFI_MODE_STA);               // init WiFi as client
                MorseDisplay::clearDisplay();
                MorseDisplay::printOnStatusLine(true, 0,  WiFi.macAddress());
                delay(2000);
                MorseDisplay::printOnScroll(0, REGULAR, 0, "RED: restart" );
                delay(1000);  
                while (true) {
                  MorseSystem::checkShutDown(false);  // possibly time-out: go to sleep
                  if (digitalRead(volButtonPin) == LOW)
                    ESP.restart();
                }
                break;
      case  _wifi_config:
                startAP();          // run as AP to get WiFi credentials from user
                break;
      case _wifi_check:
                MorseDisplay::clearDisplay();
                MorseDisplay::printOnStatusLine(true, 0,  "Connecting... ");
                if (! wifiConnect())
                    ; //return false;  
                else {
                    MorseDisplay::printOnStatusLine(true, 0,  "Connected!    ");
                    MorseDisplay::printOnScroll(0, REGULAR, 0, MorsePreferences::prefs.wlanSSID);
                    MorseDisplay::printOnScroll(1, REGULAR, 0, WiFi.localIP().toString());
                }
                WiFi.mode( WIFI_MODE_NULL ); // switch off WiFi                      
                delay(1000);
                MorseDisplay::printOnScroll(2, REGULAR, 0, "RED: return" );
                while (true) {
                      MorseSystem::checkShutDown(false);  // possibly time-out: go to sleep
                      if (digitalRead(volButtonPin) == LOW) {
                        return false;
                      }
                }
                break;
      case _wifi_upload:
                uploadFile();       // upload a text file
                break;
      case _wifi_update:
                updateFirmware();   // run OTA update
                break;
      case  _goToSleep: /// deep sleep
                MorseSystem::checkShutDown(true);
      default:  break;
  }
  return false;
}   /// end menuExec()





///////////////////
// we use the paddle for iambic keying
/////

boolean doPaddleIambic (boolean dit, boolean dah) {
  boolean paddleSwap;                      // temp storage if we need to swap left and right
  static long ktimer;                      // timer for current element (dit or dah)
  static long curtistimer;                 // timer for early paddle latch in Curtis mode B+
  static long latencytimer;                // timer for "muting" paddles for some time in state INTER_ELEMENT
  unsigned int pitch;

  if (!MorsePreferences::prefs.didah)   {              // swap left and right values if necessary!
      paddleSwap = dit; dit = dah; dah = paddleSwap; 
  }
      

  switch (keyerState) {                                         // this is the keyer state machine
     case IDLE_STATE:
         // display the interword space, if necessary
         if (millis() > interWordTimer) {
             if (morseState == loraTrx)    {                    // when in Trx mode
                 cwForLora(3);
                 sendWithLora();                        // finalise the string and send it to LoRA
             }
             MorseDisplay::printToScroll(REGULAR, " ");                       // output a blank
             interWordTimer = 4294967000;                       // almost the biggest possible unsigned long number :-) - do not output extra spaces!
             if (echoTrainerState == COMPLETE_ANSWER)   {       // change the state of the trainer at end of word
                echoTrainerState = EVAL_ANSWER;
                return false;
             }
         }
        
       // Was there a paddle press?
        if (dit || dah ) {
            updatePaddleLatch(dit, dah);  // trigger the paddle latches
            if (morseState == echoTrainer)   {      // change the state of the trainer at end of word
                echoTrainerState = COMPLETE_ANSWER;
             }
            treeptr = 0;
            if (dit) {
                setDITstate();          // set next state
                DIT_FIRST = true;          // first paddle pressed after IDLE was a DIT
            }
            else {
                setDAHstate();  
                DIT_FIRST = false;         // first paddle was a DAH
            }
        }
        else {
           if (echoTrainerState == GET_ANSWER && millis() > genTimer) {
            echoTrainerState = EVAL_ANSWER;
         } 
         return false;                // we return false if there was no paddle press in IDLE STATE - Arduino can do other tasks for a bit
        }
        break;

    case DIT:
    /// first we check that we have waited as defined by ACS settings
            if ( MorsePreferences::prefs.ACSlength > 0 && (millis() <= acsTimer))  // if we do automatic character spacing, and haven't waited for (3 or whatever) dits...
              break;
            clearPaddleLatches();                           // always clear the paddle latches at beginning of new element
            keyerControl |= DIT_LAST;                        // remember that we process a DIT

            ktimer = ditLength;                              // prime timer for dit
            switch ( MorsePreferences::prefs.keyermode ) {
              case IAMBICB:  curtistimer = 2 + (ditLength * MorsePreferences::prefs.curtisBDotTiming / 100);
                             break;                         // enhanced Curtis mode B starts checking after some time
              case NONSQUEEZE:
                             curtistimer = 3;
                             break;
              default:
                             curtistimer = ditLength;        // no early paddle checking in Curtis mode A Ultimatic mode oder Non-squeeze
                             break;
            }
            keyerState = KEY_START;                          // set next state of state machine
            break;
            
    case DAH:
            if ( MorsePreferences::prefs.ACSlength > 0 && (millis() <= acsTimer))  // if we do automatic character spacing, and haven't waited for 3 dits...
              break;
            clearPaddleLatches();                          // clear the paddle latches
            keyerControl &= ~(DIT_LAST);                    // clear dit latch  - we are not processing a DIT
            
            ktimer = dahLength;
            switch (MorsePreferences::prefs.keyermode) {
              case IAMBICB:  curtistimer = 2 + (dahLength * MorsePreferences::prefs.curtisBTiming / 100);    // enhanced Curtis mode B starts checking after some time
                             break;
              case NONSQUEEZE:
                             curtistimer = 3;
                             break;
              default:
                             curtistimer = dahLength;        // no early paddle checking in Curtis mode A or Ultimatic mode
                             break;
            }
            keyerState = KEY_START;                          // set next state of state machine
            break;
     

      
    case KEY_START:
          // Assert key down, start timing, state shared for dit or dah
          pitch = notes[MorsePreferences::prefs.sidetoneFreq];
          if ((morseState == echoTrainer || morseState == loraTrx) && MorsePreferences::prefs.echoToneShift != 0) {
             pitch = (MorsePreferences::prefs.echoToneShift == 1 ? pitch * 18 / 17 : pitch * 17 / 18);        /// one half tone higher or lower, as set in parameters in echo trainer mode
          }
           //pwmTone(pitch, MorsePreferences::prefs.sidetoneVolume, true);
           //keyTransmitter();
           keyOut(true, true, pitch, MorsePreferences::prefs.sidetoneVolume);
           ktimer += millis();                     // set ktimer to interval end time          
           curtistimer += millis();                // set curtistimer to curtis end time
           keyerState = KEYED;                     // next state
           break;
 
    case KEYED:
                                                   // Wait for timers to expire
           if (millis() > ktimer) {                // are we at end of key down ?
               //digitalWrite(keyerPin, LOW);        // turn the LED off, unkey transmitter, or whatever
               //pwmNoTone();                      // stop side tone
               keyOut(false, true, 0, 0);
               ktimer = millis() + ditLength;    // inter-element time
               latencytimer = millis() + ((MorsePreferences::prefs.latency-1) * ditLength / 8);
               keyerState = INTER_ELEMENT;       // next state
            }
            else if (millis() > curtistimer ) {     // in Curtis mode we check paddle as soon as Curtis time is off
                 if (keyerControl & DIT_LAST)       // last element was a dit
                    updatePaddleLatch(false, dah);  // not sure here: we only check the opposite paddle - should be ok for Curtis B
                 else
                    updatePaddleLatch(dit, false);  
                 // updatePaddleLatch(dit, dah);       // but we remain in the same state until element time is off! 
            }
            break;
 
    case INTER_ELEMENT:
            //if ((MorsePreferences::prefs.keyermode != NONSQUEEZE) && (millis() < latencytimer)) {     // or should it be MorsePreferences::prefs.keyermode > 2 ? Latency for Ultimatic mode?
            if (millis() < latencytimer) {
              if (keyerControl & DIT_LAST)       // last element was a dit
                    updatePaddleLatch(false, dah);  // not sure here: we only check the opposite paddle - should be ok for Curtis B
              else
                    updatePaddleLatch(dit, false);
              // updatePaddleLatch(dit, dah); 
            }
            else {
                updatePaddleLatch(dit, dah);          // latch paddle state while between elements       
                if (millis() > ktimer) {               // at end of INTER-ELEMENT
                    switch(keyerControl) {
                          case 3:                                         // both paddles are latched
                          case 7: 
                                  switch (MorsePreferences::prefs.keyermode) {
                                      case NONSQUEEZE:  if (DIT_FIRST)                      // when first element was a DIT
                                                               setDITstate();            // next element is a DIT again
                                                        else                                // but when first element was a DAH
                                                               setDAHstate();            // the next element is a DAH again! 
                                                        break;
                                      case ULTIMATIC:   if (DIT_FIRST)                      // when first element was a DIT
                                                               setDAHstate();            // next element is a DAH
                                                        else                                // but when first element was a DAH
                                                               setDITstate();            // the next element is a DIT! 
                                                        break;
                                      default:          if (keyerControl & DIT_LAST)     // Iambic: last element was a dit - this is case 7, really
                                                            setDAHstate();               // next element will be a DAH
                                                        else                                // and this is case 3 - last element was a DAH
                                                            setDITstate();               // the next element is a DIT                         
                                   }
                                   break;
                                                                          // dit only is latched, regardless what was last element  
                          case 1:
                          case 5:  
                                   setDITstate();
                                   break;
                                                                          // dah only latched, regardless what was last element
                          case 2:
                          case 6:  
                                   setDAHstate();
                                   break;
                                                                          // none latched, regardless what was last element
                          case 0:
                          case 4:  
                                   keyerState = IDLE_STATE;               // we are at the end of the character and go back into IDLE STATE
                                   displayMorse();                        // display the decoded morse character(s)
                                   if (morseState == loraTrx)
                                      cwForLora(0);
                                   
                                   MorsePreferences::fireCharSeen(false);

                                   if (MorsePreferences::prefs.ACSlength > 0)
                                        acsTimer = millis() + MorsePreferences::prefs.ACSlength * ditLength; // prime the ACS timer
                                   if (morseState == morseKeyer || morseState == loraTrx || morseState == morseTrx)
                                      interWordTimer = millis() + 5*ditLength;
                                   else
                                       interWordTimer = millis() + interWordSpace;  // prime the timer to detect a space between characters
                                                                              // nominally 7 dit-lengths, but we are not quite so strict here in keyer or TrX mode,
                                                                              // use the extended time in echo trainer mode to allow longer space between characters, 
                                                                              // like in listening
                                   keyerControl = 0;                          // clear all latches completely before we go to IDLE
                          break;
                    } // switch keyerControl : evaluation of flags
                }
            } // end of INTER_ELEMENT
  } // end switch keyerState - end of state machine

  if (keyerControl & 3)                                               // any paddle latch?                            
    return true;                                                      // we return true - we processed a paddle press
  else
    return false;                                                     // when paddle latches are cleared, we return false
} /////////////////// end function doPaddleIambic()



//// this function checks the paddles (touch or external), returns true when a paddle has been activated, 
///// and sets the global variable leftKey and rightKey accordingly


boolean checkPaddles() {
  static boolean oldL = false, newL, oldR = false, newR;
  int left, right;
  static long lTimer = 0, rTimer = 0;
  const int debDelay = 750;       // debounce time = 0,75  ms
  
  /* intral and external paddle are now working in parallel - the parameter MorsePreferences::prefs.extPaddle is used to indicate reverse polarity of external paddle
  */
  left = MorsePreferences::prefs.useExtPaddle ? rightPin : leftPin;
  right = MorsePreferences::prefs.useExtPaddle ? leftPin : rightPin;
  sensor = readSensors(LEFT, RIGHT);
  newL = (sensor >> 1) | (!digitalRead(left)) ;
  newR = (sensor & 0x01) | (!digitalRead(right)) ;

  if ((MorsePreferences::prefs.keyermode == NONSQUEEZE) && newL && newR)
    return (leftKey || rightKey);

  if (newL != oldL)
      lTimer = micros();
  if (newR != oldR)
      rTimer = micros();
  if (micros() - lTimer > debDelay)
      if (newL != leftKey) 
          leftKey = newL;
  if (micros() - rTimer > debDelay)
      if (newR != rightKey) 
          rightKey = newR;       

  oldL = newL;
  oldR = newR;
  
  return (leftKey || rightKey);
}

///
/// Keyer subroutines
///

// update the paddle latches in keyerControl
void updatePaddleLatch(boolean dit, boolean dah)
{ 
    if (dit)
      keyerControl |= DIT_L;
    if (dah)
      keyerControl |= DAH_L;
}

// clear the paddle latches in keyer control
void clearPaddleLatches ()
{
    keyerControl &= ~(DIT_L + DAH_L);   // clear both paddle latch bits
}

// functions to set DIT and DAH keyer states
void setDITstate() {
  keyerState = DIT;
  treeptr = CWtree[treeptr].dit;
  if (morseState == loraTrx)
      cwForLora(1);                         // build compressed string for LoRA
}

void setDAHstate() {
  keyerState = DAH;
  treeptr = CWtree[treeptr].dah;
  if (morseState == loraTrx)
      cwForLora(2);
}


// toggle polarity of paddles
void togglePolarity () {
      MorsePreferences::prefs.didah = !MorsePreferences::prefs.didah;
     //displayPolarity();
}
  





//////// Display the status line in CW Keyer Mode
//////// Layout of top line:
//////// Tch ul 15 WpM
//////// 0    5    0

void displayTopLine() {
    MorseDisplay::clearStatusLine();

  // printOnStatusLine(true, 0, (MorsePreferences::prefs.useExtPaddle ? "X " : "T "));          // we do not show which paddle is in use anymore
  if (morseState == morseGenerator) 
      MorseDisplay::printOnStatusLine(true, 1,  MorsePreferences::prefs.wordDoubler ? "x2" : "  ");
  else {
    switch (MorsePreferences::prefs.keyermode) {
      case IAMBICA:   MorseDisplay::printOnStatusLine(false, 2,  "A "); break;          // Iambic A (no paddle eval during dah)
      case IAMBICB:   MorseDisplay::printOnStatusLine(false, 2,  "B "); break;          // orig Curtis B mode: paddle eval during element
      case ULTIMATIC: MorseDisplay::printOnStatusLine(false, 2,  "U "); break;          // Ultimatic Mode
      case NONSQUEEZE: MorseDisplay::printOnStatusLine(false, 2,  "N "); break;         // Non-squeeze mode
    }
  }

  displayCWspeed();                                     // update display of CW speed
  if ((morseState == loraTrx ) || (morseState == morseGenerator  && MorsePreferences::prefs.loraTrainerMode == true))
      dispLoraLogo();

  MorseDisplay::displayVolume();                                     // sidetone volume
  MorseDisplay::display();
}


//////// Display the current CW speed
/////// pos 7-8, "Wpm" on 10-12

void displayCWspeed () {
  if (( morseState == morseGenerator || morseState ==  echoTrainer )) 
      sprintf(numBuffer, "(%2i)", effWpm);   
  else sprintf(numBuffer, "    ");
  
  MorseDisplay::printOnStatusLine(false, 3,  numBuffer);                                         // effective wpm
  
  sprintf(numBuffer, "%2i", MorsePreferences::prefs.wpm);
  MorseDisplay::printOnStatusLine(encoderState == speedSettingMode ? true : false, 7,  numBuffer);
  MorseDisplay::printOnStatusLine(false, 10,  "WpM");
  MorseDisplay::display();
}


/// function to read sensors:
/// read both left and right twice, repeat reading if it returns 0
/// return a binary value, depending on a (adaptable?) threshold:
/// 0 = nothing touched,  1= right touched, 2 = left touched, 3 = both touched
/// binary:   00          01                10                11

uint8_t readSensors(int left, int right) {
  //static boolean first = true;
  uint8_t v, lValue, rValue;
  
  while ( !(v=touchRead(left)) )
    ;                                       // ignore readings with value 0
  lValue = v;
   while ( !(v=touchRead(right)) )
    ;                                       // ignore readings with value 0
  rValue = v;
  while ( !(v=touchRead(left)) )
    ;                                       // ignore readings with value 0
  lValue = (lValue+v) /2;
   while ( !(v=touchRead(right)) )
    ;                                       // ignore readings with value 0
  rValue = (rValue+v) /2;

  if (lValue < (MorsePreferences::prefs.tLeft+10))     {           //adaptive calibration
      //if (first) Serial.println("p-tLeft " + String(MorsePreferences::prefs.tLeft));
      //if (first) Serial.print("lValue: "); if (first) Serial.println(lValue);
      //printOnScroll(0, INVERSE_BOLD, 0,  String(lValue) + " " + String(MorsePreferences::prefs.tLeft));
      MorsePreferences::prefs.tLeft = ( 7*MorsePreferences::prefs.tLeft +  ((lValue+lUntouched) / SENS_FACTOR) ) / 8;
      //Serial.print("MorsePreferences::prefs.tLeft: "); Serial.println(MorsePreferences::prefs.tLeft);
  }
  if (rValue < (MorsePreferences::prefs.tRight+10))     {           //adaptive calibration
      //if (first) Serial.println("p-tRight " + String(MorsePreferences::prefs.tRight));
      //if (first) Serial.print("rValue: "); if (first) Serial.println(rValue);
      //printOnScroll(1, INVERSE_BOLD, 0,  String(rValue) + " " + String(MorsePreferences::prefs.tRight));
      MorsePreferences::prefs.tRight = ( 7*MorsePreferences::prefs.tRight +  ((rValue+rUntouched) / SENS_FACTOR) ) / 8;
      //Serial.print("MorsePreferences::prefs.tRight: "); Serial.println(MorsePreferences::prefs.tRight);
  }
  //first = false;
  return ( lValue < MorsePreferences::prefs.tLeft ? 2 : 0 ) + (rValue < MorsePreferences::prefs.tRight ? 1 : 0 );
}


void initSensors() {
  int v;
  lUntouched = rUntouched = 60;       /// new: we seek minimum
  for (int i=0; i<8; ++i) {
      while ( !(v=touchRead(LEFT)) )
        ;                                       // ignore readings with value 0
        lUntouched += v;
        //lUntouched = _min(lUntouched, v);
       while ( !(v=touchRead(RIGHT)) )
        ;                                       // ignore readings with value 0
        rUntouched += v;
        //rUntouched = _min(rUntouched, v);
  }
  lUntouched /= 8;
  rUntouched /= 8;
  MorsePreferences::prefs.tLeft = lUntouched - 9;
  MorsePreferences::prefs.tRight = rUntouched - 9;
}









////// setup preferences ///////

             
boolean setupPreferences(uint8_t atMenu) {
  // enum morserinoMode {morseKeyer, loraTrx, morseGenerator, echoTrainer, shutDown, morseDecoder, invalid };
  static int oldPos = 1;
  int t;

  int ptrIndex, ptrMax;
  prefPos posPtr;
 
  ptrMax = currentOptionSize;

  ///// we should check here if the old ptr (oldIndex) is contained in the current preferences collection (currentOptions)
  ptrIndex = 1;
  for (int i = 0; i < ptrMax; ++i) {
      if (currentOptions[i] == oldPos) {
          ptrIndex = i;
          break;
      }
  }
  posPtr = currentOptions[ptrIndex];  
  keyOut(false, true, 0, 0);                // turn the LED off, unkey transmitter, or whatever; just in case....
  keyOut(false,false, 0, 0);  
  displayKeyerPreferencesMenu(posPtr);
  MorseDisplay::printOnScroll(2, REGULAR, 0,  " ");

  while (true) {                            // we wait for single click = selection or long click = exit - or single or long click or RED button
        modeButton.Update();
        switch (modeButton.clicks) {            // button was clicked
          case 1:     // change the option corresponding to pos
                      if (adjustKeyerPreference(posPtr))
                         goto exitFromHere;
                      break;
          case -1:    //////// long press indicates we are done with setting preferences - check if we need to store some of the preferences
          exitFromHere: writePreferences("morserino");
                        //delay(200);
                        return false;
                        break;
          }

          volButton.Update();                 // RED button
          switch (volButton.clicks) {         // was clicked
            case 1:     // recall snapshot
                        if (recallSnapshot())
                          writePreferences("morserino");
                        //delay(100);
                        return true;
                        break;
            case -1:    //store snapshot
                        
                        if (storeSnapshot(atMenu))
                          writePreferences("morserino");
                        while(volButton.clicks)
                          volButton.Update();
                        return false;
                        break;
          }

          
          //// display the value of the preference in question

         if ((t=checkEncoder())) {
            MorseUI::click();
            ptrIndex = (ptrIndex +ptrMax + t) % ptrMax;
            //Serial.println("ptrIndex: " + String(ptrIndex));
            posPtr = currentOptions[ptrIndex];
            //oldIndex = ptrIndex;                                                              // remember menu position
            oldPos = posPtr;
            
            displayKeyerPreferencesMenu(posPtr);
            //printOnScroll(1, BOLD, 0, ">");
            MorseDisplay::printOnScroll(2, REGULAR, 0, " ");

            MorseDisplay::display();                                                        // update the display
         }    // end if (encoderPos)
         MorseSystem::checkShutDown(false);         // check for time out
  } // end while - we leave as soon as the button has been pressed long
}   // end function setupKeyerPreferences()



///////// evaluate the response in Echo Trainer Mode
void echoTrainerEval() {
    delay(interCharacterSpace / 2);
    if (echoResponse == echoTrainerWord) {
      echoTrainerState = SEND_WORD;
      MorseDisplay::printToScroll(BOLD,  "OK");
      if (MorsePreferences::prefs.echoConf) {
          pwmTone(440,  MorsePreferences::prefs.sidetoneVolume, false);
          delay(97);
          pwmNoTone();
          pwmTone(587,  MorsePreferences::prefs.sidetoneVolume, false);
          delay(193);
          pwmNoTone();
      }
      delay(interWordSpace);
      if (MorsePreferences::prefs.speedAdapt)
          changeSpeed(1);
    } else {
      echoTrainerState = REPEAT_WORD;
      if (generatorMode != KOCH_LEARN || echoResponse != "") {
          MorseDisplay::printToScroll(BOLD, "ERR");
          if (MorsePreferences::prefs.echoConf) {
              pwmTone(311,  MorsePreferences::prefs.sidetoneVolume, false);
              delay(193);
              pwmNoTone();
          }
      }
      delay(interWordSpace);
      if (MorsePreferences::prefs.speedAdapt)
          changeSpeed(-1);
    }
    echoResponse = "";
    clearPaddleLatches();
}   // end of function



void changeSpeed( int t) {
  MorsePreferences::prefs.wpm += t;
  MorsePreferences::prefs.wpm = constrain(MorsePreferences::prefs.wpm, 5, 60);
  updateTimings();
  displayCWspeed();                     // update display of CW speed
  charCounter = 0;                                    // reset character counter
}














///////////////// a test function for adjusting audio levels

void audioLevelAdjust() {
    uint16_t i, maxi, mini;
    uint16_t testData[1216];

    display.clear();
    printOnScroll(0, BOLD, 0, "Audio Adjust");
    printOnScroll(1, REGULAR, 0, "End with RED");
    keyTx = true;
    keyOut(true,  true, 698, 0);                                  /// we generate a side tone, f=698 Hz, also on line-out, but with vol down on speaker
    while (true) {
        volButton.Update();
        if (volButton.clicks)
            break;                                                /// pressing the red button gets you out of this mode!
        for (i = 0; i < goertzel_n ; ++i)
            testData[i] = analogRead(audioInPin);                 /// read analog input
        maxi = mini = testData[0];
        for (i = 1; i< goertzel_n ; ++i) {
            if (testData[i] < mini)
              mini = testData[i];
            if (testData[i] > maxi)
              maxi = testData[i];
        }
        int a, b, c;
        a = map(mini, 0, 4096, 0, 125);
        b = map(maxi, 0, 4000, 0, 125);
        c = b - a;
        clearLine(2);
        display.drawRect(5, SCROLL_TOP + 2 * LINE_HEIGHT +5, 102, LINE_HEIGHT-8);
        display.drawRect(30, SCROLL_TOP + 2 * LINE_HEIGHT +5, 52, LINE_HEIGHT-8);
        display.fillRect(a, SCROLL_TOP + 2 * LINE_HEIGHT + 7 , c, LINE_HEIGHT -11);
        display.display();
    } // end while
    keyOut(false,  true, 698, 0);                                  /// stop keying
    keyTx = true;
}





