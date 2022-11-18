//#define TEST
#ifdef TEST

#include "unity.h"

#include <libsupla/channel.h>

static supla_channel_t test_channel = {
	.type = SUPLA_CHANNELTYPE_THERMOMETER ,
	.supported_functions = SUPLA_CHANNELFNC_THERMOMETER ,
	.default_function = SUPLA_CHANNELFNC_THERMOMETER,
};


void setUp(void)
{
	TEST_ASSERT_NULL(test_channel.priv);
	TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_channel_init(&test_channel));
}

void tearDown(void)
{
	TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_channel_deinit(&test_channel));
	TEST_ASSERT_NULL(test_channel.priv);
}

void test_channel_init(void)
{
	supla_channel_t channel = {
		.type = SUPLA_CHANNELTYPE_THERMOMETER ,
		.supported_functions = SUPLA_CHANNELFNC_THERMOMETER ,
		.default_function = SUPLA_CHANNELFNC_THERMOMETER,
	};

    TEST_ASSERT_NULL(channel.priv);
    TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_channel_init(&channel));
    TEST_ASSERT_EQUAL(SUPLA_RESULT_TRUE,supla_channel_deinit(&channel));
    TEST_ASSERT_NULL(channel.priv);
}

void test_channel_get_assigned_number(void)
{
	TEST_ASSERT_EQUAL_INT(-1,supla_channel_get_assigned_number(&test_channel));
}

void test_channel_get_active_function(void)
{
	int active_functions = 0x00;

	TEST_ASSERT_EQUAL_INT(SUPLA_RESULT_FALSE,supla_channel_get_active_function(NULL,0x00));
	TEST_ASSERT_EQUAL_INT(SUPLA_RESULT_TRUE,supla_channel_get_active_function(&test_channel,&active_functions));
}

#endif // TEST
