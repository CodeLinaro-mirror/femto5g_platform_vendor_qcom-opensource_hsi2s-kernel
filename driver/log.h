//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef __HSI2S_LOG_H__
#define __HSI2S_LOG_H__
enum {
        HSI2S_DEBUG, HSI2S_INFO, HSI2S_WARN, HSI2S_ERROR
};
static char *prefix[4] = {
        "[D]",
        "[I]",
        "[W]",
        "[E]",
};

#define hsi2s_intf_log(i, l, m, fmt, ...) \
do { \
	if (l >= HSI2S_INFO) {\
		printk("%s hsi2s%d:"fmt, prefix[l], i, ##__VA_ARGS__); \
	} \
} while (0)

#define hsi2s_log(l, m, fmt, ...) \
do { \
	if (l >= HSI2S_INFO) {\
		printk("%s hsi2s:"fmt, prefix[l], ##__VA_ARGS__); \
	} \
} while (0)

#endif
