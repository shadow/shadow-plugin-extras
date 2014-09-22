#ifndef COMMON_HPP
#define COMMON_HPP

#include <stdio.h>
#include <sys/time.h>
#include <math.h>
#include <stdint.h>
#include <sys/types.h>
#include <pwd.h>
#include <string.h>

#include "myassert.h"

/* if "path" begins with '~', then replace '~' with the homedir path
 * of the user.
 *
 * caller is responsible for freeing the returned memory.
 */
char*
expandPath(const char* path);

uint64_t
gettimeofdayMs(struct timeval* t);

void
printhex(const char *hdr,
         const unsigned char *md_value,
         unsigned int md_len);

void
to_hex(const unsigned char *value,
       unsigned int len,
       char *hex);



#ifdef ENABLE_MY_LOG_MACROS
#define logDEBUG(fmt, ...)                                              \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, "line %d: " fmt,         \
              __LINE__, ##__VA_ARGS__);                                 \
    } while (0)

#define logINFO(fmt, ...)                                               \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_INFO, __FUNCTION__, "line %d: " fmt,          \
              __LINE__, ##__VA_ARGS__);                                 \
    } while (0)

#define logMESSAGE(fmt, ...)                                            \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_MESSAGE, __FUNCTION__, "line %d: " fmt,       \
              __LINE__, ##__VA_ARGS__);                                 \
    } while (0)

#define logWARN(fmt, ...)                                               \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_WARNING, __FUNCTION__, "line %d: " fmt,       \
              __LINE__, ##__VA_ARGS__);                                 \
    } while (0)

#define logCRITICAL(fmt, ...)                                           \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_CRITICAL, __FUNCTION__, "line %d: " fmt,      \
              __LINE__, ##__VA_ARGS__);                                 \
    } while (0)

#else

/* no ops */
#define logDEBUG(fmt, ...)

#define logINFO(fmt, ...)

#define logMESSAGE(fmt, ...)

#define logWARN(fmt, ...)

#define logCRITICAL(fmt, ...)

#endif

#define ARRAY_LEN(arr)  (sizeof(arr) / sizeof((arr)[0]))

// template<typename T1, typename T2>
// inline bool
// inMap(const std::map<T1, T2>& m, const T1& k)
// {
//     return m.end() != m.find(k);
// }

#define inMap(m, k)                             \
    ((m).end() != (m).find(k))

#define inSet(s, k)                             \
    ((s).end() != (s).find(k))

#endif /* COMMON_HPP */
