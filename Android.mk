# Copyright (C) 2008 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_C_INCLUDES := \
	external/libxml2/include \
	external/icu4c/common/ \
	external/icu/icu4c/source/common
LOCAL_SRC_FILES := \
	src/configuration.c \
	src/control.c \
	src/mitigation.c \
	src/resource.c \
	src/threshold.c \
	src/dom.c \
	src/libxml2parser.c \
	src/watch.c \
	src/thermal_zone.c \
	src/cpufreq.c \
	src/util.c \
	src/main.c \

LOCAL_SHARED_LIBRARIES := liblog libicuuc libcutils
LOCAL_STATIC_LIBRARIES := libxml2
LOCAL_MODULE := thermanager
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
# C89 3.5.7 "Initialization - Sematics":
#   If there are fewer initializers in a list than there are members of an
#   aggregate, the remainder of the aggregate shall be initialized implicitly
#   the same as objects that have static storage duration.
# C99 6.7.8/21 "Initialization - Sematics":
#   If there are fewer initializers in a brace-enclosed list than there are
#   elements or members of an aggregate, or fewer characters in a string
#   literal used to initialize an array of known size than there are elements
#   in the array, the remainder of the aggregate shall be initialized
#   implicitly the same as objects that have static storage duration.
#
# See http://gcc.gnu.org/bugzilla/show_bug.cgi?id=36750
LOCAL_CFLAGS := -Wno-missing-field-initializers

LOCAL_SRC_FILES := thermonitor.c
LOCAL_MODULE := thermonitor
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
