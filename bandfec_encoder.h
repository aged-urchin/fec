#ifndef ___BANDFEC_ENCODER_H___
#define ___BANDFEC_ENCODER_H___

#include <mutex>
#include <vector>

struct FecEncoder;
class BandFecEncoder;
class IFecPacket;

class IBandFecEncoderObserver {
public:
    virtual ~IBandFecEncoderObserver() = default;

    virtual void on_encoder_output(BandFecEncoder* encoder, IFecPacket* packet) = 0;
};

class BandFecEncoder {
public:
    BandFecEncoder(IBandFecEncoderObserver* observer);

    ~BandFecEncoder();

    bool set_param(int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group);

    void encode(const uint8_t* data, int data_len);

    void flush();

private:
    void on_new_block(uint16_t sequence, const uint8_t* data, int len);

    FecEncoder* create_encoder();

    void destroy_encoder();

private:

    friend void on_fec_send(FecEncoder* f, void* buf, size_t size, int64_t user_data1, int64_t user_data2);


    enum { kFecParamW = 40, kFecParamG = 4 };

    struct Config {
        int   block_size{ 0 };
        int   blocks{ 0 };
        int   red_blocks{ 0 };
    };

    Config                      m_config;
    Config                      m_active_config;
    IBandFecEncoderObserver*    m_observer{ nullptr };
    FecEncoder*                 m_encoder{ nullptr };
    uint16_t                    m_group_num{ 0 }; 
    uint16_t                    m_frame_num{ 0 };
    std::vector<uint8_t>        m_block;
 
    std::mutex                  m_mutex;
};

#endif ///< ___BANDFEC_ENCODER_H___
