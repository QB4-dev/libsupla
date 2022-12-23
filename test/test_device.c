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

void test_device_functions(void)
{
	char buf[32] = {};

	TEST_ASSERT_NOT_NULL(dev);

	TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_dev_get_name(dev,buf,sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING_MESSAGE(DEV_NAME,buf,"wrong device name");

	TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_dev_get_software_version(dev,buf,sizeof(buf)));
	TEST_ASSERT_EQUAL_STRING_MESSAGE(SOFT_VER,buf,"wrong software version");

}

void test_device_config(void)
{
	const struct supla_config supla_config = {
		.email = "user@example.com",
		.auth_key = {0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
		.guid = {0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00},
		.server = "svr.supla.org",
	};

	TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_dev_set_config(dev,&supla_config));
}

#endif // TEST
