#ifndef ___FEC_CODEC_H___
#define ___FEC_CODEC_H___

#include <cstdint>

/** fec type
 */
enum FecType {
    kFecTypeNull,
    kFecTypeBand,   ///< [Align16][Phased out]
    kFecTypeRS,     ///< [MDS][Group256]
    kFecTypeFastRS, ///< [Align64][MDS][FAST]
};

/** fec mode
 */
enum FecMode {
    kFecModeNull,
    kFecModeCompact,
    kFecModeSoftRtp,
};

enum { kFirstSeqNum = 0, kFirstBlockIndex = 0 };
enum { kMaxContLossCount = 64 };
enum { kSoftRtpOneByteSizeMax = 127, kSoftRtpTwoByteSizeMax = 32767 };

/** for 'kFecModeCompact':
 *  protected by fec(e.g. as part of fec payload) and used to reconstruct udp packets from fec packets
 */
struct FecFragmentHeader {
    uint16_t    frame_number;
    uint16_t    frame_size;
    uint16_t    frag_offset;
    uint16_t    frag_size;
};

/** soft rtp (with 'FecHeader::typ' == 1)
 */
struct SoftRtp {
    uint16_t  base_sequence_num; ///< the sequence number of the first rtp packet from a series of consecutive rtp packets
    uint8_t   delta_size_bytes;  ///< total bytes 'delta_size' occupies

    /** delta packet size(relative to 'BandFecHeaderType::s')
     *
     * one byte length field(0 ~ 127):
     *  0
     *   0 1 2 3 4 5 6 7
     *  +-+-+-+-+-+-+-+-+
     *  |0| delta  size |
     *  +-+-+-+-+-+-+-+-+
     *
     *  two bytes length field(128 ~ 32767):
     *  0                   1
     *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *  |1|        delta  size          |
     *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
     *
     */
    uint8_t   delta_size[1];
};

/** rtp packet format                                                                  rtcp packet format
 *  0                   1                   2                   3                      0                   1                   2                   3
 *   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1                    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 *  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+                  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 *  |V=2|P|X|  CC   |M|      PT     |         sequence number       |                  |V=2|P|    RC   |       PT      |             length            |
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
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200) ///< warning C4200: nonstandard extension used: zero-sized array in struct/union
#endif
struct FecHeader {
    /** FEC:      11
     *  STUN:     00
     *  RTP/RTCP: 10
     *  DTLS:     not specified but should be within range [19, 64]
     */
    uint8_t     sig : 2; ///< signature: must be '11', distinguish from DTLS/STUN/RTP/RTCP
    uint8_t     ver : 2; ///< version:   version number
    uint8_t     typ : 2; ///< type:      FecType, 0 is kFecTypeBand, 1 is kFecTypeRS, 2 is kFecTypeFastRS
    uint8_t     mod : 2; ///< mode:      FecMode, 0 is kFecModeCompact, 1 is kFecModeSoftRtp

    uint8_t     red : 1; ///< redundant: 0 for original packet, 1 for redundant packet
    uint8_t     sid : 3; ///< stream id: identify multiply fec streams
    uint8_t     rsv : 4; ///< reserved

    uint16_t    sequence_number; ///< ranges from kFristSeqNum, kFristSeqNum + 1, ...

    /** variable-length extension field for additional FEC header information
     *  only valid and contains data when header type (typ) is non-zero
     *  [C4200]
     */
    char        ext[0];
};
#ifdef _MSC_VER
#pragma warning(pop)
#endif

/** lossrate statistics
 */
struct PacketLossStats {
    float    lossrate;                          ///< packet loss rate(in percentage) before recovery (-1 if unavailable)
    float    effective_lossrate;                ///< packet loss rate(in percentage) after recovery  (-1 if unavailable)
    int64_t  missing_groups;                    ///< # missing groups (-1 if unavailable)
    int64_t  loss_dist[kMaxContLossCount + 1];  ///< occurrences of successive packet losses
};

/**  fec packet memory layout
 *
 *   e.g. with 'kFecModeCompact & kFecTypeBand':
 *
 *   |<------------------------ extra header bytes (16 + 8*N bytes) ------------------------>| 
 *   .______________________________________________________________________________________________________________  ~~~  ________________________________________________________.
 *   |                      |                              |                                 |                      |     |                                 |                      |
 *   |  FecHeader(4 bytes)  | BandFecHeaderType (12 bytes) |   FecFragmentHeader0 (8 bytes)  |    data0 (n bytes)   |     |   FecFragmentHeaderN (8 bytes)  |    dataN (n bytes)   |
 *   |                      |                              |                                 |                      |     |                                 |                      |
 *   `--------------------------------------------------------------------------------------------------------------  ~~~  --------------------------------------------------------`
 *                                                                                           |<-- protected data -->|                                       |<-- protected data -->|
 *                                                         |<-----------------------------------------------  BandFecHeaderType::s ----------------------------------------------->|
 *                          |<----------------------------------------------------------- payload (bandfec data block) ----------------------------------------------------------->|
 *   |<------------------------------------------------------------------------------ packet buffer ------------------------------------------------------------------------------>|
 *
 *
 *   e.g. with 'kFecModeSoftRtp & kFecTypeBand':
 *
 *   |<-------- extra header bytes (12 + N bytes)--------->| 
 *   .______________________________________________________________________________________________________________.
 *   |                      |                              |                                                        |
 *   |  FecHeader(N bytes)  | BandFecHeaderType (12 bytes) |                    data (n bytes)                      |
 *   |                      |                              |                                                        |
 *   `--------------------------------------------------------------------------------------------------------------`
 *                                                         |<------------------ protected data -------------------->|
 *                                                         |<------------------- HeaderType::s -------------------->|
 *                          |<---------------------------- payload (bandfec data block) --------------------------->|
 *   |<---------------------------------------------- packet buffer ----------------------------------------------->|
 *
 */
class IFecPacket {
public:
    /** increase a reference to this object
     */
    virtual unsigned long retain() = 0;
    /** decrease a reference to this object
     */
    virtual unsigned long release() = 0;
    /** get header(in host byte order)
     */
    virtual const FecHeader* get_header() const = 0;
    /** get packet buffer(header among which is in network byte order)
     */
    virtual const void* get_packet_buffer() const = 0;
    /** get packet buffer size in bytes
     */
    virtual uint32_t get_packet_buffer_size() const = 0;
    /** get payload(fec block) buffer
     */
    virtual const void* get_payload() const = 0;
    /** get payload size in bytes
     */
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
    /** set block size(in bytes)
     */
    virtual bool set_block_size(int size_in_bytes) = 0;
    /** set group size(number data blocks and number redundant blocks)
     */
    virtual bool set_red_params(int blocks_in_group, int red_blocks_in_group) = 0;
    /** encode one frame (e.g. one udp/rtp packet)
     */
    virtual void encode(const uint8_t* data, int data_len) = 0;
    /** flush the encoder
     *  (should be called at the end of the stream)
     */
    virtual void flush() = 0;
};

class IFecDecoder;
class IFecDecoderObserver {
public:
    virtual ~IFecDecoderObserver() = default;
    /** return one reconstructed frame (e.g. one udp packet for 'kFecModeCompact' or one rtp packet for 'kFecModeSoftRtp')
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
    /** set the maximum number of subsequent out-of-order packets
     *  that can be received before forcing completion and processing of the current unfinished data sequence.
     *  (default is 3)
     */
    virtual void set_max_forward_packets(int packets) = 0;
    /** set expire time for all pending data(e.g. rtp packets or frames being reconstructed)
     *  (default is 3000ms)
     */
    virtual void set_max_packet_lifetime(const int64_t max_lifetime_ms) = 0;
    /** set window size of stats
     */
    virtual void set_stats_window_size(const int32_t wnd_ms) = 0;
    /** decode one fec packet (e.g. from an udp socket)
     */
    virtual void decode(const uint8_t* data, int len) = 0;
    /** get current packet loss statistics
     */
    virtual void loss_stats(PacketLossStats& stats) = 0;
    /** get fec mode
     */
    virtual FecMode mode() = 0;
};

/** APIs
 */
IFecEncoder*
create_fec_encoder(FecType type, FecMode mode, IFecEncoderObserver* observer);

void
destroy_fec_encoder(IFecEncoder* encoder);

IFecDecoder*
create_fec_decoder(FecType type, FecMode mode, IFecDecoderObserver* observer);

void
destroy_fec_decoder(IFecDecoder* decoder, PacketLossStats* stats = nullptr);

IFecDecoder*
create_fec_decoder2(IFecDecoderObserver* observer);

void
destroy_fec_decoder2(IFecDecoder* decoder, PacketLossStats* stats = nullptr);

#endif ///< ___FEC_CODEC_H___
