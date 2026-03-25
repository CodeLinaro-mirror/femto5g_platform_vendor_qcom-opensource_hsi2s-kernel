/*
 * SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */
#include <linux/kernel.h>
#include "hsi2s_param.h"
#include "log.h"

/* Function to calculate periodic interrupt length */
static u32 set_periodic_length(u32 bit_clk, u32 interval)
{
        /*
         * Formula to calculate
         * Bit clock -> 'm' Hz
         * Bits per sec  = m
         * Bits per msec = m * (10^(-3))
         * Bytes per msec = (m * (10^(-3))) / 8 = m / 8000
         * Bytes per 'k' msec = k * (m / 8000)
         */
        return (((unsigned long long)interval * bit_clk) / 8000);
}

/* Configure bit depth */
static void configure_bit_depth(struct i2s_config *config, u32 b_depth)
{
        if (b_depth <= 0) {
                hsi2s_log(HSI2S_WARN, module, "Defaulting to 32 bit configuration\n");
                config->bit_depth_val = 32;
        } else if (b_depth <= 16) {
                hsi2s_log(HSI2S_INFO, module, "Setting 16 bit configuration \n");
                config->bit_depth_val = 16;
        } else if (b_depth <= 24) {
                hsi2s_log(HSI2S_INFO, module, "Setting 24 bit configuration \n");
                config->bit_depth_val = 24;
        } else if (b_depth == 25) {
                hsi2s_log(HSI2S_INFO, module, "Setting 25 bit configuration \n");
                config->bit_depth_val = 25;
        } else if (b_depth <= 32) {
                hsi2s_log(HSI2S_INFO, module, "Setting 32 bit configuration \n");
                config->bit_depth_val = 32;
        } else {
                hsi2s_log(HSI2S_WARN, module, "Defaulting to 32 bit configuration \n");
                config->bit_depth_val = 32;
        }
}


/* Configure speaker channel */
static void configure_spkr_channel(struct i2s_config *config, struct dma_config *dma, u32 ch_count)
{
        switch (ch_count) {
                case 1:
                        hsi2s_log(HSI2S_INFO, module, "Setting mono configuration for speaker\n");
			config->spkr_ch_count_val = 1;
                        break;
                case 2:
                        hsi2s_log(HSI2S_INFO, module, "Setting stereo configuration for speaker\n");
			config->spkr_ch_count_val = 2;
                        break;
                case 4:
                        hsi2s_log(HSI2S_INFO, module, "Setting quad configuration for speaker\n");
			config->spkr_ch_count_val = 4;
                        break;
                default:
                        hsi2s_log(HSI2S_WARN, module, "Invalid number of channels entered");
                        hsi2s_log(HSI2S_WARN, module, "Defaulting to stereo configuration for speaker");
			config->spkr_ch_count_val = 2;
                        break;
        }
}

/* Configure mic channel */
static void configure_mic_channel(struct i2s_config *config, struct dma_config *dma, u32 ch_count)
{
        switch (ch_count) {
                case 1:
                        hsi2s_log(HSI2S_INFO, module, "Setting mono configuration for mic\n");
                        config->mic_ch_count_val = 1;
                        break;
                case 2:
                        hsi2s_log(HSI2S_INFO, module, "Setting stereo configuration for mic\n");
                        config->mic_ch_count_val = 2;
                        break;
                case 4:
                        hsi2s_log(HSI2S_INFO, module, "Setting quad configuration for mic\n");
                        config->mic_ch_count_val = 4;
                        break;
                default:
                        hsi2s_log(HSI2S_WARN, module, "Invalid number of channels entered\n");
                        hsi2s_log(HSI2S_WARN, module, "Defaulting to stereo configuration for mic\n");
                        config->mic_ch_count_val = 2;
                        break;
        }
}


/* Configure pcm rate */
static void configure_pcm_rate(struct pcm_config *config, struct dma_config *dma, u8 rate)
{
#define PCM_RATE_8_BIT_CLKS 0
#define PCM_RATE_16_BIT_CLKS 1
#define PCM_RATE_32_BIT_CLKS 2
#define PCM_RATE_64_BIT_CLKS 3
#define PCM_RATE_128_BIT_CLKS 4
#define PCM_RATE_256_BIT_CLKS 5

        switch (rate) {
                case PCM_RATE_8_BIT_CLKS:
                        hsi2s_log(HSI2S_INFO, module, "Setting pcm rate as 8 bit clocks per frame sync\n");
                        config->pcm_rate_val = 8;
                        config->rate_val = 8;
                        break;
                case PCM_RATE_16_BIT_CLKS:
                        hsi2s_log(HSI2S_INFO, module, "Setting pcm rate as 16 bit clocks per frame sync\n");
                        config->pcm_rate_val = 16;
                        config->rate_val = 16;
			break;
                case PCM_RATE_32_BIT_CLKS:
                        hsi2s_log(HSI2S_INFO, module, "Setting pcm rate as 32 bit clocks per frame sync\n");
                        config->pcm_rate_val = 32;
                        config->rate_val = 32;
			break;
                case PCM_RATE_64_BIT_CLKS:
                        hsi2s_log(HSI2S_INFO, module, "Setting pcm rate as 64 bit clocks per frame sync\n");
                        config->pcm_rate_val = 64;
                        config->rate_val = 64;
			break;
                case PCM_RATE_128_BIT_CLKS:
                        hsi2s_log(HSI2S_INFO, module, "Setting pcm rate as 128 bit clocks per frame sync\n");
                        config->pcm_rate_val = 128;
                        config->rate_val = 128;
			break;
                case PCM_RATE_256_BIT_CLKS:
                        hsi2s_log(HSI2S_INFO, module, "Setting pcm rate as 256 bit clocks per frame sync\n");
                        config->pcm_rate_val = 256;
                        config->rate_val = 256;
			break;
                default:
                        hsi2s_log(HSI2S_WARN, module, "Undefined PCM rate. Setting default value of 256 bit clocks per frame sync\n");
                        config->pcm_rate_val = 256;
                        config->rate_val = 256;
			break;
        }
}


/* Configure pcm tpcm width */
static void configure_tdm_sync_delay(struct pcm_config *config, u8 sync_delay)
{
#define DELAY_2_CYCLE 0
#define DELAY_1_CYCLE 1
#define DELAY_0_CYCLE 2

        switch (sync_delay) {
                case DELAY_2_CYCLE:
                        hsi2s_log(HSI2S_INFO, module, "Setting 2 cycle delay\n");
                        config->tdm_sync_delay = 0x2;
			config->sync_delay_val = 2;
                        break;
                case DELAY_1_CYCLE:
                        hsi2s_log(HSI2S_INFO, module, "Setting 1 cycle delay\n");
                        config->tdm_sync_delay = 0x1;
			config->sync_delay_val = 1;
                        break;
                case DELAY_0_CYCLE:
                        hsi2s_log(HSI2S_INFO, module, "Setting 0 cycle delay\n");
                        config->tdm_sync_delay = 0x0;
			config->sync_delay_val = 0;
                        break;
                default:
                        hsi2s_log(HSI2S_WARN, module, "Undefined sync delay input. Setting 1 cycle delay\n");
                        config->tdm_sync_delay = 0x1;
			config->sync_delay_val = 1;
                        break;
        }
}

static int config_dma_params(struct dma_config *dma, u32 bit_clk, u32 buffer_ms)
{
	/* Set the periodic length */
	if (bit_clk > BIT_CLK_MAX) {
		hsi2s_log(HSI2S_ERROR, module, "Bit clock rate exceeds maximum supported rate of 73.728MHz \n");
		return -1;
	}
	dma->wrdma_periodic_length_bytes = set_periodic_length(bit_clk, buffer_ms);
	hsi2s_log(HSI2S_INFO, module, "Periodic length configured as %u bytes\n", dma->wrdma_periodic_length_bytes);
	return 0;
}

int do_configure_i2s_params(struct i2s_config *config, struct dma_config *dma, struct i2s_params *params)
{
	int ret = 0;
#define LONG_RATE_MIN 0
#define LONG_RATE_MAX 63
        if (params) {
		config_dma_params(dma, params->bit_clk, params->buffer_ms);
                /* Bit depth */
                configure_bit_depth(config, params->bit_depth);
                hsi2s_log(HSI2S_INFO, module, "Bit depth configured as %u bits\n", params->bit_depth);
                /* Speaker channel */
                configure_spkr_channel(config, dma, params->spkr_channel_count);
                hsi2s_log(HSI2S_INFO, module, "Speaker channel count configured as %u \n", params->spkr_channel_count);
                /* Mic channel */
                configure_mic_channel(config, dma, params->mic_channel_count);
                hsi2s_log(HSI2S_INFO, module, "Mic channel count configured as %u \n", params->mic_channel_count);
                /* Check whether long rate is enabled */
                config->en_long_rate = params->en_long_rate;
                if (config->en_long_rate) {
                        if (params->long_rate >= LONG_RATE_MIN && params->long_rate <= LONG_RATE_MAX) {
                                config->long_rate = params->long_rate;
                        } else {
                                hsi2s_log(HSI2S_ERROR, module, "Invalid long rate value specified, disabling long rate\n");
                                config->en_long_rate = 0;
                        }
                }
        } else {
                hsi2s_log(HSI2S_ERROR, module, "Passed null hsi2s_params structure \n");
                ret = -1;
        }

	return ret;
}

int do_configure_pcm_params(struct pcm_config *config, struct dma_config *dma, struct pcm_params *params)
{
	int ret = 0;

	if (params) {
		config_dma_params(dma, params->bit_clk, params->buffer_ms);
		/* Set PCM rate */
		configure_pcm_rate(config, dma, params->rate);
		/* Set PCM sync source */
		config->pcm_sync_src = params->sync_src;
		/* Set PCM aux mode */
		config->pcm_aux_mode = params->aux_mode;
		/* Set PCM rpcm width */
		config->pcm_rpcm_width = params->rpcm_width;
		/* Set PCM tpcm width */
		config->pcm_tpcm_width = params->tpcm_width;

		config->sample_width_rx_val = (params->rpcm_width) ? 16 : 8;
		config->sample_width_tx_val = (params->tpcm_width) ? 16 : 8;
	} else {
		hsi2s_log(HSI2S_ERROR, module, "Passed null pcm_params structure \n");
		ret = -1;
	}

	return ret;
}

int do_configure_tdm_params(struct pcm_config *config, struct dma_config *dma, struct tdm_params *params)
{
	int ret = 0;

        if (params) {
                /* Set TDM enable flag */
                config->tdm_en = 1;
                /* Set TDM rate */
                config->tdm_rate = params->rate;
		config->rate_val = params->rate;

                /* Set RPCM width */
                config->tdm_rpcm_width = params->rpcm_width;
		config->sample_width_rx_val = params->rpcm_width;
                /* Set TPCM width */
                config->tdm_tpcm_width = params->tpcm_width;
		config->sample_width_tx_val = params->tpcm_width;
                /* Set sync delay */
                configure_tdm_sync_delay(config, params->sync_delay);
                /* Check whether different sample width is enabled */
                config->tdm_en_diff_sample_width = params->en_diff_sample_width;
                if (config->tdm_en_diff_sample_width) {
                        config->tdm_tpcm_sample_width = params->tpcm_sample_width;
                        config->tdm_rpcm_sample_width = params->rpcm_sample_width;
                }
        } else {
		hsi2s_log(HSI2S_ERROR, module, "Passed null tdm_params structure \n");
                ret = -1;
        }

        return ret;
}
