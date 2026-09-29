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

TEST(McucoSpy, WritesTheCannedFrequencyOnlyWhenItAnswersOk)
{
    uint32_t achieved_hz = 0;

    mcuco_spy()->next_frequency_hz = 999;
    LONGS_EQUAL(STATUS_OK, mcuco_pwm_group_get(mcuco_spy_handle(), 2, &achieved_hz));
    LONGS_EQUAL(999, achieved_hz);
    LONGS_EQUAL(2, mcuco_spy()->group);

    achieved_hz              = 0;
    mcuco_spy()->next_status = STATUS_ERR_NOT_INIT;
    LONGS_EQUAL(STATUS_ERR_NOT_INIT, mcuco_pwm_group_get(mcuco_spy_handle(), 2, &achieved_hz));
    LONGS_EQUAL(0, achieved_hz);
}

TEST(McucoSpy, WritesTheCannedDutyOnlyWhenItAnswersOk)
{
    uint16_t duty = 0;

    mcuco_spy()->next_duty = 250;
    LONGS_EQUAL(STATUS_OK, mcuco_pwm_channel_get(mcuco_spy_handle(), PORT_B, 9, &duty));
    LONGS_EQUAL(250, duty);
    LONGS_EQUAL(PORT_B, mcuco_spy()->port);
    LONGS_EQUAL(9, mcuco_spy()->pin);

    duty                     = 0;
    mcuco_spy()->next_status = STATUS_ERR_NOT_INIT;
    LONGS_EQUAL(STATUS_ERR_NOT_INIT, mcuco_pwm_channel_get(mcuco_spy_handle(), PORT_B, 9, &duty));
    LONGS_EQUAL(0, duty);
}

TEST(McucoSpy, RecordsBothPinsOfABindSeparately)
{
    (void)mcuco_gpio_irq_bind(mcuco_spy_handle(), EDGE_FALLING, PORT_B, 5, ACTION_TOGGLE, PORT_C, 7);

    LONGS_EQUAL(CALL_IRQ_BIND, mcuco_spy()->last_call);
    LONGS_EQUAL(EDGE_FALLING, mcuco_spy()->edge);
    LONGS_EQUAL(PORT_B, mcuco_spy()->port);
    LONGS_EQUAL(5, mcuco_spy()->pin);
    LONGS_EQUAL(ACTION_TOGGLE, mcuco_spy()->action);
    LONGS_EQUAL(PORT_C, mcuco_spy()->out_port);
    LONGS_EQUAL(7, mcuco_spy()->out_pin);
}
