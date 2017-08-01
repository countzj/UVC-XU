## Android.mk
LOCAL_PATH:=$(call my-dir)

include $(CLEAR_VARS)

LOCAL_CFLAGS  += -DLINUX

LOCAL_MODULE:=cam_update
LOCAL_SRC_FILES:= update.cpp

LOCAL_CFLAGS += -pie -fPIE
LOCAL_LDFLAGS += -pie -fPIE

include $(BUILD_EXECUTABLE)

LOCAL_LDLIBS := -L$(SYSROOT)/usr/lib -llog
