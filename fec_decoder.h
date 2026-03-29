#ifndef ___FEC_DECODER_H___
#define ___FEC_DECODER_H___

#include "fec_codec.h"

#include <vector>
#include <map>

struct BandFecDecoder;
class FecDecoder : public IFecDecoder {
public:
    FecDecoder(IFecDecoderObserver* observer);

    ~FecDecoder() override;

    void set_reorder_window_size(int size) override;

    void decode(const uint8_t* data, int len) override;

private:
    void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len);

    void remove_decoder(uint16_t sequence);

private:
    friend void on_fec_receive(BandFecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2);

    struct ReconstructedFrame {
        std::vector<uint8_t>    data;
        std::map<int, int>      slots;

        ReconstructedFrame(int size);
        bool ready();
        bool push_fragment(int offset, const uint8_t* data, int size);
    };

    struct Decoder {
        int                 no_packets_cnt{ 0 }; ///< death counter
        BandFecDecoder*     decoder{ nullptr };
    };

    int                                     m_reorder_window_size{ 3 };
    IFecDecoderObserver*                    m_observer;
    std::map<uint16_t, ReconstructedFrame*> m_pending_frames; ///< FecFragmentHeader::frame_number
    std::map<uint16_t, Decoder*>            m_seq_decoders;   ///< FecHeader::sequence_number
};

#endif ///< ___FEC_DECODER_H___
