/* Force-included in every MPC build file. Smooths over differences between
 * the embedded toolchain (newlib + libstdc++) and Linux (glibc + libc++). */
#pragma once
#ifdef __cplusplus
#include <cassert>
#include <cstdlib>
#include <cstring>
#else
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#endif

#ifndef _GLIBCXX_THROW_OR_ABORT
#define _GLIBCXX_THROW_OR_ABORT(x) abort()
#endif
