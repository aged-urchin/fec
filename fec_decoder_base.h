#ifndef ___FEC_DECODRE_BASE_H___
#define ___FEC_DECODRE_BASE_H___

#include "fec_codec.h"
#include <map>

struct BandFecHeaderType;
struct BandFecDecoder;

class FecDecoderBase : public IFecDecoder {
public:
    FecDecoderBase(IFecDecoderObserver* observer);

    ~FecDecoderBase() override;

    void set_max_forward_packets(int size) override;

    void decode(const uint8_t* data, int len) override;

    void loss_stats(PacketLossStats& stats) override;

protected:
    virtual void on_sequence_start(uint16_t sequence, const FecHeader* header, const BandFecHeaderType* bandfec_header) { }

    virtual void on_sequence_end(uint16_t sequence) { }

    virtual void on_new_block(uint16_t sequence_number, int32_t pos, const uint8_t* data, int len) = 0;

    void maybe_remove_outdated_decoders(int32_t reset_sequence_number = -1);

    void decode_fec_block(uint16_t sequence_number, const void* data, int len);

    void send_frame(uint16_t sequence_number, uint16_t frame_number, const uint8_t* data, int data_len);

    void destroy_decoders();

private:
    void remove_decoder(uint16_t sequence);

private:
    friend void on_fec_receive(BandFecDecoder* f, int64_t position, void* buf, int len, int64_t user_data1, int64_t user_data2);

    struct Decoder {
        int             max_consecutive_next_group_packets{ 0 }; ///< death counter
        BandFecDecoder* decoder{ nullptr };
    };

    int                                 m_max_forward_packets{ 3 };
    IFecDecoderObserver*                m_observer;
    std::map<uint16_t, Decoder*>        m_seq_decoders;   ///< FecHeader::sequence_number
};
#endif ///< ___FEC_DECODRE_BASE_H___
