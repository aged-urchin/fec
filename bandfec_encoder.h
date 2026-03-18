#ifndef ___BANDFEC_ENCODER_H___
#define ___BANDFEC_ENCODER_H___

#include "fec_codec.h"

#include <mutex>
#include <vector>
#include <sstream>

struct FecEncoder;
class BandFecEncoder;
class IFecPacket;

class BandFecEncoder : public IFecEncoder {
public:
    BandFecEncoder(IFecEncoderObserver* observer);

    ~BandFecEncoder() override;

    bool set_param(int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group) override;

    void encode(const uint8_t* data, int data_len) override;

    void flush() override;

private:
    void on_new_block(uint16_t sequence, bool red, const uint8_t* data, int len);

    FecEncoder* create_encoder();

    void destroy_encoder();

private:

    friend void on_fec_send(FecEncoder* f, void* buf, size_t size, bool red, int64_t user_data1, int64_t user_data2);


    enum { kFecParamW = 40, kFecParamG = 4 };

    struct Config {
        int   block_size{ 0 };
        int   blocks{ 0 };
        int   red_blocks{ 0 };

        bool is_equal(const Config& config) {
            return block_size == config.block_size && blocks == config.blocks && red_blocks == config.red_blocks;
        }

        std::string to_string() {
            std::ostringstream os;
            os << "[s: " << block_size << ", n: " << blocks << ", k: " << red_blocks << "]";

            return os.str();
        }
    };

    Config                      m_config;
    Config                      m_active_config;
    IFecEncoderObserver*        m_observer{ nullptr };
    FecEncoder*                 m_encoder{ nullptr };
    uint16_t                    m_group_num{ 0 }; 
    uint16_t                    m_frame_num{ 0 };
    std::vector<uint8_t>        m_block;
 
    std::mutex                  m_mutex;
};

#endif ///< ___BANDFEC_ENCODER_H___
