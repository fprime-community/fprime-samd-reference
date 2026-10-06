// ======================================================================
// \title  FramTesterTestMain.cpp
// \brief  Unit test entry point for the CuriosityReference FramTester
// ======================================================================

#include "FramTesterTester.hpp"

TEST(Polling, Alternates) {
    CuriosityReference::FramTesterTester tester;
    tester.testPollAlternates();
}

TEST(Polling, FailureRewritesSameSeed) {
    CuriosityReference::FramTesterTester tester;
    tester.testPollFailureRewritesSameSeed();
}

TEST(Commands, RoundTrip) {
    CuriosityReference::FramTesterTester tester;
    tester.testCommandRoundTrip();
}

TEST(Commands, BusyAndInvalidArgument) {
    CuriosityReference::FramTesterTester tester;
    tester.testBusyAndInvalidArgument();
}

TEST(Commands, DriverErrorStatus) {
    CuriosityReference::FramTesterTester tester;
    tester.testDriverErrorStatus();
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
