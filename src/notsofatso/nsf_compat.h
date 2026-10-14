// Minimal stand-in for <windows.h> so NotSo Fatso builds on any platform.
#ifndef NSF_COMPAT_H
#define NSF_COMPAT_H

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#else

#include <math.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;
typedef uint32_t UINT;
typedef int32_t  BOOL;
typedef const char* LPCSTR;
typedef char* LPSTR;
typedef int errno_t;

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

#define ZeroMemory(p, n) memset((p), 0, (n))
#define CopyMemory(d, s, n) memcpy((d), (s), (n))

#define __fastcall
#define __forceinline inline
#define __int64 long long

#if defined(__cplusplus) && __cplusplus >= 201703L
#define register
#endif

#include <unistd.h>
static inline void Sleep(unsigned ms) { usleep(ms * 1000); }

#define lstrlen(s) ((int)strlen(s))

#ifdef __cplusplus
#include <type_traits>
// Same semantics as the Windows min/max macros the original code relied on,
// scoped to NotSo Fatso's namespace so they don't leak into other code.
namespace NotSoFatso {
template <class A, class B>
inline typename std::common_type<A, B>::type min(A a, B b) { return (a < b) ? a : b; }
template <class A, class B>
inline typename std::common_type<A, B>::type max(A a, B b) { return (a > b) ? a : b; }
}
#endif

static inline errno_t fopen_s(FILE** f, const char* name, const char* mode)
{
    *f = fopen(name, mode);
    return *f ? 0 : (errno ? errno : 1);
}

#endif // _WIN32

#endif // NSF_COMPAT_H
