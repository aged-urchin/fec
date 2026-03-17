#ifndef ___UTILS_H___
#define ___UTILS_H___

#include <cstdint>

#define UINT16_TO_BE(val)       (uint16_t)((((val) >> 8) & 0xFF) | (((val) & 0x00FF) << 8))
#define UINT32_TO_BE(val)       (uint32_t)((((val) >> 24) & 0xFF) | (((val) >>  8) & 0x0000FF00) | (((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000))
#define UINT16_FROM_BE(val)     UINT16_TO_BE(val)
#define UINT32_FROM_BE(val)     UINT32_TO_BE(val)

#endif ///< ___UTILS_H___
