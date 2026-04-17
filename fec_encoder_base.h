#ifndef ___FEC_ENCODER_BASE_H___
#define ___FEC_ENCODER_BASE_H___

#include "fec_codec.h"
#include "fec_adapter.h"

#include <mutex>

class FecEncoderBase : public IFecEncoder,
                       public IFecEncoderAdapterObserver {
public:
    FecEncoderBase(FecType type, IFecEncoderObserver* observer);

    ~FecEncoderBase() override;

    bool set_block_size(int size_in_bytes) override;

    bool set_red_params(int blocks_in_group, int red_blocks_in_group) override;

    void encode(const uint8_t* data, int data_len) override;

    void flush() override;

protected:
    IFecEncoderAdapter* create_encoder();

    void destroy_encoder();

    bool do_set_block_size(int size_in_bytes);

    bool do_set_red_params(int blocks_in_group, int red_blocks_in_group);

    virtual void do_encode(const uint8_t* data, int data_len) = 0;

    virtual void do_flush() = 0;

    virtual FecHeader* make_fec_header(uint16_t sequence, bool red) = 0;

private:
    void on_encoder_output(IFecEncoderAdapter* adapter, uint16_t sequence, int32_t index, bool red, const uint8_t* data, int32_t len) override;

private:

    IFecEncoderObserver*        m_observer{ nullptr };
    bool                        m_flushed{ false };
    bool                        m_first_block{ true };
    uint16_t                    m_sequence_number{ kFristSeqNum };

    std::mutex                  m_mutex;

protected:

    const FecType               m_type;
    FecConfig                   m_config;
    FecConfig                   m_active_config;
    IFecEncoderAdapter*         m_encoder{ nullptr };
};

#endif ///< ___FEC_ENCODER_BASE_H___
