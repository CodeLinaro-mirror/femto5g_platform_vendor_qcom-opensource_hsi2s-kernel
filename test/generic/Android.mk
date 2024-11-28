LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE_OWNER := qti
LOCAL_MODULE_PATH := $(TARGET_OUT_VENDOR)/bin
LOCAL_SRC_FILES += \
    driver_test.c \

LOCAL_MODULE:= hsi2s_test

LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Wno-unused-parameter -Wno-unused-variable -I vendor/qcom/opensource/hsi2s-kernel/driver
LOCAL_VENDOR_MODULE := true

include $(BUILD_EXECUTABLE)


