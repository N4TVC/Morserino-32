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

void test_TennisMachine_1()
{
    printf("Testing TennisMachine 1\n");
    TennisMachine sut = createSUT();
    WordBuffer buf;

    sut.start();
    assertEquals("1", "StateInitial", sut.getState());

    buf.addWord("cq");
    sut.onMessageTransmit(buf);
    assertEquals("2", "StateInitial", sut.getState());

    buf.addWord("cq de xx0yyy");
    sut.onMessageTransmit(buf);
    assertEquals("3", "StateInviteSent", sut.getState());

    sut.onMessageReceive("xx0yyy de xx1dx");
    assertEquals("4", "StateInviteAccepted", sut.getState());

    buf.getAndClear();
    buf.addWord("xx1dx de xx0yyy");
    sut.onMessageTransmit(buf);
    assertEquals("5", "StateStartRoundSender", sut.getState());

    buf.addWord("hallo hello");
    sut.onMessageTransmit(buf);
    // Failed to key same word twice - so stay on curent state
    assertEquals("6", "StateStartRoundSender", sut.getState());

    buf.addWord("hallo hallo");
    sut.onMessageTransmit(buf);
    // Managed to key same word twice - advance to next state
    assertEquals("7", "StateWaitForAnswer", sut.getState());

    sut.onMessageReceive("hellm");
    assertEquals("8", "StateStartRoundReceiver", sut.getState());
    assertTrue("9 points us", 0 == sut.getGameState().us.points);
    assertTrue("10 points dx", 0 == sut.getGameState().dx.points)
    ;
    sut.onMessageReceive("foo");
    assertEquals("11", "StateChallengeReceived", sut.getState());
    assertTrue("12 points us", 0 == sut.getGameState().us.points);
    assertTrue("13 points dx", 0 == sut.getGameState().dx.points);

    buf.addWord("foo");
    sut.onMessageTransmit(buf);
    // Failed to key same word twice - so stay on curent state
    assertEquals("14", "StateStartRoundSender", sut.getState());
    assertTrue("15 points us", 1 == sut.getGameState().us.points);
    assertTrue("16 points dx", 0 == sut.getGameState().dx.points);

    buf.addWord("bar bar");
    sut.onMessageTransmit(buf);
    // Managed to key same word twice - advance to next state
    assertEquals("17", "StateWaitForAnswer", sut.getState());

    sut.onMessageReceive("bar");
    assertEquals("18", "StateStartRoundReceiver", sut.getState());
    assertTrue("19 points us", 1 == sut.getGameState().us.points);
    assertTrue("20 points dx", 1 == sut.getGameState().dx.points);

    buf.addWord("<sk>");
    sut.onMessageTransmit(buf);
    // Managed to key same word twice - advance to next state
    assertEquals("18", "StateEnd", sut.getState());

    WordBuffer wordBuffer = WordBuffer("<ka>");
    sut.onMessageTransmit(wordBuffer);
    assertEquals("19", "StateInitial", sut.getState());

}

void test_TennisMachine_2()
{
    printf("Testing TennisMachine 2\n");
    TennisMachine sut = createSUT();
    WordBuffer buf;

    sut.start();
    assertEquals("1", "StateInitial", sut.getState());

    buf.addWord("cq");
    sut.onMessageTransmit(buf);
    assertEquals("2", "StateInitial", sut.getState());

    buf.addWord("cq de xx0yyy");
    sut.onMessageTransmit(buf);
    assertEquals("3", "StateInviteSent", sut.getState());

    sut.onMessageReceive("xx0yyy de xx1dx");
    assertEquals("4", "StateInviteAccepted", sut.getState());

    buf.getAndClear();
    buf.addWord("xx1dx de xx0yyy");
    sut.onMessageTransmit(buf);
    assertEquals("5", "StateStartRoundSender", sut.getState());

    sut.onMessageReceive("<sk>");
    assertEquals("6", "StateEnd", sut.getState());

    WordBuffer wordBuffer = WordBuffer("<ka>");
    sut.onMessageTransmit(wordBuffer);
    assertEquals("7", "StateInitial", sut.getState());


}

void test_TennisMachine()
{
    printf("Testing TennisMachine\n");

    test_TennisMachine_1();
    test_TennisMachine_2();
}
