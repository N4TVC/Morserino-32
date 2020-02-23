#include "TestSupport.h"

std::vector<const char*> failedFrameworkTests;

void logFailure(const char* msg)
{
    failedFrameworkTests.push_back(msg);
    printf("%s -- %lu\n", msg, failedFrameworkTests.size());
}

void test_TestSupport_aE1()
{
    failedTests.clear();
    assertEquals("equals m1", "a", "a");
    if (failedTests.size() != 0)
    {
        logFailure("FAILED: assertEquals(m1, const char*, const char*)");
    }
}

void test_TestSupport_aE2()
{
    printf("equals m2 is supposed to fail!\n");
    failedTests.clear();
    assertEquals("equals m2", "a", "b");
    if (failedTests.size() != 1)
    {
        logFailure("FAILED: assertEquals(m2, const char*, const char*)");
    }
}

void test_TestSupport_aE3()
{
    failedTests.clear();
    assertEquals("equals m3", true, true);
    if (failedTests.size() != 0)
    {
        logFailure("FAILED: assertEquals(m3, bool, bool)");
    }
}

void test_TestSupport_aE4()
{
    printf("equals m4 is supposed to fail!\n");
    failedTests.clear();
    assertEquals("equals m4", true, false);
    if (failedTests.size() != 1)
    {
        logFailure("FAILED: assertEquals(m4, bool, bool)");
    }
}

void test_TestSupport_aE5()
{
    printf("equals m5 is supposed to fail!\n");
    failedTests.clear();
    uint16_t a = 1;
    uint16_t b = 2;
    assertEquals("equals m5", a, b);
    if (failedTests.size() != 1)
    {
        logFailure("FAILED: assertEquals(m5, uint16_t, uint16_t)");
    }
}

void test_String1()
{
    String a = String("a b c");
    String b = String("b");
    String x = String("x");
    a.replace(b, x);
    if (a != "a x c")
    {
        logFailure("FAILED: String.replace() 1");
    }
}

void test_String2()
{
    String a = String("a b c");
    String x = String("x");
    a.replace("b", x);
    if (a != "a x c")
    {
        logFailure("FAILED: String.replace() 2");
    }
}

void test_String3()
{
    String a = String("a b c");
    a.replace("b", "x");
    if (a != "a x c")
    {
        logFailure("FAILED: String.replace() 3");
    }
}

void test_TestSupport_1()
{
    test_TestSupport_aE1();
    test_TestSupport_aE2();
    test_TestSupport_aE3();
    test_TestSupport_aE4();
    test_TestSupport_aE5();
    test_String1();
    test_String2();
    test_String3();
}
