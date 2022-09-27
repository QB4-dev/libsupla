#ifdef TEST

#include "unity.h"

#include "device.h"

static supla_dev_t *dev;

void setUp(void)
{
    dev = supla_dev_create(NULL,NULL);
}

void tearDown(void)
{
    supla_dev_free(dev);
}

void test_device_init(void)
{
    TEST_ASSERT_NOT_NULL(dev);
}

#endif // TEST
