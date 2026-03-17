#ifndef ___FEC_CODEC_H___
#define ___FEC_CODEC_H___

#include "utils.h"

struct FecFragmentHeader {
    uint16_t    frame_number{ 0 };
    uint16_t    frame_size{ 0 };
    uint16_t    frag_offset{ 0 };
    uint16_t    frag_size{ 0 };

    bool is_empty() {
        return 0 == frame_number && 0 == frame_size && 0 == frag_offset && 0 == frag_size;
    }

    FecFragmentHeader to_network() {
        FecFragmentHeader network_header;

        network_header.frame_number = UINT16_TO_BE(frame_number);
        network_header.frame_size   = UINT16_TO_BE(frame_size);
        network_header.frag_offset  = UINT16_TO_BE(frag_offset);
        network_header.frag_size    = UINT16_TO_BE(frag_size);

        return network_header;
    }

    FecFragmentHeader to_host() {
        FecFragmentHeader host_header;

        host_header.frame_number = UINT16_FROM_BE(frame_number);
        host_header.frame_size   = UINT16_FROM_BE(frame_size);
        host_header.frag_offset  = UINT16_FROM_BE(frag_offset);
        host_header.frag_size    = UINT16_FROM_BE(frag_size);

        return host_header;
    }
};

struct FecHeader {
    unsigned char mgc2 : 1;
    unsigned char mgc1 : 1;
    unsigned char fecv : 4; /* must be 1 */
    unsigned char rtpv : 2; /* must be 0 */

    char          reserved;
    uint16_t      sequence_number;
};

/**  a fec packet memory layout
 *
 *   .________________________________________________________________________________________________________.
 *   |                      |                           |                                |                    |
 *   |   FecHeader(4bytes)  |   HeaderType (12 bytes)   |   FecFragmentHeader (8bytes)   |   data (n bytes)   |
 *   |                      |                           |                                |                    |
 *   `--------------------------------------------------------------------------------------------------------`
 *                                                      |<----------------- HeaderType::s ------------------->|
 *   |<----------------------------------------- fec packet buffer ------------------------------------------>|
 *
 */
class IFecPacket {
public:
    virtual ~IFecPacket() {}

    virtual bool get_header(FecHeader& header) const = 0;

    virtual const void* get_buffer() const = 0;

    virtual uint32_t get_buffer_size() const = 0;

    virtual const void* get_payload() const = 0;

    virtual uint32_t get_payload_size() const = 0;
};

#endif ///< ___FEC_CODEC_H___
