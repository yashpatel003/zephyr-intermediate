/*
 * temp_alarm - Happy-path unit tests (homework skeleton)
 *
 * test_status_after_init is provided as a worked example. Finish Task 0 first,
 * then fill in the Task 1 stubs according to TEST_SPEC.md. The stubs call
 * ztest_test_skip(), so the binary builds and runs cleanly until each test is
 * implemented.
 *
 * Run:
 *   west twister -T tests/temp_alarm -p native_sim
 */

#include <errno.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "sensor_fake.h"
#include "temp_alarm.h"

#define DEFAULT_THRESHOLD 30

/*
 * Shared by every suite: a clean fake, then the module initialised
 * with the fake device. temp_alarm_init() resets all module state,
 * so no deinit is needed between tests.
 */
static void before(void *fixture)
{
	ARG_UNUSED(fixture);

	sensor_fake_reset();
	sensor_fake_init();

	zassume_ok(temp_alarm_init(&fake_sensor_dev, DEFAULT_THRESHOLD),
		   "precondition: temp_alarm_init must succeed");
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_get_status
 *
 * The status right after a successful init.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_get_status, NULL, NULL, before, NULL, NULL);

/* PROVIDED — study this test before writing the rest. */
ZTEST(temp_alarm_get_status, test_status_after_init)
{
	struct temp_alarm_status status;

	zassert_ok(temp_alarm_get_status(&status), "get_status must succeed");

	zassert_equal(status.last_temp, 0, "last_temp must be 0 after init, got %d",
		      status.last_temp);
	zassert_equal(status.alarm_count, 0U, "alarm_count must be 0 after init, got %u",
		      status.alarm_count);
	zassert_false(status.is_alarming, "is_alarming must be false after init");
	zassert_equal(status.last_error, 0, "last_error must be 0 after init, got %d",
		      status.last_error);
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_check
 *
 * Readings below, at and above the threshold.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_check, NULL, NULL, before, NULL, NULL);


ZTEST(temp_alarm_check, test_below_threshold)
{
	/* TODO(l8-task1): a reading of 20 C is below the threshold.
	 * Check the status, and how the module talked to the sensor.
	 * See TEST_SPEC.md "Suite temp_alarm_check" #1.
	 */
	struct temp_alarm_status status;

    sensor_fake_set_temperature(20);
    zassert_ok(temp_alarm_check(), "check should succeed");
    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");

    zassert_equal(status.last_temp, 20, "last temperature should be 20");
    zassert_false(status.is_alarming, "20 C should not trigger alarm");

    zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1,
                  "sample fetch should be called once");
    zassert_equal(fake_sensor_sample_fetch_fake.arg1_val,
                  SENSOR_CHAN_AMBIENT_TEMP,
                  "fetch should request ambient temperature");

    zassert_equal(fake_sensor_channel_get_fake.call_count, 1,
                  "channel get should be called once");
    zassert_equal(fake_sensor_channel_get_fake.arg1_val,
                  SENSOR_CHAN_AMBIENT_TEMP,
                  "channel get should request ambient temperature");
}

ZTEST(temp_alarm_check, test_at_threshold)
{
	/* TODO(l8-task1): a reading equal to the threshold (30 C) is not an alarm.
	 * See TEST_SPEC.md "Suite temp_alarm_check" #2.
	 */
	struct temp_alarm_status status;

    sensor_fake_set_temperature(30);
    zassert_ok(temp_alarm_check(), "check should succeed");
    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");

    zassert_false(status.is_alarming,
                  "temperature equal to threshold should not alarm");
    zassert_equal(status.alarm_count, 0U,
                  "alarm count should remain zero");
}

ZTEST(temp_alarm_check, test_above_threshold)
{
	/* TODO(l8-task1): a reading of 40 C raises the alarm and counts it.
	 * See TEST_SPEC.md "Suite temp_alarm_check" #3.
	 */
    struct temp_alarm_status status;

    sensor_fake_set_temperature(40);
    zassert_ok(temp_alarm_check(), "check should succeed");
    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");

    zassert_equal(status.last_temp, 40, "last temperature should be 40");
    zassert_true(status.is_alarming, "40 C should trigger alarm");
    zassert_equal(status.alarm_count, 1U, "alarm count should be one");
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_threshold
 *
 * Changing the threshold at run time.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_threshold, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_threshold, test_new_threshold_clears_alarm)
{
	/* TODO(l8-task1): raise the alarm, then move the threshold
	 * above the reading. The alarm must clear and stay cleared.
	 * See TEST_SPEC.md "Suite temp_alarm_threshold" #1.
	 */
	struct temp_alarm_status status;

    sensor_fake_set_temperature(40);
    zassert_ok(temp_alarm_check(), "first check should succeed");

    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");
    zassert_true(status.is_alarming, "40 C should trigger alarm");

    zassert_ok(temp_alarm_set_threshold(50),
               "setting threshold to 50 should succeed");

    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");
    zassert_false(status.is_alarming,
                  "changing threshold should clear alarm immediately");

    zassert_ok(temp_alarm_check(), "second check should succeed");
    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");

    zassert_false(status.is_alarming,
                  "40 C should be below the new threshold");
    zassert_equal(status.alarm_count, 1U,
                  "alarm count should remain one");
}

/*
 * ============================================================================
 * Test Suite: temp_alarm_reset
 *
 * temp_alarm_reset() clears the alarm but keeps the last reading.
 * ============================================================================
 */
ZTEST_SUITE(temp_alarm_reset, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_reset, test_reset_clears_state)
{
	/* TODO(l8-task1): raise the alarm three times, then reset.
	 * The alarm state must clear, but the last reading must stay.
	 * See TEST_SPEC.md "Suite temp_alarm_reset" #1.
	 */
	struct temp_alarm_status status;

    sensor_fake_set_temperature(40);

    zassert_ok(temp_alarm_check(), "check 1 should succeed");
    zassert_ok(temp_alarm_check(), "check 2 should succeed");
    zassert_ok(temp_alarm_check(), "check 3 should succeed");

    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");
    zassert_equal(status.alarm_count, 3U,
                  "three alarming checks should count as three");
    zassert_true(status.is_alarming, "alarm should be active");

    temp_alarm_reset();

    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");

    zassert_false(status.is_alarming, "reset should clear alarm flag");
    zassert_equal(status.alarm_count, 0U,
                  "reset should clear alarm count");
    zassert_equal(status.last_error, 0,
                  "reset should clear last error");
    zassert_equal(status.last_temp, 40,
                  "reset should preserve last temperature");
}
