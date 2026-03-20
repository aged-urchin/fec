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

/** rtp packet format                                                                  rtcp packet format
 *  0                   1                   2                   3                      0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1                    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |V=2|P|X| CC  |M|      PT     |         sequence number         |                  |V=2|P|    RC   |   PT          |             length            |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                            timestamp                          |                  |                     SSRC of sender/participant                |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 *  |            synchronization source (SSRC) identifier           |                  |                      payload (variable)                       |
 *  +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |             contributing source (CSRC) identifiers            |
 *  |                             ....                              |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  stun packet format                                                                 dtls packet format
 *  0                   1                   2                   3                      0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1                    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |0 0|     Type (14)             |         Length (16)           |                  | Content Type  |    Version    |         Epoch (2bytes)        |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                      Value (variable)                         |                  |                     Sequence Number (6bytes)                  |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |                     Padding (0-3bytes)                        |                  |             Length (2bytes)       |         Payload ...       |
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *
 *  // TODO: Should support error response.
 *  // For STUN packet, 0x00 is binding request, 0x01 is binding success response.
 *  bool srs_is_stun(const uint8_t *data, size_t size)
 *  {
 *      return size > 0 && (data[0] == 0 || data[0] == 1);
 *  }
 *
 *  // change_cipher_spec(20), alert(21), handshake(22), application_data(23)
 *  // @see https://tools.ietf.org/html/rfc2246#section-6.2.1
 *  bool srs_is_dtls(const uint8_t *data, size_t len)
 *  {
 *      return (len >= 13 && (data[0] > 19 && data[0] < 64));
 *  }
 *
 *  // For RTP or RTCP, the V=2 which is in the high 2bits, 0xC0 (1100 0000)
 *  bool srs_is_rtp_or_rtcp(const uint8_t *data, size_t len)
 *  {
 *      return (len >= 12 && (data[0] & 0xC0) == 0x80);
 *  }
 *
 *  // For RTCP, PT is [128, 223] (or without marker [0, 95]).
 *  // Literally, RTCP starts from 64 not 0, so PT is [192, 223] (or without marker [64, 95]).
 *  // @note For RTP, the PT is [96, 127], or [224, 255] with marker.
 *  bool srs_is_rtcp(const uint8_t *data, size_t len)
 *  {
 *      return (len >= 12) && (data[0] & 0x80) && (data[1] >= 192 && data[1] <= 223);
 *  }
 *
 *  srs_error_t SrsRtcSessionManager::on_udp_packet(SrsUdpMuxSocket *skt) {
 *      char *data = skt->data();
 *      int size = skt->size();
 *      bool is_rtp_or_rtcp = srs_is_rtp_or_rtcp((uint8_t *)data, size);
 *      bool is_rtcp = srs_is_rtcp((uint8_t *)data, size);
 *      ... ...
 *      if (!is_rtp_or_rtcp && srs_is_stun((uint8_t *)data, size)) {
 *          ... ...
 *      } else {
 *          if (srs_is_dtls((uint8_t *)data, size)) {
 *              ... ... 
 *          }
 *      }
 *  }
 *
 */
struct FecHeader {
    /** FEC:      11
     *  STUN:     00
     *  RTP/RTCP: 10
     *  DTLS:     not specified but should be within range [19, 64]
     */
    uint8_t sig : 2; ///< signature: must be '11', distinguish from DTLS/STUN/RTP/RTCP
    uint8_t typ : 2; ///< type:      header type
    uint8_t sid : 3; ///< stream id: identify multiply fec streams
    uint8_t red : 1; ///< redundant: 0 for original packet, 1 for redundant packet

    uint8_t     reserved;
    uint16_t    sequence_number;
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
