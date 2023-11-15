// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#include "hsi2s_drv.h"
#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM)
#include "hsi2s_adsp_clk_ctrl.h"
#endif
#include "hsi2s_common.h"
#if (LINUX_VERSION_CODE == KERNEL_VERSION(5, 15, 0))
/*Place marker function declaration*/
extern void place_marker(const char *name);
#endif
/* Device number */
static dev_t devid;

/* Buffer length in words */
static u32 dma_buffer_length_words;

/* HS-I2S core structure */
static struct hsi2s_core *hsi2s_core;

#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM) && defined(CONFIG_QCOM_QMI_HELPERS)
static struct sockaddr_qrtr sq;
#endif

/* Module parameters */
static int lpaif_mode = HS_I2S;
module_param(lpaif_mode, int, 0644);
MODULE_PARM_DESC(lpaif_mode, "LPAIF mode: 0->I2S(Default) 1->PCM");

static int operation_mode = 1;
module_param(operation_mode, int, 0644);
MODULE_PARM_DESC(operation_mode, "Operation mode: 0->Internal loopback 1->Normal(Default)");

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

static int enable_qmi =1;
module_param(enable_qmi, int, 0644);
MODULE_PARM_DESC(enable_qmi, "Is QMI enabled: 0->Disabled 1->Enabled");

#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM) && defined(CONFIG_QCOM_QMI_HELPERS)
/* QMI callbacks */
static int hsi2s_clk_ctrl_send_sync_msg(struct qmi_handle *dev, int en)
{
	int ret;
	struct prod_hsi2s_clk_ctrl_req_msg_v01 *req;
	struct prod_hsi2s_clk_ctrl_resp_msg_v01 *resp;
	struct qmi_txn txn;

	if (!dev)
		return -ENODEV;

	req = kzalloc(sizeof(struct prod_hsi2s_clk_ctrl_req_msg_v01), GFP_KERNEL);
	if (!req) {
		dev_err(hsi2s_core->dev, "Failed to allocate QMI request message");
		return -ENOMEM;
	}

	resp = kzalloc(sizeof(struct prod_hsi2s_clk_ctrl_resp_msg_v01),
			GFP_KERNEL);
	if (!resp) {
		dev_err(hsi2s_core->dev, "Failed to allocate QMI response message");
		kfree(req);
		return -ENOMEM;
	}

	req->enable_hsi2s_clks = (u8) en;

	if (hsi2s_core->qmi_connection == false)
		wait_event_interruptible(hsi2s_core->wq_qmi,
					 hsi2s_core->qmi_connection == true);

	ret = qmi_txn_init(dev, &txn, prod_hsi2s_clk_ctrl_resp_msg_v01_ei, resp);
	if (ret < 0) {
		dev_err(hsi2s_core->dev, "Init txn failed for mode resp %d\n", ret);
		goto out;
	}

	ret = qmi_send_request(dev, &sq, &txn,
			PROD_HSI2S_CLK_CTRL_REQ_V01,
			PROD_HSI2S_CLK_CTRL_REQ_MSG_V01_MAX_MSG_LEN,
			prod_hsi2s_clk_ctrl_req_msg_v01_ei, req);

	if (ret < 0) {
		qmi_txn_cancel(&txn);
		dev_err(hsi2s_core->dev, "Fail to send mode req %d\n", ret);
		goto out;
	}

	if (en) {
		ret = qmi_txn_wait(&txn, ENABLE_TIMEOUT);

		if (ret < 0) {
			dev_err(hsi2s_core->dev, "Mode resp wait failed with ret %d\n", ret);
			goto out;
		}

		if (resp->resp.result != QMI_RESULT_SUCCESS_V01) {
			dev_err(hsi2s_core->dev, "QMI Mode request rejected, result:%d error:%d\n",
					resp->resp.result, resp->resp.error);
			ret = -resp->resp.result;
			goto out;
		}

		ret = resp->resp.result;

		dev_info(hsi2s_core->dev, "ADSP clock enabling is successful\n");
	} else {
		qmi_txn_wait(&txn, DISABLE_TIMEOUT);
		dev_info(hsi2s_core->dev, "ADSP clock disabling is successful\n");
	}

out:
	kfree(req);
	kfree(resp);
	return ret;
}

static int hsi2s_qmi_adsp_new_server(struct qmi_handle *qmi,
		struct qmi_service *service)
{
	struct platform_device *pdev;
	int ret;

	sq.sq_family = AF_QIPCRTR;
	sq.sq_node = service->node;
	sq.sq_port = service->port;

	pdev = platform_device_alloc("hsi2s_qmi_adsp_client",
			PLATFORM_DEVID_AUTO);
	if (!pdev)
		return -ENOMEM;

	ret = platform_device_add_data(pdev, &sq, sizeof(sq));
	if (ret)
		goto err_put_device;
	ret = platform_device_add(pdev);
	if (ret)
		goto err_put_device;

	service->priv = pdev;
	hsi2s_core->qmi_connection = true;
	wake_up_interruptible(&hsi2s_core->wq_qmi);

	return 0;

err_put_device:
	platform_device_put(pdev);
	return ret;
}

static void hsi2s_qmi_adsp_del_server(struct qmi_handle *qmi,
		struct qmi_service *service)
{
	struct platform_device *pdev = service->priv;

	hsi2s_core->qmi_connection = false;
	platform_device_unregister(pdev);
}

static struct qmi_ops hsi2s_qmi_adsp_ops = {
	.new_server = hsi2s_qmi_adsp_new_server,
	.del_server = hsi2s_qmi_adsp_del_server,
};
#endif

/* Macro callbacks */

/* SA6155 macros */
static void t_assign_macros(void)
{

	hsi2s_core->macro->offset_i2s_ctl = T_LPAIF_I2S_CTL;
	hsi2s_core->macro->offset_pcm_ctl = T_LPAIF_PCM_CTL;
	hsi2s_core->macro->offset_tdm_ctl = T_LPAIF_PCM_TDM_CTL;
	hsi2s_core->macro->offset_tdm_sample_width = T_LPAIF_PCM_TDM_SAMPLE_WIDTH;
	hsi2s_core->macro->offset_rpcm_slot_num = T_LPAIF_PCM_RPCM_SLOT_NUM;
	hsi2s_core->macro->offset_tpcm_slot_num = T_LPAIF_PCM_TPCM_SLOT_NUM;
	hsi2s_core->macro->offset_pcm_lane_config = T_LPAIF_PCM_LANE_CONFIG;
	hsi2s_core->macro->offset_i2s_sel = T_LPAIF_PCM_I2S_SEL;
	hsi2s_core->macro->offset_irq_en = T_LPAIF_IRQ_EN;
	hsi2s_core->macro->offset_irq_stat = T_LPAIF_IRQ_STAT;
	hsi2s_core->macro->offset_irq_clear = T_LPAIF_IRQ_CLEAR;
	hsi2s_core->macro->offset_rddma_ctl = T_LPAIF_RDDMA_CTL;
	hsi2s_core->macro->offset_rddma_base = T_LPAIF_RDDMA_BASE;
	hsi2s_core->macro->offset_rddma_buff_len = T_LPAIF_RDDMA_BUFF_LEN;
	hsi2s_core->macro->offset_rddma_curr_addr = T_LPAIF_RDDMA_CURR_ADDR;
	hsi2s_core->macro->offset_rddma_per_len = T_LPAIF_RDDMA_PER_LEN;
	hsi2s_core->macro->offset_rddma_ram_addr = T_LPAIF_RDDMA_RAM_START_ADDR;
	hsi2s_core->macro->offset_rddma_ram_len = T_LPAIF_RDDMA_RAM_LENGTH;
	hsi2s_core->macro->offset_wrdma_ctl = T_LPAIF_WRDMA_CTL;
	hsi2s_core->macro->offset_wrdma_base = T_LPAIF_WRDMA_BASE;
	hsi2s_core->macro->offset_wrdma_buff_len = T_LPAIF_WRDMA_BUFF_LEN;
	hsi2s_core->macro->offset_wrdma_curr_addr = T_LPAIF_WRDMA_CURR_ADDR;
	hsi2s_core->macro->offset_wrdma_per_len = T_LPAIF_WRDMA_PER_LEN;
	hsi2s_core->macro->offset_wrdma_ram_addr = T_LPAIF_WRDMA_RAM_START_ADDR;
	hsi2s_core->macro->offset_wrdma_ram_len = T_LPAIF_WRDMA_RAM_LENGTH;
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
	hsi2s_core->macro->bit_en_long_rate = T_I2S_EN_LONG_RATE;
	hsi2s_core->macro->bit_tpcm_width = T_TPCM_WIDTH;
	hsi2s_core->macro->bit_rpcm_width = T_RPCM_WIDTH;
	hsi2s_core->macro->bit_aux_mode = T_AUX_MODE;
	hsi2s_core->macro->bit_sync_src = T_SYNC_SRC;
	hsi2s_core->macro->bit_pcm_loopback = T_PCM_LOOPBACK;
	hsi2s_core->macro->bit_ctrl_data_oe = T_CTRL_DATA_OE;
	hsi2s_core->macro->bit_one_slot_sync_en = T_ONE_SLOT_SYNC_EN;
	hsi2s_core->macro->bit_pcm_en = T_PCM_ENABLE;
	hsi2s_core->macro->bit_pcm_en_tx = T_PCM_ENABLE_TX;
	hsi2s_core->macro->bit_pcm_en_rx = T_PCM_ENABLE_RX;
	hsi2s_core->macro->bit_pcm_reset = T_PCM_RESET;
	hsi2s_core->macro->bit_pcm_reset_tx = T_PCM_RESET_TX;
	hsi2s_core->macro->bit_pcm_reset_rx = T_PCM_RESET_RX;
	hsi2s_core->macro->bit_tdm_inv_rpcm_sync = T_TDM_INV_RPCM_SYNC;
	hsi2s_core->macro->bit_tdm_inv_tpcm_sync = T_TDM_INV_TPCM_SYNC;
	hsi2s_core->macro->bit_tdm_en_diff_sample_width = T_EN_DIFF_SAMPLE_WIDTH;
	hsi2s_core->macro->bit_tdm_en = T_EN_TDM;
	hsi2s_core->macro->bit_lane0_dir = T_LANE0_DIR;
	hsi2s_core->macro->bit_lane1_dir = T_LANE1_DIR;
	hsi2s_core->macro->bit_lane2_dir = T_LANE2_DIR;
	hsi2s_core->macro->bit_lane3_dir = T_LANE3_DIR;
	hsi2s_core->macro->bit_lane0_en = T_LANE0_EN;
	hsi2s_core->macro->bit_lane1_en = T_LANE1_EN;
	hsi2s_core->macro->bit_lane2_en = T_LANE2_EN;
	hsi2s_core->macro->bit_lane3_en = T_LANE3_EN;
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
	hsi2s_core->macro->regfield_i2s_lrate_offset = T_I2S_LONG_RATE_OFFSET;
	hsi2s_core->macro->regfield_spkr_mode_sd0 = T_I2S_SPKR_MODE_SD0;
	hsi2s_core->macro->regfield_spkr_mode_sd1 = T_I2S_SPKR_MODE_SD1;
	hsi2s_core->macro->regfield_spkr_mode_quad01 = T_I2S_SPKR_MODE_QUAD01;
	hsi2s_core->macro->regfield_spkr_mono = T_I2S_SPKR_MONO;
	hsi2s_core->macro->regfield_mic_mode_sd0 = T_I2S_MIC_MODE_SD0;
	hsi2s_core->macro->regfield_mic_mode_sd1 = T_I2S_MIC_MODE_SD1;
	hsi2s_core->macro->regfield_mic_mode_quad01 = T_I2S_MIC_MODE_QUAD01;
	hsi2s_core->macro->regfield_mic_mono = T_I2S_MIC_MONO;
	hsi2s_core->macro->regfield_bit_width16 = T_I2S_BIT_WIDTH_16;
	hsi2s_core->macro->regfield_bit_width24 = T_I2S_BIT_WIDTH_24;
	hsi2s_core->macro->regfield_bit_width32 = T_I2S_BIT_WIDTH_32;
	hsi2s_core->macro->regfield_bit_width25 = T_I2S_BIT_WIDTH_25;
	hsi2s_core->macro->regfield_pcmrate_8 = T_PCM_RATE_8_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_16 = T_PCM_RATE_16_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_32 = T_PCM_RATE_32_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_64 = T_PCM_RATE_64_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_128 = T_PCM_RATE_128_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_256 = T_PCM_RATE_256_BIT_CLKS;
	hsi2s_core->macro->regfield_sync_delay_0 = T_TDM_SYNC_DELAY_0;
	hsi2s_core->macro->regfield_sync_delay_1 = T_TDM_SYNC_DELAY_1;
	hsi2s_core->macro->regfield_sync_delay_2 = T_TDM_SYNC_DELAY_2;
	hsi2s_core->macro->regfield_rddma_wpscnt_one = T_RDDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_rddma_wpscnt_two = T_RDDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_rddma_wpscnt_four = T_RDDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_rddma_wpscnt_eight = T_RDDMA_WPSCNT_EIGHT;
	hsi2s_core->macro->regfield_rddma_pri_audio_intf = T_RDDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_sec_audio_intf = T_RDDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_fifo_wm8 = T_RDDMA_FIFO_WM_8;
	hsi2s_core->macro->regfield_wrdma_wpscnt_one = T_WRDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_wrdma_wpscnt_two = T_WRDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_wrdma_wpscnt_four = T_WRDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_wrdma_wpscnt_eight = T_WRDMA_WPSCNT_EIGHT;
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

/* SA8155/SA8195 macros */
static void h_assign_macros(void)
{

	hsi2s_core->macro->offset_i2s_ctl = H_LPAIF_I2S_CTL;
	hsi2s_core->macro->offset_pcm_ctl = H_LPAIF_PCM_CTL;
	hsi2s_core->macro->offset_tdm_ctl = H_LPAIF_PCM_TDM_CTL;
	hsi2s_core->macro->offset_tdm_sample_width = H_LPAIF_PCM_TDM_SAMPLE_WIDTH;
	hsi2s_core->macro->offset_rpcm_slot_num = H_LPAIF_PCM_RPCM_SLOT_NUM;
	hsi2s_core->macro->offset_tpcm_slot_num = H_LPAIF_PCM_TPCM_SLOT_NUM;
	hsi2s_core->macro->offset_pcm_lane_config = H_LPAIF_PCM_LANE_CONFIG;
	hsi2s_core->macro->offset_i2s_sel = H_LPAIF_PCM_I2S_SEL;
	hsi2s_core->macro->offset_irq_en = H_LPAIF_IRQ_EN;
	hsi2s_core->macro->offset_irq_stat = H_LPAIF_IRQ_STAT;
	hsi2s_core->macro->offset_irq_clear = H_LPAIF_IRQ_CLEAR;
	hsi2s_core->macro->offset_rddma_ctl = H_LPAIF_RDDMA_CTL;
	hsi2s_core->macro->offset_rddma_base = H_LPAIF_RDDMA_BASE;
	hsi2s_core->macro->offset_rddma_buff_len = H_LPAIF_RDDMA_BUFF_LEN;
	hsi2s_core->macro->offset_rddma_curr_addr = H_LPAIF_RDDMA_CURR_ADDR;
	hsi2s_core->macro->offset_rddma_per_len = H_LPAIF_RDDMA_PER_LEN;
	hsi2s_core->macro->offset_rddma_ram_addr = H_LPAIF_RDDMA_RAM_START_ADDR;
	hsi2s_core->macro->offset_rddma_ram_len = H_LPAIF_RDDMA_RAM_LENGTH;
	hsi2s_core->macro->offset_wrdma_ctl = H_LPAIF_WRDMA_CTL;
	hsi2s_core->macro->offset_wrdma_base = H_LPAIF_WRDMA_BASE;
	hsi2s_core->macro->offset_wrdma_buff_len = H_LPAIF_WRDMA_BUFF_LEN;
	hsi2s_core->macro->offset_wrdma_curr_addr = H_LPAIF_WRDMA_CURR_ADDR;
	hsi2s_core->macro->offset_wrdma_per_len = H_LPAIF_WRDMA_PER_LEN;
	hsi2s_core->macro->offset_wrdma_ram_addr = H_LPAIF_WRDMA_RAM_START_ADDR;
	hsi2s_core->macro->offset_wrdma_ram_len = H_LPAIF_WRDMA_RAM_LENGTH;
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
	hsi2s_core->macro->bit_en_long_rate = H_I2S_EN_LONG_RATE;
	hsi2s_core->macro->bit_tpcm_width = H_TPCM_WIDTH;
	hsi2s_core->macro->bit_rpcm_width = H_RPCM_WIDTH;
	hsi2s_core->macro->bit_aux_mode = H_AUX_MODE;
	hsi2s_core->macro->bit_sync_src = H_SYNC_SRC;
	hsi2s_core->macro->bit_pcm_loopback = H_PCM_LOOPBACK;
	hsi2s_core->macro->bit_ctrl_data_oe = H_CTRL_DATA_OE;
	hsi2s_core->macro->bit_one_slot_sync_en = H_ONE_SLOT_SYNC_EN;
	hsi2s_core->macro->bit_pcm_en = H_PCM_ENABLE;
	hsi2s_core->macro->bit_pcm_en_tx = H_PCM_ENABLE_TX;
	hsi2s_core->macro->bit_pcm_en_rx = H_PCM_ENABLE_RX;
	hsi2s_core->macro->bit_pcm_reset = H_PCM_RESET;
	hsi2s_core->macro->bit_pcm_reset_tx = H_PCM_RESET_TX;
	hsi2s_core->macro->bit_pcm_reset_rx = H_PCM_RESET_RX;
	hsi2s_core->macro->bit_tdm_inv_rpcm_sync = H_TDM_INV_RPCM_SYNC;
	hsi2s_core->macro->bit_tdm_inv_tpcm_sync = H_TDM_INV_TPCM_SYNC;
	hsi2s_core->macro->bit_tdm_en_diff_sample_width = H_EN_DIFF_SAMPLE_WIDTH;
	hsi2s_core->macro->bit_tdm_en = H_EN_TDM;
	hsi2s_core->macro->bit_lane0_dir = H_LANE0_DIR;
	hsi2s_core->macro->bit_lane1_dir = H_LANE1_DIR;
	hsi2s_core->macro->bit_lane2_dir = H_LANE2_DIR;
	hsi2s_core->macro->bit_lane3_dir = H_LANE3_DIR;
	hsi2s_core->macro->bit_lane0_en = H_LANE0_EN;
	hsi2s_core->macro->bit_lane1_en = H_LANE1_EN;
	hsi2s_core->macro->bit_lane2_en = H_LANE2_EN;
	hsi2s_core->macro->bit_lane3_en = H_LANE3_EN;
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
	hsi2s_core->macro->regfield_i2s_lrate_offset = H_I2S_LONG_RATE_OFFSET;
	hsi2s_core->macro->regfield_spkr_mode_sd0 = H_I2S_SPKR_MODE_SD0;
	hsi2s_core->macro->regfield_spkr_mode_sd1 = H_I2S_SPKR_MODE_SD1;
	hsi2s_core->macro->regfield_spkr_mode_quad01 = H_I2S_SPKR_MODE_QUAD01;
	hsi2s_core->macro->regfield_spkr_mono = H_I2S_SPKR_MONO;
	hsi2s_core->macro->regfield_mic_mode_sd0 = H_I2S_MIC_MODE_SD0;
	hsi2s_core->macro->regfield_mic_mode_sd1 = H_I2S_MIC_MODE_SD1;
	hsi2s_core->macro->regfield_mic_mode_quad01 = H_I2S_MIC_MODE_QUAD01;
	hsi2s_core->macro->regfield_mic_mono = H_I2S_MIC_MONO;
	hsi2s_core->macro->regfield_bit_width16 = H_I2S_BIT_WIDTH_16;
	hsi2s_core->macro->regfield_bit_width24 = H_I2S_BIT_WIDTH_24;
	hsi2s_core->macro->regfield_bit_width32 = H_I2S_BIT_WIDTH_32;
	hsi2s_core->macro->regfield_bit_width25 = H_I2S_BIT_WIDTH_25;
	hsi2s_core->macro->regfield_pcmrate_8 = H_PCM_RATE_8_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_16 = H_PCM_RATE_16_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_32 = H_PCM_RATE_32_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_64 = H_PCM_RATE_64_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_128 = H_PCM_RATE_128_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_256 = H_PCM_RATE_256_BIT_CLKS;
	hsi2s_core->macro->regfield_sync_delay_0 = H_TDM_SYNC_DELAY_0;
	hsi2s_core->macro->regfield_sync_delay_1 = H_TDM_SYNC_DELAY_1;
	hsi2s_core->macro->regfield_sync_delay_2 = H_TDM_SYNC_DELAY_2;
	hsi2s_core->macro->regfield_rddma_wpscnt_one = H_RDDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_rddma_wpscnt_two = H_RDDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_rddma_wpscnt_four = H_RDDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_rddma_wpscnt_eight = H_RDDMA_WPSCNT_EIGHT;
	hsi2s_core->macro->regfield_rddma_pri_audio_intf = H_RDDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_sec_audio_intf = H_RDDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_ter_audio_intf = H_RDDMA_TER_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_fifo_wm8 = H_RDDMA_FIFO_WM_8;
	hsi2s_core->macro->regfield_wrdma_wpscnt_one = H_WRDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_wrdma_wpscnt_two = H_WRDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_wrdma_wpscnt_four = H_WRDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_wrdma_wpscnt_eight = H_WRDMA_WPSCNT_EIGHT;
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

/* SA8295 macros */
static void m_assign_macros(void)
{
	hsi2s_core->macro->offset_i2s_ctl = T_LPAIF_I2S_CTL;
	hsi2s_core->macro->offset_pcm_ctl = T_LPAIF_PCM_CTL;
	hsi2s_core->macro->offset_tdm_ctl = T_LPAIF_PCM_TDM_CTL;
	hsi2s_core->macro->offset_tdm_sample_width = T_LPAIF_PCM_TDM_SAMPLE_WIDTH;
	hsi2s_core->macro->offset_rpcm_slot_num = T_LPAIF_PCM_RPCM_SLOT_NUM;
	hsi2s_core->macro->offset_tpcm_slot_num = T_LPAIF_PCM_TPCM_SLOT_NUM;
	hsi2s_core->macro->offset_pcm_lane_config = T_LPAIF_PCM_LANE_CONFIG;
	hsi2s_core->macro->offset_i2s_sel = T_LPAIF_PCM_I2S_SEL;
	hsi2s_core->macro->offset_irq_en = T_LPAIF_IRQ_EN;
	hsi2s_core->macro->offset_irq_stat = T_LPAIF_IRQ_STAT;
	hsi2s_core->macro->offset_irq_clear = T_LPAIF_IRQ_CLEAR;
	hsi2s_core->macro->offset_irq2_en = M_LPAIF_IRQ2_EN;
	hsi2s_core->macro->offset_irq2_stat = M_LPAIF_IRQ2_STAT;
	hsi2s_core->macro->offset_irq2_clear = M_LPAIF_IRQ2_CLEAR;
	hsi2s_core->macro->offset_rddma_ctl = T_LPAIF_RDDMA_CTL;
	hsi2s_core->macro->offset_rddma_base = T_LPAIF_RDDMA_BASE;
	hsi2s_core->macro->offset_rddma_buff_len = T_LPAIF_RDDMA_BUFF_LEN;
	hsi2s_core->macro->offset_rddma_curr_addr = T_LPAIF_RDDMA_CURR_ADDR;
	hsi2s_core->macro->offset_rddma_per_len = T_LPAIF_RDDMA_PER_LEN;
	hsi2s_core->macro->offset_rddma_ram_addr = T_LPAIF_RDDMA_RAM_START_ADDR;
	hsi2s_core->macro->offset_rddma_ram_len = T_LPAIF_RDDMA_RAM_LENGTH;
	hsi2s_core->macro->offset_wrdma_ctl = T_LPAIF_WRDMA_CTL;
	hsi2s_core->macro->offset_wrdma_base = T_LPAIF_WRDMA_BASE;
	hsi2s_core->macro->offset_wrdma_buff_len = T_LPAIF_WRDMA_BUFF_LEN;
	hsi2s_core->macro->offset_wrdma_curr_addr = T_LPAIF_WRDMA_CURR_ADDR;
	hsi2s_core->macro->offset_wrdma_per_len = T_LPAIF_WRDMA_PER_LEN;
	hsi2s_core->macro->offset_wrdma_ram_addr = T_LPAIF_WRDMA_RAM_START_ADDR;
	hsi2s_core->macro->offset_wrdma_ram_len = T_LPAIF_WRDMA_RAM_LENGTH;
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
	hsi2s_core->macro->bit_en_long_rate = T_I2S_EN_LONG_RATE;
	hsi2s_core->macro->bit_tpcm_width = T_TPCM_WIDTH;
	hsi2s_core->macro->bit_rpcm_width = T_RPCM_WIDTH;
	hsi2s_core->macro->bit_aux_mode = T_AUX_MODE;
	hsi2s_core->macro->bit_sync_src = T_SYNC_SRC;
	hsi2s_core->macro->bit_pcm_loopback = T_PCM_LOOPBACK;
	hsi2s_core->macro->bit_ctrl_data_oe = T_CTRL_DATA_OE;
	hsi2s_core->macro->bit_one_slot_sync_en = T_ONE_SLOT_SYNC_EN;
	hsi2s_core->macro->bit_pcm_en = T_PCM_ENABLE;
	hsi2s_core->macro->bit_pcm_en_tx = T_PCM_ENABLE_TX;
	hsi2s_core->macro->bit_pcm_en_rx = T_PCM_ENABLE_RX;
	hsi2s_core->macro->bit_pcm_reset = T_PCM_RESET;
	hsi2s_core->macro->bit_pcm_reset_tx = T_PCM_RESET_TX;
	hsi2s_core->macro->bit_pcm_reset_rx = T_PCM_RESET_RX;
	hsi2s_core->macro->bit_tdm_inv_rpcm_sync = T_TDM_INV_RPCM_SYNC;
	hsi2s_core->macro->bit_tdm_inv_tpcm_sync = T_TDM_INV_TPCM_SYNC;
	hsi2s_core->macro->bit_tdm_en_diff_sample_width = T_EN_DIFF_SAMPLE_WIDTH;
	hsi2s_core->macro->bit_tdm_en = T_EN_TDM;
	hsi2s_core->macro->bit_lane0_dir = T_LANE0_DIR;
	hsi2s_core->macro->bit_lane1_dir = T_LANE1_DIR;
	hsi2s_core->macro->bit_lane2_dir = T_LANE2_DIR;
	hsi2s_core->macro->bit_lane3_dir = T_LANE3_DIR;
	hsi2s_core->macro->bit_lane0_en = T_LANE0_EN;
	hsi2s_core->macro->bit_lane1_en = T_LANE1_EN;
	hsi2s_core->macro->bit_lane2_en = T_LANE2_EN;
	hsi2s_core->macro->bit_lane3_en = T_LANE3_EN;
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
	hsi2s_core->macro->regfield_i2s_lrate_offset = T_I2S_LONG_RATE_OFFSET;
	hsi2s_core->macro->regfield_spkr_mode_sd0 = T_I2S_SPKR_MODE_SD0;
	hsi2s_core->macro->regfield_spkr_mode_sd1 = T_I2S_SPKR_MODE_SD1;
	hsi2s_core->macro->regfield_spkr_mode_quad01 = T_I2S_SPKR_MODE_QUAD01;
	hsi2s_core->macro->regfield_spkr_mono = T_I2S_SPKR_MONO;
	hsi2s_core->macro->regfield_mic_mode_sd0 = T_I2S_MIC_MODE_SD0;
	hsi2s_core->macro->regfield_mic_mode_sd1 = T_I2S_MIC_MODE_SD1;
	hsi2s_core->macro->regfield_mic_mode_quad01 = T_I2S_MIC_MODE_QUAD01;
	hsi2s_core->macro->regfield_mic_mono = T_I2S_MIC_MONO;
	hsi2s_core->macro->regfield_bit_width16 = T_I2S_BIT_WIDTH_16;
	hsi2s_core->macro->regfield_bit_width24 = T_I2S_BIT_WIDTH_24;
	hsi2s_core->macro->regfield_bit_width32 = T_I2S_BIT_WIDTH_32;
	hsi2s_core->macro->regfield_bit_width25 = T_I2S_BIT_WIDTH_25;
	hsi2s_core->macro->regfield_pcmrate_8 = T_PCM_RATE_8_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_16 = T_PCM_RATE_16_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_32 = T_PCM_RATE_32_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_64 = T_PCM_RATE_64_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_128 = T_PCM_RATE_128_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_256 = T_PCM_RATE_256_BIT_CLKS;
	hsi2s_core->macro->regfield_sync_delay_0 = T_TDM_SYNC_DELAY_0;
	hsi2s_core->macro->regfield_sync_delay_1 = T_TDM_SYNC_DELAY_1;
	hsi2s_core->macro->regfield_sync_delay_2 = T_TDM_SYNC_DELAY_2;
	hsi2s_core->macro->regfield_rddma_wpscnt_one = T_RDDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_rddma_wpscnt_two = T_RDDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_rddma_wpscnt_four = T_RDDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_rddma_wpscnt_eight = T_RDDMA_WPSCNT_EIGHT;
	hsi2s_core->macro->regfield_rddma_pri_audio_intf = T_RDDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_sec_audio_intf = T_RDDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_ter_audio_intf = M_RDDMA_TER_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_quat_audio_intf = M_RDDMA_QUAT_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_quin_audio_intf = M_RDDMA_QUIN_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_fifo_wm8 = T_RDDMA_FIFO_WM_8;
	hsi2s_core->macro->regfield_wrdma_wpscnt_one = T_WRDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_wrdma_wpscnt_two = T_WRDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_wrdma_wpscnt_four = T_WRDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_wrdma_wpscnt_eight = T_WRDMA_WPSCNT_EIGHT;
	hsi2s_core->macro->regfield_wrdma_pri_audio_intf = T_WRDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_sec_audio_intf = T_WRDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_ter_audio_intf = M_WRDMA_TER_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_quat_audio_intf = M_WRDMA_QUAT_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_quin_audio_intf = M_WRDMA_QUIN_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_loopback_ch0 = T_WRDMA_LOOPBACK_CH0;
	hsi2s_core->macro->regfield_wrdma_loopback_ch1 = T_WRDMA_LOOPBACK_CH1;
	hsi2s_core->macro->regfield_wrdma_loopback_ch2 = M_WRDMA_LOOPBACK_CH2;
	hsi2s_core->macro->regfield_wrdma_loopback_ch3 = M_WRDMA_LOOPBACK_CH3;
	hsi2s_core->macro->regfield_wrdma_loopback_ch4 = M_WRDMA_LOOPBACK_CH4;
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

/* SA8255 macros */
static void l_assign_macros(void)
{
	hsi2s_core->macro->offset_i2s_ctl = T_LPAIF_I2S_CTL;
	hsi2s_core->macro->offset_pcm_ctl = T_LPAIF_PCM_CTL;
	hsi2s_core->macro->offset_tdm_ctl = T_LPAIF_PCM_TDM_CTL;
	hsi2s_core->macro->offset_tdm_sample_width = T_LPAIF_PCM_TDM_SAMPLE_WIDTH;
	hsi2s_core->macro->offset_rpcm_slot_num = T_LPAIF_PCM_RPCM_SLOT_NUM;
	hsi2s_core->macro->offset_tpcm_slot_num = T_LPAIF_PCM_TPCM_SLOT_NUM;
	hsi2s_core->macro->offset_pcm_lane_config = T_LPAIF_PCM_LANE_CONFIG;
	hsi2s_core->macro->offset_i2s_sel = T_LPAIF_PCM_I2S_SEL;
	hsi2s_core->macro->offset_irq_en = T_LPAIF_IRQ_EN;
	hsi2s_core->macro->offset_irq_stat = T_LPAIF_IRQ_STAT;
	hsi2s_core->macro->offset_irq_clear = T_LPAIF_IRQ_CLEAR;
	hsi2s_core->macro->offset_irq2_en = M_LPAIF_IRQ2_EN;
	hsi2s_core->macro->offset_irq2_stat = M_LPAIF_IRQ2_STAT;
	hsi2s_core->macro->offset_irq2_clear = M_LPAIF_IRQ2_CLEAR;
	hsi2s_core->macro->offset_rddma_ctl = T_LPAIF_RDDMA_CTL;
	hsi2s_core->macro->offset_rddma_base = T_LPAIF_RDDMA_BASE;
	hsi2s_core->macro->offset_rddma_buff_len = T_LPAIF_RDDMA_BUFF_LEN;
	hsi2s_core->macro->offset_rddma_curr_addr = T_LPAIF_RDDMA_CURR_ADDR;
	hsi2s_core->macro->offset_rddma_per_len = T_LPAIF_RDDMA_PER_LEN;
	hsi2s_core->macro->offset_rddma_ram_addr = T_LPAIF_RDDMA_RAM_START_ADDR;
	hsi2s_core->macro->offset_rddma_ram_len = T_LPAIF_RDDMA_RAM_LENGTH;
	hsi2s_core->macro->offset_wrdma_ctl = T_LPAIF_WRDMA_CTL;
	hsi2s_core->macro->offset_wrdma_base = T_LPAIF_WRDMA_BASE;
	hsi2s_core->macro->offset_wrdma_buff_len = T_LPAIF_WRDMA_BUFF_LEN;
	hsi2s_core->macro->offset_wrdma_curr_addr = T_LPAIF_WRDMA_CURR_ADDR;
	hsi2s_core->macro->offset_wrdma_per_len = T_LPAIF_WRDMA_PER_LEN;
	hsi2s_core->macro->offset_wrdma_ram_addr = T_LPAIF_WRDMA_RAM_START_ADDR;
	hsi2s_core->macro->offset_wrdma_ram_len = T_LPAIF_WRDMA_RAM_LENGTH;
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
	hsi2s_core->macro->bit_en_long_rate = T_I2S_EN_LONG_RATE;
	hsi2s_core->macro->bit_tpcm_width = T_TPCM_WIDTH;
	hsi2s_core->macro->bit_rpcm_width = T_RPCM_WIDTH;
	hsi2s_core->macro->bit_aux_mode = T_AUX_MODE;
	hsi2s_core->macro->bit_sync_src = T_SYNC_SRC;
	hsi2s_core->macro->bit_pcm_loopback = T_PCM_LOOPBACK;
	hsi2s_core->macro->bit_ctrl_data_oe = T_CTRL_DATA_OE;
	hsi2s_core->macro->bit_one_slot_sync_en = T_ONE_SLOT_SYNC_EN;
	hsi2s_core->macro->bit_pcm_en = T_PCM_ENABLE;
	hsi2s_core->macro->bit_pcm_en_tx = T_PCM_ENABLE_TX;
	hsi2s_core->macro->bit_pcm_en_rx = T_PCM_ENABLE_RX;
	hsi2s_core->macro->bit_pcm_reset = T_PCM_RESET;
	hsi2s_core->macro->bit_pcm_reset_tx = T_PCM_RESET_TX;
	hsi2s_core->macro->bit_pcm_reset_rx = T_PCM_RESET_RX;
	hsi2s_core->macro->bit_tdm_inv_rpcm_sync = T_TDM_INV_RPCM_SYNC;
	hsi2s_core->macro->bit_tdm_inv_tpcm_sync = T_TDM_INV_TPCM_SYNC;
	hsi2s_core->macro->bit_tdm_en_diff_sample_width = T_EN_DIFF_SAMPLE_WIDTH;
	hsi2s_core->macro->bit_tdm_en = T_EN_TDM;
	hsi2s_core->macro->bit_lane0_dir = T_LANE0_DIR;
	hsi2s_core->macro->bit_lane1_dir = T_LANE1_DIR;
	hsi2s_core->macro->bit_lane2_dir = T_LANE2_DIR;
	hsi2s_core->macro->bit_lane3_dir = T_LANE3_DIR;
	hsi2s_core->macro->bit_lane0_en = T_LANE0_EN;
	hsi2s_core->macro->bit_lane1_en = T_LANE1_EN;
	hsi2s_core->macro->bit_lane2_en = T_LANE2_EN;
	hsi2s_core->macro->bit_lane3_en = T_LANE3_EN;
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
	hsi2s_core->macro->regfield_i2s_lrate_offset = T_I2S_LONG_RATE_OFFSET;
	hsi2s_core->macro->regfield_spkr_mode_sd0 = T_I2S_SPKR_MODE_SD0;
	hsi2s_core->macro->regfield_spkr_mode_sd1 = T_I2S_SPKR_MODE_SD1;
	hsi2s_core->macro->regfield_spkr_mode_quad01 = T_I2S_SPKR_MODE_QUAD01;
	hsi2s_core->macro->regfield_spkr_mono = T_I2S_SPKR_MONO;
	hsi2s_core->macro->regfield_mic_mode_sd0 = T_I2S_MIC_MODE_SD0;
	hsi2s_core->macro->regfield_mic_mode_sd1 = T_I2S_MIC_MODE_SD1;
	hsi2s_core->macro->regfield_mic_mode_quad01 = T_I2S_MIC_MODE_QUAD01;
	hsi2s_core->macro->regfield_mic_mono = T_I2S_MIC_MONO;
	hsi2s_core->macro->regfield_bit_width16 = T_I2S_BIT_WIDTH_16;
	hsi2s_core->macro->regfield_bit_width24 = T_I2S_BIT_WIDTH_24;
	hsi2s_core->macro->regfield_bit_width32 = T_I2S_BIT_WIDTH_32;
	hsi2s_core->macro->regfield_bit_width25 = T_I2S_BIT_WIDTH_25;
	hsi2s_core->macro->regfield_pcmrate_8 = T_PCM_RATE_8_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_16 = T_PCM_RATE_16_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_32 = T_PCM_RATE_32_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_64 = T_PCM_RATE_64_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_128 = T_PCM_RATE_128_BIT_CLKS;
	hsi2s_core->macro->regfield_pcmrate_256 = T_PCM_RATE_256_BIT_CLKS;
	hsi2s_core->macro->regfield_sync_delay_0 = T_TDM_SYNC_DELAY_0;
	hsi2s_core->macro->regfield_sync_delay_1 = T_TDM_SYNC_DELAY_1;
	hsi2s_core->macro->regfield_sync_delay_2 = T_TDM_SYNC_DELAY_2;
	hsi2s_core->macro->regfield_rddma_wpscnt_one = T_RDDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_rddma_wpscnt_two = T_RDDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_rddma_wpscnt_four = T_RDDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_rddma_wpscnt_eight = T_RDDMA_WPSCNT_EIGHT;
	hsi2s_core->macro->regfield_rddma_pri_audio_intf = T_RDDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_sec_audio_intf = T_RDDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_ter_audio_intf = M_RDDMA_TER_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_quat_audio_intf = M_RDDMA_QUAT_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_quin_audio_intf = M_RDDMA_QUIN_AUDIO_INTF;
	hsi2s_core->macro->regfield_rddma_fifo_wm8 = T_RDDMA_FIFO_WM_8;
	hsi2s_core->macro->regfield_wrdma_wpscnt_one = T_WRDMA_WPSCNT_ONE;
	hsi2s_core->macro->regfield_wrdma_wpscnt_two = T_WRDMA_WPSCNT_TWO;
	hsi2s_core->macro->regfield_wrdma_wpscnt_four = T_WRDMA_WPSCNT_FOUR;
	hsi2s_core->macro->regfield_wrdma_wpscnt_eight = T_WRDMA_WPSCNT_EIGHT;
	hsi2s_core->macro->regfield_wrdma_pri_audio_intf = T_WRDMA_PRI_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_sec_audio_intf = T_WRDMA_SEC_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_ter_audio_intf = M_WRDMA_TER_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_quat_audio_intf = M_WRDMA_QUAT_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_quin_audio_intf = M_WRDMA_QUIN_AUDIO_INTF;
	hsi2s_core->macro->regfield_wrdma_loopback_ch0 = T_WRDMA_LOOPBACK_CH0;
	hsi2s_core->macro->regfield_wrdma_loopback_ch1 = T_WRDMA_LOOPBACK_CH1;
	hsi2s_core->macro->regfield_wrdma_loopback_ch2 = M_WRDMA_LOOPBACK_CH2;
	hsi2s_core->macro->regfield_wrdma_loopback_ch3 = M_WRDMA_LOOPBACK_CH3;
	hsi2s_core->macro->regfield_wrdma_loopback_ch4 = M_WRDMA_LOOPBACK_CH4;
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


/* Register callbacks */
static u32 l_calculate_muxmode_offset(struct hsi2s_device *hs_dev, int intf)
{
	u32 offset = 0;

	if(intf < 5)
		offset = (0xc * intf);

	dev_info(hs_dev->dev, "lemans intf %d ,Muxmode offset : %u\n", intf, offset);

	return offset;
}
static u32 calculate_muxmode_offset(struct hsi2s_device *hs_dev, int intf)
{
	u32 offset;

	offset = intf < 3 ? (0x1000 * intf): (0x1000 * (intf + 1));
	dev_info(hs_dev->dev, "Muxmode offset : %u\n", offset);

	return offset;
}

/* Map the register memory regions */
static int map_registers(struct hsi2s_device *hs_dev, int intf)
{
	int ret = 0;

	if (hsi2s_core->macro) {
		hs_dev->i2s_ctl = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_i2s_ctl +
						  (0x1000 * intf);
		hs_dev->pcm_ctl = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_pcm_ctl +
						  (0x1000 * intf);
		hs_dev->tdm_ctl = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_tdm_ctl +
						  (0x1000 * intf);
		hs_dev->tdm_sample_width = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_tdm_sample_width +
						  (0x1000 * intf);
		hs_dev->rpcm_slot_num = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_rpcm_slot_num +
						  (0x1000 * intf);
		hs_dev->tpcm_slot_num = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_tpcm_slot_num +
						  (0x1000 * intf);
		hs_dev->pcm_lane_config = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_pcm_lane_config +
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
		hs_dev->rddma_ram_addr = hsi2s_core->lpaif_base_va +
								hsi2s_core->macro->offset_rddma_ram_addr +
								(0x1000 * intf);
		hs_dev->rddma_ram_len = hsi2s_core->lpaif_base_va +
								hsi2s_core->macro->offset_rddma_ram_len +
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
		hs_dev->wrdma_ram_addr = hsi2s_core->lpaif_base_va +
								hsi2s_core->macro->offset_wrdma_ram_addr +
								(0x1000 * intf);
		hs_dev->wrdma_ram_len = hsi2s_core->lpaif_base_va +
								hsi2s_core->macro->offset_wrdma_ram_len +
								(0x1000 * intf);
		if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195) {
			hs_dev->lpaif_muxmode = hsi2s_core->lpass_tcsr_base_va +
									H_LPAIF_MUXMODE + (0x4 * intf);
		}
		if (hsi2s_core->target == 8295) {
			hs_dev->lpaif_muxmode = hsi2s_core->lpass_core_cc_hs_if +
						M_LPAIF_MUXMODE +
						calculate_muxmode_offset(hs_dev, intf);
		}
		if (hsi2s_core->target == 8255)
		{
			hs_dev->lpaif_muxmode = hsi2s_core->lpass_core_cc_hs_if +
						L_LPAIF_MUXMODE +
						l_calculate_muxmode_offset(hs_dev, intf);
			dev_info(hs_dev->dev, "base muxmode address %x = %x + %x + %x\n", hs_dev->lpaif_muxmode, hsi2s_core->lpass_core_cc_hs_if, L_LPAIF_MUXMODE , l_calculate_muxmode_offset(hs_dev, intf));
		}
	} else {
		dev_err(hs_dev->dev, "HS-I2S macro structure is NULL");
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
		if ((hsi2s_core->target == 8295) || (hsi2s_core->target == 8255)) {
			hsi2s_core->irq2_en = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_irq2_en;
			hsi2s_core->irq2_stat = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_irq2_stat;
			hsi2s_core->irq2_clear = hsi2s_core->lpaif_base_va + hsi2s_core->macro->offset_irq2_clear;
		}
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
		dev_err(hsi2s_core->dev, "HS-I2S macro structure is NULL");
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
	if ((hsi2s_core->target == 8295) || (hsi2s_core->target == 8255))
		writel_relaxed(0xFFFFFFFF, hsi2s_core->irq2_clear);
}

/* Reset the registers */
static void reset_registers(struct hsi2s_device *hs_dev)
{
	if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195) {
		dev_info(hs_dev->dev, "Resetting muxmode register\n");
		reg_clear(hs_dev->lpaif_muxmode);
	}
	if (hs_dev->lpaif_mode == HS_I2S) {
		reg_clear(hs_dev->i2s_ctl);
	} else {
		reg_clear(hs_dev->pcm_ctl);
		reg_clear(hs_dev->tdm_ctl);
	}
	clear_irqs();
	reg_clear(hs_dev->rddma_ctl);
	reg_clear(hs_dev->rddma_base);
	reg_clear(hs_dev->rddma_buff_len);
	reg_clear(hs_dev->rddma_curr_addr);
	reg_clear(hs_dev->rddma_per_len);
	setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_reset);
	clearbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_reset);

	reg_clear(hs_dev->wrdma_ctl);
	reg_clear(hs_dev->wrdma_base);
	reg_clear(hs_dev->wrdma_buff_len);
	reg_clear(hs_dev->wrdma_curr_addr);
	reg_clear(hs_dev->wrdma_per_len);
	reg_clear(hs_dev->wrdma_ram_addr);
	reg_clear(hs_dev->wrdma_ram_len);
	setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_reset);
	clearbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_reset);

	if (hs_dev->lpaif_mode == HS_I2S) {
		setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
		clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
	} else {
		setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset);
		clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset);
	}
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
	reg_clear(hs_dev->wrdma_ram_addr);
	reg_clear(hs_dev->wrdma_ram_len);
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
		} else if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195) {
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
		} else if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195) {
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
	return (((unsigned long long)interval * bit_clk) / 8000);
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

/* Configure LPAIF mode */
static void configure_lpaif_mode(struct hsi2s_device *hs_dev, u8 mode)
{
	switch (mode) {
		case HS_I2S:
			dev_info(hs_dev->dev, "Configure LPAIF in HS-I2S mode");
			clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
			hs_dev->lpaif_mode = HS_I2S;
			break;
		case HS_PCM:
			dev_info(hs_dev->dev, "Configure LPAIF in HS-PCM mode");
			setbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
			hs_dev->lpaif_mode = HS_PCM;
			break;
		default:
			dev_warn(hs_dev->dev, "Undefined LPAIF mode. Defaulting to HS-I2S mode");
			clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
			hs_dev->lpaif_mode = HS_I2S;
			break;
	}
}

/* Configure bit depth */
static void configure_bit_depth(struct hsi2s_device *hs_dev, u32 b_depth)
{
	if (b_depth <= 0) {
		dev_warn(hs_dev->dev, "Defaulting to 32 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width32;
		hs_dev->bit_depth_val = 32;
	} else if (b_depth <= 16) {
		dev_info(hs_dev->dev, "Setting 16 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width16;
		hs_dev->bit_depth_val = 16;
	} else if (b_depth <= 24) {
		dev_info(hs_dev->dev, "Setting 24 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width24;
		hs_dev->bit_depth_val = 24;
	} else if (b_depth == 25) {
		dev_info(hs_dev->dev, "Setting 25 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width25;
		hs_dev->bit_depth_val = 25;
	} else if (b_depth <= 32) {
		dev_info(hs_dev->dev, "Setting 32 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width32;
		hs_dev->bit_depth_val = 32;
	} else {
		dev_warn(hs_dev->dev, "Defaulting to 32 bit configuration");
		hs_dev->bit_depth = hsi2s_core->macro->regfield_bit_width32;
		hs_dev->bit_depth_val = 32;
	}
}

/* Configure speaker channel */
static void configure_spkr_channel(struct hsi2s_device *hs_dev, u32 ch_count)
{
	switch (ch_count) {
		case 0:
			dev_warn(hs_dev->dev, "Defaulting to stereo configuration for speaker");
			hs_dev->spkr_channel_count = SPKR_STEREO;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_sd1;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
			else
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
			break;
		case 1:
			dev_info(hs_dev->dev, "Setting mono configuration for speaker");
			hs_dev->spkr_channel_count = hsi2s_core->macro->regfield_spkr_mono;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_sd1;
			hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
			break;
		case 2:
			dev_info(hs_dev->dev, "Setting stereo configuration for speaker");
			hs_dev->spkr_channel_count = SPKR_STEREO;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_sd1;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
			else
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
			break;
		case 4:
			dev_info(hs_dev->dev, "Setting quad configuration for speaker");
			hs_dev->spkr_channel_count = SPKR_QUAD;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_quad01;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
			else
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_four;
			break;
		default:
			dev_warn(hs_dev->dev, "Invalid number of channels entered");
			dev_warn(hs_dev->dev, "Defaulting to stereo configuration for speaker");
			hs_dev->spkr_channel_count = SPKR_STEREO;
			hs_dev->spkr_mode = hsi2s_core->macro->regfield_spkr_mode_sd1;
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
			dev_warn(hs_dev->dev, "Defaulting to stereo configuration for mic");
			hs_dev->mic_channel_count = MIC_STEREO;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_sd0;
			hs_dev->mic_ch_count_val = 2;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			else
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
			break;
		case 1:
			dev_info(hs_dev->dev, "Setting mono configuration for mic");
			hs_dev->mic_channel_count = hsi2s_core->macro->regfield_mic_mono;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_sd0;
			hs_dev->mic_ch_count_val = 1;
			hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			break;
		case 2:
			dev_info(hs_dev->dev, "Setting stereo configuration for mic");
			hs_dev->mic_channel_count = MIC_STEREO;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_sd0;
			hs_dev->mic_ch_count_val = 2;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			else
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
			break;
		case 4:
			dev_info(hs_dev->dev, "Setting quad configuration for mic");
			hs_dev->mic_channel_count = MIC_QUAD;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_quad01;
			hs_dev->mic_ch_count_val = 4;
			if (hs_dev->bit_depth == hsi2s_core->macro->regfield_bit_width16)
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
			else
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_four;
			break;
		default:
			dev_warn(hs_dev->dev, "Invalid number of channels entered");
			dev_warn(hs_dev->dev, "Defaulting to stereo configuration for mic");
			hs_dev->mic_channel_count = MIC_STEREO;
			hs_dev->mic_mode = hsi2s_core->macro->regfield_mic_mode_sd0;
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
		if (params->bit_clk > BIT_CLK_MAX) {
			dev_err(hs_dev->dev, "Bit clock rate exceeds maximum supported rate of 73.728MHz");
			return -EINVAL;
		}
		hs_dev->wrdma_periodic_length_bytes = set_periodic_length(params->bit_clk, params->buffer_ms);
		hs_dev->wrdma_periodic_length = hs_dev->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
		dev_info(hs_dev->dev, "Periodic length configured as %u words", hs_dev->wrdma_periodic_length);
		/* Bit depth */
		configure_bit_depth(hs_dev, params->bit_depth);
		dev_info(hs_dev->dev, "Bit depth configured as %u bits", params->bit_depth);
		/* Speaker channel */
		configure_spkr_channel(hs_dev, params->spkr_channel_count);
		dev_info(hs_dev->dev, "Speaker channel count configured as %u", params->spkr_channel_count);
		/* Mic channel */
		configure_mic_channel(hs_dev, params->mic_channel_count);
		dev_info(hs_dev->dev, "Mic channel count configured as %u", params->mic_channel_count);
		/* Check whether long rate is enabled */
		hs_dev->en_long_rate = params->en_long_rate;
		if (hs_dev->en_long_rate) {
			if (params->long_rate >= LONG_RATE_MIN && params->long_rate <= LONG_RATE_MAX) {
				hs_dev->long_rate = params->long_rate << hsi2s_core->macro->regfield_i2s_lrate_offset;
			} else {
				dev_warn(hs_dev->dev, "Invalid long rate value specified, disabling long rate");
				hs_dev->en_long_rate = 0;
			}
		}
	} else {
		dev_err(hs_dev->dev, "Passed null hsi2s_params structure");
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
	if (hs_dev->en_long_rate) {
		setbits(hs_dev->i2s_ctl, hs_dev->long_rate);
		setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_en_long_rate);
	}
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
	if (hs_dev->en_long_rate) {
		setbits(hs_dev->i2s_ctl, hs_dev->long_rate);
		setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_en_long_rate);
	}
	clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
	setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
	clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_i2s_reset);
}

/* Configure pcm rate */
static void configure_pcm_rate(struct hsi2s_device *hs_dev, u8 rate)
{
	switch (rate) {
		case PCM_RATE_8_BIT_CLKS:
			dev_info(hs_dev->dev, "Setting pcm rate as 8 bit clocks per frame sync");
			hs_dev->pcm_rate = hsi2s_core->macro->regfield_pcmrate_8;
			if (hs_dev->lpaif_mode == HS_PCM) {
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			}
			break;
		case PCM_RATE_16_BIT_CLKS:
			dev_info(hs_dev->dev, "Setting pcm rate as 16 bit clocks per frame sync");
			hs_dev->pcm_rate = hsi2s_core->macro->regfield_pcmrate_16;
			if (hs_dev->lpaif_mode == HS_PCM) {
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			}
			break;
		case PCM_RATE_32_BIT_CLKS:
			dev_info(hs_dev->dev, "Setting pcm rate as 32 bit clocks per frame sync");
			hs_dev->pcm_rate = hsi2s_core->macro->regfield_pcmrate_32;
			if (hs_dev->lpaif_mode == HS_PCM) {
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
			}
			break;
		case PCM_RATE_64_BIT_CLKS:
			dev_info(hs_dev->dev, "Setting pcm rate as 64 bit clocks per frame sync");
			hs_dev->pcm_rate = hsi2s_core->macro->regfield_pcmrate_64;
			if (hs_dev->lpaif_mode == HS_PCM) {
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
			}
			break;
		case PCM_RATE_128_BIT_CLKS:
			dev_info(hs_dev->dev, "Setting pcm rate as 128 bit clocks per frame sync");
			hs_dev->pcm_rate = hsi2s_core->macro->regfield_pcmrate_128;
			if (hs_dev->lpaif_mode == HS_PCM) {
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_four;
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_four;
			}
			break;
		case PCM_RATE_256_BIT_CLKS:
			dev_info(hs_dev->dev, "Setting pcm rate as 256 bit clocks per frame sync");
			hs_dev->pcm_rate = hsi2s_core->macro->regfield_pcmrate_256;
			if (hs_dev->lpaif_mode == HS_PCM) {
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_eight;
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_eight;
			}
			break;
		default:
			dev_warn(hs_dev->dev, "Undefined PCM rate. Setting default value of 256 bit clocks per frame sync");
			hs_dev->pcm_rate = hsi2s_core->macro->regfield_pcmrate_256;
			if (hs_dev->lpaif_mode == HS_PCM) {
				hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_eight;
				hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_eight;
			}
			break;
	}
}

/* Configure pcm sync source */
static void configure_pcm_sync_src(struct hsi2s_device *hs_dev, u8 sync_src)
{
	switch (sync_src) {
		case PCM_SYNC_EXT:
			dev_info(hs_dev->dev, "Setting pcm sync source as external");
			clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_sync_src);
			break;
		case PCM_SYNC_INT:
			dev_info(hs_dev->dev, "Setting pcm sync source as internal");
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_sync_src);
			break;
		default:
			dev_warn(hs_dev->dev, "Undefined sync source input. Setting default source(internal)");
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_sync_src);
			break;
	}
}

/* Configure pcm aux mode */
static void configure_pcm_aux_mode(struct hsi2s_device *hs_dev, u8 aux_mode)
{
	switch (aux_mode) {
		case PCM_AUXMODE_PCM:
			dev_info(hs_dev->dev, "Setting PCM mode(short sync)");
			clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_aux_mode);
			break;
		case PCM_AUXMODE_AUX:
			dev_info(hs_dev->dev, "Setting AUX mode(long sync)");
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_aux_mode);
			if (hs_dev->tdm_inv_sync) {
				dev_info(hs_dev->dev, "Inverting frame sync pulses");
				setbits(hs_dev->tdm_ctl, hsi2s_core->macro->bit_tdm_inv_rpcm_sync);
				setbits(hs_dev->tdm_ctl, hsi2s_core->macro->bit_tdm_inv_tpcm_sync);
			}
			break;
		default:
			dev_warn(hs_dev->dev, "Undefined AUX mode input. Setting default mode(PCM)");
			clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_aux_mode);
			break;
	}
}

/* Configure pcm rpcm width */
static void configure_pcm_rpcm_width(struct hsi2s_device *hs_dev, u8 rpcm_width)
{
	switch (rpcm_width) {
		case RPCM_WIDTH_8:
			dev_info(hs_dev->dev, "Setting RPCM width as 8 bits");
			clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_rpcm_width);
			break;
		case RPCM_WIDTH_16:
			dev_info(hs_dev->dev, "Setting RPCM width as 16 bits");
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_rpcm_width);
			break;
		default:
			dev_warn(hs_dev->dev, "Undefined RPCM width input. Setting default width(16)");
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_rpcm_width);
			break;
	}
}

/* Configure pcm tpcm width */
static void configure_pcm_tpcm_width(struct hsi2s_device *hs_dev, u8 tpcm_width)
{
	switch (tpcm_width) {
		case TPCM_WIDTH_8:
			dev_info(hs_dev->dev, "Setting TPCM width as 8 bits");
			clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_tpcm_width);
			break;
		case TPCM_WIDTH_16:
			dev_info(hs_dev->dev, "Setting TPCM width as 16 bits");
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_tpcm_width);
			break;
		default:
			dev_warn(hs_dev->dev, "Undefined TPCM width input. Setting default width(16)");
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_tpcm_width);
			break;
	}
}

/* Configure PCM parameters based on user input */
static int configure_pcm_params(struct hsi2s_device *hs_dev, struct hspcm_params *params)
{
	int ret = 0;

	if (params) {
		/* Set the periodic length */
		if (params->bit_clk > BIT_CLK_MAX) {
			dev_err(hs_dev->dev, "Bit clock rate exceeds maximum supported rate of 73.728MHz");
			return -EINVAL;
		}
		hs_dev->wrdma_periodic_length_bytes = set_periodic_length(params->bit_clk, params->buffer_ms);
		hs_dev->wrdma_periodic_length = hs_dev->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
		dev_info(hs_dev->dev, "Periodic length configured as %u words", hs_dev->wrdma_periodic_length);
		/* Set PCM rate */
		configure_pcm_rate(hs_dev, params->rate);
		/* Set PCM sync source */
		hs_dev->pcm_sync_src = params->sync_src;
		/* Set PCM aux mode */
		hs_dev->pcm_aux_mode = params->aux_mode;
		/* Set PCM rpcm width */
		hs_dev->pcm_rpcm_width = params->rpcm_width;
		/* Set PCM tpcm width */
		hs_dev->pcm_tpcm_width = params->tpcm_width;
	} else {
		dev_err(hs_dev->dev, "Passed null hspcm_params structure");
		ret = -EINVAL;
	}

	return ret;
}

/* Configure PCM control register */
static void configure_pcm_ctl(struct hsi2s_device *hs_dev)
{
	setbits(hs_dev->pcm_ctl, hs_dev->pcm_rate);
	setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_ctrl_data_oe);
	configure_pcm_sync_src(hs_dev, hs_dev->pcm_sync_src);
	configure_pcm_aux_mode(hs_dev, hs_dev->pcm_aux_mode);
}

/* Configure PCM control register for tx operation */
static void configure_pcm_tx(struct hsi2s_device *hs_dev)
{
	configure_pcm_tpcm_width(hs_dev, hs_dev->pcm_tpcm_width);
	setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset_tx);
	clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset_tx);
}

/* Configure PCM control register for rx operation */
static void configure_pcm_rx(struct hsi2s_device *hs_dev)
{
	configure_pcm_rpcm_width(hs_dev, hs_dev->pcm_rpcm_width);
	setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset_rx);
	clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset_rx);
}

/* Configure PCM rx slot */
static void configure_rpcm_slot(struct hsi2s_device *hs_dev, u32 slot, int enable)
{
	if (enable) {
		setbits(hs_dev->rpcm_slot_num, 1 << slot);
	} else {
		clearbits(hs_dev->rpcm_slot_num, 1 << slot);
	}
}

/* Enable RPCM slots */
static void enable_rpcm_slot(struct hsi2s_device *hs_dev)
{
	int slot;

	for (slot = 0; slot < MAX_SLOTS; slot++) {
		configure_rpcm_slot(hs_dev, slot, 1);
	}
}

/* Configure PCM tx slot */
static void configure_tpcm_slot(struct hsi2s_device *hs_dev, u32 slot, int enable)
{
	if (enable) {
		setbits(hs_dev->tpcm_slot_num, 1 << slot);
	} else {
		clearbits(hs_dev->tpcm_slot_num, 1 << slot);
	}
}

/* Enable TPCM slots */
static void enable_tpcm_slot(struct hsi2s_device *hs_dev)
{
	int slot;

	for (slot = 0; slot < MAX_SLOTS; slot++) {
		configure_tpcm_slot(hs_dev, slot, 1);
	}
}

/* Configure PCM lane */
static void configure_pcm_lane(struct hsi2s_device *hs_dev, u32 lane, int enable, int direction)
{
	switch(lane) {
		case LANE0:
			if (enable) {
				if (direction) {
					dev_info(hs_dev->dev, "Enabling mic on lane 0");
					setbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane0_dir);
				} else {
					dev_info(hs_dev->dev, "Enabling speaker on lane 0");
					clearbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane0_dir);
				}
				setbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane0_en);
			} else {
				dev_info(hs_dev->dev, "Disabling lane 0");
				clearbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane0_en);
			}
			break;

		case LANE1:
			if (enable) {
				if (direction) {
					dev_info(hs_dev->dev, "Enabling mic on lane 1");
					setbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane1_dir);
				} else {
					dev_info(hs_dev->dev, "Enabling speaker on lane 1");
					clearbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane1_dir);
				}
				setbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane1_en);
			} else {
				dev_info(hs_dev->dev, "Disabling lane 1");
				clearbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane1_en);
			}
			break;

		case LANE2:
			if (enable) {
				if (direction) {
					dev_info(hs_dev->dev, "Enabling mic on lane 2");
					setbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane2_dir);
				} else {
					dev_info(hs_dev->dev, "Enabling speaker on lane 2");
					clearbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane2_dir);
				}
				setbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane2_en);
			} else {
				dev_info(hs_dev->dev, "Disabling lane 2");
				clearbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane2_en);
			}
			break;

		case LANE3:
			if (enable) {
				if (direction) {
					dev_info(hs_dev->dev, "Enabling mic on lane 3");
					setbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane3_dir);
				} else {
					dev_info(hs_dev->dev, "Enabling speaker on lane 3");
					clearbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane3_dir);
				}
				setbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane3_en);
			} else {
				dev_info(hs_dev->dev, "Disabling lane 3");
				clearbits(hs_dev->pcm_lane_config, hsi2s_core->macro->bit_lane3_en);
			}
			break;
		default:
			dev_warn(hs_dev->dev, "Undefined PCM lane");
	}
}

/* Set PCM lane configuration on D0 and D1 lines */
static void set_pcm_lane_config(struct hsi2s_device *hs_dev, u32 config)
{
	switch (config) {
		case SINGLE_LANE:
			/* Single lane */
			dev_info(hs_dev->dev, "Single lane configuration");
			configure_pcm_lane(hs_dev, LANE0, 1, MIC);
			configure_pcm_lane(hs_dev, LANE1, 1, SPKR);
			break;
		case MULTI_LANE_RX:
			/* Multi lane Rx */
			dev_info(hs_dev->dev, "Multi lane Rx configuration");
			configure_pcm_lane(hs_dev, LANE0, 1, MIC);
			configure_pcm_lane(hs_dev, LANE1, 1, MIC);
			break;
		case MULTI_LANE_TX:
			/* Multi lane Tx */
			dev_info(hs_dev->dev, "Multi lane Tx configuration");
			configure_pcm_lane(hs_dev, LANE0, 1, SPKR);
			configure_pcm_lane(hs_dev, LANE1, 1, SPKR);
			break;
		default:
			/* Single lane */
			dev_warn(hs_dev->dev, "Setting default lane configuration(single lane)");
			configure_pcm_lane(hs_dev, LANE0, 1, MIC);
			configure_pcm_lane(hs_dev, LANE1, 1, SPKR);
	}
}

/* Configure TDM rate */
static void configure_tdm_rate(struct hsi2s_device *hs_dev, u32 rate)
{
	if (hs_dev->lpaif_mode == HS_PCM) {
		if (hs_dev->tdm_rate < 63) {
			hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_one;
			hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_one;
		} else if (hs_dev->tdm_rate < 127) {
			hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_two;
			hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_two;
		} else if (hs_dev->tdm_rate < 255) {
			hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_four;
			hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_four;
		} else {
			hs_dev->wpscnt_rddma = hsi2s_core->macro->regfield_rddma_wpscnt_eight;
			hs_dev->wpscnt_wrdma = hsi2s_core->macro->regfield_wrdma_wpscnt_eight;
		}
	}
}

/* Configure pcm tpcm width */
static void configure_tdm_sync_delay(struct hsi2s_device *hs_dev, u8 sync_delay)
{
	switch (sync_delay) {
		case DELAY_2_CYCLE:
			dev_info(hs_dev->dev, "Setting 2 cycle delay");
			hs_dev->tdm_sync_delay = hsi2s_core->macro->regfield_sync_delay_2;
			break;
		case DELAY_1_CYCLE:
			dev_info(hs_dev->dev, "Setting 1 cycle delay");
			hs_dev->tdm_sync_delay = hsi2s_core->macro->regfield_sync_delay_1;
			break;
		case DELAY_0_CYCLE:
			dev_info(hs_dev->dev, "Setting 0 cycle delay");
			hs_dev->tdm_sync_delay = hsi2s_core->macro->regfield_sync_delay_0;
			break;
		default:
			dev_warn(hs_dev->dev, "Undefined sync delay input. Setting 1 cycle delay");
			hs_dev->tdm_sync_delay = hsi2s_core->macro->regfield_sync_delay_1;
			break;
	}
}

/* Configure TDM parameters based on user input */
static int configure_tdm_params(struct hsi2s_device *hs_dev, struct hstdm_params *params)
{
	int ret = 0;

	if (params) {
		/* Set TDM enable flag */
		hs_dev->tdm_en = 1;
		/* Set TDM rate */
		hs_dev->tdm_rate = params->rate - 1;
		configure_tdm_rate(hs_dev, hs_dev->tdm_rate);
		/* Set RPCM width */
		hs_dev->tdm_rpcm_width = (params->rpcm_width - 1) << 9;
		/* Set TPCM width */
		hs_dev->tdm_tpcm_width = (params->tpcm_width - 1) << 14;
		/* Set sync delay */
		configure_tdm_sync_delay(hs_dev, params->sync_delay);
		/* Check whether different sample width is enabled */
		hs_dev->tdm_en_diff_sample_width = params->en_diff_sample_width;
		if (hs_dev->tdm_en_diff_sample_width) {
			hs_dev->tdm_tpcm_sample_width = (params->tpcm_sample_width - 1) << 5;
			hs_dev->tdm_rpcm_sample_width = params->rpcm_sample_width - 1;
		}
	} else {
		dev_err(hs_dev->dev, "Passed null hstdm_params structure");
		ret = -EINVAL;
	}

	return ret;
}

/* Configure TDM control register */
static void configure_tdm_ctl(struct hsi2s_device *hs_dev)
{
	setbits(hs_dev->tdm_ctl, hs_dev->tdm_rate |
				 hs_dev->tdm_rpcm_width |
				 hs_dev->tdm_tpcm_width |
				 hs_dev->tdm_sync_delay);
	if (hs_dev->tdm_en_diff_sample_width) {
		setbits(hs_dev->tdm_sample_width, hs_dev->tdm_tpcm_sample_width |
						  hs_dev->tdm_rpcm_sample_width);
		setbits(hs_dev->tdm_ctl, hsi2s_core->macro->bit_tdm_en_diff_sample_width);
	}
	setbits(hs_dev->tdm_ctl, hsi2s_core->macro->bit_tdm_en);
}

/* Configure the read DMA registers */
static void configure_rddma(struct hsi2s_device *hs_dev, int intf)
{
	writel_relaxed(hs_dev->read_buffer->handle, hs_dev->rddma_base);
	writel_relaxed(dma_buffer_length_words, hs_dev->rddma_buff_len);
	/* Use ping/pong size as periodic length */
	writel_relaxed(((dma_buffer_length_words + 1) / 2) - 1, hs_dev->rddma_per_len);

	if (intf == HS0_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hsi2s_core->macro->regfield_rddma_pri_audio_intf |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH0 |
					IRQ_UNDR_RDDMA_CH0 |
					IRQ_ERR_RDDMA_CH0);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr0");
	} else if (intf == HS1_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hsi2s_core->macro->regfield_rddma_sec_audio_intf |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH1 |
					IRQ_UNDR_RDDMA_CH1 |
					IRQ_ERR_RDDMA_CH1);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr1");
	} else if (intf == HS2_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hsi2s_core->macro->regfield_rddma_ter_audio_intf |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH2 |
					IRQ_UNDR_RDDMA_CH2 |
					IRQ_ERR_RDDMA_CH2);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr2");
	} else if (intf == HS3_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hsi2s_core->macro->regfield_rddma_quat_audio_intf |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH3 |
					IRQ_UNDR_RDDMA_CH3 |
					IRQ_ERR_RDDMA_CH3);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr3");
	} else if (intf == HS4_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hsi2s_core->macro->regfield_rddma_quin_audio_intf |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH4 |
					IRQ_UNDR_RDDMA_CH4 |
					IRQ_ERR_RDDMA_CH4);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr4");
	}
}

/* Configure the write DMA registers */
static void configure_wrdma(struct hsi2s_device *hs_dev, int intf)
{
	writel_relaxed(hs_dev->write_buffer->handle, hs_dev->wrdma_base);
	writel_relaxed(dma_buffer_length_words, hs_dev->wrdma_buff_len);
	/* Increase the FIFO watermark */
	writel_relaxed((WRDMA_RAM_LENGTH * intf), hs_dev->wrdma_ram_addr);
	writel_relaxed(WRDMA_RAM_LENGTH, hs_dev->wrdma_ram_len);

	/*
	 * Setting periodic length
	 * Normal mode - use the calculated length as per bit clock
	 * Loopback modes - use ping/pong size
	 */
	if (hs_dev->mode == NORMAL)
		writel_relaxed(hs_dev->wrdma_periodic_length - 1, hs_dev->wrdma_per_len);
	else
		writel_relaxed(((dma_buffer_length_words + 1) / 2) - 1, hs_dev->wrdma_per_len);

	if (intf == HS0_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hsi2s_core->macro->regfield_wrdma_pri_audio_intf |
					   hs_dev->wpscnt_wrdma |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH0 |
					IRQ_OVR_WRDMA_CH0 |
					IRQ_ERR_WRDMA_CH0);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr0");
	} else if (intf == HS1_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hsi2s_core->macro->regfield_wrdma_sec_audio_intf |
					   hs_dev->wpscnt_wrdma |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH1 |
					IRQ_OVR_WRDMA_CH1 |
					IRQ_ERR_WRDMA_CH1);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr1");
	} else if (intf == HS2_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hsi2s_core->macro->regfield_wrdma_ter_audio_intf |
					   hs_dev->wpscnt_wrdma |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH2 |
					IRQ_OVR_WRDMA_CH2 |
					IRQ_ERR_WRDMA_CH2);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr2");
	} else if (intf == HS3_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hsi2s_core->macro->regfield_wrdma_quat_audio_intf |
					   hs_dev->wpscnt_wrdma |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH3 |
					IRQ_OVR_WRDMA_CH3 |
					IRQ_ERR_WRDMA_CH3);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr3");
	} else if (intf == HS4_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hsi2s_core->macro->regfield_wrdma_quin_audio_intf |
					   hs_dev->wpscnt_wrdma |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq2_en, IRQ2_PER_WRDMA_CH4 |
					IRQ2_OVR_WRDMA_CH4 |
					IRQ2_ERR_WRDMA_CH4);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr4");
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

/* Configure the PCM control register for internal loopback */
static void configure_pcm_int_lb(struct hsi2s_device *hs_dev)
{
	setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_loopback);
	configure_pcm_ctl(hs_dev);
	if(hs_dev->tdm_en)
		configure_tdm_ctl(hs_dev);
	configure_pcm_tx(hs_dev);
	configure_pcm_rx(hs_dev);
	setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset);
	clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset);
}

/* Configure the read DMA registers */
static void configure_rddma_int_lb(struct hsi2s_device *hs_dev, int intf)
{
	writel_relaxed(hs_dev->read_buffer->handle, hs_dev->rddma_base);
	writel_relaxed(dma_buffer_length_words, hs_dev->rddma_buff_len);
	/* Use ping/pong size as periodic length */
	writel_relaxed(((dma_buffer_length_words + 1) / 2) - 1, hs_dev->rddma_per_len);

	if (intf == HS0_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_pri_audio_intf |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH0 |
					IRQ_UNDR_RDDMA_CH0 |
					IRQ_ERR_RDDMA_CH0);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr0");
	} else if (intf == HS1_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_sec_audio_intf |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH1 |
					IRQ_UNDR_RDDMA_CH1 |
					IRQ_ERR_RDDMA_CH1);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr1");
	} else if (intf == HS2_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_ter_audio_intf |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH2 |
					IRQ_UNDR_RDDMA_CH2 |
					IRQ_ERR_RDDMA_CH2);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr2");
	} else if (intf == HS3_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_quat_audio_intf |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH3 |
					IRQ_UNDR_RDDMA_CH3 |
					IRQ_ERR_RDDMA_CH3);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr3");
	} else if (intf == HS4_I2S) {
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_burst_en |
					   hsi2s_core->macro->bit_rddma_dyn_clk |
					   hs_dev->wpscnt_rddma |
					   hsi2s_core->macro->regfield_rddma_quin_audio_intf |
					   hsi2s_core->macro->regfield_rddma_fifo_wm8);
		setbits(hsi2s_core->irq_en, IRQ_PER_RDDMA_CH4 |
					IRQ_UNDR_RDDMA_CH4 |
					IRQ_ERR_RDDMA_CH4);
		dev_info(hs_dev->dev, "Configured rddma channel for sdr4");
	}
}

/* Configure the write DMA registers for internal loopback */
static void configure_wrdma_int_lb(struct hsi2s_device *hs_dev, int intf)
{
	writel_relaxed(hs_dev->write_buffer->handle, hs_dev->wrdma_base);
	writel_relaxed(dma_buffer_length_words, hs_dev->wrdma_buff_len);
	/* Use ping/pong size as periodic length */
	writel_relaxed(((dma_buffer_length_words + 1) / 2) - 1, hs_dev->wrdma_per_len);
	/* Increase the FIFO watermark */
	writel_relaxed((WRDMA_RAM_LENGTH * intf), hs_dev->wrdma_ram_addr);
	writel_relaxed(WRDMA_RAM_LENGTH, hs_dev->wrdma_ram_len);

	if (intf == HS0_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_loopback_ch0 |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH0 |
					IRQ_OVR_WRDMA_CH0 |
					IRQ_ERR_WRDMA_CH0);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr0");
	} else if (intf == HS1_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_loopback_ch1 |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH1 |
					IRQ_OVR_WRDMA_CH1 |
					IRQ_ERR_WRDMA_CH1);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr1");
	} else if (intf == HS2_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_loopback_ch2 |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH2 |
					IRQ_OVR_WRDMA_CH2 |
					IRQ_ERR_WRDMA_CH2);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr2");
	} else if (intf == HS3_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_loopback_ch3 |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq_en, IRQ_PER_WRDMA_CH3 |
					IRQ_OVR_WRDMA_CH3 |
					IRQ_ERR_WRDMA_CH3);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr3");
	} else if (intf == HS4_I2S) {
		setbits(hs_dev->wrdma_ctl, hsi2s_core->macro->bit_wrdma_dyn_clk |
					   hsi2s_core->macro->bit_wrdma_burst_en |
					   hs_dev->wpscnt_wrdma |
					   hsi2s_core->macro->regfield_wrdma_loopback_ch4 |
					   (WRDMA_RAM_LENGTH - 1) << 1);
		setbits(hsi2s_core->irq2_en, IRQ2_PER_WRDMA_CH4 |
					IRQ2_OVR_WRDMA_CH4 |
					IRQ2_ERR_WRDMA_CH4);
		dev_info(hs_dev->dev, "Enabling wrdma channel for sdr4");
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

/* Configure the PCM control register for external loopback */
static void configure_pcm_ext_lb(struct hsi2s_device *hs_dev)
{
	configure_pcm_ctl(hs_dev);
	if(hs_dev->tdm_en)
		configure_tdm_ctl(hs_dev);
	configure_pcm_tx(hs_dev);
	configure_pcm_rx(hs_dev);
	setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset);
	clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_reset);
}

/* Function to configure HS-I2S registers in normal mode */
static void configure_normal_mode(struct hsi2s_device *hs_dev, int intf)
{
	dev_info(hs_dev->dev, "Configuring normal mode operation");
	/* Set operational mode */
	hs_dev->mode = NORMAL;
	/* Reset the DMA registers */
	reset_rddma_registers(hs_dev);
	reset_wrdma_registers(hs_dev);
	if (hs_dev->lpaif_mode == HS_I2S) {
		/* Reset I2S control register */
		reg_clear(hs_dev->i2s_ctl);
		/* Reset I2S select register */
		clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
		/* Configure I2S control register */
		configure_i2s_spkr(hs_dev);
		configure_i2s_mic(hs_dev);
	} else {
		/* Reset PCM control register */
		reg_clear(hs_dev->pcm_ctl);
		/* Reset TDM control register */
		reg_clear(hs_dev->tdm_ctl);
		/* Set I2S select register */
		setbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
		configure_pcm_ctl(hs_dev);
		if(hs_dev->tdm_en)
			configure_tdm_ctl(hs_dev);
		configure_pcm_tx(hs_dev);
		configure_pcm_rx(hs_dev);
		/* Enable PCM slots for Rx and Tx */
		enable_rpcm_slot(hs_dev);
		enable_tpcm_slot(hs_dev);
		/* Set PCM lane configuration */
		set_pcm_lane_config(hs_dev, hs_dev->lane_config);
	}
	/* Configure RDDMA registers */
	configure_rddma(hs_dev, intf);
	/* Configure WRDMA registers */
	configure_wrdma(hs_dev, intf);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	if (hs_dev->lpaif_mode == HS_I2S)
		setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
	else
		setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
	/* Reset buffer pointers */
	hs_dev->read_buffer->last_copy = 1;
	hs_dev->read_buffer->last_xfer = 1;
	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
}

/* Function to configure HS-I2S registers in loopback mode */
static void configure_int_loopback_mode(struct hsi2s_device *hs_dev, int intf)
{
	dev_info(hs_dev->dev, "Configuring loopback mode operation");
	/* Set operational mode */
	hs_dev->mode = INTERNAL_LB;
	/* Reset the DMA registers */
	reset_rddma_registers(hs_dev);
	reset_wrdma_registers(hs_dev);
	if (hs_dev->lpaif_mode == HS_I2S) {
		/* Reset I2S control register */
		reg_clear(hs_dev->i2s_ctl);
		/* Reset I2S select register */
		clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
		/* Configure I2S control register */
		configure_i2s_int_lb(hs_dev);
	} else {
		/* Reset PCM control register */
		reg_clear(hs_dev->pcm_ctl);
		/* Reset TDM control register */
		reg_clear(hs_dev->tdm_ctl);
		/* Set I2S select register */
		setbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
		/* Configure PCM control register */
		configure_pcm_int_lb(hs_dev);
		/* Enable PCM slots for Rx and Tx */
		enable_rpcm_slot(hs_dev);
		enable_tpcm_slot(hs_dev);
		/* Set PCM lane configuration */
		set_pcm_lane_config(hs_dev, hs_dev->lane_config);
	}
	/* Configure RDDMA registers */
	configure_rddma_int_lb(hs_dev, intf);
	/* Configure WRDMA registers */
	configure_wrdma_int_lb(hs_dev, intf);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	if (hs_dev->lpaif_mode == HS_I2S)
		setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
	else
		setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
	/* Reset buffer pointers */
	hs_dev->read_buffer->last_copy = 1;
	hs_dev->read_buffer->last_xfer = 1;
	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
}

/* Function to configure HS-I2S registers in external loopback mode */
static void configure_ext_loopback_mode(struct hsi2s_device *hs_dev, int intf)
{
	dev_info(hs_dev->dev, "Configuring external loopback mode operation");
	/* Set operational mode */
	hs_dev->mode = EXTERNAL_LB_MASTER;
	/* Reset the DMA registers */
	reset_rddma_registers(hs_dev);
	reset_wrdma_registers(hs_dev);
	if (hs_dev->lpaif_mode == HS_I2S) {
		/* Reset I2S control register */
		reg_clear(hs_dev->i2s_ctl);
		/* Reset I2S select register */
		clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
		/* Configure I2S control register */
		configure_i2s_ext_lb(hs_dev);
	} else {
		/* Reset PCM control register */
		reg_clear(hs_dev->pcm_ctl);
		/* Reset TDM control register */
		reg_clear(hs_dev->tdm_ctl);
		/* Set I2S select register */
		setbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
		/* Configure PCM control register */
		configure_pcm_ext_lb(hs_dev);
		/* Enable PCM slots for Rx and Tx */
		enable_rpcm_slot(hs_dev);
		enable_tpcm_slot(hs_dev);
		/* Set PCM lane configuration */
		set_pcm_lane_config(hs_dev, hs_dev->lane_config);
	}
	/* Configure WRDMA registers */
	configure_wrdma(hs_dev, intf);
	/* Configure RDDMA registers */
	configure_rddma(hs_dev, intf);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	if (hs_dev->lpaif_mode == HS_I2S)
		setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
	else
		setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
	/* Reset buffer pointers */
	hs_dev->read_buffer->last_copy = 1;
	hs_dev->read_buffer->last_xfer = 1;
	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
}

/* Mic enabler thread for DAB mode */
static int dab_enabler(void *data)
{
	struct hsi2s_device *hs_dev;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO-1};

	hs_dev = (struct hsi2s_device *)data;

	/* Set maximum priority */
	sched_setscheduler(current, SCHED_FIFO, &param);

	while (1) {
		if (kthread_should_stop()) {
			dev_info(hs_dev->dev, "DAB enabler asked to exit...");
			break;
		}
		if (hsi2s_core->en_mic) {
			if (hs_dev->lpaif_mode == HS_I2S)
				setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
			else
				setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
		}
	}
	dev_info(hs_dev->dev, "Enabled mic for DAB mode");

	return 0;
}

/* Configure DAB MRC */
static int configure_dab_mrc(void)
{
	struct hsi2s_device *hs_dev;
	int i;
	int ret = 0;

	for (i = 0; i < hsi2s_core->i_count; i++) {
		if (!hsi2s_core->hsi2s_arr[i]) {
			dev_err(hsi2s_core->dev, "hs%d interface is not up", i);
			return -EINVAL;
		}
		hs_dev = hsi2s_core->hsi2s_arr[i];
		dev_info(hs_dev->dev, "Configuring normal mode operation on hs%d_i2s interface", i);
		/* Set operational mode */
		hs_dev->mode = NORMAL;
		if (hs_dev->lpaif_mode == HS_I2S) {
			/* Configure I2S control register */
			configure_i2s_mic(hs_dev);
		} else {
			/* Configure PCM control register */
			configure_pcm_ctl(hs_dev);
			if(hs_dev->tdm_en)
				configure_tdm_ctl(hs_dev);
			configure_pcm_rx(hs_dev);
		}

		/* Configure WRDMA registers */
		configure_wrdma(hs_dev, i);

		/* Clear the IRQs */
		clear_irqs();
	}

	/* Enable mic on different interfaces */
	for (i = 0; i < hsi2s_core->i_count; i++) {
		hsi2s_core->hsi2s_arr[i]->dab_thread = kthread_create(dab_enabler, hsi2s_core->hsi2s_arr[i],
								      "Mic enabler thread for DAB");
		if (hsi2s_core->hsi2s_arr[i]->dab_thread) {
			wake_up_process(hsi2s_core->hsi2s_arr[i]->dab_thread);
		} else {
			dev_err(hsi2s_core->hsi2s_arr[i]->dev, "Cannot create DAB enabler thread");
			return -EINVAL;
		}
	}

	msleep(1);

	/* Set the flag to enable mics on the interfaces */
	hsi2s_core->en_mic = 1;

	msleep(1);

	/* Stop the threads */
	for (i = 0; i < hsi2s_core->i_count; i++)
		kthread_stop(hsi2s_core->hsi2s_arr[i]->dab_thread);

	/* Reset the mic enabler flag */
	hsi2s_core->en_mic = 0;

	return ret;
}

/* Configure interface as master/slave */
static void configure_muxmode(struct hsi2s_device *hs_dev, int mode)
{
	if (mode) {
		/* Configure slave */
		setbits(hs_dev->lpaif_muxmode, BIT(0));
		setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_ws_src);
		dev_info(hs_dev->dev, "hs%d configured as slave", hs_dev->minor_num);
	} else {
		/* Configure master */
		clearbits(hs_dev->lpaif_muxmode, BIT(0));
		clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_ws_src);
		dev_info(hs_dev->dev, "hs%d configured as master", hs_dev->minor_num);
	}

}

/* DMA buffer callbacks */
static int dma_sg_alloc_compacted(struct sg_buffer *buf)
{
	unsigned int last_page = 0;
	unsigned long size = buf->size;

	while (size > 0) {
		struct page *pages;
		int order;
		int i;

		order = get_order(size);
		/* Don't over allocate*/
		if ((PAGE_SIZE << order) > size)
			order--;

		pages = NULL;
		while (!pages) {
			pages = alloc_pages(GFP_KERNEL | __GFP_ZERO |
					__GFP_NOWARN, order);
			if (pages)
				break;

			if (order == 0) {
				while (last_page--)
					__free_page(buf->pages[last_page]);
				return -ENOMEM;
			}
			order--;
		}

		split_page(pages, order);
		for (i = 0; i < (1 << order); i++)
			buf->pages[last_page++] = &pages[i];

		size -= PAGE_SIZE << order;
	}

	return 0;
}

static unsigned long get_contiguous_size(struct sg_table *sgt)
{
	struct scatterlist *s;
	unsigned int i;
	unsigned long size = 0;

	for_each_sg(sgt->sgl, s, sgt->nents, i) {
		pr_info("hsi2s: index=%lu size=%lu", i, sg_dma_len(s));
		size += sg_dma_len(s);
	}
	return size;
}

static void *dma_sg_alloc(struct device *dev, unsigned long size,
						  enum dma_data_direction dma_dir)
{
	struct sg_buffer *buf;
	struct sg_table *sgt;
	int ret;
	int num_pages;
	unsigned long contig_size;

	if (WARN_ON(!dev))
		return ERR_PTR(-EINVAL);

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	/* Initialize the SG wrapper structure */
	buf->dev = dev;
	buf->vaddr = NULL;
	buf->dma_dir = dma_dir;
	buf->offset = 0;
	buf->size = size;
	buf->num_pages = size >> PAGE_SHIFT;
	buf->dma_sgt = &buf->sg_table;
	/* Allocate the page array */
	buf->pages = kvmalloc_array(buf->num_pages, sizeof(struct page *),
				    GFP_KERNEL | __GFP_ZERO);
	if (!buf->pages) {
		dev_err(buf->dev, "Failed to allocate SG pages");
		goto fail_pages_array_alloc;
	}
	/* Allocate the pages */
	ret = dma_sg_alloc_compacted(buf);
	if (ret) {
		dev_err(buf->dev, "dma_sg_alloc_compacted() failed");
		goto fail_pages_alloc;
	}
	/* Initialize the SG table from the page array */
	ret = sg_alloc_table_from_pages(buf->dma_sgt, buf->pages,
			buf->num_pages, 0, size, GFP_KERNEL);
	if (ret) {
		dev_err(buf->dev, "Failed to allocate SG table");
		goto fail_table_alloc;
	}

	sgt = &buf->sg_table;
	/* Map the SG table for DMA operations */
	sgt->nents = dma_map_sg_attrs(buf->dev, sgt->sgl, sgt->orig_nents,
				      buf->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (!sgt->nents) {
		dev_err(buf->dev, "Failed to map SG table");
		goto fail_map;
	}
	/* Check the size of mapping */
	contig_size = get_contiguous_size(sgt);
	if (contig_size < size) {
		dev_err(buf->dev, "contiguous mapping is too small %lu/%lu\n",
			contig_size, size);
		goto fail_map_sg;
	}
	/* Store the DMA handle */
	buf->dma_addr = sg_dma_address(sgt->sgl);
	/* Create virtual address mapping */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,15,0)
	buf->vaddr = vm_map_ram(buf->pages, buf->num_pages, -1);
#else
	buf->vaddr = vm_map_ram(buf->pages, buf->num_pages, -1, PAGE_KERNEL);
#endif
	if (buf->vaddr == NULL) {
		dev_err(buf->dev, "Failed to map pages to vmalloc space");
		goto fail_map_sg;
	}

	dev_info(buf->dev, "%s: Allocated buffer of %d pages\n", __func__, buf->num_pages);
	return buf;

fail_map_sg:
	dma_unmap_sg_attrs(buf->dev, sgt->sgl, sgt->orig_nents,
			   buf->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
fail_map:
	sg_free_table(buf->dma_sgt);
fail_table_alloc:
	num_pages = buf->num_pages;
	while (num_pages--)
		__free_page(buf->pages[num_pages]);
fail_pages_alloc:
	kvfree(buf->pages);
fail_pages_array_alloc:
	kfree(buf);
	return ERR_PTR(-ENOMEM);
}

static void dma_sg_free(void *buf_priv)
{
	struct sg_buffer  *buf = buf_priv;
	struct sg_table *sgt = &buf->sg_table;
	int i = buf->num_pages;

	dev_err(buf->dev, "%s: Freeing buffer of %d pages\n", __func__, buf->num_pages);
	dma_unmap_sg_attrs(buf->dev, sgt->sgl, sgt->orig_nents,
			   buf->dma_dir, DMA_ATTR_SKIP_CPU_SYNC);
	if (buf->vaddr)
		vm_unmap_ram(buf->vaddr, buf->num_pages);
	sg_free_table(buf->dma_sgt);
	while (--i >= 0)
		__free_page(buf->pages[i]);
	kvfree(buf->pages);
	kfree(buf);
}

/* Function to allocate buffers */
static int hsi2s_buffer_init(struct hsi2s_device *hs_dev)
{
	int ret = 0;

	/* Allocate read buffer */
	dev_info(hs_dev->dev, "Allocating kernel buffer for read DMA");
	hs_dev->read_buffer = kzalloc(sizeof(*hs_dev->read_buffer),
				       GFP_KERNEL);
	if (!hs_dev->read_buffer) {
		ret = -ENOMEM;
		goto err_read_buffer;
	}

	hs_dev->read_buffer->buffer = dma_sg_alloc(hsi2s_core->dev, sizeof(int32_t) * (dma_buffer_length_words + 1), DMA_TO_DEVICE);
	if (IS_ERR(hs_dev->read_buffer->buffer)) {
		dev_err(hs_dev->dev, "SG read buffer allocation failed");
		hs_dev->read_buffer->buffer = NULL;
		ret = -ENOMEM;
		goto err_read_dma_buffer;
	}
	hs_dev->read_buffer->handle = hs_dev->read_buffer->buffer->dma_addr;
	hs_dev->read_buffer->ping_start = hs_dev->read_buffer->buffer->vaddr;
	hs_dev->read_buffer->pong_start = hs_dev->read_buffer->buffer->vaddr + (dma_buffer_length / 2);
	hs_dev->read_buffer->length = dma_buffer_length / 2;
	hs_dev->read_buffer->last_copy = 1;
	hs_dev->read_buffer->last_xfer = 1;

	/* Allocate write buffer */
	dev_info(hs_dev->dev, "Allocating kernel buffer for write DMA");
	hs_dev->write_buffer = kzalloc(sizeof(*hs_dev->write_buffer),
				       GFP_KERNEL);
	if (!hs_dev->write_buffer) {
		ret = -ENOMEM;
		goto err_read_dma_map;
	}

	hs_dev->write_buffer->buffer = dma_sg_alloc(hsi2s_core->dev, sizeof(int32_t) * (dma_buffer_length_words + 1), DMA_FROM_DEVICE);
	if (IS_ERR(hs_dev->write_buffer->buffer)) {
		dev_err(hs_dev->dev, "SG write buffer allocation failed");
		hs_dev->write_buffer->buffer = NULL;
		ret = -ENOMEM;
		goto err_write_dma_buffer;
	}
	hs_dev->write_buffer->handle = hs_dev->write_buffer->buffer->dma_addr;
	hs_dev->lpass_wrdma_start = hs_dev->write_buffer->buffer->vaddr;
	hs_dev->lpass_wrdma_end = hs_dev->lpass_wrdma_start + dma_buffer_length;
	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->data_ready = 0;
	hs_dev->write_buffer->pollin = 0;

	return ret;

err_write_dma_buffer:
	if (hs_dev->write_buffer) {
		kfree(hs_dev->write_buffer);
		hs_dev->write_buffer = NULL;
	}
err_read_dma_map:
	if (hs_dev->read_buffer->buffer) {
		dma_sg_free(hs_dev->read_buffer->buffer);
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
			dma_sg_free(hs_dev->write_buffer->buffer);
			hs_dev->write_buffer->buffer = NULL;
		}
		kfree(hs_dev->write_buffer);
		hs_dev->write_buffer = NULL;
	}

	/* Freeing read DMA buffer */
	if (hs_dev->read_buffer) {
		if (hs_dev->read_buffer->buffer) {
			dma_sg_free(hs_dev->read_buffer->buffer);
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
		dev_err(hs_dev->dev, "Unable to map device registers");
		return ret;
	}

	reset_registers(hs_dev);

	/* Allocate kernel buffers */
	ret = hsi2s_buffer_init(hs_dev);
	if (ret < 0) {
		dev_err(hs_dev->dev, "Buffer allocation failed");
		return ret;
	}

	/* Initialize the wait queues */
	init_waitqueue_head(&hs_dev->wq_rddma);
	init_waitqueue_head(&hs_dev->wq_wrdma);

	return ret;
}

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
/* SMMU functions */

/* Function to init smmu */
static int hsi2s_smmu_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dma_iommu_mapping *mapping;
	u32 iova_ap_mapping[2];
	int ret = 0;

	hsi2s_core->hsi2s_smmu_ctx = kzalloc(sizeof(*hsi2s_core->hsi2s_smmu_ctx),
					 GFP_KERNEL);
	if (!hsi2s_core->hsi2s_smmu_ctx)
		return -ENOMEM;

	ret = of_property_read_u32_array(dev->of_node, "qcom,iova-mapping",
					 iova_ap_mapping, 2);
	if (ret) {
		dev_err(hsi2s_core->dev, "Failed to read smmu start/size iova addresses");
		goto err_smmu_probe;
	}

	hsi2s_core->hsi2s_smmu_ctx->va_start = iova_ap_mapping[0];
	hsi2s_core->hsi2s_smmu_ctx->va_size = iova_ap_mapping[1];
	hsi2s_core->hsi2s_smmu_ctx->smmu_pdev = pdev;

	hsi2s_core->hsi2s_smmu_ctx->mapping =
		arm_iommu_create_mapping(dev->bus,
					 hsi2s_core->hsi2s_smmu_ctx->va_start,
					 hsi2s_core->hsi2s_smmu_ctx->va_size);
	if (IS_ERR_OR_NULL(hsi2s_core->hsi2s_smmu_ctx->mapping)) {
		dev_err(hsi2s_core->dev, "Fail to create mapping");
		/* assume this failure is because iommu driver is not ready */
		ret = -EPROBE_DEFER;
		goto err_smmu_probe;
	}
	dev_info(hsi2s_core->dev, "Successfully Created SMMU mapping");
	hsi2s_core->hsi2s_smmu_ctx->valid = true;
	mapping = hsi2s_core->hsi2s_smmu_ctx->mapping;

	dev_info(hsi2s_core->dev, "Configuring SMMU S1");
	ret = arm_iommu_attach_device(&hsi2s_core->hsi2s_smmu_ctx->smmu_pdev->dev,
				      mapping);
	if (ret) {
		dev_err(hsi2s_core->dev, "couldn't attach to IOMMU ret=%d", ret);
		goto err_smmu_probe;
	}

	hsi2s_core->hsi2s_smmu_ctx->iommu_domain =
	iommu_get_domain_for_dev(&hsi2s_core->hsi2s_smmu_ctx->smmu_pdev->dev);

	dev_info(hsi2s_core->dev, "Successfully attached to IOMMU");
	return ret;

err_smmu_probe:
	if (hsi2s_core->hsi2s_smmu_ctx->mapping)
		arm_iommu_release_mapping(hsi2s_core->hsi2s_smmu_ctx->mapping);
	hsi2s_core->hsi2s_smmu_ctx->valid = false;

	kfree(hsi2s_core->hsi2s_smmu_ctx);
	hsi2s_core->hsi2s_smmu_ctx = NULL;

	return ret;
}
#endif

/* GPIO management functions */

/* Function to configure gpio pins */
static int hsi2s_configure_gpio_pins(struct platform_device *pdev, int active)
{
	struct pinctrl *pinctrl;
	struct pinctrl_state *hsi2s_state;
	int ret = 0;

	pinctrl = devm_pinctrl_get(&pdev->dev);
	if (IS_ERR_OR_NULL(pinctrl)) {
		ret = PTR_ERR(pinctrl);
		dev_err(&pdev->dev, "Failed to get pinctrl, err = %d", ret);
		return ret;
	}
	dev_info(&pdev->dev, "get pinctrl succeed\n");

	if (active) {
		hsi2s_state = pinctrl_lookup_state(pinctrl, "default");
		if (IS_ERR_OR_NULL(hsi2s_state)) {
			ret = PTR_ERR(hsi2s_state);
			dev_err(&pdev->dev, "Failed to get default state, err = %d\n", ret);
			return ret;
		}
		dev_info(&pdev->dev, "Get default state succeed\n");
	} else {
		hsi2s_state = pinctrl_lookup_state(pinctrl, "sleep");
		if (IS_ERR_OR_NULL(hsi2s_state)) {
			ret = PTR_ERR(hsi2s_state);
			dev_err(&pdev->dev, "Failed to get sleep state, err = %d\n", ret);
			return ret;
		}
		dev_info(&pdev->dev, "Get sleep state succeed\n");
	}

	ret = pinctrl_select_state(pinctrl, hsi2s_state);
	if (ret)
		dev_err(&pdev->dev, "Unable to set pinctrl state, err = %d", ret);
	else
		dev_info(&pdev->dev, "Set pinctrl state succeeded");

	return ret;
}

/* Clock management functions */

#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM) && defined(CONFIG_QCOM_QMI_HELPERS)
/* Function to disable clocks for SA8155/SA8195 using QMI */
static int hsi2s_adsp_disable_clks(void)
{
	int ret = 0;

	dev_info(hsi2s_core->dev, "Disabling LPASS clocks via QMI");
	ret = hsi2s_clk_ctrl_send_sync_msg(hsi2s_core->qmi_dev, 0);

	if (ret < 0)
		dev_err(hsi2s_core->dev, "Failed to disable LPASS clocks\n");

	return ret;
}

/* Function to enable clocks for SA8155/SA8195 using QMI */
static int hsi2s_adsp_enable_clks(void)
{
	int ret = 0;

	dev_info(hsi2s_core->dev, "Enabling LPASS clocks via QMI");
	ret = hsi2s_clk_ctrl_send_sync_msg(hsi2s_core->qmi_dev, 1);

	if (ret < 0)
		dev_err(hsi2s_core->dev, "Failed to enable LPASS clocks\n");

	return ret;
}
#endif

/* Function to enable/disable core clocks for SA8155/SA8195 */
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
		dev_info(hsi2s_core->dev, "Enable core clocks for SA8155/SA8195");
		if (!(readl_relaxed(lpass_core_cbcr) & 0x1))
			setbits(lpass_core_cbcr, 0x1);
		if (!(readl_relaxed(hs_rdmem) & 0x1))
			setbits(hs_rdmem, 0x1);
		if (!(readl_relaxed(hs_wrmem) & 0x1))
			setbits(hs_wrmem, 0x1);
		if (!(readl_relaxed(lpass_mport) & 0x1))
			setbits(lpass_mport, 0x1);
		dev_info(hsi2s_core->dev, "Core clocks enabled for SA8155/SA8195");
	} else {
		dev_info(hsi2s_core->dev, "Disable core clocks for SA8155/SA8195");
		clearbits(hs_wrmem, 0x1);
		clearbits(hs_rdmem, 0x1);
		dev_info(hsi2s_core->dev, "Core clocks disabled for SA8155/SA8195");
	}

	iounmap(lpass_core_cbcr);
	iounmap(hs_rdmem);
	iounmap(hs_wrmem);
	iounmap(lpass_mport);
}

/* Function to enable/disable interface clocks for SA8155/SA8195 */
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
		dev_info(hsi2s_core->dev, "Enable interface clocks for SA8155/SA8195");
		setbits(hs_if0_ibit, 0x1);
		setbits(hs_if1_ibit, 0x1);
		setbits(hs_if2_ibit, 0x1);
		setbits(hs_if0_ebit, 0x1);
		setbits(hs_if1_ebit, 0x1);
		setbits(hs_if2_ebit, 0x1);
		setbits(hs_if0_mclk, 0x1);
		setbits(hs_if1_mclk, 0x1);
		setbits(hs_if2_mclk, 0x1);
		dev_info(hsi2s_core->dev, "Interface clocks enabled for SA8155/SA8195");
	} else {
		dev_info(hsi2s_core->dev, "Disable interface clocks for SA8155/SA8195");
		clearbits(hs_if0_ibit, 0x1);
		clearbits(hs_if1_ibit, 0x1);
		clearbits(hs_if2_ibit, 0x1);
		clearbits(hs_if0_ebit, 0x1);
		clearbits(hs_if1_ebit, 0x1);
		clearbits(hs_if2_ebit, 0x1);
		clearbits(hs_if0_mclk, 0x1);
		clearbits(hs_if1_mclk, 0x1);
		clearbits(hs_if2_mclk, 0x1);
		dev_info(hsi2s_core->dev, "Interface clocks disabled for SA8155/SA8195");
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

/* Function to enable/disable core clocks for SA8295 */
static void m_modify_core_clks(int enable)
{
	void __iomem *lpass_core_cbcr;
	void __iomem *hs_rdmem;
	void __iomem *hs_wrmem;
	void __iomem *lpass_mport;

	lpass_core_cbcr = ioremap(0x391F000, 4);
	hs_rdmem = ioremap(0x390800C, 4);
	hs_wrmem = ioremap(0x3908000, 4);
	lpass_mport = ioremap(0x3923000, 4);

	if (enable) {
		dev_info(hsi2s_core->dev, "Enable core clocks for SA8295");
		if (!(readl_relaxed(lpass_core_cbcr) & 0x1))
			setbits(lpass_core_cbcr, 0x1);
		if (!(readl_relaxed(hs_rdmem) & 0x1))
			setbits(hs_rdmem, 0x1);
		if (!(readl_relaxed(hs_wrmem) & 0x1))
			setbits(hs_wrmem, 0x1);
		if (!(readl_relaxed(lpass_mport) & 0x1))
			setbits(lpass_mport, 0x1);
		dev_info(hsi2s_core->dev, "Core clocks enabled for SA8295/SA8255");
	} else {
		dev_info(hsi2s_core->dev, "Disable core clocks for SA8295/SA8255");
		clearbits(hs_wrmem, 0x1);
		clearbits(hs_rdmem, 0x1);
		dev_info(hsi2s_core->dev, "Core clocks disabled for SA8295/SA855");
	}

	iounmap(lpass_core_cbcr);
	iounmap(hs_rdmem);
	iounmap(hs_wrmem);
	iounmap(lpass_mport);
}


/* Function to enable/disable interface clocks for SA8295 */
static void m_modify_interface_clks(int enable)
{
	void __iomem *hs_if0_ibit;
	void __iomem *hs_if1_ibit;
	void __iomem *hs_if2_ibit;
	void __iomem *hs_if3_ibit;
	void __iomem *hs_if4_ibit;
	void __iomem *hs_if0_ebit;
	void __iomem *hs_if1_ebit;
	void __iomem *hs_if2_ebit;
	void __iomem *hs_if3_ebit;
	void __iomem *hs_if4_ebit;

	hs_if0_ibit = ioremap(0x3905018, 4);
	hs_if0_ebit = ioremap(0x390501C, 4);
	hs_if1_ibit = ioremap(0x3906018, 4);
	hs_if1_ebit = ioremap(0x390601C, 4);
	hs_if2_ibit = ioremap(0x3907018, 4);
	hs_if2_ebit = ioremap(0x390701C, 4);
	hs_if3_ibit = ioremap(0x3909018, 4);
	hs_if3_ebit = ioremap(0x390901C, 4);
	hs_if4_ibit = ioremap(0x390A018, 4);
	hs_if4_ebit = ioremap(0x390A01C, 4);

	if (enable) {
		dev_info(hsi2s_core->dev, "Enable interface clocks for SA8295");
		setbits(hs_if0_ibit, 0x1);
		setbits(hs_if1_ibit, 0x1);
		setbits(hs_if2_ibit, 0x1);
		setbits(hs_if3_ibit, 0x1);
		setbits(hs_if4_ibit, 0x1);
		setbits(hs_if0_ebit, 0x1);
		setbits(hs_if1_ebit, 0x1);
		setbits(hs_if2_ebit, 0x1);
		setbits(hs_if3_ebit, 0x1);
		setbits(hs_if4_ebit, 0x1);
		dev_info(hsi2s_core->dev, "Interface clocks enabled for SA8295");
	} else {
		dev_info(hsi2s_core->dev, "Disable interface clocks for SA8295");
		clearbits(hs_if0_ibit, 0x1);
		clearbits(hs_if1_ibit, 0x1);
		clearbits(hs_if2_ibit, 0x1);
		clearbits(hs_if3_ibit, 0x1);
		clearbits(hs_if4_ibit, 0x1);
		clearbits(hs_if0_ebit, 0x1);
		clearbits(hs_if1_ebit, 0x1);
		clearbits(hs_if2_ebit, 0x1);
		clearbits(hs_if3_ebit, 0x1);
		clearbits(hs_if4_ebit, 0x1);
		dev_info(hsi2s_core->dev, "Interface clocks disabled for SA8295");
	}

	iounmap(hs_if0_ibit);
	iounmap(hs_if0_ebit);
	iounmap(hs_if1_ibit);
	iounmap(hs_if1_ebit);
	iounmap(hs_if2_ibit);
	iounmap(hs_if2_ebit);
	iounmap(hs_if3_ibit);
	iounmap(hs_if3_ebit);
	iounmap(hs_if4_ibit);
	iounmap(hs_if4_ebit);
}

/* Function to disable core clocks for SA6155 */
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

/* Function to enable core clocks for SA6155 */
static int hsi2s_enable_core_clks(struct platform_device *pdev)
{
	struct hsi2s_core *hs_core;
	struct device *dev = &pdev->dev;
	int ret = 0;

	hs_core = (struct hsi2s_core *)platform_get_drvdata(pdev);

	hs_core->core_clk = devm_clk_get(dev, "core_clk");
	if (!hs_core->core_clk) {
		dev_err(hsi2s_core->dev, "Unable to get sdr_core clock ");
		return -EIO;
	}

	hs_core->csr_hclk = devm_clk_get(dev, "csr_hclk");
	if (!hs_core->csr_hclk) {
		dev_err(hsi2s_core->dev, "Unable to get sdr_csr_hclk clock ");
		return -EIO;
	}

	hs_core->wr0_mem_clk = devm_clk_get(dev, "wr0_mem_clk");
	if (!hs_core->wr0_mem_clk) {
		dev_err(hsi2s_core->dev, "Unable to get sdr_wr0_mem_clk clock ");
		return -EIO;
	}

	hs_core->wr1_mem_clk = devm_clk_get(dev, "wr1_mem_clk");
	if (!hs_core->wr1_mem_clk) {
		dev_err(hsi2s_core->dev, "Unable to get sdr_wr1_mem_clk clock ");
		return -EIO;
	}

	hs_core->wr2_mem_clk = devm_clk_get(dev, "wr2_mem_clk");
	if (!hs_core->wr2_mem_clk) {
		dev_err(hsi2s_core->dev, "Unable to get sdr_wr2_mem_clk clock ");
		return -EIO;
	}

	ret = clk_prepare_enable(hs_core->core_clk);
	if (ret) {
		dev_err(hsi2s_core->dev, "Failed to enable sdr_core clock");
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_core->csr_hclk);
	if (ret) {
		dev_err(hsi2s_core->dev, "Failed to enable sdr_csr_hclk clock");
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_core->wr0_mem_clk);
	if (ret) {
		dev_err(hsi2s_core->dev, "Failed to enable sdr_wr0_mem_clk clock");
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_core->wr1_mem_clk);
	if (ret) {
		dev_err(hsi2s_core->dev, "Failed to enable sdr_wr1_mem_clk clock");
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_core->wr2_mem_clk);
	if (ret) {
		dev_err(hsi2s_core->dev, "Failed to enable sdr_wr2_mem_clk clock");
		goto fail_clk;
	}

	return ret;

fail_clk:
	hsi2s_disable_core_clks(pdev);
	return ret;
}

/* Function to suspend core clocks for SA6155 */
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

/* Function to resume core clocks SA6155 */
static int hsi2s_resume_core_clks(struct platform_device *pdev)
{
	struct hsi2s_core *hs_core;
	int ret = 0;

	hs_core = (struct hsi2s_core *)platform_get_drvdata(pdev);

	if (hs_core->core_clk) {
		ret = clk_prepare_enable(hs_core->core_clk);
		if (ret) {
			dev_err(hsi2s_core->dev, "Failed to enable sdr_core clock");
			goto fail_clk;
		}
	}

	if (hs_core->csr_hclk) {
		ret = clk_prepare_enable(hs_core->csr_hclk);
		if (ret) {
			dev_err(hsi2s_core->dev, "Failed to enable sdr_csr_hclk clock");
			goto fail_clk;
		}
	}

	if (hs_core->wr0_mem_clk) {
		ret = clk_prepare_enable(hs_core->wr0_mem_clk);
		if (ret) {
			dev_err(hsi2s_core->dev, "Failed to enable sdr_wr0_mem_clk clock");
			goto fail_clk;
		}
	}

	if (hs_core->wr1_mem_clk) {
		ret = clk_prepare_enable(hs_core->wr1_mem_clk);
		if (ret) {
			dev_err(hsi2s_core->dev, "Failed to enable sdr_wr1_mem_clk clock");
			goto fail_clk;
		}
	}

	if (hs_core->wr2_mem_clk) {
		ret = clk_prepare_enable(hs_core->wr2_mem_clk);
		if (ret) {
			dev_err(hsi2s_core->dev, "Failed to enable sdr_wr2_mem_clk clock");
			goto fail_clk;
		}
	}

	return ret;
fail_clk:
	dev_err(hsi2s_core->dev, "Failed to enable some core clocks");
	hsi2s_disable_core_clks(pdev);
	return ret;
}

/* Function to disable interface clocks SA6155 */
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

/* Function to enable interface clocks SA6155 */
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
		dev_err(hs_dev->dev, "Unable to get interface clock for SDR%d interface",
		       hs_dev->minor_num);
		ret = -EIO;
		goto fail_clk;
	}

	ret = clk_prepare_enable(hs_dev->intf_clk);
	if (ret) {
#ifdef SKIP_BIT_CLK_CHECK
		ret = 0;
#else
		dev_err(hs_dev->dev, "Failed to enable interface clock for SDR%d\n",
			hs_dev->minor_num);
#endif
	}

fail_clk:
	return ret;
}

/* Function to suspend interface clocks SA6155 */
static void hsi2s_suspend_intf_clks(struct platform_device *pdev)
{
	struct hsi2s_device *hs_dev;

	hs_dev = (struct hsi2s_device *)platform_get_drvdata(pdev);
	if (hs_dev->intf_clk)
		clk_disable_unprepare(hs_dev->intf_clk);
}

/* Function to resume interface clocks SA6155 */
static int hsi2s_resume_intf_clks(struct platform_device *pdev)
{
	struct hsi2s_device *hs_dev;
	int ret = 0;

	hs_dev = (struct hsi2s_device *)platform_get_drvdata(pdev);
	if (hs_dev->intf_clk) {
		ret = clk_prepare_enable(hs_dev->intf_clk);
		if (ret) {
#ifdef SKIP_BIT_CLK_CHECK
			ret = 0;
#else
			dev_err(hs_dev->dev, "Failed to enable interface clock for SDR%d\n",
				hs_dev->minor_num);
#endif
		}
	}

	return ret;
}

/* RDDMA Scheduler */
static int rddma_schedule(void *data)
{
	struct hsi2s_device *hs_dev;

	hs_dev = (struct hsi2s_device *)data;
	dev_info(hs_dev->dev, "Starting RDDMA scheduler...");

	while (1) {
		if (kthread_should_stop()) {
			dev_info(hs_dev->dev, "RDDMA scheduler asked to exit...");
			break;
		}

		msleep(1);

		if (!hs_dev->rddma_copy_busy) {
			hs_dev->rddma_copy_busy = 1;

			/* Enable the DMA channel */
			setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_en);
			/* Enable speaker */
			if (hs_dev->lpaif_mode == HS_I2S)
				setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_spkr_en);
			else
				setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_en_tx);
			/* Set the RDDMA busy flag */
			hs_dev->rddma_xfer_busy = 1;
			dev_info(hs_dev->dev, "DMA scheduled on hs%d interface", hs_dev->minor_num);
		}
	}

	return 0;
}

/* Interrupt thread function */
static irqreturn_t irq_thread_fn(int irq, void *devid)
{
#ifndef DISABLE_DEVICE_READ
	u32 temp_len;
	u32 write_len;
	void *tail;
#endif
	u32 irq_stat;
	struct hsi2s_device **hs_arr;
	int slave;

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

	/* Checking for read DMA interrupt on HS3 interface */
	if (hs_arr[3]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on read channel 3 */
		if (irq_stat & IRQ_PER_RDDMA_CH3) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_RDDMA_CH3);
			hs_arr[3]->read_buffer->last_xfer = !hs_arr[3]->read_buffer->last_xfer;
			/* Notify event write */
			wake_up_interruptible(&hs_arr[3]->wq_rddma);
		}
	}

	/* Checking for read DMA interrupt on HS4 interface */
	if (hs_arr[4]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on read channel 4 */
		if (irq_stat & IRQ_PER_RDDMA_CH4) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_RDDMA_CH4);
			hs_arr[4]->read_buffer->last_xfer = !hs_arr[4]->read_buffer->last_xfer;
			/* Notify event write */
			wake_up_interruptible(&hs_arr[4]->wq_rddma);
		}
	}

	/* Checking for write DMA interrupt on HS0 interface */
	if (hs_arr[0]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on write channel 0 */
		if (irq_stat & IRQ_PER_WRDMA_CH0) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_WRDMA_CH0);
#ifndef DISABLE_DEVICE_READ
			write_len = readl_relaxed(hs_arr[0]->wrdma_per_len);
			write_len += 1;
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
#endif
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
#ifndef DISABLE_DEVICE_READ
			write_len = readl_relaxed(hs_arr[1]->wrdma_per_len);
			write_len += 1;
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
#endif
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
#ifndef DISABLE_DEVICE_READ
			write_len = readl_relaxed(hs_arr[2]->wrdma_per_len);
			write_len += 1;
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
#endif
			hs_arr[2]->write_buffer->pollin = 1;
			/* Notify event read */
			wake_up_interruptible(&hs_arr[2]->wq_wrdma);
		}
	}

	/* Checking for write DMA interrupt on HS3 interface */
	if (hs_arr[3]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Periodic interrupt on write channel 3 */
		if (irq_stat & IRQ_PER_WRDMA_CH3) {
			setbits(hsi2s_core->irq_clear, IRQ_PER_WRDMA_CH3);
#ifndef DISABLE_DEVICE_READ
			write_len = readl_relaxed(hs_arr[3]->wrdma_per_len);
			write_len += 1;
			write_len *= BYTES_PER_SAMPLE;

			tail = hs_arr[3]->write_buffer->tail;
			if (tail + write_len >= hs_arr[3]->lpass_wrdma_end) {
				temp_len = hs_arr[3]->lpass_wrdma_end - tail;
				tail = hs_arr[3]->lpass_wrdma_start + (write_len - temp_len);
			}
			else
				tail += write_len;
			hs_arr[3]->write_buffer->tail = tail;
			hs_arr[3]->write_buffer->data_ready = 1;
#endif
			hs_arr[3]->write_buffer->pollin = 1;
			/* Notify event read */
			wake_up_interruptible(&hs_arr[3]->wq_wrdma);
		}
	}

	/* Checking for write DMA interrupt on HS4 interface */
	if (hs_arr[4]) {
		irq_stat = readl_relaxed(hsi2s_core->irq2_stat);
		/* Periodic interrupt on write channel 4 */
		if (irq_stat & IRQ2_PER_WRDMA_CH4) {
			setbits(hsi2s_core->irq2_clear, IRQ2_PER_WRDMA_CH4);
#ifndef DISABLE_DEVICE_READ
			write_len = readl_relaxed(hs_arr[4]->wrdma_per_len);
			write_len += 1;
			write_len *= BYTES_PER_SAMPLE;

			tail = hs_arr[4]->write_buffer->tail;
			if (tail + write_len >= hs_arr[4]->lpass_wrdma_end) {
				temp_len = hs_arr[4]->lpass_wrdma_end - tail;
				tail = hs_arr[4]->lpass_wrdma_start + (write_len - temp_len);
			}
			else
				tail += write_len;
			hs_arr[4]->write_buffer->tail = tail;
			hs_arr[4]->write_buffer->data_ready = 1;
#endif
			hs_arr[4]->write_buffer->pollin = 1;
			/* Notify event read */
			wake_up_interruptible(&hs_arr[4]->wq_wrdma);
		}
	}

	/* Check for DMA errors on HS0 interface */
	if (hs_arr[0]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Error on read channel 0 */
		if (irq_stat & (IRQ_UNDR_RDDMA_CH0 | IRQ_ERR_RDDMA_CH0)) {
			dev_err(hsi2s_core->dev, "Error on read DMA channel 0");
			if (irq_stat & IRQ_UNDR_RDDMA_CH0) {
				setbits(hsi2s_core->irq_clear, IRQ_UNDR_RDDMA_CH0);
				dev_err(hsi2s_core->dev, "Underrun detected");
			}
			if (irq_stat & IRQ_ERR_RDDMA_CH0) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_RDDMA_CH0);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}
		/* Error on write channel 0 */
		if (irq_stat & (IRQ_OVR_WRDMA_CH0 | IRQ_ERR_WRDMA_CH0)) {
			dev_err(hsi2s_core->dev, "Error on write DMA channel 0");
			if (irq_stat & IRQ_OVR_WRDMA_CH0) {
				setbits(hsi2s_core->irq_clear, IRQ_OVR_WRDMA_CH0);
				dev_err(hsi2s_core->dev, "Overrun detected");
			}
			if (irq_stat & IRQ_ERR_WRDMA_CH0) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_WRDMA_CH0);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}

	}

	/* Check for DMA errors on HS1 interface */
	if (hs_arr[1]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Error on read channel 1 */
		if (irq_stat & (IRQ_UNDR_RDDMA_CH1 | IRQ_ERR_RDDMA_CH1)) {
			dev_err(hsi2s_core->dev, "Error on read DMA channel 1");
			if (irq_stat & IRQ_UNDR_RDDMA_CH1) {
				setbits(hsi2s_core->irq_clear, IRQ_UNDR_RDDMA_CH1);
				dev_err(hsi2s_core->dev, "Underrun detected");
			}
			if (irq_stat & IRQ_ERR_RDDMA_CH1) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_RDDMA_CH1);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}
		/* Error on write channel 1 */
		if (irq_stat & (IRQ_OVR_WRDMA_CH1 | IRQ_ERR_WRDMA_CH1)) {
			dev_err(hsi2s_core->dev, "Error on write DMA channel 1");
			if (irq_stat & IRQ_OVR_WRDMA_CH1) {
				setbits(hsi2s_core->irq_clear, IRQ_OVR_WRDMA_CH1);
				dev_err(hsi2s_core->dev, "Overrun detected");
			}
			if (irq_stat & IRQ_ERR_WRDMA_CH1) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_WRDMA_CH1);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}

	}

	/* Check for DMA errors on HS2 interface */
	if (hs_arr[2]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Error on read channel 2 */
		if (irq_stat & (IRQ_UNDR_RDDMA_CH2 | IRQ_ERR_RDDMA_CH2)) {
			dev_err(hsi2s_core->dev, "Error on read DMA channel 2");
			if (irq_stat & IRQ_UNDR_RDDMA_CH2) {
				setbits(hsi2s_core->irq_clear, IRQ_UNDR_RDDMA_CH2);
				dev_err(hsi2s_core->dev, "Underrun detected");
			}
			if (irq_stat & IRQ_ERR_RDDMA_CH2) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_RDDMA_CH2);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}
		/* Error on write channel 2 */
		if (irq_stat & (IRQ_OVR_WRDMA_CH2 | IRQ_ERR_WRDMA_CH2)) {
			dev_err(hsi2s_core->dev, "Error on write DMA channel 2");
			if (irq_stat & IRQ_OVR_WRDMA_CH2) {
				setbits(hsi2s_core->irq_clear, IRQ_OVR_WRDMA_CH2);
				dev_err(hsi2s_core->dev, "Overrun detected");
			}
			if (irq_stat & IRQ_ERR_WRDMA_CH2) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_WRDMA_CH2);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}

	}

	/* Check for DMA errors on HS3 interface */
	if (hs_arr[3]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Error on read channel 3 */
		if (irq_stat & (IRQ_UNDR_RDDMA_CH3 | IRQ_ERR_RDDMA_CH3)) {
			dev_err(hsi2s_core->dev, "Error on read DMA channel 3");
			if (irq_stat & IRQ_UNDR_RDDMA_CH3) {
				setbits(hsi2s_core->irq_clear, IRQ_UNDR_RDDMA_CH3);
				dev_err(hsi2s_core->dev, "Underrun detected");
			}
			if (irq_stat & IRQ_ERR_RDDMA_CH3) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_RDDMA_CH3);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}
		/* Error on write channel 3 */
		if (irq_stat & (IRQ_OVR_WRDMA_CH3 | IRQ_ERR_WRDMA_CH3)) {
			dev_err(hsi2s_core->dev, "Error on write DMA channel 3");
			if (irq_stat & IRQ_OVR_WRDMA_CH3) {
				setbits(hsi2s_core->irq_clear, IRQ_OVR_WRDMA_CH3);
				dev_err(hsi2s_core->dev, "Overrun detected");
			}
			if (irq_stat & IRQ_ERR_WRDMA_CH3) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_WRDMA_CH3);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}
	}

	/* Check for DMA errors on HS4 interface */
	if (hs_arr[4]) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		/* Error on read channel 4 */
		if (irq_stat & (IRQ_UNDR_RDDMA_CH4 | IRQ_ERR_RDDMA_CH4)) {
			dev_err(hsi2s_core->dev, "Error on read DMA channel 4");
			if (irq_stat & IRQ_UNDR_RDDMA_CH4) {
				setbits(hsi2s_core->irq_clear, IRQ_UNDR_RDDMA_CH4);
				dev_err(hsi2s_core->dev, "Underrun detected");
			}
			if (irq_stat & IRQ_ERR_RDDMA_CH4) {
				setbits(hsi2s_core->irq_clear, IRQ_ERR_RDDMA_CH4);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}
		irq_stat = readl_relaxed(hsi2s_core->irq2_stat);
		/* Error on write channel 4 */
		if (irq_stat & (IRQ2_OVR_WRDMA_CH4 | IRQ2_ERR_WRDMA_CH4)) {
			dev_err(hsi2s_core->dev, "Error on write DMA channel 4");
			if (irq_stat & IRQ2_OVR_WRDMA_CH4) {
				setbits(hsi2s_core->irq2_clear, IRQ2_OVR_WRDMA_CH4);
				dev_err(hsi2s_core->dev, "Overrun detected");
			}
			if (irq_stat & IRQ2_ERR_WRDMA_CH4) {
				setbits(hsi2s_core->irq2_clear, IRQ2_ERR_WRDMA_CH4);
				dev_err(hsi2s_core->dev, "Bus error detected");
			}
		}
	}

	/* Rate detection */
	if (hsi2s_core->is_rate_enabled) {
		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		if (irq_stat & IRQ_PRI_RD_DIFF_RATE) {
			setbits(hsi2s_core->irq_clear, IRQ_PRI_RD_DIFF_RATE);
			/* Get the new WS rate */
			hsi2s_core->pri_ws_rate = get_ws_rate(PRI_RATE_DET);
			dev_info(hsi2s_core->dev, "WS rate detected as %lu Hz ", hsi2s_core->pri_ws_rate);
			if (hsi2s_core->pri_ws_rate) {
				slave = hsi2s_core->pri_rate_interface;
				/* Disable mic */
				if (hs_arr[slave]->lpaif_mode == HS_I2S)
					clearbits(hs_arr[slave]->i2s_ctl, hsi2s_core->macro->bit_mic_en);
				else
					clearbits(hs_arr[slave]->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
				clearbits(hs_arr[slave]->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
				/* Set the new periodic length */
				hs_arr[slave]->wrdma_periodic_length_bytes = (set_periodic_length(calculate_bit_rate(hs_arr[slave], PRI_RATE_DET),
															  hs_arr[slave]->data_buffer_ms_val));
				hs_arr[slave]->wrdma_periodic_length = hs_arr[slave]->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
				dev_info(hsi2s_core->dev, "Periodic length reconfigured to %lu words", hs_arr[slave]->wrdma_periodic_length);
				writel_relaxed(hs_arr[slave]->wrdma_periodic_length - 1, hs_arr[slave]->wrdma_per_len);
				/* Enable mic */
				setbits(hs_arr[slave]->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
				if (hs_arr[slave]->lpaif_mode == HS_I2S)
					setbits(hs_arr[slave]->i2s_ctl, hsi2s_core->macro->bit_mic_en);
				else
					setbits(hs_arr[slave]->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
			}
		}

		irq_stat = readl_relaxed(hsi2s_core->irq_stat);
		if (irq_stat & IRQ_SEC_RD_DIFF_RATE) {
			setbits(hsi2s_core->irq_clear, IRQ_SEC_RD_DIFF_RATE);
			/* Get the new WS rate */
			hsi2s_core->sec_ws_rate = get_ws_rate(SEC_RATE_DET);
			dev_info(hsi2s_core->dev, "WS rate detected as %lu Hz ", hsi2s_core->sec_ws_rate);
			if (hsi2s_core->sec_ws_rate) {
				slave = hsi2s_core->sec_rate_interface;
				/* Disable mic */
				if (hs_arr[slave]->lpaif_mode == HS_I2S)
					clearbits(hs_arr[slave]->i2s_ctl, hsi2s_core->macro->bit_mic_en);
				else
					clearbits(hs_arr[slave]->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
				clearbits(hs_arr[slave]->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
				/* Set the new periodic length */
				hs_arr[slave]->wrdma_periodic_length_bytes = (set_periodic_length(calculate_bit_rate(hs_arr[slave],SEC_RATE_DET),
															  hs_arr[slave]->data_buffer_ms_val));
				hs_arr[slave]->wrdma_periodic_length = hs_arr[slave]->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
				dev_info(hsi2s_core->dev, "Periodic length reconfigured to %lu words", hs_arr[slave]->wrdma_periodic_length);
				writel_relaxed(hs_arr[slave]->wrdma_periodic_length - 1, hs_arr[slave]->wrdma_per_len);
				/* Enable mic */
				setbits(hs_arr[slave]->wrdma_ctl, hsi2s_core->macro->bit_wrdma_en);
				if (hs_arr[slave]->lpaif_mode == HS_I2S)
					setbits(hs_arr[slave]->i2s_ctl, hsi2s_core->macro->bit_mic_en);
				else
					setbits(hs_arr[slave]->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
			}
		}
	}

	mutex_unlock(&hsi2s_core->irqlock);

	return IRQ_HANDLED;
}

/* Interrupt handler function */
static irqreturn_t i2s_interrupt_handler(int irq, void *dev_id)
{
	return IRQ_WAKE_THREAD;
}

/* File operation functions for character drivers */

/* Function to read from the Rx buffer and transfer data to user space */
static ssize_t device_read(struct file *file, char __user *buffer,
			   size_t length, loff_t *offset)
{
#ifndef DISABLE_DEVICE_READ
	struct hsi2s_device *hs_dev;
	int temp_length;
	int copy_len;
	int bytes_read = 0;
	int ret = 0;
	void *head;

	hs_dev = (struct hsi2s_device *)file->private_data;
	head = hs_dev->write_buffer->head;

	temp_length = (readl_relaxed(hs_dev->wrdma_per_len) + 1) * 4;

	while (length > temp_length) {
		if (head == hs_dev->write_buffer->tail) {
			wait_event_interruptible(hs_dev->wq_wrdma,
			hs_dev->write_buffer->data_ready == 1);
		}

		hs_dev->write_buffer->data_ready = 0;

		if (head + temp_length >= hs_dev->lpass_wrdma_end) {
			usleep_range(10000,10000);
			dma_sync_sg_for_cpu(hsi2s_core->dev,
					    hs_dev->write_buffer->buffer->dma_sgt->sgl,
					    hs_dev->write_buffer->buffer->dma_sgt->orig_nents,
					    DMA_FROM_DEVICE);
			copy_len = hs_dev->lpass_wrdma_end - head;
			ret = copy_to_user(buffer + bytes_read,
					   head,
					   copy_len);
			if (ret) {
				dev_err(hs_dev->dev, "Error copying data to userspace");
				return -ret;
			}
			bytes_read += copy_len;
			dma_sync_sg_for_cpu(hsi2s_core->dev,
					    hs_dev->write_buffer->buffer->dma_sgt->sgl,
					    hs_dev->write_buffer->buffer->dma_sgt->orig_nents,
					    DMA_FROM_DEVICE);
			ret = copy_to_user(buffer + bytes_read,
				   hs_dev->lpass_wrdma_start,
				   temp_length - copy_len);
			if (ret) {
				dev_err(hs_dev->dev, "Error copying data to userspace");
				return -ret;
			}
			head = hs_dev->lpass_wrdma_start + (temp_length - copy_len);
			bytes_read += (temp_length - copy_len);
		} else  {
			usleep_range(10000,10000);
			dma_sync_sg_for_cpu(hsi2s_core->dev,
					    hs_dev->write_buffer->buffer->dma_sgt->sgl,
					    hs_dev->write_buffer->buffer->dma_sgt->orig_nents,
					    DMA_FROM_DEVICE);
			ret = copy_to_user(buffer + bytes_read,
					   head,
					   temp_length);
			if (ret) {
				dev_err(hs_dev->dev, "Error copying data to userspace");
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
		dma_sync_sg_for_cpu(hsi2s_core->dev,
				    hs_dev->write_buffer->buffer->dma_sgt->sgl,
				    hs_dev->write_buffer->buffer->dma_sgt->orig_nents,
				    DMA_FROM_DEVICE);
		copy_len = hs_dev->lpass_wrdma_end - head;
		ret = copy_to_user(buffer + bytes_read,
				   head,
				   copy_len);
		if (ret) {
			dev_err(hs_dev->dev, "Error copying data to userspace");
			return -ret;
		}
		bytes_read += copy_len;
		dma_sync_sg_for_cpu(hsi2s_core->dev,
				    hs_dev->write_buffer->buffer->dma_sgt->sgl,
				    hs_dev->write_buffer->buffer->dma_sgt->orig_nents,
				    DMA_FROM_DEVICE);
		ret = copy_to_user(buffer + bytes_read,
			   hs_dev->lpass_wrdma_start,
			   length - copy_len);
		if (ret) {
			dev_err(hs_dev->dev, "Error copying data to userspace");
			return -ret;
		}
		head = hs_dev->lpass_wrdma_start + (length - copy_len);
		bytes_read += (length - copy_len);
	} else  {
		usleep_range(10000,10000);
		dma_sync_sg_for_cpu(hsi2s_core->dev,
				    hs_dev->write_buffer->buffer->dma_sgt->sgl,
				    hs_dev->write_buffer->buffer->dma_sgt->orig_nents,
				    DMA_FROM_DEVICE);
		ret = copy_to_user(buffer + bytes_read,
				   head,
				   length);
		if (ret) {
			dev_err(hs_dev->dev, "Error copying data to userspace");
			return -ret;
		}
		head += length;
		bytes_read += length;
	}
	hs_dev->write_buffer->head = head;

	return bytes_read;
#else
	pr_err("[hsi2s] device_read() is disabled. Use shared memory read using poll()");
	return -EINVAL;
#endif
}

/* Function to write data from user space to the Tx buffer */
static ssize_t device_write(struct file *file, const char __user *buffer,
			    size_t length, loff_t *offset)
{
	struct hsi2s_device *hs_dev;
	int bytes_written = 0;
	int temp_length;
	int ret=0;

	hs_dev = (struct hsi2s_device *)file->private_data;
	temp_length = hs_dev->read_buffer->length;

	while (length > temp_length) {
		if (hs_dev->rddma_in_progress) {
			if (!(hs_dev->read_buffer->last_copy ^ hs_dev->read_buffer->last_xfer))
				wait_event_interruptible(hs_dev->wq_rddma,
							(hs_dev->read_buffer->last_copy ^ hs_dev->read_buffer->last_xfer) == 1);
		}

		if (hs_dev->read_buffer->last_copy) {
			ret = copy_from_user(hs_dev->read_buffer->ping_start,
				       (const void __user *)buffer + bytes_written,
				       temp_length);
		} else {
			ret = copy_from_user(hs_dev->read_buffer->pong_start,
				       (const void __user *)buffer + bytes_written,
				       temp_length);
		}
		if (ret) {
		dev_err(hs_dev->dev, "Error copying data to userspace");
		return -ret;
	    }

		dma_sync_sg_for_device(hsi2s_core->dev,
				       hs_dev->read_buffer->buffer->dma_sgt->sgl,
				       hs_dev->read_buffer->buffer->dma_sgt->orig_nents,
				       DMA_TO_DEVICE);
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
		ret = copy_from_user(hs_dev->read_buffer->ping_start,
			       (const void __user *)buffer + bytes_written,
			       length);
	} else {
		memset(hs_dev->read_buffer->pong_start, 0, hs_dev->read_buffer->length);
		ret = copy_from_user(hs_dev->read_buffer->pong_start,
			       (const void __user *)buffer + bytes_written,
			       length);
	}
	if (ret) {
		dev_err(hs_dev->dev, "Error copying data to userspace");
		return -ret;
	}
	dma_sync_sg_for_device(hsi2s_core->dev,
			       hs_dev->read_buffer->buffer->dma_sgt->sgl,
			       hs_dev->read_buffer->buffer->dma_sgt->orig_nents,
			       DMA_TO_DEVICE);
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
	struct hsi2s_device *hs_dev;
	int minor_num;

	/* Find the minor number of the device */
	minor_num = MINOR(inode->i_rdev);
	hs_dev = hsi2s_core->hsi2s_arr[minor_num];

	if (hs_dev->client_count) {
		dev_err(hs_dev->dev, "Device busy");
		return -EBUSY;
	}

	/* Increment client count */
	hs_dev->client_count++;
	dev_err(hs_dev->dev, "Client connected. Active clients : %d",
		hs_dev->client_count);

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
	dev_err(hs_dev->dev, "Client disconnected. Active clients : %d",
		hs_dev->client_count);

	/* Decrement usage count to be able to properly close the module. */
	module_put(THIS_MODULE);

	return 0;
}

static int toggle_bit_clock(struct hsi2s_device *hs_dev)
{
	int ret = 0;
#if ((defined(CONFIG_QTI_GVM) || defined(CONFIG_QTI_QUIN_GVM)) && defined(CONFIG_MSM_HAB))
	u32 resp_size = sizeof(msg_t);
#endif

	dev_info(hsi2s_core->dev, "Toggling bit clock directions\n");
#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM)
#if defined(CONFIG_QCOM_QMI_HELPERS)
	if (hsi2s_core->qmi_dev) {
		ret = hsi2s_adsp_enable_clks();
		if (ret < 0) {
			dev_err(hsi2s_core->dev, "Failed to toggle bit clocks\n");
			return ret;
		}
	}
	dev_info(hs_dev->dev, "Toggled bit clock direction\n");
#else
	dev_info(hs_dev->dev, "QMI kernel configuration is not enabled\n");
#endif
#else
#if defined(CONFIG_MSM_HAB)
	hsi2s_core->hab_req->clk_en = 1;

	ret = habmm_socket_send(hsi2s_core->hab_handle, hsi2s_core->hab_req, resp_size, 0);
	if (ret) {
		dev_err(hsi2s_core->dev, "habmm socket send failed (%d)\n", ret);
		return ret;
	}

	ret = habmm_socket_recv(hsi2s_core->hab_handle, hsi2s_core->hab_resp, &resp_size,
		  UINT_MAX, HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	if (ret) {
		dev_err(hsi2s_core->dev, "habmm socket receive failed (%d)\n", ret);
		return ret;
	}

	if (hsi2s_core->hab_resp->rsp) {
		dev_err(hsi2s_core->dev, "error response (%d)\n", hsi2s_core->hab_resp->rsp);
		return -EIO;
	}
#else
	dev_info(hs_dev->dev, "HAB kernel configuration is not enabled\n");
#endif
#endif

	return ret;
}

static int ioctl_handler1(struct hsi2s_device *hs_dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int minor;

	if (hs_dev->client_count != 1) {
		dev_err(hs_dev->dev, "Mode already set by previous client\n");
		return -EINVAL;
	}

	minor = hs_dev->minor_num;

	if (cmd == LPAIF_NORMAL_MODE) {
		dev_info(hs_dev->dev, "Triggering normal operation\n");
		/* Setting slave mode for SA8155/SA8195 targets */
		if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195)
			configure_muxmode(hs_dev, 1);
		configure_normal_mode(hs_dev, minor);
		hs_dev->slave = minor;

	} else if (cmd == LPAIF_INTERNAL_LOOPBACK) {
		dev_info(hs_dev->dev, "Triggering internal loopback\n");
		/* Start the read DMA scheduler */
		dev_info(hs_dev->dev, "Creating DMA scheduler thread\n");
		hs_dev->rddma_thread = kthread_create(rddma_schedule, hs_dev,
							  "DMA scheduler thread");
		if (hs_dev->rddma_thread) {
			wake_up_process(hs_dev->rddma_thread);
		} else {
			dev_err(hs_dev->dev, "Cannot create rddma scheduler thread\n");
			return -EINVAL;
		}
		/* Configure the interface registers */
		configure_int_loopback_mode(hs_dev, minor);
		hs_dev->slave = minor;
	} else if (cmd == LPAIF_EXTERNAL_LOOPBACK) {
		if (hsi2s_core->target == 6155) {
			dev_err(hs_dev->dev, "Mode not supported by target\n");
			return -EINVAL;
		}
		dev_warn(hs_dev->dev, "Triggering external loopback on master\n");
		/* Start the read DMA scheduler */
		dev_info(hs_dev->dev, "Creating DMA scheduler thread\n");
		hs_dev->rddma_thread = kthread_create(rddma_schedule, hs_dev,
							  "DMA scheduler thread");
		if (hs_dev->rddma_thread) {
			wake_up_process(hs_dev->rddma_thread);
		} else {
			dev_err(hs_dev->dev, "Cannot create rddma scheduler thread\n");
			return -EINVAL;
		}
		/* Configure the interface registers */
		configure_muxmode(hs_dev, 0);
		configure_ext_loopback_mode(hs_dev, minor);
		hs_dev->slave = minor;
	} else {
		if (hsi2s_core->target == 6155) {
			dev_err(hs_dev->dev, "Mode not supported by target\n");
			return -EINVAL;
		}
		dev_info(hs_dev->dev, "Setting master/slave muxmode configuration\n");
		configure_muxmode(hs_dev, arg);
	}

	return ret;
}

static int ioctl_handler2(struct hsi2s_device *hs_dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int minor;

	if (hs_dev->client_count != 1) {
		dev_err(hs_dev->dev, "Mode already set by previous client\n");
		return -EINVAL;
	}

	minor = hs_dev->minor_num;

	if (cmd == LPAIF_SPEAKER) {
		dev_info(hs_dev->dev, "Configuring hs%d as speaker\n", hs_dev->minor_num);
		/* Start the read DMA scheduler */
		dev_info(hs_dev->dev, "Creating DMA scheduler thread\n");
		hs_dev->rddma_thread = kthread_create(rddma_schedule, hs_dev,
							  "DMA scheduler thread");
		if (hs_dev->rddma_thread) {
			wake_up_process(hs_dev->rddma_thread);
		} else {
			dev_err(hs_dev->dev, "Cannot create rddma scheduler thread\n");
			return -EINVAL;
		}
		/* Configure the interface registers */
		if (hs_dev->lpaif_mode == HS_I2S) {
			/* Reset I2S select register */
			clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
			configure_i2s_spkr(hs_dev);
		} else {
			/* Set I2S select register */
			setbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
			configure_pcm_ctl(hs_dev);
			if (hs_dev->tdm_en)
				configure_tdm_ctl(hs_dev);
			configure_pcm_tx(hs_dev);
			/* Enable PCM slots for Tx */
			enable_tpcm_slot(hs_dev);
			/* Set PCM lane configuration */
			set_pcm_lane_config(hs_dev, hs_dev->lane_config);
		}
		configure_rddma(hs_dev, minor);
	} else if (cmd == LPAIF_MIC) {
		dev_info(hs_dev->dev, "Configuring hs%d as mic\n", hs_dev->minor_num);
		if (hs_dev->lpaif_mode == HS_I2S) {
			/* Reset I2S select register */
			clearbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
			configure_i2s_mic(hs_dev);
		} else {
			/* Set I2S select register */
			setbits(hs_dev->i2s_sel, hsi2s_core->macro->bit_i2s_sel);
			configure_pcm_ctl(hs_dev);
			if (hs_dev->tdm_en)
				configure_tdm_ctl(hs_dev);
			configure_pcm_rx(hs_dev);
			/* Enable PCM slots for Rx */
			enable_rpcm_slot(hs_dev);
			/* Set PCM lane configuration */
			set_pcm_lane_config(hs_dev, hs_dev->lane_config);
		}
		configure_wrdma(hs_dev, minor);

		if (hs_dev->lpaif_mode == HS_I2S)
			setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_mic_en);
		else
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_en_rx);
	} else if (cmd == LPAIF_SET_SLAVE) {
		if (hsi2s_core->target == 6155) {
			dev_err(hs_dev->dev, "Mode not supported by target\n");
			return -EINVAL;
		}
		dev_info(hs_dev->dev, "Triggering external loopback with hs%d master and hs%d slave\n",
				 hs_dev->minor_num, arg);
		hs_dev->slave = arg;
		hs_dev->mode = EXTERNAL_LB_MASTER_SLAVE;
		hsi2s_core->hsi2s_arr[arg]->mode = EXTERNAL_LB_MASTER_SLAVE;
	} else {
		hs_dev->rddma_copy_busy = 1;
		/* Enable the DMA channel */
		setbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_en);
		/* Enable speaker */
		if (hs_dev->lpaif_mode == HS_I2S)
			setbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_spkr_en);
		else
			setbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_en_tx);
		/* Set the RDDMA busy flags */
		hs_dev->rddma_xfer_busy = 1;
		hs_dev->rddma_in_progress = 1;
	}

	return ret;
}

static int ioctl_handler3(struct hsi2s_device *hs_dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	void __iomem *clk_val_reg;
	void __iomem *clk_update_reg;

	if (hs_dev->client_count != 1) {
		dev_err(hs_dev->dev, "Mode already set by previous client\n");
		return -EINVAL;
	}

	if (cmd == LPAIF_DEINIT_TX) {
		dev_info(hs_dev->dev, "Stopping rddma\n");
		hs_dev->rddma_copy_busy = 1;
		/* Disable speaker */
		if (hs_dev->lpaif_mode == HS_I2S)
			clearbits(hs_dev->i2s_ctl, hsi2s_core->macro->bit_spkr_en);
		else
			clearbits(hs_dev->pcm_ctl, hsi2s_core->macro->bit_pcm_en_tx);
		/* Disable the DMA channel */
		clearbits(hs_dev->rddma_ctl, hsi2s_core->macro->bit_rddma_en);
		/* Clear the RDDMA busy flags */
		hs_dev->rddma_xfer_busy = 0;
		hs_dev->rddma_in_progress = 0;
		/* Stop the DMA scheduler thread */
		kthread_stop(hs_dev->rddma_thread);
	} else if (cmd == LPAIF_SET_CLOCK) {
		if (hsi2s_core->target == 6155) {
			dev_err(hs_dev->dev, "Master mode not supported by target\n");
			return -EINVAL;
		}
		dev_info(hs_dev->dev, "Configuring master clock on HS%d interface\n",
				 hs_dev->minor_num);
		if (hs_dev->minor_num == 0) {
			clk_update_reg = ioremap(HS0_BITCLK_CMD_REG, 4);
			clk_val_reg = ioremap(HS0_BITCLK_CFG_REG, 4);

			clearbits(clk_val_reg, HS_BITCLK_RESET);
			setbits(clk_val_reg, arg);
			setbits(clk_update_reg, HS_BITCLK_UPDATE);
		} else if (hs_dev->minor_num == 1) {
			clk_update_reg = ioremap(HS1_BITCLK_CMD_REG, 4);
			clk_val_reg = ioremap(HS1_BITCLK_CFG_REG, 4);

			clearbits(clk_val_reg, HS_BITCLK_RESET);
			setbits(clk_val_reg, arg);
			setbits(clk_update_reg, HS_BITCLK_UPDATE);
		} else {
			clk_update_reg = ioremap(HS2_BITCLK_CMD_REG, 4);
			clk_val_reg = ioremap(HS2_BITCLK_CFG_REG, 4);

			clearbits(clk_val_reg, HS_BITCLK_RESET);
			setbits(clk_val_reg, arg);
			setbits(clk_update_reg, HS_BITCLK_UPDATE);
		}
		dev_info(hs_dev->dev, "Re-configured master clock\n");
	} else if (cmd == LPAIF_RESET) {
		if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195) {
			dev_info(hs_dev->dev, "Resetting muxmode register\n");
			reg_clear(hs_dev->lpaif_muxmode);
		}
		if (hs_dev->lpaif_mode == HS_I2S) {
			dev_info(hs_dev->dev, "Resetting I2S control register\n");
			reg_clear(hs_dev->i2s_ctl);
			dev_info(hs_dev->dev, "Resetting DMA registers\n");
			reset_rddma_registers(hs_dev);
			reset_wrdma_registers(hs_dev);
			/* Clear IRQs */
			clear_irqs();
			/* Reset buffer pointers */
			hs_dev->read_buffer->last_copy = 1;
			hs_dev->read_buffer->last_xfer = 1;
			hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
			hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
			hs_dev->write_buffer->data_ready = 0;
			hs_dev->write_buffer->pollin = 0;
			hs_dev->rddma_xfer_busy = 0;
			hs_dev->rddma_copy_busy = 1;
			hs_dev->rddma_in_progress = 0;
		} else {
			dev_info(hs_dev->dev, "Resetting PCM control registers\n");
			reg_clear(hs_dev->pcm_ctl);
			reg_clear(hs_dev->tdm_ctl);
			reg_clear(hs_dev->tdm_sample_width);
			dev_info(hs_dev->dev, "Resetting DMA registers\n");
			reset_rddma_registers(hs_dev);
			reset_wrdma_registers(hs_dev);
			/* Clear IRQs */
			clear_irqs();
			/* Reset buffer pointers */
			hs_dev->read_buffer->last_copy = 1;
			hs_dev->read_buffer->last_xfer = 1;
			hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
			hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
			hs_dev->write_buffer->data_ready = 0;
			hs_dev->write_buffer->pollin = 0;
			hs_dev->rddma_xfer_busy = 0;
			hs_dev->rddma_copy_busy = 1;
			hs_dev->rddma_in_progress = 0;
		}
	} else {
		dev_info(hs_dev->dev, "Setting LPAIF mode\n");
		configure_lpaif_mode(hs_dev, (u8)arg);
	}

	return ret;
}

static int ioctl_handler4(struct hsi2s_device *hs_dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	struct hsi2s_params *i2s_params;
	struct hspcm_params *pcm_params;
	struct hstdm_params *tdm_params;

	if (hs_dev->client_count != 1) {
		dev_err(hs_dev->dev, "Mode already set by previous client\n");
		return -EINVAL;
	}

	if (cmd == I2S_CONFIG_PARAMS) {
		dev_info(hs_dev->dev, "Configuring I2S parameters from test application\n");
		i2s_params = kzalloc(sizeof(struct hsi2s_params), GFP_KERNEL);
		if (!i2s_params) {
			dev_err(hs_dev->dev, "Failed to allocate params structure\n");
			return -ENOMEM;
		}
		ret = copy_from_user(i2s_params, (const void __user *)arg, sizeof(struct hsi2s_params));
		ret = configure_i2s_params(hs_dev, i2s_params);
		if (ret < 0) {
			dev_err(hs_dev->dev, "Failed to configure I2S parameters\n");
			ret = -EINVAL;
		}
		kfree(i2s_params);
		i2s_params = NULL;
	} else if (cmd == PCM_CONFIG_PARAMS) {
		dev_info(hs_dev->dev, "Configuring PCM parameters from test application\n");
		pcm_params = kzalloc(sizeof(struct hspcm_params), GFP_KERNEL);
		if (!pcm_params) {
			dev_err(hs_dev->dev, "Failed to allocate params structure\n");
			return -ENOMEM;
		}
		ret = copy_from_user(pcm_params, (const void __user *)arg, sizeof(struct hspcm_params));
		ret = configure_pcm_params(hs_dev, pcm_params);
		if (ret < 0) {
			dev_err(hs_dev->dev, "Failed to configure PCM parameters\n");
			ret = -EINVAL;
		}
		kfree(pcm_params);
		pcm_params = NULL;
	} else if (cmd == TDM_CONFIG_PARAMS) {
		dev_info(hs_dev->dev, "Configuring TDM parameters from test application\n");
		tdm_params = kzalloc(sizeof(struct hstdm_params), GFP_KERNEL);
		if (!tdm_params) {
			dev_err(hs_dev->dev, "Failed to allocate params structure\n");
			return -ENOMEM;
		}
		ret = copy_from_user(tdm_params, (const void __user *)arg, sizeof(struct hstdm_params));
		ret = configure_tdm_params(hs_dev, tdm_params);
		if (ret < 0) {
			dev_err(hs_dev->dev, "Failed to configure TDM parameters\n");
			ret = -EINVAL;
		}
		kfree(tdm_params);
		tdm_params = NULL;
	} else {
		dev_info(hs_dev->dev, "Setting PCM lane configuration\n");
		hs_dev->lane_config = arg;
		set_pcm_lane_config(hs_dev, arg);
	}

	return ret;
}

static int ioctl_handler5(struct hsi2s_device *hs_dev, unsigned int cmd, unsigned long arg)
{
	int ret = 0;

	if (hs_dev->client_count != 1) {
		dev_err(hs_dev->dev, "Mode already set by previous client\n");
		return -EINVAL;
	}

	if (cmd == LPAIF_INVERT_BIT_CLOCK) {
		if (hsi2s_core->target == 6155) {
			dev_err(hs_dev->dev, "Bit clock configuration is not supported by target\n");
			return -EINVAL;
		}
		ret = toggle_bit_clock(hs_dev);
	} else if (cmd == CONFIGURE_DAB_MRC) {
		if (hsi2s_core->target != 6155) {
			dev_err(hs_dev->dev, "DAB MRC configuration is not supported by target\n");
			return -EINVAL;
		}
		dev_info(hs_dev->dev, "Setting DAB MRC configuration\n");
		ret = configure_dab_mrc();
		if (ret < 0)
			dev_err(hs_dev->dev, "Failed to configure DAB MRC mode\n");
	} else {
		ret = -EINVAL;
	}

	return ret;
}

/* IOCTL handler */
static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct hsi2s_device *hs_dev;
	int ret = 0;

	hs_dev = (struct hsi2s_device *)file->private_data;

	if (cmd < LPAIF_SPEAKER)
		ret = ioctl_handler1(hs_dev, cmd, arg);
	else if (cmd < LPAIF_DEINIT_TX)
		ret = ioctl_handler2(hs_dev, cmd, arg);
	else if (cmd < I2S_CONFIG_PARAMS)
		ret = ioctl_handler3(hs_dev, cmd, arg);
	else if (cmd < LPAIF_INVERT_BIT_CLOCK)
		ret = ioctl_handler4(hs_dev, cmd, arg);
	else
		ret = ioctl_handler5(hs_dev, cmd, arg);

	return ret;
}

static __poll_t device_poll(struct file *file, poll_table *wait)
{
	struct hsi2s_device *hs_dev;
	__poll_t mask = 0;
	u32 reg;

	hs_dev = (struct hsi2s_device *)file->private_data;

	poll_wait(file, &hs_dev->wq_wrdma, wait);

	/* Check for periodic interrupt on write DMA channel */
	if (hs_dev->write_buffer->pollin) {
		hs_dev->write_buffer->pollin = 0;
		mask |= EPOLLIN | EPOLLRDNORM;
		dma_sync_sg_for_cpu(hsi2s_core->dev,
				    hs_dev->write_buffer->buffer->dma_sgt->sgl,
				    hs_dev->write_buffer->buffer->dma_sgt->orig_nents,
				    DMA_FROM_DEVICE);
		reg = readl_relaxed(hs_dev->wrdma_curr_addr);
		*((u32 *)(hsi2s_core->sh_mem) + ((hs_dev->minor_num * PAGE_SIZE) / BYTES_PER_SAMPLE) + SHM_WRDMA_CURRENT) = reg;
	}

	return mask;
}

static int device_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct hsi2s_device *hs_dev;
	unsigned long len = vma->vm_end - vma->vm_start;
	u32 reg;
	int ret = 0;
	unsigned long uaddr = vma->vm_start;
	int i = 0;

	hs_dev = (struct hsi2s_device *)file->private_data;

	if (len > dma_buffer_length) {
		dev_err(hs_dev->dev, "Size of map area exceeds DMA buffer length");
		ret = -EINVAL;
	} else {
		do {
			ret = vm_insert_page(vma, uaddr, hs_dev->write_buffer->buffer->pages[i++]);
			if (ret) {
				dev_err(hs_dev->dev, "Remapping memory, error: %d\n", ret);
				break;
			}
			uaddr += PAGE_SIZE;
			len -= PAGE_SIZE;
		} while (len > 0);
		reg = readl_relaxed(hs_dev->wrdma_base);
		*((u32 *)(hsi2s_core->sh_mem) + ((hs_dev->minor_num * PAGE_SIZE) / BYTES_PER_SAMPLE) + SHM_WRDMA_BASE) = reg;
	}

	return ret;
}

/* Called when a process attempts to open the core device file */
static int c_device_open(struct inode *inode, struct file *file)
{
	dev_err(hsi2s_core->dev, "Client connected");

	/* Increment usage count to be able to properly close the module. */
	try_module_get(THIS_MODULE);

	return 0;
}

/* Called when the a process closes the core device file */
static int c_device_release(struct inode *inode, struct file *file)
{
	dev_err(hsi2s_core->dev, "Client disconnected");

	/* Decrement usage count to be able to properly close the module. */
	module_put(THIS_MODULE);

	return 0;
}

/* Map the shared memory region for core device */
static int c_device_mmap(struct file *file, struct vm_area_struct *vma)
{
	int ret = 0;
	unsigned long pa;
	unsigned long pfn;
	unsigned long len = vma->vm_end - vma->vm_start;

	pa = virt_to_phys(hsi2s_core->sh_mem);
	pfn = (pa >> PAGE_SHIFT) + vma->vm_pgoff;

	if (len > SHM_SIZE) {
		dev_err(hsi2s_core->dev, "Size of map area(%d) exceeds shared memory size", len);
		ret = -EINVAL;
	} else {
		ret = remap_pfn_range(vma, vma->vm_start, pfn, len, vma->vm_page_prot);
		if (ret)
			dev_err(hsi2s_core->dev, "%s failed", __func__);
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

static const struct file_operations c_fops = {
	.open  = c_device_open,
	.release = c_device_release,
	.mmap = c_device_mmap,

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
	u32 pcm_rate = PCM_RATE_32_BIT_CLKS;
	u32 pcm_sync_src = PCM_SYNC_EXT;
	u32 aux_mode = PCM_AUXMODE_PCM;
	u32 rpcm_width = RPCM_WIDTH_16;
	u32 tpcm_width = TPCM_WIDTH_16;
	u32 en_tdm = 1;
	u32 tdm_sync_delay = DELAY_0_CYCLE;
	u32 tdm_inv_sync = 0;
	u32 tdm_rate = TDM_DEFAULT_RATE;
	u32 tdm_rpcm_width = TDM_DEFAULT_SLOT_SIZE;
	u32 tdm_tpcm_width = TDM_DEFAULT_SLOT_SIZE;
	u32 lane_config = MULTI_LANE_RX;
	int slot;

	/* Read the device minor number */
	ret = of_property_read_u32(dev->of_node, "minor-number",
				   &minor);
	if (ret) {
		dev_err(hsi2s_core->dev, "Resource 'minor-number' unavailable in dtsi");
		goto err_out;
	} else {
		dev_err(hsi2s_core->dev, "Device minor-number %d probed", minor);
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

	/* Set LPAIF mode */
	hs_dev->lpaif_mode = lpaif_mode;

	/* Configure the I2S parameters */

	/* Bit clock  */
	ret = of_property_read_u32(dev->of_node, "bit-clock-hz",
				   &bit_clk);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'bit-clock-hz' unavailable in dtsi");

	if (bit_clock_hz)
		bit_clk = bit_clock_hz;

	if (!bit_clk) {
		dev_err(hs_dev->dev, "Bit clock not configured. Exiting...");
		ret = -EINVAL;
		goto err_disable_intf_clock;
	}

	if (bit_clk > BIT_CLK_MAX) {
		dev_err(hs_dev->dev, "Bit clock rate exceeds maximum supported rate of 73.728MHz. Exiting...");
		ret = -EINVAL;
		goto err_disable_intf_clock;
	}

	/* Data buffer in ms */
	ret = of_property_read_u32(dev->of_node, "data-buffer-ms",
				   &buffer_ms);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'data-buffer-ms' unavailable in dtsi");

	if (data_buffer_ms)
		buffer_ms = data_buffer_ms;

	if (!buffer_ms) {
		dev_err(hs_dev->dev, "Data buffer in ms not configured. Exiting...");
		ret = -EINVAL;
		goto err_disable_intf_clock;
	}
	hs_dev->data_buffer_ms_val = buffer_ms;

	/* Set the periodic length */
	hs_dev->wrdma_periodic_length_bytes = set_periodic_length(bit_clk, buffer_ms);
	hs_dev->wrdma_periodic_length = hs_dev->wrdma_periodic_length_bytes / BYTES_PER_SAMPLE;
	dev_info(hs_dev->dev, "Periodic length configured as %u words", hs_dev->wrdma_periodic_length);

	/* Bit depth */
	ret = of_property_read_u32(dev->of_node, "bit-depth",
				  &b_depth);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'bit-depth' unavailable in dtsi");

	if (bit_depth)
		b_depth = bit_depth;

	configure_bit_depth(hs_dev, b_depth);

	/* Speaker channel count */
	ret = of_property_read_u32(dev->of_node, "spkr-channel-count",
				   &ch_count);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'spkr-channel-count' unavailable in dtsi");

	if (channel_count)
		ch_count = channel_count;

	configure_spkr_channel(hs_dev, ch_count);

	/* Mic channel count */
	ret = of_property_read_u32(dev->of_node, "mic-channel-count",
				   &ch_count);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'spkr-channel-count' unavailable in dtsi");

	if (channel_count)
		ch_count = channel_count;

	configure_mic_channel(hs_dev, ch_count);

	/* Configure PCM parameters */

	/* PCM rate */
	ret = of_property_read_u32(dev->of_node, "pcm-rate",
				   &pcm_rate);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'pcm-rate' unavailable in dtsi");

	configure_pcm_rate(hs_dev, (u8)pcm_rate);

	/* PCM sync source */
	ret = of_property_read_u32(dev->of_node, "pcm-sync-src",
				   &pcm_sync_src);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'pcm-sync-src' unavailable in dtsi");

	hs_dev->pcm_sync_src = (u8)pcm_sync_src;

	/* Aux mode */
	ret = of_property_read_u32(dev->of_node, "aux-mode",
				   &aux_mode);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'aux-mode' unavailable in dtsi");

	hs_dev->pcm_aux_mode = (u8)aux_mode;

	/* RPCM width */
	ret = of_property_read_u32(dev->of_node, "rpcm-width",
				   &rpcm_width);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'rpcm-width' unavailable in dtsi");

	hs_dev->pcm_rpcm_width = (u8)rpcm_width;

	/* TPCM width */
	ret = of_property_read_u32(dev->of_node, "tpcm-width",
				   &tpcm_width);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'tpcm-width' unavailable in dtsi");

	hs_dev->pcm_tpcm_width = (u8)tpcm_width;

	/* Configure TDM parameters */

	/* TDM enable flag */
	ret = of_property_read_u32(dev->of_node, "enable-tdm",
				   &en_tdm);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'enable-tdm' unavailable in dtsi");

	hs_dev->tdm_en = (u8)en_tdm;

	/* TDM rate */
	ret = of_property_read_u32(dev->of_node, "tdm-rate",
				   &tdm_rate);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'tdm-rate' unavailable in dtsi");

	if (tdm_rate <= TDM_MAX_RATE)
		hs_dev->tdm_rate = tdm_rate - 1;
	else
		hs_dev->tdm_rate = TDM_MAX_RATE - 1;

	configure_tdm_rate(hs_dev, hs_dev->tdm_rate);

	/* TDM RPCM width */
	ret = of_property_read_u32(dev->of_node, "tdm-rpcm-width",
				   &tdm_rpcm_width);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'tdm-rpcm-width' unavailable in dtsi");

	if (tdm_rpcm_width <= TDM_MAX_SLOT_SIZE)
		hs_dev->tdm_rpcm_width = (tdm_rpcm_width - 1) << 9;
	else
		hs_dev->tdm_rpcm_width = (TDM_MAX_SLOT_SIZE - 1) << 9;

	/* TDM TPCM width */
	ret = of_property_read_u32(dev->of_node, "tdm-tpcm-width",
				   &tdm_tpcm_width);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'tdm-tpcm-width' unavailable in dtsi");

	if (tdm_tpcm_width <= TDM_MAX_SLOT_SIZE)
		hs_dev->tdm_tpcm_width = (tdm_tpcm_width - 1) << 14;
	else
		hs_dev->tdm_tpcm_width = (TDM_MAX_SLOT_SIZE - 1) << 14;

	/* TDM sync delay */
	ret = of_property_read_u32(dev->of_node, "tdm-sync-delay",
				   &tdm_sync_delay);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'tdm-sync-delay' unavailable in dtsi");

	configure_tdm_sync_delay(hs_dev, (u8)tdm_sync_delay);

	/* TDM invert frame sync flag */
	ret = of_property_read_u32(dev->of_node, "tdm-inv-sync",
				   &tdm_inv_sync);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'tdm-inv-sync' unavailable in dtsi");

	hs_dev->tdm_inv_sync = (u8)tdm_inv_sync;

	/* Map the interface registers */
	ret = init_default(hs_dev, minor);
	if (ret < 0) {
		dev_err(hs_dev->dev, "Failed to set default settings for hsi2s device");
		goto err_deinit_default;
	}

	/* Configure the gpios */
	if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
		hs_dev->is_pinctrl_names = true;
		ret = hsi2s_configure_gpio_pins(pdev, 1);
		if (ret < 0) {
			dev_err(hs_dev->dev, "Failed to configure gpios");
			goto err_deinit_default;
		}
	}

	/* Configure the LPAIF mode register */
	configure_lpaif_mode(hs_dev, (u8)lpaif_mode);

	/* Enable PCM slots for Rx and Tx */
	for (slot = 0; slot < MAX_SLOTS; slot++) {
		configure_rpcm_slot(hs_dev, slot, 1);
		configure_tpcm_slot(hs_dev, slot, 1);
	}

	/* Set lane configuration for PCM */
	ret = of_property_read_u32(dev->of_node, "pcm-lane-config",
				   &lane_config);
	if (ret)
		dev_warn(hs_dev->dev, "Resource 'pcm-lane-config' unavailable in dtsi");

	hs_dev->lane_config = lane_config;
	set_pcm_lane_config(hs_dev, lane_config);

	/* Set read DMA flags */
	hs_dev->rddma_xfer_busy = 0;
	hs_dev->rddma_copy_busy = 1;
	hs_dev->rddma_in_progress = 0;

	/* Configure the operational mode */
	if (operation_mode) {
		/* Setting slave mode for SA8155/SA8195 targets */
		if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195)
			configure_muxmode(hs_dev, 1);
		configure_normal_mode(hs_dev, minor);
	} else {
		configure_int_loopback_mode(hs_dev, minor);
	}

	/* Create device file for the interface */
	hs_dev->cdev_sdr = kzalloc(sizeof(*hs_dev->cdev_sdr),
				   GFP_KERNEL);
	if (!hs_dev->cdev_sdr) {
		ret = -ENOMEM;
		goto err_deinit_default;
	}

	hs_dev->curr_devid = MKDEV(MAJOR(devid), MINOR(devid) + minor);
	cdev_init(hs_dev->cdev_sdr, &fops);
	hs_dev->cdev_sdr->owner = THIS_MODULE;
	ret = cdev_add(hs_dev->cdev_sdr, hs_dev->curr_devid, 1);
	if (ret < 0) {
		dev_err(hs_dev->dev, "Unable to add cdev for sdr%d interface"
		       , minor);
		goto err_free_cdev;
	}

	if (!minor)
		devname = SDR0;
	else if (minor == 1)
		devname = SDR1;
	else if (minor == 2)
		devname = SDR2;
	else if (minor == 3)
		devname = SDR3;
	else
		devname = SDR4;

	hs_dev->class_sdr = class_create(THIS_MODULE,
					 devname);
	if (!hs_dev->class_sdr) {
		dev_err(hs_dev->dev, "Failed to create device class %d"
		       , minor);
		ret = -EEXIST;
		goto err_delete_cdev;
	}
	if (!device_create(hs_dev->class_sdr, NULL, hs_dev->curr_devid,
			   NULL, "hs%d_i2s", minor)) {
		dev_err(hs_dev->dev, "Failed to create device file for sdr%d interface"
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
			if (hsi2s_core->desc) {
				if (hsi2s_core->desc->core_internal_state__do_not_mess_with_it & 0x00000200) {
					dev_info(hsi2s_core->dev, "Removing pending IRQs");
					hsi2s_core->desc->core_internal_state__do_not_mess_with_it &= ~(0x00000200);
				}
			} else {
				dev_warn(hsi2s_core->dev, "Unable to remove pending IRQs");
			}
			dev_info(hsi2s_core->dev, "Enabling IRQ line");
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
#if defined(CONFIG_QTI_GVM) || defined(CONFIG_QTI_QUIN_GVM)
	int handle;
	u32 resp_size = sizeof(msg_t);
#endif

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,hsi2s-interface"))
		return hsi2s_interface_probe(pdev);
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
	place_marker("M - DRIVER HS-I2S Init");
#else
	pr_info("M - DRIVER HS-I2S Init");
#endif

	hsi2s_core = kzalloc(sizeof(*hsi2s_core), GFP_KERNEL);
	if (!hsi2s_core)
		return -ENOMEM;

	/* Set the hsi2s_core structure as platform data */
	platform_set_drvdata(pdev, hsi2s_core);

	/* Set the device pointer */
	hsi2s_core->dev = &pdev->dev;

	/* Read the target property */
	if (of_device_is_compatible(pdev->dev.of_node, "qcom,sa6155-hsi2s"))
		target = 6155;
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,sa8155-hsi2s"))
		target = 8155;
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,sa8195-hsi2s"))
		target = 8195;
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,sa8295-hsi2s"))
		target = 8295;
	else if (of_device_is_compatible(pdev->dev.of_node, "qcom,sa8255-hsi2s"))
		target = 8255;
	else {
		dev_err(hsi2s_core->dev, "Uncompatible target");
		goto err_free_core;
	}

	/* Allocate target macro structure */
	hsi2s_core->macro = kzalloc(sizeof(struct hsi2s_macros), GFP_KERNEL);
	if (!hsi2s_core->macro)
		return -ENOMEM;

	/* Assign target specific macros */
	if (target == 6155) {
		dev_info(hsi2s_core->dev, "6155 target detected");
		t_assign_macros();
	} else if (target == 8155 || target == 8195) {
		dev_info(hsi2s_core->dev, "8155/8195 target detected");
		h_assign_macros();
	} else if (target == 8295) {
		dev_info(hsi2s_core->dev, "8295 target detected");
		m_assign_macros();
	}else if (target == 8255) {
		dev_info(hsi2s_core->dev, "8255 target detected");
		l_assign_macros();
	}

	hsi2s_core->target = target;

	/* Read the number of HS-I2S interfaces */
	ret = of_property_read_u32(dev->of_node, "number-of-interfaces",
				   &interface_count);
	if (ret) {
		dev_err(hsi2s_core->dev, "Resource 'number-of-interfaces' unavailable in dtsi");
		goto err_free_macro;
	}

	/* Store the interface count */
	hsi2s_core->i_count = interface_count;

	/* Set the write DMA buffer length */
	if (dma_buffer_length < 1 || dma_buffer_length > 4) {
		dev_warn(hsi2s_core->dev, "No valid buffer length entered. Setting 4MB default");
		dma_buffer_length = DEFAULT_BUFF_LEN_BYTES;
	} else {
		/* Converting into bytes */
		dma_buffer_length *= (1024 * 1024);
	}
	dev_info(hsi2s_core->dev, "Write DMA buffer length set to %u bytes", dma_buffer_length);

	dma_buffer_length_words = ((dma_buffer_length / 4) - 1);

	/* Register the character device numbers */
	ret = alloc_chrdev_region(&devid, 0, interface_count,
				  "hsi2s_dev");
	if (ret < 0) {
		dev_err(hsi2s_core->dev, "Major number allocation failed");
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
	else if (target == 8155 || target == 8195 || target == 8295 || target == 8255) {
		if (enable_qmi) {
#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM)
#if defined(CONFIG_QCOM_QMI_HELPERS)
			/* Allocate QMI handle */
			hsi2s_core->qmi_dev = kzalloc(sizeof(*hsi2s_core->qmi_dev), GFP_KERNEL);
			if (!hsi2s_core->qmi_dev) {
				dev_err(hsi2s_core->dev, "Failed to allocate QMI handle");
				ret = -ENOMEM;
				goto err_free_macro;
			}

			/* Initialize QMI wait queue */
			init_waitqueue_head(&hsi2s_core->wq_qmi);

			/* Initialize QMI connection flag */
			hsi2s_core->qmi_connection = false;

			ret = qmi_handle_init(hsi2s_core->qmi_dev,
					PROD_HSI2S_CLK_CTRL_REQ_MSG_V01_MAX_MSG_LEN,
					&hsi2s_qmi_adsp_ops, NULL);

			if (ret < 0) {
				dev_err(hsi2s_core->dev, "Failed to initialize the qmi_handle for the client");
				goto err_disable_core_clocks;
			}

			/* Register a new lookup with the service PGS_SERVICE_ID_V01 */
			ret = qmi_add_lookup(hsi2s_core->qmi_dev, HSI2S_SERVICE_ID_V01,
					HSI2S_SERVICE_VERS_V01, 0);

			if (ret < 0) {
				dev_err(hsi2s_core->dev, "Failed to add QMI lookup");
				goto err_disable_core_clocks;
			}

			/* Enable clocks */
			ret = hsi2s_adsp_enable_clks();
			if (ret < 0) {
				goto err_disable_core_clocks;
			}
#else
			dev_err(hsi2s_core->dev, "QMI kernel configuration is not enabled");
			if (target == 8295) {
				m_modify_core_clks(1);
				m_modify_interface_clks(1);
			} else if ((target == 8155) || (target == 8195)){
				h_modify_core_clks(1);
				h_modify_interface_clks(1);
			}
#endif
#else
			hsi2s_core->hab_req = kzalloc(sizeof(msg_t), GFP_KERNEL);
			if (!hsi2s_core->hab_req) {
				dev_err(hsi2s_core->dev, "Failed to allocate HAB request message");
				ret = -ENOMEM;
				goto err_free_macro;
			}
			hsi2s_core->hab_resp = kzalloc(sizeof(msg_t), GFP_KERNEL);
			if (!hsi2s_core->hab_resp) {
				dev_err(hsi2s_core->dev, "Failed to allocate HAB response message");
				ret = -ENOMEM;
				goto err_free_hab_req;
			}

			ret = habmm_socket_open(&handle, MM_HSI2S_1, 0, 0);
			if (ret) {
				pr_err("open habmm socket failed (%d)\n", ret);
				goto err_free_hab_resp;
			}
			hsi2s_core->hab_handle = handle;
			hsi2s_core->hab_req->clk_en = 1;

			ret = habmm_socket_send(handle, hsi2s_core->hab_req, resp_size, 0);
			if (ret) {
				dev_err(hsi2s_core->dev, "habmm socket send failed (%d)\n", ret);
				goto err_close_hab;
			}

			ret = habmm_socket_recv(handle, hsi2s_core->hab_resp, &resp_size, UINT_MAX, HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
			if (ret) {
				dev_err(hsi2s_core->dev, "habmm socket receive failed (%d)\n", ret);
				goto err_close_hab;
			}

			if (hsi2s_core->hab_resp->rsp) {
				dev_err(hsi2s_core->dev, "error response (%d)\n", hsi2s_core->hab_resp->rsp);
				ret = -EIO;
				goto err_close_hab;
			}
#endif
		} else {
			if (target == 8295) {
				m_modify_core_clks(1);
				m_modify_interface_clks(1);
			} else if ((target == 8155) || (target == 8195)){
				h_modify_core_clks(1);
				h_modify_interface_clks(1);
			}
		}
	}

	/* Map the register memory region */
	resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"lpa_if");
	if (!resource) {
		dev_err(hsi2s_core->dev, "get lpa_if resource failed");
		ret = -ENODEV;
		goto err_disable_core_clocks;
	}

	hsi2s_core->lpaif_base_va = devm_ioremap_resource(&pdev->dev,
								  resource);
	if (IS_ERR(hsi2s_core->lpaif_base_va)) {
		dev_err(hsi2s_core->dev, "ioremap failed");
		ret = PTR_ERR(hsi2s_core->lpaif_base_va);
		goto err_disable_core_clocks;
	}

	if (target == 8155 || target == 8195) {
		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"lpass_tcsr");
		if (!resource) {
			dev_err(hsi2s_core->dev, "get lpass_tcsr resource failed");
			ret = -ENODEV;
			goto err_iounmap_lpaif;
		}

		hsi2s_core->lpass_tcsr_base_va =
		devm_ioremap_resource(&pdev->dev, resource);

		if (IS_ERR(hsi2s_core->lpass_tcsr_base_va)) {
			dev_err(hsi2s_core->dev, "ioremap failed");
			ret = PTR_ERR(hsi2s_core->lpass_tcsr_base_va);
			goto err_iounmap_lpaif;
		}

	}

	if (target == 8295 || target == 8255) {
		resource = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						"lpass_core_cc_hs_if");
		if (!resource) {
			dev_err(hsi2s_core->dev, "get lpass_core_cc_hs_if resource failed");
			ret = -ENODEV;
			goto err_iounmap_lpaif;
		}

		hsi2s_core->lpass_core_cc_hs_if =
		devm_ioremap_resource(&pdev->dev, resource);

		if (IS_ERR(hsi2s_core->lpass_core_cc_hs_if)) {
			dev_err(hsi2s_core->dev, "ioremap failed");
			ret = PTR_ERR(hsi2s_core->lpass_core_cc_hs_if);
			goto err_iounmap_lpaif;
		}
	}

	/* Map the core registers */
	ret = map_core_registers();
	if (ret < 0) {
		dev_err(hsi2s_core->dev, "Unable to map core registers");
		goto err_iounmap_lpass_tcsr;
	}

#ifndef DISABLE_RATE_DETECTION
	ret = of_property_read_u32(dev->of_node, "number-of-rate-detectors",
				   &rate_detector_count);
	if (ret)
		dev_warn(hsi2s_core->dev, "Resource 'number-of-rate-detectors' unavailable in dtsi");
#else
	rate_detector_count = 0;
#endif

	if (rate_detector_count) {
		hsi2s_core->is_rate_enabled = true;
		switch (rate_detector_count) {
			case 1:
				ret = of_property_read_u32(dev->of_node, "rate-detector-interfaces", &hsi2s_core->pri_rate_interface);
				if (ret) {
					dev_warn(hsi2s_core->dev, "Resource 'rate-detector-interfaces' unavailable in dtsi");
					hsi2s_core->is_rate_enabled = false;
				} else if (hsi2s_core->pri_rate_interface >= interface_count) {
					dev_warn(hsi2s_core->dev, "Invalid minor number for the rate detector");
					hsi2s_core->is_rate_enabled = false;
				} else {
					/* Configure rate detection */
					configure_rate_detection(PRI_RATE_DET);
				}
				break;
			case 2:
				ret = of_property_read_u32_array(dev->of_node, "rate-detector-interfaces", rate_array, 2);
				if (ret) {
					dev_warn(hsi2s_core->dev, "Resource 'rate-detector-interfaces' unavailable in dtsi");
					hsi2s_core->is_rate_enabled = false;
				} else if (rate_array[0] >= interface_count && rate_array[1] >= interface_count) {
					dev_warn(hsi2s_core->dev, "Invalid minor numbers for the rate detector");
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
				dev_warn(hsi2s_core->dev, "Invalid rate detector count specified. Check device tree");
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
		dev_err(hsi2s_core->dev, "Err getting IRQ0");
		ret = hsi2s_core->irq0;
		goto err_iounmap_lpass_tcsr;
	}

	ret =
	devm_request_threaded_irq(&pdev->dev,
				  hsi2s_core->irq0,
				  i2s_interrupt_handler,
				  irq_thread_fn,
				  IRQF_TRIGGER_HIGH | IRQF_ONESHOT,
				  "lpaif_hs_out0_irq", hsi2s_core);
	if (ret) {
		dev_err(hsi2s_core->dev, "Request_irq failed:%d: err:%d\n",
		       hsi2s_core->irq0, ret);
		goto err_iounmap_lpass_tcsr;
	}

	/* Disable the irq line until child devices are probed */
	disable_irq_nosync(hsi2s_core->irq0);
	hsi2s_core->is_irq_enabled = false;

#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
	/* Configure SMMU */
	ret = hsi2s_smmu_init(pdev);
	if (ret) {
		dev_err(hsi2s_core->dev, "Failed to init smmu");
		goto err_free_irq;
	}
#endif

	/* Create shared memory for register info */
	hsi2s_core->sh_mem = kzalloc(SHM_SIZE, GFP_KERNEL | GFP_DMA);
	if (!hsi2s_core->sh_mem) {
		dev_err(hsi2s_core->dev, "Unable to allocate shared memory");
		ret = -ENOMEM;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
		goto err_free_smmu;
#else
		goto err_free_irq;
#endif
	}

	/* Create device file for register mapping */
	hsi2s_core->cdev_sdr = kzalloc(sizeof(*hsi2s_core->cdev_sdr),
				   GFP_KERNEL);
	if (!hsi2s_core->cdev_sdr) {
		dev_err(hsi2s_core->dev, "Unable to allocate cdev for core interface");
		ret = -ENOMEM;
		goto err_free_shm;
	}

	hsi2s_core->curr_devid = MKDEV(MAJOR(devid), MINOR(devid) + hsi2s_core->i_count);
	cdev_init(hsi2s_core->cdev_sdr, &c_fops);
	hsi2s_core->cdev_sdr->owner = THIS_MODULE;
	ret = cdev_add(hsi2s_core->cdev_sdr, hsi2s_core->curr_devid, 1);
	if (ret < 0) {
		dev_err(hsi2s_core->dev, "Unable to add cdev for core interface");
		goto err_free_cdev;
	}

	hsi2s_core->class_sdr = class_create(THIS_MODULE, "hsi2s_reginfo");
	if (!hsi2s_core->class_sdr) {
		dev_err(hsi2s_core->dev, "Failed to create device class");
		ret = -EEXIST;
		goto err_delete_cdev;
	}
	if (!device_create(hsi2s_core->class_sdr, NULL, hsi2s_core->curr_devid,
					   NULL, "hsi2s_reginfo")) {
		dev_err(hsi2s_core->dev, "Failed to create device file for core interface");
		ret = -EINVAL;
		goto err_class_destroy;
	}

	/* Configure the output routing gpio for 8195 */
	if (target == 8195) {
		if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
			dev_info(hsi2s_core->dev, "Configure output routing gpio during init");
			ret = hsi2s_configure_gpio_pins(pdev, 1);
			if (ret < 0) {
				dev_err(hsi2s_core->dev, "Failed to configure the output routing gpio during init");
			}
		}
	}

	/* Reset the mic enabler flag */
	hsi2s_core->en_mic = 0;

	/* Probe child devices */
	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret)
		dev_err(hsi2s_core->dev, "Failed to add child devices");
	else
		dev_info(hsi2s_core->dev, "Added child devices");
#if (LINUX_VERSION_CODE < KERNEL_VERSION(5, 19, 0))
	place_marker("M - DRIVER HS-I2S Ready");
#else
	pr_info("M - DRIVER HS-I2S Ready");
#endif

	return ret;

err_class_destroy:
	class_destroy(hsi2s_core->class_sdr);
err_delete_cdev:
	cdev_del(hsi2s_core->cdev_sdr);
err_free_cdev:
	if (hsi2s_core->cdev_sdr) {
		kfree(hsi2s_core->cdev_sdr);
		hsi2s_core->cdev_sdr = NULL;
	}
err_free_shm:
	if (hsi2s_core->sh_mem) {
		kfree(hsi2s_core->sh_mem);
		hsi2s_core->sh_mem = NULL;
	}
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
err_free_smmu:
	/* Detach and release iommu mapping */
	if (hsi2s_core->hsi2s_smmu_ctx) {
		if (hsi2s_core->hsi2s_smmu_ctx->valid) {
			if (hsi2s_core->hsi2s_smmu_ctx->smmu_pdev)
				arm_iommu_detach_device(&hsi2s_core->hsi2s_smmu_ctx->smmu_pdev->dev);
			if (hsi2s_core->hsi2s_smmu_ctx->mapping)
				arm_iommu_release_mapping(hsi2s_core->hsi2s_smmu_ctx->mapping);
			hsi2s_core->hsi2s_smmu_ctx->valid = false;
			hsi2s_core->hsi2s_smmu_ctx->mapping = NULL;
			hsi2s_core->hsi2s_smmu_ctx->pdev_master = NULL;
			hsi2s_core->hsi2s_smmu_ctx->smmu_pdev = NULL;
			dev_info(hsi2s_core->dev, "Detached and released iommu mapping");
		}
		kfree(hsi2s_core->hsi2s_smmu_ctx);
		hsi2s_core->hsi2s_smmu_ctx = NULL;
	}
#endif
err_free_irq:
	devm_free_irq(hsi2s_core->dev, hsi2s_core->irq0, hsi2s_core);
err_iounmap_lpass_tcsr:
	if (target == 8155 || target == 8195)
		iounmap(hsi2s_core->lpass_tcsr_base_va);
	else if (target == 8295 || target == 8255)
		iounmap(hsi2s_core->lpass_core_cc_hs_if);
err_iounmap_lpaif:
	iounmap(hsi2s_core->lpaif_base_va);
err_disable_core_clocks:
	if (target == 6155) {
		hsi2s_disable_core_clks(pdev);
	} else if (target == 8155 || target == 8195 || target == 8295 || target == 8255) {
		if (enable_qmi) {
#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM)
#if defined(CONFIG_QCOM_QMI_HELPERS)
			if (hsi2s_core->qmi_dev)
				kfree(hsi2s_core->qmi_dev);
#else
			if (target == 8295) {
				m_modify_core_clks(0);
				m_modify_interface_clks(0);
			} else if ((target == 8155) || (target == 8195)){
				h_modify_interface_clks(0);
				h_modify_core_clks(0);
			}
#endif
#else
err_close_hab:
			habmm_socket_close(handle);
			hsi2s_core->hab_handle = 0;
err_free_hab_resp:
			kfree(hsi2s_core->hab_req);
err_free_hab_req:
			kfree(hsi2s_core->hab_resp);
#endif
		} else {
			if (target == 8295) {
				m_modify_core_clks(0);
				m_modify_interface_clks(0);
			} else if ((target == 8155) || (target == 8195)){
				h_modify_interface_clks(0);
				h_modify_core_clks(0);
			}
		}
	}
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
	if (hs_dev) {
		/* Get minor number for the interface */
		minor = hs_dev->minor_num;

		/* Reset the interface registers */
		reset_registers(hs_dev);
		/* Remove the device file */
		device_destroy(hs_dev->class_sdr, hs_dev->curr_devid);
		class_destroy(hs_dev->class_sdr);
		cdev_del(hs_dev->cdev_sdr);
		kfree(hs_dev->cdev_sdr);
		hs_dev->cdev_sdr = NULL;
		/* Disable the interface clocks */
		if (hsi2s_core->target == 6155)
			hsi2s_disable_intf_clks(pdev);
		/* Free the allocated buffers and device data structures */
		hsi2s_buffer_free(hs_dev);
		kfree(hs_dev);
		hs_dev = NULL;
		hsi2s_core->hsi2s_arr[minor] = NULL;
	} else {
		dev_warn(hsi2s_core->dev, "Platform driver data is NULL");
	}

	mutex_unlock(&hsi2s_core->irqlock);

	dev_info(hsi2s_core->dev, "Child device removed");

	return 0;
}

/* Function to release all resources from driver */
static int hsi2s_remove(struct platform_device *pdev)
{
	struct hsi2s_core *hs_core;
	int ret = 0;
#if defined(CONFIG_QTI_GVM) || defined(CONFIG_QTI_QUIN_GVM)
	u32 resp_size = sizeof(msg_t);
#endif

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,hsi2s-interface"))
		return hsi2s_interface_remove(pdev);

	/* Remove the child devices */
	of_platform_depopulate(&pdev->dev);
	/* Remove the core device */
	hs_core = (struct hsi2s_core *)platform_get_drvdata(pdev);
	/* Reset the output routing gpio for 8195 */
	if (hs_core->target == 8195) {
		if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
			dev_info(hs_core->dev, "Reset output routing gpio");
			ret = hsi2s_configure_gpio_pins(pdev, 0);
			if (ret < 0) {
				dev_err(hs_core->dev, "Failed to reset the output routing gpio");
			}
		}
	}
	/* Remove the device file */
	device_destroy(hs_core->class_sdr, hs_core->curr_devid);
	class_destroy(hs_core->class_sdr);
	cdev_del(hs_core->cdev_sdr);
	kfree(hs_core->cdev_sdr);
	hs_core->cdev_sdr = NULL;
	kfree(hs_core->sh_mem);
	hs_core->sh_mem = NULL;
#if LINUX_VERSION_CODE <= KERNEL_VERSION(4,15,1)
	/* Detach and release iommu mapping */
	if (hs_core->hsi2s_smmu_ctx) {
		if (hs_core->hsi2s_smmu_ctx->valid) {
			if (hs_core->hsi2s_smmu_ctx->smmu_pdev)
				arm_iommu_detach_device(&hs_core->hsi2s_smmu_ctx->smmu_pdev->dev);
			if (hs_core->hsi2s_smmu_ctx->mapping)
				arm_iommu_release_mapping(hs_core->hsi2s_smmu_ctx->mapping);
			hs_core->hsi2s_smmu_ctx->valid = false;
			hs_core->hsi2s_smmu_ctx->mapping = NULL;
			hs_core->hsi2s_smmu_ctx->pdev_master = NULL;
			hs_core->hsi2s_smmu_ctx->smmu_pdev = NULL;
			dev_info(hs_core->dev, "Detached and released iommu mapping");
		}
		kfree(hs_core->hsi2s_smmu_ctx);
		hs_core->hsi2s_smmu_ctx = NULL;
	}
#endif
	/* Free IRQ */
	devm_free_irq(&pdev->dev, hs_core->irq0, hs_core);
	/* Reset rate detection block */
	if (hs_core->is_rate_enabled) {
		reset_rate_detection(PRI_RATE_DET);
		reset_rate_detection(SEC_RATE_DET);
	}
	/* Disable the core clocks */
	if (hs_core->target == 6155)
		hsi2s_disable_core_clks(pdev);
	else if (hs_core->target == 8155 || hs_core->target == 8195 || hs_core->target == 8295) {
		if (enable_qmi) {
#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM)
#if defined(CONFIG_QCOM_QMI_HELPERS)
			if (hs_core->qmi_dev) {
				hsi2s_adsp_disable_clks();
				qmi_handle_release(hs_core->qmi_dev);
				kfree(hs_core->qmi_dev);
			}
#else
			if (hs_core->target == 8295) {
				m_modify_core_clks(0);
				m_modify_interface_clks(0);
			} else if ((target == 8155) || (target == 8195)){
				h_modify_interface_clks(0);
				h_modify_core_clks(0);
			}
#endif
#else
#if defined(CONFIG_MSM_HAB)
			hs_core->hab_req->clk_en = 0;

			ret = habmm_socket_send(hs_core->hab_handle, hs_core->hab_req, resp_size, 0);
			if (ret) {
				dev_err(hs_core->dev, "habmm socket send failed (%d)\n", ret);
				goto err_close_hab;
			}

			ret = habmm_socket_recv(hs_core->hab_handle, hs_core->hab_resp, &resp_size, UINT_MAX, HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
			if (ret) {
				dev_err(hs_core->dev, "habmm socket receive failed (%d)\n", ret);
				goto err_close_hab;
			}

			if (hs_core->hab_resp->rsp) {
				dev_err(hs_core->dev, "error response (%d)\n", hs_core->hab_resp->rsp);
				ret = -EIO;
				goto err_close_hab;
			}

err_close_hab:
			habmm_socket_close(hs_core->hab_handle);
			hs_core->hab_handle = 0;
			kfree(hs_core->hab_req);
			kfree(hs_core->hab_resp);
#endif
#endif
		} else {
			if (hs_core->target == 8295) {
				m_modify_core_clks(0);
				m_modify_interface_clks(0);
			} else if ((hs_core->target == 8155) || (hs_core->target == 8195)){
				h_modify_interface_clks(0);
				h_modify_core_clks(0);
			}
		}
	}
	/* Unregister the device numbers */
	unregister_chrdev_region(devid, hsi2s_core->i_count);
	/* Free the core data structure */
	kfree(hs_core->hsi2s_arr);
	hs_core->hsi2s_arr = NULL;
	kfree(hs_core->macro);
	hs_core->macro = NULL;
	kfree(hs_core);
	hs_core = NULL;
	hsi2s_core = NULL;

	pr_info("[hsi2s] Core device removed");

	return 0;
}

/* Function to put the device in suspend mode */
static int hsi2s_suspend(struct platform_device *pdev, pm_message_t state)
{
	int ret = 0;
	//u32 target;
#if ((defined(CONFIG_QTI_GVM) || defined(CONFIG_QTI_QUIN_GVM)) && defined(CONFIG_MSM_HAB))
	u32 resp_size = sizeof(msg_t);
#endif

	if (of_device_is_compatible(pdev->dev.of_node,
				    "qcom,hsi2s-interface")) {
		/* Set the GPIOs in sleep state */
		ret = hsi2s_configure_gpio_pins(pdev, 0);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to set gpios in sleep state");
			goto err_suspend;
		}
		/* Suspend the interface clocks */
		if (hsi2s_core->target == 6155)
			hsi2s_suspend_intf_clks(pdev);
	} else {
		/* Suspend the core clocks */
		if (hsi2s_core->target == 6155)
			hsi2s_suspend_core_clks(pdev);
		else if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195) {
			/* Reset the output routing gpio for 8195 */
			if (hsi2s_core->target == 8195) {
				if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
					dev_info(hsi2s_core->dev, "Reset output routing gpio during suspend");
					ret = hsi2s_configure_gpio_pins(pdev, 0);
					if (ret < 0) {
						dev_err(hsi2s_core->dev, "Failed to reset the output routing gpio during suspend");
					}
				}
			}
			if((hsi2s_core->disable_adsp_clk_flg)){
				if (enable_qmi) {
#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM)
#if defined(CONFIG_QCOM_QMI_HELPERS)
				if (hsi2s_core->qmi_dev) {
					ret = hsi2s_adsp_disable_clks();
					if (ret < 0) {
						dev_err(&pdev->dev, "Failed to suspend core clocks");
						goto err_suspend;
					}
				}
#else
			h_modify_interface_clks(0);
			h_modify_core_clks(0);
#endif
#else
#if defined(CONFIG_MSM_HAB)
			hsi2s_core->hab_req->clk_en = 0;

				ret = habmm_socket_send(hsi2s_core->hab_handle, hsi2s_core->hab_req, resp_size, 0);
				if (ret) {
					dev_err(hsi2s_core->dev, "habmm socket send failed (%d)", ret);
					goto err_suspend;
				}

				ret = habmm_socket_recv(hsi2s_core->hab_handle, hsi2s_core->hab_resp, &resp_size,
										UINT_MAX, HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
				if (ret) {
					dev_err(hsi2s_core->dev, "habmm socket receive failed (%d)", ret);
					goto err_suspend;
				}

				if (hsi2s_core->hab_resp->rsp) {
					dev_err(hsi2s_core->dev, "error response (%d)", hsi2s_core->hab_resp->rsp);
					ret = -EIO;
					goto err_suspend;
				}
#endif
#endif
			} else {
				h_modify_interface_clks(0);
				h_modify_core_clks(0);
				}
			}
		}
	}
	dev_info(&pdev->dev, "Device entering suspend state");
	return 0;

err_suspend:
	dev_err(hsi2s_core->dev, "Failed to suspend HS-I2S device");
	return ret;
}

/* Function to resume the device from suspend */
static int hsi2s_resume(struct platform_device *pdev)
{
	int ret = 0;
	struct hsi2s_device *hs_dev = hsi2s_core->hsi2s_arr[0];
	int i;
#if defined(CONFIG_QTI_GVM) || defined(CONFIG_QTI_QUIN_GVM)
	u32 resp_size = sizeof(msg_t);
#endif

	if (of_device_is_compatible(pdev->dev.of_node,
				    "qcom,hsi2s-interface")) {
		/* Resume the interface clocks */
		if (hsi2s_core->target == 6155) {
			ret = hsi2s_resume_intf_clks(pdev);
			if (ret) {
				dev_err(&pdev->dev, "Failed to resume interface clocks");
				return ret;
			}
		}
		/* Set the GPIOs in sleep state */
		ret = hsi2s_configure_gpio_pins(pdev, 1);
		if (ret < 0) {
			dev_err(&pdev->dev, "Failed to set gpios in active state");
			goto err_resume;
		}
	} else {
		/* Resume the core clocks */
		if (hsi2s_core->target == 6155) {
			ret = hsi2s_resume_core_clks(pdev);
			if (ret) {
				dev_err(&pdev->dev, "Failed to resume core clocks");
				return ret;
			}
		} else if (hsi2s_core->target == 8155 || hsi2s_core->target == 8195) {
			if(hsi2s_core->enable_adsp_clk_flg){
			if (enable_qmi) {
#if !defined(CONFIG_QTI_GVM) && !defined(CONFIG_QTI_QUIN_GVM)
#if defined(CONFIG_QCOM_QMI_HELPERS)
				if (hsi2s_core->qmi_dev) {
					ret = hsi2s_adsp_enable_clks();
					if (ret < 0) {
						dev_err(&pdev->dev, "Failed to resume core clocks");
						goto err_resume;
					}
				}
#else
				h_modify_interface_clks(1);
				h_modify_core_clks(1);
#endif
#else
#if defined(CONFIG_MSM_HAB)
				hsi2s_core->hab_req->clk_en = 1;

				ret = habmm_socket_send(hsi2s_core->hab_handle, hsi2s_core->hab_req, resp_size, 0);
				if (ret) {
					dev_err(hsi2s_core->dev, "habmm socket send failed (%d)", ret);
					goto err_resume;
				}

				ret = habmm_socket_recv(hsi2s_core->hab_handle, hsi2s_core->hab_resp, &resp_size,
										UINT_MAX, HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
				if (ret) {
					dev_err(hsi2s_core->dev, "habmm socket receive failed (%d)", ret);
					goto err_resume;
				}

				if (hsi2s_core->hab_resp->rsp) {
					dev_err(hsi2s_core->dev, "error response (%d)", hsi2s_core->hab_resp->rsp);
					ret = -EIO;
					goto err_resume;
				}
#endif
#endif
			} else {
				h_modify_interface_clks(1);
				h_modify_core_clks(1);
			}
			}
			/* Configure the output routing gpio for 8195 */
			if (hsi2s_core->target == 8195) {
				if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
					dev_info(hsi2s_core->dev, "Configure output routing gpio during resume");
					ret = hsi2s_configure_gpio_pins(pdev, 1);
					if (ret < 0) {
						dev_err(hsi2s_core->dev, "Failed to configure the output routing gpio during resume");
					}
				}
			}

			if (hsi2s_core->suspend_to_disk_trigger_flg) {
			for(i=0;i<3;i++){
				hs_dev = hsi2s_core->hsi2s_arr[i];
				/*LPAIF Reset sequence*/
				dev_info(hs_dev->dev, "Resetting muxmode register\n");
				reg_clear(hs_dev->lpaif_muxmode);
				if (hs_dev->lpaif_mode == HS_I2S) {
					dev_info(hs_dev->dev, "Resetting I2S control register\n");
					reg_clear(hs_dev->i2s_ctl);
					dev_info(hs_dev->dev, "Resetting DMA registers\n");
					reset_rddma_registers(hs_dev);
					reset_wrdma_registers(hs_dev);
					/* Clear IRQs */
					clear_irqs();
					/* Reset buffer pointers */
					hs_dev->read_buffer->last_copy = 1;
					hs_dev->read_buffer->last_xfer = 1;
					hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
					hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
					hs_dev->write_buffer->data_ready = 0;
					hs_dev->write_buffer->pollin = 0;
					hs_dev->rddma_xfer_busy = 0;
					hs_dev->rddma_copy_busy = 1;
					hs_dev->rddma_in_progress = 0;
				} else {
					dev_info(hs_dev->dev, "Resetting PCM control registers\n");
					reg_clear(hs_dev->pcm_ctl);
					reg_clear(hs_dev->tdm_ctl);
					reg_clear(hs_dev->tdm_sample_width);
					dev_info(hs_dev->dev, "Resetting DMA registers\n");
					reset_rddma_registers(hs_dev);
					reset_wrdma_registers(hs_dev);
					/* Clear IRQs */
					clear_irqs();
					/* Reset buffer pointers */
					hs_dev->read_buffer->last_copy = 1;
					hs_dev->read_buffer->last_xfer = 1;
					hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
					hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
					hs_dev->write_buffer->data_ready = 0;
					hs_dev->write_buffer->pollin = 0;
					hs_dev->rddma_xfer_busy = 0;
					hs_dev->rddma_copy_busy = 1;
					hs_dev->rddma_in_progress = 0;
				}
				/*Normal Rx sequence*/
				configure_muxmode(hs_dev, 1);
				configure_normal_mode(hs_dev, i);
			}
			hsi2s_core->suspend_to_disk_trigger_flg= false;
			}
		}
	}

	dev_info(&pdev->dev, "Device resuming from suspend state");
	return ret;

err_resume:
	dev_err(hsi2s_core->dev, "Failed to resume HS-I2S device");
	return ret;
}



static int hsi2s_pm_suspend(struct device *dev)
{
	int ret =0;
	pm_message_t message;
	struct platform_device *pdev ;
	pdev = to_platform_device(dev);
	hsi2s_core->disable_adsp_clk_flg = true;
	message.event = PM_EVENT_SUSPEND;
	ret = hsi2s_suspend(pdev,message);
	return ret;
}
static int hsi2s_pm_resume(struct device *dev)
{
	int ret =0;
	struct platform_device *pdev = to_platform_device(dev);
	hsi2s_core->enable_adsp_clk_flg = true;
	ret = hsi2s_resume(pdev);
	return ret;
}
static int hsi2s_pm_freeze(struct device *dev)
{
	int ret =0;
	pm_message_t message;
	struct platform_device *pdev ;
	hsi2s_core->disable_adsp_clk_flg = false;
	hsi2s_core->suspend_to_disk_trigger_flg= true;
	pdev = to_platform_device(dev);
	message.event = PM_EVENT_FREEZE;
	ret = hsi2s_suspend(pdev,message);
	return ret;
}
static int hsi2s_pm_thaw(struct device *dev)
{
	int ret =0;
	return ret;
}
static int hsi2s_pm_restore(struct device *dev)
{
	int ret =0;
	struct platform_device *pdev = to_platform_device(dev);
	hsi2s_core->enable_adsp_clk_flg = false;
	ret = hsi2s_resume(pdev);
	return ret;
}
static const struct dev_pm_ops hsi2s_pm_ops = {
	.suspend = hsi2s_pm_suspend,
	.freeze = hsi2s_pm_freeze,
	.thaw = hsi2s_pm_thaw,
	.restore = hsi2s_pm_restore,
	.resume = hsi2s_pm_resume,
};

static struct platform_driver hsi2s_driver = {
	.probe = hsi2s_probe,
	.remove = hsi2s_remove,
	.driver = {
		.name = "hsi2s_device",
		.of_match_table = hsi2s_idtable,
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &hsi2s_pm_ops,
#endif
	},
};

static int __init hsi2s_init_module(void)
{
	int ret = 0;

	ret = platform_driver_register(&hsi2s_driver);
	if (ret < 0) {
		pr_err("[hsi2s] Driver registration failed");
		return ret;
	}

	return ret;
}

static void __exit hsi2s_exit_module(void)
{
	platform_driver_unregister(&hsi2s_driver);

	pr_info("[hsi2s] Driver removed");
}

module_init(hsi2s_init_module);
module_exit(hsi2s_exit_module);

MODULE_DESCRIPTION("HSI2S Driver");
MODULE_LICENSE("GPL v2");
