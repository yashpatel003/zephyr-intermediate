
#include <errno.h>
#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include "sensor_fake.h"
#include "temp_alarm.h"

#define DEFAULT_THRESHOLD 30

static void before(void *fixture)
{
    ARG_UNUSED(fixture);

    sensor_fake_reset();
    sensor_fake_init();

    zassume_ok(temp_alarm_init(&fake_sensor_dev, DEFAULT_THRESHOLD),
               "precondition: temp_alarm_init must succeed");
}

ZTEST_SUITE(temp_alarm_sensor_errors, NULL, NULL, before, NULL, NULL);

/* Task 2: fetch failure must prevent channel_get from being called. */
ZTEST(temp_alarm_sensor_errors, test_fetch_error_propagates)
{
    struct temp_alarm_status status;

    fake_sensor_sample_fetch_fake.return_val = -EIO;

    zassert_equal(temp_alarm_check(), -EIO,
                  "fetch error should propagate");

    zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1,
                  "fetch should be called once");
    zassert_equal(fake_sensor_channel_get_fake.call_count, 0,
                  "channel_get must not run after fetch failure");

    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");
    zassert_equal(status.last_error, -EIO,
                  "last_error should record fetch failure");
}

/* Custom fake to simulate a channel_get error. */
static int channel_get_error(const struct device *dev,
                             enum sensor_channel chan,
                             struct sensor_value *val)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(chan);
    ARG_UNUSED(val);

    return -EIO;
}

ZTEST(temp_alarm_sensor_errors, test_channel_get_error_propagates)
{
    struct temp_alarm_status status;

    fake_sensor_channel_get_fake.custom_fake = channel_get_error;

    zassert_equal(temp_alarm_check(), -EIO,
                  "channel_get error should propagate");

    zassert_equal(fake_sensor_sample_fetch_fake.call_count, 1,
                  "sample should be fetched once");
    zassert_equal(fake_sensor_channel_get_fake.call_count, 1,
                  "channel_get should be called once");

    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");
    zassert_equal(status.last_error, -EIO,
                  "last_error should record channel_get failure");
}

ZTEST(temp_alarm_sensor_errors, test_fail_then_recover)
{
    struct temp_alarm_status status;
    static int fetch_results[] = {-EIO, 0};

    SET_RETURN_SEQ(fake_sensor_sample_fetch, fetch_results,
                   ARRAY_SIZE(fetch_results));

    sensor_fake_set_temperature(20);

    zassert_equal(temp_alarm_check(), -EIO,
                  "first check should fail");
    zassert_ok(temp_alarm_check(),
               "second check should recover");

    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");

    zassert_equal(status.last_temp, 20,
                  "successful recovery should update temperature");
    zassert_false(status.is_alarming,
                  "20 C should not trigger alarm");
    zassert_equal(status.last_error, -EIO,
                  "last_error remains until reset");
    zassert_equal(fake_sensor_sample_fetch_fake.call_count, 2,
                  "fetch should be called twice");
}

/* Invalid-input tests */
ZTEST_SUITE(temp_alarm_invalid_input, NULL, NULL, before, NULL, NULL);

ZTEST(temp_alarm_invalid_input, test_get_status_null)
{
    zassert_equal(temp_alarm_get_status(NULL), -EINVAL,
                  "NULL status should return -EINVAL");
}

ZTEST(temp_alarm_invalid_input, test_init_without_device)
{
    zassert_equal(temp_alarm_init(NULL, DEFAULT_THRESHOLD), -ENODEV,
                  "NULL sensor device should return -ENODEV");
}

ZTEST(temp_alarm_invalid_input, test_init_threshold_out_of_range)
{
    zassert_equal(temp_alarm_init(&fake_sensor_dev, -1), -EINVAL,
                  "negative threshold should be rejected");
    zassert_equal(temp_alarm_init(&fake_sensor_dev, 101), -EINVAL,
                  "threshold above maximum should be rejected");
}

ZTEST(temp_alarm_invalid_input, test_set_threshold_out_of_range)
{
    struct temp_alarm_status status;

    zassert_equal(temp_alarm_set_threshold(-1), -EINVAL,
                  "negative threshold should be rejected");
    zassert_equal(temp_alarm_set_threshold(101), -EINVAL,
                  "threshold above maximum should be rejected");

    /* Prove the previous threshold (30 C) remains active. */
    sensor_fake_set_temperature(31);
    zassert_ok(temp_alarm_check(), "check should succeed");
    zassert_ok(temp_alarm_get_status(&status), "get status should succeed");

    zassert_true(status.is_alarming,
                 "31 C should alarm if threshold remains 30 C");
    zassert_equal(status.last_temp, 31,
                  "last temperature should be 31");
}
