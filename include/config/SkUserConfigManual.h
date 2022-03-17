/*
 * Copyright 2017 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkUserConfigManual_DEFINED
#define SkUserConfigManual_DEFINED
  #include <android/log.h>
  #define GR_TEST_UTILS 1
  #define SK_BUILD_FOR_ANDROID_FRAMEWORK
  #define SK_DEFAULT_FONT_CACHE_LIMIT   (768 * 1024)
  #define SK_DEFAULT_GLOBAL_DISCARDABLE_MEMORY_POOL_SIZE (512 * 1024)
  #define SK_USE_FREETYPE_EMBOLDEN

  // Disable these Ganesh features
  #define SK_DISABLE_REDUCE_OPLIST_SPLITTING
  // Check error is expensive. HWUI historically also doesn't check its allocations
  #define GR_GL_CHECK_ALLOC_WITH_GET_ERROR 0

  // Staging flags
  #define SK_LEGACY_PATH_ARCTO_ENDPOINT
  #define SK_SUPPORT_STROKEANDFILL

  // Needed until we fix https://bug.skia.org/2440
  #define SK_SUPPORT_LEGACY_CLIPTOLAYERFLAG
  #define SK_SUPPORT_LEGACY_EMBOSSMASKFILTER
  #define SK_SUPPORT_LEGACY_AA_CHOICE
  #define SK_SUPPORT_LEGACY_AAA_CHOICE

  #define SK_DISABLE_DAA  // skbug.com/6886

  #ifdef LOG_TAG
    #undef LOG_TAG
  #endif
  #define LOG_TAG "skia"
  #define SK_ABORT(...) __android_log_assert(nullptr, LOG_TAG, ##__VA_ARGS__)
#endif // SkUserConfigManual_DEFINED
