#include "CppUTest/TestHarness.h"

#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern "C" {
#include "commands.h"
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
    CHECK(strstr(printed, "  irq    ") != NULL);
}

/* --- mcuco_run_command, timer --- */

TEST(McucoCommand, TimerCfgHandsTheFrequencyAndGroupThrough)
{
    mcuco_args_t mcuco_args = command("timer", "cfg");
    mcuco_args.frequency_hz = 1000;
    mcuco_args.timer        = 2;

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_PWM_GROUP_CFG);
    LONGS_EQUAL(1000, mcuco_spy()->frequency_hz);
    LONGS_EQUAL(2, mcuco_spy()->group);
}

TEST(McucoCommand, TimerReleaseHandsTheGroupThrough)
{
    mcuco_args_t mcuco_args = command("timer", "release");
    mcuco_args.timer        = 1;

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_PWM_GROUP_RELEASE);
    LONGS_EQUAL(1, mcuco_spy()->group);
}

/* The achieved frequency, not the requested one: integer prescaler division means
 * they differ, which is why the command exists. */
TEST(McucoCommand, TimerGetPrintsTheAchievedFrequencyAndNothingElse)
{
    mcuco_args_t mcuco_args = command("timer", "get");
    mcuco_args.timer        = 2;
    char printed[64]        = {0};

    mcuco_spy()->next_frequency_hz = 999;

    start_capture();
    mcu_status_t status = mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
    stop_capture(printed, sizeof(printed));

    LONGS_EQUAL(STATUS_OK, status);
    STRCMP_EQUAL("999\n", printed);
    expect_one_call(CALL_PWM_GROUP_GET);
    LONGS_EQUAL(2, mcuco_spy()->group);
}

TEST(McucoCommand, TimerGetPrintsNothingWhenTheMcuRefuses)
{
    mcuco_args_t mcuco_args = command("timer", "get");
    char printed[64]        = {0};

    mcuco_spy()->next_status = STATUS_ERR_NOT_INIT;

    start_capture();
    mcu_status_t status = mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
    stop_capture(printed, sizeof(printed));

    LONGS_EQUAL(STATUS_ERR_NOT_INIT, status);
    STRCMP_EQUAL("", printed);
}

/* --- mcuco_run_command, pwm --- */

TEST(McucoCommand, PwmCfgHandsThePolarityPortAndPinThrough)
{
    mcuco_args_t mcuco_args = command("pwm", "cfg");
    mcuco_args.polarity     = POL_ACTIVE_LOW;

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_PWM_CHANNEL_CFG);
    LONGS_EQUAL(POL_ACTIVE_LOW, mcuco_spy()->polarity);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

/* The CLI took percent; the library and the wire take tenths. */
TEST(McucoCommand, PwmSetHandsTheDutyInTenthsPortAndPinThrough)
{
    mcuco_args_t mcuco_args = command("pwm", "set");
    mcuco_args.duty_tenths  = 250;

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_PWM_CHANNEL_SET);
    LONGS_EQUAL(250, mcuco_spy()->duty);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

TEST(McucoCommand, PwmReleaseHandsThePortAndPinThrough)
{
    mcuco_args_t mcuco_args = command("pwm", "release");

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_PWM_CHANNEL_RELEASE);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

/* Tenths on the wire, percent on stdout, one decimal always - so 250 reads as
 * the "25.0" a user would type back into pwm set. */
TEST(McucoCommand, PwmGetPrintsTheDutyAsAPercentAndNothingElse)
{
    mcuco_args_t mcuco_args = command("pwm", "get");
    char printed[64]        = {0};

    mcuco_spy()->next_duty = 250;

    start_capture();
    mcu_status_t status = mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
    stop_capture(printed, sizeof(printed));

    LONGS_EQUAL(STATUS_OK, status);
    STRCMP_EQUAL("25.0\n", printed);
    expect_one_call(CALL_PWM_CHANNEL_GET);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

/* The CLI only sets whole percents, but the MCU holds tenths and something else
 * may have set one - printing 25.0 for 255 tenths would be a quiet lie. */
TEST(McucoCommand, PwmGetPrintsTheTenthItWasGiven)
{
    static const struct
    {
        uint16_t duty_tenths;
        const char *expected;
    } CASES[] = {{255, "25.5\n"}, {0, "0.0\n"}, {5, "0.5\n"}, {1000, "100.0\n"}};

    for (size_t index = 0; index < sizeof(CASES) / sizeof(CASES[0]); index++)
    {
        mcuco_args_t mcuco_args = command("pwm", "get");
        char printed[64]        = {0};

        mcuco_spy_reset();
        mcuco_spy()->next_duty = CASES[index].duty_tenths;

        start_capture();
        (void)mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
        stop_capture(printed, sizeof(printed));

        STRCMP_EQUAL(CASES[index].expected, printed);
    }
}

TEST(McucoCommand, PwmGetPrintsNothingWhenTheMcuRefuses)
{
    mcuco_args_t mcuco_args = command("pwm", "get");
    char printed[64]        = {0};

    mcuco_spy()->next_status = STATUS_ERR_NOT_INIT;

    start_capture();
    mcu_status_t status = mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
    stop_capture(printed, sizeof(printed));

    LONGS_EQUAL(STATUS_ERR_NOT_INIT, status);
    STRCMP_EQUAL("", printed);
}

/* --- mcuco_run_command, irq --- */

TEST(McucoCommand, IrqCfgHandsTheEdgePortAndPinThrough)
{
    mcuco_args_t mcuco_args = command("irq", "cfg");
    mcuco_args.edge         = EDGE_FALLING;

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_IRQ_CFG);
    LONGS_EQUAL(EDGE_FALLING, mcuco_spy()->edge);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

/* Two different pins, so a handler that swapped trigger and output would fail. */
TEST(McucoCommand, IrqBindHandsTheTriggerPinAndTheOutputPinThroughInOrder)
{
    mcuco_args_t mcuco_args = command("irq", "bind");
    mcuco_args.edge         = EDGE_RISING;
    mcuco_args.port         = PORT_B;
    mcuco_args.pin          = 5;
    mcuco_args.action       = ACTION_TOGGLE;
    mcuco_args.out_port     = PORT_C;
    mcuco_args.out_pin      = 7;

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_IRQ_BIND);
    LONGS_EQUAL(EDGE_RISING, mcuco_spy()->edge);
    LONGS_EQUAL(PORT_B, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
    LONGS_EQUAL(ACTION_TOGGLE, mcuco_spy()->action);
    LONGS_EQUAL(PORT_C, mcuco_spy()->out_port);
    LONGS_EQUAL(7, mcuco_spy()->out_pin);
}

TEST(McucoCommand, IrqUnbindHandsThePortAndPinThrough)
{
    mcuco_args_t mcuco_args = command("irq", "unbind");

    LONGS_EQUAL(STATUS_OK, mcuco_run_command(mcuco_spy_handle(), &mcuco_args));
    expect_one_call(CALL_IRQ_UNBIND);
    LONGS_EQUAL(PORT_A, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
}

/* --- the whole table --- */

/* Parse and run every command end to end. Each makes exactly one library call,
 * so no row is left calling nothing and none calls twice. */
TEST(McucoCommand, EveryCommandMakesExactlyOneLibraryCall)
{
    static const char *const EXAMPLES[] = {
        "mcu probe",       "mcu reset",          "gpio cfg output A 5",     "gpio set high A 5",
        "gpio get A 5",    "gpio toggle A 5",    "timer cfg 1000 0",        "timer get 0",
        "timer release 0", "pwm cfg active-high A 5", "pwm set 25 A 5",  "pwm get A 5",
        "pwm release A 5", "irq cfg rising B 5", "irq bind rising B 5 toggle A 0", "irq unbind B 5",
    };

    for (size_t index = 0; index < sizeof(EXAMPLES) / sizeof(EXAMPLES[0]); index++)
    {
        char buffer[64];
        char *words[12];
        int word_count = 0;

        (void)snprintf(buffer, sizeof(buffer), "%s", EXAMPLES[index]);
        for (char *word = strtok(buffer, " "); word != NULL; word = strtok(NULL, " "))
        {
            words[word_count++] = word;
        }

        mcuco_args_t mcuco_args = {};
        LONGS_EQUAL_TEXT(0, args_parse_mcuco(word_count, words, &mcuco_args), EXAMPLES[index]);

        mcuco_spy_reset();
        start_capture();
        mcu_status_t status = mcuco_run_command(mcuco_spy_handle(), &mcuco_args);
        stop_capture();

        LONGS_EQUAL_TEXT(STATUS_OK, status, EXAMPLES[index]);
        LONGS_EQUAL_TEXT(1, mcuco_spy()->calls, EXAMPLES[index]);
    }
}
