#ifndef ___FEC_ENCODER_BASE_H___
#define ___FEC_ENCODER_BASE_H___

#include "fec_codec.h"

#include <mutex>
#include <sstream>

struct BandFecEncoder;
class FecEncoderBase : public IFecEncoder {
public:
    FecEncoderBase(IFecEncoderObserver* observer);

    ~FecEncoderBase() override;

    bool set_param(int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group) override;

    void flush() override;

protected:
    BandFecEncoder* create_encoder();

    void destroy_encoder();

    virtual void do_flush() = 0;

private:
    friend void on_fec_send(BandFecEncoder* f, void* buf, size_t size, bool red, int64_t user_data1, int64_t user_data2);

    void on_new_block(uint16_t sequence, bool red, const uint8_t* data, int len);

private:

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
    uint16_t                    m_group_num{ 0 };
    IFecEncoderObserver*        m_observer{ nullptr };

protected:

    Config                      m_active_config;
    BandFecEncoder*             m_encoder{ nullptr };
    uint16_t                    m_frame_num{ 0 };

    std::mutex                  m_mutex;
};

#endif ///< ___FEC_ENCODER_BASE_H___