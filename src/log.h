#ifndef LOG_TAG
#define LOG_TAG "thermanager"

#ifdef ANDROID

#include <cutils/log.h>
#include <cutils/klog.h>
#define LOGV ALOGV
#define LOGI ALOGI
#define LOGW ALOGW
#define LOGE ALOGE
#define KLOGE(x, ...) KLOG_ERROR(LOG_TAG, x, ##__VA_ARGS__)

#elif defined(__linux__)

#include <stdio.h>
#define LOGV(x, ...) fprintf(stderr, "[V] "LOG_TAG": " x, ##__VA_ARGS__)
#define LOGI(x, ...) fprintf(stderr, "[I] "LOG_TAG": " x, ##__VA_ARGS__)
#define LOGW(x, ...) fprintf(stderr, "[W] "LOG_TAG": " x, ##__VA_ARGS__)
#define LOGE(x, ...) fprintf(stderr, "[E] "LOG_TAG": " x, ##__VA_ARGS__)
#define KLOGE(x, ...) fprintf(stderr, "[E] "LOG_TAG": " x, ##__VA_ARGS__)

#endif
#define LOG LOGI

#endif
