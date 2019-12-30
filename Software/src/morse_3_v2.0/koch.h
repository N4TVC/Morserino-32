#ifndef KOCH_H
#define KOCH_H

/// Koch stuff
///////////////////////////////////////////////////

#include <Arduino.h>

namespace Koch
{

    String kochChars;

    void setup();
    void updateKochChars(boolean lcwoKochSeq);

    uint8_t wordIsKoch(String thisWord);
    String getChar(uint8_t maxKochLevel);
}

#endif
