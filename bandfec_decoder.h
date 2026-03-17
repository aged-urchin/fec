#ifndef ___BANDFEC_DECODER_H___
#define ___BANDFEC_DECODER_H___

#include "fec_codec.h"
#include <vector>
#include <map>

struct FecDecoder;
class BandFecDecoder;

class IBandFecDecoderObserver {
public:
    virtual ~IBandFecDecoderObserver() = default;

    virtual void on_decoder_output(BandFecDecoder*  decoder,
                                   uint16_t         sequence_number,
                                   uint16_t         frame_number,
                                   const uint8_t*   data,
                                   int              data_len) = 0;
};

class BandFecDecoder {
public:
    BandFecDecoder(IBandFecDecoderObserver* observer);

    ~BandFecDecoder();

    void decode(const uint8_t* data, int len);

private:
    void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len);

    void remove_decoder(uint16_t sequence);

private:
    friend void on_fec_receive(FecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2);

    enum { kDeathCounterOnNoData = 3 };

    struct ReconstructedFrame {
        std::vector<uint8_t>    data;
        std::map<int, int>      slots;

        ReconstructedFrame(int size);
        bool ready();
        bool push_fragment(int offset, const uint8_t* data, int size);
    };

    struct Decoder {
        int             no_packets_cnt{ 0 }; ///< death counter
        FecDecoder*     decoder{ nullptr };
    };

    IBandFecDecoderObserver*                m_observer;
    std::map<uint16_t, ReconstructedFrame*> m_pending_frames; ///< FecFragmentHeader::frame_number
    std::map<uint16_t, Decoder*>            m_seq_decoders;   ///< FecHeader::sequence_number
};

#endif ///< ___BANDFEC_DECODER_H___
