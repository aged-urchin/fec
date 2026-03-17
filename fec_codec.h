#ifndef ___FEC_CODEC_H___
#define ___FEC_CODEC_H___

#include "utils.h"

/** protected by fec(e.g. as part of fec payload) and used to reconstruct udp packets from fec packets
 */
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
    /** TODO: add semantics to distinguish us from other kinds of packets(e.g. STUN/RTP/RTCP)
     */
    unsigned char mgc2 : 1;
    unsigned char mgc1 : 1;
    unsigned char fecv : 4;
    unsigned char rtpv : 2;

    char          reserved;
    uint16_t      sequence_number;
};

/**  a fec packet memory layout
 *
 *   |<------------------------------- extra header bytes ------------------------------->| 
 *   .___________________________________________________________________________________________________________.
 *   |                      |                           |                                 |                      |
 *   |  FecHeader(4 bytes)  |   HeaderType (12 bytes)   |   FecFragmentHeader (8 bytes)   |    data (n bytes)    |
 *   |                      |                           |                                 |                      |
 *   `-----------------------------------------------------------------------------------------------------------`
 *                                                                                        |<----- user data ---->|
 *                                                      |<------------------ HeaderType::s --------------------->|
 *                          |<----------------------------- one bandfec data block ----------------------------->|
 *   |<----------------------------------------- fec packet buffer --------------------------------------------->|
 *
 */
class IFecPacket {
public:
    virtual unsigned long retain() = 0;

    virtual unsigned long release() = 0;

    virtual bool get_header(FecHeader& header) const = 0;

    /** get header buffer and data buffer as a whole(e.g. FecHeader + HeaderType + FecFragmentHeader + user data)
     */
    virtual const void* get_buffer() const = 0;

    virtual uint32_t get_buffer_size() const = 0;

    /** get fec block buffer(e.g. HeaderType + FecFragmentHeader + user data)
     */
    virtual const void* get_payload() const = 0;

    virtual uint32_t get_payload_size() const = 0;

protected:
    virtual ~IFecPacket() = default;
};

class IFecEncoder;
class IFecEncoderObserver {
public:
    virtual ~IFecEncoderObserver() = default;

    virtual void on_encoder_output(IFecEncoder* encoder, IFecPacket* packet) = 0;
};

class IFecEncoder {
public:
    virtual ~IFecEncoder() = default;

    virtual bool set_param(int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group) = 0;

    /** encode one frame (e.g. one udp packet)
     */
    virtual void encode(const uint8_t* data, int data_len) = 0;

    virtual void flush() = 0;
};

class IFecDecoder;
class IFecDecoderObserver {
public:
    virtual ~IFecDecoderObserver() = default;

    /** return one reconstructed frame (e.g. one udp packet)
     */
    virtual void on_decoder_output(IFecDecoder*     decoder,
                                   uint16_t         sequence_number,
                                   uint16_t         frame_number,
                                   const uint8_t*   data,
                                   int              data_len) = 0;
};

class IFecDecoder {
public:
    virtual ~IFecDecoder() = default;

    virtual void set_reorder_window_size(int size) = 0;

    /** decode one fec packet (e.g. from an udp socket)
     */
    virtual void decode(const uint8_t* data, int len) = 0;
};

IFecEncoder*
create_fec_encoder(IFecEncoderObserver* observer);

void
destroy_fec_encoder(IFecEncoder* encoder);

IFecDecoder*
create_fec_decoder(IFecDecoderObserver* observer);

void
destroy_fec_decoder(IFecDecoder* decoder);

#endif ///< ___FEC_CODEC_H___
