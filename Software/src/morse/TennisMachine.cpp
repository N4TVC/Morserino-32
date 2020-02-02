/*
 * TennisMachine.cpp
 *
 *  Created on: 28.01.2020
 *      Author: mj
 */
#include "TennisMachine.h"

void TennisMachine::start()
{
    switchToState(&stateInitial);
}

void TennisMachine::loop()
{
    currentState->loop();
}

void TennisMachine::onMessageTransmit(WordBuffer &message)
{
    currentState->onMessageTransmit(message);
    MORSELOGLN("TM::oMT");
}

void TennisMachine::onMessageReceive(String message)
{
    currentState->onMessageReceive(message);
}

void TennisMachine::switchToState(State *newState)
{
    if (currentState != 0)
    {
        currentState->onLeave();
        delay(1000);
    }
    newState->onEnter();
    delay(1000);
    currentState = newState;
}

const char* TennisMachine::getState() {
    return currentState->getName();
}

/**
 * We call this to send a message.
 */
void TennisMachine::send(String message)
{
    MORSELOGLN("TM:send() 1 ");
    client.send(message);
    MORSELOGLN("TM:send() 2");
}

void TennisMachine::print(String message)
{
    MORSELOGLN("TM:print() 1 " );
    try {
    client.print(message);
    }
    catch (std::exception &e) {
        MORSELOGLN("TM:print() 2 " + String(e.what()));
    }
    MORSELOGLN("TM:print() 2");
}

/*****************************************************************************
 *
 *  State: INITIAL
 */
const char* TennisMachine::StateInitial::getName() {
    return "initial";
}

void TennisMachine::StateInitial::onEnter()
{
    MORSELOGLN("TM:SI:oE machine: " + String((unsigned long) machine));
    machine->print("Initial entered\n");

}

void TennisMachine::StateInitial::onLeave()
{
    machine->print("Initial left\n");

}

void TennisMachine::StateInitial::onMessageReceive(String message)
{
    machine->print("Initial received " + message + "\n");
}

void TennisMachine::StateInitial::onMessageTransmit(WordBuffer &message)
{
    MORSELOGLN("message: " + message.get());
    if (message >= "cq")
    {
        MORSELOGLN("message 2: " + message.get());
        machine->print("Initial sent cq - off to end state\n");
        machine->send(message.getAndClear());
        machine->switchToState(&machine->stateEnd);
    }
    else
    {
        MORSELOGLN("message 3: " + message.get());
        machine->print("Send cq to continue!\n");
    }
    MORSELOGLN("message 4: " + message.get());
}

/*****************************************************************************
 *
 *  State: INVITE RECEIVED
 */
const char* TennisMachine::StateInviteReceived::getName() {
    return "InviteReceived";
}

void TennisMachine::StateInviteReceived::onEnter()
{
    MORSELOGLN("TM:SI:oE machine: " + String((unsigned long) machine));
    machine->print("StateInviteReceived entered\n");

}

void TennisMachine::StateInviteReceived::onLeave()
{
    machine->print("StateInviteReceived left\n");

}

void TennisMachine::StateInviteReceived::onMessageReceive(String message)
{
    machine->print("StateInviteReceived received " + message + "\n");
}

void TennisMachine::StateInviteReceived::onMessageTransmit(WordBuffer &message)
{
    MORSELOGLN("message: " + message.get());
    if (message >= "cq")
    {
        MORSELOGLN("message 2: " + message.get());
        machine->print("Initial sent cq - off to end state\n");
        machine->send(message.getAndClear());
        machine->switchToState(&machine->stateEnd);
    }
    else
    {
        MORSELOGLN("message 3: " + message.get());
        machine->print("Send cq to continue!\n");
    }
    MORSELOGLN("message 4: " + message.get());
}


/*****************************************************************************
 *
 *  State: END
 */

const char* TennisMachine::StateEnd::getName() {
    return "end";
}

void TennisMachine::StateEnd::onEnter()
{
    machine->print("End - send k on Serial to restart!\n");
}

void TennisMachine::StateEnd::onMessageReceive(String message)
{
    machine->print("End received " + message + "\n");
    if (message == "k")
    {
        machine->switchToState(&machine->stateInitial);
    }
}

void TennisMachine::StateEnd::onMessageTransmit(WordBuffer &message)
{
}
