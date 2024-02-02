HSI2S_ENABLED := true
ifeq ($(TARGET_USES_QMAA),true)
        ifdef ($(TARGET_USES_QMAA_OVERRIDE_HSI2S))
        ifneq ($(TARGET_USES_QMAA_OVERRIDE_HSI2S),true)
           HSI2S_ENABLED := false
        endif # TARGET_USES_QMAA_OVERRIDE_HSI2S
        endif
endif # TARGET_USES_QMAA

ifeq ($(HSI2S_ENABLED),true)
LOCAL_PATH := $(call my-dir)
DLKM_DIR := $(TOP)/device/qcom/common/dlkm

BOARD_VENDOR_KERNEL_MODULES += $(KERNEL_MODULES_OUT)/hsi2s.ko

#KBUILD_OPTIONS
KBUILD_OPTIONS += KERNEL_ROOT=$(shell pwd)/kernel/msm-$(TARGET_KERNEL_VERSION)/
KBUILD_OPTIONS += MODNAME=hsi2s
KBUILD_OPTIONS += BOARD_PLATFORM=$(TARGET_BOARD_PLATFORM)
KBUILD_OPTIONS += CONFIG_HSI2S=y
$(info value of TARGET_USES_KERNEL_PLATFORM IS '$(TARGET_USES_KERNEL_PLATFORM)')

#Clear Environment Variables
include $(CLEAR_VARS)
#Defining the local options
LOCAL_SRC_FILES             :=  \
                                $(shell find $(LOCAL_PATH)/driver/ -L -type f) \
                                $(shell find $(LOCAL_PATH)/test/generic/ -L -type f)\
                                $(LOCAL_PATH)/Android.mk \
                                $(LOCAL_PATH)/hsi2s-board.mk   \
                                $(LOCAL_PATH)/hsi2s-product.mk \
                                $(LOCAL_PATH)/Kbuild
LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)
LOCAL_MODULE              := hsi2s.ko
LOCAL_MODULE_TAGS         := optional


include $(DLKM_DIR)/Build_external_kernelmodule.mk
include $(LOCAL_PATH)/test/generic/Android.mk
endif #HSI2S_ENABLED

