
//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef __TARGET_OPS_H__
#define __TARGET_OPS_H__

typedef unsigned int u32;
typedef unsigned char u8;

#define CMD_IRQ_PER_RDDMA_CH0                       BIT(0)
#define CMD_IRQ_UNDR_RDDMA_CH0                      BIT(1)
#define CMD_IRQ_ERR_RDDMA_CH0                       BIT(2)
#define CMD_IRQ_PER_RDDMA_CH1                       BIT(3)
#define CMD_IRQ_UNDR_RDDMA_CH1                      BIT(4)
#define CMD_IRQ_ERR_RDDMA_CH1                       BIT(5)
#define CMD_IRQ_PER_RDDMA_CH2                       BIT(6)
#define CMD_IRQ_UNDR_RDDMA_CH2                      BIT(7)
#define CMD_IRQ_ERR_RDDMA_CH2                       BIT(8)
#define CMD_IRQ_PER_RDDMA_CH3                       BIT(9)
#define CMD_IRQ_UNDR_RDDMA_CH3                      BIT(10)
#define CMD_IRQ_ERR_RDDMA_CH3                       BIT(11)
#define CMD_IRQ_PER_RDDMA_CH4                       BIT(12)
#define CMD_IRQ_UNDR_RDDMA_CH4                      BIT(13)
#define CMD_IRQ_ERR_RDDMA_CH4                       BIT(14)
#define CMD_IRQ_PER_WRDMA_CH0                       BIT(15)
#define CMD_IRQ_OVR_WRDMA_CH0                       BIT(16)
#define CMD_IRQ_ERR_WRDMA_CH0                       BIT(17)
#define CMD_IRQ_PER_WRDMA_CH1                       BIT(18)
#define CMD_IRQ_OVR_WRDMA_CH1                       BIT(19)
#define CMD_IRQ_ERR_WRDMA_CH1                       BIT(20)
#define CMD_IRQ_PER_WRDMA_CH2                       BIT(21)
#define CMD_IRQ_OVR_WRDMA_CH2                       BIT(22)
#define CMD_IRQ_ERR_WRDMA_CH2                       BIT(23)
#define CMD_IRQ_PER_WRDMA_CH3                       BIT(24)
#define CMD_IRQ_OVR_WRDMA_CH3                       BIT(25)
#define CMD_IRQ_ERR_WRDMA_CH3                       BIT(26)

struct interface_config {
        const char* dev_name;
        u32 range_count;

        int count;
        struct {
                int interface;
                u32 hs_index;
                u32 dma_index;
                u32 txdmaaddr;
                u32 rxdmaaddr;
        } intf[5];
        u32 dma_buffer_length;
};


struct i2s_params;
struct pcm_params;
struct tdm_params;

struct target_ops {
        int  (*enable_clock)(int enable);

        void (*init_interfaces)(struct interface_config *config);
        void (*configure_lpaif_mode)(int interface, u32 mode);
        void (*configure_muxmode)(int interface, int mode);
        void (*reset_interface)(int interface);
        void (*configure_normal_mode)(int interface);
        void (*configure_int_loopback_mode)(int interface);
        void (*configure_ext_loopback_mode)(int interface);
        void (*start_rddma)(int interface);
        void (*stop_rddma)(int interface);
        void (*set_master_clock)(int interface, u32 value);
        void (*set_slave)(int interface, u32 value);
        void (*config_as_speaker)(int interface);
        void (*config_as_mic)(int interface);
	void (*configure_rate_detection)(int block);
	void (*reset_rate_detection)(int block);

        int (*configure_i2s_params)(int interface, struct i2s_params *params);
        int (*configure_pcm_params)(int interface, struct pcm_params *params);
        int (*configure_tdm_params)(int interface, struct tdm_params *params);
        void (*set_pcm_lane_config)(int interface, u32 config);
        u32 (*get_wrdma_base)(int interface);
        u32 (*get_wrdma_curr)(int interface);
        u32 (*get_irq_stat)(void);
        void (*irq_clear_bits)(u32 bits);
#define NOTIFY_TX_DONE 0
#define NOTIFY_RX_DONE 1
	void (*interrupt)(void (*notify)(int intf, int type));

        void (*set_reg_base)(void * base[], const int count);
};

#endif //__TARGET_OPS_H__

