/*
 * Copyright (c) 2022 <qb4.dev@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "supla-value.h"
#include "port/arch.h"

void supla_val_init(supla_value_t *supla_val, char sync_onchange)
{
    memset(supla_val, 0, sizeof(supla_value_t));
    supla_val->sync = 1;
    supla_val->sync_onchange = sync_onchange;
}

void supla_val_free(supla_value_t *supla_val)
{
}

int supla_val_set(supla_value_t *supla_val, void *value, size_t len)
{
    if (!supla_val || len > SUPLA_CHANNELVALUE_SIZE)
        return SUPLA_RESULT_FALSE;

    if (!supla_val->sync_onchange)
        supla_val->sync = 0;

    if (memcmp(supla_val->data.value, value, len) != 0) {
        memcpy(supla_val->data.value, value, len);
        supla_val->sync = 0;
    }
    return SUPLA_RESULT_TRUE;
}
