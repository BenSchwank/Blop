#pragma once

// Single fallback for the runtime version string. CMake injects the live
// git-describe value as BLOP_VERSION_STR at compile time; this header is
// the only in-tree default when that define is absent (IDEs, unit tests).
#ifndef BLOP_VERSION_STR
#define BLOP_VERSION_STR "3.29.47"
#endif
