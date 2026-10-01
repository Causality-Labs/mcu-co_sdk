#include "CppUTest/TestHarness.h"

#include <string.h>

extern "C" {
#include "commands.h"
}

TEST_GROUP(Commands)
{
    const command_t *command = NULL;
    mcuco_args_t mcuco_args  = {};
};

/* --- args_parse_mcuco --- */

TEST(Commands, ParsesValueWordsIntoArgsAndSelectsTheMatchingRow)
{
    const char *words[] = {"gpio", "set", "high", "A", "9"};

    LONGS_EQUAL(0, args_parse_mcuco(5, (char **)words, &command, &mcuco_args));
    CHECK(command != NULL);
    STRCMP_EQUAL("gpio", command->subsystem);
    STRCMP_EQUAL("set", command->verb);
    LONGS_EQUAL(LEVEL_HIGH, mcuco_args.level);
    LONGS_EQUAL(PORT_A, mcuco_args.port);
    LONGS_EQUAL(9, mcuco_args.pin);
}

/* The trigger pin and the driven pin share kinds, so a mix-up would still parse. */
TEST(Commands, BindKeepsTheTriggerPinAndTheDrivenPinApart)
{
    const char *words[] = {"irq", "bind", "rising", "C", "13", "toggle", "A", "5"};

    LONGS_EQUAL(0, args_parse_mcuco(8, (char **)words, &command, &mcuco_args));
    LONGS_EQUAL(EDGE_RISING, mcuco_args.edge);
    LONGS_EQUAL(PORT_C, mcuco_args.port);
    LONGS_EQUAL(13, mcuco_args.pin);
    LONGS_EQUAL(ACTION_TOGGLE, mcuco_args.action);
    LONGS_EQUAL(PORT_A, mcuco_args.out_port);
    LONGS_EQUAL(5, mcuco_args.out_pin);
}

/* The CLI takes whole percent; the wire carries tenths. */
TEST(Commands, DutyIsConvertedFromPercentToTenths)
{
    const char *words[] = {"pwm", "set", "25", "A", "5"};

    LONGS_EQUAL(0, args_parse_mcuco(5, (char **)words, &command, &mcuco_args));
    LONGS_EQUAL(250, mcuco_args.duty_tenths);
}

TEST(Commands, RejectsAnUnknownCommand)
{
    const char *words[] = {"gpio", "bogus"};

    LONGS_EQUAL(-1, args_parse_mcuco(2, (char **)words, &command, &mcuco_args));
    POINTERS_EQUAL(NULL, command);
}

/* Guards the read of the verb word, not just the value count. */
TEST(Commands, RejectsALoneSubsystemWord)
{
    const char *words[] = {"gpio"};

    LONGS_EQUAL(-1, args_parse_mcuco(1, (char **)words, &command, &mcuco_args));
    POINTERS_EQUAL(NULL, command);
}

TEST(Commands, RejectsTooFewOrTooManyValueWords)
{
    const char *too_few[]  = {"gpio", "set", "high", "A"};
    const char *too_many[] = {"gpio", "set", "high", "A", "9", "extra"};

    LONGS_EQUAL(-1, args_parse_mcuco(4, (char **)too_few, &command, &mcuco_args));
    LONGS_EQUAL(-1, args_parse_mcuco(6, (char **)too_many, &command, &mcuco_args));
    POINTERS_EQUAL(NULL, command);
}

/* A failed parse must not hand back a row the caller could go on to run. */
TEST(Commands, OutOfRangeValueLeavesTheCommandUnset)
{
    const char *words[] = {"gpio", "set", "high", "A", "16"};

    LONGS_EQUAL(-1, args_parse_mcuco(5, (char **)words, &command, &mcuco_args));
    POINTERS_EQUAL(NULL, command);
}

/* --- mcuco_run_command --- */

TEST(Commands, RunWithoutACommandReturnsArgErrorWithoutTouchingTheLink)
{
    LONGS_EQUAL(STATUS_ERR_ARG, mcuco_run_command(NULL, NULL, &mcuco_args));
}
