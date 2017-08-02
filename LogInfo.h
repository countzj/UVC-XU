/************************************************************************************
* UpodateLog.h
*
************************************************************************************/
#ifndef    _UPDATECAM_UPDATELOG_H_
#define    _UPDATECAM_UPDATELOG_H_





#include <android/log.h>

#define LOG_TAG "cam_update"

#if 0
#define LOGD(...) printf
#define LOGI(...) printf
#define LOGW(...) printf
#define LOGE(...) printf
#else
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#endif

#endif // _UPDATECAM_UPDATELOG_H_