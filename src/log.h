#ifndef LOG_TAG
#define LOG_TAG "thermanager"

#ifdef ANDROID

#include <cutils/log.h>
#define LOGI ALOGI
#define LOGE ALOGE

#elif defined(__linux__)

#include <stdio.h>
#define LOGI(x, ...) fprintf(stderr, "[I] "LOG_TAG": " x, ##__VA_ARGS__)
#define LOGE(x, ...) fprintf(stderr, "[E] "LOG_TAG": " x, ##__VA_ARGS__)

#endif
#define LOG LOGI

#endif
