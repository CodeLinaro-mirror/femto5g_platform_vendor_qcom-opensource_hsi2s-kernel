LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES += \
    driver_test.c \

LOCAL_MODULE:= hsi2s_test

LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -Wno-unused-parameter -Wno-unused-variable

include $(BUILD_EXECUTABLE)


