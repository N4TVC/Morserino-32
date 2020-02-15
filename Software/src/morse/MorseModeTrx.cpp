#include "MorseModeTrx.h"

#include "MorseKeyer.h"
#include "decoder.h"
#include "MorseMachine.h"
#include "MorseDisplay.h"
#include "MorseInput.h"

MorseModeTrx morseModeTrx;

boolean MorseModeTrx::menuExec(String mode)
{
    MorseMachine::morseState = MorseMachine::morseTrx;
    MorseDisplay::clear();
    MorseDisplay::printOnScroll(1, REGULAR, 0, "Start CW Trx");
    delay(650);
    MorseDisplay::getKeyerModeSymbol = MorseDisplay::getKeyerModeSymbolWStraightKey;
    MorseDisplay::clear();
    MorseDisplay::displayTopLine();
    MorseDisplay::drawInputStatus(false);
    MorseDisplay::printToScroll(REGULAR, "");      // clear the buffer

    MorseDisplay::displayCWspeed();
    MorseDisplay::displayVolume();

    MorseKeyer::keyTx = true;
    MorseInput::start([](String s) {
        MorseDisplay::printToScroll(FONT_OUTGOING, s);
    }, [](){
        MorseDisplay::printToScroll(FONT_OUTGOING, " ");
    });
    Decoder::startDecoder();
    Decoder::onCharacter = [](String s)
    {
        MorseDisplay::printToScroll(FONT_INCOMING, s);
    };
    Decoder::onWordEnd = []()
    {
        MorseDisplay::printToScroll(FONT_INCOMING, " ");
    };
    return true;
}

boolean MorseModeTrx::loop()
{
    if (MorseInput::doInput())
    {
        return true;                                                        // we are busy keying and so need a very tight loop !
    }
    Decoder::doDecodeShow();
    return false;
}

void MorseModeTrx::onPreferencesChanged()
{
}

boolean MorseModeTrx::togglePause()
{
    return false;
}
