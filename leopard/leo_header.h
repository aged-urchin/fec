#ifndef ___LEO_HEADER_H___
#define ___LEO_HEADER_H___

#include <cstdint>

/** Precondition: originalCount + recoveryCount <= 65536
 */
#pragma pack(push, 1)
struct LeopardHeader {
    uint16_t    s;
    uint16_t    n;
    uint16_t    k;
    uint16_t    i;
};
#pragma pack(pop)

void
leopard_parse_block(const void* buf, LeopardHeader& header);

bool
init_leopard();


#endif ///< ___LEO_HEADER_H___
