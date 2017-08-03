## Android.mk
LOCAL_PATH:=$(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE:=cam_update
LOCAL_C_INCLUDES += $(LOCAL_PATH)/
LOCAL_SRC_FILES:= update.cpp

LOCAL_CFLAGS += -pie -fPIE -std=c++11
LOCAL_LDFLAGS += -pie -fPIE

LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog
include $(BUILD_EXECUTABLE)


