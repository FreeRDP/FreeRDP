LOCAL_PATH := $(call my-dir)

ifdef HISTORICAL_NDK_VERSIONS_ROOT
# This is included by the platform build system.
include $(CLEAR_VARS)
LOCAL_MODULE := cpufeatures
LOCAL_SRC_FILES := cpu-features.c
LOCAL_SDK_VERSION := 9
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)
include $(BUILD_STATIC_LIBRARY)

else # NDK build system

include $(CLEAR_VARS)
LOCAL_MODULE := cpufeatures
LOCAL_SRC_FILES := cpu-features.c
LOCAL_CFLAGS := -Wall -Wextra -Werror
LOCAL_EXPORT_C_INCLUDES := $(LOCAL_PATH)
LOCAL_EXPORT_LDLIBS := -ldl
include $(BUILD_STATIC_LIBRARY)

endif # HISTORICAL_NDK_VERSIONS_ROOT
