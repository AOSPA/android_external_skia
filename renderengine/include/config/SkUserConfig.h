// DO NOT MODIFY! This file is autogenerated by gn_to_bp.py.
// If need to change a define, modify SkUserConfigManual.h
#pragma once
#include "SkUserConfigManual.h"

#ifndef SK_ENABLE_SKSL
#define SK_ENABLE_SKSL
#endif

#ifndef SK_ENCODE_PNG
#define SK_ENCODE_PNG
#endif

#ifndef SK_GAMMA_APPLY_TO_A8
#define SK_GAMMA_APPLY_TO_A8
#endif

#ifndef SK_GL
#define SK_GL
#endif

#ifndef SK_HAS_ANDROID_CODEC
#define SK_HAS_ANDROID_CODEC
#endif

#ifndef SK_USE_VMA
#define SK_USE_VMA
#endif

#ifndef SK_VULKAN
#define SK_VULKAN
#endif

#ifndef SK_BUILD_FOR_ANDROID
    #error "SK_BUILD_FOR_ANDROID must be defined!"
#endif
#if defined(SK_BUILD_FOR_IOS) || defined(SK_BUILD_FOR_MAC) || \
    defined(SK_BUILD_FOR_UNIX) || defined(SK_BUILD_FOR_WIN)
    #error "Only SK_BUILD_FOR_ANDROID should be defined!"
#endif
