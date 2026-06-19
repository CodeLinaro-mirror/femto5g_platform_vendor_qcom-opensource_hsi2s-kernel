/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __HSI2S_PARAM_H__
#define __HSI2S_PARAM_H__

typedef unsigned int u32;
typedef unsigned char u8;

struct i2s_config {
	/* Absolute values */
	u32 data_buffer_ms_val;
	u32 bit_depth_val;
	u32 spkr_ch_count_val;
	u32 mic_ch_count_val;
	u8 en_long_rate;
	u32 long_rate;
};// i2s;

/* PCM configurations */
struct pcm_config {
	u32 pcm_rate_val;
	u8 pcm_sync_src;
	u8 pcm_aux_mode;
	u8 pcm_rpcm_width;
	u8 pcm_tpcm_width;
	u8 tdm_en;
	u32 tdm_sync_delay;
	u32 tdm_tpcm_width;
	u32 tdm_rpcm_width;
	u32 tdm_rate;
	u8 tdm_en_diff_sample_width;
	u32 tdm_tpcm_sample_width;
	u32 tdm_rpcm_sample_width;
	u8 tdm_inv_sync;
	u32 lane_config;

	/* Absolute values */
	u32 rate_val;
	u32 sample_width_rx_val;
	u32 sample_width_tx_val;
	u32 sync_delay_val;
};// pcm;

struct dma_config {
	u32 wrdma_periodic_length_bytes;

	/* physical address of dma */
	u32 rddma_base;
	u32 rddma_buff_len;
	u32 rddma_per_len;
	u32 wrdma_base;
	u32 wrdma_buff_len;
	u32 wrdma_per_len;
};

#define BIT_CLK_MAX 73728000
struct i2s_params {
	u32 bit_clk;
	u32 buffer_ms;
	u32 bit_depth;
	u32 spkr_channel_count;
	u32 mic_channel_count;
	u8 en_long_rate;
	u32 long_rate;
};

struct pcm_params {
	u32 bit_clk;
	u32 buffer_ms;
	u8 rate;
	u8 sync_src;
	u8 aux_mode;
	u8 rpcm_width;
	u8 tpcm_width;
};

struct tdm_params {
	u8 sync_delay;
	u32 tpcm_width;
	u32 rpcm_width;
	u32 rate;
	u8 en_diff_sample_width;
	u32 tpcm_sample_width;
	u32 rpcm_sample_width;
};

/* Configure I2S parameters based on user input */
int do_configure_i2s_params(struct i2s_config *config, struct dma_config *dma, struct i2s_params *params);

/* Configure PCM parameters based on user input */
int do_configure_pcm_params(struct pcm_config *config, struct dma_config *dma, struct pcm_params *params);

/* Configure TDM parameters based on user input */
int do_configure_tdm_params(struct pcm_config *config, struct dma_config *dma, struct tdm_params *params);
#endif
