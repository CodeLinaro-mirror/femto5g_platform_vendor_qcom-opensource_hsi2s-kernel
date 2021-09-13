 /* Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef ADSP_HSI2S_CLK_CTRL_H
#define ADSP_HSI2S_CLK_CTRL_H

#define HSI2S_SERVICE_ID_V01 0x42A /* QMI service ID */
#define HSI2S_SERVICE_VERS_V01 0x01

#define PROD_HSI2S_CLK_CTRL_REQ_V01 0x0001
#define PROD_HSI2S_CLK_CTRL_RESP_V01 0x0001


struct prod_hsi2s_clk_ctrl_req_msg_v01 {
	u8 enable_hsi2s_clks;
};

#define PROD_HSI2S_CLK_CTRL_REQ_MSG_V01_MAX_MSG_LEN 4

static struct qmi_elem_info prod_hsi2s_clk_ctrl_req_msg_v01_ei[] = {
	{
		.data_type      = QMI_UNSIGNED_1_BYTE,
		.elem_len       = 1,
		.elem_size      = sizeof(u8),
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
		.is_array       = NO_ARRAY,
#else
		.array_type     = NO_ARRAY,
#endif
		.tlv_type       = 0x01,
		.offset         = offsetof(struct prod_hsi2s_clk_ctrl_req_msg_v01,
					   enable_hsi2s_clks),
	},
	{
		.data_type      = QMI_EOTI,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
		.is_array       = NO_ARRAY,
#else
		.array_type     = NO_ARRAY,
#endif
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

struct prod_hsi2s_clk_ctrl_resp_msg_v01 {
	struct qmi_response_type_v01 resp;
};

#define PROD_HSI2S_CLK_CTRL_RESP_MSG_V01_MAX_MSG_LEN 7

static struct qmi_elem_info prod_hsi2s_clk_ctrl_resp_msg_v01_ei[] = {
	{
		.data_type      = QMI_STRUCT,
		.elem_len       = 1,
		.elem_size      = sizeof(struct qmi_response_type_v01),
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
		.is_array       = NO_ARRAY,
#else
		.array_type     = NO_ARRAY,
#endif
		.tlv_type       = 0x02,
		.offset         = offsetof(struct prod_hsi2s_clk_ctrl_resp_msg_v01,
					   resp),
		.ei_array      = qmi_response_type_v01_ei,
	},
	{
		.data_type      = QMI_EOTI,
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
		.is_array       = NO_ARRAY,
#else
		.array_type     = NO_ARRAY,
#endif
		.tlv_type       = QMI_COMMON_TLV_TYPE,
	},
};

#endif
