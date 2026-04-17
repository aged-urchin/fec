#ifndef ___HEADER_H___
#define ___HEADER_H___

#include <cstdint>

/** Precondition: originalCount + recoveryCount <= 256
 */
#pragma pack(push, 1)
struct CM256Header {
    uint16_t  s;
    uint8_t   n;
    uint8_t   k;
    uint8_t   i;
};
#pragma pack(pop)

void
cm256_parse_block(const void* buf, CM256Header& header);

bool
init_cm256();

#endif ///< ___HEADER_H___
