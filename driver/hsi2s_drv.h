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
#include <asm/dma-iommu.h>

/* Register offsets */
#define LPAIF_I2S_CTL				0x1000
#define LPAIF_PCM_I2S_SEL			0x1200
#define LPAIF_IRQ_EN				0x9000
#define LPAIF_IRQ_STAT				0x9004
#define LPAIF_IRQ_CLEAR				0x900C
#define LPAIF_RDDMA_CTL				0xC000
#define LPAIF_RDDMA_BASE			0xC004
#define LPAIF_RDDMA_BUFF_LEN			0xC008
#define LPAIF_RDDMA_CURR_ADDR			0xC00C
#define LPAIF_RDDMA_PER_LEN			0xC010
#define LPAIF_WRDMA_CTL				0x18000
#define LPAIF_WRDMA_BASE			0x18004
#define LPAIF_WRDMA_BUFF_LEN			0x18008
#define LPAIF_WRDMA_CURR_ADDR			0x1800C
#define LPAIF_WRDMA_PER_LEN			0x18010

/* Bits for I2S control register */
#define I2S_WS_SRC				BIT(2)
#define I2S_MIC_EN				BIT(9)
#define I2S_SPKR_EN				BIT(16)
#define I2S_LOOPBACK				BIT(17)
#define I2S_RESET				BIT(31)

/* Bits for I2S select register */
#define I2S_SEL					BIT(0)

/* Bits for read DMA control register */
#define RDDMA_EN				BIT(0)
#define RDDMA_BURST_EN				BIT(20)
#define RDDMA_DYN_CLK				BIT(21)
#define RDDMA_RESET				BIT(31)

/* Bits for write DMA control register */
#define WRDMA_EN				BIT(0)
#define WRDMA_BURST_EN				BIT(21)
#define WRDMA_DYN_CLK				BIT(22)
#define WRDMA_RESET				BIT(31)

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

/* IOCTLs */
#define I2S_NORMAL_MODE _IOWR('i', 0, int)
#define I2S_INTERNAL_LOOPBACK _IOWR('i', 1, int)

/* Additional macros */
#define DEVICE_NAME "hsi2s_driver"
#define SDR0 "hs0_i2s"
#define SDR1 "hs1_i2s"
#define BYTES_PER_SAMPLE 4
#define DEFAULT_BUFF_LEN_BYTES   (4 * 1024 * 1024)
#define DEFAULT_BUFF_LEN_WORDS   ((DEFAULT_BUFF_LEN_BYTES / 4) - 1)
#define DEFAULT_NUM_WORDS 1024
#define DEFAULT_NUM_BYTES (DEFAULT_NUM_WORDS * 4)
#define METADATA_SIZE 256

#define I2S_LONG_RATE 0x3C0000
#define I2S_SPKR_MODE_QUAD01 0x2800
#define I2S_MIC_MODE_QUAD01 0x50
#define I2S_BIT_WIDTH_32 0x2
#define RDDMA_WPSCNT_TWO 0x10000
#define RDDMA_PRI_AUDIO_INTF 0x1000
#define RDDMA_SEC_AUDIO_INTF 0x2000
#define RDDMA_FIFO_WM_8 0xE
#define WRDMA_WPSCNT_TWO 0x20000
#define WRDMA_WPSCNT_FOUR 0x60000
#define WRDMA_PRI_AUDIO_INTF 0x1000
#define WRDMA_LOOPBACK_CH0 0x9000
#define WRDMA_LOOPBACK_CH1 0xA000
#define WRDMA_SEC_AUDIO_INTF 0x2000
#define WRDMA_FIFO_WM_8 0xE

/* Structure prototypes */

/* LPAIF HS-I2S core structure */
struct hsi2s_core {
	/* HS-I2S device structure */
	struct hsi2s_device **hsi2s_arr;

	/* Memory mapping */
	void __iomem *lpaif_base_va;

	/* IRQ */
	struct irq_desc *desc;
	int irq0;

	/* Clocks */
	struct clk *core_clk;
	struct clk *csr_hclk;
	struct clk *wr0_mem_clk;
	struct clk *wr1_mem_clk;
	struct clk *wr2_mem_clk;

	/* Locks */
	struct mutex irqlock;
};

/* LPAIF HS-I2S device structure */
struct hsi2s_device {
	/* Configuration registers */
	void __iomem *i2s_ctl;
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
	void __iomem *irq_en;
	void __iomem *irq_stat;
	void __iomem *irq_clear;

	/* GPIOs */
	bool is_pinctrl_names;

	/* Clocks */
	struct clk *intf_clk;

	/* Buffers */
	struct hsi2s_buffer *write_buffer;

	/* Buffer metadata */
	struct buffer_metadata *b_meta_read;
	struct buffer_metadata *b_meta_write;

	/* Buffer indices */
	int meta_index_read;
	int meta_index_write;
	int user_read_index;

	/* DMA thread */
	struct task_struct *rddma_thread;

	/* DMA addresses */
	void *lpass_rddma_start;
	void *lpass_wrdma_start;
	void *lpass_rddma_end;
	void *lpass_wrdma_end;

	/* DMA flags */
	int rddma_busy;

	/* SMMU context */
	struct hsi2s_smmu_cb_ctx *hsi2s_smmu_ctx;

	/* Spinlocks */
	spinlock_t metacount_lock;

	/* Wait queues */
	wait_queue_head_t wq_rddma;
	wait_queue_head_t wq_wrdma;

	/* Minor number */
	int minor_num;

	/* Operational mode */
	int mode;

	/* Number of clients */
	int client_count;

	/* Device file attributes */
	dev_t curr_devid;
	struct cdev *cdev_sdr;
	struct class *class_sdr;
};

/* FIFO for holding HSI2S data in the kernel space */
struct hsi2s_buffer {
	s32 *buffer;
	void *head;
	void *tail;
	int size;
};

/* Buffer metadata */
struct buffer_metadata {
	void *start_address;
	u32 length;
	int data_ready;
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

/* Function prototypes */

/* Char driver functions */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char *, size_t, loff_t *);
static long device_ioctl(struct file *, unsigned int, unsigned long);

#endif
