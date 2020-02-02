/*
 * TennisMachineTest.cpp
 *
 *  Created on: 02.02.2020
 *      Author: mj
 */

#include "TestSupport.h"
#include "TennisMachine.h"

#define TESTPR(m,v) printf(m, v)

TennisMachine createSUT() {
    TennisMachine sut;
    TennisMachine::Client client;
    client.print = [](String m)
    {   TESTPR("DISPLAY: '%s'\n", m.c_str());};
    client.printReceivedMessage = [](String m)
    {   TESTPR("DISPLAY: '< %s'\n", m.c_str());};
    client.send = [](String m)
    {   TESTPR("> '%s'\n", m.c_str());};
    sut.setClient(client);
    return sut;
}

void test_TennisMachine()
{
    printf("Testing TennisMachine\n");

    TennisMachine sut = createSUT();
    WordBuffer buf;

    sut.start();
    assertEquals("StateInitial 1", "StateInitial", sut.getState());

    buf.addWord("cq");
    sut.onMessageTransmit(buf);
    assertEquals("StateInitial 2", "StateInitial", sut.getState());

    buf.addWord("cq");
    buf.addWord("de");
    buf.addWord("XX0YYY");
    sut.onMessageTransmit(buf);
    assertEquals("StateInviteSent", "StateInviteSent", sut.getState());

    sut.onMessageReceive("XX0YYY de XX1aaa");
    assertEquals("StateInviteAccepted", "StateInviteAccepted", sut.getState());
}
