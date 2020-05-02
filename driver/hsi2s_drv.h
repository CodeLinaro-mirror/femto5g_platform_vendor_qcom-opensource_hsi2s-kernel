/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#ifndef _HSI2S_DRV_H_
#define _HSI2S_DRV_H_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#include <linux/wait.h>
#include <linux/ioctl.h>
#include <linux/clk.h>
#include <linux/dma-mapping.h>
#include <linux/iommu.h>
#include <linux/irqdesc.h>
#include <linux/io.h>
#include <linux/poll.h>
#include <linux/soc/qcom/qmi.h>
#include <asm/dma-iommu.h>

/* Register offsets */
#define T_LPAIF_I2S_CTL				0x1000
#define T_LPAIF_PCM_I2S_SEL			0x1200
#define T_LPAIF_PCM_CTL				0x1500
#define T_LPAIF_PCM_TDM_CTL			0x1518
#define T_LPAIF_PCM_TDM_SAMPLE_WIDTH		0x151C
#define T_LPAIF_PCM_RPCM_SLOT_NUM		0x1520
#define T_LPAIF_PCM_TPCM_SLOT_NUM		0x1524
#define T_LPAIF_PCM_LANE_CONFIG			0x1528
#define T_LPAIF_IRQ_EN				0x9000
#define T_LPAIF_IRQ_STAT			0x9004
#define T_LPAIF_IRQ_CLEAR			0x900C
#define T_LPAIF_RDDMA_CTL			0xC000
#define T_LPAIF_RDDMA_BASE			0xC004
#define T_LPAIF_RDDMA_BUFF_LEN			0xC008
#define T_LPAIF_RDDMA_CURR_ADDR			0xC00C
#define T_LPAIF_RDDMA_PER_LEN			0xC010
#define T_LPAIF_WRDMA_CTL			0x18000
#define T_LPAIF_WRDMA_BASE			0x18004
#define T_LPAIF_WRDMA_BUFF_LEN			0x18008
#define T_LPAIF_WRDMA_CURR_ADDR			0x1800C
#define T_LPAIF_WRDMA_PER_LEN			0x18010
#define T_LPAIF_PRI_RATE_DET_CONFIG		0x22000
#define T_LPAIF_PRI_RATE_DET_TARGET1_CONFIG	0x22004
#define T_LPAIF_PRI_RATE_DET_TARGET2_CONFIG	0x22008
#define T_LPAIF_PRI_RATE_BIN			0x2200C
#define T_LPAIF_PRI_STC_DIFF			0x22010
#define T_LPAIF_PRI_RATE_DET_SEL		0x22014
#define T_LPAIF_SEC_RATE_DET_CONFIG		0x23000
#define T_LPAIF_SEC_RATE_DET_TARGET1_CONFIG	0x23004
#define T_LPAIF_SEC_RATE_DET_TARGET2_CONFIG	0x23008
#define T_LPAIF_SEC_RATE_BIN			0x2300C
#define T_LPAIF_SEC_STC_DIFF			0x23010
#define T_LPAIF_SEC_RATE_DET_SEL		0x23014

#define H_LPAIF_I2S_CTL				0x1000
#define H_LPAIF_PCM_CTL				0x1500
#define H_LPAIF_PCM_TDM_CTL			0x1518
#define H_LPAIF_PCM_TDM_SAMPLE_WIDTH		0x151C
#define H_LPAIF_PCM_RPCM_SLOT_NUM		0x1520
#define H_LPAIF_PCM_TPCM_SLOT_NUM		0x1524
#define H_LPAIF_PCM_LANE_CONFIG			0x1528
#define H_LPAIF_IRQ_EN				0xA000
#define H_LPAIF_IRQ_STAT			0xA004
#define H_LPAIF_IRQ_CLEAR			0xA00C
#define H_LPAIF_RDDMA_CTL			0xD000
#define H_LPAIF_RDDMA_BASE			0xD004
#define H_LPAIF_RDDMA_BUFF_LEN			0xD008
#define H_LPAIF_RDDMA_CURR_ADDR			0xD00C
#define H_LPAIF_RDDMA_PER_LEN			0xD010
#define H_LPAIF_WRDMA_CTL			0x13000
#define H_LPAIF_WRDMA_BASE			0x13004
#define H_LPAIF_WRDMA_BUFF_LEN			0x13008
#define H_LPAIF_WRDMA_CURR_ADDR			0x1300C
#define H_LPAIF_WRDMA_PER_LEN			0x13010
#define H_LPAIF_PRI_RATE_DET_CONFIG		0x19000
#define H_LPAIF_PRI_RATE_DET_TARGET1_CONFIG	0x19004
#define H_LPAIF_PRI_RATE_DET_TARGET2_CONFIG	0x19008
#define H_LPAIF_PRI_RATE_BIN			0x1900C
#define H_LPAIF_PRI_STC_DIFF			0x19010
#define H_LPAIF_SEC_RATE_DET_CONFIG		0x1A000
#define H_LPAIF_SEC_RATE_DET_TARGET1_CONFIG	0x1A004
#define H_LPAIF_SEC_RATE_DET_TARGET2_CONFIG	0x1A008
#define H_LPAIF_SEC_RATE_BIN			0x1A00C
#define H_LPAIF_SEC_STC_DIFF			0x1A010
#define H_LPAIF_PCM_I2S_SEL			0x1B000
#define H_LPAIF_MUXMODE				0xB000

/* Bits for I2S control register */
#define T_I2S_WS_SRC				BIT(2)
#define T_I2S_MIC_EN				BIT(9)
#define T_I2S_SPKR_EN				BIT(16)
#define T_I2S_LOOPBACK				BIT(17)
#define T_I2S_EN_LONG_RATE			BIT(24)
#define T_I2S_RESET				BIT(31)

#define H_I2S_WS_SRC				BIT(2)
#define H_I2S_MIC_EN				BIT(8)
#define H_I2S_SPKR_EN				BIT(14)
#define H_I2S_LOOPBACK				BIT(15)
#define H_I2S_EN_LONG_RATE			BIT(22)
#define H_I2S_RESET				BIT(31)

/* Bits for PCM control register */
#define T_TPCM_WIDTH				BIT(10)
#define T_RPCM_WIDTH				BIT(11)
#define T_AUX_MODE				BIT(12)
#define T_SYNC_SRC				BIT(13)
#define T_PCM_LOOPBACK				BIT(14)
#define T_CTRL_DATA_OE				BIT(18)
#define T_ONE_SLOT_SYNC_EN			BIT(19)
#define T_PCM_ENABLE				BIT(24)
#define T_PCM_ENABLE_TX				BIT(25)
#define T_PCM_ENABLE_RX				BIT(26)
#define T_PCM_RESET_TX				BIT(27)
#define T_PCM_RESET_RX				BIT(28)
#define T_PCM_RESET				BIT(31)

#define H_TPCM_WIDTH				BIT(10)
#define H_RPCM_WIDTH				BIT(11)
#define H_AUX_MODE				BIT(12)
#define H_SYNC_SRC				BIT(13)
#define H_PCM_LOOPBACK				BIT(14)
#define H_CTRL_DATA_OE				BIT(18)
#define H_ONE_SLOT_SYNC_EN			BIT(19)
#define H_PCM_ENABLE				BIT(24)
#define H_PCM_ENABLE_TX				BIT(25)
#define H_PCM_ENABLE_RX				BIT(26)
#define H_PCM_RESET_TX				BIT(27)
#define H_PCM_RESET_RX				BIT(28)
#define H_PCM_RESET				BIT(31)

/* Bits for TDM control register */
#define T_TDM_INV_RPCM_SYNC			BIT(27)
#define T_TDM_INV_TPCM_SYNC			BIT(28)
#define T_EN_DIFF_SAMPLE_WIDTH			BIT(29)
#define T_EN_TDM				BIT(30)

#define H_TDM_INV_RPCM_SYNC			BIT(27)
#define H_TDM_INV_TPCM_SYNC			BIT(28)
#define H_EN_DIFF_SAMPLE_WIDTH			BIT(29)
#define H_EN_TDM				BIT(30)

/* Bits for PCM lane configuration register */
#define T_LANE0_DIR				BIT(0)
#define T_LANE1_DIR				BIT(1)
#define T_LANE2_DIR				BIT(2)
#define T_LANE3_DIR				BIT(3)
#define T_LANE0_EN				BIT(16)
#define T_LANE1_EN				BIT(17)
#define T_LANE2_EN				BIT(18)
#define T_LANE3_EN				BIT(19)

#define H_LANE0_DIR				BIT(0)
#define H_LANE1_DIR				BIT(1)
#define H_LANE2_DIR				BIT(2)
#define H_LANE3_DIR				BIT(3)
#define H_LANE0_EN				BIT(16)
#define H_LANE1_EN				BIT(17)
#define H_LANE2_EN				BIT(18)
#define H_LANE3_EN				BIT(19)

/* Bits for I2S select register */
#define T_I2S_SEL				BIT(0)
#define H_I2S_SEL				BIT(0)

/* Bits for read DMA control register */
#define T_RDDMA_EN				BIT(0)
#define T_RDDMA_BURST_EN			BIT(20)
#define T_RDDMA_DYN_CLK				BIT(21)
#define T_RDDMA_RESET				BIT(31)

#define H_RDDMA_EN				BIT(0)
#define H_RDDMA_BURST_EN			BIT(17)
#define H_RDDMA_DYN_CLK				BIT(18)
#define H_RDDMA_RESET				BIT(31)

/* Bits for write DMA control register */
#define T_WRDMA_EN				BIT(0)
#define T_WRDMA_BURST_EN			BIT(21)
#define T_WRDMA_DYN_CLK				BIT(22)
#define T_WRDMA_RESET				BIT(31)

#define H_WRDMA_EN				BIT(0)
#define H_WRDMA_BURST_EN			BIT(19)
#define H_WRDMA_DYN_CLK				BIT(20)
#define H_WRDMA_RESET				BIT(31)

/* Bits for rate detection */
#define T_RATE_DET_EN				BIT(0)
#define T_RATE_DET_RESET			BIT(31)

#define H_RATE_DET_EN				BIT(0)
#define H_RATE_DET_RESET			BIT(31)

/* Bits for interrupt register */
#define IRQ_PER_RDDMA_CH0			BIT(0)
#define IRQ_UNDR_RDDMA_CH0			BIT(1)
#define IRQ_ERR_RDDMA_CH0			BIT(2)
#define IRQ_PER_RDDMA_CH1			BIT(3)
#define IRQ_UNDR_RDDMA_CH1			BIT(4)
#define IRQ_ERR_RDDMA_CH1			BIT(5)
#define IRQ_PER_RDDMA_CH2			BIT(6)
#define IRQ_UNDR_RDDMA_CH2			BIT(7)
#define IRQ_ERR_RDDMA_CH2			BIT(8)
#define IRQ_PER_RDDMA_CH3			BIT(9)
#define IRQ_UNDR_RDDMA_CH3			BIT(10)
#define IRQ_ERR_RDDMA_CH3			BIT(11)
#define IRQ_PER_RDDMA_CH4			BIT(12)
#define IRQ_UNDR_RDDMA_CH4			BIT(13)
#define IRQ_ERR_RDDMA_CH4			BIT(14)
#define IRQ_PER_WRDMA_CH0			BIT(15)
#define IRQ_OVR_WRDMA_CH0			BIT(16)
#define IRQ_ERR_WRDMA_CH0			BIT(17)
#define IRQ_PER_WRDMA_CH1			BIT(18)
#define IRQ_OVR_WRDMA_CH1			BIT(19)
#define IRQ_ERR_WRDMA_CH1			BIT(20)
#define IRQ_PER_WRDMA_CH2			BIT(21)
#define IRQ_OVR_WRDMA_CH2			BIT(22)
#define IRQ_ERR_WRDMA_CH2			BIT(23)
#define IRQ_PER_WRDMA_CH3			BIT(24)
#define IRQ_OVR_WRDMA_CH3			BIT(25)
#define IRQ_ERR_WRDMA_CH3			BIT(26)
#define IRQ_FRM_REF				BIT(27)
#define IRQ_PRI_RD_DIFF_RATE			BIT(28)
#define IRQ_PRI_RD_NO_RATE			BIT(29)
#define IRQ_SEC_RD_DIFF_RATE			BIT(30)
#define IRQ_SEC_RD_NO_RATE			BIT(31)

/* Bits for clock invert register */
#define INV_INT_CLK				BIT(0)
#define INV_EXT_CLK				BIT(1)

/* Additional macros */
#define DEVICE_NAME "hsi2s_driver"
#define SDR0 "hs0_i2s"
#define SDR1 "hs1_i2s"
#define SDR2 "hs2_i2s"
#define HS_I2S 0
#define HS_PCM 1
#define HS0_I2S 0
#define HS1_I2S 1
#define HS2_I2S 2
#define PCM_RATE_8_BIT_CLKS 0
#define PCM_RATE_16_BIT_CLKS 1
#define PCM_RATE_32_BIT_CLKS 2
#define PCM_RATE_64_BIT_CLKS 3
#define PCM_RATE_128_BIT_CLKS 4
#define PCM_RATE_256_BIT_CLKS 5
#define PCM_SYNC_EXT 0
#define PCM_SYNC_INT 1
#define PCM_AUXMODE_PCM 0
#define PCM_AUXMODE_AUX 1
#define RPCM_WIDTH_8 0
#define RPCM_WIDTH_16 1
#define TPCM_WIDTH_8 0
#define TPCM_WIDTH_16 1
#define TDM_DEFAULT_RATE 32
#define TDM_DEFAULT_SLOT_SIZE 16
#define TDM_MAX_RATE 512
#define TDM_MAX_SLOT_SIZE 32
#define LANE0 0
#define LANE1 1
#define LANE2 2
#define LANE3 3
#define DELAY_2_CYCLE 0
#define DELAY_1_CYCLE 1
#define DELAY_0_CYCLE 2
#define SINGLE_LANE 0
#define MULTI_LANE_RX 1
#define MULTI_LANE_TX 2
#define MAX_SLOTS 32
#define BYTES_PER_SAMPLE 4
#define DEFAULT_BUFF_LEN_BYTES   (4 * 1024 * 1024)
#define DEFAULT_BUFF_LEN_WORDS   ((DEFAULT_BUFF_LEN_BYTES / 4) - 1)
#define DEFAULT_NUM_WORDS 1024
#define DEFAULT_NUM_BYTES (DEFAULT_NUM_WORDS * 4)
#define SPKR_STEREO 0x0
#define MIC_STEREO 0x0
#define SPKR_QUAD 0x0
#define MIC_QUAD 0x0
#define PRI_RATE_DET 0
#define SEC_RATE_DET 1
#define DISABLE_DEVICE_READ
#define DISABLE_RATE_DETECTION
#define SPKR 0
#define MIC 1
#define INVERT_INT_BIT_CLOCK 0x0
#define INVERT_EXT_BIT_CLOCK 0x1
#define DONT_INVERT_INT_BIT_CLOCK 0x2
#define DONT_INVERT_EXT_BIT_CLOCK 0x3
#define PGS_TIMEOUT msecs_to_jiffies(3000)
#define LONG_RATE_MIN 0
#define LONG_RATE_MAX 63
#define BIT_CLK_MAX 73728000

#define T_I2S_LONG_RATE_OFFSET 18
#define T_I2S_SPKR_MODE_SD0 0x800
#define T_I2S_SPKR_MODE_SD1 0x1000
#define T_I2S_SPKR_MODE_QUAD01 0x2800
#define T_I2S_SPKR_MONO 0x400
#define T_I2S_MIC_MODE_SD0 0x10
#define T_I2S_MIC_MODE_SD1 0x20
#define T_I2S_MIC_MODE_QUAD01 0x50
#define T_I2S_MIC_MONO 0x8
#define T_I2S_BIT_WIDTH_16 0x0
#define T_I2S_BIT_WIDTH_24 0x1
#define T_I2S_BIT_WIDTH_32 0x2
#define T_I2S_BIT_WIDTH_25 0x3
#define T_PCM_RATE_8_BIT_CLKS 0x0
#define T_PCM_RATE_16_BIT_CLKS 0x8000
#define T_PCM_RATE_32_BIT_CLKS 0x10000
#define T_PCM_RATE_64_BIT_CLKS 0x18000
#define T_PCM_RATE_128_BIT_CLKS 0x20000
#define T_PCM_RATE_256_BIT_CLKS 0x28000
#define T_TDM_SYNC_DELAY_0 0x4000000
#define T_TDM_SYNC_DELAY_1 0x2000000
#define T_TDM_SYNC_DELAY_2 0x0
#define T_RDDMA_WPSCNT_ONE 0x0
#define T_RDDMA_WPSCNT_TWO 0x10000
#define T_RDDMA_WPSCNT_FOUR 0x30000
#define T_RDDMA_WPSCNT_EIGHT 0x70000
#define T_RDDMA_PRI_AUDIO_INTF 0x1000
#define T_RDDMA_SEC_AUDIO_INTF 0x2000
#define T_RDDMA_FIFO_WM_8 0xE
#define T_WRDMA_WPSCNT_ONE 0x0
#define T_WRDMA_WPSCNT_TWO 0x20000
#define T_WRDMA_WPSCNT_FOUR 0x60000
#define T_WRDMA_WPSCNT_EIGHT 0xE0000
#define T_WRDMA_PRI_AUDIO_INTF 0x1000
#define T_WRDMA_LOOPBACK_CH0 0x9000
#define T_WRDMA_LOOPBACK_CH1 0xA000
#define T_WRDMA_SEC_AUDIO_INTF 0x2000
#define T_WRDMA_FIFO_WM_8 0xE
#define T_RATE_NUM_FS_1 0x0
#define T_RATE_NUM_FS_8 0x70
#define T_RATE_VAR_192_176P4_FS1 0x2800000
#define T_RATE_VAR_128_44P1_FS1 0x28000
#define T_RATE_VAR_32_8_FS1 0x280
#define T_RATE_TARGET128_FS1 0x960000
#define T_RATE_TARGET176P4_FS1 0x6D
#define T_RATE_TARGET192_FS1 0x64
#define T_RATE_VAR_192_176P4_FS8 0x11800000
#define T_RATE_VAR_128_44P1_FS8 0x238000
#define T_RATE_VAR_32_8_FS8 0x7F80
#define T_RATE_TARGET128_FS8 0x4B00000
#define T_RATE_TARGET176P4_FS8 0x367
#define T_RATE_TARGET192_FS8 0x320
#define T_SYNC_SEL_PRI 0x1
#define T_SYNC_SEL_SEC 0x2

#define H_I2S_LONG_RATE_OFFSET 16
#define H_I2S_SPKR_MODE_SD0 0x400
#define H_I2S_SPKR_MODE_SD1 0x800
#define H_I2S_SPKR_MODE_QUAD01 0x1400
#define H_I2S_SPKR_MONO 0x200
#define H_I2S_MIC_MODE_SD0 0x10
#define H_I2S_MIC_MODE_SD1 0x20
#define H_I2S_MIC_MODE_QUAD01 0x50
#define H_I2S_MIC_MONO 0x8
#define H_I2S_BIT_WIDTH_16 0x0
#define H_I2S_BIT_WIDTH_24 0x1
#define H_I2S_BIT_WIDTH_32 0x2
#define H_I2S_BIT_WIDTH_25 0x3
#define H_PCM_RATE_8_BIT_CLKS 0x0
#define H_PCM_RATE_16_BIT_CLKS 0x8000
#define H_PCM_RATE_32_BIT_CLKS 0x10000
#define H_PCM_RATE_64_BIT_CLKS 0x18000
#define H_PCM_RATE_128_BIT_CLKS 0x20000
#define H_PCM_RATE_256_BIT_CLKS 0x28000
#define H_TDM_SYNC_DELAY_0 0x4000000
#define H_TDM_SYNC_DELAY_1 0x2000000
#define H_TDM_SYNC_DELAY_2 0x0
#define H_RDDMA_WPSCNT_ONE 0x0
#define H_RDDMA_WPSCNT_TWO 0x4000
#define H_RDDMA_WPSCNT_FOUR 0xC000
#define H_RDDMA_WPSCNT_EIGHT 0x1C000
#define H_RDDMA_PRI_AUDIO_INTF 0x400
#define H_RDDMA_SEC_AUDIO_INTF 0x800
#define H_RDDMA_TER_AUDIO_INTF 0xC00
#define H_RDDMA_FIFO_WM_8 0xE
#define H_WRDMA_WPSCNT_ONE 0x0
#define H_WRDMA_WPSCNT_TWO 0x10000
#define H_WRDMA_WPSCNT_FOUR 0x30000
#define H_WRDMA_WPSCNT_EIGHT 0x70000
#define H_WRDMA_PRI_AUDIO_INTF 0x1000
#define H_WRDMA_SEC_AUDIO_INTF 0x2000
#define H_WRDMA_TER_AUDIO_INTF 0x3000
#define H_WRDMA_LOOPBACK_CH0 0x9000
#define H_WRDMA_LOOPBACK_CH1 0xA000
#define H_WRDMA_LOOPBACK_CH2 0xB000
#define H_WRDMA_FIFO_WM_8 0xE
#define H_RATE_NUM_FS_1 0x0
#define H_RATE_NUM_FS_8 0x70
#define H_RATE_VAR_192_176P4_FS1 0x2800000
#define H_RATE_VAR_128_44P1_FS1 0x28000
#define H_RATE_VAR_32_8_FS1 0x280
#define H_RATE_TARGET128_FS1 0x960000
#define H_RATE_TARGET176P4_FS1 0x6D
#define H_RATE_TARGET192_FS1 0x64
#define H_RATE_VAR_192_176P4_FS8 0x11800000
#define H_RATE_VAR_128_44P1_FS8 0x238000
#define H_RATE_VAR_32_8_FS8 0x7F80
#define H_RATE_TARGET128_FS8 0x4B00000
#define H_RATE_TARGET176P4_FS8 0x367
#define H_RATE_TARGET192_FS8 0x320
#define H_SYNC_SEL_PRI 0x2
#define H_SYNC_SEL_SEC 0x4
#define H_SYNC_SEL_TER 0x6

#define HS0_BITCLK_CMD_REG 0x17046000
#define HS0_BITCLK_CFG_REG 0x17046004
#define HS0_BITCLK_INV_REG 0x17046020
#define HS1_BITCLK_CMD_REG 0x17047000
#define HS1_BITCLK_CFG_REG 0x17047004
#define HS1_BITCLK_INV_REG 0x17047020
#define HS2_BITCLK_CMD_REG 0x17048000
#define HS2_BITCLK_CFG_REG 0x17048004
#define HS2_BITCLK_INV_REG 0x17048020
#define HS_BITCLK_UPDATE 0x1
#define HS_BITCLK_RESET 0x71F

enum operation_mode {
	NORMAL,
	INTERNAL_LB,
	EXTERNAL_LB_MASTER,
	EXTERNAL_LB_MASTER_SLAVE
};

/* Structure prototypes */

/* LPAIF HS-I2S core structure */
struct hsi2s_core {
	/* Device pointer */
	struct device *dev;

	/* HS-I2S device structure */
	struct hsi2s_device **hsi2s_arr;

	/* Memory mapping */
	void __iomem *lpaif_base_va;
	void __iomem *lpass_tcsr_base_va;

	/* IRQ */
	struct irq_desc *desc;
	int irq0;
	bool is_irq_enabled;
	void __iomem *irq_en;
	void __iomem *irq_stat;
	void __iomem *irq_clear;

	/* Rate detection registers */
	bool is_rate_enabled;
	u32 pri_ws_rate;
	u32 sec_ws_rate;
	int pri_rate_interface;
	int sec_rate_interface;
	void __iomem *pri_rate_config;
	void __iomem *pri_rate_target1_config;
	void __iomem *pri_rate_target2_config;
	void __iomem *pri_rate_bin;
	void __iomem *pri_rate_stc_diff;
	void __iomem *pri_rate_sel;
	void __iomem *sec_rate_config;
	void __iomem *sec_rate_target1_config;
	void __iomem *sec_rate_target2_config;
	void __iomem *sec_rate_bin;
	void __iomem *sec_rate_stc_diff;
	void __iomem *sec_rate_sel;

	/* QMI */
	struct qmi_handle *qmi_dev;
	wait_queue_head_t wq_qmi;
	bool qmi_connection;

	/* Clocks */
	struct clk *core_clk;
	struct clk *csr_hclk;
	struct clk *wr0_mem_clk;
	struct clk *wr1_mem_clk;
	struct clk *wr2_mem_clk;

	/* Locks */
	struct mutex irqlock;

	/* Target */
	u32 target;

	/* Target macros */
	struct hsi2s_macros *macro;

	/* Interface count */
	int i_count;
};

/* LPAIF HS-I2S device structure */
struct hsi2s_device {
	/* I2S/PCM mode */
	u8 lpaif_mode;

	/* Device pointer */
	struct device *dev;

	/* Configuration registers */
	void __iomem *i2s_ctl;
	void __iomem *pcm_ctl;
	void __iomem *tdm_ctl;
	void __iomem *tdm_sample_width;
	void __iomem *rpcm_slot_num;
	void __iomem *tpcm_slot_num;
	void __iomem *pcm_lane_config;
	void __iomem *i2s_sel;
	void __iomem *rddma_ctl;
	void __iomem *rddma_base;
	void __iomem *rddma_buff_len;
	void __iomem *rddma_curr_addr;
	void __iomem *rddma_per_len;
	void __iomem *wrdma_ctl;
	void __iomem *wrdma_base;
	void __iomem *wrdma_buff_len;
	void __iomem *wrdma_curr_addr;
	void __iomem *wrdma_per_len;
	void __iomem *lpaif_muxmode;

	/* GPIOs */
	bool is_pinctrl_names;

	/* Clocks */
	struct clk *intf_clk;

	/* Buffers */
	struct hsi2s_buffer *write_buffer;
	struct ping_pong *read_buffer;

	/* DMA thread */
	struct task_struct *rddma_thread;

	/* DMA addresses */
	void *lpass_rddma_start;
	void *lpass_wrdma_start;
	void *lpass_rddma_end;
	void *lpass_wrdma_end;

	/* DMA flags */
	int rddma_xfer_busy;
	int rddma_copy_busy;
	int rddma_in_progress;

	/* SMMU context */
	struct hsi2s_smmu_cb_ctx *hsi2s_smmu_ctx;

	/* Wait queues */
	wait_queue_head_t wq_rddma;
	wait_queue_head_t wq_wrdma;

	/* Minor number */
	int minor_num;

	/* Operational mode */
	enum operation_mode mode;

	/* Slave info */
	int slave;

	/* Number of clients */
	int client_count;

	/* I2S configurations */
	/* Register fields */
	u32 spkr_mode;
	u32 mic_mode;
	u32 mic_channel_count;
	u32 spkr_channel_count;
	u32 bit_depth;
	u32 wpscnt_rddma;
	u32 wpscnt_wrdma;
	u8 en_long_rate;
	u32 long_rate;
	/* Absolute values */
	u32 data_buffer_ms_val;
	u32 bit_depth_val;
	u32 mic_ch_count_val;
	u32 wrdma_periodic_length;
	u32 wrdma_periodic_length_bytes;

	/* PCM configurations */
	u32 pcm_rate;
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

	/* Device file attributes */
	dev_t curr_devid;
	struct cdev *cdev_sdr;
	struct class *class_sdr;
};

/* I2S parameters */
struct hsi2s_params {
	u32 bit_clk;
	u32 buffer_ms;
	u32 bit_depth;
	u32 spkr_channel_count;
	u32 mic_channel_count;
	u8 en_long_rate;
	u32 long_rate;
};

/* PCM parameters */
struct hspcm_params {
	u32 bit_clk;
	u32 buffer_ms;
	u8 rate;
	u8 sync_src;
	u8 aux_mode;
	u8 rpcm_width;
	u8 tpcm_width;
};

/* TDM parameters */
struct hstdm_params {
	u8 sync_delay;
	u32 tpcm_width;
	u32 rpcm_width;
	u32 rate;
	u8 en_diff_sample_width;
	u32 tpcm_sample_width;
	u32 rpcm_sample_width;
};

/* FIFO for holding HSI2S data in the kernel space */
struct hsi2s_buffer {
	void *buffer;
	void *head;
	void *tail;
	int size;
	bool data_ready;
	bool pollin;
	dma_addr_t handle;
};

/* Ping pong buffer for Tx */
struct ping_pong {
	void *buffer;
	void *ping_start;
	void *pong_start;
	u32 length;
	int last_xfer;
	int last_copy;
	dma_addr_t handle;
};

/* SMMU related */
struct hsi2s_smmu_cb_ctx {
	bool valid;
	struct platform_device *pdev_master;
	struct platform_device *smmu_pdev;
	struct dma_iommu_mapping *mapping;
	struct iommu_domain *iommu_domain;
	u32 va_start;
	u32 va_size;
	int ret;
};

/* Target specific macros */
struct hsi2s_macros {
	/* Offsets */
	u32 offset_i2s_ctl;
	u32 offset_pcm_ctl;
	u32 offset_tdm_ctl;
	u32 offset_tdm_sample_width;
	u32 offset_rpcm_slot_num;
	u32 offset_tpcm_slot_num;
	u32 offset_pcm_lane_config;
	u32 offset_i2s_sel;
	u32 offset_irq_en;
	u32 offset_irq_stat;
	u32 offset_irq_clear;
	u32 offset_rddma_ctl;
	u32 offset_rddma_base;
	u32 offset_rddma_buff_len;
	u32 offset_rddma_curr_addr;
	u32 offset_rddma_per_len;
	u32 offset_wrdma_ctl;
	u32 offset_wrdma_base;
	u32 offset_wrdma_buff_len;
	u32 offset_wrdma_curr_addr;
	u32 offset_wrdma_per_len;
	u32 offset_pri_rate_det_config;
	u32 offset_pri_rate_det_target1_config;
	u32 offset_pri_rate_det_target2_config;
	u32 offset_pri_rate_bin;
	u32 offset_pri_stc_diff;
	u32 offset_pri_rate_det_sel;
	u32 offset_sec_rate_det_config;
	u32 offset_sec_rate_det_target1_config;
	u32 offset_sec_rate_det_target2_config;
	u32 offset_sec_rate_bin;
	u32 offset_sec_stc_diff;
	u32 offset_sec_rate_det_sel;

	/* Bit fields */
	u32 bit_ws_src;
	u32 bit_mic_en;
	u32 bit_spkr_en;
	u32 bit_loopback;
	u32 bit_i2s_reset;
	u32 bit_en_long_rate;
	u32 bit_tpcm_width;
	u32 bit_rpcm_width;
	u32 bit_aux_mode;
	u32 bit_sync_src;
	u32 bit_pcm_loopback;
	u32 bit_ctrl_data_oe;
	u32 bit_one_slot_sync_en;
	u32 bit_pcm_en;
	u32 bit_pcm_en_tx;
	u32 bit_pcm_en_rx;
	u32 bit_pcm_reset;
	u32 bit_pcm_reset_tx;
	u32 bit_pcm_reset_rx;
	u32 bit_tdm_inv_rpcm_sync;
	u32 bit_tdm_inv_tpcm_sync;
	u32 bit_tdm_en_diff_sample_width;
	u32 bit_tdm_en;
	u32 bit_lane0_dir;
	u32 bit_lane1_dir;
	u32 bit_lane2_dir;
	u32 bit_lane3_dir;
	u32 bit_lane0_en;
	u32 bit_lane1_en;
	u32 bit_lane2_en;
	u32 bit_lane3_en;
	u32 bit_i2s_sel;
	u32 bit_rddma_en;
	u32 bit_rddma_burst_en;
	u32 bit_rddma_dyn_clk;
	u32 bit_rddma_reset;
	u32 bit_wrdma_en;
	u32 bit_wrdma_burst_en;
	u32 bit_wrdma_dyn_clk;
	u32 bit_wrdma_reset;
	u32 bit_rate_en;
	u32 bit_rate_reset;

	/* Register fields */
	u32 regfield_i2s_lrate_offset;
	u32 regfield_spkr_mode_sd0;
	u32 regfield_spkr_mode_sd1;
	u32 regfield_spkr_mode_quad01;
	u32 regfield_spkr_mono;
	u32 regfield_mic_mode_sd0;
	u32 regfield_mic_mode_sd1;
	u32 regfield_mic_mode_quad01;
	u32 regfield_mic_mono;
	u32 regfield_bit_width16;
	u32 regfield_bit_width24;
	u32 regfield_bit_width32;
	u32 regfield_bit_width25;
	u32 regfield_pcmrate_8;
	u32 regfield_pcmrate_16;
	u32 regfield_pcmrate_32;
	u32 regfield_pcmrate_64;
	u32 regfield_pcmrate_128;
	u32 regfield_pcmrate_256;
	u32 regfield_sync_delay_0;
	u32 regfield_sync_delay_1;
	u32 regfield_sync_delay_2;
	u32 regfield_rddma_wpscnt_one;
	u32 regfield_rddma_wpscnt_two;
	u32 regfield_rddma_wpscnt_four;
	u32 regfield_rddma_wpscnt_eight;
	u32 regfield_rddma_pri_audio_intf;
	u32 regfield_rddma_sec_audio_intf;
	u32 regfield_rddma_ter_audio_intf;
	u32 regfield_rddma_fifo_wm8;
	u32 regfield_wrdma_wpscnt_one;
	u32 regfield_wrdma_wpscnt_two;
	u32 regfield_wrdma_wpscnt_four;
	u32 regfield_wrdma_wpscnt_eight;
	u32 regfield_wrdma_pri_audio_intf;
	u32 regfield_wrdma_sec_audio_intf;
	u32 regfield_wrdma_ter_audio_intf;
	u32 regfield_wrdma_loopback_ch0;
	u32 regfield_wrdma_loopback_ch1;
	u32 regfield_wrdma_loopback_ch2;
	u32 regfield_wrdma_fifo_wm8;
	u32 regfield_rate_num_fs_1;
	u32 regfield_rate_num_fs_8;
	u32 regfield_rate_var_192_176p4_fs1;
	u32 regfield_rate_var_128_44p1_fs1;
	u32 regfield_rate_var_32_8_fs1;
	u32 regfield_rate_target128_fs1;
	u32 regfield_rate_target_176p4_fs1;
	u32 regfield_rate_target_192_fs1;
	u32 regfield_rate_var_192_176p4_fs8;
	u32 regfield_rate_var_128_44p1_fs8;
	u32 regfield_rate_var_32_8_fs8;
	u32 regfield_rate_target128_fs8;
	u32 regfield_rate_target_176p4_fs8;
	u32 regfield_rate_target_192_fs8;
	u32 regfield_rate_sync_sel_pri;
	u32 regfield_rate_sync_sel_sec;
	u32 regfield_rate_sync_sel_ter;
};

/* Function prototypes */

/* Char driver functions */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static long device_ioctl(struct file *, unsigned int, unsigned long);

#endif
