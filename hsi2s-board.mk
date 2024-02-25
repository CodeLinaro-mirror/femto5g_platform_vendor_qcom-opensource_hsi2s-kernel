# Build HSI2S kernel driver
INCLUDE_KO_FILES := true
ifneq ($(TARGET_USES_GY), true)
ifeq ($(TARGET_USES_QMAA),true)
     ifdef TARGET_USES_QMAA_OVERRIDE_HSI2S
     ifneq ($(TARGET_USES_QMAA_OVERRIDE_HSI2S),true)
          ifeq ($(TARGET_BOARD_AUTO),true)
	      INCLUDE_KO_FILES := false
          endif #TARGET_BOARD_AUTO
     endif #TARGET_USES_QMAA_OVERRIDE_HSI2S
     endif
endif #TARGET_USES_QMAA
endif #TARGET_USUS_GY
ifeq ($(INCLUDE_KO_FILES),true)
	BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/hsi2s.ko
endif #INCLUDE_KO_FILES
