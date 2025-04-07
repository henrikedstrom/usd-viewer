//
// Include common OpenUSD headers and supress warnings based on compiler
//
#pragma once

#if _MSC_VER

#define NOMINMAX // Avoid defining min and max macros in Windows headers

#ifndef TBB_SUPPRESS_DEPRECATED_MESSAGES
#define TBB_SUPPRESS_DEPRECATED_MESSAGES 1
#endif

#ifdef APIENTRY
#undef APIENTRY
#endif

#pragma warning(push)
#pragma warning(disable : 4244) // conversion from 'type1' to 'type2', possible loss of data
#pragma warning(disable : 4251) // class 'X' needs to have dll-interface...
#pragma warning(disable : 4267) // conversion from 'size_t' to 'type', possible loss of data
#pragma warning(disable : 4305) // truncation from 'type1' to 'type2'
#endif

#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdGeom/bboxCache.h>
#include <pxr/usd/usdGeom/metrics.h>
#include <pxr/usdImaging/usdImagingGL/engine.h>

#if _MSC_VER
#pragma warning(pop)
#endif