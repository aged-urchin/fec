#ifndef ___BANDFEC_ENCODER_H___
#define ___BANDFEC_ENCODER_H___

#include <mutex>
#include <vector>

struct FecEncoder;
class BandFecEncoder;

uint32_t
get_block_index(const uint8_t* buf, size_t size);

class IBandFecEncoderObserver {
public:
    virtual ~IBandFecEncoderObserver() = default;

    virtual void on_encoder_output(BandFecEncoder* encoder, uint32_t sequence, const uint8_t* data, int len) = 0;
};

class BandFecEncoder {
public:
    BandFecEncoder(IBandFecEncoderObserver* observer);

    ~BandFecEncoder();

    void set_param(int block_size_in_bytes, int data_blocks_in_group, int redundant_blocks_in_group);

    void encode(const uint8_t* data, int data_len);

private:
    void on_new_block(uint32_t sequence, const uint8_t* data, int len);

private:

    friend void on_fec_send(FecEncoder* f, void* buf, size_t size, int64_t user_data1, int64_t user_data2);

    struct GroupEncoder {
        const struct Config {
            int   block_size{ 1024 };
            int   blocks{ 10 };
            int   red_blocks{ 10 };
        } config;

        FecEncoder*             encoder{ nullptr };

        int                     block_idx{ 0 };
        std::vector<uint8_t>    block;

        GroupEncoder(const Config& _config);
        ~GroupEncoder();

        bool is_same(const Config& _config) const;
    };

    IBandFecEncoderObserver*    m_observer{ nullptr };

    GroupEncoder*               m_main_encoder{ nullptr };
    GroupEncoder*               m_secondary_encoder{ nullptr };

    uint32_t                    m_sequence{ 0 };
 
    std::mutex                  m_mutex;
};

#endif ///< ___BANDFEC_ENCODER_H___
