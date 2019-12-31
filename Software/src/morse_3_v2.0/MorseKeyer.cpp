#include "MorseKeyer.h"
#include "MorseMachine.h"
#include "MorsePreferences.h"
#include "MorseGenerator.h"

using namespace MorseKeyer;


namespace MorseKeyer::internal {

    void updatePaddleLatch(boolean dit, boolean dah);
    void clearPaddleLatches ();
    void setDITstate();
    void setDAHstate();


}


void updateTimings() {
  ditLength = 1200 / MorsePreferences::prefs.wpm;                    // set new value for length of dits and dahs and other timings
  dahLength = 3 * ditLength;
  interCharacterSpace =  MorsePreferences::prefs.interCharSpace *  ditLength;
  //interWordSpace = _max(p_interWordSpace * ditLength, (p_interCharSpace+6)*ditLength);
  interWordSpace = _max(MorsePreferences::prefs.interWordSpace, MorsePreferences::prefs.interCharSpace+4) * ditLength;

  effWpm = 60000 / (31 * ditLength + 4 * interCharacterSpace + interWordSpace );  ///  effective wpm with lengthened spaces = Farnsworth speed
}


void keyTransmitter() {
  if (MorsePreferences::prefs.keyTrainerMode == 0 || MorseMachine::isMode(MorseMachine::echoTrainer) || MorseMachine::isMode(MorseMachine::loraTrx) )
      return;                              // we never key Tx under these conditions
  if (MorsePreferences::prefs.keyTrainerMode == 1 && MorseMachine::isMode(MorseMachine::morseGenerator))
      return;                              // we key only in keyer mode; in all other case we key TX
  if (MorseKeyer::keyTx == false)
      return;                              // do not key when input is from tone decoder
   digitalWrite(keyerPin, HIGH);           // turn the LED on, key transmitter, or whatever
}



///////////////////
// we use the paddle for iambic keying
/////

boolean MorseKeyer::doPaddleIambic (boolean dit, boolean dah) {
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
             if (MorseMachine::isMode(MorseMachine::loraTrx))    {                    // when in Trx mode
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
            internal::updatePaddleLatch(dit, dah);  // trigger the paddle latches
            if (MorseMachine::isMode(MorseMachine::echoTrainer))   {      // change the state of the trainer at end of word
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
            internal::clearPaddleLatches();                           // always clear the paddle latches at beginning of new element
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
            internal::clearPaddleLatches();                          // clear the paddle latches
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
          if ((MorseMachine::isMode(MorseMachine::echoTrainer) || MorseMachine::isMode(MorseMachine::loraTrx)) && MorsePreferences::prefs.echoToneShift != 0) {
             pitch = (MorsePreferences::prefs.echoToneShift == 1 ? pitch * 18 / 17 : pitch * 17 / 18);        /// one half tone higher or lower, as set in parameters in echo trainer mode
          }
           //pwmTone(pitch, MorsePreferences::prefs.sidetoneVolume, true);
           //keyTransmitter();
           MorseGenerator::keyOut(true, true, pitch, MorsePreferences::prefs.sidetoneVolume);
           ktimer += millis();                     // set ktimer to interval end time
           curtistimer += millis();                // set curtistimer to curtis end time
           keyerState = KEYED;                     // next state
           break;

    case KEYED:
                                                   // Wait for timers to expire
           if (millis() > ktimer) {                // are we at end of key down ?
               //digitalWrite(keyerPin, LOW);        // turn the LED off, unkey transmitter, or whatever
               //pwmNoTone();                      // stop side tone
               MorseGenerator::keyOut(false, true, 0, 0);
               ktimer = millis() + ditLength;    // inter-element time
               latencytimer = millis() + ((MorsePreferences::prefs.latency-1) * ditLength / 8);
               keyerState = INTER_ELEMENT;       // next state
            }
            else if (millis() > curtistimer ) {     // in Curtis mode we check paddle as soon as Curtis time is off
                 if (keyerControl & DIT_LAST)       // last element was a dit
                     internal::updatePaddleLatch(false, dah);  // not sure here: we only check the opposite paddle - should be ok for Curtis B
                 else
                     internal::updatePaddleLatch(dit, false);
                 // updatePaddleLatch(dit, dah);       // but we remain in the same state until element time is off!
            }
            break;

    case INTER_ELEMENT:
            //if ((MorsePreferences::prefs.keyermode != NONSQUEEZE) && (millis() < latencytimer)) {     // or should it be MorsePreferences::prefs.keyermode > 2 ? Latency for Ultimatic mode?
            if (millis() < latencytimer) {
              if (keyerControl & DIT_LAST)       // last element was a dit
                  internal::updatePaddleLatch(false, dah);  // not sure here: we only check the opposite paddle - should be ok for Curtis B
              else
                  internal::updatePaddleLatch(dit, false);
              // updatePaddleLatch(dit, dah);
            }
            else {
                internal::updatePaddleLatch(dit, dah);          // latch paddle state while between elements
                if (millis() > ktimer) {               // at end of INTER-ELEMENT
                    switch(keyerControl) {
                          case 3:                                         // both paddles are latched
                          case 7:
                                  switch (MorsePreferences::prefs.keyermode) {
                                      case NONSQUEEZE:  if (DIT_FIRST)                      // when first element was a DIT
                                                            internal::setDITstate();            // next element is a DIT again
                                                        else                                // but when first element was a DAH
                                                            internal::setDAHstate();            // the next element is a DAH again!
                                                        break;
                                      case ULTIMATIC:   if (DIT_FIRST)                      // when first element was a DIT
                                                            internal::setDAHstate();            // next element is a DAH
                                                        else                                // but when first element was a DAH
                                                            internal::setDITstate();            // the next element is a DIT!
                                                        break;
                                      default:          if (keyerControl & DIT_LAST)     // Iambic: last element was a dit - this is case 7, really
                                                            internal::setDAHstate();               // next element will be a DAH
                                                        else                                // and this is case 3 - last element was a DAH
                                                            internal::setDITstate();               // the next element is a DIT
                                   }
                                   break;
                                                                          // dit only is latched, regardless what was last element
                          case 1:
                          case 5:
                                   internal::setDITstate();
                                   break;
                                                                          // dah only latched, regardless what was last element
                          case 2:
                          case 6:
                                  internal::setDAHstate();
                                   break;
                                                                          // none latched, regardless what was last element
                          case 0:
                          case 4:
                                   keyerState = IDLE_STATE;               // we are at the end of the character and go back into IDLE STATE
                                   displayMorse();                        // display the decoded morse character(s)
                                   if (MorseMachine::isMode(MorseMachine::loraTrx))
                                      cwForLora(0);

                                   MorsePreferences::fireCharSeen(false);

                                   if (MorsePreferences::prefs.ACSlength > 0)
                                        acsTimer = millis() + MorsePreferences::prefs.ACSlength * ditLength; // prime the ACS timer
                                   if (MorseMachine::isMode(MorseMachine::morseKeyer) || MorseMachine::isMode(MorseMachine::loraTrx) || MorseMachine::isMode(MorseMachine::morseTrx))
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


boolean MorseKeyer::checkPaddles() {
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
void MorseKeyer::internal::updatePaddleLatch(boolean dit, boolean dah)
{
    if (dit)
      keyerControl |= DIT_L;
    if (dah)
      keyerControl |= DAH_L;
}

// clear the paddle latches in keyer control
void MorseKeyer::internal::clearPaddleLatches ()
{
    keyerControl &= ~(DIT_L + DAH_L);   // clear both paddle latch bits
}

// functions to set DIT and DAH keyer states
void MorseKeyer::internal::setDITstate() {
  keyerState = DIT;
  treeptr = CWtree[treeptr].dit;
  if (MorseMachine::isMode(MorseMachine::loraTrx))
      cwForLora(1);                         // build compressed string for LoRA
}

void MorseKeyer::internal::setDAHstate() {
  keyerState = DAH;
  treeptr = CWtree[treeptr].dah;
  if (MorseMachine::isMode(MorseMachine::loraTrx))
      cwForLora(2);
}

