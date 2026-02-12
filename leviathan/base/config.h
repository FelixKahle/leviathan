// Copyright (c) 2026 Felix Kahle.
//
// Permission is hereby granted, free of charge, to any person obtaining
// a copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be
// included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
// EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
// NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
// LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
// OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
// WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#ifndef LEVIATHAN_BASE_CONFIG_H_
#define LEVIATHAN_BASE_CONFIG_H_

// Determine the C++ Standard Version
#ifndef LEVIATHAN_CPLUSPLUS_LANG
    #if defined(_MSVC_LANG)
        #define LEVIATHAN_CPLUSPLUS_LANG _MSVC_LANG
    #elif defined(__cplusplus)
        #define LEVIATHAN_CPLUSPLUS_LANG __cplusplus
    #else
        #define LEVIATHAN_CPLUSPLUS_LANG 0L
    #endif
#endif

// Enforce C++20
#define LEVIATHAN_CPP_20_STANDARD 202002L

#if LEVIATHAN_CPLUSPLUS_LANG < LEVIATHAN_CPP_20_STANDARD
    #error "Leviathan requires at least C++20."
#endif

// Verify specific C++20 features (Double check compiler compliance)
#if !defined(__cpp_constexpr) || __cpp_constexpr < 201907L
    #error "Leviathan requires full compiler support for C++20 constexpr."
#endif

#if !defined(__cpp_concepts) || __cpp_concepts < 201907L
    #error "Leviathan requires compiler support for C++20 concepts."
#endif

#if defined(__cpp_lib_containers_ranges) && __cpp_lib_containers_ranges >= 202202L
    #define LEVIATHAN_SUPPORTS_CONTAINER_RANGE_INSERT 1
#else
    #define LEVIATHAN_SUPPORTS_CONTAINER_RANGE_INSERT 0
#endif

#ifdef NDEBUG
    #define LEVIATHAN_BUILD_RELEASE 1
    #define LEVIATHAN_BUILD_DEBUG 0
#else
    #define LEVIATHAN_BUILD_RELEASE 0
    #define LEVIATHAN_BUILD_DEBUG 1
#endif

#if (LEVIATHAN_BUILD_DEBUG + LEVIATHAN_BUILD_RELEASE) != 1
    #error "Inconsistent build configuration: Both LEVIATHAN_BUILD_DEBUG and LEVIATHAN_BUILD_RELEASE are set or unset."
#endif

// Force Inline
#ifndef LEVIATHAN_ALLOW_FORCE_INLINE
    #define LEVIATHAN_ALLOW_FORCE_INLINE 1
#endif

#ifndef LEVIATHAN_FORCE_INLINE
    #if LEVIATHAN_ALLOW_FORCE_INLINE
        #if defined(_MSC_VER) || defined(__INTEL_COMPILER) || (defined(__INTEL_LLVM_COMPILER) && defined(_WIN32))
            #define LEVIATHAN_FORCE_INLINE __forceinline
        #elif defined(__GNUC__) || defined(__clang__) || defined(__IBMCPP__) || defined(__NVCOMPILER) || defined(__ARMCC_VERSION)
            #define LEVIATHAN_FORCE_INLINE inline __attribute__((always_inline))
        #else
            #define LEVIATHAN_FORCE_INLINE inline
        #endif
    #else
        #define LEVIATHAN_FORCE_INLINE inline
    #endif
#endif

// No Inline
#if defined(_MSC_VER) || defined(__INTEL_LLVM_COMPILER)
    #define LEVIATHAN_NO_INLINE __declspec(noinline)
#elif defined(__GNUC__) || defined(__clang__) || defined(__NVCOMPILER) || defined(__IBMCPP__) || defined(__ARMCC_VERSION)
    #define LEVIATHAN_NO_INLINE __attribute__((noinline))
#else
    #define LEVIATHAN_NO_INLINE
#endif

// Branch Prediction Hints
// Note: C++20 introduced [[likely]] and [[unlikely]] attributes.
// However, these macros are useful for wrapping expressions (e.g., inside if conditions).
#if defined(__GNUC__) || defined(__clang__) || defined(__INTEL_COMPILER) || defined(__NVCOMPILER) || defined(__IBMCPP__) || defined(__ARMCC_VERSION)
    #define LEVIATHAN_LIKELY(x)   __builtin_expect(!!(x), 1)
    #define LEVIATHAN_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LEVIATHAN_LIKELY(x)   (x)
    #define LEVIATHAN_UNLIKELY(x) (x)
#endif

#if !defined(LEVIATHAN_SYMBOL_EXPORT) && !defined(LEVIATHAN_SYMBOL_IMPORT) && !defined(LEVIATHAN_SYMBOL_LOCAL)
    #if defined(_WIN32) || defined(__CYGWIN__)
        #define LEVIATHAN_SYMBOL_EXPORT __declspec(dllexport)
        #define LEVIATHAN_SYMBOL_IMPORT __declspec(dllimport)
        #define LEVIATHAN_SYMBOL_LOCAL
    #elif defined(__GNUC__) || defined(__clang__) || defined(__INTEL_LLVM_COMPILER) || defined(__NVCOMPILER)
        #define LEVIATHAN_SYMBOL_EXPORT __attribute__((visibility("default")))
        #define LEVIATHAN_SYMBOL_IMPORT __attribute__((visibility("default")))
        #define LEVIATHAN_SYMBOL_LOCAL  __attribute__((visibility("hidden")))
    #elif defined(__SUNPRO_CC)
        #define LEVIATHAN_SYMBOL_EXPORT __global
        #define LEVIATHAN_SYMBOL_IMPORT __global
        #define LEVIATHAN_SYMBOL_LOCAL  __hidden
    #else
        #define LEVIATHAN_SYMBOL_EXPORT
        #define LEVIATHAN_SYMBOL_IMPORT
        #define LEVIATHAN_SYMBOL_LOCAL
    #endif
#endif

#endif // LEVIATHAN_BASE_CONFIG_H_
