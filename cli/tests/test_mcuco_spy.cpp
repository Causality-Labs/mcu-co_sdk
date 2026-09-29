#include "CppUTest/TestHarness.h"

extern "C" {
#include "mcuco_spy.h"
}

/* The spy is test infrastructure, so it gets its own checks: a double that
 * quietly records the wrong thing makes every test that uses it a lie. */
TEST_GROUP(McucoSpy)
{
    void setup()
    {
        mcuco_spy_reset();
    }
};

TEST(McucoSpy, StartsWithNothingRecordedAndAnswersOkByDefault)
{
    LONGS_EQUAL(0, mcuco_spy()->calls);
    LONGS_EQUAL(CALL_NONE, mcuco_spy()->last_call);
    LONGS_EQUAL(STATUS_OK, mcuco_spy()->next_status);
    LONGS_EQUAL(LEVEL_LOW, mcuco_spy()->next_level);
}

TEST(McucoSpy, RecordsWhichCallWasMadeAndWhatItWasHanded)
{
    LONGS_EQUAL(STATUS_OK, mcuco_gpio_set(mcuco_spy_handle(), LEVEL_HIGH, PORT_C, 13));

    LONGS_EQUAL(1, mcuco_spy()->calls);
    LONGS_EQUAL(CALL_GPIO_SET, mcuco_spy()->last_call);
    LONGS_EQUAL(LEVEL_HIGH, mcuco_spy()->level);
    LONGS_EQUAL(PORT_C, mcuco_spy()->port);
    LONGS_EQUAL(13, mcuco_spy()->pin);
    POINTERS_EQUAL(mcuco_spy_handle(), (void *)mcuco_spy()->handle);
}

/* One counter across every function, so a test can say "exactly one library
 * call" or "none at all" without naming which. */
TEST(McucoSpy, CountsEveryCallTogether)
{
    (void)mcuco_probe(mcuco_spy_handle());
    (void)mcuco_reset(mcuco_spy_handle());
    (void)mcuco_gpio_cfg(mcuco_spy_handle(), DIR_INPUT, PORT_A, 0);

    LONGS_EQUAL(3, mcuco_spy()->calls);
    LONGS_EQUAL(CALL_GPIO_CFG, mcuco_spy()->last_call);
}

TEST(McucoSpy, HandsBackTheStatusTheTestAsked)
{
    mcuco_spy()->next_status = STATUS_ERR_BUSY;

    LONGS_EQUAL(STATUS_ERR_BUSY, mcuco_probe(mcuco_spy_handle()));
}

TEST(McucoSpy, WritesTheCannedLevelOnlyWhenItAnswersOk)
{
    level_t level = LEVEL_LOW;

    mcuco_spy()->next_level = LEVEL_HIGH;
    LONGS_EQUAL(STATUS_OK, mcuco_gpio_get(mcuco_spy_handle(), PORT_A, 5, &level));
    LONGS_EQUAL(LEVEL_HIGH, level);

    /* Matches the real library's contract: untouched unless STATUS_OK. */
    level                    = LEVEL_LOW;
    mcuco_spy()->next_status = STATUS_ERR_INVALID_STATE;
    LONGS_EQUAL(STATUS_ERR_INVALID_STATE, mcuco_gpio_get(mcuco_spy_handle(), PORT_A, 5, &level));
    LONGS_EQUAL(LEVEL_LOW, level);
}

TEST(McucoSpy, ResetForgetsEverything)
{
    (void)mcuco_probe(mcuco_spy_handle());
    mcuco_spy()->next_status = STATUS_ERR_BUSY;

    mcuco_spy_reset();

    LONGS_EQUAL(0, mcuco_spy()->calls);
    LONGS_EQUAL(CALL_NONE, mcuco_spy()->last_call);
    LONGS_EQUAL(STATUS_OK, mcuco_spy()->next_status);
}
