#include "CppUTest/TestHarness.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

extern "C" {
#include "mcuco_args.h"
}

/* args_parse_mcuco wants a mutable word vector, so the words are copied into
 * storage that outlives the call. Twelve leaves room past the longest line the
 * CLI accepts, which is the eight words of "irq bind rising B 5 high A 0". */
#define MAX_WORDS     12
#define MAX_WORD_SIZE 32

static char word_storage[MAX_WORDS][MAX_WORD_SIZE];
static char *words[MAX_WORDS];

/* Takes a command line the way a user would type it, minus the program name. */
static int parse(const char *line, mcuco_args_t *mcuco_args)
{
    char buffer[128];
    (void)snprintf(buffer, sizeof(buffer), "%s", line);

    int word_count = 0;
    for (char *word = strtok(buffer, " "); word != NULL && word_count < MAX_WORDS; word = strtok(NULL, " "))
    {
        (void)snprintf(word_storage[word_count], sizeof(word_storage[word_count]), "%s", word);
        words[word_count] = word_storage[word_count];
        word_count++;
    }

    return args_parse_mcuco(word_count, words, mcuco_args);
}

static bool rejected(const char *line)
{
    mcuco_args_t mcuco_args = {};

    return parse(line, &mcuco_args) != 0;
}

static void expect_command(const mcuco_args_t &mcuco_args, const char *subsystem, const char *verb)
{
    CHECK(mcuco_args.command != NULL);
    STRCMP_EQUAL(subsystem, mcuco_args.command->subsystem);
    STRCMP_EQUAL(verb, mcuco_args.command->verb);
}

TEST_GROUP(McucoArgs){};

/* --- args_parse_mcuco --- */

TEST(McucoArgs, McuVerbsTakeNoValues)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("mcu probe", &mcuco_args));
    expect_command(mcuco_args, "mcu", "probe");

    LONGS_EQUAL(0, parse("mcu reset", &mcuco_args));
    expect_command(mcuco_args, "mcu", "reset");
}

TEST(McucoArgs, GpioCfgReachesDirectionPortAndPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("gpio cfg output A 5", &mcuco_args));
    expect_command(mcuco_args, "gpio", "cfg");
    LONGS_EQUAL(DIR_OUTPUT, mcuco_args.direction);
    LONGS_EQUAL(PORT_A, mcuco_args.port);
    LONGS_EQUAL(5, mcuco_args.pin);

    LONGS_EQUAL(0, parse("gpio cfg input B 0", &mcuco_args));
    LONGS_EQUAL(DIR_INPUT, mcuco_args.direction);
}

/* The value comes before the pin, matching mcuco_gpio_set(mcu, level, port, pin)
 * and the payload byte order. */
TEST(McucoArgs, GpioSetReachesLevelPortAndPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("gpio set high A 5", &mcuco_args));
    expect_command(mcuco_args, "gpio", "set");
    LONGS_EQUAL(LEVEL_HIGH, mcuco_args.level);
    LONGS_EQUAL(PORT_A, mcuco_args.port);
    LONGS_EQUAL(5, mcuco_args.pin);

    LONGS_EQUAL(0, parse("gpio set low C 13", &mcuco_args));
    LONGS_EQUAL(LEVEL_LOW, mcuco_args.level);
    LONGS_EQUAL(PORT_C, mcuco_args.port);
    LONGS_EQUAL(13, mcuco_args.pin);
}

TEST(McucoArgs, GpioGetAndToggleTakeOnlyAPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("gpio get C 13", &mcuco_args));
    expect_command(mcuco_args, "gpio", "get");
    LONGS_EQUAL(PORT_C, mcuco_args.port);
    LONGS_EQUAL(13, mcuco_args.pin);

    LONGS_EQUAL(0, parse("gpio toggle G 15", &mcuco_args));
    expect_command(mcuco_args, "gpio", "toggle");
    LONGS_EQUAL(PORT_G, mcuco_args.port);
    LONGS_EQUAL(15, mcuco_args.pin);
}

TEST(McucoArgs, EveryPortLetterMapsInOrder)
{
    static const char *const LINES[] = {"gpio get A 0", "gpio get B 0", "gpio get C 0", "gpio get D 0",
                                        "gpio get E 0", "gpio get F 0", "gpio get G 0"};

    for (size_t index = 0; index < sizeof(LINES) / sizeof(LINES[0]); index++)
    {
        mcuco_args_t mcuco_args = {};

        LONGS_EQUAL_TEXT(0, parse(LINES[index], &mcuco_args), LINES[index]);
        LONGS_EQUAL_TEXT((long)index, (long)mcuco_args.port, LINES[index]);
    }
}

TEST(McucoArgs, BothEndsOfThePinRangeAreAccepted)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("gpio get A 0", &mcuco_args));
    LONGS_EQUAL(0, mcuco_args.pin);

    LONGS_EQUAL(0, parse("gpio get A 15", &mcuco_args));
    LONGS_EQUAL(15, mcuco_args.pin);
}

TEST(McucoArgs, NoWordsAtAllIsRejected)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(-EINVAL, parse("", &mcuco_args));
}

TEST(McucoArgs, AnUnknownSubsystemOrVerbIsRejected)
{
    CHECK_TRUE(rejected("spi cfg output A 5"));
    CHECK_TRUE(rejected("gpio explode A 5"));
    CHECK_TRUE(rejected("mcu probe A 5"));
}

/* The word count is what decides whether a line is well formed, so one word
 * either way is a rejection rather than something to ignore. */
TEST(McucoArgs, TheWrongNumberOfWordsIsRejected)
{
    CHECK_TRUE(rejected("gpio"));
    CHECK_TRUE(rejected("gpio set"));
    CHECK_TRUE(rejected("gpio set high A"));
    CHECK_TRUE(rejected("gpio set high A 5 extra"));
    CHECK_TRUE(rejected("gpio get A"));
    CHECK_TRUE(rejected("gpio get A 5 5"));
    CHECK_TRUE(rejected("mcu"));
    CHECK_TRUE(rejected("mcu probe extra"));
}

TEST(McucoArgs, AnUnknownValueWordIsRejected)
{
    CHECK_TRUE(rejected("gpio cfg sideways A 5"));
    CHECK_TRUE(rejected("gpio set hgih A 5"));
}

/* The port is one capital letter on its own: "A5" is the old spelling and "a"
 * is not accepted, so neither can be mistaken for a valid port. */
TEST(McucoArgs, ThePortMustBeOneCapitalLetterFromAToG)
{
    CHECK_TRUE(rejected("gpio get H 0"));
    CHECK_TRUE(rejected("gpio get a 5"));
    CHECK_TRUE(rejected("gpio get A5 5"));
    CHECK_TRUE(rejected("gpio get AA 5"));
    CHECK_TRUE(rejected("gpio get 5 5"));
}

TEST(McucoArgs, ThePinMustBeZeroToFifteen)
{
    CHECK_TRUE(rejected("gpio get A 16"));
    CHECK_TRUE(rejected("gpio get A -1"));
    CHECK_TRUE(rejected("gpio get A 5x"));
    CHECK_TRUE(rejected("gpio get A x"));
    CHECK_TRUE(rejected("gpio get A 99999999999999999999"));
}

/* --- assign_timer_system --- */

TEST(McucoArgs, TimerGetReachesTheTimerIndex)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("timer get 2", &mcuco_args));
    expect_command(mcuco_args, "timer", "get");
    LONGS_EQUAL(2, mcuco_args.timer);
}

TEST(McucoArgs, ATimerIndexPastTwoIsRejectedAndLeavesTheIndexUntouched)
{
    mcuco_args_t mcuco_args       = {};
    mcuco_args.timer = 9;

    CHECK_TRUE(parse("timer get 3", &mcuco_args) != 0);
    LONGS_EQUAL(9, mcuco_args.timer);
}

/* Frequency first, then the timer: the order of
 * mcuco_pwm_group_cfg(mcu, freq_hz, group) and of the [FREQ_LE32, GROUP] payload. */
TEST(McucoArgs, TimerCfgReachesTheFrequencyThenTheTimerIndex)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("timer cfg 1000 2", &mcuco_args));
    expect_command(mcuco_args, "timer", "cfg");
    LONGS_EQUAL(1000, mcuco_args.frequency_hz);
    LONGS_EQUAL(2, mcuco_args.timer);
}

/* A suffix is in Plans.md's Deferred list, so "1k" is not 1000 yet. */
TEST(McucoArgs, AFrequencyWithTrailingJunkIsRejected)
{
    CHECK_TRUE(rejected("timer cfg 1k 0"));
    CHECK_TRUE(rejected("timer cfg 1000x 0"));
}

/* Zero is a range error, not shorthand for teardown - that is timer release. */
TEST(McucoArgs, AFrequencyOfZeroIsRejected)
{
    CHECK_TRUE(rejected("timer cfg 0 0"));
}

TEST(McucoArgs, AFrequencyPastAMegahertzIsRejected)
{
    CHECK_TRUE(rejected("timer cfg 1000001 0"));
    CHECK_TRUE(rejected("timer cfg 99999999999999999999 0"));
}

TEST(McucoArgs, TimerReleaseReachesTheTimerIndex)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("timer release 1", &mcuco_args));
    expect_command(mcuco_args, "timer", "release");
    LONGS_EQUAL(1, mcuco_args.timer);
}

TEST(McucoArgs, TheWrongNumberOfWordsIsRejectedForEveryTimerVerb)
{
    CHECK_TRUE(rejected("timer cfg 1000"));
    CHECK_TRUE(rejected("timer cfg 1000 0 extra"));
    CHECK_TRUE(rejected("timer get"));
    CHECK_TRUE(rejected("timer get 0 0"));
    CHECK_TRUE(rejected("timer release"));
    CHECK_TRUE(rejected("timer release 0 0"));
    CHECK_TRUE(rejected("timer"));
}

TEST(McucoArgs, AVerbFromAnotherSubsystemIsRejected)
{
    CHECK_TRUE(rejected("timer toggle 0"));
    CHECK_TRUE(rejected("timer set 1000 0"));
    CHECK_TRUE(rejected("timer probe"));
    CHECK_TRUE(rejected("timer bind 0"));
}

/* The frequency is checked before the timer index, so a bad index still has to
 * fail on its own account rather than riding on the frequency's check. */
TEST(McucoArgs, ABadTimerIndexIsRejectedEvenWithAValidFrequency)
{
    CHECK_TRUE(rejected("timer cfg 1000 3"));
    CHECK_TRUE(rejected("timer cfg 1000 -1"));
    CHECK_TRUE(rejected("timer cfg 1000 x"));
}

/* --- pwm --- */

TEST(McucoArgs, PwmGetReachesThePortAndPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("pwm get C 13", &mcuco_args));
    expect_command(mcuco_args, "pwm", "get");
    LONGS_EQUAL(PORT_C, mcuco_args.port);
    LONGS_EQUAL(13, mcuco_args.pin);
}

TEST(McucoArgs, PwmReleaseReachesThePortAndPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("pwm release B 9", &mcuco_args));
    expect_command(mcuco_args, "pwm", "release");
    LONGS_EQUAL(PORT_B, mcuco_args.port);
    LONGS_EQUAL(9, mcuco_args.pin);
}

TEST(McucoArgs, PwmCfgReachesThePolarityPortAndPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("pwm cfg active-low A 5", &mcuco_args));
    expect_command(mcuco_args, "pwm", "cfg");
    LONGS_EQUAL(POL_ACTIVE_LOW, mcuco_args.polarity);
    LONGS_EQUAL(PORT_A, mcuco_args.port);
    LONGS_EQUAL(5, mcuco_args.pin);
}

TEST(McucoArgs, PwmCfgAcceptsActiveHigh)
{
    mcuco_args_t mcuco_args = {};
    mcuco_args.polarity     = POL_ACTIVE_LOW;

    LONGS_EQUAL(0, parse("pwm cfg active-high A 5", &mcuco_args));
    LONGS_EQUAL(POL_ACTIVE_HIGH, mcuco_args.polarity);
}

/* The protocol's POL column spells these high|low, but those are level words
 * here: "pwm cfg high A 5" would read like "gpio set high A 5" and mean
 * something else entirely. */
TEST(McucoArgs, PolarityMustBeSpelledOutInFull)
{
    CHECK_TRUE(rejected("pwm cfg high A 5"));
    CHECK_TRUE(rejected("pwm cfg low A 5"));
    CHECK_TRUE(rejected("pwm cfg ACTIVE-LOW A 5"));
    CHECK_TRUE(rejected("pwm cfg active A 5"));
}

/* Percent on the command line, tenths of a percent on the wire. */
TEST(McucoArgs, PwmSetReachesTheDutyInTenthsThenThePortAndPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("pwm set 25 A 5", &mcuco_args));
    expect_command(mcuco_args, "pwm", "set");
    LONGS_EQUAL(250, mcuco_args.duty_tenths);
    LONGS_EQUAL(PORT_A, mcuco_args.port);
    LONGS_EQUAL(5, mcuco_args.pin);
}

TEST(McucoArgs, ADutyWithTrailingJunkIsRejected)
{
    CHECK_TRUE(rejected("pwm set 25x A 5"));
}

TEST(McucoArgs, ADutyPastOneHundredPercentIsRejected)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("pwm set 100 A 5", &mcuco_args));
    LONGS_EQUAL(1000, mcuco_args.duty_tenths);

    CHECK_TRUE(rejected("pwm set 101 A 5"));
    CHECK_TRUE(rejected("pwm set 99999999999999999999 A 5"));
}

/* strtoul skips leading space, takes a sign, and reads an empty word as 0 - and
 * 0 is a valid duty. The same digit-first rule as parse_pin keeps all three out. */
TEST(McucoArgs, ADutyMustStartWithADigit)
{
    CHECK_TRUE(rejected("pwm set +25 A 5"));
    CHECK_TRUE(rejected("pwm set -1 A 5"));

    char subsystem[] = "pwm";
    char verb[]      = "set";
    char duty[]      = "";
    char port[]      = "A";
    char pin[]       = "5";
    char *line[]     = {subsystem, verb, duty, port, pin};

    mcuco_args_t mcuco_args = {};
    CHECK_TRUE(args_parse_mcuco(5, line, &mcuco_args) != 0);
}

TEST(McucoArgs, AZeroDutyIsAcceptedAndADecimalIsNot)
{
    mcuco_args_t mcuco_args = {};
    mcuco_args.duty_tenths  = 999;

    LONGS_EQUAL(0, parse("pwm set 0 A 5", &mcuco_args));
    LONGS_EQUAL(0, mcuco_args.duty_tenths);

    /* One decimal place is deferred; until then 25.5 is refused, not truncated. */
    CHECK_TRUE(rejected("pwm set 25.5 A 5"));
}

/* Every value is required, so a missing one is a short line, not a default. */
TEST(McucoArgs, TheWrongNumberOfWordsIsRejectedForEveryPwmVerb)
{
    CHECK_TRUE(rejected("pwm cfg A 5"));
    CHECK_TRUE(rejected("pwm cfg active-high A 5 5"));
    CHECK_TRUE(rejected("pwm set A 5"));
    CHECK_TRUE(rejected("pwm set 25 A 5 5"));
    CHECK_TRUE(rejected("pwm get A"));
    CHECK_TRUE(rejected("pwm get A 5 5"));
    CHECK_TRUE(rejected("pwm release A"));
    CHECK_TRUE(rejected("pwm release A 5 5"));
    CHECK_TRUE(rejected("pwm"));
}

/* Which timer drives a pin is the MCU's business, so no pwm command takes one. */
TEST(McucoArgs, NoPwmCommandTakesATimer)
{
    CHECK_TRUE(rejected("pwm set 25 A 5 0"));
    CHECK_TRUE(rejected("pwm cfg active-high A 5 0"));
}

TEST(McucoArgs, AVerbPwmDoesNotHaveIsRejected)
{
    CHECK_TRUE(rejected("pwm toggle A 5"));
    CHECK_TRUE(rejected("pwm bind 25 A 5"));
    CHECK_TRUE(rejected("pwm probe"));
}

/* --- irq --- */

TEST(McucoArgs, IrqUnbindReachesThePortAndPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("irq unbind B 5", &mcuco_args));
    expect_command(mcuco_args, "irq", "unbind");
    LONGS_EQUAL(PORT_B, mcuco_args.port);
    LONGS_EQUAL(5, mcuco_args.pin);
}

TEST(McucoArgs, IrqCfgReachesTheEdgePortAndPin)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("irq cfg rising B 5", &mcuco_args));
    expect_command(mcuco_args, "irq", "cfg");
    LONGS_EQUAL(EDGE_RISING, mcuco_args.edge);
    LONGS_EQUAL(PORT_B, mcuco_args.port);
    LONGS_EQUAL(5, mcuco_args.pin);
}

TEST(McucoArgs, IrqCfgAcceptsFallingAndBoth)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("irq cfg falling B 5", &mcuco_args));
    LONGS_EQUAL(EDGE_FALLING, mcuco_args.edge);

    LONGS_EQUAL(0, parse("irq cfg both B 5", &mcuco_args));
    LONGS_EQUAL(EDGE_BOTH, mcuco_args.edge);
}

/* off is the disarm: it clears the pin's trigger and any binding on it. */
TEST(McucoArgs, IrqCfgAcceptsOff)
{
    mcuco_args_t mcuco_args = {};
    mcuco_args.edge         = EDGE_BOTH;

    LONGS_EQUAL(0, parse("irq cfg off B 5", &mcuco_args));
    LONGS_EQUAL(EDGE_OFF, mcuco_args.edge);
}

/* The one command with two pins. The output pin has its own word kinds so it
 * lands in out_port / out_pin rather than overwriting the trigger pin - the only
 * place positional order can silently do the wrong thing. */
TEST(McucoArgs, IrqBindKeepsTheTriggerPinAndTheOutputPinApart)
{
    mcuco_args_t mcuco_args = {};

    LONGS_EQUAL(0, parse("irq bind rising B 5 high C 7", &mcuco_args));
    expect_command(mcuco_args, "irq", "bind");
    LONGS_EQUAL(EDGE_RISING, mcuco_args.edge);
    LONGS_EQUAL(PORT_B, mcuco_args.port);
    LONGS_EQUAL(5, mcuco_args.pin);
    LONGS_EQUAL(ACTION_HIGH, mcuco_args.action);
    LONGS_EQUAL(PORT_C, mcuco_args.out_port);
    LONGS_EQUAL(7, mcuco_args.out_pin);
}

TEST(McucoArgs, IrqBindAcceptsLowAndToggle)
{
    mcuco_args_t mcuco_args = {};
    mcuco_args.action       = ACTION_HIGH;

    LONGS_EQUAL(0, parse("irq bind falling B 5 low C 7", &mcuco_args));
    LONGS_EQUAL(ACTION_LOW, mcuco_args.action);

    LONGS_EQUAL(0, parse("irq bind both B 5 toggle C 7", &mcuco_args));
    LONGS_EQUAL(ACTION_TOGGLE, mcuco_args.action);
}

/* EXTI cannot report which edge fired, so a binding has to name a real one. off
 * is only meaningful to cfg, where it disarms the pin. */
TEST(McucoArgs, IrqBindRejectsOffTheEdgeCfgAccepts)
{
    CHECK_TRUE(rejected("irq bind off B 5 high C 7"));
}

TEST(McucoArgs, AnEdgeMustBeOneOfTheFourWords)
{
    CHECK_TRUE(rejected("irq cfg RISING B 5"));
    CHECK_TRUE(rejected("irq cfg rise B 5"));
    CHECK_TRUE(rejected("irq cfg none B 5"));
    CHECK_TRUE(rejected("irq bind RISING B 5 high C 7"));
}

TEST(McucoArgs, AnActionMustBeOneOfTheThreeWords)
{
    CHECK_TRUE(rejected("irq bind rising B 5 on C 7"));
    CHECK_TRUE(rejected("irq bind rising B 5 HIGH C 7"));
    CHECK_TRUE(rejected("irq bind rising B 5 flip C 7"));
}

/* Each of bind's two pins is checked on its own account. */
TEST(McucoArgs, ABadPinOnEitherSideOfABindIsRejected)
{
    CHECK_TRUE(rejected("irq bind rising H 5 high C 7"));
    CHECK_TRUE(rejected("irq bind rising B 16 high C 7"));
    CHECK_TRUE(rejected("irq bind rising B 5 high H 7"));
    CHECK_TRUE(rejected("irq bind rising B 5 high C 16"));
}

TEST(McucoArgs, TheWrongNumberOfWordsIsRejectedForEveryIrqVerb)
{
    CHECK_TRUE(rejected("irq cfg rising B"));
    CHECK_TRUE(rejected("irq cfg rising B 5 5"));
    CHECK_TRUE(rejected("irq bind rising B 5 high C"));
    CHECK_TRUE(rejected("irq bind rising B 5 high C 7 7"));
    CHECK_TRUE(rejected("irq unbind B"));
    CHECK_TRUE(rejected("irq unbind B 5 5"));
    CHECK_TRUE(rejected("irq"));
}

TEST(McucoArgs, AVerbIrqDoesNotHaveIsRejected)
{
    CHECK_TRUE(rejected("irq set high B 5"));
    CHECK_TRUE(rejected("irq get B 5"));
    CHECK_TRUE(rejected("irq release B 5"));
}

/* --- the whole table --- */

/* One example per row. A row whose value_count disagrees with its value list
 * cannot parse its own example, so this is what makes a wrong count impossible
 * to miss. A new row needs a new line here. */
TEST(McucoArgs, EveryCommandInTheTableParsesFromAnExample)
{
    static const char *const EXAMPLES[] = {
        "mcu probe",
        "mcu reset",
        "gpio cfg output A 5",
        "gpio set high A 5",
        "gpio get A 5",
        "gpio toggle A 5",
        "timer cfg 1000 0",
        "timer get 0",
        "timer release 0",
        "pwm cfg active-high A 5",
        "pwm set 25 A 5",
        "pwm get A 5",
        "pwm release A 5",
        "irq cfg rising B 5",
        "irq bind rising B 5 toggle A 0",
        "irq unbind B 5",
    };

    for (size_t index = 0; index < sizeof(EXAMPLES) / sizeof(EXAMPLES[0]); index++)
    {
        mcuco_args_t mcuco_args = {};

        LONGS_EQUAL_TEXT(0, parse(EXAMPLES[index], &mcuco_args), EXAMPLES[index]);
    }
}
