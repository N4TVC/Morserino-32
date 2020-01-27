#ifndef MORSEKEYER_H_
#define MORSEKEYER_H_

#include <Arduino.h>

// defines for keyer modi
//

namespace MorseKeyer
{

    struct OnWordEndHandler
    {
            virtual ~OnWordEndHandler() = default;
            virtual boolean handle() = 0;
    };

#define    IAMBICA      1
// Curtis Mode A
#define    IAMBICB      2
// Curtis Mode B (with enhanced Curtis timing, set as parameter
#define    ULTIMATIC    3
// Ultimatic mode
#define    NONSQUEEZE   4
// Non-squeeze mode of dual-lever paddles - simulate a single-lever paddle

///////////////////////////////////////////////////////////////////////////////
//
//  Iambic Keyer State Machine Defines

    enum KSTYPE
    {
        IDLE_STATE, DIT, DAH, KEY_START, KEYED, INTER_ELEMENT
    };

//  keyerControl bit definitions

#define     DIT_L      0x01     // Dit latch
#define     DAH_L      0x02     // Dah latch
#define     DIT_LAST   0x04     // Dit was last processed element

//  Global Keyer Variables
//
    extern unsigned char keyerControl; // this holds the latches for the paddles and the DIT_LAST latch, see above

    extern boolean DIT_FIRST; // first latched was dit?
    extern unsigned int ditLength;        // dit length in milliseconds - 100ms = 60bpm = 12 wpm
    extern unsigned int dahLength;        // dahs are 3 dits long
    extern KSTYPE keyerState;
    extern uint8_t sensor;                 // what we read from checking the touch sensors
    extern boolean leftKey, rightKey;
    extern unsigned int interCharacterSpace;
    extern unsigned int interWordSpace;   // need to be properly initialised!
    extern unsigned int effWpm;                                // calculated effective speed in WpM

    extern boolean keyTx;             // we use this to decide if Tx should be keyed or not

    extern boolean (*onWordEnd)();
    extern void (*onWordEndDitDah)();
    extern void (*onWordEndNDitDah)();

    void setup();

    void updateTimings();
    void keyTransmitter();
    void unkeyTransmitter();
    boolean doPaddleIambic();
    boolean checkPaddles();
    void clearPaddleLatches();
    void changeSpeed(int t);
}

#endif /* MORSEKEYER_H_ */
