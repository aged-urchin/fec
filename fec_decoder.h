#ifndef ___FEC_DECODER_H___
#define ___FEC_DECODER_H___

#include "fec_decoder_base.h"
#include <vector>

class FecDecoder : public FecDecoderBase {
public:
    FecDecoder(IFecDecoderObserver* observer);

    void destroy();

private:
    void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len, bool recovered) override;

private:
    struct ReconstructedFrame {
        std::vector<uint8_t>    data;
        std::map<int, int>      slots;

        ReconstructedFrame(int size);

        bool ready();
        bool push_fragment(int offset, const uint8_t* data, int size);
    };

    std::map<uint16_t, ReconstructedFrame*> m_pending_frames; ///< FecFragmentHeader::frame_number
};

#endif ///< ___FEC_DECODER_H___
