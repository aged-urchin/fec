#ifndef ___FEC_DECODER_COMPACT_H___
#define ___FEC_DECODER_COMPACT_H___

#include "fec_decoder_base.h"
#include <vector>

class FecDecoderCompact : public FecDecoderBase {
public:
    FecDecoderCompact(FecType type, IFecDecoderObserver* observer);

    ~FecDecoderCompact() override;

    void destroy(PacketLossStats* stats);

private:
    void clean_old_frames(uint16_t frame_number);

    void on_new_block(uint16_t sequence_number, int32_t index, const uint8_t* data, int len, bool recovered) override;

private:
    struct ReconstructedFrame {
        std::vector<uint8_t>    data;
        std::map<int, int>      slots;
        int64_t                 creation_time;

        ReconstructedFrame(int size);

        bool ready();
        bool push_fragment(int offset, const uint8_t* data, int size);
    };

    std::map<uint16_t, ReconstructedFrame*> m_pending_frames; ///< FecFragmentHeader::frame_number
};

#endif ///< ___FEC_DECODER_COMPACT_H___
