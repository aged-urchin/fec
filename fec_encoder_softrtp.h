#ifndef ___FEC_ENCODER_SOFTRTP_H___
#define ___FEC_ENCODER_SOFTRTP_H___

#include "fec_encoder_base.h"
#include "./utils/number_unwrapper.h"

#include <map>
#include <set>
#include <vector>

class FecEncoderSoftRtp : public FecEncoderBase {
public:
    FecEncoderSoftRtp(FecType type, IFecEncoderObserver* observer);

    bool set_red_params(int blocks_in_group, int red_blocks_in_group) override;

private:
    void do_set_max_reorder_depth(int packets) override;

    void do_encode(const uint8_t* data, int data_len) override;

    void do_flush() override;

    FecHeader* make_fec_header(uint16_t sequence, bool red) override;

    void encode_group();

    bool append_packet_to_group(const uint8_t* data, int data_len, uint16_t rtp_seq);

    void flush_ordered_group();

    void drain_pending_packets();

    bool is_reorder_window_full();

    void handle_reorder_window_full();

    void reset();

private:

    /** maximum gap tolerated between the latest received RTP sequence and the
     *  earliest missing one before we give up waiting and start a new run from the earliest pending packet.
     */
    static constexpr int kDefaultMaxReorderDepth = 8;

    int                                      m_max_packet_len{ 0 };
    uint16_t                                 m_base_rtp_sequence_number{ 0 };
    /** SSRC is bound once by the first accepted RTP packet and must stay unchanged for the whole encoder lifetime.
     */
    int64_t                                  m_ssrc{ -1 };
    std::vector<uint8_t>                     m_delta_sizes;
    /** consecutive packets that have already been drained from the reorder buffer and appended to the current FEC group.
     */
    std::vector<std::vector<uint8_t>>        m_packets;
    /** the unwrapper expands the 16-bit rtp sequence numbers into a monotonic 64-bit space.
     */
    NumberUnwrapper<uint16_t>                m_seq_unwrapper;
    /** packets older than this sequence have either been consumed into a FEC group or explicitly abandoned after the reorder window overflowed.
     */
    int64_t                                  m_min_acceptable_rtp_sequence{ -1 };
    /** the earliest not-yet-consumed sequence number we are still waiting for in the global input stream, even across FEC group boundaries.
     */
    int64_t                                  m_expected_rtp_sequence{ -1 };
    /** highest unwrapped sequence number currently known to the reorder buffer.
     *  used together with m_expected_rtp_sequence to detect when the wait for a missing packet has grown too large.
     */
    int64_t                                  m_latest_rtp_sequence{ -1 };
    /** out-of-order packets keyed by unwrapped RTP sequence number.
     */
    std::map<int64_t, std::vector<uint8_t>>  m_pending_packets;
    /** duplicate filter for packets that are still pending in the reorder buffer.
     */
    std::set<int64_t>                        m_seen_rtp_sequences;
    /** the reorder window size
     */
    int                                      m_max_reorder_depth{ kDefaultMaxReorderDepth };
};

#endif ///< ___FEC_ENCODER_SOFTRTP_H___
