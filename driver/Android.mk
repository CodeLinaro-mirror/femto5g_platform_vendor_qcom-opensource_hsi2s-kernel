#This makefile is only to compile HSI2S for AUTO platform

ifeq ($(TARGET_BOARD_AUTO),true)
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

# This makefile is only for DLKM
ifneq ($(findstring vendor,$(LOCAL_PATH)),)

ifneq ($(findstring opensource,$(LOCAL_PATH)),)
HSI2S_BLD_DIR := ../../vendor/qcom/opensource/hsi2s-kernel/driver
endif # opensource

LOCAL_MODULE_PATH := $(KERNEL_MODULES_OUT)

DLKM_DIR := ./device/qcom/common/dlkm
KBUILD_OPTIONS := $(HSI2S_BLD_DIR)

LOCAL_MODULE := hsi2s.ko
LOCAL_MODULE_TAGS := debug

include $(DLKM_DIR)/AndroidKernelModule.mk
endif
endif

