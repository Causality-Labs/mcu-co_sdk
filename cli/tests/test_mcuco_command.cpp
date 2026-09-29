#include "CppUTest/TestHarness.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern "C" {
#include "mcuco_command.h"
#include "mcuco_spy.h"
}

TEST_GROUP(McucoCommand)
{
    int saved_stdout;
    FILE *captured;

    void setup()
    {
        mcuco_spy_reset();
        saved_stdout = -1;
        captured     = NULL;
    }

    void teardown()
    {
        if (saved_stdout >= 0)
        {
            stop_capture();
        }
    }

    /* Built from the table itself, so a test cannot name a command the CLI
     * does not have. */
    mcuco_args_t command(const char *subsystem, const char *verb)
    {
        mcuco_args_t mcuco_args = {};
        mcuco_args.command      = mcuco_find_command(subsystem, verb);
        CHECK(mcuco_args.command != NULL);

        mcuco_args.port = PORT_A;
        mcuco_args.pin  = 5;

        return mcuco_args;
    }

    /* mcuco_command prints a command's value to stdout, so the only way to see
     * it is to point stdout at a file for the duration of the call. */
    void start_capture()
    {
        fflush(stdout);
        saved_stdout = dup(STDOUT_FILENO);
        captured     = tmpfile();
        CHECK(captured != NULL);
        dup2(fileno(captured), STDOUT_FILENO);
    }

    void stop_capture(char *text = NULL, size_t size = 0)
    {
        fflush(stdout);
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
        saved_stdout = -1;

        rewind(captured);
        if (text != NULL)
        {
            size_t read_count = fread(text, 1, size - 1, captured);
            text[read_count]  = '\0';
        }
        fclose(captured);
        captured = NULL;
    }

    void expect_one_call(mcuco_call_t call)
    {
        LONGS_EQUAL(1, mcuco_spy()->calls);
        LONGS_EQUAL(call, mcuco_spy()->last_call);
        POINTERS_EQUAL(mcuco_spy_handle(), (void *)mcuco_spy()->handle);
    }
};

/* --- mcuco_find_command --- */

TEST(McucoCommand, FindsEachSubsystemAndVerbPair)
{
    const command_t *row = mcuco_find_command("gpio", "set");

    CHECK(row != NULL);
    STRCMP_EQUAL("gpio", row->subsystem);
    STRCMP_EQUAL("set", row->verb);
}

/* A verb only exists paired with the subsystems that have it. */
TEST(McucoCommand, AVerbUnderTheWrongSubsystemIsNotACommand)
{
    POINTERS_EQUAL(NULL, (void *)mcuco_find_command("mcu", "set"));
    POINTERS_EQUAL(NULL, (void *)mcuco_find_command("timer", "toggle"));
    POINTERS_EQUAL(NULL, (void *)mcuco_find_command("spi", "cfg"));
}

/* --- mcuco_run_command --- */

TEST(McucoCommand, ProbeCallsProbeOnceWithTheHandle)
{
    mcuco_args_t mcuco_args = command("mcu", "probe");

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_PROBE);
}

TEST(McucoCommand, ResetCallsResetOnce)
{
    mcuco_args_t mcuco_args = command("mcu", "reset");

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_RESET);
}

TEST(McucoCommand, GpioCfgHandsTheDirectionPortAndPinThrough)
{
    mcuco_args_t mcuco_args = command("gpio", "cfg");
    mcuco_args.direction    = DIR_OUTPUT;

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_GPIO_CFG);
    LONGS_EQUAL(DIR_OUTPUT, mcuco_spy()->direction);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

TEST(McucoCommand, GpioSetHandsTheLevelPortAndPinThrough)
{
    mcuco_args_t mcuco_args = command("gpio", "set");
    mcuco_args.level        = LEVEL_HIGH;

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_GPIO_SET);
    LONGS_EQUAL(LEVEL_HIGH, mcuco_spy()->level);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

/* stdout is the value and nothing else - no label, no trailing prose - so that
 * level=$(mcu-co-cli gpio get A 5) needs no parsing. */
TEST(McucoCommand, GpioGetPrintsTheLevelAndNothingElse)
{
    mcuco_args_t mcuco_args = command("gpio", "get");
    char printed[64]        = {0};

    mcuco_spy()->next_level = LEVEL_HIGH;

    start_capture();
    mcu_status_t status = mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
    stop_capture(printed, sizeof(printed));

    LONGS_EQUAL(STATUS_OK, status);
    STRCMP_EQUAL("high\n", printed);
    expect_one_call(CALL_GPIO_GET);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

TEST(McucoCommand, GpioToggleReportsTheLevelItReached)
{
    mcuco_args_t mcuco_args = command("gpio", "toggle");
    char printed[64]        = {0};

    mcuco_spy()->next_level = LEVEL_HIGH;

    start_capture();
    mcu_status_t status = mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
    stop_capture(printed, sizeof(printed));

    LONGS_EQUAL(STATUS_OK, status);
    STRCMP_EQUAL("high\n", printed);
    expect_one_call(CALL_GPIO_TOGGLE);
}

/* A refused read has no value to print, so stdout stays empty and only the
 * status carries the failure. */
TEST(McucoCommand, GpioGetPrintsNothingWhenTheMcuRefuses)
{
    mcuco_args_t mcuco_args = command("gpio", "get");
    char printed[64]        = {0};

    mcuco_spy()->next_status = STATUS_ERR_INVALID_STATE;

    start_capture();
    mcu_status_t status = mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
    stop_capture(printed, sizeof(printed));

    LONGS_EQUAL(STATUS_ERR_INVALID_STATE, status);
    STRCMP_EQUAL("", printed);
}

TEST(McucoCommand, TheLibrarysStatusIsReturnedUnchanged)
{
    mcuco_args_t mcuco_args = command("gpio", "set");

    mcuco_spy()->next_status = STATUS_ERR_BUSY;

    LONGS_EQUAL(STATUS_ERR_BUSY, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
}

/* Nothing matched, so nothing is called - by the time this runs the port is open,
 * and a stray call would put a frame on the wire. */
TEST(McucoCommand, NoMatchedCommandCallsNothing)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(STATUS_ERR_ARG, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    LONGS_EQUAL(0, mcuco_spy()->calls);
}

/* The timer rows exist so their words parse and --help lists them, but no
 * library call is wired to them yet. */
TEST(McucoCommand, TimerCommandsParseButAreNotWiredYet)
{
    mcuco_args_t mcuco_args = command("timer", "get");

    LONGS_EQUAL(STATUS_ERR_UNSUPPORTED, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    LONGS_EQUAL(0, mcuco_spy()->calls);
}

/* --- mcuco_print_commands --- */

/* One line per row, so a command in the table is a command in --help. */
TEST(McucoCommand, EveryCommandInTheTableIsListed)
{
    static const char *const LINES[] = {
        "  mcu    probe",   "         reset",   "  gpio   cfg",     "         set",
        "         get",     "         toggle",  "  timer  cfg",     "         release",
    };
    char printed[2048] = {0};

    start_capture();
    mcuco_print_commands(stdout);
    stop_capture(printed, sizeof(printed));

    for (size_t index = 0; index < sizeof(LINES) / sizeof(LINES[0]); index++)
    {
        CHECK_TEXT(strstr(printed, LINES[index]) != NULL, LINES[index]);
    }

    CHECK(strstr(printed, "  pwm    ") != NULL);
    CHECK(strstr(printed, "irq") == NULL);
}
