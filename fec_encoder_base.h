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

    bool set_block_size(int size_in_bytes) override;

    bool set_red_params(int blocks_in_group, int red_blocks_in_group) override;

    void encode(const uint8_t* data, int data_len) override;

    void flush() override;

protected:
    BandFecEncoder* create_encoder();

    void destroy_encoder();

    bool do_set_block_size(int size_in_bytes);

    bool do_set_red_params(int blocks_in_group, int red_blocks_in_group);

    virtual void do_encode(const uint8_t* data, int data_len) = 0;

    virtual void do_flush() = 0;

    virtual FecHeader* make_fec_header(uint16_t sequence, bool red) = 0;

private:
    friend void on_fec_send(BandFecEncoder* f, void* buf, size_t size, bool red, int64_t user_data1, int64_t user_data2);

    void on_new_block(uint16_t sequence, bool red, const uint8_t* data, int len);

private:

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

    enum {
        kFecParamW = 40, kFecParamG = 4,
    };

    bool                        m_flushed{ false };
    bool                        m_first_block{ true };
    uint16_t                    m_sequence_number{ kFristSeqNum };
    IFecEncoderObserver*        m_observer{ nullptr };

    std::mutex                  m_mutex;

protected:

    Config                      m_config;
    Config                      m_active_config;
    BandFecEncoder*             m_encoder{ nullptr };
};

#endif ///< ___FEC_ENCODER_BASE_H___
