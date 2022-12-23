//#define TEST
#ifdef TEST

#include "unity.h"

#include <libsupla/channel.h>

static supla_channel_t *temp_channel;

void setUp(void)
{
	supla_channel_config_t temp_channel_config = {
		.type = SUPLA_CHANNELTYPE_THERMOMETER ,
		.supported_functions = SUPLA_CHANNELFNC_THERMOMETER,
		.default_function = SUPLA_CHANNELFNC_THERMOMETER,
	};

	temp_channel = supla_channel_create(&temp_channel_config);
	TEST_ASSERT_NOT_NULL(temp_channel);
}

void tearDown(void)
{
	TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_channel_free(temp_channel));
}

void test_channel_get_assigned_number(void)
{
	TEST_ASSERT_EQUAL_INT(-1,supla_channel_get_assigned_number(temp_channel));
}

void test_channel_get_active_function(void)
{
	int active_functions = 0x00;

	TEST_ASSERT_EQUAL_INT(SUPLA_RESULT_TRUE,supla_channel_get_active_function(temp_channel,&active_functions));
}

#endif // TEST
