/*
  Peony Game Engine
  Copyright (C) 2020 Vlad-Stefan Harbuz <vlad@vladh.net>
  All rights reserved.
*/

#pragma once

#include "types.hpp"

// Platform things
#if !defined(MAX_PATH)
  #if defined(PATH_MAX)
    #define MAX_PATH PATH_MAX
  #else
    #define MAX_PATH 260
  #endif
#endif

#define PLATFORM_WINDOWS (1 << 0)
#define PLATFORM_MACOS (1 << 1)
#define PLATFORM_POSIX (1 << 2)
#define PLATFORM_UNIX (1 << 3)
#define PLATFORM_FREEBSD (1 << 4)

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || \
    defined(__NT__) || defined(__CYGWIN__) || defined(__MINGW32__)
  #define PLATFORM PLATFORM_WINDOWS
#elif defined(__APPLE__)
  #define PLATFORM (PLATFORM_MACOS | PLATFORM_POSIX)
#elif defined(__linux__) || defined(__unix__)
  #define PLATFORM (PLATFORM_UNIX | PLATFORM_POSIX)
#elif defined(__FreeBSD)
  #define (PLATFORM_FREEBSD | PLATFORM_POSIX)
#else
  #error "Unknown platform"
#endif
