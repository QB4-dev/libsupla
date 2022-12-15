#ifdef TEST

#include "unity.h"

#include <libsupla/device.h>


#define DEV_NAME "TEST device"
#define SOFT_VER "ver test"

static supla_dev_t *dev;

void setUp(void)
{
    dev = supla_dev_create(DEV_NAME,SOFT_VER);
}

void tearDown(void)
{
    supla_dev_free(dev);
}

void test_device_init(void)
{
    TEST_ASSERT_NOT_NULL(dev);
}

void test_device_functions(void)
{
	char buf[32] = {};

    TEST_ASSERT_NOT_NULL(dev);

    TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_dev_get_name(dev,buf,sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING_MESSAGE(DEV_NAME,buf,"wrong device name");

    TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_dev_get_software_version(dev,buf,sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING_MESSAGE(SOFT_VER,buf,"wrong software version");

}

#endif // TEST
