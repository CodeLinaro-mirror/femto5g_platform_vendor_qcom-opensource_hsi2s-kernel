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

/* HS-I2S core structure */
static struct hsi2s_core *hsi2s_core;

/* Module parameters */
static u32 wrdma_periodic_length;
module_param(wrdma_periodic_length, uint, 0644);
MODULE_PARM_DESC(wrdma_periodic_length, "Periodic length for write channel in KB");

static int operation_mode;
module_param(operation_mode, int, 0644);
MODULE_PARM_DESC(operation_mode, "Default operation mode");

/* Register callbacks */

/* Map the register memory regions */
static void map_registers(struct hsi2s_device *hs_dev, int intf)
{
	hs_dev->i2s_ctl = hsi2s_core->lpaif_base_va + LPAIF_I2S_CTL +
				(0x1000 * intf);
	hs_dev->i2s_sel = hsi2s_core->lpaif_base_va + LPAIF_PCM_I2S_SEL +
				(0x1000 * intf);
	hs_dev->rddma_ctl = hsi2s_core->lpaif_base_va +
					     LPAIF_RDDMA_CTL +
					     (0x1000 * intf);
	hs_dev->rddma_base = hsi2s_core->lpaif_base_va +
					     LPAIF_RDDMA_BASE +
					     (0x1000 * intf);
	hs_dev->rddma_buff_len = hsi2s_core->lpaif_base_va +
					     LPAIF_RDDMA_BUFF_LEN +
					     (0x1000 * intf);
	hs_dev->rddma_curr_addr = hsi2s_core->lpaif_base_va +
					     LPAIF_RDDMA_CURR_ADDR +
					     (0x1000 * intf);
	hs_dev->rddma_per_len = hsi2s_core->lpaif_base_va +
					     LPAIF_RDDMA_PER_LEN +
					     (0x1000 * intf);
	hs_dev->wrdma_ctl = hsi2s_core->lpaif_base_va +
					     LPAIF_WRDMA_CTL +
					     (0x1000 * intf);
	hs_dev->wrdma_base = hsi2s_core->lpaif_base_va +
					     LPAIF_WRDMA_BASE +
					     (0x1000 * intf);
	hs_dev->wrdma_buff_len = hsi2s_core->lpaif_base_va +
					     LPAIF_WRDMA_BUFF_LEN +
					     (0x1000 * intf);
	hs_dev->wrdma_curr_addr = hsi2s_core->lpaif_base_va +
					     LPAIF_WRDMA_CURR_ADDR +
					     (0x1000 * intf);
	hs_dev->wrdma_per_len = hsi2s_core->lpaif_base_va +
					     LPAIF_WRDMA_PER_LEN +
					     (0x1000 * intf);
	hs_dev->irq_en = hsi2s_core->lpaif_base_va + LPAIF_IRQ_EN;
	hs_dev->irq_stat = hsi2s_core->lpaif_base_va + LPAIF_IRQ_STAT;
	hs_dev->irq_clear = hsi2s_core->lpaif_base_va + LPAIF_IRQ_CLEAR;
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
static void clear_irqs(struct hsi2s_device *hs_dev)
{
	writel_relaxed(0xFFFFFFFF, hs_dev->irq_clear);
}

/* Reset the registers */
static void reset_registers(struct hsi2s_device *hs_dev)
{
	reg_clear(hs_dev->i2s_ctl);
	setbits(hs_dev->i2s_ctl, I2S_LONG_RATE |
				 I2S_SPKR_MODE_QUAD01 |
				 I2S_MIC_MODE_QUAD01 |
				 I2S_LOOPBACK);
	clear_irqs(hs_dev);
	reg_clear(hs_dev->rddma_ctl);
	reg_clear(hs_dev->rddma_base);
	reg_clear(hs_dev->rddma_buff_len);
	reg_clear(hs_dev->rddma_curr_addr);
	reg_clear(hs_dev->rddma_per_len);
	setbits(hs_dev->rddma_ctl, RDDMA_RESET);
	clearbits(hs_dev->rddma_ctl, RDDMA_RESET);

	msleep(1000);

	reg_clear(hs_dev->wrdma_ctl);
	reg_clear(hs_dev->wrdma_base);
	reg_clear(hs_dev->wrdma_buff_len);
	reg_clear(hs_dev->wrdma_curr_addr);
	reg_clear(hs_dev->wrdma_per_len);
	setbits(hs_dev->wrdma_ctl, WRDMA_RESET);
	clearbits(hs_dev->wrdma_ctl, WRDMA_RESET);

	msleep(1000);

	setbits(hs_dev->i2s_ctl, I2S_RESET);
	clearbits(hs_dev->i2s_ctl, I2S_RESET);
}

/* Reset the read DMA registers */
static void reset_rddma_registers(struct hsi2s_device *hs_dev)
{
	reg_clear(hs_dev->rddma_ctl);
	reg_clear(hs_dev->rddma_base);
	reg_clear(hs_dev->rddma_buff_len);
	reg_clear(hs_dev->rddma_curr_addr);
	reg_clear(hs_dev->rddma_per_len);
	setbits(hs_dev->rddma_ctl, RDDMA_RESET);
	clearbits(hs_dev->rddma_ctl, RDDMA_RESET);
}

/* Reset the write DMA registers */
static void reset_wrdma_registers(struct hsi2s_device *hs_dev)
{
	reg_clear(hs_dev->wrdma_ctl);
	reg_clear(hs_dev->wrdma_base);
	reg_clear(hs_dev->wrdma_buff_len);
	reg_clear(hs_dev->wrdma_curr_addr);
	reg_clear(hs_dev->wrdma_per_len);
	setbits(hs_dev->wrdma_ctl, WRDMA_RESET);
	clearbits(hs_dev->wrdma_ctl, WRDMA_RESET);
}

/* Configure the read DMA registers */
static void configure_rddma(struct hsi2s_device *hs_dev, int intf)
{
	reg_clear(hs_dev->i2s_ctl);
	setbits(hs_dev->i2s_ctl, I2S_LONG_RATE |
				 I2S_SPKR_MODE_QUAD01 |
				 I2S_MIC_MODE_QUAD01 |
				 I2S_LOOPBACK);
	clearbits(hs_dev->i2s_sel, I2S_SEL);
	setbits(hs_dev->i2s_ctl, I2S_RESET);
	clearbits(hs_dev->i2s_ctl, I2S_RESET);

	if (!intf) {
		setbits(hs_dev->rddma_ctl, RDDMA_BURST_EN |
					   RDDMA_DYN_CLK |
					   RDDMA_WPSCNT_TWO |
					   RDDMA_PRI_AUDIO_INTF |
					   RDDMA_FIFO_WM_8);
		setbits(hs_dev->irq_en, IRQ_PER_RDDMA_CH0 |
					IRQ_UNDR_RDDMA_CH0 |
					IRQ_ERR_RDDMA_CH0);
		pr_warn("[HSI2S] Configured rddma channel for sdr0");
	} else {
		setbits(hs_dev->rddma_ctl, RDDMA_BURST_EN |
					   RDDMA_DYN_CLK |
					   RDDMA_WPSCNT_TWO |
					   RDDMA_SEC_AUDIO_INTF |
					   RDDMA_FIFO_WM_8);
		setbits(hs_dev->irq_en, IRQ_PER_RDDMA_CH1 |
					IRQ_UNDR_RDDMA_CH1 |
					IRQ_ERR_RDDMA_CH1);
		pr_warn("[HSI2S] Configured rddma channel for sdr1");
	}
}

/* Configure the write DMA registers */
static void configure_wrdma(struct hsi2s_device *hs_dev, int intf)
{
	reg_clear(hs_dev->i2s_ctl);
	setbits(hs_dev->i2s_ctl, I2S_LONG_RATE |
				 I2S_MIC_MODE_QUAD01 |
				 I2S_WS_SRC |
				 I2S_BIT_WIDTH_32);
	clearbits(hs_dev->i2s_sel, I2S_SEL);
	setbits(hs_dev->i2s_ctl, I2S_RESET);
	clearbits(hs_dev->i2s_ctl, I2S_RESET);

	writel_relaxed(virt_to_phys(hs_dev->lpass_wrdma_start),
		       hs_dev->wrdma_base);
	writel_relaxed(DEFAULT_BUFF_LEN_WORDS, hs_dev->wrdma_buff_len);
	writel_relaxed(wrdma_periodic_length, hs_dev->wrdma_per_len);

	if (!intf) {
		setbits(hs_dev->wrdma_ctl, WRDMA_DYN_CLK |
					   WRDMA_WPSCNT_FOUR |
					   WRDMA_PRI_AUDIO_INTF |
					   WRDMA_FIFO_WM_8);
		setbits(hs_dev->irq_en, IRQ_PER_WRDMA_CH0 |
					IRQ_OVR_WRDMA_CH0 |
					IRQ_ERR_WRDMA_CH0);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr0");
	} else {
		setbits(hs_dev->wrdma_ctl, WRDMA_DYN_CLK |
					   WRDMA_WPSCNT_FOUR |
					   WRDMA_SEC_AUDIO_INTF |
					   WRDMA_FIFO_WM_8);
		setbits(hs_dev->irq_en, IRQ_PER_WRDMA_CH1 |
					IRQ_OVR_WRDMA_CH1 |
					IRQ_ERR_WRDMA_CH1);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr1");
	}

	setbits(hs_dev->wrdma_ctl, WRDMA_EN);
}

/* Configure the write DMA registers for internal loopback */
static void configure_wrdma_lb(struct hsi2s_device *hs_dev, int intf)
{
	reg_clear(hs_dev->i2s_ctl);
	setbits(hs_dev->i2s_ctl, I2S_LONG_RATE |
				 I2S_SPKR_MODE_QUAD01 |
				 I2S_MIC_MODE_QUAD01 |
				 I2S_LOOPBACK);
	clearbits(hs_dev->i2s_sel, I2S_SEL);
	setbits(hs_dev->i2s_ctl, I2S_RESET);
	clearbits(hs_dev->i2s_ctl, I2S_RESET);

	writel_relaxed(virt_to_phys(hs_dev->lpass_wrdma_start),
		       hs_dev->wrdma_base);
	writel_relaxed(DEFAULT_BUFF_LEN_WORDS, hs_dev->wrdma_buff_len);
	writel_relaxed(wrdma_periodic_length, hs_dev->wrdma_per_len);

	if (!intf) {
		setbits(hs_dev->wrdma_ctl, WRDMA_DYN_CLK |
					   WRDMA_BURST_EN |
					   WRDMA_WPSCNT_TWO |
					   WRDMA_LOOPBACK_CH0 |
					   WRDMA_FIFO_WM_8);
		setbits(hs_dev->irq_en, IRQ_PER_WRDMA_CH0 |
					IRQ_OVR_WRDMA_CH0 |
					IRQ_ERR_WRDMA_CH0);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr0");
	} else {
		setbits(hs_dev->wrdma_ctl, WRDMA_DYN_CLK |
					   WRDMA_BURST_EN |
					   WRDMA_WPSCNT_TWO |
					   WRDMA_LOOPBACK_CH1 |
					   WRDMA_FIFO_WM_8);
		setbits(hs_dev->irq_en, IRQ_PER_WRDMA_CH1 |
					IRQ_OVR_WRDMA_CH1 |
					IRQ_ERR_WRDMA_CH1);
		pr_warn("[HSI2S] Enabling wrdma channel for sdr1");
	}

	setbits(hs_dev->wrdma_ctl, WRDMA_EN);
}

/* Function to configure HS-I2S registers in normal mode */
static void configure_normal_mode(struct hsi2s_device *hs_dev, int intf)
{
	pr_warn("[HSI2S] Configuring normal mode operation");
	/* Set operational mode */
	hs_dev->mode = 1;
	/* Reset the DMA registers */
	reset_rddma_registers(hs_dev);
	reset_wrdma_registers(hs_dev);
	/* Configure RDDMA registers */
	configure_rddma(hs_dev, intf);
	/* Configure WRDMA registers */
	configure_wrdma(hs_dev, intf);
	msleep(1000);
	/* Clear the IRQs */
	clear_irqs(hs_dev);
	/* Enable mic */
	setbits(hs_dev->i2s_ctl, I2S_MIC_EN);
}

/* Function to configure HS-I2S registers in loopback mode */
static void configure_loopback_mode(struct hsi2s_device *hs_dev, int intf)
{
	pr_warn("[HSI2S] Configuring loopback mode operation");
	/* Set operational mode */
	hs_dev->mode = 0;
	/* Reset the DMA registers */
	reset_rddma_registers(hs_dev);
	reset_wrdma_registers(hs_dev);
	/* Configure RDDMA registers */
	configure_rddma(hs_dev, intf);
	/* Configure WRDMA registers */
	configure_wrdma_lb(hs_dev, intf);
	msleep(1000);
	/* Clear the IRQs */
	clear_irqs(hs_dev);
	/* Enable mic */
	setbits(hs_dev->i2s_ctl, I2S_MIC_EN);
	/* Reset metadata counters */
	hs_dev->meta_index_read = 0;
	hs_dev->meta_index_write = 0;
	hs_dev->user_read_index = 0;
	/* Reset buffer pointers */
	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;
}

/* DMA buffer callbacks */

/* Function to allocate buffers and metadata structures */
static int hsi2s_buffer_init(struct hsi2s_device *hs_dev)
{
	int ret = 0;
	int i;
	u32 wrdma_periodic_length_bytes = wrdma_periodic_length * 4;

	pr_warn("[HSI2S] Allocating metadata structure for read DMA");

	/* Allocate read metadata */
	hs_dev->b_meta_read = kcalloc(METADATA_SIZE,
				      sizeof(struct buffer_metadata),
				      GFP_KERNEL);
	if (!hs_dev->b_meta_read)
		goto err_read_meta;

	for (i = 0; i < METADATA_SIZE; i++)
		hs_dev->b_meta_read[i].data_ready = 0;

	hs_dev->rddma_busy = 0;
	hs_dev->meta_index_read = 0;

	pr_warn("[HSI2S] Allocating kernel buffers for write DMA");

	/* Allocate write buffer */
	hs_dev->write_buffer = kzalloc(sizeof(*hs_dev->write_buffer),
				       GFP_KERNEL);
	if (!hs_dev->write_buffer)
		goto err_write_buffer;

	hs_dev->write_buffer->buffer = kzalloc(sizeof(int32_t) *
				DEFAULT_BUFF_LEN_WORDS, GFP_KERNEL | GFP_DMA);
	if (!hs_dev->write_buffer->buffer)
		goto err_write_buffer;

	hs_dev->lpass_wrdma_start = (void *)hs_dev->write_buffer->buffer;
	hs_dev->lpass_wrdma_end = hs_dev->lpass_wrdma_start +
					DEFAULT_BUFF_LEN_BYTES;

	hs_dev->write_buffer->head = hs_dev->lpass_wrdma_start;
	hs_dev->write_buffer->tail = hs_dev->lpass_wrdma_start;

	pr_warn("[HSI2S] Allocating metadata structure for write DMA");

	/* Allocate write metadata */
	hs_dev->b_meta_write = kcalloc(METADATA_SIZE,
				       sizeof(struct buffer_metadata),
					GFP_KERNEL);
	if (!hs_dev->b_meta_write)
		goto err_write_meta;

	for (i = 0; i < METADATA_SIZE; i++) {
		hs_dev->b_meta_write[i].start_address =
		kzalloc(wrdma_periodic_length_bytes, GFP_KERNEL | GFP_DMA);
		if (!hs_dev->b_meta_write[i].start_address)
			goto err_write_meta;
		hs_dev->b_meta_write[i].length = wrdma_periodic_length_bytes;
		hs_dev->b_meta_write[i].data_ready = 0;
	}
	hs_dev->meta_index_write = 0;
	hs_dev->user_read_index = 0;

	return ret;

err_write_meta:
	if (hs_dev->b_meta_write) {
		for (i = 0; i < METADATA_SIZE; i++) {
			kfree(hs_dev->b_meta_write[i].start_address);
			hs_dev->b_meta_write[i].start_address = NULL;
		}
		kfree(hs_dev->b_meta_write);
		hs_dev->b_meta_write = NULL;
	}

err_write_buffer:
	if (hs_dev->write_buffer) {
		kfree(hs_dev->write_buffer->buffer);
		hs_dev->write_buffer->buffer = NULL;
		kfree(hs_dev->write_buffer);
		hs_dev->write_buffer = NULL;
	}

	if (hs_dev->b_meta_read) {
		for (i = 0; i < METADATA_SIZE; i++) {
			kfree(hs_dev->b_meta_read[i].start_address);
			hs_dev->b_meta_read[i].start_address = NULL;
		}
		kfree(hs_dev->b_meta_read);
		hs_dev->b_meta_read = NULL;
	}

err_read_meta:
	return -ENOMEM;
}

/* Function to free allocated buffers and metadata structures */
static void hsi2s_buffer_free(struct hsi2s_device *hs_dev)
{
	int i;

	/* Freeing metadata structure for write DMA */
	if (hs_dev->b_meta_write) {
		for (i = 0; i < METADATA_SIZE; i++) {
			kfree(hs_dev->b_meta_write[i].start_address);
			hs_dev->b_meta_write[i].start_address = NULL;
		}
		kfree(hs_dev->b_meta_write);
		hs_dev->b_meta_write = NULL;
	}

	/* Freeing write DMA buffer */
	if (hs_dev->write_buffer) {
		kfree(hs_dev->write_buffer->buffer);
		hs_dev->write_buffer->buffer = NULL;
		kfree(hs_dev->write_buffer);
		hs_dev->write_buffer = NULL;
	}

	/* Freeing metadata structure for read DMA */
	if (hs_dev->b_meta_read) {
		for (i = 0; i < METADATA_SIZE; i++) {
			kfree(hs_dev->b_meta_read[i].start_address);
			hs_dev->b_meta_read[i].start_address = NULL;
		}
		kfree(hs_dev->b_meta_read);
		hs_dev->b_meta_read = NULL;
	}
}

/* Function to call register mapping and buffer management callbacks */
static int init_default(struct hsi2s_device *hs_dev, int intf)
{
	int ret = 0;

	/* Map the hs-i2s registers */
	map_registers(hs_dev, intf);

	reset_registers(hs_dev);

	/* Allocate kernel buffers */
	hsi2s_buffer_init(hs_dev);
	if (ret < 0) {
		pr_err("[HSI2S] Buffer allocation failed");
		return ret;
	}

	/* Initialize the spinlocks */
	spin_lock_init(&hs_dev->metacount_lock);

	/* Initialize the wait queues */
	init_waitqueue_head(&hs_dev->wq_rddma);
	init_waitqueue_head(&hs_dev->wq_wrdma);

	return ret;
}

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

/* Function to schedule rddma operation */
static int rddma_schedule(void *data)
{
	struct hsi2s_device *hs_dev;
	int i = 0;
	u32 base_addr;
	u32 buff_len;
	u32 per_len;

	hs_dev = (struct hsi2s_device *)data;
	pr_warn("[HSI2S] Starting RDDMA scheduler...");

	while (i < METADATA_SIZE) {
		if (kthread_should_stop()) {
			pr_warn("[HSI2S] RDDMA scheduler asked to exit...");
			break;
		}

		if (hs_dev->b_meta_read[i].data_ready) {
			/* Wait until any pending DMA is complete */
			wait_event_interruptible(hs_dev->wq_rddma,
						 hs_dev->rddma_busy == 0);
			hs_dev->rddma_busy = 1;
			/* Configure the DMA registers */
			base_addr =
			virt_to_phys(hs_dev->b_meta_read[i].start_address);
			buff_len = ((hs_dev->b_meta_read[i].length) /
					BYTES_PER_SAMPLE) - 1;
			per_len = (hs_dev->b_meta_read[i].length) /
					BYTES_PER_SAMPLE;
			writel_relaxed(base_addr, hs_dev->rddma_base);
			writel_relaxed(buff_len, hs_dev->rddma_buff_len);
			writel_relaxed(per_len, hs_dev->rddma_per_len);
			/* Enable the DMA channel */
			setbits(hs_dev->rddma_ctl, RDDMA_EN);
			/* Enable output */
			setbits(hs_dev->i2s_ctl, I2S_SPKR_EN);
			hs_dev->b_meta_read[i].data_ready = 0;
		}

		i = (i + 1) % METADATA_SIZE;
	}

	return 0;
}

/* Interrupt thread function */
static irq_handler_t irq_thread_fn(int irq, void *devid)
{
	u32 temp_len;
	u32 wrdma_periodic_length_bytes = wrdma_periodic_length * 4;
	u32 irq_stat;
	u32 curr_addr;
	struct hsi2s_device **hs_arr;
	int index;
	void *tail;

	hs_arr = hsi2s_core->hsi2s_arr;
	mutex_lock(&hsi2s_core->irqlock);

	if (hs_arr[0]) {
		irq_stat = readl_relaxed(hs_arr[0]->irq_stat);
		/* Periodic interrupt on read channel 0 */
		if (irq_stat & IRQ_PER_RDDMA_CH0) {
			clearbits(hs_arr[0]->i2s_ctl, I2S_SPKR_EN);
			clearbits(hs_arr[0]->rddma_ctl, RDDMA_EN);
			setbits(hs_arr[0]->irq_clear, IRQ_PER_RDDMA_CH0);
			hs_arr[0]->rddma_busy = 0;
			wake_up_interruptible(&hs_arr[0]->wq_rddma);
		}
		irq_stat = readl_relaxed(hs_arr[0]->irq_stat);
		/* Periodic interrupt on write channel 0 */
		if (irq_stat & IRQ_PER_WRDMA_CH0) {
			setbits(hs_arr[0]->irq_clear, IRQ_PER_WRDMA_CH0);

			index = hs_arr[0]->meta_index_write;
			tail = hs_arr[0]->write_buffer->tail;
			/* Boundary condition on write buffer */
			if ((hs_arr[0]->lpass_wrdma_end - tail) <
			    wrdma_periodic_length_bytes) {
				temp_len = hs_arr[0]->lpass_wrdma_end - tail;
				memcpy(hs_arr[0]->b_meta_write[index].start_address,
				       tail, temp_len);
				memcpy(hs_arr[0]->b_meta_write[index].start_address + temp_len,
				       hs_arr[0]->lpass_wrdma_start,
				       wrdma_periodic_length_bytes - temp_len);
			} else {
				memcpy(hs_arr[0]->b_meta_write[index].start_address,
				       tail, wrdma_periodic_length_bytes);
			}
			hs_arr[0]->b_meta_write[index].data_ready = 1;

			/* Notify event read */
			wake_up_interruptible(&hs_arr[0]->wq_wrdma);

			hs_arr[0]->meta_index_write =
			(hs_arr[0]->meta_index_write + 1) % METADATA_SIZE;

			/* Update tail */
			if (hs_arr[0]->mode) {
				hs_arr[0]->write_buffer->tail +=
				wrdma_periodic_length_bytes;
			} else {
				curr_addr =
				readl_relaxed(hs_arr[0]->wrdma_curr_addr);
				hs_arr[0]->write_buffer->tail =
				hs_arr[0]->lpass_wrdma_start +
				(curr_addr -
				 virt_to_phys(hs_arr[0]->lpass_wrdma_start));
			}
			if (hs_arr[0]->write_buffer->tail >=
			    hs_arr[0]->lpass_wrdma_end)
				hs_arr[0]->write_buffer->tail =
				hs_arr[0]->lpass_wrdma_start;

		}
	}

	if (hs_arr[1]) {
		irq_stat = readl_relaxed(hs_arr[1]->irq_stat);
		/* Periodic interrupt on read channel 1 */
		if (irq_stat & IRQ_PER_RDDMA_CH1) {
			clearbits(hs_arr[1]->i2s_ctl, I2S_SPKR_EN);
			clearbits(hs_arr[1]->rddma_ctl, RDDMA_EN);
			setbits(hs_arr[1]->irq_clear, IRQ_PER_RDDMA_CH1);
			hs_arr[1]->rddma_busy = 0;
			wake_up_interruptible(&hs_arr[1]->wq_rddma);
		}

		irq_stat = readl_relaxed(hs_arr[1]->irq_stat);
		/* Periodic interrupt on write channel 1 */
		if (irq_stat & IRQ_PER_WRDMA_CH1) {
			setbits(hs_arr[1]->irq_clear, IRQ_PER_WRDMA_CH1);

			index = hs_arr[1]->meta_index_write;
			tail = hs_arr[1]->write_buffer->tail;
			/* Boundary condition on write buffer */
			if (hs_arr[1]->lpass_wrdma_end - tail <
				wrdma_periodic_length_bytes) {
				temp_len = hs_arr[1]->lpass_wrdma_end - tail;
				memcpy(hs_arr[1]->b_meta_write[index].start_address,
				       tail, temp_len);
				memcpy(hs_arr[1]->b_meta_write[index].start_address + temp_len,
				       hs_arr[1]->lpass_wrdma_start,
				       wrdma_periodic_length_bytes - temp_len);
			} else {
				memcpy(hs_arr[1]->b_meta_write[index].start_address,
				       tail, wrdma_periodic_length_bytes);
			}
			hs_arr[1]->b_meta_write[index].data_ready = 1;

			/* Notify event read */
			wake_up_interruptible(&hs_arr[1]->wq_wrdma);

			hs_arr[1]->meta_index_write =
			(hs_arr[1]->meta_index_write + 1) % METADATA_SIZE;

			/* Update tail */
			if (hs_arr[1]->mode) {
				hs_arr[1]->write_buffer->tail +=
				wrdma_periodic_length_bytes;
			} else {
				curr_addr =
				readl_relaxed(hs_arr[1]->wrdma_curr_addr);
				hs_arr[1]->write_buffer->tail =
				hs_arr[1]->lpass_wrdma_start +
				(curr_addr -
				 virt_to_phys(hs_arr[1]->lpass_wrdma_start));
			}
			if (hs_arr[1]->write_buffer->tail >=
			    hs_arr[1]->lpass_wrdma_end)
				hs_arr[1]->write_buffer->tail =
				hs_arr[1]->lpass_wrdma_start;
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
	int bytes_read = 0;
	int ret = 0;
	int index;
	u32 periodic_len;

	hs_dev = (struct hsi2s_device *)file->private_data;

	periodic_len = readl_relaxed(hs_dev->wrdma_per_len);
	temp_length = periodic_len * BYTES_PER_SAMPLE;
	while (length > 0) {
		index = hs_dev->user_read_index;
		wait_event_interruptible(hs_dev->wq_wrdma,
					 hs_dev->b_meta_write[index].data_ready == 1);

		hs_dev->b_meta_write[index].data_ready = 0;

		/* Copy periodic length at a time */
		ret = copy_to_user(buffer + bytes_read,
				   hs_dev->b_meta_write[index].start_address,
				   temp_length);
		if (ret) {
			pr_err("[HSI2S] Error copying data to userspace");
			return -ret;
		}

		length -= temp_length;
		bytes_read += temp_length;
		hs_dev->user_read_index = (hs_dev->user_read_index + 1) %
						METADATA_SIZE;
	}

	return bytes_read;
}

/* Function to write data from user space to the Tx buffer */
static ssize_t device_write(struct file *file, const char *buffer,
			    size_t length, loff_t *offset)
{
	struct hsi2s_device *hs_dev;
	int write_index;

	hs_dev = (struct hsi2s_device *)file->private_data;

	/* Enter critical section */
	spin_lock(&hs_dev->metacount_lock);

	write_index = hs_dev->meta_index_read;
	hs_dev->meta_index_read = (hs_dev->meta_index_read + 1) %
					METADATA_SIZE;

	/* Exit critical section */
	spin_unlock(&hs_dev->metacount_lock);

	hs_dev->b_meta_read[write_index].start_address =
			kzalloc(length, GFP_KERNEL | GFP_DMA);
	if (!hs_dev->b_meta_read[write_index].start_address)
		return -ENOMEM;

	copy_from_user(hs_dev->b_meta_read[write_index].start_address,
		       buffer, length);
	hs_dev->b_meta_read[write_index].length = length;
	hs_dev->b_meta_read[write_index].data_ready = 1;

	return length;
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
	int minor;

	hs_dev = (struct hsi2s_device *)file->private_data;
	minor = hs_dev->minor_num;

	switch (cmd) {
	case I2S_NORMAL_MODE:
		pr_warn("[HSI2S] Triggering normal operation");
		if (hs_dev->client_count == 1)
			configure_normal_mode(hs_dev, minor);
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	case I2S_INTERNAL_LOOPBACK:
		pr_warn("[HSI2S] Triggering internal loopback");
		if (hs_dev->client_count == 1)
			configure_loopback_mode(hs_dev, minor);
		else
			pr_warn("[HSI2S] Mode already set by previous client");
		break;

	default:
		return -EINVAL;
	}

	return 0;
}

static const struct file_operations fops = {
	.read  = device_read,
	.write = device_write,
	.open  = device_open,
	.release = device_release,
	.unlocked_ioctl = device_ioctl
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
	int minor = 0;
	int ret = 0;

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

	if (!hsi2s_core->hsi2s_arr[minor])
		return -ENOMEM;

	hs_dev = hsi2s_core->hsi2s_arr[minor];

	/* Set the hsi2s device structure as platform data */
	platform_set_drvdata(pdev, hs_dev);

	/* Store the minor number */
	hs_dev->minor_num = minor;

	/* Map the interface registers */
	ret = init_default(hs_dev, minor);
	if (ret < 0) {
		pr_err("[HSI2S] Failed to set default settings for hsi2s device");
		goto err_deinit_default;
	}

	/* Configure SMMU */
	ret = hsi2s_smmu_init(pdev, minor);
	if (ret) {
		pr_err("[HSI2S] Failed to init smmu");
		goto err_free_smmu;
	}

	/* Configure the gpios */
	if (of_property_read_bool(pdev->dev.of_node, "pinctrl-names")) {
		hs_dev->is_pinctrl_names = true;
		ret = hsi2s_configure_gpio_pins(pdev);
		if (ret < 0) {
			pr_err("[HSI2S] Failed to configure gpios");
			goto err_free_smmu;
		}
	}

	/* Enable the interface clocks */
	ret = hsi2s_enable_intf_clks(pdev);
	if (ret)
		goto err_free_smmu;

	/* Start the read DMA scheduler */
	hs_dev->rddma_thread = kthread_create(rddma_schedule, hs_dev,
					      "DMA scheduler thread");
	if (hs_dev->rddma_thread) {
		wake_up_process(hs_dev->rddma_thread);
	} else {
		ret = -EINVAL;
		pr_err("Cannot create rddma scheduler thread");
		goto err_disable_intf_clock;
	}

	/* Configure the operational mode */
	if (operation_mode)
		configure_normal_mode(hs_dev, minor);
	else
		configure_loopback_mode(hs_dev, minor);

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

	hs_dev->class_sdr = class_create(THIS_MODULE,
					 (minor ? SDR1 : SDR0));
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
err_disable_intf_clock:
	hsi2s_disable_intf_clks(pdev);
err_free_smmu:
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
err_deinit_default:
	hsi2s_buffer_free(hs_dev);
	kfree(hs_dev);
	hs_dev = NULL;
	hsi2s_core->hsi2s_arr[minor] = NULL;
err_out:
	/* Enable the IRQ line */
	if (minor) {
		if (hsi2s_core->irq0 > 0) {
			hsi2s_core->desc = irq_to_desc(hsi2s_core->irq0);
			if (hsi2s_core->desc->core_internal_state__do_not_mess_with_it & 0x00000200) {
				pr_warn("[HSI2S] Removing pending IRQs");
				hsi2s_core->desc->core_internal_state__do_not_mess_with_it &= ~(0x00000200);
			}
			pr_warn("[HSI2S] Enabling IRQ line");
			enable_irq(hsi2s_core->irq0);
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
	int ret = 0;

	if (of_device_is_compatible(pdev->dev.of_node, "qcom,hsi2s-interface"))
		return hsi2s_interface_probe(pdev);

	hsi2s_core = kzalloc(sizeof(*hsi2s_core), GFP_KERNEL);
	if (!hsi2s_core)
		return -ENOMEM;

	/* Set the hsi2s_core structure as platform data */
	platform_set_drvdata(pdev, hsi2s_core);

	/* Read the number of HS-I2S interfaces */
	ret = of_property_read_u32(dev->of_node, "number-of-interfaces",
				   &interface_count);
	if (ret) {
		pr_err("[HSI2S] Resource 'number-of-interfaces' unavailable in dtsi");
		goto err_free_core;
	}

	/* Register the character device numbers */
	ret = alloc_chrdev_region(&devid, 0, interface_count,
				  "hsi2s_dev");
	if (ret < 0) {
		pr_err("[HSI2S] Major number allocation failed");
		goto err_free_core;
	}

	/* Allocate hsi2s_dev structure pointers */
	hsi2s_core->hsi2s_arr = kcalloc(interface_count,
					sizeof(struct hsi2s_device *),
					GFP_KERNEL);

	/* Enable the core clocks */
	ret = hsi2s_enable_core_clks(pdev);
	if (ret)
		goto err_free_core;

	/* Initialize the IRQ mutex */
	mutex_init(&hsi2s_core->irqlock);

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

	/* Enable the interrupt line 0 */
	hsi2s_core->irq0 = platform_get_irq(pdev, 0);
	if (hsi2s_core->irq0 < 0) {
		pr_err("[HSI2S] Err getting IRQ0");
		ret = hsi2s_core->irq0;
		goto err_iounmap;
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
		goto err_iounmap;
	}

	/* Disable the irq line until child devices are probed */
	disable_irq_nosync(hsi2s_core->irq0);

	/* Probe child devices */
	ret = of_platform_populate(dev->of_node, NULL, NULL, dev);
	if (ret)
		pr_err("[HSI2S] Failed to add child devices");
	else
		pr_warn("[HSI2S] Added child devices");

	return ret;

err_iounmap:
	iounmap(hsi2s_core->lpaif_base_va);
err_disable_core_clocks:
	hsi2s_disable_core_clks(pdev);
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
	/* Remove the device file */
	device_destroy(hs_dev->class_sdr, hs_dev->curr_devid);
	class_destroy(hs_dev->class_sdr);
	cdev_del(hs_dev->cdev_sdr);
	kfree(hs_dev->cdev_sdr);
	hs_dev->cdev_sdr = NULL;
	/* Stop the DMA scheduler thread */
	kthread_stop(hs_dev->rddma_thread);
	/* Disable the interface clocks */
	hsi2s_disable_intf_clks(pdev);
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
	/* Reset the interface registers */
	reset_registers(hs_dev);
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
	/* Unmap the register memory region */
	iounmap(hsi2s_core->lpaif_base_va);
	/* Disable the core clocks */
	hsi2s_disable_core_clks(pdev);
	/* Unregister the device numbers */
	unregister_chrdev_region(devid, 1);
	/* Free the core data structure */
	kfree(hs_core->hsi2s_arr);
	hs_core->hsi2s_arr = NULL;
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
		hsi2s_suspend_intf_clks(pdev);
	} else {
		/* Suspend the core clocks */
		hsi2s_suspend_core_clks(pdev);
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
		ret = hsi2s_resume_intf_clks(pdev);
		if (ret) {
			pr_warn("[HSI2S] Failed to resume interface clocks");
			return ret;
		}
	} else {
		/* Resume the core clocks */
		ret = hsi2s_resume_core_clks(pdev);
		if (ret) {
			pr_warn("[HSI2S] Failed to resume core clocks");
			return ret;
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

	if (!wrdma_periodic_length)
		wrdma_periodic_length = DEFAULT_NUM_WORDS;
	else {
		/* Convert the periodic length from KB to words */
		wrdma_periodic_length = (wrdma_periodic_length * 1024) /
					BYTES_PER_SAMPLE;
	}

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
