ifneq ($(TARGET_DISABLE_HSI2S_DLKM),true)
# Build HSI2S kernel driver
ifeq ($(TARGET_BOARD_AUTO),true)
	BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/hsi2s.ko
endif
endif # TARGET_DISABLE_HSI2S_DLKM
