LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := freerdp-android
LOCAL_SRC_FILES := $(TARGET_ARCH_ABI)/libfreerdp-android.so
LOCAL_EXPORT_C_INCLUDES := ../../../../include
include $(PREBUILT_SHARED_LIBRARY)
