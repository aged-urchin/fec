#ifndef ___BANDFEC_DECODER_H___
#define ___BANDFEC_DECODER_H___

#include "reorder.h"
#include <mutex>

struct FecDecoder;
class BandFecDecoder;

class IBandFecDecoderObserver {
public:
    virtual ~IBandFecDecoderObserver() = default;

    virtual void on_decoder_output(BandFecDecoder* decoder, uint32_t sequence, int32_t pos, const uint8_t* data, int len) = 0;
};

class BandFecDecoder {
public:
    BandFecDecoder(IBandFecDecoderObserver* observer);

    ~BandFecDecoder();

    void max_reorder_delay(int delay_in_packets);

    void decode(uint32_t sequence, const uint8_t* data, int len);

private:
    void delete_decoder(uint32_t sequence);

    void on_new_block(uint32_t sequence, int32_t pos, const uint8_t* data, int len);

private:
    friend void on_fec_receive(FecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2);

    struct BlockDecoder {
        struct Config {
            uint16_t   block_size;
            uint16_t   blocks;
            uint16_t   red_blocks;
            uint8_t    param_w;
            uint8_t    param_g;
        } config;

        int             blocks{ 0 };
        FecDecoder*     decoder{ nullptr };
        Reorder         reorder;
        int             no_packets_cnt{ 0 }; ///< death counter
        const int       kDeathCounterOnNoData = 6;

        BlockDecoder(const Config& _config);
        ~BlockDecoder();

        bool is_same(const Config& _config) const;
    };

    enum { kMaxDecoders = 2 };

    IBandFecDecoderObserver*            m_observer;
    std::map<uint32_t, BlockDecoder*>   m_decoders;
};

#endif ///< ___BANDFEC_DECODER_H___
