//SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/io.h> //writel_relaxed
#include "target_ops.h"
#include "hsi2s_param.h"
#include "log.h"

struct nord_reg {
        /* which interface start use for hsi2s */
        u32 intf_base;

        u32 HPASS_AUDIO_CC_AUD_INTF0_IBIT_CBCR;
        u32 HPASS_AUDIO_CC_AUD_INTF0_EBIT_CBCR;
        u32 HPASS_AUDIO_CC_AUD_INTF0_CLK_INV;
        u32 HPASS_AUDIO_CC_AUD_INTF0_MODE_MUXSEL;
        u32 HPASS_AUDIO_CC_AUD_INTF0_CMD_RCGR;
        u32 HPASS_AUDIO_CC_AUD_INTF0_CFG_RCGR;

        u32 HPASS_AUDIO_CC_AUD_INTF3_IBIT_CBCR;
        u32 HPASS_AUDIO_CC_AUD_INTF3_CLK_INV;
        u32 HPASS_AUDIO_CC_AUD_INTF3_MODE_MUXSEL;

        u32 HPASS_AUDIO_CC_AUD_INTF4_IBIT_CBCR;
        u32 HPASS_AUDIO_CC_AUD_INTF4_EBIT_CBCR;
        u32 HPASS_AUDIO_CC_AUD_INTF4_CLK_INV;
        u32 HPASS_AUDIO_CC_AUD_INTF4_MODE_MUXSEL;

        u32 HPASS_AUDIO_CC_AUD_INTF5_IBIT_CBCR;
        u32 HPASS_AUDIO_CC_AUD_INTF5_CLK_INV;
        u32 HPASS_AUDIO_CC_AUD_INTF5_MODE_MUXSEL;

        u32 HPASS_AUDIO_CC_PCMOE_CBCR;

        u32 QAIF_EEa_GRP_MAP;
        u32 QAIF_EEa_RDDMA_MAP;
        u32 QAIF_EEa_WRDMA_MAP;
        u32 QAIF_EEa_INTF_MAP;

	u32 QAIF_EEa_RDDMA_PERIOD_IRQ_EN;
	u32 QAIF_EEa_RDDMA_PERIOD_IRQ_STATUS;
	u32 QAIF_EEa_RDDMA_PERIOD_IRQ_RAW_STATUS;
	u32 QAIF_EEa_RDDMA_PERIOD_IRQ_CLEAR;
	u32 QAIF_EEa_RDDMA_UNDERFLOW_IRQ_EN;
	u32 QAIF_EEa_RDDMA_UNDERFLOW_IRQ_CLEAR;
	u32 QAIF_EEa_RDDMA_UNDERFLOW_IRQ_STATUS;
	u32 QAIF_EEa_RDDMA_ERROR_RSP_IRQ_EN;
	u32 QAIF_EEa_RDDMA_ERROR_RSP_IRQ_STATUS;
	u32 QAIF_EEa_RDDMA_ERROR_RSP_IRQ_CLEAR;

	u32 QAIF_EEa_WRDMA_PERIOD_IRQ_EN;
	u32 QAIF_EEa_WRDMA_PERIOD_IRQ_STATUS;
	u32 QAIF_EEa_WRDMA_PERIOD_IRQ_RAW_STATUS;
	u32 QAIF_EEa_WRDMA_PERIOD_IRQ_CLEAR;
	u32 QAIF_EEa_WRDMA_OVERFLOW_IRQ_EN;
	u32 QAIF_EEa_WRDMA_OVERFLOW_IRQ_STATUS;
	u32 QAIF_EEa_WRDMA_OVERFLOW_IRQ_CLEAR;
	u32 QAIF_EEa_WRDMA_ERROR_RSP_IRQ_EN;
	u32 QAIF_EEa_WRDMA_ERROR_RSP_IRQ_CLEAR;
	u32 QAIF_EEa_WRDMA_ERROR_RSP_IRQ_STATUS;

	u32 QAIF_WRDMA0_INTF_MAP;
	u32 QAIF_WRDMA3_INTF_MAP;
	u32 QAIF_WRDMA4_INTF_MAP;
	u32 QAIF_WRDMA5_INTF_MAP;
	u32 QAIF_WRDMA_MAPPED_TO_DDR;
	u32 QAIF_WRDMA_LOOPBACK_EN;
	u32 QAIF_WRDMAa_LOOPBACK_SEL;
	u32 QAIF_WRDMAa_SID_MAP;
	u32 QAIF_WRDMAa_DDR_SHRAM_START_ADDR;
	u32 QAIF_WRDMAa_DDR_SHRAM_LENGTH;
	u32 QAIF_WRDMAa_CFG ;
	u32 QAIF_WRDMAa_PERIOD_LENGTH;
	u32 QAIF_WRDMAa_BUFFER_LENGTH;
	u32 QAIF_WRDMAa_BASE_ADDR;
	u32 QAIF_WRDMAa_CURRENT_ADDR;

	u32 QAIF_RDDMA0_INTF_MAP;
	u32 QAIF_RDDMA3_INTF_MAP;
	u32 QAIF_RDDMA4_INTF_MAP;
	u32 QAIF_RDDMA5_INTF_MAP;
	u32 QAIF_RDDMA_MAPPED_TO_DDR;
	u32 QAIF_RDDMAa_SID_MAP;
	u32 QAIF_RDDMAa_DDR_SHRAM_START_ADDR;
	u32 QAIF_RDDMAa_DDR_SHRAM_LENGTH;
	u32 QAIF_RDDMAa_CFG;
	u32 QAIF_RDDMAa_PERIOD_LENGTH;
	u32 QAIF_RDDMAa_BUFFER_LENGTH;
	u32 QAIF_RDDMAa_BASE_ADDR;
	u32 QAIF_RDDMAa_CURRENT_ADDR;

	u32 QAIF_RDDMAa_PERIOD_DET_STATUS;
	u32 QAIF_RDDMAa_PERIOD_DET_STATUS_CLR;
	u32 QAIF_RDDMAa_DBG_STATUS;

	u32 QAIF_AUD_INTFa_SYNC_CFG;
	u32 QAIF_AUD_INTFa_BIT_WIDTH_CFG;
	u32 QAIF_AUD_INTFa_FRAME_CFG;
	u32 QAIF_AUD_INTFa_ACTV_SLOT_EN_TX;
	u32 QAIF_AUD_INTFa_ACTV_SLOT_EN_RX;
	u32 QAIF_AUD_INTFa_LANE_CFG;
	u32 QAIF_AUD_INTFa_MI2S_CFG;
	u32 QAIF_AUD_INTFa_CFG;
	u32 QAIF_AUD_INTFa_CHAR_CTL;
	u32 QAIF_AUD_INTFa_CHAR_CFG;
	u32 QAIF_AUD_INTFa_CHAR_DATA;
	u32 QAIF_AUD_INTFa_CHAR_DATA_EXT;
	u32 QAIF_AUD_INTFa_CHAR_SYNC;
	u32 QAIF_AUD_INTFa_INIT_DBG_STATUS;
	u32 QAIF_AUD_INTFa_TX_DBG_STATUS;
	u32 QAIF_AUD_INTFa_RX_DBG_STATUS;

	u32 QAIF_RDDMAa_CTL;
	u32 QAIF_WRDMAa_CTL;
	u32 QAIF_AUD_INTFa_CTL;
};

#define EE_INDEX 3 // 0 ~ 4
#define INTF_INDEX 4 //0 ~ 12
static struct nord_reg nord_reg = {
	.intf_base = INTF_INDEX,


	// HPASS CC clock and muxmode
	.HPASS_AUDIO_CC_AUD_INTF0_IBIT_CBCR 	= 0x501901C,
	.HPASS_AUDIO_CC_AUD_INTF0_EBIT_CBCR 	= 0x5019020,
	.HPASS_AUDIO_CC_AUD_INTF0_CLK_INV 	= 0x5011010,
	.HPASS_AUDIO_CC_AUD_INTF0_MODE_MUXSEL 	= 0x501100C,
	.HPASS_AUDIO_CC_AUD_INTF0_CMD_RCGR	= 0x5019004,
	.HPASS_AUDIO_CC_AUD_INTF0_CFG_RCGR	= 0x5019008,

/*
	.HPASS_AUDIO_CC_AUD_INTF2_IBIT_CBCR	= 0x501B01C,
	.HPASS_AUDIO_CC_AUD_INTF2_CLK_INV	= 0x5011028,
	.HPASS_AUDIO_CC_AUD_INTF2_MODE_MUXSEL	= 0x5011024,
*/

	.HPASS_AUDIO_CC_AUD_INTF3_IBIT_CBCR	= 0x501C01C,
	.HPASS_AUDIO_CC_AUD_INTF3_CLK_INV	= 0x5011034,
	.HPASS_AUDIO_CC_AUD_INTF3_MODE_MUXSEL	= 0x5011030,

	.HPASS_AUDIO_CC_AUD_INTF4_IBIT_CBCR	= 0x501D01C,
	.HPASS_AUDIO_CC_AUD_INTF4_EBIT_CBCR	= 0x501D020,
	.HPASS_AUDIO_CC_AUD_INTF4_CLK_INV	= 0x5011040,
	.HPASS_AUDIO_CC_AUD_INTF4_MODE_MUXSEL	= 0x501103C,

	.HPASS_AUDIO_CC_AUD_INTF5_IBIT_CBCR	= 0x501E01C,
	.HPASS_AUDIO_CC_AUD_INTF5_CLK_INV	= 0x501104C,
	.HPASS_AUDIO_CC_AUD_INTF5_MODE_MUXSEL	= 0x5011048,

	.HPASS_AUDIO_CC_PCMOE_CBCR		= 0x502A01C,


	// ee3 - (ee0,1,2 - belongs to DSP, ee3 is for APPS )
	.QAIF_EEa_GRP_MAP	= 0x4F01900 + (0x4 * EE_INDEX),
	.QAIF_EEa_RDDMA_MAP	= 0x4F01920 + (0x4 * EE_INDEX),
	.QAIF_EEa_WRDMA_MAP	= 0x4F01940 + (0x4 * EE_INDEX),
	.QAIF_EEa_INTF_MAP	= 0x4F01960 + (0x4 * EE_INDEX),

	.QAIF_EEa_RDDMA_PERIOD_IRQ_EN		= 0x4F58000 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_PERIOD_IRQ_STATUS	= 0x4F58008 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_PERIOD_IRQ_RAW_STATUS	= 0x4F58010 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_PERIOD_IRQ_CLEAR	= 0x4F58018 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_EN	= 0x4F58028 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_STATUS	= 0x4F58030 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_CLEAR	= 0x4F58040 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_EN	= 0x4F58050 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_CLEAR	= 0x4F58068 + (0x1000 * EE_INDEX),
	.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_STATUS	= 0x4F58058 + (0x1000 * EE_INDEX),

	.QAIF_EEa_WRDMA_PERIOD_IRQ_EN		= 0x4F58078 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_PERIOD_IRQ_STATUS	= 0x4F58080 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_PERIOD_IRQ_RAW_STATUS	= 0x4F58088 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_PERIOD_IRQ_CLEAR	= 0x4F58090 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_OVERFLOW_IRQ_EN		= 0x4F580A0 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_OVERFLOW_IRQ_STATUS	= 0x4F580A8 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_OVERFLOW_IRQ_CLEAR	= 0x4F580B8 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_EN	= 0x4F580C8 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_CLEAR	= 0x4F580E0 + (0x1000 * EE_INDEX),
	.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_STATUS	= 0x4F580D0 + (0x1000 * EE_INDEX),

	// WRDMA4 - 0x4000
	.QAIF_WRDMA0_INTF_MAP			= 0x4F000A0,
	.QAIF_WRDMA3_INTF_MAP			= 0x4F000AC,
	.QAIF_WRDMA4_INTF_MAP			= 0x4F000B0,
	.QAIF_WRDMA5_INTF_MAP			= 0x4F000B4,
	.QAIF_WRDMA_MAPPED_TO_DDR		= 0x4F01000,
	.QAIF_WRDMAa_SID_MAP			= 0x4F01980 + (0x4 * INTF_INDEX),
	.QAIF_WRDMA_LOOPBACK_EN			= 0x4F00204,
	.QAIF_WRDMAa_LOOPBACK_SEL		= 0x4F00208 + (0x4 * INTF_INDEX),
	.QAIF_WRDMAa_DDR_SHRAM_START_ADDR	= 0x4F01500 + (0x4 * INTF_INDEX),
	.QAIF_WRDMAa_DDR_SHRAM_LENGTH		= 0x4F01700 + (0x4 * INTF_INDEX),
	.QAIF_WRDMAa_CFG			= 0x4F38004 + (0x1000 * INTF_INDEX),
	.QAIF_WRDMAa_PERIOD_LENGTH		= 0x4F3801C + (0x1000 * INTF_INDEX),
	.QAIF_WRDMAa_BUFFER_LENGTH		= 0x4F38010 + (0x1000 * INTF_INDEX),
	.QAIF_WRDMAa_BASE_ADDR			= 0x4F38008 + (0x1000 * INTF_INDEX),
	.QAIF_WRDMAa_CURRENT_ADDR		= 0x4F38014 + (0x1000 * INTF_INDEX),

	// RDDMA4 + 0x4000
	.QAIF_RDDMA0_INTF_MAP			= 0x4F00020,
	.QAIF_RDDMA3_INTF_MAP			= 0x4F0002C,
	.QAIF_RDDMA4_INTF_MAP			= 0x4F00030,
	.QAIF_RDDMA5_INTF_MAP			= 0x4F00034,
	.QAIF_RDDMA_MAPPED_TO_DDR		= 0x4F01010,
	.QAIF_RDDMAa_SID_MAP			= 0x4F01A10 + (0x4 * INTF_INDEX),
	.QAIF_RDDMAa_DDR_SHRAM_START_ADDR	= 0x4F01100 + (0x4 * INTF_INDEX),
	.QAIF_RDDMAa_DDR_SHRAM_LENGTH		= 0x4F01300 + (0x4 * INTF_INDEX),
	.QAIF_RDDMAa_CFG			= 0x4F18004 + (0x1000 * INTF_INDEX),
	.QAIF_RDDMAa_PERIOD_LENGTH		= 0x4F1801C + (0x1000 * INTF_INDEX),
	.QAIF_RDDMAa_BUFFER_LENGTH		= 0x4F18010 + (0x1000 * INTF_INDEX),
	.QAIF_RDDMAa_BASE_ADDR			= 0x4F18008 + (0x1000 * INTF_INDEX),
	.QAIF_RDDMAa_CURRENT_ADDR		= 0x4F18014 + (0x1000 * INTF_INDEX),
	.QAIF_RDDMAa_PERIOD_DET_STATUS		= 0x4F18044 + (0x1000 * INTF_INDEX),
	.QAIF_RDDMAa_PERIOD_DET_STATUS_CLR	= 0x4F18048 + (0x1000 * INTF_INDEX),
	.QAIF_RDDMAa_DBG_STATUS			= 0x4F18FF0 + (0x1000 * INTF_INDEX),

	// INTF4 - + 0x4000
	.QAIF_AUD_INTFa_SYNC_CFG	= 0x4F08004 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_BIT_WIDTH_CFG	= 0x4F08008 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_FRAME_CFG	= 0x4F0800C + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_ACTV_SLOT_EN_TX	= 0x4F08010 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_ACTV_SLOT_EN_RX	= 0x4F08030 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_LANE_CFG	= 0x4F08050 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_MI2S_CFG	= 0x4F08054 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_CFG		= 0x4F08058 + (0x1000* INTF_INDEX),

	.QAIF_AUD_INTFa_CHAR_CTL	= 0x4F0805C + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_CHAR_CFG	= 0x4F08060 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_CHAR_DATA	= 0x4F08064 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_CHAR_DATA_EXT	= 0x4F08068 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_CHAR_SYNC	= 0x4F0806C + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_INIT_DBG_STATUS = 0x4F08FF0 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_TX_DBG_STATUS	= 0x4F08FF4 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_RX_DBG_STATUS	= 0x4F08FF8 + (0x1000* INTF_INDEX),


	.QAIF_RDDMAa_CTL	= 0x4F18000 + (0x1000* INTF_INDEX),
	.QAIF_WRDMAa_CTL	= 0x4F38000 + (0x1000* INTF_INDEX),
	.QAIF_AUD_INTFa_CTL	= 0x4F08000 + (0x1000* INTF_INDEX),
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
        { //qaif
                .paddr = 0x4F00000,
                .size = 0x60000,
                .vaddr = NULL,
        },
        { //hpass_audio_cc
                .paddr = 0x5011000,
                .size = 0x20000,
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
	printk("%s: paddr 0x%.8x, val 0x%.8x\n", __func__, paddr, val);
	for(int i=0; i< sizeof(regions)/sizeof(regions[0]); i++) {
		if (in_region(paddr, regions[i])) {
			vaddr = regions[i].vaddr;
			offset = paddr - regions[i].paddr;
			return write_reg(vaddr, offset, val);
		}
	}
        printk("paddr 0x%x out of Region\n", paddr);
}

#if 0
/* Clear the complete register */
static void do_clearallbits(u32 paddr)
{
	printk("reg_clear: paddr 0x%x\n", paddr);
        do_write_paddr(paddr, 0x0);
}
#endif

/* Set specific register bits */
static void do_setbits(u32 paddr, u32 val)
{
        u32 reg;

	printk("setbits: paddr 0x%x, val 0x%x\n", paddr, val);
        reg = do_read_paddr(paddr);
        reg |= val;
        do_write_paddr(paddr, reg);
}

/* Clear specific register bits */
static void do_clearbits(u32 paddr, u32 val)
{
        u32 reg;

	printk("clearbits: paddr 0x%x, val 0x%x\n", paddr, val);
        reg = do_read_paddr(paddr);
        reg &= ~val;
        do_write_paddr(paddr, reg);
}

void set_nord_reg_base(void * base[], const int count)
{
	if (sizeof(regions)/sizeof(regions[0]) != count) {
		printk("%s: count %d expected to %lu\n", __func__, count, sizeof(regions)/sizeof(regions[0]));
		return;
	}

        for(int i=0; i< sizeof(regions)/sizeof(regions[0]); i++) {
                regions[i].vaddr = base[i];
        }
        //read_all_interface_registers(0);

        printk("nord_reg.intf_base = %d \n", nord_reg.intf_base);
}

#define read_paddr(pa) do_read_paddr(pa)
#define write_paddr(pa, val) do_write_paddr(pa, val)
#define clearallbits(pa) do_clearallbits(pa)
#define setbits(pa, bits) do_setbits(pa, bits)
#define clearbits(pa, bits) do_clearbits(pa, bits)

#define DEFAULT_BUFF_LEN_BYTES   (4 * 1024 * 1024)
#define BYTES_PER_SAMPLE_NORD 8
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


static void clear_irqs(void)
{
	hsi2s_log(HSI2S_DEBUG, module, "%s() enter\n", __func__);
	write_paddr(nord_reg.QAIF_EEa_RDDMA_PERIOD_IRQ_CLEAR, 0xFFFFFFFF);
	write_paddr(nord_reg.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_CLEAR, 0xFFFFFFFF);
	write_paddr(nord_reg.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_CLEAR, 0xFFFFFFFF);

	write_paddr(nord_reg.QAIF_EEa_WRDMA_PERIOD_IRQ_CLEAR, 0xFFFFFFFF);
	write_paddr(nord_reg.QAIF_EEa_WRDMA_OVERFLOW_IRQ_CLEAR, 0xFFFFFFFF);
	write_paddr(nord_reg.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_CLEAR, 0xFFFFFFFF);

	hsi2s_log(HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void reset_rddma_registers(int interface)
{
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	write_paddr(nord_reg.QAIF_RDDMAa_CTL + interface * 0x1000, 0x10);
	write_paddr(nord_reg.QAIF_RDDMAa_CFG + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_RDDMAa_BASE_ADDR + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_RDDMAa_BUFFER_LENGTH + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_RDDMAa_PERIOD_LENGTH + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_RDDMAa_CTL + interface * 0x1000, 0x0);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void reset_wrdma_registers(int interface)
{
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	write_paddr(nord_reg.QAIF_WRDMAa_CTL + interface * 0x1000, 0x10);
	write_paddr(nord_reg.QAIF_WRDMAa_CFG + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_WRDMAa_BASE_ADDR + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_WRDMAa_BUFFER_LENGTH + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_WRDMAa_PERIOD_LENGTH + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_WRDMAa_CTL + interface * 0x1000, 0x0);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void reset_registers(int interface)
{
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	write_paddr(nord_reg.QAIF_AUD_INTFa_CTL  + interface * 0x1000, 0x11100);
	write_paddr(nord_reg.QAIF_AUD_INTFa_CTL  + interface * 0x1000, 0x0);
	clear_irqs();
	reset_rddma_registers(interface);

	reset_wrdma_registers(interface);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}




static void update_dma_config(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	u32 dma_buffer_length_words = ((dma_buffer_length / BYTES_PER_SAMPLE_NORD) - 1);

	hs_intf->dma.rddma_buff_len = dma_buffer_length_words;
	hs_intf->dma.rddma_per_len = ((dma_buffer_length_words + 1) / 2) - 1;

	hs_intf->dma.wrdma_buff_len = dma_buffer_length_words;
	if (hs_intf->operation_mode == NORMAL) {
		hs_intf->dma.wrdma_per_len = (hs_intf->dma.wrdma_periodic_length_bytes / BYTES_PER_SAMPLE_NORD) - 1; //0xeff
	} else {
		hs_intf->dma.wrdma_per_len = ((dma_buffer_length_words + 1) / 2) - 1;
	}
}

/* Configure the read DMA registers */
static void configure_rddma(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() operation_mode %d\n", __func__, hs_intf->operation_mode);

	int intf = nord_reg.intf_base + interface;
	write_paddr(nord_reg.QAIF_RDDMA0_INTF_MAP + intf * 0x4, intf);

	setbits(nord_reg.QAIF_RDDMA_MAPPED_TO_DDR, 0x1 << intf);
	write_paddr(nord_reg.QAIF_RDDMAa_DDR_SHRAM_START_ADDR + interface * 0x4, 0x4E * intf);
	write_paddr(nord_reg.QAIF_RDDMAa_DDR_SHRAM_LENGTH + interface * 0x4, 0x4E);
	write_paddr(nord_reg.QAIF_RDDMAa_SID_MAP + interface * 0x4, 0x1B); //
	write_paddr(nord_reg.QAIF_RDDMAa_CFG + interface * 0x1000, 0x11040004);

	write_paddr(nord_reg.QAIF_RDDMAa_PERIOD_LENGTH + interface * 0x1000, hs_intf->dma.rddma_per_len);
	write_paddr(nord_reg.QAIF_RDDMAa_BUFFER_LENGTH + interface * 0x1000, hs_intf->dma.rddma_buff_len);
	write_paddr(nord_reg.QAIF_RDDMAa_BASE_ADDR + interface * 0x1000, hs_intf->dma.rddma_base);// readBuf);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void configure_wrdma(int interface, int loopback)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	int intf = nord_reg.intf_base + interface;
	write_paddr(nord_reg.QAIF_WRDMA0_INTF_MAP + intf * 0x4, intf);

	setbits(nord_reg.QAIF_WRDMA_MAPPED_TO_DDR, 0x1 << intf);
	write_paddr(nord_reg.QAIF_WRDMAa_SID_MAP + interface * 0x4, 0x1B); //
	if( loopback == 1) {
		write_paddr(nord_reg.QAIF_WRDMAa_DDR_SHRAM_START_ADDR + interface * 0x4, 0x9D * intf);
		write_paddr(nord_reg.QAIF_WRDMAa_DDR_SHRAM_LENGTH + interface * 0x4, 0x9D);
	} else {
		write_paddr(nord_reg.QAIF_WRDMAa_DDR_SHRAM_START_ADDR + interface * 0x4, 0x100 * intf);
		write_paddr(nord_reg.QAIF_WRDMAa_DDR_SHRAM_LENGTH + interface * 0x4, 0x100);
	}
	write_paddr(nord_reg.QAIF_WRDMAa_CFG + interface * 0x1000, 0x11040100);

	if( loopback == 1) {
		setbits(nord_reg.QAIF_WRDMA_LOOPBACK_EN, 0x1 << intf);
		write_paddr(nord_reg.QAIF_WRDMAa_LOOPBACK_SEL + interface * 0x4, intf);
	} else {
		clearbits(nord_reg.QAIF_WRDMA_LOOPBACK_EN, 0x1 << intf);
		write_paddr(nord_reg.QAIF_WRDMAa_LOOPBACK_SEL + interface * 0x4, 0x3f);
	}

	write_paddr(nord_reg.QAIF_WRDMAa_PERIOD_LENGTH + interface * 0x1000, hs_intf->dma.wrdma_per_len);
	write_paddr(nord_reg.QAIF_WRDMAa_BUFFER_LENGTH + interface * 0x1000, hs_intf->dma.wrdma_buff_len);
	write_paddr(nord_reg.QAIF_WRDMAa_BASE_ADDR + interface * 0x1000, hs_intf->dma.wrdma_base); //writeBuf);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

#define configure_rddma_int_lb configure_rddma
#define configure_wrdma_int_lb configure_wrdma



#if 0
/* Enable RPCM slots */
static void enable_rpcm_slot(int interface)
{
}


/* Enable TPCM slots */
static void enable_tpcm_slot(int interface)
{
}
#endif

static void nord_configure_lpaif_mode(int interface, u32 mode)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	switch (mode) {
		case HS_I2S:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Configure LPAIF in HS-I2S mode\n");
			hs_intf->lpaif_mode = HS_I2S;
			break;
		case HS_PCM:
			hsi2s_intf_log(interface, HSI2S_INFO, module, "Configure LPAIF in HS-PCM mode\n");
			hs_intf->lpaif_mode = HS_PCM;
			break;
		default:
			hsi2s_intf_log(interface, HSI2S_WARN, module, "Undefined LPAIF mode. Defaulting to HS-I2S mode\n");
			hs_intf->lpaif_mode = HS_I2S;
			break;
	}
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}


/* Configure interface as master/slave */
static void nord_configure_muxmode(int interface, int mode)
{
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	int intf = nord_reg.intf_base + interface;
	if (mode ) {
		/* Configure slave */
		/* 0: master, 1: slave */
		write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_MODE_MUXSEL + intf * 0xC, 0x1);
	} else {
		/* Configure master */
		/* 0: master, 1: slave */
		write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_MODE_MUXSEL + intf * 0xC, 0x0);
	}
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}


static void nord_set_pcm_lane_config(int interface, u32 config)
{
}

static void nord_reset_interface(int interface)
{
	//struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	write_paddr(nord_reg.QAIF_AUD_INTFa_CTL  + interface * 0x1000, 0x11100);
	write_paddr(nord_reg.QAIF_AUD_INTFa_CTL  + interface * 0x1000, 0x0);
	reset_rddma_registers(interface);
	reset_wrdma_registers(interface);
	/* Clear IRQs */
	clear_irqs();
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

#if 0
/* Configure i2s control register for mic operation */
static void configure_i2s_mic(int interface)
{
	//struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	// QAIF_AUD_INTFa_BIT_WIDTH_CFG
	// QAIF_AUD_INTFa_MI2S_CFG
}

/* Configure i2s control register for speaker operation */
static void configure_i2s_spkr(int interface)
{
	//struct hsi2s_interface *hs_intf = &hs_intfs[interface];
}

/* Configure pcm sync source */
static void configure_pcm_sync_src(int interface, u8 sync_src)
{
}

/* Configure pcm aux mode */
static void configure_pcm_aux_mode(int interface, u8 aux_mode)
{
}

/* Configure pcm rpcm width */
static void configure_pcm_rpcm_width(int interface, u8 rpcm_width)
{
}

/* Configure pcm tpcm width */
static void configure_pcm_tpcm_width(int interface, u8 tpcm_width)
{
}


/* Configure PCM control register */
static void configure_pcm_ctl(int interface)
{
}

/* Configure PCM control register for tx operation */
static void configure_pcm_tx(int interface)
{
}

/* Configure PCM control register for rx operation */
static void configure_pcm_rx(int interface)
{
}

/* Configure TDM control register */
static void configure_tdm_ctl(int interface)
{
}
#endif

static void nord_configure_normal_mode(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	hs_intf->operation_mode = NORMAL;
	update_dma_config(interface);
	slave = interface;

	/* Reset the DMA registers */
	reset_rddma_registers(interface);
	reset_wrdma_registers(interface);

	int intf = nord_reg.intf_base + interface;
	hsi2s_intf_log(interface, HSI2S_INFO, module, "intf = %d !!!!!\n ", intf);


	/* 0: diable loopback , 1: wrdma loopback, 2: lane loopback, 3: npl loopback*/
	int loopback = 0;

	//  === exec_environment_init ===
	clearbits(nord_reg.QAIF_EEa_GRP_MAP, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_RDDMA_MAP, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_WRDMA_MAP, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_INTF_MAP, 0x1 << intf);

	//  === rumi_clock_command ===

	//  === clock_settings ===
	write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_IBIT_CBCR + intf * 0x1000, 0x01);
	write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_EBIT_CBCR + intf * 0x1000, 0x01);
	if (hs_intf->lpaif_mode == HS_I2S)
		write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_CLK_INV + intf * 0xC, 0x1);
	else
		write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_CLK_INV + intf * 0xC, 0x0);

	//do in function  nord_configure_muxmode
	write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_MODE_MUXSEL + intf * 0xC, 0x1);

	write_paddr(nord_reg.HPASS_AUDIO_CC_PCMOE_CBCR, 0x01);
	write_paddr(nord_reg.QAIF_AUD_INTFa_CFG + interface * 0x1000, 0x0);


	//  === write_dma_init ===
	configure_wrdma(interface, loopback);

	//  === read_dma_init ===
	//configure_rddma(interface);

	//  === aud_intf_init ===

	int slot_width = hs_intf->i2s.bit_depth_val;
	int mic_ch_count = hs_intf->i2s.mic_ch_count_val;
	hsi2s_intf_log(interface, HSI2S_INFO, module, "ch = %d, bit_depth= %d\n", mic_ch_count, hs_intf->i2s.bit_depth_val);
	if (hs_intf->lpaif_mode == HS_I2S)
		write_paddr(nord_reg.QAIF_AUD_INTFa_SYNC_CFG + interface * 0x1000, 0x1120);
	else
		write_paddr(nord_reg.QAIF_AUD_INTFa_SYNC_CFG + interface * 0x1000, 0x100);

	if (slot_width == 24 || slot_width == 25) {
		slot_width = 32;
	}
	write_paddr(nord_reg.QAIF_AUD_INTFa_BIT_WIDTH_CFG + interface * 0x1000, (slot_width-1)<<24 | (slot_width-1) <<16 | (slot_width-1) << 8 | (slot_width-1));

	write_paddr(nord_reg.QAIF_AUD_INTFa_FRAME_CFG + interface * 0x1000, slot_width * 2 -1);

	if (slot_width == 8) {
		write_paddr(nord_reg.QAIF_AUD_INTFa_MI2S_CFG + interface * 0x1000, 0x3);
	} else {
		write_paddr(nord_reg.QAIF_AUD_INTFa_MI2S_CFG + interface * 0x1000, 0x0);
	}

	write_paddr(nord_reg.QAIF_AUD_INTFa_ACTV_SLOT_EN_RX + interface * 0x1000, 0x3);

	if (mic_ch_count == 2) {
		write_paddr(nord_reg.QAIF_AUD_INTFa_LANE_CFG + interface * 0x1000, 0x101);
	} else {
		write_paddr(nord_reg.QAIF_AUD_INTFa_LANE_CFG + interface * 0x1000, 0x303);
	}

	//  === dma_irq ===
	clear_irqs();

	//setbits(nord_reg.QAIF_EEa_RDDMA_PERIOD_IRQ_EN, 0x1 << intf);
	//setbits(nord_reg.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_EN, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_WRDMA_PERIOD_IRQ_EN, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_WRDMA_OVERFLOW_IRQ_EN, 0x1 << intf);

	//setbits(nord_reg.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_EN, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_EN, 0x1 << intf);


	//  === run ===
	write_paddr(nord_reg.QAIF_WRDMAa_CTL + interface * 0x1000, 0x1); //enable wrdma
	write_paddr(nord_reg.QAIF_AUD_INTFa_CTL  + interface * 0x1000, 0x100);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

#if 0
/* Configure the I2S control register for internal loopback */
static void configure_i2s_int_lb(int interface)
{
}

/* Configure the PCM control register for internal loopback */
static void configure_pcm_int_lb(int interface)
{
}


/* Configure the I2S control register for external loopback */
static void configure_i2s_ext_lb(int interface)
{
}

/* Configure the PCM control register for external loopback */
static void configure_pcm_ext_lb(int interface)
{
}

#endif

static void nord_configure_loopback(int interface, int loopback)
{
        /* 0: diable loopback , 1: wrdma loopback, 2: lane loopback, 3: npl loopback*/

        struct hsi2s_interface *hs_intf = &hs_intfs[interface];
        int intf = nord_reg.intf_base + interface;
        hsi2s_intf_log(interface, HSI2S_INFO, module, "intf = %d !!!!!\n ", intf);

        //  === exec_environment_init ===
        clearbits(nord_reg.QAIF_EEa_GRP_MAP, 0x1 << intf);
        setbits(nord_reg.QAIF_EEa_RDDMA_MAP, 0x1 << intf);
        setbits(nord_reg.QAIF_EEa_WRDMA_MAP, 0x1 << intf);
        setbits(nord_reg.QAIF_EEa_INTF_MAP, 0x1 << intf);

        //  === rumi_clock_command ===

        //  === clock_settings ===
        write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_IBIT_CBCR + intf * 0x1000, 0x01);
        if (hs_intf->lpaif_mode == HS_I2S)
                write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_CLK_INV + intf * 0xC, 0x1);
        else
                write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_CLK_INV + intf * 0xC, 0x0);

        //do in function  nord_configure_muxmode
        //write_paddr(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_MODE_MUXSEL + intf * 0xC, 0x0);

        write_paddr(nord_reg.HPASS_AUDIO_CC_PCMOE_CBCR, 0x01);
        write_paddr(nord_reg.QAIF_AUD_INTFa_CFG + interface * 0x1000, 0x0);


        //  === write_dma_init ===
        configure_wrdma(interface, loopback);

        //  === read_dma_init ===
        configure_rddma(interface);

        //  === aud_intf_init ===

	int sample_width = hs_intf->i2s.bit_depth_val;
	if (hs_intf->lpaif_mode == HS_I2S)
		write_paddr(nord_reg.QAIF_AUD_INTFa_SYNC_CFG + interface * 0x1000, 0x121);
	else
		write_paddr(nord_reg.QAIF_AUD_INTFa_SYNC_CFG + interface * 0x1000, 0x101);

	if (sample_width == 8) {
		write_paddr(nord_reg.QAIF_AUD_INTFa_BIT_WIDTH_CFG + interface * 0x1000, 0x07070707);
		write_paddr(nord_reg.QAIF_AUD_INTFa_FRAME_CFG + interface * 0x1000, 0x07);
		write_paddr(nord_reg.QAIF_AUD_INTFa_MI2S_CFG + interface * 0x1000, 0x3);
	} else if (sample_width == 16) {
		write_paddr(nord_reg.QAIF_AUD_INTFa_BIT_WIDTH_CFG + interface * 0x1000, 0x0F0F0F0F);
		write_paddr(nord_reg.QAIF_AUD_INTFa_FRAME_CFG + interface * 0x1000, 0x0F);
		write_paddr(nord_reg.QAIF_AUD_INTFa_MI2S_CFG + interface * 0x1000, 0x0);
	} else if (sample_width == 32) {
		write_paddr(nord_reg.QAIF_AUD_INTFa_BIT_WIDTH_CFG + interface * 0x1000, 0x1F1F1F1F);
		write_paddr(nord_reg.QAIF_AUD_INTFa_FRAME_CFG + interface * 0x1000, 0x1F);
		write_paddr(nord_reg.QAIF_AUD_INTFa_MI2S_CFG + interface * 0x1000, 0x0);
	} else {
		write_paddr(nord_reg.QAIF_AUD_INTFa_BIT_WIDTH_CFG + interface * 0x1000, 0x1F1F1F1F);
		write_paddr(nord_reg.QAIF_AUD_INTFa_FRAME_CFG + interface * 0x1000, 0x1F);
		write_paddr(nord_reg.QAIF_AUD_INTFa_MI2S_CFG + interface * 0x1000, 0x0);
	}
	write_paddr(nord_reg.QAIF_AUD_INTFa_ACTV_SLOT_EN_TX + interface * 0x1000, 0x1);
	write_paddr(nord_reg.QAIF_AUD_INTFa_ACTV_SLOT_EN_RX + interface * 0x1000, 0x1);

	if (loopback == 2) {
		write_paddr(nord_reg.QAIF_AUD_INTFa_LANE_CFG + interface * 0x1000, 0x80000300);
	} else {
		write_paddr(nord_reg.QAIF_AUD_INTFa_LANE_CFG + interface * 0x1000, 0x300);
	}

	//  === dma_irq ===
	clear_irqs();

	setbits(nord_reg.QAIF_EEa_RDDMA_PERIOD_IRQ_EN, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_EN, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_WRDMA_PERIOD_IRQ_EN, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_WRDMA_OVERFLOW_IRQ_EN, 0x1 << intf);

	setbits(nord_reg.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_EN, 0x1 << intf);
	setbits(nord_reg.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_EN, 0x1 << intf);


	//  === run ===
	// for internal loopback: WRDDMA, then RDDMA
	write_paddr(nord_reg.QAIF_WRDMAa_CTL + interface * 0x1000, 0x1); //enable wrdma

	//enable rddma in function nord_start_rddma
}

static void nord_configure_int_loopback_mode(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);

	hs_intf->operation_mode = INTERNAL_LB;

	update_dma_config(interface);
	/* Reset the DMA registers */
	reset_rddma_registers(interface);
	reset_wrdma_registers(interface);

	/* 0: diable loopback , 1: wrdma loopback, 2: lane loopback, 3: npl loopback*/
	nord_configure_loopback(interface, 1);

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void nord_configure_ext_loopback_mode(int interface)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

        hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
        /* Set operational mode */
        hs_intf->operation_mode = EXTERNAL_LB_MASTER;

        update_dma_config(interface);
        nord_configure_muxmode(interface, 0);
        /* Reset the DMA registers */
        reset_rddma_registers(interface);
        reset_wrdma_registers(interface);

        /* 0: diable loopback , 1: wrdma loopback, 2: lane loopback, 3: npl loopback*/
        nord_configure_loopback(interface, 2);

        hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void nord_start_rddma(int interface)
{
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() enter\n", __func__);
	write_paddr(nord_reg.QAIF_RDDMAa_CTL  + interface * 0x1000, 0x1);
	write_paddr(nord_reg.QAIF_AUD_INTFa_CTL  + interface * 0x1000, 0x1);

	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() leave\n", __func__);
}

static void nord_stop_rddma(int interface)
{
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	write_paddr(nord_reg.QAIF_AUD_INTFa_CTL + interface * 0x1000, 0x0);
	write_paddr(nord_reg.QAIF_RDDMAa_CTL + interface * 0x1000, 0x0);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void nord_set_master_clock(int interface, u32 value)
{
#define HS_BITCLK_UPDATE 0x1
#define HS_BITCLK_RESET 0x71F
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() enter\n", __func__);
	int intf = nord_reg.intf_base + interface;

	clearbits(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_CFG_RCGR + intf * 0x1000, HS_BITCLK_RESET);
	setbits(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_CFG_RCGR + intf * 0x1000, value);
	setbits(nord_reg.HPASS_AUDIO_CC_AUD_INTF0_CMD_RCGR + intf * 0x1000, HS_BITCLK_UPDATE);
	hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() leave\n", __func__);
}

static void nord_set_slave(int interface, u32 value)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];
	struct hsi2s_interface *slave_intf = &hs_intfs[value];

	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	hs_intf->operation_mode = EXTERNAL_LB_MASTER_SLAVE;
	slave_intf->operation_mode = EXTERNAL_LB_MASTER_SLAVE;
	slave = value;
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void nord_config_as_speaker(int interface)
{
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	update_dma_config(interface);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void nord_config_as_mic(int interface)
{
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	update_dma_config(interface);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
}

static void nord_configure_rate_detection(int block)
{
	//TODO
}

static void nord_reset_rate_detection(int block)
{
	//TODO
}

/* Configure I2S parameters based on user input */
static int nord_configure_i2s_params(int interface, struct i2s_params *params)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	return do_configure_i2s_params(&hs_intf->i2s, &hs_intf->dma, params);
}

/* Configure PCM parameters based on user input */
static int nord_configure_pcm_params(int interface, struct pcm_params *params)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	return do_configure_pcm_params(&hs_intf->pcm, &hs_intf->dma, params);
}

/* Configure TDM parameters based on user input */
static int nord_configure_tdm_params(int interface, struct tdm_params *params)
{
	struct hsi2s_interface *hs_intf = &hs_intfs[interface];

	return do_configure_tdm_params(&hs_intf->pcm, &hs_intf->dma, params);
}

static void nord_init_interfaces(struct interface_config *config)
{
	int count = config->count;
	for(int i=0; i<count; i++) {
		int interface = config->intf[i].interface;
		//u32 hs_index  = config->intf[i].hs_index;
		//u32 dma_index = config->intf[i].dma_index;
		u32 txdmaaddr = config->intf[i].txdmaaddr;
		u32 rxdmaaddr = config->intf[i].rxdmaaddr;

		struct hsi2s_interface *hs_intf = &hs_intfs[interface];

		hs_intf->interface = interface;
		//hs_intf->hs_index = hs_index;
		//hs_intf->dma_index = dma_index;
		hs_intf->dma.rddma_base = txdmaaddr;
		hs_intf->dma.wrdma_base = rxdmaaddr;

		hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() rddma_base = 0x%x\n", __func__, hs_intf->dma.rddma_base);
		hsi2s_intf_log(interface, HSI2S_INFO, module, "%s() wrdma_base = 0x%x\n", __func__, hs_intf->dma.wrdma_base);

		reset_registers(interface);
		nord_configure_lpaif_mode(interface, HS_I2S);
		//enable_rpcm_slot(interface);
		//enable_tpcm_slot(interface);
		//nord_set_pcm_lane_config(interface, MULTI_LANE_RX);
		//configure_normal_mode(interface);
	}
	dma_buffer_length = config->dma_buffer_length;
}

static u32 nord_get_wrdma_base(int interface)
{
	u32 base_addr_phy = 0;
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	base_addr_phy = read_paddr(nord_reg.QAIF_WRDMAa_BASE_ADDR + interface * 0x1000);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
	return base_addr_phy;
}

static u32 nord_get_wrdma_curr(int interface)
{
	u32 curr_addr_phy = 0;
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() enter\n", __func__);
	curr_addr_phy = read_paddr(nord_reg.QAIF_WRDMAa_CURRENT_ADDR + interface * 0x1000);
	hsi2s_intf_log(interface, HSI2S_DEBUG, module, "%s() leave\n", __func__);
	return curr_addr_phy;
}

static u32 nord_get_irq_stat(void)
{
	u32 irq_stat = 0;
	int intf = nord_reg.intf_base;
	int intf_1 = intf + 1;

	u32 rddma_per_irq_status = read_paddr(nord_reg.QAIF_EEa_RDDMA_PERIOD_IRQ_STATUS);
	u32 rddma_undr_irq_status = read_paddr(nord_reg.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_STATUS);
	u32 rddma_err_irq_status =  read_paddr(nord_reg.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_STATUS);
	//u32 rddma_per_irq_raw_status = read_paddr(nord_reg.QAIF_EEa_RDDMA_PERIOD_IRQ_RAW_STATUS);


	u32 wrdma_per_irq_status = read_paddr(nord_reg.QAIF_EEa_WRDMA_PERIOD_IRQ_STATUS);
	u32 wrdma_ovr_irq_status =  read_paddr(nord_reg.QAIF_EEa_WRDMA_OVERFLOW_IRQ_STATUS);
	u32 wrdma_err_irq_status =  read_paddr(nord_reg.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_STATUS);
	//u32 wrdma_per_irq_raw_satus = read_paddr(nord_reg.QAIF_EEa_WRDMA_PERIOD_IRQ_RAW_STATUS);

	//rddma
	if (rddma_per_irq_status & (1<<intf)) {
		irq_stat |= CMD_IRQ_PER_RDDMA_CH0;
	}
	if (rddma_per_irq_status & (1<<intf_1)) {
		irq_stat |= CMD_IRQ_PER_RDDMA_CH1;
	}

	if (rddma_undr_irq_status & (1<<intf)) {
		irq_stat |= CMD_IRQ_UNDR_RDDMA_CH0;
	}
	if (rddma_undr_irq_status & (1<<intf_1)) {
		irq_stat |= CMD_IRQ_UNDR_RDDMA_CH1;
	}

	if (rddma_err_irq_status & (1<<intf)) {
		irq_stat |= CMD_IRQ_ERR_RDDMA_CH0;
	}
	if (rddma_err_irq_status & (1<<intf_1)) {
		irq_stat |= CMD_IRQ_ERR_RDDMA_CH1;
	}


	//wrdma
	if (wrdma_per_irq_status & (1<<intf)) {
		irq_stat |= CMD_IRQ_PER_WRDMA_CH0;
	}
	if (wrdma_per_irq_status & (1<<intf_1)) {
		irq_stat |= CMD_IRQ_PER_WRDMA_CH1;
	}

	if (wrdma_ovr_irq_status & (1<<intf)) {
		irq_stat |= CMD_IRQ_OVR_WRDMA_CH0;
	}
	if (wrdma_ovr_irq_status & (1<<intf_1)) {
		irq_stat |= CMD_IRQ_OVR_WRDMA_CH1;
	}

	if (wrdma_err_irq_status & (1<<intf)) {
		irq_stat |= CMD_IRQ_ERR_WRDMA_CH0;
	}
	if (wrdma_err_irq_status & (1<<intf_1)) {
		irq_stat |= CMD_IRQ_ERR_WRDMA_CH2;
	}

	return irq_stat;
}

static void nord_irq_clear_bits(u32 bits)
{
	int intf = nord_reg.intf_base;
	int intf_1 = intf + 1;
	//rddma
	if (bits & CMD_IRQ_PER_RDDMA_CH0) {
		write_paddr(nord_reg.QAIF_EEa_RDDMA_PERIOD_IRQ_CLEAR, 1<<intf);
	}
	if (bits & CMD_IRQ_PER_RDDMA_CH1) {
		write_paddr(nord_reg.QAIF_EEa_RDDMA_PERIOD_IRQ_CLEAR, 1<<intf_1);
	}

	if (bits & CMD_IRQ_UNDR_RDDMA_CH0) {
		write_paddr(nord_reg.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_CLEAR, 1<<intf);
	}
	if (bits & CMD_IRQ_UNDR_RDDMA_CH1) {
		write_paddr(nord_reg.QAIF_EEa_RDDMA_UNDERFLOW_IRQ_CLEAR, 1<<intf_1);
	}

	if (bits & CMD_IRQ_ERR_RDDMA_CH0) {
		write_paddr(nord_reg.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_CLEAR, 1<<intf);
	}
	if (bits & CMD_IRQ_ERR_RDDMA_CH1) {
		write_paddr(nord_reg.QAIF_EEa_RDDMA_ERROR_RSP_IRQ_CLEAR, 1<<intf_1);
	}

	//wrdma
	if (bits & CMD_IRQ_PER_WRDMA_CH0) {
		write_paddr(nord_reg.QAIF_EEa_WRDMA_PERIOD_IRQ_CLEAR, 1<<intf);
	}
	if (bits & CMD_IRQ_PER_WRDMA_CH1) {
		write_paddr(nord_reg.QAIF_EEa_WRDMA_PERIOD_IRQ_CLEAR, 1<<intf_1);
	}

	if (bits & CMD_IRQ_OVR_WRDMA_CH0) {
		write_paddr(nord_reg.QAIF_EEa_WRDMA_OVERFLOW_IRQ_CLEAR, 1<<intf);
	}
	if (bits & CMD_IRQ_OVR_WRDMA_CH1) {
		write_paddr(nord_reg.QAIF_EEa_WRDMA_OVERFLOW_IRQ_CLEAR, 1<<intf_1);
	}

	if (bits & CMD_IRQ_ERR_WRDMA_CH0) {
		write_paddr(nord_reg.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_CLEAR, 1<<intf);
	}
	if (bits & CMD_IRQ_ERR_WRDMA_CH1) {
		write_paddr(nord_reg.QAIF_EEa_WRDMA_ERROR_RSP_IRQ_CLEAR, 1<<intf_1);
	}
}

#define get_irq_stat() nord_get_irq_stat()
#define irq_clear_bits(bits) nord_irq_clear_bits(bits)
static void nord_interrupt(void (*notify)(int intf, int rw))
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

static void nord_set_reg_base(void * base[], const int count)
{
	set_nord_reg_base(base, count);
}

struct target_ops nord_ops = {
	.init_interfaces = nord_init_interfaces,
	.configure_lpaif_mode = nord_configure_lpaif_mode,
	.configure_muxmode = nord_configure_muxmode,
	.reset_interface = nord_reset_interface,
	.configure_normal_mode = nord_configure_normal_mode,
	.configure_int_loopback_mode = nord_configure_int_loopback_mode,
	.configure_ext_loopback_mode = nord_configure_ext_loopback_mode ,
	.start_rddma = nord_start_rddma,
	.stop_rddma = nord_stop_rddma,
	.set_master_clock = nord_set_master_clock,
	.set_slave = nord_set_slave,
	.config_as_speaker = nord_config_as_speaker,
	.config_as_mic = nord_config_as_mic,
	.configure_rate_detection = nord_configure_rate_detection,
	.reset_rate_detection = nord_reset_rate_detection,

	.configure_i2s_params = nord_configure_i2s_params,
	.configure_pcm_params = nord_configure_pcm_params,
	.configure_tdm_params = nord_configure_tdm_params,
	.set_pcm_lane_config = nord_set_pcm_lane_config,
	.get_wrdma_base = nord_get_wrdma_base,
	.get_wrdma_curr = nord_get_wrdma_curr,
	.get_irq_stat = nord_get_irq_stat,
	.irq_clear_bits = nord_irq_clear_bits,
	.interrupt = nord_interrupt,

	.set_reg_base = nord_set_reg_base,
};

