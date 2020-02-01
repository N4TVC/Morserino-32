/*
 * WordBuffer.h
 *
 *  Created on: 30.01.2020
 *      Author: mj
 */

#ifndef WORDBUFFER_H_
#define WORDBUFFER_H_

#include "arduino.h"

class WordBuffer
{
    public:
        WordBuffer();
        ~WordBuffer() = default;
        void addWord(String word);
        void addChar(String c);
        void endWord();
        String getAndClear();
        String get();
        boolean operator==(String other);
        boolean operator>=(String other);
        void handleWordEnd();

    private:
        String buffer;
        boolean wordEnd;

};

#endif /* WORDBUFFER_H_ */
