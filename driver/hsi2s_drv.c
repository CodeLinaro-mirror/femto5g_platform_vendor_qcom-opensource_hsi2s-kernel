/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "hsi2s_drv.h"

/* Device number */
static dev_t devid;

/* Buffer length in words */
static u32 dma_buffer_length_words;

/* HS-I2S core structure */
static struct hsi2s_core *hsi2s_core;

/* Module parameters */
static int operation_mode = 1;
module_param(operation_mode, int, 0644);
MODULE_PARM_DESC(operation_mode, "Default operation mode");

static u32 bit_clock_hz;
module_param(bit_clock_hz, uint, 0644);
MODULE_PARM_DESC(bit_clock_hz, "Bit clock frequency in Hz");

static u32 data_buffer_ms;
module_param(data_buffer_ms, uint, 0644);
MODULE_PARM_DESC(data_buffer_ms, "Data buffer in ms");

static u32 dma_buffer_length;
module_param(dma_buffer_length, uint, 0644);
MODULE_PARM_DESC(dma_buffer_length, "DMA buffer length in MB");

static u32 channel_count;
module_param(channel_count, uint, 0644);
MODULE_PARM_DESC(channel_count, "Number of channels(mono/stereo)");

static u32 bit_depth;
module_param(bit_depth, uint, 0644);
MODULE_PARM_DESC(bit_depth, "Bit depth of the I2S interface");

/* Macro callbacks */

static void t_assign_macros(void)
{

	hsi2s_core->macro->offset_i2s_ctl = T_LPAIF_I2S_CTL;
	hsi2s_core->macro->offset_i2s_sel = T_LPAIF_PCM_I2S_SEL;
	hsi2s_core->macro->offset_irq_en = T_LPAIF_IRQ_EN;
	hsi2s_core->macro->offset_irq_stat = T_LPAIF_IRQ_STAT;
	hsi2s_core->macro->offset_irq_clear = T_LPAIF_IRQ_CLEAR;
	hsi2s_core->macro->offset_rddma_ctl = T_LPAIF_RDDMA_CTL;
	hsi2s_core->macro->offset_rddma_base = T_LPAIF_RDDMA_BASE;
	hsi2s_core->macro->offset_rddma_buff_len = T_LPAIF_RDDMA_BUFF_LEN;
	hsi2s_core->macro->offset_rddma_curr_addr = T_LPAIF_RDDMA_CURR_ADDR;
	hsi2s_core->macro->offset_rddma_per_len = T_LPAIF_RDDMA_PER_LEN;
	hsi2s_core->macro->offset_wrdma_ctl = T_LPAIF_WRDMA_CTL;
	hsi2s_core->macro->offset_wrdma_base = T_LPAIF_WRDMA_BASE;
	hsi2s_core->macro->offset_wrdma_buff_len = T_LPAIF_WRDMA_BUFF_LEN;
	hsi2s_core->macro->offset_wrdma_curr_addr = T_LPAIF_WRDMA_CURR_ADDR;
	hsi2s_core->macro->offset_wrdma_per_len = T_LPAIF_WRDMA_PER_LEN;
	hsi2s_core->macro->offset_pri_rate_det_config = T_LPAIF_PRI_RATE_DET_CONFIG;
	hsi2s_core->macro->offset_pri_rate_det_target1_config = T_LPAIF_PRI_RATE_DET_TARGET1_CONFIG;
	hsi2s_core->macro->offset_pri_rate_det_target2_config = T_LPAIF_PRI_RATE_DET_TARGET2_CONFIG;
	hsi2s_core->macro->offset_pri_rate_bin = T_LPAIF_PRI_RATE_BIN;
	hsi2s_core->macro->offset_pri_stc_diff = T_LPAIF_PRI_STC_DIFF;
	hsi2s_core->macro->offset_pri_rate_det_sel = T_LPAIF_PRI_RATE_DET_SEL;
	hsi2s_core->macro->offset_sec_rate_det_config = T_LPAIF_SEC_RATE_DET_CONFIG;
	hsi2s_core->macro->offset_sec_rate_det_target1_config = T_LPAIF_SEC_RATE_DET_TARGET1_CONFIG;
	hsi2s_core->macro->offset_sec_rate_det_target2_config = T_LPAIF_SEC_RATE_DET_TARGET2_CONFIG;
	hsi2s_core->macro->offset_sec_rate_bin = T_LPAIF_SEC_RATE_BIN;
	hsi2s_core->macro->offset_sec_stc_diff = T_LPAIF_SEC_STC_DIFF;
	hsi2s_core->macro->offset_sec_rate_det_sel = T_LPAIF_SEC_RATE_DET_SEL;
	hsi2s_core->macro->bit_ws_src = T_I2S_WS_SRC;
	hsi2s_core->macro->bit_mic_en = T_I2S_MIC_EN;
	hsi2s_core->macro->bit_spkr_en = T_I2S_SPKR_EN;
	hsi2s_core->macro->bit_loopback = T_I2S_LOOPBACK;
	hsi2s_core->macro->bit_i2s_reset = T_I2S_RESET;
	hsi2s_core->macro->bit_i2s_sel = T_I2S_SEL;
	hsi2s_core->macro->bit_rddma_en = T_RDDMA_EN;
	hsi2s_core->macro->bit_rddma_burst_en = T_RDDMA_BURST_EN;
	hsi2s_core->macro->bit_rddma_dyn_clk = T_RDDMA_DYN_CLK;
	hsi2s_core->macro->bit_rddma_reset = T_RDDMA_RESET;
	hsi2s_core->macro->bit_wrdma_en = T_WRDMA_EN;
	hsi2s_core->macro->bit_wrdma_burst_en = T_WRDMA_BURST_EN;
	hsi2s_core->macro->bit_wrdma_dyn_clk = T_WRDMA_DYN_CLK;
	hsi2s_core->macro->bit_wrdma_reset = T_WRDMA_RESET;
	hsi2s_core->macro->bit_rate_en = T_RATE_DET_EN;
	hsi2s_core->macro->bit_rate_reset = T_RATE_DET_RESET;
	hsi2s_core->macro->regfield_i2s_lrate15 = T_I2S_LONG_RATE_15;
	hsi2s_core->macro->regfield_spkr_mode_sd0 = T_I2S_SPKR_MODE_SD0;
	hsi2s_core->macro->regfield_spkr_mode_quad01 = T_I2S_SPKR_MODE_QUAD01;
	hsi2s_core->macro->regfield_spkr_mono = T_I2S_SPKR_MONO;
	hsi2s_core->macro->regfield_mic_mode_sd1 = T_I2S_MIC_MODE_SD1;
	hsi2s_core->macro->regfield_mic_mode_quad01 = T_I2S_MIC_MODE_QUAD01;
	hsi2s_core->macro->regfield_mic_mono = T_I2S_MIC_MONO;
	hsi2s_core->macro->regfield_bit_width16 = T_I2S_BIT_WIDTH_16;
	hsi2s_core->macro->regfield_bit_width24 = T_I2S_BIT_WIDTH_24;
	hsi2s_core->macro->regfield_bit_width32 = T_I2S_BIT_WIDTH_32;
	hsi2s_core->macro->regfield_bit_width25 = T_I2S_BIT_WIDTH_25;
	hsi2s_core->macro->regfield_rddma_wpscnt_one = T_RDDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_rddma_wpscnt_two = T_RDDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_rddma_wpscnt_four = T_RDDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_rddma_pri_audio_intf = T_RDDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_sec_audio_intf = T_RDDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_fifo_wm8 = T_RDDMA_FIFO_WM_8;
	hsi2s_core->macro->regfield_wrdma_wpscnt_one = T_WRDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_wrdma_wpscnt_two = T_WRDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_wrdma_wpscnt_four = T_WRDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_wrdma_pri_audio_intf = T_WRDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_sec_audio_intf = T_WRDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_loopback_ch0 = T_WRDMA_LOOPBACK_CH0;
	hsi2s_core->macro->regfield_wrdma_loopback_ch1 = T_WRDMA_LOOPBACK_CH1;
	hsi2s_core->macro->regfield_wrdma_fifo_wm8 = T_WRDMA_FIFO_WM_8;
	hsi2s_core->macro->regfield_rate_num_fs_1 = T_RATE_NUM_FS_1;
	hsi2s_core->macro->regfield_rate_num_fs_8 = T_RATE_NUM_FS_8;
	hsi2s_core->macro->regfield_rate_var_192_176p4_fs1 = T_RATE_VAR_192_176P4_FS1;
	hsi2s_core->macro->regfield_rate_var_128_44p1_fs1 = T_RATE_VAR_128_44P1_FS1;
	hsi2s_core->macro->regfield_rate_var_32_8_fs1 = T_RATE_VAR_32_8_FS1;
	hsi2s_core->macro->regfield_rate_target128_fs1 = T_RATE_TARGET128_FS1;
	hsi2s_core->macro->regfield_rate_target_176p4_fs1 = T_RATE_TARGET176P4_FS1;
	hsi2s_core->macro->regfield_rate_target_192_fs1 = T_RATE_TARGET192_FS1;
	hsi2s_core->macro->regfield_rate_var_192_176p4_fs8 = T_RATE_VAR_192_176P4_FS8;
	hsi2s_core->macro->regfield_rate_var_128_44p1_fs8 = T_RATE_VAR_128_44P1_FS8;
	hsi2s_core->macro->regfield_rate_var_32_8_fs8 = T_RATE_VAR_32_8_FS8;
	hsi2s_core->macro->regfield_rate_target128_fs8 = T_RATE_TARGET128_FS8;
	hsi2s_core->macro->regfield_rate_target_176p4_fs8 = T_RATE_TARGET176P4_FS8;
	hsi2s_core->macro->regfield_rate_target_192_fs8 = T_RATE_TARGET192_FS8;
	hsi2s_core->macro->regfield_rate_sync_sel_pri = T_SYNC_SEL_PRI;
	hsi2s_core->macro->regfield_rate_sync_sel_sec = T_SYNC_SEL_SEC;
}

static void h_assign_macros(void)
{

	hsi2s_core->macro->offset_i2s_ctl = H_LPAIF_I2S_CTL;
	hsi2s_core->macro->offset_i2s_sel = H_LPAIF_PCM_I2S_SEL;
	hsi2s_core->macro->offset_irq_en = H_LPAIF_IRQ_EN;
	hsi2s_core->macro->offset_irq_stat = H_LPAIF_IRQ_STAT;
	hsi2s_core->macro->offset_irq_clear = H_LPAIF_IRQ_CLEAR;
	hsi2s_core->macro->offset_rddma_ctl = H_LPAIF_RDDMA_CTL;
	hsi2s_core->macro->offset_rddma_base = H_LPAIF_RDDMA_BASE;
	hsi2s_core->macro->offset_rddma_buff_len = H_LPAIF_RDDMA_BUFF_LEN;
	hsi2s_core->macro->offset_rddma_curr_addr = H_LPAIF_RDDMA_CURR_ADDR;
	hsi2s_core->macro->offset_rddma_per_len = H_LPAIF_RDDMA_PER_LEN;
	hsi2s_core->macro->offset_wrdma_ctl = H_LPAIF_WRDMA_CTL;
	hsi2s_core->macro->offset_wrdma_base = H_LPAIF_WRDMA_BASE;
	hsi2s_core->macro->offset_wrdma_buff_len = H_LPAIF_WRDMA_BUFF_LEN;
	hsi2s_core->macro->offset_wrdma_curr_addr = H_LPAIF_WRDMA_CURR_ADDR;
	hsi2s_core->macro->offset_wrdma_per_len = H_LPAIF_WRDMA_PER_LEN;
	hsi2s_core->macro->offset_pri_rate_det_config = H_LPAIF_PRI_RATE_DET_CONFIG;
	hsi2s_core->macro->offset_pri_rate_det_target1_config = H_LPAIF_PRI_RATE_DET_TARGET1_CONFIG;
	hsi2s_core->macro->offset_pri_rate_det_target2_config = H_LPAIF_PRI_RATE_DET_TARGET2_CONFIG;
	hsi2s_core->macro->offset_pri_rate_bin = H_LPAIF_PRI_RATE_BIN;
	hsi2s_core->macro->offset_pri_stc_diff = H_LPAIF_PRI_STC_DIFF;
	hsi2s_core->macro->offset_sec_rate_det_config = H_LPAIF_SEC_RATE_DET_CONFIG;
	hsi2s_core->macro->offset_sec_rate_det_target1_config = H_LPAIF_SEC_RATE_DET_TARGET1_CONFIG;
	hsi2s_core->macro->offset_sec_rate_det_target2_config = H_LPAIF_SEC_RATE_DET_TARGET2_CONFIG;
	hsi2s_core->macro->offset_sec_rate_bin = H_LPAIF_SEC_RATE_BIN;
	hsi2s_core->macro->offset_sec_stc_diff = H_LPAIF_SEC_STC_DIFF;
	hsi2s_core->macro->bit_ws_src = H_I2S_WS_SRC;
	hsi2s_core->macro->bit_mic_en = H_I2S_MIC_EN;
	hsi2s_core->macro->bit_spkr_en = H_I2S_SPKR_EN;
	hsi2s_core->macro->bit_loopback = H_I2S_LOOPBACK;
	hsi2s_core->macro->bit_i2s_reset = H_I2S_RESET;
	hsi2s_core->macro->bit_i2s_sel = H_I2S_SEL;
	hsi2s_core->macro->bit_rddma_en = H_RDDMA_EN;
	hsi2s_core->macro->bit_rddma_burst_en = H_RDDMA_BURST_EN;
	hsi2s_core->macro->bit_rddma_dyn_clk = H_RDDMA_DYN_CLK;
	hsi2s_core->macro->bit_rddma_reset = H_RDDMA_RESET;
	hsi2s_core->macro->bit_wrdma_en = H_WRDMA_EN;
	hsi2s_core->macro->bit_wrdma_burst_en = H_WRDMA_BURST_EN;
	hsi2s_core->macro->bit_wrdma_dyn_clk = H_WRDMA_DYN_CLK;;
	hsi2s_core->macro->bit_wrdma_reset = H_WRDMA_RESET;
	hsi2s_core->macro->bit_rate_en = H_RATE_DET_EN;
	hsi2s_core->macro->bit_rate_reset = H_RATE_DET_RESET;
	hsi2s_core->macro->regfield_i2s_lrate15 = H_I2S_LONG_RATE_15;
	hsi2s_core->macro->regfield_spkr_mode_sd0 = H_I2S_SPKR_MODE_SD0;
	hsi2s_core->macro->regfield_spkr_mode_quad01 = H_I2S_SPKR_MODE_QUAD01;
	hsi2s_core->macro->regfield_spkr_mono = H_I2S_SPKR_MONO;
	hsi2s_core->macro->regfield_mic_mode_sd1 = H_I2S_MIC_MODE_SD1;
	hsi2s_core->macro->regfield_mic_mode_quad01 = H_I2S_MIC_MODE_QUAD01;
	hsi2s_core->macro->regfield_mic_mono = H_I2S_MIC_MONO;
	hsi2s_core->macro->regfield_bit_width16 = H_I2S_BIT_WIDTH_16;
	hsi2s_core->macro->regfield_bit_width24 = H_I2S_BIT_WIDTH_24;
	hsi2s_core->macro->regfield_bit_width32 = H_I2S_BIT_WIDTH_32;
	hsi2s_core->macro->regfield_bit_width25 = H_I2S_BIT_WIDTH_25;
	hsi2s_core->macro->regfield_rddma_wpscnt_one = H_RDDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_rddma_wpscnt_two = H_RDDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_rddma_wpscnt_four = H_RDDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_rddma_pri_audio_intf = H_RDDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_sec_audio_intf = H_RDDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_ter_audio_intf = H_RDDMA_TER_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_fifo_wm8 = H_RDDMA_FIFO_WM_8;
	hsi2s_core->macro->regfield_wrdma_wpscnt_one = H_WRDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_wrdma_wpscnt_two = H_WRDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_wrdma_wpscnt_four = H_WRDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_wrdma_pri_audio_intf = H_WRDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_sec_audio_intf = H_WRDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_ter_audio_intf = H_WRDMA_TER_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_loopback_ch0 = H_WRDMA_LOOPBACK_CH0;
	hsi2s_core->macro->regfield_wrdma_loopback_ch1 = H_WRDMA_LOOPBACK_CH1;
	hsi2s_core->macro->regfield_wrdma_loopback_ch2 = H_WRDMA_LOOPBACK_CH2;
	hsi2s_core->macro->regfield_wrdma_fifo_wm8 = H_WRDMA_FIFO_WM_8;
	hsi2s_core->macro->regfield_rate_num_fs_1 = H_RATE_NUM_FS_1;
	hsi2s_core->macro->regfield_rate_num_fs_8 = H_RATE_NUM_FS_8;
	hsi2s_core->macro->regfield_rate_var_192_176p4_fs1 = H_RATE_VAR_192_176P4_FS1;
	hsi2s_core->macro->regfield_rate_var_128_44p1_fs1 = H_RATE_VAR_128_44P1_FS1;
	hsi2s_core->macro->regfield_rate_var_32_8_fs1 = H_RATE_VAR_32_8_FS1;
	hsi2s_core->macro->regfield_rate_target128_fs1 = H_RATE_TARGET128_FS1;
	hsi2s_core->macro->regfield_rate_target_176p4_fs1 = H_RATE_TARGET176P4_FS1;
	hsi2s_core->macro->regfield_rate_target_192_fs1 = H_RATE_TARGET192_FS1;
	hsi2s_core->macro->regfield_rate_var_192_176p4_fs8 = H_RATE_VAR_192_176P4_FS8;
	hsi2s_core->macro->regfield_rate_var_128_44p1_fs8 = H_RATE_VAR_128_44P1_FS8;
	hsi2s_core->macro->regfield_rate_var_32_8_fs8 = H_RATE_VAR_32_8_FS8;
	hsi2s_core->macro->regfield_rate_target128_fs8 = H_RATE_TARGET128_FS8;
	hsi2s_core->macro->regfield_rate_target_176p4_fs8 = H_RATE_TARGET176P4_FS8;
	hsi2s_core->macro->regfield_rate_target_192_fs8 = H_RATE_TARGET192_FS8;
	hsi2s_core->macro->regfield_rate_sync_sel_pri = H_SYNC_SEL_PRI;
	hsi2s_core->macro->regfield_rate_sync_sel_sec = H_SYNC_SEL_SEC;
	hsi2s_core->macro->regfield_rate_sync_sel_ter = H_SYNC_SEL_TER;
}

/* Register callbacks */

/* Map the register memory regions */
static int map_registers(struct hsi2s_device *hs_dev, int intf)
{
	int ret = 0;

	if (hsi2s_core->macro) {
		hs_dev->i2s_ctl = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_i2s_ctl +
						  (0x1000 * intf);
		hs_dev->i2s_sel = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_i2s_sel +
						  (0x1000 * intf);
		hs_dev->rddma_ctl = hsi2s_core->lpaif_base_va +
							hsi2s_core->macro->offset_rddma_ctl +
							(0x1000 * intf);
		hs_dev->rddma_base = hsi2s_core->lpaif_base_va +
							 hsi2s_core->macro->offset_rddma_base +
							 (0x1000 * intf);
		hs_dev->rddma_buff_len = hsi2s_core->lpaif_base_va +
								 hsi2s_core->macro->offset_rddma_buff_len +
								 (0x1000 * intf);
		hs_dev->rddma_curr_addr = hsi2s_core->lpaif_base_va +
								  hsi2s_core->macro->offset_rddma_curr_addr +
								  (0x1000 * intf);
		hs_dev->rddma_per_len = hsi2s_core->lpaif_base_va +
								hsi2s_core->macro->offset_rddma_per_len +
								(0x1000 * intf);
		hs_dev->wrdma_ctl = hsi2s_core->lpaif_base_va +
							hsi2s_core->macro->offset_wrdma_ctl +
							(0x1000 * intf);
		hs_dev->wrdma_base = hsi2s_core->lpaif_base_va +
							 hsi2s_core->macro->offset_wrdma_base +
							 (0x1000 * intf);
		hs_dev->wrdma_buff_len = hsi2s_core->lpaif_base_va +
								 hsi2s_core->macro->offset_wrdma_buff_len +
								 (0x1000 * intf);
		hs_dev->wrdma_curr_addr = hsi2s_core->lpaif_base_va +
								  hsi2s_core->macro->offset_wrdma_curr_addr +
								  (0x1000 * intf);
		hs_dev->wrdma_per_len = hsi2s_core->lpaif_base_va +
								hsi2s_core->macro->offset_wrdma_per_len +
								(0x1000 * intf);
		if (hsi2s_core->target == 8155) {
			hs_dev->lpaif_muxmode = hsi2s_core->lpass_tcsr_base_va +
									H_LPAIF_MUXMODE + (0x4 * intf);
		}
	} else {
		pr_err("[HSI2S] HS-I2S macro structure is NULL");
		ret = -EINVAL;
	}

	return ret;
}

/* Map the irq registers */
static int map_core_registers(void)
{
	int ret = 0;

	if (hsi2s_core->macro) {
		/* IRQ registers */
		hsi2s_core->irq_en = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_irq_en;
		hsi2s_core->irq_stat = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_irq_stat;
		hsi2s_core->irq_clear = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_irq_clear;

		/* Rate detection registers */
		hsi2s_core->pri_rate_config = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_pri_rate_det_config;
		hsi2s_core->pri_rate_target1_config = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_pri_rate_det_target1_config;
		hsi2s_core->pri_rate_target2_config = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_pri_rate_det_target2_config;
		hsi2s_core->pri_rate_bin = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_pri_rate_bin;
		hsi2s_core->pri_rate_stc_diff = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_pri_stc_diff;
		if (hsi2s_core->target == 6155)
			hsi2s_core->pri_rate_sel = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_pri_rate_det_sel;
		hsi2s_core->sec_rate_config = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_sec_rate_det_config;
		hsi2s_core->sec_rate_target1_config = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_sec_rate_det_target1_config;
		hsi2s_core->sec_rate_target2_config = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_sec_rate_det_target2_config;
		hsi2s_core->sec_rate_bin = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_sec_rate_bin;
		hsi2s_core->sec_rate_stc_diff = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_sec_stc_diff;
		if (hsi2s_core->target == 6155)
			hsi2s_core->sec_rate_sel = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_sec_rate_det_sel;
	} else {
		pr_err("[HSI2S] HS-I2S macro structure is NULL");
		ret = -EINVAL;
	}

	return ret;
}

/* Set specific register bits */
static void setbits(void __iomem *addr, u32 val)
{
	u32 reg;

	reg = readl_relaxed(addr);
	reg |= val;
	writel_relaxed(reg, addr);
}

/* Clear specific register bits */
static void clearbits(void __iomem *addr, u32 val)
{
	u32 reg;

	reg = readl_relaxed(addr);
	reg &= ~val;
	writel_relaxed(reg, addr);
}

/* Clear the complete register */
static void reg_clear(void __iomem *addr)
{
	clearbits(addr, 0xFFFFFFFF);
}

/* Clear all the IRQs */
static void clear_irqs(void)
{
	writel_relaxed(0xFFFFFFFF, hsi2s_core->irq_clear);
}

/* Reset the registers */
static void reset_registers(struct hsi2s_device *hs_dev)
{
	reg_clear(hs_dev->i2s_ctl);

	clear_irqs();
	reg_clear(hs_dev->rddma_ctl);
	reg_clear(hs_dev->rddma_base);
	reg_clear(hs_dev->rddma_buff_len);
	reg_clear(hs_dev->rddma_curr_addr);
	reg_clear(hs_dev->rddma_per_len);
	setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_reset);
	clearbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_reset);

	msleep(1000);

	reg_clear(hs_dev->wrdma_ctl);
	reg_clear(hs_dev->wrdma_base);
	reg_clear(hs_dev->wrdma_buff_len);
	reg_clear(hs_dev->wrdma_curr_addr);
	reg_clear(hs_dev->wrdma_per_len);
	setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_reset);
	clearbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_reset);

	msleep(1000);

	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
	clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
}

/* Reset the read DMA registers */
static void reset_rddma_registers(struct hsi2s_device *hs_dev)
{
	reg_clear(hs_dev->rddma_ctl);
	reg_clear(hs_dev->rddma_base);
	reg_clear(hs_dev->rddma_buff_len);
	reg_clear(hs_dev->rddma_curr_addr);
	reg_clear(hs_dev->rddma_per_len);
	setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_reset);
	clearbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_reset);
}

/* Reset the write DMA registers */
static void reset_wrdma_registers(struct hsi2s_device *hs_dev)
{
	reg_clear(hs_dev->wrdma_ctl);
	reg_clear(hs_dev->wrdma_base);
	reg_clear(hs_dev->wrdma_buff_len);
	reg_clear(hs_dev->wrdma_curr_addr);
	reg_clear(hs_dev->wrdma_per_len);
	setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_reset);
	clearbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_reset);
}

/* Reset the rate detection registers */
static void reset_rate_detection(int block)
{
	if (block == PRI_RATE_DET) {
		reg_clear(hsi2s_core->pri_rate_config);
		reg_clear(hsi2s_core->pri_rate_target1_config);
		reg_clear(hsi2s_core->pri_rate_target2_config);
		if (hsi2s_core->target == 6155)
			reg_clear(hsi2s_core->pri_rate_sel);
		reg_clear(hsi2s_core->pri_rate_bin);
		reg_clear(hsi2s_core->pri_rate_stc_diff);
		setbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->bit_rate_reset);
		clearbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->bit_rate_reset);
	} else {
		reg_clear(hsi2s_core->sec_rate_config);
		reg_clear(hsi2s_core->sec_rate_target1_config);
		reg_clear(hsi2s_core->sec_rate_target2_config);
		if (hsi2s_core->target == 6155)
			reg_clear(hsi2s_core->sec_rate_sel);
		reg_clear(hsi2s_core->sec_rate_bin);
		reg_clear(hsi2s_core->sec_rate_stc_diff);
		setbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->bit_rate_reset);
		clearbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->bit_rate_reset);
	}

}

/* Configure the rate detection registers */
static void configure_rate_detection(int block)
{
	int minor;

	if (block == PRI_RATE_DET) {
		minor = hsi2s_core->pri_rate_interface;
		/* Set the timestamp interval */
		setbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->regfield_rate_num_fs_8);
		/* Add target configs */
		setbits(hsi2s_core->pri_rate_target1_config, hsi2s_core->macro->regfield_rate_target128_fs8 |
						     hsi2s_core->macro->regfield_rate_target_176p4_fs8);
		setbits(hsi2s_core->pri_rate_target2_config, hsi2s_core->macro->regfield_rate_target_192_fs8);
		/* Add the target variances */
		setbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->regfield_rate_var_192_176p4_fs8 |
					     hsi2s_core->macro->regfield_rate_var_128_44p1_fs8 |
					     hsi2s_core->macro->regfield_rate_var_32_8_fs8);
		/* Select WS interfaces */
		if (hsi2s_core->target == 6155) {
			if (!minor)
				setbits(hsi2s_core->pri_rate_sel, hsi2s_core->macro->regfield_rate_sync_sel_pri);
			else
				setbits(hsi2s_core->pri_rate_sel, hsi2s_core->macro->regfield_rate_sync_sel_sec);
		} else if (hsi2s_core->target == 8155) {
			switch (minor) {
				case 0:
					setbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->regfield_rate_sync_sel_pri);
					break;
				case 1:
					setbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->regfield_rate_sync_sel_sec);
					break;
				case 2:
					setbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->regfield_rate_sync_sel_ter);
					break;
				default:
					break;
			}
		}
		/* Enable the rate detection interrupts */
		setbits(hsi2s_core->irq_en, IRQ_PRI_RD_DIFF_RATE |
				    IRQ_PRI_RD_NO_RATE);
	} else {
		minor = hsi2s_core->sec_rate_interface;
		/* Set the timestamp interval */
		setbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->regfield_rate_num_fs_8);
		/* Add target configs */
		setbits(hsi2s_core->sec_rate_target1_config, hsi2s_core->macro->regfield_rate_target128_fs8 |
						     hsi2s_core->macro->regfield_rate_target_176p4_fs8);
		setbits(hsi2s_core->sec_rate_target2_config, hsi2s_core->macro->regfield_rate_target_192_fs8);
		/* Add the target variances */
		setbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->regfield_rate_var_192_176p4_fs8 |
					     hsi2s_core->macro->regfield_rate_var_128_44p1_fs8 |
					     hsi2s_core->macro->regfield_rate_var_32_8_fs8);
		/* Select WS interfaces */
		if (hsi2s_core->target == 6155) {
			if (!minor)
				setbits(hsi2s_core->sec_rate_sel, hsi2s_core->macro->regfield_rate_sync_sel_pri);
			else
				setbits(hsi2s_core->sec_rate_sel, hsi2s_core->macro->regfield_rate_sync_sel_sec);
		} else if (hsi2s_core->target == 8155) {
			switch (minor) {
				case 0:
					setbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->regfield_rate_sync_sel_pri);
					break;
				case 1:
					setbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->regfield_rate_sync_sel_sec);
					break;
				case 2:
					setbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->regfield_rate_sync_sel_ter);
					break;
				default:
					break;
			}
		}
		/* Enable the rate detection interrupts */
		setbits(hsi2s_core->irq_en, IRQ_SEC_RD_DIFF_RATE |
				    IRQ_SEC_RD_NO_RATE);
	}
}

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
	return ((interval * bit_clk) / 8000);
}

/* Function to calculate bit rate */
static u32 calculate_bit_rate(struct hsi2s_device *hs_dev, int mode)
{
	u32 b_depth;
	u32 ch_count;
	u32 b_rate;

	b_depth = hs_dev->bit_depth_val;
	ch_count = hs_dev->mic_ch_count_val;

	if (mode == PRI_RATE_DET)
		b_rate = b_depth * ch_count * hsi2s_core->pri_ws_rate;
	else
		b_rate = b_depth * ch_count * hsi2s_core->sec_ws_rate;

	return b_rate;
}

/* Get the WS rate */
static u32 get_ws_rate(int block)
{
	u32 val;

	if (block == PRI_RATE_DET)
		val = readl_relaxed(hsi2s_core->pri_rate_bin);
	else
		val = readl_relaxed(hsi2s_core->sec_rate_bin);

	switch (val) {
		case 0x1:
			return 8000;
		case 0x2:
			return 11025;
		case 0x4:
			return 12000;
		case 0x8:
			return 16000;
		case 0x10:
			return 22050;
		case 0x20:
			return 24000;
		case 0x40:
			return 32000;
		case 0x80:
			return 44100;
		case 0x100:
			return 48000;
		case 0x200:
			return 64000;
		case 0x400:
			return 88200;
		case 0x800:
			return 96000;
		case 0x1000:
			return 128000;
		case 0x2000:
			return 176400;
		case 0x4000:
			return 192000;
		default:
			return 0;
	}

}

/* Configure bit depth */
static void configure_bit_depth(struct hsi2s_device *hs_dev, u32 b_depth)
{
	if (b_depth <= 0) {
		pr_warn("[HSI2S] Defaulting to 32 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width32;
		hs_dev->bit_depth_val = 32;
	} else if (b_depth <= 16) {
		pr_warn("[HSI2S] Setting 16 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width16;
		hs_dev->bit_depth_val = 16;
	} else if (b_depth <= 24) {
		pr_warn("[HSI2S] Setting 24 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width24;
		hs_dev->bit_depth_val = 24;
	} else if (b_depth == 25) {
		pr_warn("[HSI2S] Setting 25 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width25;
		hs_dev->bit_depth_val = 25;
	} else if (b_depth <= 32) {
		pr_warn("[HSI2S] Setting 32 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width32;
		hs_dev->bit_depth_val = 32;
	} else {
		pr_warn("[HSI2S] Defaulting to 32 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width32;
		hs_dev->bit_depth_val = 32;
	}
}

/* Configure speaker channel */
static void configure_spkr_channel(struct hsi2s_device *hs_dev, u32 ch_count)
{
	switch (ch_count) {
		case 0:
			pr_warn("[HSI2S] Defaulting to stereo configuration for speaker");
			hs_dev->spkr_channel_count = SPKR_STEREO;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_sd0;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
			else
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
			break;
		case 1:
			pr_warn("[HSI2S] Setting mono configuration for speaker");
			hs_dev->spkr_channel_count = hsi2s_core->macro->regfield_spkr_mono;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_sd0;
			hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
			break;
		case 2:
			pr_warn("[HSI2S] Setting stereo configuration for speaker");
			hs_dev->spkr_channel_count = SPKR_STEREO;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_sd0;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
			else
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
			break;
		case 4:
			pr_warn("[HSI2S] Setting quad configuration for speaker");
			hs_dev->spkr_channel_count = SPKR_QUAD;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_quad01;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
			else
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_four;
			break;
		default:
			pr_warn("[HSI2S] Invalid number of channels entered");
			pr_warn("[HSI2S] Defaulting to stereo configuration for speaker");
			hs_dev->spkr_channel_count = SPKR_STEREO;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_sd0;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
			else
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
			break;
	}
}

/* Configure mic channel */
static void configure_mic_channel(struct hsi2s_device *hs_dev, u32 ch_count)
{
	switch (ch_count) {
		case 0:
			pr_warn("[HSI2S] Defaulting to stereo configuration for mic");
			hs_dev->mic_channel_count = MIC_STEREO;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_sd1;
			hs_dev->mic_ch_count_val = 2;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			else
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
			break;
		case 1:
			pr_warn("[HSI2S] Setting mono configuration for mic");
			hs_dev->mic_channel_count = hsi2s_core->macro->regfield_mic_mono;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_sd1;
			hs_dev->mic_ch_count_val = 1;
			hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			break;
		case 2:
			pr_warn("[HSI2S] Setting stereo configuration for mic");
			hs_dev->mic_channel_count = MIC_STEREO;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_sd1;
			hs_dev->mic_ch_count_val = 2;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			else
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
			break;
		case 4:
			pr_warn("[HSI2S] Setting quad configuration for mic");
			hs_dev->mic_channel_count = MIC_QUAD;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_quad01;
			hs_dev->mic_ch_count_val = 4;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
			else
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_four;
			break;
		default:
			pr_warn("[HSI2S] Invalid number of channels entered");
			pr_warn("[HSI2S] Defaulting to stereo configuration for mic");
			hs_dev->mic_channel_count = MIC_STEREO;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_sd1;
			hs_dev->mic_ch_count_val = 2;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			else
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
			break;
	}
}

/* Configure I2S parameters based on user input */
static int configure_i2s_params(struct hsi2s_device *hs_dev, struct hsi2s_params *params)
{
	int ret = 0;

	if (params) {
		/* Set the periodic length */
		hs_dev->wrdma_periodic_length_bytes = set_periodic_length(params->bit_clk, params->buffer_ms);
		hs_dev->wrdma_periodic_length = hs_dev->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
		pr_warn("[HSI2S] Periodic length configured as %u words", hs_dev->wrdma_periodic_length);
		/* Bit depth */
		configure_bit_depth(hs_dev, params->bit_depth);
		pr_warn("[HSI2S] Bit depth configured as %u bits", params->bit_depth);
		/* Speaker channel */
		configure_spkr_channel(hs_dev, params->spkr_channel_count);
		pr_warn("[HSI2S] Speaker channel count configured as %u", params->spkr_channel_count);
		/* Mic channel */
		configure_mic_channel(hs_dev, params->mic_channel_count);
		pr_warn("[HSI2S] Mic channel count configured as %u", params->mic_channel_count);
	} else {
		pr_err("[HSI2S] Passed null hsi2s_params structure");
		ret = -EINVAL;
	}

	return ret;
}

/* Configure i2s control register for speaker operation */
static void configure_i2s_spkr(struct hsi2s_device *hs_dev)
{
	setbits(hs_dev->i2s_ctl, hs_dev->spkr_mode |
				 hs_dev->spkr_channel_count |
				 hs_dev->bit_depth);
	clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
	clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
}

/* Configure i2s control register for mic operation */
static void configure_i2s_mic(struct hsi2s_device *hs_dev)
{
	setbits(hs_dev->i2s_ctl, hs_dev->mic_mode |
				 hsi2s_core->macro->bit_ws_src |
				 hs_dev->mic_channel_count |
				 hs_dev->bit_depth);
	clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
	clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);

}

/* Configure the read DMA registers */
static void configure_rddma(struct hsi2s_device *hs_dev, int intf)
{
	writel_relaxed(virt_to_phys(hs_dev->read_buffer->ping_start),
		       hs_dev->rddma_base);
	writel_relaxed(dma_buffer_length_words, hs_dev->rddma_buff_len);
	/* Use ping/pong size as periodic length */
	writel_relaxed((dma_buffer_length_words + 1) / 2, hs_dev->rddma_per_len);

	if (intf == HS0_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hsi2s_core->macro->regfield_rddma_pri_audio_intf |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH0 |
					IRQ_UNDR_RDDMA_CH0 |
					IRQ_ERR_RDDMA_CH0);
		pr_warn("[HSI2S] Configured rddma channel for sdr0");
	} else if (intf == HS1_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hsi2s_core->macro->regfield_rddma_sec_audio_intf |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH1 |
					IRQ_UNDR_RDDMA_CH1 |
					IRQ_ERR_RDDMA_CH1);
		pr_warn("[HSI2S] Configured rddma channel for sdr1");
	} else if (intf == HS2_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hsi2s_core->macro->regfield_rddma_ter_audio_intf |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH2 |
					IRQ_UNDR_RDDMA_CH2 |
					IRQ_ERR_RDDMA_CH2);
		pr_warn("[HSI2S] Configured rddma channel for sdr2");
	}
}

/* Configure the write DMA registers */
static void configure_wrdma(struct hsi2s_device *hs_dev, int intf)
{
	writel_relaxed(virt_to_phys(hs_dev->lpass_wrdma_start),
		       hs_dev->wrdma_base);
	writel_relaxed(dma_buffer_length_words, hs_dev->wrdma_buff_len);

	/*
	 * Setting periodic length
	 * Normal mode - use the calculated length as per bit clock
	 * Loopback modes - use ping/pong size
	 */
	if (hs_dev->mode == NORMAL)
		writel_relaxed(hs_dev->wrdma_periodic_length, hs_dev->wrdma_per_len);
	else
		writel_relaxed((dma_buffer_length_words + 1) / 2, hs_dev->wrdma_per_len);

	if (intf == HS0_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hsi2s_core->macro->regfield_wrdma_pri_audio_intf |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH0 |
					IRQ_OVR_WRDMA_CH0 |
					IRQ_ERR_WRDMA_CH0);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr0");
	} else if (intf == HS1_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hsi2s_core->macro->regfield_wrdma_sec_audio_intf |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH1 |
					IRQ_OVR_WRDMA_CH1 |
					IRQ_ERR_WRDMA_CH1);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr1");
	} else if (intf == HS2_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hsi2s_core->macro->regfield_wrdma_ter_audio_intf |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH2 |
					IRQ_OVR_WRDMA_CH2 |
					IRQ_ERR_WRDMA_CH2);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr2");
	}

	setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
	if (hsi2s_core->is_rate_enabled) {
		if (intf == hsi2s_core->pri_rate_interface)
			setbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->bit_rate_en);
		else if (intf == hsi2s_core->sec_rate_interface)
			setbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->bit_rate_en);
	}
}

/* Configure the I2S control register for internal loopback */
static void configure_i2s_int_lb(struct hsi2s_device *hs_dev)
{
	setbits(hs_dev->i2s_ctl, hs_dev->spkr_mode |
				 hs_dev->mic_mode |
				 hs_dev->spkr_channel_count |
				 hs_dev->mic_channel_count |
				 hs_dev->bit_depth |
				 hsi2s_core->macro->bit_loopback);
	clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
	clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
}

/* Configure the read DMA registers */
static void configure_rddma_int_lb(struct hsi2s_device *hs_dev, int intf)
{
	writel_relaxed(virt_to_phys(hs_dev->read_buffer->ping_start),
		       hs_dev->rddma_base);
	writel_relaxed(dma_buffer_length_words, hs_dev->rddma_buff_len);
	/* Use ping/pong size as periodic length */
	writel_relaxed((dma_buffer_length_words + 1) / 2, hs_dev->rddma_per_len);

	if (intf == HS0_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_pri_audio_intf |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH0 |
					IRQ_UNDR_RDDMA_CH0 |
					IRQ_ERR_RDDMA_CH0);
		pr_warn("[HSI2S] Configured rddma channel for sdr0");
	} else if (intf == HS1_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_sec_audio_intf |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH1 |
					IRQ_UNDR_RDDMA_CH1 |
					IRQ_ERR_RDDMA_CH1);
		pr_warn("[HSI2S] Configured rddma channel for sdr1");
	} else if (intf == HS2_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_ter_audio_intf |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH2 |
					IRQ_UNDR_RDDMA_CH2 |
					IRQ_ERR_RDDMA_CH2);
		pr_warn("[HSI2S] Configured rddma channel for sdr2");
	}
}

/* Configure the write DMA registers for internal loopback */
static void configure_wrdma_int_lb(struct hsi2s_device *hs_dev, int intf)
{
	writel_relaxed(virt_to_phys(hs_dev->lpass_wrdma_start),
		       hs_dev->wrdma_base);
	writel_relaxed(dma_buffer_length_words, hs_dev->wrdma_buff_len);
	/* Use ping/pong size as periodic length */
	writel_relaxed((dma_buffer_length_words + 1) / 2, hs_dev->wrdma_per_len);

	if (intf == HS0_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_loopback_ch0 |
					   hsi2s_core->macro->regfield_wrdma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH0 |
					IRQ_OVR_WRDMA_CH0 |
					IRQ_ERR_WRDMA_CH0);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr0");
	} else if (intf == HS1_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_loopback_ch1 |
					   hsi2s_core->macro->regfield_wrdma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH1 |
					IRQ_OVR_WRDMA_CH1 |
					IRQ_ERR_WRDMA_CH1);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr1");
	} else if (intf == HS2_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_loopback_ch2 |
					   hsi2s_core->macro->regfield_wrdma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH2 |
					IRQ_OVR_WRDMA_CH2 |
					IRQ_ERR_WRDMA_CH2);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr2");
	}

	setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
}

/* Configure the I2S control register for external loopback */
static void configure_i2s_ext_lb(struct hsi2s_device *hs_dev)
{
	setbits(hs_dev->i2s_ctl, hs_dev->spkr_mode |
				 hs_dev->spkr_channel_count |
				 hs_dev->mic_mode |
				 hs_dev->mic_channel_count |
				 hs_dev->bit_depth);
	clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
	clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
}

/* Function to configure HS-I2S registers in normal mode */
static void configure_normal_mode(struct hsi2s_device *hs_dev, int intf)
{
	pr_warn("[HSI2S] Configuring normal mode operation");
	/* Set operational mode */
	hs_dev->mode = NORMAL;
	/* Reset the DMA registers */
	reset_rddma_registers(hs_dev);
	reset_wrdma_registers(hs_dev);
	/* Reset I2S control register */
	reg_clear(hs_dev->i2s_ctl);
	/* Configure I2S control register */
	configure_i2s_spkr(hs_dev);
	configure_i2s_mic(hs_dev);
	/* Configure RDDMA registers */
	configure_rddma(hs_dev, intf);
	/* Configure WRDMA registers */
	configure_wrdma(hs_dev, intf);
	msleep(1000);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
	/* Reset buffer pointers */
	hs_dev->read_buffer->last_copy = 1;
	hs_dev->read_buffer->last_xfer = 1;
	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
}

/* Function to configure HS-I2S registers in loopback mode */
static void configure_int_loopback_mode(struct hsi2s_device *hs_dev, int intf)
{
	pr_warn("[HSI2S] Configuring loopback mode operation");
	/* Set operational mode */
	hs_dev->mode = INTERNAL_LB;
	/* Reset the DMA registers */
	reset_rddma_registers(hs_dev);
	reset_wrdma_registers(hs_dev);
	/* Reset I2S control register */
	reg_clear(hs_dev->i2s_ctl);
	/* Configure I2S control register */
	configure_i2s_int_lb(hs_dev);
	/* Configure RDDMA registers */
	configure_rddma_int_lb(hs_dev, intf);
	/* Configure WRDMA registers */
	configure_wrdma_int_lb(hs_dev, intf);
	msleep(1000);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
	/* Reset buffer pointers */
	hs_dev->read_buffer->last_copy = 1;
	hs_dev->read_buffer->last_xfer = 1;
	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
}

/* Function to configure HS-I2S registers in external loopback mode */
static void configure_ext_loopback_mode(struct hsi2s_device *hs_dev, int intf)
{
	pr_warn("[HSI2S] Configuring external loopback mode operation");
	/* Set operational mode */
	hs_dev->mode = EXTERNAL_LB_MASTER;
	/* Reset the DMA registers */
	reset_rddma_registers(hs_dev);
	reset_wrdma_registers(hs_dev);
	/* Reset I2S control register */
	reg_clear(hs_dev->i2s_ctl);
	/* Configure I2S control register */
	configure_i2s_ext_lb(hs_dev);
	/* Configure WRDMA registers */
	configure_wrdma(hs_dev, intf);
	/* Configure RDDMA registers */
	configure_rddma(hs_dev, intf);
	msleep(1000);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
	/* Reset buffer pointers */
	hs_dev->read_buffer->last_copy = 1;
	hs_dev->read_buffer->last_xfer = 1;
	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
}

/* Configure interface as master/slave */
static void configure_muxmode(struct hsi2s_device *hs_dev, int mode)
{
	if (mode) {
		/* Configure slave */
		setbits(hs_dev->lpaif_muxmode, BIT(0));
		setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_ws_src);
		pr_warn("[HSI2S] hs%d configured as slave", hs_dev->minor_num);
	} else {
		/* Configure master */
		clearbits(hs_dev->lpaif_muxmode, BIT(0));
		clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_ws_src);
		pr_warn("[HSI2S] hs%d configured as master", hs_dev->minor_num);
	}

}

/* DMA buffer callbacks */

/* Function to allocate buffers */
static int hsi2s_buffer_init(struct hsi2s_device *hs_dev)
{
	int ret = 0;

	/* Allocate read buffer */
	pr_warn("[HSI2S] Allocating kernel buffer for write DMA");
	hs_dev->read_buffer = kzalloc(sizeof(*hs_dev->read_buffer),
				       GFP_KERNEL);
	if (!hs_dev->read_buffer) {
		ret = -ENOMEM;
		goto err_read_buffer;
	}

	hs_dev->read_buffer->buffer = kzalloc(sizeof(int32_t) *
				dma_buffer_length_words, GFP_KERNEL | GFP_DMA);
	if (!hs_dev->read_buffer->buffer) {
		ret = -ENOMEM;
		goto err_read_dma_buffer;
	}

	hs_dev->read_buffer->handle = dma_map_single(hs_dev->dev, hs_dev->read_buffer->buffer, dma_buffer_length, DMA_TO_DEVICE);
        if (dma_mapping_error(hs_dev->dev, hs_dev->read_buffer->handle)) {
                pr_err("[HSI2S] Failed to perform dma_map_single");
                ret = -EINVAL;
                goto err_read_dma_map;
        }

	hs_dev->read_buffer->ping_start = hs_dev->read_buffer->buffer;
	hs_dev->read_buffer->pong_start = hs_dev->read_buffer->buffer + (dma_buffer_length / 2);
	hs_dev->read_buffer->length = dma_buffer_length / 2;
	hs_dev->read_buffer->last_copy = 1;
	hs_dev->read_buffer->last_xfer = 1;

	/* Allocate write buffer */
	pr_warn("[HSI2S] Allocating kernel buffer for write DMA");
	hs_dev->write_buffer = kzalloc(sizeof(*hs_dev->write_buffer),
				       GFP_KERNEL);
	if (!hs_dev->write_buffer) {
		ret = -ENOMEM;
		goto err_write_buffer;
	}

	hs_dev->write_buffer->buffer = kzalloc(sizeof(int32_t) *
				dma_buffer_length_words, GFP_KERNEL | GFP_DMA);
	if (!hs_dev->write_buffer->buffer) {
		ret = -ENOMEM;
		goto err_write_dma_buffer;
	}

	hs_dev->write_buffer->handle = dma_map_single(hs_dev->dev, hs_dev->write_buffer->buffer, dma_buffer_length, DMA_FROM_DEVICE);
        if (dma_mapping_error(hs_dev->dev, hs_dev->write_buffer->handle)) {
                pr_err("[HSI2S] Failed to perform dma_map_single");
                ret = -EINVAL;
                goto err_write_dma_map;
        }

	hs_dev->lpass_wrdma_start = hs_dev->write_buffer->buffer;
	hs_dev->lpass_wrdma_end = hs_dev->lpass_wrdma_start +
					dma_buffer_length;

	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->data_ready = 0;
	hs_dev->write_buffer->pollin = 0;

	return ret;

err_write_dma_map:
	if (hs_dev->write_buffer->buffer) {
		kfree(hs_dev->write_buffer->buffer);
		hs_dev->write_buffer->buffer = NULL;
	}
err_write_dma_buffer:
	if (hs_dev->write_buffer) {
		kfree(hs_dev->write_buffer);
		hs_dev->write_buffer = NULL;
	}
err_write_buffer:
	if (hs_dev->read_buffer) {
		dma_unmap_single(hs_dev->dev, hs_dev->read_buffer->handle,
				 dma_buffer_length, DMA_TO_DEVICE);
	}
err_read_dma_map:
	if (hs_dev->read_buffer->buffer) {
		kfree(hs_dev->read_buffer->buffer);
		hs_dev->read_buffer->buffer = NULL;
	}
err_read_dma_buffer:
	if (hs_dev->read_buffer) {
		kfree(hs_dev->read_buffer);
		hs_dev->read_buffer = NULL;
	}
err_read_buffer:
	return ret;
}

/* Function to free allocated buffers */
static void hsi2s_buffer_free(struct hsi2s_device *hs_dev)
{
	/* Freeing write DMA buffer */
	if (hs_dev->write_buffer) {
		if (hs_dev->write_buffer->buffer) {
			dma_unmap_single(hs_dev->dev, hs_dev->write_buffer->handle,
					 dma_buffer_length, DMA_FROM_DEVICE);
			kfree(hs_dev->write_buffer->buffer);
			hs_dev->write_buffer->buffer = NULL;
		}
		kfree(hs_dev->write_buffer);
		hs_dev->write_buffer = NULL;
	}

	/* Freeing read DMA buffer */
	if (hs_dev->read_buffer) {
		if (hs_dev->read_buffer->buffer) {
			dma_unmap_single(hs_dev->dev, hs_dev->read_buffer->handle,
					 dma_buffer_length, DMA_FROM_DEVICE);
			kfree(hs_dev->read_buffer->buffer);
			hs_dev->read_buffer->buffer = NULL;
		}
		kfree(hs_dev->read_buffer);
		hs_dev->read_buffer = NULL;
	}
}

/* Function to call register mapping and buffer management callbacks */
static int init_default(struct hsi2s_device *hs_dev, int intf)
{
	int ret = 0;

	/* Map the hs-i2s registers */
	ret = map_registers(hs_dev, intf);
	if (ret < 0) {
		pr_err("[HSI2S] Unable to map device registers");
		return ret;
	}

	reset_registers(hs_dev);

	/* Allocate kernel buffers */
	ret = hsi2s_buffer_init(hs_dev);
	if (ret < 0) {
		pr_err("[HSI2S] Buffer allocation failed");
		return ret;
	}

	/* Initialize the wait queues */
	init_waitqueue_head(&hs_dev->wq_rddma);
	init_waitqueue_head(&hs_dev->wq_wrdma);
	init_waitqueue_head(&hs_dev->wq_copy);

	return ret;
}

#ifndef CONFIG_QTI_GVM
/* SMMU functions */

/* Function to init smmu */
static int hsi2s_smmu_init(struct platform_device *pdev, int minor)
{
	struct device *dev = &pdev->dev;
	struct hsi2s_device *hs_dev;
	struct dma_iommu_mapping *mapping;
	u32 iova_ap_mapping[2];
	int bypass = 1;
	int ret = 0;

	hs_dev = (struct hsi2s_device *)platform_get_drvdata(pdev);
	hs_dev->hsi2s_smmu_ctx = kzalloc(sizeof(*hs_dev->hsi2s_smmu_ctx),
					 GFP_KERNEL);
	if (!hs_dev->hsi2s_smmu_ctx)
		return -ENOMEM;

	ret = of_property_read_u32_array(dev->of_node, "qcom,iova-mapping",
					 iova_ap_mapping, 2);
	if (ret) {
		pr_err("[HSI2S] Failed to read smmu start/size iova addresses");
		goto err_smmu_probe;
	}

	hs_dev->hsi2s_smmu_ctx->va_start = iova_ap_mapping[0];
	hs_dev->hsi2s_smmu_ctx->va_size = iova_ap_mapping[1];
	hs_dev->hsi2s_smmu_ctx->smmu_pdev = pdev;

	hs_dev->hsi2s_smmu_ctx->mapping =
		arm_iommu_create_mapping(dev->bus,
					 hs_dev->hsi2s_smmu_ctx->va_start,
					 hs_dev->hsi2s_smmu_ctx->va_size);
	if (IS_ERR_OR_NULL(hs_dev->hsi2s_smmu_ctx->mapping)) {
		pr_err("[HSI2S] Fail to create mapping");
		/* assume this failure is because iommu driver is not ready */
		ret = -EPROBE_DEFER;
		goto err_smmu_probe;
	}
	pr_err("[HSI2S] Successfully Created SMMU mapping");
	hs_dev->hsi2s_smmu_ctx->valid = true;
	mapping = hs_dev->hsi2s_smmu_ctx->mapping;

	if (of_property_read_bool(dev->of_node, "qcom,smmu-s1-bypass")) {
		if (iommu_domain_set_attr(mapping->domain,
					  DOMAIN_ATTR_S1_BYPASS,
					  &bypass)) {
			pr_err("[HSI2S] Couldn't set SMMU S1 bypass\n");
			ret = -EIO;
			goto err_smmu_probe;
		}
	}

	ret = arm_iommu_attach_device(&hs_dev->hsi2s_smmu_ctx->smmu_pdev->dev,
				      mapping);
	if (ret) {
		pr_err("[HSI2S] couldn't attach to IOMMU ret=%d", ret);
		goto err_smmu_probe;
	}

	hs_dev->hsi2s_smmu_ctx->iommu_domain =
	iommu_get_domain_for_dev(&hs_dev->hsi2s_smmu_ctx->smmu_pdev->dev);

	pr_warn("[HSI2S] Successfully attached to IOMMU");
	return ret;

err_smmu_probe:
	if (hs_dev->hsi2s_smmu_ctx->mapping)
		arm_iommu_release_mapping(hs_dev->hsi2s_smmu_ctx->mapping);
	hs_dev->hsi2s_smmu_ctx->valid = false;

	kfree(hs_dev->hsi2s_smmu_ctx);
	hs_dev->hsi2s_smmu_ctx = NULL;

	hs_dev->hsi2s_smmu_ctx->ret = ret;
	return ret;
}
#endif

/* GPIO management functions */

/* Function to configure gpio pins */
static int hsi2s_configure_gpio_pins(struct platform_device *pdev)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *hsi2s_active_state;
	int ret = 0;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		pr_err("[HSI2S] Failed to get pinctrl, err = %d", ret);
		return ret;
	}
	pr_warn("[HSI2S] get pinctrl succeed\n");

	hsi2s_active_state = pinctrl_lookup_state(pinctrl, "default");
	if (IS_ERR_OR_NULL(hsi2s_active_state)) {
		ret = PTR_ERR(hsi2s_active_state);
		pr_err("[HSI2S] Failed to get default state, err = %d\n", ret);
		return ret;
	}
	pr_warn("[HSI2S] Get default state succeed\n");
	ret = pinctrl_select_state(pinctrl, hsi2s_active_state);
	if (ret)
		pr_err("[HSI2S] Unable to set default state, err = %d", ret);
	else
		pr_err("[HSI2S] Set default pinctrl state succeeded");

	return ret;
}

/* Clock management functions */

static void h_modify_core_clks(int enable)
{
	void __iomem *lpass_core_cbcr;
	void __iomem *hs_rdmem;
	void __iomem *hs_wrmem;
	void __iomem *lpass_mport;

	lpass_core_cbcr = ioremap(0x1701F000, 4);
	hs_rdmem = ioremap(0x17049004, 4);
	hs_wrmem = ioremap(0x17049000, 4);
	lpass_mport = ioremap(0x17023000, 4);

	if (enable) {
		pr_warn("[HSI2S] Enable core clocks for 8155");
		if (!(readl_relaxed(lpass_core_cbcr) & 0x1))
			setbits(lpass_core_cbcr, 0x1);
		if (!(readl_relaxed(hs_rdmem) & 0x1))
			setbits(hs_rdmem, 0x1);
		if (!(readl_relaxed(hs_wrmem) & 0x1))
			setbits(hs_wrmem, 0x1);
		if (!(readl_relaxed(lpass_mport) & 0x1))
			setbits(lpass_mport, 0x1);
		pr_warn("[HSI2S] Core clocks enabled for 8155");
	} else {
		pr_warn("[HSI2S] Disable core clocks for 8155");
		clearbits(hs_wrmem, 0x1);
		clearbits(hs_rdmem, 0x1);
		pr_warn("[HSI2S] Core clocks disabled for 8155");
	}

	iounmap(lpass_core_cbcr);
	iounmap(hs_rdmem);
	iounmap(hs_wrmem);
	iounmap(lpass_mport);
}

static void h_modify_interface_clks(int enable)
{
	void __iomem *hs_if0_ibit;
	void __iomem *hs_if1_ibit;
	void __iomem *hs_if2_ibit;
	void __iomem *hs_if0_ebit;
	void __iomem *hs_if1_ebit;
	void __iomem *hs_if2_ebit;
	void __iomem *hs_if0_mclk;
	void __iomem *hs_if1_mclk;
	void __iomem *hs_if2_mclk;

	hs_if0_ibit = ioremap(0x17046018, 4);
	hs_if0_ebit = ioremap(0x1704601C, 4);
	hs_if0_mclk = ioremap(0x17020014, 4);
	hs_if1_ibit = ioremap(0x17047018, 4);
	hs_if1_ebit = ioremap(0x1704701C, 4);
	hs_if1_mclk = ioremap(0x17021014, 4);
	hs_if2_ibit = ioremap(0x17048018, 4);
	hs_if2_ebit = ioremap(0x1704801C, 4);
	hs_if2_mclk = ioremap(0x17022014, 4);

	if (enable) {
		pr_warn("[HSI2S] Enable interface clocks for 8155");
		setbits(hs_if0_ibit, 0x1);
		setbits(hs_if1_ibit, 0x1);
		setbits(hs_if2_ibit, 0x1);
		setbits(hs_if0_ebit, 0x1);
		setbits(hs_if1_ebit, 0x1);
		setbits(hs_if2_ebit, 0x1);
		setbits(hs_if0_mclk, 0x1);
		setbits(hs_if1_mclk, 0x1);
		setbits(hs_if2_mclk, 0x1);
		pr_warn("[HSI2S] Interface clocks enabled for 8155");
	} else {
		pr_warn("[HSI2S] Disable interface clocks for 8155");
		clearbits(hs_if0_ibit, 0x1);
		clearbits(hs_if1_ibit, 0x1);
		clearbits(hs_if2_ibit, 0x1);
		clearbits(hs_if0_ebit, 0x1);
		clearbits(hs_if1_ebit, 0x1);
		clearbits(hs_if2_ebit, 0x1);
		clearbits(hs_if0_mclk, 0x1);
		clearbits(hs_if1_mclk, 0x1);
		clearbits(hs_if2_mclk, 0x1);
		pr_warn("[HSI2S] Interface clocks disabled for 8155");
	}

	iounmap(hs_if0_ibit);
	iounmap(hs_if0_ebit);
	iounmap(hs_if0_mclk);
	iounmap(hs_if1_ibit);
	iounmap(hs_if1_ebit);
	iounmap(hs_if1_mclk);
	iounmap(hs_if2_ibit);
	iounmap(hs_if2_ebit);
	iounmap(hs_if2_mclk);
}

/* Function to disable core clocks */
static void hsi2s_disable_core_clks(struct platform_device *pdev)
{
	struct hsi2s_core *hs_core;
	struct device *dev = &pdev->dev;

	hs_core = (struct hsi2s_core *)platform_get_drvdata(pdev);

	if (hs_core->core_clk) {
		clk_disable_unprepare(hs_core->core_clk);
		devm_clk_put(dev, hs_core->core_clk);
	}

	hs_core->core_clk = NULL;

	if (hs_core->csr_hclk) {
		clk_disable_unprepare(hs_core->csr_hclk);
		devm_clk_put(dev, hs_core->csr_hclk);
	}

	hs_core->csr_hclk = NULL;

	if (hs_core->wr0_mem_clk) {
		clk_disable_unprepare(hs_core->wr0_mem_clk);
		devm_clk_put(dev, hs_core->wr0_mem_clk);
	}

	hs_core->wr0_mem_clk = NULL;

	if (hs_core->wr1_mem_clk) {
		clk_disable_unprepare(hs_core->wr1_mem_clk);
		devm_clk_put(dev, hs_core->wr1_mem_clk);
	}

	hs_core->wr1_mem_clk = NULL;

	if (hs_core->wr2_mem_clk) {
		clk_disable_unprepare(hs_core->wr2_mem_clk);
		devm_clk_put(dev, hs_core->wr2_mem_clk);
	}

	hs_core->wr2_mem_clk = NULL;
}

/* Function to enable core clocks */
static int hsi2s_enable_core_clks(struct platform_device *pdev)
{
	struct hsi2s_core *hs_core;
	struct device *dev = &pdev->dev;
	int ret = 0;

	hs_core = (struct hsi2s_core *)platform_get_drvdata(pdev);

	hs_core->core_clk = devm_clk_get(dev, "core_clk");
	if (!hs_core->core_clk) {
		pr_err("[HSI2S] Unable to get sdr_core clock ");
		return -EIO;
	}

	hs_core->csr_hclk = devm_clk_get(dev, "csr_hclk");
	if (!hs_core->csr_hclk) {
		pr_err("[HSI2S] Unable to get sdr_csr_hclk clock ");
		return -EIO;
	}

	hs_core->wr0_mem_clk = devm_clk_get(dev, "wr0_mem_clk");
	if (!hs_core->wr0_mem_clk) {
		pr_err("[HSI2S] Unable to get sdr_wr0_mem_clk clock ");
		return -EIO;
	}

	hs_core->wr1_mem_clk = devm_clk_get(dev, "wr1_mem_clk");
	if (!hs_core->wr1_mem_clk) {
		pr_err("[HSI2S] Unable to get sdr_wr1_mem_clk clock ");
		return -EIO;
	}

	hs_core->wr2_mem_clk = devm_clk_get(dev, "wr2_mem_clk");
	if (!hs_core->wr2_mem_clk) {
		pr_err("[HSI2S] Unable to get sdr_wr2_mem_clk clock ");
		return -EIO;
	}

	ret = clk_prepare_enable(hs_core->core_clk);
	if (ret) {
		pr_err("[HSI2S] Failed to enable sdr_core clock");
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_core->csr_hclk);
	if (ret) {
		pr_err("[HSI2S] Failed to enable sdr_csr_hclk clock");
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_core->wr0_mem_clk);
	if (ret) {
		pr_err("[HSI2S] Failed to enable sdr_wr0_mem_clk clock");
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_core->wr1_mem_clk);
	if (ret) {
		pr_err("[HSI2S] Failed to enable sdr_wr1_mem_clk clock");
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_core->wr2_mem_clk);
	if (ret) {
		pr_err("[HSI2S] Failed to enable sdr_wr2_mem_clk clock");
		goto fail_clk;
	}

	return ret;

fail_clk:
	hsi2s_disable_core_clks(pdev);
	return ret;
}

/* Function to suspend core clocks */
static void hsi2s_suspend_core_clks(struct platform_device *pdev)
{
	struct hsi2s_core *hs_core;

	hs_core = (struct hsi2s_core *)platform_get_drvdata(pdev);

	if (hs_core->core_clk)
		clk_disable_unprepare(hs_core->core_clk);

	if (hs_core->csr_hclk)
		clk_disable_unprepare(hs_core->csr_hclk);

	if (hs_core->wr0_mem_clk)
		clk_disable_unprepare(hs_core->wr0_mem_clk);

	if (hs_core->wr1_mem_clk)
		clk_disable_unprepare(hs_core->wr1_mem_clk);

	if (hs_core->wr2_mem_clk)
		clk_disable_unprepare(hs_core->wr2_mem_clk);
}

/* Function to resume core clocks */
static int hsi2s_resume_core_clks(struct platform_device *pdev)
{
	struct hsi2s_core *hs_core;
	int ret = 0;

	hs_core = (struct hsi2s_core *)platform_get_drvdata(pdev);

	if (hs_core->core_clk) {
		ret = clk_prepare_enable(hs_core->core_clk);
		if (ret) {
			pr_err("[HSI2S] Failed to enable sdr_core clock");
			goto fail_clk;
		}
	}

	if (hs_core->csr_hclk) {
		ret = clk_prepare_enable(hs_core->csr_hclk);
		if (ret) {
			pr_err("[HSI2S] Failed to enable sdr_csr_hclk clock");
			goto fail_clk;
		}
	}

	if (hs_core->wr0_mem_clk) {
		ret = clk_prepare_enable(hs_core->wr0_mem_clk);
		if (ret) {
			pr_err("[HSI2S] Failed to enable sdr_wr0_mem_clk clock");
			goto fail_clk;
		}
	}

	if (hs_core->wr1_mem_clk) {
		ret = clk_prepare_enable(hs_core->wr1_mem_clk);
		if (ret) {
			pr_err("[HSI2S] Failed to enable sdr_wr1_mem_clk clock");
			goto fail_clk;
		}
	}

	if (hs_core->wr2_mem_clk) {
		ret = clk_prepare_enable(hs_core->wr2_mem_clk);
		if (ret) {
			pr_err("[HSI2S] Failed to enable sdr_wr2_mem_clk clock");
			goto fail_clk;
		}
	}

	return ret;
fail_clk:
	pr_err("[HSI2S] Failed to enable some core clocks");
	hsi2s_disable_core_clks(pdev);
	return ret;
}

/* Function to disable interface clocks */
static void hsi2s_disable_intf_clks(struct platform_device *pdev)
{
	struct hsi2s_device *hs_dev;
	struct device *dev = &pdev->dev;

	hs_dev = (struct hsi2s_device *)platform_get_drvdata(pdev);

	if (hs_dev->intf_clk) {
		clk_disable_unprepare(hs_dev->intf_clk);
		devm_clk_put(dev, hs_dev->intf_clk);
	}

	hs_dev->intf_clk = NULL;
}

/* Function to enable interface clocks */
static int hsi2s_enable_intf_clks(struct platform_device *pdev)
{
	struct hsi2s_device *hs_dev;
	struct device *dev = &pdev->dev;
	const char *intf_clock_name;
	int ret = 0;

	hs_dev = (struct hsi2s_device *)platform_get_drvdata(pdev);

	if (!hs_dev->minor_num)
		intf_clock_name = "pri_mi2s_clk";
	else
		intf_clock_name = "sec_mi2s_clk";

	hs_dev->intf_clk = devm_clk_get(dev, intf_clock_name);
	if (!hs_dev->intf_clk) {
		pr_err("[HSI2S] Unable to get interface clock for SDR%d interface",
		       hs_dev->minor_num);
		return -EIO;
	}

	ret = clk_prepare_enable(hs_dev->intf_clk);
	if (ret) {
		pr_err("[HSI2S] Failed to enable interface clock for SDR%d interface",
		       hs_dev->minor_num);
		goto fail_clk;
	}

fail_clk:
	return ret;
}

/* Function to suspend interface clocks */
static void hsi2s_suspend_intf_clks(struct platform_device *pdev)
{
	struct hsi2s_device *hs_dev;

	hs_dev = (struct hsi2s_device *)platform_get_drvdata(pdev);
	if (hs_dev->intf_clk)
		clk_disable_unprepare(hs_dev->intf_clk);
}

/* Function to resume interface clocks */
static int hsi2s_resume_intf_clks(struct platform_device *pdev)
{
	struct hsi2s_device *hs_dev;
	int ret = 0;

	hs_dev = (struct hsi2s_device *)platform_get_drvdata(pdev);
	if (hs_dev->intf_clk) {
		ret = clk_prepare_enable(hs_dev->intf_clk);
		if (ret) {
			pr_err("[HSI2S] Failed to enable interface clock for SDR%d",
			       hs_dev->minor_num);
		}
	}

	return ret;
}

/* RDDMA Scheduler */
static int rddma_schedule(void *data)
{
	struct hsi2s_device *hs_dev;

	hs_dev = (struct hsi2s_device *)data;
	pr_warn("[HSI2S] Starting RDDMA scheduler...");

	while (1) {
		if (kthread_should_stop()) {
			pr_warn("[HSI2S] RDDMA scheduler asked to exit...");
			break;
		}

		msleep(1);

		if (!hs_dev->rddma_copy_busy) {
			hs_dev->rddma_copy_busy = 1;

			/* Enable the DMA channel */
			setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_en);
			/* Enable speaker */
			setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_spkr_en);
			/* Set the RDDMA busy flag */
			hs_dev->rddma_xfer_busy = 1;
			pr_warn("[HSI2S] DMA scheduled on hs%d interface",hs_dev->minor_num);
		}
	}

	return 0;
}

/* Interrupt thread function */
static irq_handler_t irq_thread_fn(int irq, void *devid)
{
	u32 temp_len;
	u32 write_len;
	u32 irq_stat;
	struct hsi2s_device **hs_arr;
	int slave;
	void *tail;

	hs_arr = hsi2s_core->hsi2s_arr;
	mutex_lock(&hsi2s_core->irqlock);

	/* Checking for read DMA interrupt on HS0 interface */
	if (hs_arr[0]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on read channel 0 */
		if (irq_stat & IRQ_PER_RDDMA_CH0) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_RDDMA_CH0);
			hs_arr[0]->read_buffer->last_xfer = !hs_arr[0]->read_buffer->last_xfer;
			/* Notify event write */
			wake_up_interruptible(&hs_arr[0]->wq_rddma);
		}
	}

	/* Checking for read DMA interrupt on HS1 interface */
	if (hs_arr[1]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on read channel 1 */
		if (irq_stat & IRQ_PER_RDDMA_CH1) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_RDDMA_CH1);
			hs_arr[1]->read_buffer->last_xfer = !hs_arr[1]->read_buffer->last_xfer;
			/* Notify event write */
			wake_up_interruptible(&hs_arr[1]->wq_rddma);
		}
	}

	/* Checking for read DMA interrupt on HS2 interface */
	if (hs_arr[2]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on read channel 2 */
		if (irq_stat & IRQ_PER_RDDMA_CH2) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_RDDMA_CH2);
			hs_arr[2]->read_buffer->last_xfer = !hs_arr[2]->read_buffer->last_xfer;
			/* Notify event write */
			wake_up_interruptible(&hs_arr[2]->wq_rddma);
		}
	}

	/* Checking for write DMA interrupt on HS0 interface */
	if (hs_arr[0]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on write channel 0 */
		if (irq_stat & IRQ_PER_WRDMA_CH0) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_WRDMA_CH0);

			write_len = readl_relaxed(hs_arr[0]->wrdma_per_len);
			write_len *= BYTES_PER_SAMPLE;

			tail = hs_arr[0]->write_buffer->tail;
			if (tail + write_len >= hs_arr[0]->lpass_wrdma_end) {
				temp_len = hs_arr[0]->lpass_wrdma_end - tail;
				tail = hs_arr[0]->lpass_wrdma_start + (write_len - temp_len);
			}
			else
				tail += write_len;
			hs_arr[0]->write_buffer->tail = tail;
			hs_arr[0]->write_buffer->data_ready = 1;
			hs_arr[0]->write_buffer->pollin = 1;
			/* Notify event read */
			wake_up_interruptible(&hs_arr[0]->wq_wrdma);
		}
	}

	/* Checking for write DMA interrupt on HS1 interface */
	if (hs_arr[1]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on write channel 1 */
		if (irq_stat & IRQ_PER_WRDMA_CH1) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_WRDMA_CH1);

			write_len = readl_relaxed(hs_arr[1]->wrdma_per_len);
			write_len *= BYTES_PER_SAMPLE;

			tail = hs_arr[1]->write_buffer->tail;
			if (tail + write_len >= hs_arr[1]->lpass_wrdma_end) {
				temp_len = hs_arr[1]->lpass_wrdma_end - tail;
				tail = hs_arr[1]->lpass_wrdma_start + (write_len - temp_len);
			}
			else
				tail += write_len;
			hs_arr[1]->write_buffer->tail = tail;
			hs_arr[1]->write_buffer->data_ready = 1;
			hs_arr[1]->write_buffer->pollin = 1;
			/* Notify event read */
			wake_up_interruptible(&hs_arr[1]->wq_wrdma);
		}
	}

	/* Checking for write DMA interrupt on HS2 interface */
	if (hs_arr[2]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on write channel 2 */
		if (irq_stat & IRQ_PER_WRDMA_CH2) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_WRDMA_CH2);

			write_len = readl_relaxed(hs_arr[2]->wrdma_per_len);
			write_len *= BYTES_PER_SAMPLE;

			tail = hs_arr[2]->write_buffer->tail;
			if (tail + write_len >= hs_arr[2]->lpass_wrdma_end) {
				temp_len = hs_arr[2]->lpass_wrdma_end - tail;
				tail = hs_arr[2]->lpass_wrdma_start + (write_len - temp_len);
			}
			else
				tail += write_len;
			hs_arr[2]->write_buffer->tail = tail;
			hs_arr[2]->write_buffer->data_ready = 1;
			hs_arr[2]->write_buffer->pollin = 1;
			/* Notify event read */
			wake_up_interruptible(&hs_arr[2]->wq_wrdma);
		}
	}

	if (hsi2s_core->is_rate_enabled) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		if (irq_stat & IRQ_PRI_RD_DIFF_RATE) {
			setbits(hsi2s_core->irq_clear, IRQ_PRI_RD_DIFF_RATE);
			/* Get the new WS rate */
			hsi2s_core->pri_ws_rate = get_ws_rate(PRI_RATE_DET);
			pr_warn("[HSI2S] WS rate detected as %lu Hz ", hsi2s_core->pri_ws_rate);
			if (hsi2s_core->pri_ws_rate) {
				slave = hsi2s_core->pri_rate_interface;
				/* Disable mic */
				clearbits(hs_arr[slave]->i2s_ctl, hsi2s_core->macro->bit_mic_en);
				clearbits(hs_arr[slave]->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
				/* Set the new periodic length */
				hs_arr[slave]->wrdma_periodic_length_bytes = (set_periodic_length(calculate_bit_rate(hs_arr[slave], PRI_RATE_DET),
															  hs_arr[slave]->data_buffer_ms_val));
				hs_arr[slave]->wrdma_periodic_length = hs_arr[slave]->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
				pr_warn("[HSI2S] Periodic length reconfigured to %lu words", hs_arr[slave]->wrdma_periodic_length);
				writel_relaxed(hs_arr[slave]->wrdma_periodic_length, hs_arr[slave]->wrdma_per_len);
				/* Enable mic */
				setbits(hs_arr[slave]->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
				setbits(hs_arr[slave]->i2s_ctl, hsi2s_core->macro->bit_mic_en);
			}
		}

		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		if (irq_stat & IRQ_SEC_RD_DIFF_RATE) {
			setbits(hsi2s_core->irq_clear, IRQ_SEC_RD_DIFF_RATE);
			/* Get the new WS rate */
			hsi2s_core->sec_ws_rate = get_ws_rate(SEC_RATE_DET);
			pr_warn("[HSI2S] WS rate detected as %lu Hz ", hsi2s_core->sec_ws_rate);
			if (hsi2s_core->sec_ws_rate) {
				slave = hsi2s_core->sec_rate_interface;
				/* Disable mic */
				clearbits(hs_arr[slave]->i2s_ctl, hsi2s_core->macro->bit_mic_en);
				clearbits(hs_arr[slave]->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
				/* Set the new periodic length */
				hs_arr[slave]->wrdma_periodic_length_bytes = (set_periodic_length(calculate_bit_rate(hs_arr[slave],SEC_RATE_DET),
															  hs_arr[slave]->data_buffer_ms_val));
				hs_arr[slave]->wrdma_periodic_length = hs_arr[slave]->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
				pr_warn("[HSI2S] Periodic length reconfigured to %lu words", hs_arr[slave]->wrdma_periodic_length);
				writel_relaxed(hs_arr[slave]->wrdma_periodic_length, hs_arr[slave]->wrdma_per_len);
				/* Enable mic */
				setbits(hs_arr[slave]->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
				setbits(hs_arr[slave]->i2s_ctl, hsi2s_core->macro->bit_mic_en);
			}
		}
	}

	mutex_unlock(&hsi2s_core->irqlock);

	return (irq_handler_t)IRQ_HANDLED;
}

/* Interrupt handler function */
static irq_handler_t i2s_interrupt_handler(int irq, void *dev_id,
					   struct pt_regs *regs)
{
	return (irq_handler_t)IRQ_WAKE_THREAD;
}

/* File operation functions for character drivers */

/* Function to read from the Rx buffer and transfer data to user space */
static ssize_t device_read(struct file *file, char *buffer,
			   size_t length, loff_t *offset)
{
	struct hsi2s_device *hs_dev;
	int temp_length;
	int copy_len;
	int bytes_read = 0;
	int ret = 0;
	void *head;

	hs_dev = (struct hsi2s_device *)file->private_data;
	head = hs_dev->write_buffer->head;

	temp_length = readl_relaxed(hs_dev->wrdma_per_len) * 4;

	while (length > temp_length) {
		if (head == hs_dev->write_buffer->tail) {
			wait_event_interruptible(hs_dev->wq_wrdma,
			hs_dev->write_buffer->data_ready == 1);
		}

		hs_dev->write_buffer->data_ready = 0;

		if (head + temp_length >= hs_dev->lpass_wrdma_end) {
			usleep_range(10000,10000);
			dma_sync_single_for_cpu(hs_dev->dev, hs_dev->write_buffer->handle, dma_buffer_length, DMA_FROM_DEVICE);
			copy_len = hs_dev->lpass_wrdma_end - head;
			ret = copy_to_user(buffer + bytes_read,
					   head,
					   copy_len);
			if (ret) {
				pr_err("[HSI2S] Error copying data to userspace");
				return -ret;
			}
			bytes_read += copy_len;
			dma_sync_single_for_cpu(hs_dev->dev, hs_dev->write_buffer->handle, dma_buffer_length, DMA_FROM_DEVICE);
			ret = copy_to_user(buffer + bytes_read,
				   hs_dev->lpass_wrdma_start,
				   temp_length - copy_len);
			if (ret) {
				pr_err("[HSI2S] Error copying data to userspace");
				return -ret;
			}
			head = hs_dev->lpass_wrdma_start + (temp_length - copy_len);
			bytes_read += (temp_length - copy_len);
		} else  {
			usleep_range(10000,10000);
			dma_sync_single_for_cpu(hs_dev->dev, hs_dev->write_buffer->handle, dma_buffer_length, DMA_FROM_DEVICE);
			ret = copy_to_user(buffer + bytes_read,
					   head,
					   temp_length);
			if (ret) {
				pr_err("[HSI2S] Error copying data to userspace");
				return -ret;
			}
			head += temp_length;
			bytes_read += temp_length;
		}
		hs_dev->write_buffer->head = head;
		length -= temp_length;
	}

	if (hs_dev->write_buffer->head == hs_dev->write_buffer->tail) {
		wait_event_interruptible(hs_dev->wq_wrdma,
		hs_dev->write_buffer->data_ready == 1);
	}

	hs_dev->write_buffer->data_ready = 0;

	if (head + length >= hs_dev->lpass_wrdma_end) {
		usleep_range(10000,10000);
		dma_sync_single_for_cpu(hs_dev->dev, hs_dev->write_buffer->handle, dma_buffer_length, DMA_FROM_DEVICE);
		copy_len = hs_dev->lpass_wrdma_end - head;
		ret = copy_to_user(buffer + bytes_read,
				   head,
				   copy_len);
		if (ret) {
			pr_err("[HSI2S] Error copying data to userspace");
			return -ret;
		}
		bytes_read += copy_len;
		dma_sync_single_for_cpu(hs_dev->dev, hs_dev->write_buffer->handle, dma_buffer_length, DMA_FROM_DEVICE);
		ret = copy_to_user(buffer + bytes_read,
			   hs_dev->lpass_wrdma_start,
			   length - copy_len);
		if (ret) {
			pr_err("[HSI2S] Error copying data to userspace");
			return -ret;
		}
		head = hs_dev->lpass_wrdma_start + (length - copy_len);
		bytes_read += (length - copy_len);
	} else  {
		usleep_range(10000,10000);
		if (!hs_dev->minor_num)
			dma_sync_single_for_cpu(hs_dev->dev, hs_dev->write_buffer->handle, dma_buffer_length, DMA_FROM_DEVICE);
		ret = copy_to_user(buffer + bytes_read,
				   head,
				   length);
		if (ret) {
			pr_err("[HSI2S] Error copying data to userspace");
			return -ret;
		}
		head += length;
		bytes_read += length;
	}
	hs_dev->write_buffer->head = head;

	return bytes_read;
}

/* Function to write data from user space to the Tx buffer */
static ssize_t device_write(struct file *file, const char *buffer,
			    size_t length, loff_t *offset)
{
	struct hsi2s_device *hs_dev;
	int bytes_written = 0;
	int temp_length;

	hs_dev = (struct hsi2s_device *)file->private_data;
	temp_length = hs_dev->read_buffer->length;

	while (length > temp_length) {
		if (hs_dev->rddma_in_progress) {
			if (!(hs_dev->read_buffer->last_copy ^ hs_dev->read_buffer->last_xfer))
				wait_event_interruptible(hs_dev->wq_rddma,
							(hs_dev->read_buffer->last_copy ^ hs_dev->read_buffer->last_xfer) == 1);
		}

		if (hs_dev->read_buffer->last_copy) {
			copy_from_user(hs_dev->read_buffer->ping_start,
				       buffer + bytes_written,
				       temp_length);
		} else {
			copy_from_user(hs_dev->read_buffer->pong_start,
				       buffer + bytes_written,
				       temp_length);
		}

		dma_sync_single_for_device(hs_dev->dev, hs_dev->read_buffer->handle, dma_buffer_length, DMA_TO_DEVICE);
		hs_dev->read_buffer->last_copy = !hs_dev->read_buffer->last_copy;
		bytes_written += temp_length;
		length -= temp_length;

		if (!hs_dev->rddma_in_progress) {
			hs_dev->rddma_copy_busy = 0;
			hs_dev->rddma_in_progress = 1;
		}
	}

	if (hs_dev->rddma_in_progress) {
		if (!(hs_dev->read_buffer->last_copy ^ hs_dev->read_buffer->last_xfer))
			wait_event_interruptible(hs_dev->wq_rddma,
						(hs_dev->read_buffer->last_copy ^ hs_dev->read_buffer->last_xfer) == 1);
	}

	if (hs_dev->read_buffer->last_copy) {
		memset(hs_dev->read_buffer->ping_start, 0, hs_dev->read_buffer->length);
		copy_from_user(hs_dev->read_buffer->ping_start,
			       buffer + bytes_written,
			       length);
	} else {
		memset(hs_dev->read_buffer->pong_start, 0, hs_dev->read_buffer->length);
		copy_from_user(hs_dev->read_buffer->pong_start,
			       buffer + bytes_written,
			       length);
	}
	dma_sync_single_for_device(hs_dev->dev, hs_dev->read_buffer->handle, dma_buffer_length, DMA_TO_DEVICE);
	hs_dev->read_buffer->last_copy = !hs_dev->read_buffer->last_copy;
	bytes_written += length;

	if (!hs_dev->rddma_in_progress) {
		hs_dev->rddma_copy_busy = 0;
		hs_dev->rddma_in_progress = 1;
	}

	return bytes_written;
}

/* Called when a process attempts to open the device file */
static int device_open(struct inode *inode, struct file *file)
{
	int minor_num;

	/* Find the minor number of the device */
	minor_num = MINOR(inode->i_rdev);

	if (hsi2s_core->hsi2s_arr[minor_num]->client_count) {
		pr_warn("[HSI2S] Device busy");
		return -EBUSY;
	}

	/* Increment client count */
	hsi2s_core->hsi2s_arr[minor_num]->client_count++;
	pr_warn("[HSI2S] Client connected. Active clients : %d",
		hsi2s_core->hsi2s_arr[minor_num]->client_count);

	/* Store the platform device pointer */
	file->private_data = hsi2s_core->hsi2s_arr[minor_num];

	/* Increment usage count to be able to properly close the module. */
	try_module_get(THIS_MODULE);

	return 0;
}

/* Called when the a process closes the device file */
static int device_release(struct inode *inode, struct file *file)
{
	struct hsi2s_device *hs_dev;

	/* Decrement client count */
	hs_dev = (struct hsi2s_device *)file->private_data;
	hs_dev->client_count--;
	pr_warn("[HSI2S] Client disconnected. Active clients : %d",
		hs_dev->client_count);

	/* Decrement usage count to be able to properly close the module. */
	module_put(THIS_MODULE);

	return 0;
}

/* IOCTL handler */
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct hsi2s_device *hs_dev;
	struct hsi2s_params *params;
	void __iomem *clk_val_reg;
	void __iomem *clk_update_reg;
	int minor;
	int ret = 0;

	hs_dev = (struct hsi2s_device *)file->private_data;
	minor = hs_dev->minor_num;

	switch (cmd) {
	case I2S_NORMAL_MODE:
		pr_warn("[HSI2S] Triggering normal operation");
		if (hs_dev->client_count == 1) {
			/* Setting slave mode for SA8155 target */
			if (hsi2s_core->target == 8155)
				configure_muxmode(hs_dev, 1);
			configure_normal_mode(hs_dev, minor);
			hs_dev->slave = minor;
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	case I2S_INTERNAL_LOOPBACK:
		pr_warn("[HSI2S] Triggering internal loopback");
		if (hs_dev->client_count == 1) {
			configure_int_loopback_mode(hs_dev, minor);
			hs_dev->slave = minor;
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	case I2S_EXTERNAL_LOOPBACK:
		if (hsi2s_core->target == 6155) {
			pr_warn("[HSI2S] Mode not supported by target");
			return -EINVAL;
		}
		pr_warn("[HSI2S] Triggering external loopback on master");
		if (hs_dev->client_count == 1) {
			configure_muxmode(hs_dev, 0);
			configure_ext_loopback_mode(hs_dev, minor);
			hs_dev->slave = minor;
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	case I2S_MUXMODE:
		if (hsi2s_core->target == 6155) {
			pr_warn("[HSI2S] Mode not supported by target");
			return -EINVAL;
		}
		pr_warn("[HSI2S] Setting master/slave muxmode configuration");
		if (hs_dev->client_count == 1)
			configure_muxmode(hs_dev, arg);
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	case I2S_SPEAKER:
		pr_warn("[HSI2S] Configuring hs%d as speaker",hs_dev->minor_num);
		if (hs_dev->client_count == 1) {
			configure_i2s_spkr(hs_dev);
			configure_rddma(hs_dev, minor);
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	case I2S_MIC:
		pr_warn("[HSI2S] Configuring hs%d as mic",hs_dev->minor_num);
		if (hs_dev->client_count == 1) {
			configure_i2s_mic(hs_dev);
			configure_wrdma(hs_dev, minor);
			setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	case I2S_SET_SLAVE:
		if (hsi2s_core->target == 6155) {
			pr_warn("[HSI2S] Mode not supported by target");
			return -EINVAL;
		}
		pr_warn("[HSI2S] Triggering external loopback with hs%d master and hs%d slave", hs_dev->minor_num, arg);
		if (hs_dev->client_count == 1) {
			hs_dev->slave = arg;
			hs_dev->mode = EXTERNAL_LB_MASTER_SLAVE;
			hsi2s_core->hsi2s_arr[arg]->mode = EXTERNAL_LB_MASTER_SLAVE;
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	case I2S_INIT_TX:
		if (hs_dev->client_count == 1) {
			hs_dev->rddma_copy_busy = 1;
			/* Enable the DMA channel */
			setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_en);
			/* Enable speaker */
			setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_spkr_en);
			/* Set the RDDMA busy flags */
			hs_dev->rddma_xfer_busy = 1;
			hs_dev->rddma_in_progress = 1;
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;
	case I2S_DEINIT_TX:
		if (hs_dev->client_count == 1) {
			pr_warn("[HSI2S] Stopping rddma");
			hs_dev->rddma_copy_busy = 1;
			/* Disable speaker */
			clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_spkr_en);
			/* Disable the DMA channel */
			clearbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_en);
			/* Clear the RDDMA busy flags */
			hs_dev->rddma_xfer_busy = 0;
			hs_dev->rddma_in_progress = 0;
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;
	case I2S_CONFIG_PARAMS:
		if (hs_dev->client_count == 1) {
			pr_warn("[HSI2S] Configuring I2S parameters from test application");
			params = kzalloc(sizeof(params), GFP_KERNEL);
			if (!params) {
				pr_err("[HSI2S] Failed to allocate params structure");
				ret = -ENOMEM;
				break;
			}
			copy_from_user(params, (void *)arg, sizeof(struct hsi2s_params));
			ret = configure_i2s_params(hs_dev, params);
			if (ret < 0) {
				pr_err("[HSI2S] Failed to configure I2S parameters");
				ret = -EINVAL;
			}
			kfree(params);
			params = NULL;
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;
	case I2S_SET_CLOCK:
		if (hsi2s_core->target == 6155) {
			pr_warn("[HSI2S] Master mode not supported by target");
			return -EINVAL;
		}

		pr_warn("[HSI2S] Configuring master clock on HS%d interface", hs_dev->minor_num);

		if (hs_dev->client_count == 1) {
			if (hs_dev->minor_num == 0) {
				clk_update_reg = ioremap(HS0_BITCLK_CMD,4);
				clk_val_reg = ioremap(HS0_BITCLK_CFG,4);

				clearbits(clk_val_reg, HS_BITCLK_RESET);
				setbits(clk_val_reg, arg);
				setbits(clk_update_reg, HS_BITCLK_UPDATE);
			}
			else if (hs_dev->minor_num == 1) {
				clk_update_reg = ioremap(HS1_BITCLK_CMD,4);
				clk_val_reg = ioremap(HS1_BITCLK_CFG,4);

				clearbits(clk_val_reg, HS_BITCLK_RESET);
				setbits(clk_val_reg, arg);
				setbits(clk_update_reg, HS_BITCLK_UPDATE);
			}
			else {
				clk_update_reg = ioremap(HS2_BITCLK_CMD,4);
				clk_val_reg = ioremap(HS2_BITCLK_CFG,4);

				clearbits(clk_val_reg, HS_BITCLK_RESET);
				setbits(clk_val_reg, arg);
				setbits(clk_update_reg, HS_BITCLK_UPDATE);
			}
			pr_warn("[HSI2S] Re-configured master clock");
		}
		else
			pr_warn("[HSI2S] Clock already set by previous client");
		break;
	case I2S_RESET:
		if (hs_dev->client_count == 1) {
			pr_warn("[HSI2S] Resetting I2S control register");
			reg_clear(hs_dev->i2s_ctl);
			pr_warn("[HSI2S] Resetting DMA registers");
			reset_rddma_registers(hs_dev);
			reset_wrdma_registers(hs_dev);
			/* Clear IRQs */
			clear_irqs();
			/* Reset buffer pointers */
			memset(hs_dev->read_buffer->buffer, 0, dma_buffer_length);
			hs_dev->read_buffer->last_copy = 1;
			hs_dev->read_buffer->last_xfer = 1;
			memset(hs_dev->write_buffer->buffer, 0, dma_buffer_length);
			hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
			hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
			hs_dev->write_buffer->data_ready = 0;
			hs_dev->write_buffer->pollin = 0;
			hs_dev->rddma_xfer_busy = 0;
			hs_dev->rddma_copy_busy = 1;
			hs_dev->rddma_in_progress = 0;
		}
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	default:
		return -EINVAL;
	}

	return ret;
}

static unsigned int device_poll(struct file *file, poll_table *wait)
{
	struct hsi2s_device *hs_dev;
	unsigned int mask = 0;

	hs_dev = (struct hsi2s_device *)file->private_data;

	poll_wait(file, &hs_dev->wq_wrdma, wait);

	/* Check for periodic interrupt on write DMA channel */
	if (hs_dev->write_buffer->pollin) {
		hs_dev->write_buffer->pollin = 0;
		mask |= POLLIN | POLLRDNORM;
	}

	return mask;
}

static int device_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct hsi2s_device *hs_dev;
	unsigned long pa;
	unsigned long pfn;
	unsigned long len = vma->vm_end - vma->vm_start;
	int ret = 0;

	hs_dev = (struct hsi2s_device *)file->private_data;
	pa = virt_to_phys(hs_dev->write_buffer->buffer);
	pfn = (pa >> PAGE_SHIFT) + vma->vm_pgoff;

	if (len > dma_buffer_length) {
		pr_err("[HSI2S] Size of map area exceeds DMA buffer length");
		ret = -EINVAL;
	} else {
		ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
		if (ret)
			pr_err("[HSI2S] %s failed", __func__);
	}

	return ret;
}

static const struct file_operations fops = {
	.read  = device_read,
	.write = device_write,
	.open  = device_open,
	.release = device_release,
	.unlocked_ioctl = device_ioctl,
	.mmap = device_mmap,
	.poll = device_poll
};

/* Module callbacks */

static const struct of_device_id hsi2s_idtable[] = {
	{ .compatible = "qcom,hsi2s", },
	{ .compatible = "qcom,hsi2s-interface", },
	{ },
};
MODULE_DEVICE_TABLE(of, hsi2s_idtable);

/* Probe function for the child interface nodes */
static int hsi2s_interface_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct hsi2s_device *hs_dev;
	char *devname;
	int minor = 0;
	int ret = 0;
	u32 bit_clk = 0;
	u32 buffer_ms = 0;
	u32 ch_count = 0;
	u32 b_depth = 0;

	/* Read the device minor number */
	ret = of_property_read_u32(dev->of_node, "minor-number",
				   &minor);
	if (ret) {
		pr_err("[HSI2S] Resource 'minor-number' unavailable in dtsi");
		goto err_out;
	}

	/* Allocate the hsi2s device structure */
	hsi2s_core->hsi2s_arr[minor] =
		kzalloc(sizeof(*hsi2s_core->hsi2s_arr[minor]), GFP_KERNEL);

	if (!hsi2s_core->hsi2s_arr[minor]) {
		ret = -ENOMEM;
		goto err_out;
	}

	hs_dev = hsi2s_core->hsi2s_arr[minor];

	/* Set the hsi2s device structure as platform data */
	platform_set_drvdata(pdev, hs_dev);

	/* Store the device pointer */
	hs_dev->dev = dev;

	/* Store the minor number */
	hs_dev->minor_num = minor;

	if (hsi2s_core->target == 6155) {
		/* Enable the interface clocks */
		ret = hsi2s_enable_intf_clks(pdev);
		if (ret)
			goto err_free_hsdev;
	}

	/* Configure the I2S parameters */

	/* Bit clock  */
	ret = of_property_read_u32(dev->of_node, "bit-clock-hz",
				   &bit_clk);
	if (ret)
		pr_warn("[HSI2S] Resource 'bit-clock-hz' unavailable in dtsi");

	if (bit_clock_hz)
		bit_clk = bit_clock_hz;

	if (!bit_clk) {
		pr_err("[HSI2S] Bit clock not configured. Exiting...");
		ret = -EINVAL;
		goto err_disable_intf_clock;
	}

	/* Data buffer in ms */
	ret = of_property_read_u32(dev->of_node, "data-buffer-ms",
				   &buffer_ms);
	if (ret)
		pr_warn("[HSI2S] Resource 'data-buffer-ms' unavailable in dtsi");

	if (data_buffer_ms)
		buffer_ms = data_buffer_ms;

	if (!buffer_ms) {
		pr_err("[HSI2S] Data buffer in ms not configured. Exiting...");
		ret = -EINVAL;
		goto err_disable_intf_clock;
	}
	hs_dev->data_buffer_ms_val = buffer_ms;

	/* Set the periodic length */
	hs_dev->wrdma_periodic_length_bytes = set_periodic_length(bit_clk, buffer_ms);
	hs_dev->wrdma_periodic_length = hs_dev->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
	pr_warn("[HSI2S] Periodic length configured as %u words", hs_dev->wrdma_periodic_length);

	/* Bit depth */
	ret = of_property_read_u32(dev->of_node, "bit-depth",
				  &b_depth);
	if (ret)
		pr_warn("[HSI2S] Resource 'bit-depth' unavailable in dtsi");

	if (bit_depth)
		b_depth = bit_depth;

	configure_bit_depth(hs_dev, b_depth);

	/* Speaker channel count */
	ret = of_property_read_u32(dev->of_node, "spkr-channel-count",
				   &ch_count);
	if (ret)
		pr_warn("[HSI2S] Resource 'spkr-channel-count' unavailable in dtsi");

	if (channel_count)
		ch_count = channel_count;

	configure_spkr_channel(hs_dev, ch_count);

	/* Mic channel count */
	ret = of_property_read_u32(dev->of_node, "mic-channel-count",
				   &ch_count);
	if (ret)
		pr_warn("[HSI2S] Resource 'spkr-channel-count' unavailable in dtsi");

	if (channel_count)
		ch_count = channel_count;

	configure_mic_channel(hs_dev, ch_count);

	/* Map the interface registers */
	ret = init_default(hs_dev, minor);
	if (ret < 0) {
		pr_err("[HSI2S] Failed to set default settings for hsi2s device");
		goto err_deinit_default;
	}

	#ifndef CONFIG_QTI_GVM
	/* Configure SMMU */
	ret = hsi2s_smmu_init(pdev, minor);
	if (ret) {
		pr_err("[HSI2S] Failed to init smmu");
		goto err_free_smmu;
	}
	#endif

	/* Configure the gpios */
	if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
		hs_dev->is_pinctrl_names = true;
		ret = hsi2s_configure_gpio_pins(pdev);
		if (ret < 0) {
			pr_err("[HSI2S] Failed to configure gpios");
			goto err_free_smmu;
		}
	}

	hs_dev->rddma_xfer_busy = 0;
	hs_dev->rddma_copy_busy = 1;
	hs_dev->rddma_in_progress = 0;

	/* Start the read DMA scheduler */
	hs_dev->rddma_thread = kthread_create(rddma_schedule, hs_dev,
					      "DMA scheduler thread");
	if (hs_dev->rddma_thread) {
		wake_up_process(hs_dev->rddma_thread);
	} else {
		ret = -EINVAL;
		pr_err("Cannot create rddma scheduler thread");
		goto err_free_smmu;
	}

	/* Configure the operational mode */
	if (operation_mode)
		configure_normal_mode(hs_dev, minor);
	else
		configure_int_loopback_mode(hs_dev, minor);

	/* Create device file for the interface */
	hs_dev->cdev_sdr = kzalloc(sizeof(*hs_dev->cdev_sdr),
				   GFP_KERNEL);
	if (!hs_dev->cdev_sdr) {
		ret = -ENOMEM;
		goto err_stop_thread;
	}

	hs_dev->curr_devid = MKDEV(MAJOR(devid), MINOR(devid) + minor);
	cdev_init(hs_dev->cdev_sdr, &fops);
	hs_dev->cdev_sdr->owner = THIS_MODULE;
	ret = cdev_add(hs_dev->cdev_sdr, hs_dev->curr_devid, 1);
	if (ret < 0) {
		pr_err("[HSI2S] Unable to add cdev for sdr%d interface"
		       , minor);
		goto err_free_cdev;
	}

	if (!minor)
		devname = SDR0;
	else if (minor == 1)
		devname = SDR1;
	else if (minor == 2)
		devname = SDR2;

	hs_dev->class_sdr = class_create(THIS_MODULE,
					 devname);
	if (!hs_dev->class_sdr) {
		pr_err("[HSI2S] Failed to create device class %d"
		       , minor);
		ret = -EEXIST;
		goto err_delete_cdev;
	}
	if (!device_create(hs_dev->class_sdr, NULL, hs_dev->curr_devid,
			   NULL, "hs%d_i2s", minor)) {
		pr_err("[HSI2S] Failed to create device file for sdr%d interface"
		       , minor);
		ret = -EINVAL;
		goto err_class_destroy;
	}

	hs_dev->client_count = 0;

	goto err_out;

err_class_destroy:
	class_destroy(hs_dev->class_sdr);
err_delete_cdev:
	cdev_del(hs_dev->cdev_sdr);
err_free_cdev:
	kfree(hs_dev->cdev_sdr);
	hs_dev->cdev_sdr = NULL;
err_stop_thread:
	kthread_stop(hs_dev->rddma_thread);
err_free_smmu:
	#ifndef CONFIG_QTI_GVM
	/* Detach and release iommu mapping */
	if (hs_dev->hsi2s_smmu_ctx->valid) {
		if (hs_dev->hsi2s_smmu_ctx->smmu_pdev)
			arm_iommu_detach_device(&hs_dev->hsi2s_smmu_ctx->smmu_pdev->dev);
		if (hs_dev->hsi2s_smmu_ctx->mapping)
			arm_iommu_release_mapping(hs_dev->hsi2s_smmu_ctx->mapping);
		hs_dev->hsi2s_smmu_ctx->valid = false;
		hs_dev->hsi2s_smmu_ctx->mapping = NULL;
		hs_dev->hsi2s_smmu_ctx->pdev_master = NULL;
		hs_dev->hsi2s_smmu_ctx->smmu_pdev = NULL;
		pr_warn("[HSI2S] Detach and release iommu mapping");
	}
	kfree(hs_dev->hsi2s_smmu_ctx);
	hs_dev->hsi2s_smmu_ctx = NULL;
	#endif
err_deinit_default:
	hsi2s_buffer_free(hs_dev);
err_disable_intf_clock:
	if (hsi2s_core->target == 6155)
		hsi2s_disable_intf_clks(pdev);
err_free_hsdev:
	kfree(hs_dev);
	hs_dev = NULL;
	hsi2s_core->hsi2s_arr[minor] = NULL;
err_out:
	/* Enable the IRQ line */
	if (minor == (hsi2s_core->i_count - 1) && !(hsi2s_core->is_irq_enabled)) {
		if (hsi2s_core->irq0 > 0) {
			hsi2s_core->desc = irq_to_desc(hsi2s_core->irq0);
			if (hsi2s_core->desc->core_internal_state__do_not_mess_with_it & 0x00000200) {
				pr_warn("[HSI2S] Removing pending IRQs");
				hsi2s_core->desc->core_internal_state__do_not_mess_with_it &= ~(0x00000200);
			}
			pr_warn("[HSI2S] Enabling IRQ line");
			enable_irq(hsi2s_core->irq0);
			hsi2s_core->is_irq_enabled = true;
		}
	}
	return ret;
}

/* Function to initialise the device */
static int hsi2s_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct resource *resource = NULL;
	int interface_count = 0;
	int rate_detector_count = 0;
	u32 target;
	u32 rate_array[2];
	int ret = 0;

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,hsi2s-interface"))
		return hsi2s_interface_probe(pdev);

	hsi2s_core = kzalloc(sizeof(*hsi2s_core), GFP_KERNEL);
	if (!hsi2s_core)
		return -ENOMEM;

	/* Set the hsi2s_core structure as platform data */
	platform_set_drvdata(pdev, hsi2s_core);

	/* Read the target property */
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,sa6155-hsi2s"))
		target = 6155;
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,sa8155-hsi2s"))
		target = 8155;
	else {
		pr_err("[HSI2S] Uncompatible target");
		goto err_free_core;
	}

	/* Allocate target macro structure */
	hsi2s_core->macro = kzalloc(sizeof(struct hsi2s_macros), GFP_KERNEL);
	if (!hsi2s_core->macro)
		return -ENOMEM;

	/* Assign target specific macros */
	if (target == 6155) {
		pr_warn("[HSI2S] Talos target detected");
		t_assign_macros();
	}
	else if (target == 8155) {
		pr_warn("[HSI2S] Hana target detected");
		h_assign_macros();
	}

	hsi2s_core->target = target;

	/* Read the number of HS-I2S interfaces */
	ret = of_property_read_u32(dev->of_node, "number-of-interfaces",
				   &interface_count);
	if (ret) {
		pr_err("[HSI2S] Resource 'number-of-interfaces' unavailable in dtsi");
		goto err_free_macro;
	}

	/* Store the interface count */
	hsi2s_core->i_count = interface_count;

	/* Set the write DMA buffer length */
	if (dma_buffer_length < 1 || dma_buffer_length > 4) {
		pr_warn("[HSI2S] No valid buffer length entered. Setting 4MB default");
		dma_buffer_length = DEFAULT_BUFF_LEN_BYTES;
	} else {
		/* Converting into bytes */
		dma_buffer_length *= (1024 * 1024);
	}
	pr_warn("[HSI2S] Write DMA buffer length set to %u bytes", dma_buffer_length);

	dma_buffer_length_words = ((dma_buffer_length / 4) - 1);

	/* Register the character device numbers */
	ret = alloc_chrdev_region(&devid, 0, interface_count,
				  "hsi2s_dev");
	if (ret < 0) {
		pr_err("[HSI2S] Major number allocation failed");
		goto err_free_macro;
	}

	/* Allocate hsi2s_dev structure pointers */
	hsi2s_core->hsi2s_arr = kcalloc(interface_count,
					sizeof(struct hsi2s_device *),
					GFP_KERNEL);

	/* Enable the core clocks */
	if (target == 6155) {
		ret = hsi2s_enable_core_clks(pdev);
		if (ret)
			goto err_free_macro;
	}
	else if (target == 8155) {
		h_modify_core_clks(1);
		h_modify_interface_clks(1);
	}

	/* Map the register memory region */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"lpa_if");
	if (!resource) {
		pr_err("[HSI2S] get lpa_if resource failed");
		ret = -ENODEV;
		goto err_disable_core_clocks;
	}

	hsi2s_core->lpaif_base_va = devm_ioremap_resource(&pdev->dev,
								  resource);
	if (IS_ERR(hsi2s_core->lpaif_base_va)) {
		pr_err("[HSI2S] ioremap failed");
		ret = PTR_ERR(hsi2s_core->lpaif_base_va);
		goto err_disable_core_clocks;
	}

	if (target == 8155) {
		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"lpass_tcsr");
		if (!resource) {
			pr_err("[HSI2S] get lpass_tcsr resource failed");
			ret = -ENODEV;
			goto err_iounmap_lpaif;
		}

		hsi2s_core->lpass_tcsr_base_va =
		devm_ioremap_resource(&pdev->dev, resource);

		if (IS_ERR(hsi2s_core->lpass_tcsr_base_va)) {
			pr_err("[HSI2S] ioremap failed");
			ret = PTR_ERR(hsi2s_core->lpass_tcsr_base_va);
			goto err_iounmap_lpaif;
		}

	}

	/* Map the core registers */
	ret = map_core_registers();
	if (ret < 0) {
		pr_err("[HSI2S] Unable to map core registers");
		goto err_iounmap_lpass_tcsr;
	}

	ret = of_property_read_u32(dev->of_node, "number-of-rate-detectors",
				   &rate_detector_count);
	if (ret)
		pr_warn("[HSI2S] Resource 'number-of-rate-detectors' unavailable in dtsi");

	if (rate_detector_count) {
		hsi2s_core->is_rate_enabled = true;
		switch (rate_detector_count) {
			case 1:
				ret = of_property_read_u32(dev->of_node, "rate-detector-interfaces", &hsi2s_core->pri_rate_interface);
				if (ret) {
					pr_warn("[HSI2S] Resource 'rate-detector-interfaces' unavailable in dtsi");
					hsi2s_core->is_rate_enabled = false;
				} else if (hsi2s_core->pri_rate_interface >= interface_count) {
					pr_warn("[HSI2S] Invalid minor number for the rate detector");
					hsi2s_core->is_rate_enabled = false;
				} else {
					/* Configure rate detection */
					configure_rate_detection(PRI_RATE_DET);
				}
				break;
			case 2:
				ret = of_property_read_u32_array(dev->of_node, "rate-detector-interfaces", rate_array, 2);
				if (ret) {
					pr_warn("[HSI2S] Resource 'rate-detector-interfaces' unavailable in dtsi");
					hsi2s_core->is_rate_enabled = false;
				} else if (rate_array[0] >= interface_count && rate_array[1] >= interface_count) {
					pr_warn("[HSI2S] Invalid minor numbers for the rate detector");
					hsi2s_core->is_rate_enabled = false;
				} else {
					if (rate_array[0] < interface_count) {
						hsi2s_core->pri_rate_interface = rate_array[0];
						/* Configure rate detection */
						configure_rate_detection(PRI_RATE_DET);
					}
					if (rate_array[1] < interface_count) {
						hsi2s_core->sec_rate_interface = rate_array[1];
						/* Configure rate detection */
						configure_rate_detection(SEC_RATE_DET);
					}
				}
				break;
			default:
				pr_warn("[HSI2S] Invalid rate detector count specified. Check device tree");
				hsi2s_core->is_rate_enabled = false;
				break;
		}
	} else {
		hsi2s_core->is_rate_enabled = false;
	}

	/* Initialize the IRQ mutex */
	mutex_init(&hsi2s_core->irqlock);

	/* Enable the interrupt line 0 */
	hsi2s_core->irq0 = platform_get_irq(pdev, 0);
	if (hsi2s_core->irq0 < 0) {
		pr_err("[HSI2S] Err getting IRQ0");
		ret = hsi2s_core->irq0;
		goto err_iounmap_lpass_tcsr;
	}

	ret =
	devm_request_threaded_irq(&pdev->dev,
				  hsi2s_core->irq0,
				  (irq_handler_t)i2s_interrupt_handler,
				  (irq_handler_t)irq_thread_fn,
				  IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				  "lpaif_hs_out0_irq", hsi2s_core);
	if (ret) {
		pr_err("[HSI2S] Request_irq failed:%d: err:%d\n",
		       hsi2s_core->irq0, ret);
		goto err_iounmap_lpass_tcsr;
	}

	/* Disable the irq line until child devices are probed */
	disable_irq_nosync(hsi2s_core->irq0);
	hsi2s_core->is_irq_enabled = false;

	/* Probe child devices */
	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret)
		pr_err("[HSI2S] Failed to add child devices");
	else
		pr_warn("[HSI2S] Added child devices");

	return ret;

err_iounmap_lpass_tcsr:
	iounmap(hsi2s_core->lpass_tcsr_base_va);
err_iounmap_lpaif:
	iounmap(hsi2s_core->lpaif_base_va);
err_disable_core_clocks:
	if (target == 6155)
		hsi2s_disable_core_clks(pdev);
err_free_macro:
	kfree(hsi2s_core->macro);
	hsi2s_core->macro = NULL;
err_free_core:
	kfree(hsi2s_core);
	hsi2s_core = NULL;

	return ret;
}

/* Remove function for the child interface nodes */
static int hsi2s_interface_remove(struct platform_device *pdev)
{
	struct hsi2s_device *hs_dev;
	int minor;

	mutex_lock(&hsi2s_core->irqlock);

	hs_dev = (struct hsi2s_device *)platform_get_drvdata(pdev);
	minor = hs_dev->minor_num;

	/* Reset the interface registers */
	reset_registers(hs_dev);
	/* Remove the device file */
	device_destroy(hs_dev->class_sdr, hs_dev->curr_devid);
	class_destroy(hs_dev->class_sdr);
	cdev_del(hs_dev->cdev_sdr);
	kfree(hs_dev->cdev_sdr);
	hs_dev->cdev_sdr = NULL;
	/* Stop the DMA scheduler thread */
	kthread_stop(hs_dev->rddma_thread);
	/* Disable the interface clocks */
	if (hsi2s_core->target == 6155)
		hsi2s_disable_intf_clks(pdev);
	/* Detach and release iommu mapping */
	#ifndef CONFIG_QTI_GVM
	if (hs_dev->hsi2s_smmu_ctx->valid) {
		if (hs_dev->hsi2s_smmu_ctx->smmu_pdev)
			arm_iommu_detach_device(&hs_dev->hsi2s_smmu_ctx->smmu_pdev->dev);
		if (hs_dev->hsi2s_smmu_ctx->mapping)
			arm_iommu_release_mapping(hs_dev->hsi2s_smmu_ctx->mapping);
		hs_dev->hsi2s_smmu_ctx->valid = false;
		hs_dev->hsi2s_smmu_ctx->mapping = NULL;
		hs_dev->hsi2s_smmu_ctx->pdev_master = NULL;
		hs_dev->hsi2s_smmu_ctx->smmu_pdev = NULL;
		pr_warn("[HSI2S] Detach and release iommu mapping");
	}
	kfree(hs_dev->hsi2s_smmu_ctx);
	hs_dev->hsi2s_smmu_ctx = NULL;
	#endif

	/* Free the allocated buffers and device data structures */
	if (hs_dev) {
		hsi2s_buffer_free(hs_dev);
		kfree(hs_dev);
		hs_dev = NULL;
		hsi2s_core->hsi2s_arr[minor] = NULL;
	} else {
		pr_warn("[HSI2S] hs_dev is already NULL for SDR%d"
			, minor);
	}

	mutex_unlock(&hsi2s_core->irqlock);

	pr_warn("[HSI2S] Child device removed");

	return 0;
}

/* Function to release all resources from driver */
static int hsi2s_remove(struct platform_device *pdev)
{
	struct hsi2s_core *hs_core;

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,hsi2s-interface"))
		return hsi2s_interface_remove(pdev);

	/* Remove the child devices */
	of_platform_depopulate(&pdev->dev);
	/* Remove the core device */
	hs_core = (struct hsi2s_core *)platform_get_drvdata(pdev);
	/* Free IRQ */
	devm_free_irq(&pdev->dev, hs_core->irq0, hs_core);
	/* Reset rate detection block */
	if (hsi2s_core->is_rate_enabled) {
		reset_rate_detection(PRI_RATE_DET);
		reset_rate_detection(SEC_RATE_DET);
	}
	/* Disable the core clocks */
	if (hs_core->target == 6155)
		hsi2s_disable_core_clks(pdev);
	else if (hs_core->target == 8155) {
		h_modify_interface_clks(0);
		h_modify_core_clks(0);
	}
	/* Unregister the device numbers */
	unregister_chrdev_region(devid, 1);
	/* Free the core data structure */
	kfree(hs_core->hsi2s_arr);
	hs_core->hsi2s_arr = NULL;
	kfree(hs_core->macro);
	hs_core->macro = NULL;
	kfree(hs_core);
	hs_core = NULL;
	hsi2s_core = NULL;

	pr_warn("[HSI2S] Core device removed");

	return 0;
}

/* Function to put the device in suspend mode */
static int hsi2s_suspend(struct platform_device *pdev, pm_message_t state)
{
	if (of_device_is_compatible(pdev->dev.of_node,
				    "qcom,hsi2s-interface")) {
		/* Suspend the interface clocks */
		if (hsi2s_core->target == 6155)
			hsi2s_suspend_intf_clks(pdev);
	} else {
		/* Suspend the core clocks */
		if (hsi2s_core->target == 6155)
			hsi2s_suspend_core_clks(pdev);
		else if (hsi2s_core->target == 8155) {
			h_modify_interface_clks(0);
			h_modify_core_clks(0);
		}
	}

	pr_warn("[HSI2S] Device entering suspend state");

	return 0;
}

/* Function to resume the device from suspend */
static int hsi2s_resume(struct platform_device *pdev)
{
	int ret = 0;

	if (of_device_is_compatible(pdev->dev.of_node,
				    "qcom,hsi2s-interface")) {
		/* Resume the interface clocks */
		if (hsi2s_core->target == 6155) {
			ret = hsi2s_resume_intf_clks(pdev);
			if (ret) {
				pr_warn("[HSI2S] Failed to resume interface clocks");
				return ret;
			}
		}
	} else {
		/* Resume the core clocks */
		if (hsi2s_core->target == 6155) {
			ret = hsi2s_resume_core_clks(pdev);
			if (ret) {
				pr_warn("[HSI2S] Failed to resume core clocks");
				return ret;
			}
		} else if (hsi2s_core->target == 8155) {
			h_modify_core_clks(1);
			h_modify_interface_clks(1);
		}
	}

	pr_warn("[HSI2S] Device resuming from suspend state");

	return ret;
}

static struct platform_driver hsi2s_driver = {
	.probe = hsi2s_probe,
	.remove = hsi2s_remove,
#ifdef CONFIG_PM
	.suspend = hsi2s_suspend,
	.resume = hsi2s_resume,
#endif
	.driver = {
		.name = "hsi2s_device",
		.of_match_table = hsi2s_idtable,
		.owner = THIS_MODULE,
	},
};

static int __init hsi2s_init_module(void)
{
	int ret = 0;

	ret = platform_driver_register(&hsi2s_driver);
	if (ret < 0) {
		pr_err("[HSI2S] Driver registration failed");
		return ret;
	}

	return ret;
}

static void __exit hsi2s_exit_module(void)
{
	platform_driver_unregister(&hsi2s_driver);

	pr_warn("[HSI2S] Driver removed");
}

module_init(hsi2s_init_module);
module_exit(hsi2s_exit_module);

MODULE_DESCRIPTION("HSI2S Driver");
MODULE_LICENSE("GPL v2");
