//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/kernel.h>
#include <linux/io.h> //writel_relaxed
#include "target_ops.h"
#include "hsi2s_param.h"
#include "log.h"

struct lemans_reg {
	u32 CORE_CC_I2S_IF_CTL; // 0x390C000  LPASS_LPASS_CORE_CC_I2S_IF_CTL
	u32 HS_IF_0_CMD_RCGR;	// 0x3947000
	u32 HS_IF_0_CFG_RCGR;	// 0x3947004
	u32 HS_IF_1_CMD_RCGR;	// 0x3947020
	u32 HS_IF_1_CFG_RCGR;	// 0x3947024

	u32 lpaif_muxmode;	// 0x3947120

	u32 i2s_ctl;		// 0x3b41000
	u32 i2s_sel;		// 0x3b41200
	u32 pcm_ctl;		// 0x3b41500
	u32 tdm_ctl;		// 0x3b41518
	u32 tdm_sample_width;	// 0x3b4151c
	u32 tdm_rpcm_slot;	// 0x3b41520
	u32 tdm_tpcm_slot;	// 0x3b41524
	u32 pcm_lane_config;	// 0x3b41528

	u32 irq_en;		// 0x3b49000
	u32 irq_stat;		// 0x3b49004
	u32 irq_clear;		// 0x3b4900c
	u32 irq2_en;		// 0x3b49014
	u32 irq2_stat;		// 0x3b49018
	u32 irq2_clear; 	// 0x3b49020

	u32 rddma_ctl;		// 0x3b4c000
	u32 rddma_base;		// 0x3b4c004
	u32 rddma_buff_len;	// 0x3b4c008
	u32 rddma_curr_addr;	// 0x3b4c00c
	u32 rddma_per_len;	// 0x3b4c010

	u32 wrdma_ctl;		// 0x3b58000
	u32 wrdma_base;		// 0x3b58004
	u32 wrdma_buff_len;	// 0x3b58008
	u32 wrdma_curr_addr;	// 0x3b5800c
	u32 wrdma_per_len;	// 0x3b58010
	u32 wrdma_ram_addr;	// 0x3b58048
	u32 wrdma_ram_len;	// 0x3b5804c

};

static struct lemans_reg lemans_reg = {
	.CORE_CC_I2S_IF_CTL 	= 0x390c000,

	.HS_IF_0_CMD_RCGR	= 0x3947000,
	.HS_IF_0_CFG_RCGR	= 0x3947004,
	.HS_IF_1_CMD_RCGR	= 0x3947020,
	.HS_IF_1_CFG_RCGR	= 0x3947024,

	.lpaif_muxmode 		= 0x3947120,

	.i2s_ctl		= 0x3b41000,
	.i2s_sel		= 0x3b41200,
	.pcm_ctl		= 0x3b41500,
	.tdm_ctl		= 0x3b41518,
	.tdm_sample_width	= 0x3b4151c,
	.tdm_rpcm_slot		= 0x3b41520,
	.tdm_tpcm_slot		= 0x3b41524,
	.pcm_lane_config	= 0x3b41528,

	.irq_en			= 0x3b49000,
	.irq_stat		= 0x3b49004,
	.irq_clear		= 0x3b4900c,
	.irq2_en		= 0x3b49014,
	.irq2_stat		= 0x3b49018,
	.irq2_clear	 	= 0x3b49020,

	.rddma_ctl		= 0x3b4c000,
	.rddma_base		= 0x3b4c004,
	.rddma_buff_len		= 0x3b4c008,
	.rddma_curr_addr	= 0x3b4c00c,
	.rddma_per_len		= 0x3b4c010,

	.wrdma_ctl		= 0x3b58000,
	.wrdma_base		= 0x3b58004,
	.wrdma_buff_len		= 0x3b58008,
	.wrdma_curr_addr	= 0x3b5800c,
	.wrdma_per_len		= 0x3b58010,
	.wrdma_ram_addr		= 0x3b58048,
	.wrdma_ram_len		= 0x3b5804c,
};

static inline u32 read_reg(u8* base, u32 offset)
{
	void __iomem *addr = (void __iomem *)(base + offset);
	u32 val = readl_relaxed(addr);
        return val;
}

static inline void write_reg(u8 * base, u32 offset, u32 val)
{
	void __iomem *addr = (void __iomem *)(base + offset);
	writel_relaxed(val, addr);
}

struct Resource_reg {
        u32 paddr;
        u32 size;
        u8* vaddr;
};

static struct Resource_reg regions[] = {
	{ //lpaif
		.paddr = 0x3B40000,
		.size = 0x29000,
		.vaddr = NULL,
	},
	{ //lpass_core_cc_hs_if
		.paddr = 0x3942000,
		.size = 0x6000,
		.vaddr = NULL,
	},
	{ //lpass_core_cc_i2s_if_ctl
		.paddr = 0x390C000,
		.size = 4,//0x1000,
		.vaddr = NULL,
	},
};

#define in_region(paddr, region) (paddr >= region.paddr && paddr < region.paddr + region.size)
static u32 do_read_paddr(u32 paddr)
{
	u8 *vaddr = NULL;
	u32 offset;

	for(int i=0; i< sizeof(regions)/sizeof(regions[0]); i++) {
		if (in_region(paddr, regions[i])) {
			vaddr = regions[i].vaddr;
			offset = paddr - regions[i].paddr;
			return read_reg(vaddr, offset);
		}
	}
	printk("paddr 0x%x out of Region\n", paddr);
	return 0;
}

static void  do_write_paddr(u32 paddr, u32 val)
{
	u8 *vaddr = NULL;
	u32 offset;
	//printk("%s: paddr 0x%.8x, val 0x%.8x\n", __func__, paddr, val);
	for(int i=0; i< sizeof(regions)/sizeof(regions[0]); i++) {
		if (in_region(paddr, regions[i])) {
			vaddr = regions[i].vaddr;
			offset = paddr - regions[i].paddr;
			return write_reg(vaddr, offset, val);
		}
	}
	printk("paddr 0x%x out of Region\n", paddr);
}

/* Clear the complete register */
static void do_clearallbits(u32 paddr)
{
	//printk("reg_clear: paddr 0x%x\n", paddr);
	do_write_paddr(paddr, 0x0);
}

/* Set specific register bits */
static void do_setbits(u32 paddr, u32 val)
{
	u32 reg;

	//printk("setbits: paddr 0x%x, val 0x%x\n", paddr, val);
	reg = do_read_paddr(paddr);
	reg |= val;
	do_write_paddr(paddr, reg);
}

/* Clear specific register bits */
static void do_clearbits(u32 paddr, u32 val)
{
	u32 reg;

	//printk("clearbits: paddr 0x%x, val 0x%x\n", paddr, val);
	reg = do_read_paddr(paddr);
	reg &= ~val;
	do_write_paddr(paddr, reg);
}

void set_lemans_reg_base(void * base[], const int count)
{
	if (sizeof(regions)/sizeof(regions[0]) != count) {
                printk("%s: count %d expected to %lu\n", __func__, count, sizeof(regions)/sizeof(regions[0]));
                return;
        }

        for(int i=0; i< sizeof(regions)/sizeof(regions[0]); i++) {
                regions[i].vaddr = base[i];
        }
}

#define read_paddr(pa) do_read_paddr(pa)
#define write_paddr(pa, val) do_write_paddr(pa, val)
#define clearallbits(pa) do_clearallbits(pa)
#define setbits(pa, bits) do_setbits(pa, bits)
#define clearbits(pa, bits) do_clearbits(pa, bits)

#define reg_clear(paddr) clearallbits(paddr)

#define MAX_SLOTS 32
#define BYTES_PER_SAMPLE 4
#define DEFAULT_BUFF_LEN_BYTES   (4 * 1024 * 1024)
#define DEFAULT_BUFF_LEN_WORDS   ((DEFAULT_BUFF_LEN_BYTES / 4) - 1)
#define WRDMA_RAM_LENGTH 512
static int slave;
static u32 dma_buffer_length = DEFAULT_BUFF_LEN_BYTES;

enum operation_mode {
	NORMAL,
	INTERNAL_LB,
	EXTERNAL_LB_MASTER,
	EXTERNAL_LB_MASTER_SLAVE
};
static struct hsi2s_interface {
	int interface;

	u32 hs_index;
	u32 dma_index;
#define HS_I2S 0
#define HS_PCM 1
	int lpaif_mode;
	enum operation_mode operation_mode;
	/* I2S configurations */
	struct i2s_config i2s;

	/* PCM configurations */
	struct pcm_config pcm;

	/* DMA related */
	struct dma_config dma;
} hs_intfs[5];

static u32 intf2dma(int interface)
{
	return hs_intfs[interface].dma_index;
}
static u32 intf2hs(int interface)
{
	return hs_intfs[interface].hs_index;
}

static void clear_irqs(void)
{
	hsi2s_log(HSI2S_DEBUG, module, "%s() enter\n", __func__);
	write_paddr(lemans_reg.irq_clear, 0xFFFFFFFF); //irq_clear
	write_paddr(lemans_reg.irq2_clear, 0xFFFFFFFF); //irq2_clear
	hsi2s_log(HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void reset_rddma_registers(int interface)
{
	u32 dma_index = intf2dma(interface);

	u32 bit_rddma_reset = BIT(31);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	/* rddma */
	clearallbits(lemans_reg.rddma_ctl + dma_index * 0x1000); //rddma_ctl
	clearallbits(lemans_reg.rddma_base + dma_index * 0x1000); //rddma_base
	clearallbits(lemans_reg.rddma_buff_len + dma_index * 0x1000); //rddma_buff_len
	clearallbits(lemans_reg.rddma_curr_addr + dma_index * 0x1000); //rddma_curr_addr
	clearallbits(lemans_reg.rddma_per_len + dma_index * 0x1000); //rddma_per_len
	setbits(lemans_reg.rddma_ctl + dma_index * 0x1000, bit_rddma_reset); //rddma_ctl, bit_rddma_reset
	clearbits(lemans_reg.rddma_ctl + dma_index * 0x1000, bit_rddma_reset); //rddma_ctl, bit_rddma_reset
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void reset_wrdma_registers(int interface)
{
	u32 dma_index = intf2dma(interface);
	u32 bit_wrdma_reset = BIT(31);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	/* wrdma */
	clearallbits(lemans_reg.wrdma_ctl + dma_index * 0x1000); //wrdma_ctl
	clearallbits(lemans_reg.wrdma_base + dma_index * 0x1000); //wrdma_base
	clearallbits(lemans_reg.wrdma_buff_len + dma_index * 0x1000); //wrdma_buff_len
	clearallbits(lemans_reg.wrdma_curr_addr + dma_index * 0x1000); //wrdma_curr_addr
	clearallbits(lemans_reg.wrdma_per_len + dma_index * 0x1000); //wrdma_per_len
	clearallbits(lemans_reg.wrdma_ram_addr + dma_index * 0x1000); //wrdma_ram_addr
	clearallbits(lemans_reg.wrdma_ram_len + dma_index * 0x1000); //wrdma_ram_len
	setbits(lemans_reg.wrdma_ctl + dma_index * 0x1000, bit_wrdma_reset); //wrdma_ctl, bit_wrdma_reset
	clearbits(lemans_reg.wrdma_ctl + dma_index * 0x1000, bit_wrdma_reset); //wrdma_ctl, bit_wrdma_reset
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void reset_registers(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	u32 bit_i2s_reset = BIT(31);
	u32 bit_pcm_reset = BIT(31);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	if (hs_intf->lpaif_mode == HS_I2S) {
		reg_clear(lemans_reg.i2s_ctl + hs_index * 0x1000); //i2s_ctl
	} else {
		reg_clear(lemans_reg.pcm_ctl + hs_index * 0x1000); //pcm_ctl
		reg_clear(lemans_reg.tdm_ctl + hs_index * 0x1000); //tdm_ctl
	}

	clear_irqs();
	reset_rddma_registers(interface);

	reset_wrdma_registers(interface);

	if (hs_intf->lpaif_mode == HS_I2S) {
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset); //i2s_ctl, bit_i2s_reset
		clearbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset); //i2s_ctl, bit_i2s_reset
	} else {
		setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset); //pcm_ctl, bit_pcm_reset
		clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset); //pcm_ctl, bit_pcm_reset
	}

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}




static void update_dma_config(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 dma_buffer_length_words = dma_buffer_length / BYTES_PER_SAMPLE - 1;

	hs_intf->dma.rddma_buff_len = dma_buffer_length_words;
	hs_intf->dma.rddma_per_len = ((dma_buffer_length_words + 1) / 2) - 1;

	hs_intf->dma.wrdma_buff_len = dma_buffer_length_words;
	if (hs_intf->operation_mode == NORMAL) {
		hs_intf->dma.wrdma_per_len = (hs_intf->dma.wrdma_periodic_length_bytes / BYTES_PER_SAMPLE) - 1; //0xeff
	} else {
		hs_intf->dma.wrdma_per_len = ((dma_buffer_length_words + 1) / 2) - 1;
	}
}

static u32 cal_i2s_wpscnt(u32 bit_depth, u32 ch_count)
{
/*
0x0: ONE - ONE (mono, 16 bit stereo)
0x1: TWO - TWO ( 16 bit 4 channel, 32/24/20 bit stereo)
0x2: THREE - THREE (16 bit 6 channel)
0x3: FOUR - FOUR (16 bit 8 channel, 32/24/20 bit 4 channel)
0x5: SIX - SIX (32/24/20 bit 6 channel)
0x7: EIGHT - EIGHT (32/24/20 bit 8 channel)
0x9: TEN - TEN (32/24/20 bit 10 channel)
0xB: TWELVE - TWELVE (32/24/20 bit 12 channel)
0xD: FOURTEEN - FOURTEEN (32/24/20 bit 14 channel)
0xF: SIXTEEN - SIXTEEN (32/24/20 bit 16 channel)
*/
	u32 bits = ch_count * bit_depth;
	u32 num = bits / 32;
	if (bits % 32) {
		num += 1;
	}
	switch (num) {
		case 1: return 0x0;
		case 2: return 0x1;
		case 3: return 0x2;
		case 4: return 0x3;
		case 6: return 0x5;
		case 8: return 0x7;
		case 10: return 0x9;
		case 12: return 0xB;
		case 14: return 0xD;
		case 16: return 0xF;
	}
	return 0x1;
}

static u32 cal_pcm_wpscnt(u32 rate)
{
	u32 num = rate / 32;
	if (rate % 32) {
		num += 1;
	}
	switch (num) {
		case 1: return 0x0;
		case 2: return 0x1;
		case 3: return 0x2;
		case 4: return 0x3;
		case 6: return 0x5;
		case 8: return 0x7;
		case 10: return 0x9;
		case 12: return 0xB;
		case 14: return 0xD;
		case 16: return 0xF;
	}
	return 0x1;
}

/* Configure the read DMA registers */
static void configure_rddma(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	u32 hs_index = intf2hs(interface);
	u32 dma_index = intf2dma(interface);
	u32 value;
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() operation_mode %d\n", __func__, hs_intf->operation_mode);

	//write_paddr(lemans_reg.rddma_base + hs_index * 0x1000, hs_intf->read_buffer->handle); //rddma_basewrite_paddr(lemans_reg.rddma_buff_len + hs_index * 0x1000, dma_buffer_length_words); //rddma_buff_len
	value = hs_intf->dma.rddma_base;
	write_paddr(lemans_reg.rddma_base + dma_index * 0x1000, value); //rddma_basewrite_paddr(lemans_reg.rddma_buff_len + hs_index * 0x1000, dma_buffer_length_words); //rddma_buff_len
	value = hs_intf->dma.rddma_buff_len;
	write_paddr(lemans_reg.rddma_buff_len + dma_index * 0x1000, value); //rddma_buff_len
	value = hs_intf->dma.rddma_per_len;
	write_paddr(lemans_reg.rddma_per_len + dma_index * 0x1000, value); //rddma_per_len

	u32 bit_rddma_burst_en = BIT(20);
	u32 bit_rddma_dyn_clk = BIT(21);
	u32 regfield_rddma_fifo_wm8 = 0xE;
	u32 wpscnt_rddma = 0;
	if(hs_intf->lpaif_mode == HS_I2S) {
		wpscnt_rddma = cal_i2s_wpscnt(hs_intf->i2s.bit_depth_val, hs_intf->i2s.spkr_ch_count_val) << 16;
	} else {
		if(hs_intf->pcm.tdm_en) {
			wpscnt_rddma = cal_pcm_wpscnt(hs_intf->pcm.tdm_rate) << 16;
		} else {
			wpscnt_rddma = cal_pcm_wpscnt(hs_intf->pcm.pcm_rate_val) << 16;
		}
	}

	u32 rddma_ctl_bits = bit_rddma_burst_en | bit_rddma_dyn_clk | wpscnt_rddma | regfield_rddma_fifo_wm8;
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() rddma_ctl_bits = 0x%x (should be 0x33000e)\n", __func__, rddma_ctl_bits);
	//rddma_ctl_bits = 0x33000e;
	int audio_intf[] = {0x1, 0x2, 0x3, 0x4, 0xe};
	if (hs_intf->operation_mode == INTERNAL_LB) {
		setbits(lemans_reg.rddma_ctl + dma_index * 0x1000, rddma_ctl_bits | audio_intf[hs_index] << 12); //rddma_ctl, bit_rddma_burst_en | bit_rddma_dyn_clk | wpscnt_rddma | regfield_rddma_fifo_wm8  | regfield_rddma_*_audio_intf
	} else {
		setbits(lemans_reg.rddma_ctl + dma_index * 0x1000, rddma_ctl_bits | audio_intf[hs_index] << 12); //rddma_ctl, bit_rddma_burst_en | bit_rddma_dyn_clk | wpscnt_rddma | regfield_rddma_fifo_wm8  | regfield_rddma_*_audio_intf
	}

	setbits(lemans_reg.irq_en, 0x07 << (dma_index * 3)); //irq_en, IRQ_PER_RDDMA_CH* | IRQ_UNDR_RDDMA_CH* | IRQ_ERR_RDDMA_CH*

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void configure_wrdma(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	u32 hs_index = intf2hs(interface);
	u32 dma_index = intf2dma(interface);
	u32 value;
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);

	//write_paddr(lemans_reg.wrdma_base + hs_index * 0x1000, hs_intf->write_buffer->handle); //wrdma_base
	value = hs_intf->dma.wrdma_base;
	write_paddr(lemans_reg.wrdma_base + dma_index * 0x1000, value); //wrdma_base
	value = hs_intf->dma.wrdma_buff_len;
	write_paddr(lemans_reg.wrdma_buff_len + dma_index * 0x1000, value); //wrdma_buff_len
	value = hs_intf->dma.wrdma_per_len;
	write_paddr(lemans_reg.wrdma_per_len + dma_index * 0x1000, value);

	/* Increase the FIFO watermark */
	write_paddr(lemans_reg.wrdma_ram_addr + dma_index * 0x1000, (WRDMA_RAM_LENGTH * hs_index)); //wrdma_ram_addr
	write_paddr(lemans_reg.wrdma_ram_len + dma_index * 0x1000, WRDMA_RAM_LENGTH); //wrdma_ram_len

	u32 bit_wrdma_burst_en = BIT(21);
	u32 bit_wrdma_dyn_clk = BIT(22);
	u32 wpscnt_wrdma = 0;
	if(hs_intf->lpaif_mode == HS_I2S) {
		wpscnt_wrdma = cal_i2s_wpscnt(hs_intf->i2s.bit_depth_val, hs_intf->i2s.mic_ch_count_val) << 17;
	} else {
		if(hs_intf->pcm.tdm_en) {
			wpscnt_wrdma = cal_pcm_wpscnt(hs_intf->pcm.tdm_rate) << 17;
		} else {
			wpscnt_wrdma = cal_pcm_wpscnt(hs_intf->pcm.pcm_rate_val) << 17;
		}
	}

	u32 wrdma_ctl_bits = bit_wrdma_burst_en | bit_wrdma_dyn_clk | wpscnt_wrdma | (WRDMA_RAM_LENGTH - 1) << 1;
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() wrdma_ctl_bits = 0x%x (should be 0x6603fe)\n", __func__, wrdma_ctl_bits);
	//wrdma_ctl_bits = 0x6603fe;
	if (hs_intf->operation_mode == INTERNAL_LB) {
		int loopback_ch[] = {0x9, 0xA, 0xB, 0xC, 0xD};
		setbits(lemans_reg.wrdma_ctl + dma_index * 0x1000, wrdma_ctl_bits | loopback_ch[dma_index] << 12); //wrdma_ctl,  bit_wrdma_dyn_clk | bit_wrdma_burst_en | wpscnt_wrdma | (WRDMA_RAM_LENGTH - 1) << 1 | regfield_wrdma_loopback_ch*
	} else {
		int audio_intf[] = {0x1, 0x2, 0x3, 0x4, 0x5};
		setbits(lemans_reg.wrdma_ctl + dma_index * 0x1000, wrdma_ctl_bits | audio_intf[hs_index] << 12); //wrdma_ctl,  bit_wrdma_dyn_clk | bit_wrdma_burst_en | wpscnt_wrdma | (WRDMA_RAM_LENGTH - 1) << 1 | regfield_wrdma_*_audio_intf
	}

	switch (dma_index) {
		case 0:
		case 1:
		case 2:
		case 3:
			setbits(lemans_reg.irq_en, 0x07 << (dma_index * 3 + 15)); //irq_en, IRQ_PER_WRDMA_CH* | IRQ_OVR_WRDMA_CH* | IRQ_ERR_WRDMA_CH*
			break;
		case 4:
			setbits(lemans_reg.irq2_en, 0x07 << 4); //irq2_en, IRQ2_PER_WRDMA_CH4 | IRQ2_OVR_WRDMA_CH4 | IRQ2_ERR_WRDMA_CH4
			break;
	}

	setbits(lemans_reg.wrdma_ctl + dma_index * 0x1000, 0x01); //wrdma_ctl, bit_wrdma_en

/*
	if (hsi2s_core->is_rate_enabled) {
		if (intf == hsi2s_core->pri_rate_interface)
			setbits(hsi2s_core->pri_rate_config, hsi2s_core->macro->bit_rate_en);
		else if (intf == hsi2s_core->sec_rate_interface)
			setbits(hsi2s_core->sec_rate_config, hsi2s_core->macro->bit_rate_en);
	}
*/
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

#define configure_rddma_int_lb configure_rddma
#define configure_wrdma_int_lb configure_wrdma



/* Enable RPCM slots */
static void enable_rpcm_slot(int interface)
{
	u32 hs_index = intf2hs(interface);
	int slot;

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	for (slot = 0; slot < MAX_SLOTS; slot++) {
		setbits(lemans_reg.tdm_rpcm_slot + hs_index * 0x1000, 1 << slot);
	}
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}


/* Enable TPCM slots */
static void enable_tpcm_slot(int interface)
{
	u32 hs_index = intf2hs(interface);
	int slot;

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	for (slot = 0; slot < MAX_SLOTS; slot++) {
		setbits(lemans_reg.tdm_tpcm_slot + hs_index * 0x1000, 1 << slot);
	}
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}


static void lemans_configure_lpaif_mode(int interface, u32 mode)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	u32 bit_i2s_sel = BIT(0);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	switch (mode) {
		case HS_I2S:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Configure LPAIF in HS-I2S mode\n");
			clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //i2s_sel, bit_i2s_sel
			hs_intf->lpaif_mode = HS_I2S;
			break;
		case HS_PCM:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Configure LPAIF in HS-PCM mode\n");
			setbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //i2s_sel, bit_i2s_sel
			hs_intf->lpaif_mode = HS_PCM;
			break;
		default:
			hsi2s_intf_log(interface, HSI2S_WARN, module, "Undefined LPAIF mode. Defaulting to HS-I2S mode\n");
			clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //i2s_sel, bit_i2s_sel
			hs_intf->lpaif_mode = HS_I2S;
			break;
	}
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}


/* Configure interface as master/slave */
static void lemans_configure_muxmode(int interface, int mode)
{
	u32 hs_index = intf2hs(interface);
	u32 lpaif_muxmode = BIT(0);
	u32 bit_ws_src = BIT(2);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	//u32 paddr[] = {0x3947120, 0x394712C, 0x3947138, 0x3947144, 0x3947150};
	if (mode) {
		/* Configure slave */
		setbits(lemans_reg.lpaif_muxmode + hs_index * 0xC, lpaif_muxmode); //lpaif_muxmode
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_ws_src); //i2s_ctl, bit_ws_src
		hsi2s_intf_log(interface, HSI2S_INFO, module, "configured as slave\n");
	} else {
		/* Configure master */
		clearbits(lemans_reg.lpaif_muxmode + hs_index * 0xC, lpaif_muxmode); //lpaif_muxmode
		clearbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_ws_src); //i2s_ctl, bit_ws_src
		hsi2s_intf_log(interface, HSI2S_INFO, module, "configured as master\n");
	}
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}


static void lemans_set_pcm_lane_config(int interface, u32 config)
{
#define SINGLE_LANE 0
#define MULTI_LANE_RX 1
#define MULTI_LANE_TX 2
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);
	u32 paddr = lemans_reg.pcm_lane_config + hs_index * 0x1000;

	u32 bit_lane0_dir = BIT(0);
	u32 bit_lane1_dir = BIT(1);
	u32 bit_lane0_en = BIT(16);
	u32 bit_lane1_en = BIT(17);

	hs_intf->pcm.lane_config = config;
	switch (config) {
		case SINGLE_LANE:
			/* Single lane */
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Single lane configuration\n");
			setbits(paddr, bit_lane0_dir); //pcm_lane_config, bit_lane0_dir
			setbits(paddr, bit_lane0_en); //pcm_lane_config, bit_lane0_en
			clearbits(paddr, bit_lane1_dir); //pcm_lane_config, bit_lane1_dir
			setbits(paddr, bit_lane1_en); //pcm_lane_config, bit_lane1_en
			break;
		case MULTI_LANE_RX:
			/* Multi lane Rx */
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Multi lane Rx configuration\n");
			setbits(paddr, bit_lane0_dir); //pcm_lane_config, bit_lane0_dir
			setbits(paddr, bit_lane0_en); //pcm_lane_config, bit_lane0_en
			setbits(paddr, bit_lane1_dir); //pcm_lane_config, bit_lane1_dir
			setbits(paddr, bit_lane1_en); //pcm_lane_config, bit_lane1_en
			break;
		case MULTI_LANE_TX:
			/* Multi lane Tx */
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Multi lane Tx configuration\n");
			clearbits(paddr, bit_lane0_dir); //pcm_lane_config, bit_lane0_dir
			setbits(paddr, bit_lane0_en); //pcm_lane_config, bit_lane0_en
			clearbits(paddr, bit_lane1_dir); //pcm_lane_config, bit_lane1_dir
			setbits(paddr, bit_lane1_en); //pcm_lane_config, bit_lane1_en
			break;
		default:
			/* Single lane */
			hsi2s_intf_log(interface, HSI2S_WARN, module, "Setting default lane configuration(single lane)\n");
			setbits(paddr, bit_lane0_dir); //pcm_lane_config, bit_lane0_dir
			setbits(paddr, bit_lane0_en); //pcm_lane_config, bit_lane0_en
			clearbits(paddr, bit_lane1_dir); //pcm_lane_config, bit_lane1_dir
			setbits(paddr, bit_lane1_en); //pcm_lane_config, bit_lane1_en
	}

}

static void lemans_reset_interface(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	//reset_registers(interface);
	if (hs_intf->lpaif_mode == HS_I2S) {
		hsi2s_intf_log(interface, HSI2S_INFO, module, "Resetting I2S control register\n");
		reg_clear(lemans_reg.i2s_ctl + hs_index * 0x1000); //i2s_ctl
	} else {
		hsi2s_intf_log(interface, HSI2S_INFO, module, "Resetting PCM control register\n");
		reg_clear(lemans_reg.pcm_ctl + hs_index * 0x1000); //pcm_ctl
		reg_clear(lemans_reg.tdm_ctl + hs_index * 0x1000); //tdm_ctl
		reg_clear(lemans_reg.tdm_sample_width + hs_index * 0x1000); //tdm_sample_width
	}

	hsi2s_intf_log(interface, HSI2S_INFO, module, "Resetting DMA registers\n");
	reset_rddma_registers(interface);
	reset_wrdma_registers(interface);
	/* Clear IRQs */
	clear_irqs();
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static u32 bit_depth_to_fields(u32 val)
{
	//  LPASS_SDR_LPAIF_I2S_CTLa : BIT_WIDTH
	switch (val) {
		case 16: return 0x0;
		case 24: return 0x1;
		case 32: return 0x2;
		case 25: return 0x3;
	}
	return 0x2;
}

static u32 spkr_channel_to_fields(u32 ch_count)
{
#define SPKR_MONO   (1<<10) //0x400
#define SPKR_STEREO (0<<10) //0x0

#define T_I2S_SPKR_MODE_SD0 (0x1 << 11) //0x800
#define T_I2S_SPKR_MODE_SD1 (0x2 << 11) //0x1000
#define T_I2S_SPKR_MODE_QUAD01 (0x5 << 11) //0x2800

	// LPASS_SDR_LPAIF_I2S_CTLa : SPKR_MODE  / SPKR_MONO
	switch (ch_count) {
		case 1: return (SPKR_MONO   | T_I2S_SPKR_MODE_SD1);
		case 2: return (SPKR_STEREO | T_I2S_SPKR_MODE_SD1);
		case 4: return (SPKR_STEREO | T_I2S_SPKR_MODE_QUAD01);
	}
	return (SPKR_STEREO | T_I2S_SPKR_MODE_SD1);
}

static u32 mic_channel_to_fields(u32 ch_count)
{
#define MIC_MONO   (1<<3) //0x8
#define MIC_STEREO (0<<3)

#define T_I2S_MIC_MODE_SD0 (0x1 << 4) //0x10
#define T_I2S_MIC_MODE_SD1 (0x2 << 4) //0x20
#define T_I2S_MIC_MODE_QUAD01 (0x5 << 4) //0x50

	// LPASS_SDR_LPAIF_I2S_CTLa : MIC_MODE  / MIC_MONO
	switch (ch_count) {
		case 1: return (MIC_MONO   | T_I2S_MIC_MODE_SD0);
		case 2: return (MIC_STEREO | T_I2S_MIC_MODE_SD0);
		case 4: return (MIC_STEREO | T_I2S_MIC_MODE_QUAD01);
	}
	return (MIC_STEREO | T_I2S_MIC_MODE_SD0);
}

/* Configure i2s control register for mic operation */
static void configure_i2s_mic(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);
	u32 bit_ws_src = BIT(2);
	u32 bit_i2s_sel = BIT(0);
	u32 bit_i2s_reset = BIT(31);
	u32 bit_en_long_rate = BIT(24);
	u32 bit_depth = bit_depth_to_fields(hs_intf->i2s.bit_depth_val);
	u32 mic_mode_mono = mic_channel_to_fields(hs_intf->i2s.mic_ch_count_val);

	u32 i2s_ctl_bits = mic_mode_mono | bit_depth;
	i2s_ctl_bits |= bit_ws_src;
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() i2s_ctl_bits = 0x%x (should be 0x56)\n", __func__, i2s_ctl_bits);
	//i2s_ctl_bits = 0x56;
	setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, i2s_ctl_bits); //i2s_ctl, mic_mode | bit_ws_src | mic_channel_count | bit_dep

	if (hs_intf->i2s.en_long_rate) {
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, hs_intf->i2s.long_rate << 18);
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_en_long_rate);
	}

	clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel);  //i2s_sel, bit_i2s_sel
	setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset); //i2s_ctl, bit_i2s_reset
	clearbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset); //i2s_ctl, bit_i2s_reset
}

/* Configure i2s control register for speaker operation */
static void configure_i2s_spkr(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	u32 bit_depth = bit_depth_to_fields(hs_intf->i2s.bit_depth_val);
	u32 spkr_mode_mono = spkr_channel_to_fields(hs_intf->i2s.spkr_ch_count_val);

	u32 i2s_ctl_bits = spkr_mode_mono | bit_depth;
	u32 bit_i2s_sel = BIT(0);
	u32 bit_i2s_reset = BIT(31);
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() i2s_ctl_bits = 0x%x (should be 0x2802)\n", __func__, i2s_ctl_bits);
	//i2s_ctl_bits = 0x2802;
	setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, i2s_ctl_bits); //i2s_ctl, spkr_mode | spkr_channel_count | bit_depth

	clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel);  //i2s_sel, bit_i2s_sel
	setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset); //i2s_ctl, bit_i2s_reset
	clearbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset); //i2s_ctl, bit_i2s_reset
}

/* Configure pcm sync source */
static void configure_pcm_sync_src(int interface, u8 sync_src)
{
#define PCM_SYNC_EXT 0
#define PCM_SYNC_INT 1
	u32 hs_index = intf2hs(interface);
	u32 bit_sync_src = BIT(13);
	switch (sync_src) {
		case PCM_SYNC_EXT:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Setting pcm sync source as external\n");
			clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_sync_src);  //bit_sync_src
			break;
		case PCM_SYNC_INT:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Setting pcm sync source as internal\n");
			setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_sync_src); //bit_sync_src
			break;
		default:
			hsi2s_intf_log(interface, HSI2S_WARN, module, "Undefined sync source input. Setting default source(internal)\n");
			setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_sync_src); //bit_sync_src
			break;
	}
}

/* Configure pcm aux mode */
static void configure_pcm_aux_mode(int interface, u8 aux_mode)
{
#define PCM_AUXMODE_PCM 0
#define PCM_AUXMODE_AUX 1
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	u32 bit_aux_mode = BIT(12);
	u32 bit_tdm_inv_rpcm_sync = BIT(27);
	u32 bit_tdm_inv_tpcm_sync = BIT(28);
	switch (aux_mode) {
		case PCM_AUXMODE_PCM:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Setting PCM mode(short sync)\n");
			clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_aux_mode);
			break;
		case PCM_AUXMODE_AUX:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Setting AUX mode(long sync)\n");
			setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_aux_mode);
			if (hs_intf->pcm.tdm_inv_sync) {
				hsi2s_intf_log(interface, HSI2S_INFO, module, "Inverting frame sync pulses\n");
				setbits(lemans_reg.tdm_ctl + hs_index * 0x1000, bit_tdm_inv_rpcm_sync);
				setbits(lemans_reg.tdm_ctl + hs_index * 0x1000, bit_tdm_inv_tpcm_sync);
			}
			break;
		default:
			hsi2s_intf_log(interface, HSI2S_WARN, module, "Undefined AUX mode input. Setting default mode(PCM)\n");
			clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_aux_mode);
			break;
	}
}

/* Configure pcm rpcm width */
static void configure_pcm_rpcm_width(int interface, u8 rpcm_width)
{
#define RPCM_WIDTH_8 0
#define RPCM_WIDTH_16 1
	u32 hs_index = intf2hs(interface);
	u32 bit_rpcm_width = BIT(11);
	switch (rpcm_width) {
		case RPCM_WIDTH_8:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Setting RPCM width as 8 bits\n");
			clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_rpcm_width);
			break;
		case RPCM_WIDTH_16:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Setting RPCM width as 16 bits\n");
			setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_rpcm_width);
			break;
		default:
			hsi2s_intf_log(interface, HSI2S_WARN, module, "Undefined RPCM width input. Setting default width(16)\n");
			setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_rpcm_width);
			break;
	}
}

/* Configure pcm tpcm width */
static void configure_pcm_tpcm_width(int interface, u8 tpcm_width)
{
#define TPCM_WIDTH_8 0
#define TPCM_WIDTH_16 1
	u32 hs_index = intf2hs(interface);
	u32 bit_tpcm_width = BIT(10);
	switch (tpcm_width) {
		case TPCM_WIDTH_8:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Setting TPCM width as 8 bits\n");
			clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_tpcm_width);
			break;
		case TPCM_WIDTH_16:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Setting TPCM width as 16 bits\n");
			setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_tpcm_width);
			break;
		default:
			hsi2s_intf_log(interface, HSI2S_WARN, module, "Undefined TPCM width input. Setting default width(16)\n");
			setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_tpcm_width);
			break;
	}
}

static int cal_pcm_rate(u32 rate)
{
	switch (rate) {
		case 8: return 0x0;
		case 16: return 0x1;
		case 32: return 0x2;
		case 64: return 0x3;
		case 128: return 0x4;
		case 256: return 0x5;
	}
	return 0x5;
}

/* Configure PCM control register */
static void configure_pcm_ctl(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	u32 bit_ctrl_data_oe = BIT(18);
	setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, cal_pcm_rate(hs_intf->pcm.pcm_rate_val) << 15);
	setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_ctrl_data_oe); //bit_ctrl_data_oe
	configure_pcm_sync_src(interface, hs_intf->pcm.pcm_sync_src);
	configure_pcm_aux_mode(interface, hs_intf->pcm.pcm_aux_mode);
}

/* Configure PCM control register for tx operation */
static void configure_pcm_tx(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	u32 bit_pcm_reset_tx = BIT(27);
	configure_pcm_tpcm_width(interface, hs_intf->pcm.pcm_tpcm_width);
	setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset_tx);
	clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset_tx);
}

/* Configure PCM control register for rx operation */
static void configure_pcm_rx(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	u32 bit_pcm_reset_rx = BIT(28);
	configure_pcm_rpcm_width(interface, hs_intf->pcm.pcm_rpcm_width);
	setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset_rx);
	clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset_rx);
}

static u32 lemans_tdm_sync_delay(u32 cycles)
{
/*
Delay the data relative to the sync pulse.
0x0: DELAY_2_CYCLE - First data appears two cycles after frame pulse
0x1: DELAY_1_CYCLE - First data appears one cycle after frame pulse
0x2: DELAY_0_CYCLE - First data and frame pulse occur on the same cycle
*/
	switch(cycles) {
		case 0: return 0x2;
		case 1: return 0x1;
		case 2: return 0x0;
	}
	return 0x1;
}

/* Configure TDM control register */
static void configure_tdm_ctl(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	u32 bit_tdm_en = BIT(30);
	u32 bit_tdm_en_diff_sample_width = BIT(29);
	u32 tdm_sync_delay = lemans_tdm_sync_delay(hs_intf->pcm.tdm_sync_delay);
	setbits(lemans_reg.tdm_ctl + hs_index * 0x1000, (hs_intf->pcm.tdm_rate - 1) |
			(hs_intf->pcm.tdm_rpcm_width - 1) << 9 |
			(hs_intf->pcm.tdm_tpcm_width - 1) << 14 |
			tdm_sync_delay << 25);
	if (hs_intf->pcm.tdm_en_diff_sample_width) {
		setbits(lemans_reg.tdm_sample_width + hs_index * 0x1000, (hs_intf->pcm.tdm_tpcm_sample_width - 1) << 5 | (hs_intf->pcm.tdm_rpcm_sample_width - 1));
		setbits(lemans_reg.tdm_ctl + hs_index * 0x1000, bit_tdm_en_diff_sample_width);
	}
	setbits(lemans_reg.tdm_ctl + hs_index * 0x1000, bit_tdm_en);
}


static void lemans_configure_normal_mode(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);

	hs_intf->operation_mode = NORMAL;
	update_dma_config(interface);
	slave = interface;

	/* Reset the DMA registers */
	reset_rddma_registers(interface);
	reset_wrdma_registers(interface);

	u32 bit_i2s_sel = BIT(0);
	u32 bit_mic_en = BIT(9);
	u32 bit_pcm_en_rx = BIT(26);
	if (hs_intf->lpaif_mode == HS_I2S) {
		/* Reset I2S control register */
		reg_clear(lemans_reg.i2s_ctl + hs_index * 0x1000); //i2s_ctl
		/* Reset I2S select register */
		clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //i2s_sel, bit_i2s_sel

		/* Configure I2S control register */
		configure_i2s_spkr(interface);

		configure_i2s_mic(interface);
	} else {
		/* Reset PCM control register */
		reg_clear(lemans_reg.pcm_ctl + hs_index * 0x1000);
		/* Reset TDM control register */
		reg_clear(lemans_reg.tdm_ctl + hs_index * 0x1000);
		/* Set I2S select register */
		setbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //bit_i2s_sel
		configure_pcm_ctl(interface);
		if(hs_intf->pcm.tdm_en)
			configure_tdm_ctl(interface);
		configure_pcm_tx(interface);
		configure_pcm_rx(interface);
		/* Enable PCM slots for Rx and Tx */
		enable_rpcm_slot(interface);
		enable_tpcm_slot(interface);
		/* Set PCM lane configuration */
		lemans_set_pcm_lane_config(interface, hs_intf->pcm.lane_config);
	}

	/* Configure RDDMA registers */
	configure_rddma(interface);
	/* Configure WRDMA registers */
	configure_wrdma(interface);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	if (hs_intf->lpaif_mode == HS_I2S)
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_mic_en); //i2s_ctl, bit_mic_en
	else
		setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_en_rx); //pcm_ctl, bit_pcm_en_rx

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

/* Configure the I2S control register for internal loopback */
static void configure_i2s_int_lb(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);
	u32 bit_loopback = BIT(17);
	u32 bit_i2s_sel = BIT(0);
	u32 bit_i2s_reset = BIT(31);
	u32 bit_depth = bit_depth_to_fields(hs_intf->i2s.bit_depth_val);
	u32 spkr_mode_mono = spkr_channel_to_fields(hs_intf->i2s.spkr_ch_count_val);
	u32 mic_mode_mono = mic_channel_to_fields(hs_intf->i2s.mic_ch_count_val);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	u32 i2s_ctl_bits = spkr_mode_mono | mic_mode_mono | bit_depth;
	i2s_ctl_bits |= bit_loopback;

	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() i2s_ctl_bits = 0x%x(should be 0x22852)\n", __func__, i2s_ctl_bits);
	//i2s_ctl_bits = 0x22852;
	setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, i2s_ctl_bits); // i2s_ctl,  (spkr_mode | mic_mode | spkr_channel_count | mic_channel_count | bit_depth | bit_loopback)

	clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //i2s_sel, bit_i2s_sel
	setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset);  //i2s_ctl, bit_i2s_reset
	clearbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset);  // i2s_ctl, bit_i2s_reset
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

/* Configure the PCM control register for internal loopback */
static void configure_pcm_int_lb(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);
	u32 bit_pcm_loopback = BIT(14);
	u32 bit_pcm_reset = BIT(31);

	setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_loopback);
	configure_pcm_ctl(interface);
	if(hs_intf->pcm.tdm_en)
		configure_tdm_ctl(interface);
	configure_pcm_tx(interface);
	configure_pcm_rx(interface);
	setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset);
	clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset);
}


/* Configure the I2S control register for external loopback */
static void configure_i2s_ext_lb(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);
	u32 bit_i2s_sel = BIT(0);
	u32 bit_i2s_reset = BIT(31);
	u32 bit_depth = bit_depth_to_fields(hs_intf->i2s.bit_depth_val);
	u32 spkr_mode_mono = spkr_channel_to_fields(hs_intf->i2s.spkr_ch_count_val);
	u32 mic_mode_mono = mic_channel_to_fields(hs_intf->i2s.mic_ch_count_val);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);

	u32 i2s_ctl_bits = spkr_mode_mono | mic_mode_mono | bit_depth;
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() i2s_ctl_bits = 0x%x(should be 0x2852)\n", __func__, i2s_ctl_bits);
	//i2s_ctl_bits = 0x2852;
	setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, i2s_ctl_bits); // i2s_ctl,  (spkr_mode | mic_mode | spkr_channel_count | mic_channel_count | bit_depth)

	clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //i2s_sel, bit_i2s_sel
	setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset);  //i2s_ctl, bit_i2s_reset
	clearbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_i2s_reset);  // i2s_ctl, bit_i2s_reset
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

/* Configure the PCM control register for external loopback */
static void configure_pcm_ext_lb(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);
	u32 bit_pcm_reset = BIT(31);

	configure_pcm_ctl(interface);
	if(hs_intf->pcm.tdm_en)
		configure_tdm_ctl(interface);
	configure_pcm_tx(interface);
	configure_pcm_rx(interface);
	setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset);
	clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_reset);
}


static void lemans_configure_int_loopback_mode(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);

	hs_intf->operation_mode = INTERNAL_LB;

	update_dma_config(interface);
	/* Reset the DMA registers */
	reset_rddma_registers(interface);
	reset_wrdma_registers(interface);

	u32 bit_i2s_sel = BIT(0);
	u32 bit_mic_en = BIT(9);
	u32 bit_pcm_en_rx = BIT(26);
	if (hs_intf->lpaif_mode == HS_I2S) {
		/* Reset I2S control register */
		reg_clear(lemans_reg.i2s_ctl + hs_index * 0x1000); //i2s_ctl
		/* Reset I2S select register */
		clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //i2s_sel, bit_i2s_sel
		/* Configure I2S control register */
		configure_i2s_int_lb(interface);
	} else {
		/* Reset PCM control register */
		reg_clear(lemans_reg.pcm_ctl + hs_index * 0x1000); //pcm_ctl
		/* Reset TDM control register */
		reg_clear(lemans_reg.tdm_ctl + hs_index * 0x1000); //tdm_ctl
		/* Set I2S select register */
		setbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //i2s_sel, bit_i2s_sel
		/* Configure PCM control register */
		configure_pcm_int_lb(interface);
		/* Enable PCM slots for Rx and Tx */
		enable_rpcm_slot(interface);
		enable_tpcm_slot(interface);
		/* Set PCM lane configuration */
		lemans_set_pcm_lane_config(interface, hs_intf->pcm.lane_config);
	}
	/* Configure RDDMA registers */
	configure_rddma_int_lb(interface);
	/* Configure WRDMA registers */
	configure_wrdma_int_lb(interface);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	if (hs_intf->lpaif_mode == HS_I2S)
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_mic_en); //i2s_ctl, bit_mic_en
	else
		setbits(lemans_reg.pcm_ctl + hs_index * 0x1000 , bit_pcm_en_rx); //pcm_ctl, bit_pcm_en_rx
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void lemans_configure_ext_loopback_mode(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	/* Set operational mode */
	hs_intf->operation_mode = EXTERNAL_LB_MASTER;

	update_dma_config(interface);
	lemans_configure_muxmode(interface, 0);
	/* Reset the DMA registers */
	reset_rddma_registers(interface);
	reset_wrdma_registers(interface);

	u32 bit_i2s_sel = BIT(0);
	u32 bit_mic_en = BIT(9);
	u32 bit_pcm_en_rx = BIT(26);
	if (hs_intf->lpaif_mode == HS_I2S) {
		/* Reset I2S control register */
		reg_clear(lemans_reg.i2s_ctl + hs_index * 0x1000);
		/* Reset I2S select register */
		clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //bit_i2s_sel
		/* Configure I2S control register */
		configure_i2s_ext_lb(interface);
	} else {
		/* Reset PCM control register */
		reg_clear(lemans_reg.pcm_ctl + hs_index * 0x1000);
		/* Reset TDM control register */
		reg_clear(lemans_reg.tdm_ctl + hs_index * 0x1000);
		/* Set I2S select register */
		setbits(lemans_reg.i2s_sel + hs_index * 0x1000, bit_i2s_sel); //bit_i2s_sel
		/* Configure PCM control register */
		configure_pcm_ext_lb(interface);
		/* Enable PCM slots for Rx and Tx */
		enable_rpcm_slot(interface);
		enable_tpcm_slot(interface);
		/* Set PCM lane configuration */
		lemans_set_pcm_lane_config(interface, hs_intf->pcm.lane_config);
	}
	/* Configure WRDMA registers */
	configure_wrdma(interface);
	/* Configure RDDMA registers */
	configure_rddma(interface);
	/* Clear the IRQs */
	clear_irqs();
	/* Enable mic */
	if (hs_intf->lpaif_mode == HS_I2S)
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_mic_en); // bit_mic_en
	else
		setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_en_rx); // bit_pcm_en_rx

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void lemans_start_rddma(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	u32 hs_index = intf2hs(interface);
	u32 dma_index = intf2dma(interface);
	u32 bit_rddma_en = BIT(0);
	u32 bit_spkr_en = BIT(16);
	u32 bit_pcm_en_tx = BIT(25);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	/* Enable the DMA channel */
	setbits(lemans_reg.rddma_ctl + dma_index * 0x1000, bit_rddma_en); // bit_rddma_en
	/* Enable speaker */
	if (hs_intf->lpaif_mode == HS_I2S)
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_spkr_en); // bit_spkr_en
	else
		setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_en_tx); // bit_pcm_en_tx
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void lemans_stop_rddma(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);
	u32 dma_index = intf2dma(interface);
	u32 bit_rddma_en = BIT(0);
	u32 bit_spkr_en = BIT(16);
	u32 bit_pcm_en_tx = BIT(25);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	/* Disable speaker */
	if (hs_intf->lpaif_mode == HS_I2S)
		clearbits(lemans_reg.i2s_ctl + hs_index * 0x1000, bit_spkr_en); // bit_spkr_en
	else
		clearbits(lemans_reg.pcm_ctl + hs_index * 0x1000, bit_pcm_en_tx); // bit_pcm_en_tx
	/* Disable the DMA channel */
	clearbits(lemans_reg.rddma_ctl + dma_index * 0x1000, bit_rddma_en); // bit_rddma_en
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void lemans_set_master_clock(int interface, u32 value)
{
#define HS_BITCLK_UPDATE 0x1
#define HS_BITCLK_RESET 0x71F
	u32 hs_index = intf2hs(interface);
	u32 clk_update_reg;
	u32 clk_val_reg;

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	clk_update_reg = lemans_reg.HS_IF_0_CMD_RCGR + hs_index * 0x20;
	clk_val_reg = lemans_reg.HS_IF_0_CFG_RCGR + hs_index * 0x20;

	clearbits(clk_val_reg, HS_BITCLK_RESET);
	setbits(clk_val_reg, value);
	setbits(clk_update_reg, HS_BITCLK_UPDATE);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void lemans_set_slave(int interface, u32 value)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	struct hsi2s_interface *slave_intf = &hs_intfs[value];

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	hs_intf->operation_mode = EXTERNAL_LB_MASTER_SLAVE;
	slave_intf->operation_mode = EXTERNAL_LB_MASTER_SLAVE;
	slave = value;
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void lemans_config_as_speaker(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	update_dma_config(interface);

	/* Configure the interface registers */
	if (hs_intf->lpaif_mode == HS_I2S) {
		/* Reset I2S select register */
		clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, BIT(0)); // bit_i2s_sel
		configure_i2s_spkr(interface);
	} else {
		/* Set I2S select register */
		setbits(lemans_reg.i2s_sel + hs_index * 0x1000, BIT(0)); // bit_i2s_sel
		configure_pcm_ctl(interface);
		if (hs_intf->pcm.tdm_en)
			configure_tdm_ctl(interface);
		configure_pcm_tx(interface);
		/* Enable PCM slots for Tx */
		enable_tpcm_slot(interface);
		/* Set PCM lane configuration */
		lemans_set_pcm_lane_config(interface, hs_intf->pcm.lane_config);
	}
	configure_rddma(interface);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void lemans_config_as_mic(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 hs_index = intf2hs(interface);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	update_dma_config(interface);

	if (hs_intf->lpaif_mode == HS_I2S) {
		/* Reset I2S select register */
		clearbits(lemans_reg.i2s_sel + hs_index * 0x1000, BIT(0)); // bit_i2s_sel
		configure_i2s_mic(interface);
	} else {
		/* Set I2S select register */
		setbits(lemans_reg.i2s_sel + hs_index * 0x1000, BIT(0)); // bit_i2s_sel
		configure_pcm_ctl(interface);
		if (hs_intf->pcm.tdm_en)
			configure_tdm_ctl(interface);
		configure_pcm_rx(interface);
		/* Enable PCM slots for Rx */
		enable_rpcm_slot(interface);
		/* Set PCM lane configuration */
		lemans_set_pcm_lane_config(interface, hs_intf->pcm.lane_config);
	}
	configure_wrdma(interface);

	if (hs_intf->lpaif_mode == HS_I2S)
		setbits(lemans_reg.i2s_ctl + hs_index * 0x1000, BIT(9)); // bit_mic_en
	else
		setbits(lemans_reg.pcm_ctl + hs_index * 0x1000, BIT(26)); // bit_pcm_en_rx
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void lemans_configure_rate_detection(int block)
{
	//TODO
}

static void lemans_reset_rate_detection(int block)
{
	//TODO
	/*
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
	*/
}

/* Configure I2S parameters based on user input */
static int lemans_configure_i2s_params(int interface, struct i2s_params *params)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	return do_configure_i2s_params(&hs_intf->i2s, &hs_intf->dma, params);
}

/* Configure PCM parameters based on user input */
static int lemans_configure_pcm_params(int interface, struct pcm_params *params)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	return do_configure_pcm_params(&hs_intf->pcm, &hs_intf->dma, params);
}

/* Configure TDM parameters based on user input */
static int lemans_configure_tdm_params(int interface, struct tdm_params *params)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	return do_configure_tdm_params(&hs_intf->pcm, &hs_intf->dma, params);
}

static int valid_intf_count = 0;
static void lemans_init_interfaces(struct interface_config *config)
{
	int count = config->count;
	for(int i=0; i<count; i++) {
		int interface = config->intf[i].interface;
		u32 hs_index  = config->intf[i].hs_index;
		u32 dma_index = config->intf[i].dma_index;
		u32 txdmaaddr = config->intf[i].txdmaaddr;
		u32 rxdmaaddr = config->intf[i].rxdmaaddr;

		if (interface < 0 || interface  > 4 || hs_index > 4 || dma_index > 4) {
			hsi2s_log(HSI2S_ERROR, module, "!!!!!!!!!!interface = %d, hs_index = %u, dma_index = %u !!!!!!, return\n", interface, hs_index, dma_index);
			continue;
		}

		struct hsi2s_interface *hs_intf = &hs_intfs[interface];

		hs_intf->interface = interface;
		hs_intf->hs_index = hs_index;
		hs_intf->dma_index = dma_index;
		hs_intf->dma.rddma_base = txdmaaddr;
		hs_intf->dma.wrdma_base = rxdmaaddr;

		hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() hs_index = %u, dma_index = %u\n", __func__, hs_intf->hs_index, hs_intf->dma_index);
		hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() rddma_base = 0x%x\n", __func__, hs_intf->dma.rddma_base);
		hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() wrdma_base = 0x%x\n", __func__, hs_intf->dma.wrdma_base);

		if(hs_index == 4) {
			u32 val = 0;
			val =  read_paddr(lemans_reg.CORE_CC_I2S_IF_CTL);
			hsi2s_intf_log(interface, HSI2S_INFO, module, "the lpass core hsis ctl val 0x%x \n", val);
			write_paddr(lemans_reg.CORE_CC_I2S_IF_CTL, 0x10);
		}
		reset_registers(interface);
		lemans_configure_lpaif_mode(interface, HS_I2S);
		enable_rpcm_slot(interface);
		enable_tpcm_slot(interface);
		lemans_set_pcm_lane_config(interface, MULTI_LANE_RX);
		//configure_normal_mode(interface);
		valid_intf_count ++;
	}
	dma_buffer_length = config->dma_buffer_length;
}

static u32 lemans_get_wrdma_base(int interface)
{
	u32 dma_index = intf2dma(interface);
	u32 base_addr_phy = read_paddr(lemans_reg.wrdma_base + dma_index * 0x1000);
	return base_addr_phy;
}

static u32 lemans_get_wrdma_curr(int interface)
{
	u32 dma_index = intf2dma(interface);
	u32 curr_addr_phy = read_paddr(lemans_reg.wrdma_curr_addr + dma_index * 0x1000);
	return curr_addr_phy;
}

#define IRQ_PER_RDDMA_CH0                       BIT(0)
#define IRQ_UNDR_RDDMA_CH0                      BIT(1)
#define IRQ_ERR_RDDMA_CH0                       BIT(2)
#define IRQ_PER_RDDMA_CH1                       BIT(3)
#define IRQ_UNDR_RDDMA_CH1                      BIT(4)
#define IRQ_ERR_RDDMA_CH1                       BIT(5)
#define IRQ_PER_RDDMA_CH2                       BIT(6)
#define IRQ_UNDR_RDDMA_CH2                      BIT(7)
#define IRQ_ERR_RDDMA_CH2                       BIT(8)
#define IRQ_PER_RDDMA_CH3                       BIT(9)
#define IRQ_UNDR_RDDMA_CH3                      BIT(10)
#define IRQ_ERR_RDDMA_CH3                       BIT(11)
#define IRQ_PER_RDDMA_CH4                       BIT(12)
#define IRQ_UNDR_RDDMA_CH4                      BIT(13)
#define IRQ_ERR_RDDMA_CH4                       BIT(14)
#define IRQ_PER_WRDMA_CH0                       BIT(15)
#define IRQ_OVR_WRDMA_CH0                       BIT(16)
#define IRQ_ERR_WRDMA_CH0                       BIT(17)
#define IRQ_PER_WRDMA_CH1                       BIT(18)
#define IRQ_OVR_WRDMA_CH1                       BIT(19)
#define IRQ_ERR_WRDMA_CH1                       BIT(20)
#define IRQ_PER_WRDMA_CH2                       BIT(21)
#define IRQ_OVR_WRDMA_CH2                       BIT(22)
#define IRQ_ERR_WRDMA_CH2                       BIT(23)
#define IRQ_PER_WRDMA_CH3                       BIT(24)
#define IRQ_OVR_WRDMA_CH3                       BIT(25)
#define IRQ_ERR_WRDMA_CH3                       BIT(26)

#define IRQ2_PER_WRDMA_CH4                      BIT(4)
#define IRQ2_OVR_WRDMA_CH4                      BIT(5)
#define IRQ2_ERR_WRDMA_CH4                      BIT(6)
struct {
	u32 index;
	u32 pad;
	u32 rddma_per;
	u32 rddma_undr;
	u32 rddma_err;
	u32 wrdma_per;
	u32 wrdma_ovr;
	u32 wrdma_err;

} dma_bits[5] = {
	{
		.index = 0,
		.rddma_per = IRQ_PER_RDDMA_CH0,
		.rddma_undr = IRQ_UNDR_RDDMA_CH0,
		.rddma_err = IRQ_ERR_RDDMA_CH0,
		.wrdma_per = IRQ_PER_WRDMA_CH0,
		.wrdma_ovr = IRQ_OVR_WRDMA_CH0,
		.wrdma_err = IRQ_ERR_WRDMA_CH0,
	},
	{
		.index = 1,
		.rddma_per = IRQ_PER_RDDMA_CH1,
		.rddma_undr = IRQ_UNDR_RDDMA_CH1,
		.rddma_err = IRQ_ERR_RDDMA_CH1,
		.wrdma_per = IRQ_PER_WRDMA_CH1,
		.wrdma_ovr = IRQ_OVR_WRDMA_CH1,
		.wrdma_err = IRQ_ERR_WRDMA_CH1,
	},
	{
		.index = 2,
		.rddma_per = IRQ_PER_RDDMA_CH2,
		.rddma_undr = IRQ_UNDR_RDDMA_CH2,
		.rddma_err = IRQ_ERR_RDDMA_CH2,
		.wrdma_per = IRQ_PER_WRDMA_CH2,
		.wrdma_ovr = IRQ_OVR_WRDMA_CH2,
		.wrdma_err = IRQ_ERR_WRDMA_CH2,
	},
	{
		.index = 3,
		.rddma_per = IRQ_PER_RDDMA_CH3,
		.rddma_undr = IRQ_UNDR_RDDMA_CH3,
		.rddma_err = IRQ_ERR_RDDMA_CH3,
		.wrdma_per = IRQ_PER_WRDMA_CH3,
		.wrdma_ovr = IRQ_OVR_WRDMA_CH3,
		.wrdma_err = IRQ_ERR_WRDMA_CH3,
	},
	{
		.index = 4,
		.rddma_per = IRQ_PER_RDDMA_CH4,
		.rddma_undr = IRQ_UNDR_RDDMA_CH4,
		.rddma_err = IRQ_ERR_RDDMA_CH4,
		.wrdma_per = IRQ2_PER_WRDMA_CH4, //
		.wrdma_ovr = IRQ2_OVR_WRDMA_CH4, //
		.wrdma_err = IRQ2_ERR_WRDMA_CH4, //
	},
};

static u32 cmd_irq_per_rddma_ch[2] = {CMD_IRQ_PER_RDDMA_CH0, CMD_IRQ_PER_RDDMA_CH1};
static u32 cmd_irq_undr_rddma_ch[2] = {CMD_IRQ_UNDR_RDDMA_CH0, CMD_IRQ_UNDR_RDDMA_CH1};
static u32 cmd_irq_err_rddma_ch[2] = {CMD_IRQ_ERR_RDDMA_CH0, CMD_IRQ_ERR_RDDMA_CH1};

static u32 cmd_irq_per_wrdma_ch[2] = {CMD_IRQ_PER_WRDMA_CH0, CMD_IRQ_PER_WRDMA_CH1};
static u32 cmd_irq_ovr_wrdma_ch[2] = {CMD_IRQ_OVR_WRDMA_CH0, CMD_IRQ_OVR_WRDMA_CH1};
static u32 cmd_irq_err_wrdma_ch[2] = {CMD_IRQ_ERR_WRDMA_CH0, CMD_IRQ_ERR_WRDMA_CH1};

static u32 lemans_get_irq_stat(void)
{
	u32 irq_stat, irq2_stat;
	u32 cmds_irq_stat = 0;

	irq_stat = read_paddr(lemans_reg.irq_stat);
	irq2_stat = read_paddr(lemans_reg.irq2_stat);

	for (int i=0; i<valid_intf_count; i++) {
		u32 dma_index = intf2dma(i);
		//rddma
		if (irq_stat) {
			if (dma_bits[dma_index].rddma_per & irq_stat) {
				cmds_irq_stat |= cmd_irq_per_rddma_ch[i];
			}
			if (dma_bits[dma_index].rddma_undr & irq_stat) {
				cmds_irq_stat |= cmd_irq_undr_rddma_ch[i];
			}
			if (dma_bits[dma_index].rddma_err & irq_stat) {
				cmds_irq_stat |= cmd_irq_err_rddma_ch[i];
			}
		}
		//wrdma
		if (dma_index < 4) {
			if (irq_stat) {
				if (dma_bits[dma_index].wrdma_per & irq_stat) {
					cmds_irq_stat |= cmd_irq_per_wrdma_ch[i];
				}
				if (dma_bits[dma_index].wrdma_ovr & irq_stat) {
					cmds_irq_stat |= cmd_irq_ovr_wrdma_ch[i];
				}
				if (dma_bits[dma_index].wrdma_err & irq_stat) {
					cmds_irq_stat |= cmd_irq_err_wrdma_ch[i];
				}
			}
		} else {
			if (irq2_stat) {
				if (dma_bits[dma_index].wrdma_per & irq2_stat) {
					cmds_irq_stat |= cmd_irq_per_wrdma_ch[i];
				}
				if (dma_bits[dma_index].wrdma_ovr & irq2_stat) {
					cmds_irq_stat |= cmd_irq_ovr_wrdma_ch[i];
				}
				if (dma_bits[dma_index].wrdma_err & irq2_stat) {
					cmds_irq_stat |= cmd_irq_err_wrdma_ch[i];
				}
			}
		}
	}

	return cmds_irq_stat;
}

static void lemans_irq_clear_bits(u32 bits)
{
	for (int i=0; i<valid_intf_count; i++) {
		u32 dma_index = intf2dma(i);

		//rddma
		if (bits & cmd_irq_per_rddma_ch[i]) {
			setbits(lemans_reg.irq_clear, dma_bits[dma_index].rddma_per);
		}
		if (bits & cmd_irq_undr_rddma_ch[i]) {
			setbits(lemans_reg.irq_clear, dma_bits[dma_index].rddma_undr);
		}
		if (bits & cmd_irq_err_rddma_ch[i]) {
			setbits(lemans_reg.irq_clear, dma_bits[dma_index].rddma_err);
		}
		//wrdma
		if (bits & cmd_irq_per_wrdma_ch[i]) {
			if (dma_index < 4)
				setbits(lemans_reg.irq_clear, dma_bits[dma_index].wrdma_per);
			else
				setbits(lemans_reg.irq2_clear, IRQ2_PER_WRDMA_CH4);
		}
		if (bits & cmd_irq_ovr_wrdma_ch[i]) {
			if (dma_index < 4)
				setbits(lemans_reg.irq_clear, dma_bits[dma_index].wrdma_ovr);
			else
				setbits(lemans_reg.irq2_clear, IRQ2_OVR_WRDMA_CH4);
		}
		if (bits & cmd_irq_err_wrdma_ch[i]) {
			if (dma_index < 4)
				setbits(lemans_reg.irq_clear, dma_bits[dma_index].wrdma_err);
			else
				setbits(lemans_reg.irq2_clear, IRQ2_ERR_WRDMA_CH4);
		}
	}
}

#define get_irq_stat() lemans_get_irq_stat()
#define irq_clear_bits(bits) lemans_irq_clear_bits(bits)
static void lemans_interrupt(void (*notify)(int intf, int rw))
{
	u32 irq_stat;

        irq_stat = get_irq_stat();
        /* Periodic interrupt on read channel 0 */
        if (irq_stat & CMD_IRQ_PER_RDDMA_CH0) {
                irq_clear_bits(CMD_IRQ_PER_RDDMA_CH0);
                notify(0, 0);
        }
        /* Periodic interrupt on read channel 1 */
        if (irq_stat & CMD_IRQ_PER_RDDMA_CH1) {
                irq_clear_bits(CMD_IRQ_PER_RDDMA_CH1);
                notify(1, 0);
        }

        /* Periodic interrupt on write channel 0 */
        if (irq_stat & CMD_IRQ_PER_WRDMA_CH0) {
                irq_clear_bits(CMD_IRQ_PER_WRDMA_CH0);
                notify(0, 1);
        }
        /* Periodic interrupt on write channel 1 */
        if (irq_stat & CMD_IRQ_PER_WRDMA_CH1) {
                irq_clear_bits(CMD_IRQ_PER_WRDMA_CH1);
                notify(1, 1);
        }

        /* Error on read channel 0 */
        if (irq_stat & (CMD_IRQ_UNDR_RDDMA_CH0 | CMD_IRQ_ERR_RDDMA_CH0)) {
                hsi2s_intf_log(0, HSI2S_WARN, module, "Error on read DMA channel 0\n");
                if (irq_stat & CMD_IRQ_UNDR_RDDMA_CH0) {
                        irq_clear_bits(CMD_IRQ_UNDR_RDDMA_CH0);
                        hsi2s_intf_log(0, HSI2S_WARN, module, "Underrun detected\n");
                }
                if (irq_stat & CMD_IRQ_ERR_RDDMA_CH0) {
                        irq_clear_bits(CMD_IRQ_ERR_RDDMA_CH0);
                        hsi2s_intf_log(0, HSI2S_WARN, module, "Bus read error detected\n");
                }
        }
	/* Error on write channel 0 */
        if (irq_stat & (CMD_IRQ_OVR_WRDMA_CH0 | CMD_IRQ_ERR_WRDMA_CH0)) {
                hsi2s_intf_log(0, HSI2S_WARN, module, "Error on write DMA channel 0\n");
                if (irq_stat & CMD_IRQ_OVR_WRDMA_CH0) {
                        irq_clear_bits(CMD_IRQ_OVR_WRDMA_CH0);
                        hsi2s_intf_log(0, HSI2S_WARN, module, "Overrun detected\n");
                }
                if (irq_stat & CMD_IRQ_ERR_WRDMA_CH0) {
                        irq_clear_bits(CMD_IRQ_ERR_WRDMA_CH0);
                        hsi2s_intf_log(0, HSI2S_WARN, module, "Bus write error detected\n");
                }
        }
	/* Error on read channel 1 */
        if (irq_stat & (CMD_IRQ_UNDR_RDDMA_CH1 | CMD_IRQ_ERR_RDDMA_CH1)) {
                hsi2s_intf_log(1, HSI2S_WARN, module, "Error on read DMA channel 1\n");
                if (irq_stat & CMD_IRQ_UNDR_RDDMA_CH1) {
                        irq_clear_bits(CMD_IRQ_UNDR_RDDMA_CH1);
                        hsi2s_intf_log(1, HSI2S_WARN, module, "Underrun detected\n");
                }
                if (irq_stat & CMD_IRQ_ERR_RDDMA_CH1) {
                        irq_clear_bits(CMD_IRQ_ERR_RDDMA_CH1);
                        hsi2s_intf_log(1, HSI2S_WARN, module, "Bus read error detected\n");
                }
        }
        /* Error on write channel 1 */
        if (irq_stat & (CMD_IRQ_OVR_WRDMA_CH1 | CMD_IRQ_ERR_WRDMA_CH1)) {
                hsi2s_intf_log(1, HSI2S_WARN, module, "Error on write DMA channel 1\n");
                if (irq_stat & CMD_IRQ_OVR_WRDMA_CH1) {
                        irq_clear_bits(CMD_IRQ_OVR_WRDMA_CH1);
                        hsi2s_intf_log(1, HSI2S_WARN, module, "Overrun detected\n");
                }
                if (irq_stat & CMD_IRQ_ERR_WRDMA_CH1) {
                        irq_clear_bits(CMD_IRQ_ERR_WRDMA_CH1);
                        hsi2s_intf_log(1, HSI2S_WARN, module, "Bus write error detected\n");
                }
        }

        /* Rate detection */
}

static void lemans_set_reg_base(void * base[], const int count)
{
	set_lemans_reg_base(base, count);
}

static int lemans_enable_clock(int enable)
{
	printk("%s unpported\n", __func__);
	return 0;
}


struct target_ops lemans_ops = {
	.init_interfaces = lemans_init_interfaces,
	.configure_lpaif_mode = lemans_configure_lpaif_mode,
	.configure_muxmode = lemans_configure_muxmode,
	.reset_interface = lemans_reset_interface,
	.configure_normal_mode = lemans_configure_normal_mode,
	.configure_int_loopback_mode = lemans_configure_int_loopback_mode,
	.configure_ext_loopback_mode = lemans_configure_ext_loopback_mode ,
	.start_rddma = lemans_start_rddma,
	.stop_rddma = lemans_stop_rddma,
	.set_master_clock = lemans_set_master_clock,
	.set_slave = lemans_set_slave,
	.config_as_speaker = lemans_config_as_speaker,
	.config_as_mic = lemans_config_as_mic,
	.configure_rate_detection = lemans_configure_rate_detection,
	.reset_rate_detection = lemans_reset_rate_detection,

	.configure_i2s_params = lemans_configure_i2s_params,
	.configure_pcm_params = lemans_configure_pcm_params,
	.configure_tdm_params = lemans_configure_tdm_params,
	.set_pcm_lane_config = lemans_set_pcm_lane_config,
	.get_wrdma_base = lemans_get_wrdma_base,
	.get_wrdma_curr = lemans_get_wrdma_curr,
	.get_irq_stat = lemans_get_irq_stat,
	.irq_clear_bits = lemans_irq_clear_bits,

	.set_reg_base = lemans_set_reg_base,
	.enable_clock = lemans_enable_clock,
	.interrupt = lemans_interrupt,
};
