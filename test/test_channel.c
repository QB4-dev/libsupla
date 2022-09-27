#ifdef TEST

#include "unity.h"

#include "channel.h"

static supla_channel_t temp_channel = {
	.type = SUPLA_CHANNELTYPE_THERMOMETER ,
	.supported_functions = SUPLA_CHANNELFNC_THERMOMETER ,
	.default_function = SUPLA_CHANNELFNC_THERMOMETER,
};


void setUp(void)
{

}

void tearDown(void)
{
}

void test_channel_init(void)
{
    TEST_ASSERT_NULL(temp_channel.priv);
    TEST_ASSERT_EQUAL(0,supla_channel_init(&temp_channel));
    TEST_ASSERT_EQUAL(0,supla_channel_deinit(&temp_channel));
    TEST_ASSERT_NULL(temp_channel.priv);
}

#endif // TEST
