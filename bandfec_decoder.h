#ifndef ___BANDFEC_DECODER_H___
#define ___BANDFEC_DECODER_H___

#include <mutex>
#include <vector>
#include <map>

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

    class Reorder {
    public:
        typedef std::map<uint32_t, std::vector<uint8_t>> BLOCKS;

        Reorder(uint32_t max_disorder = 6) : m_max_disorder(max_disorder) {

        }

        BLOCKS add_block(uint32_t index, const uint8_t* data, int len) {
            BLOCKS blocks;
            if (index >= m_expecting_index) {
                m_pending_blocks[index] = std::vector<uint8_t>(data, data + len);
            }

            if (!m_pending_blocks.count(m_expecting_index)) {
                if (index - m_expecting_index < m_max_disorder) {
                    return {};
                }
            }

            if (m_pending_blocks.empty()) {
                return {};
            }

            m_expecting_index = m_pending_blocks.begin()->first;
            while (m_pending_blocks.count(m_expecting_index)) {
                auto itr = m_pending_blocks.begin();

                blocks.insert(*itr);
                m_pending_blocks.erase(itr);

                ++m_expecting_index;
            }

            return blocks;
        }

        BLOCKS flush() {
            auto blocks = m_pending_blocks;
            m_pending_blocks.clear();

            return blocks;
        }

    private:

        const uint32_t      m_max_disorder{ 0 };
        uint32_t            m_expecting_index{ 1 };
        BLOCKS              m_pending_blocks;
    };

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

        BlockDecoder(const Config& _config);
        ~BlockDecoder();

        bool is_same(const Config& _config) const;
    };

    enum { kMaxDecoders = 1 };

    IBandFecDecoderObserver*            m_observer;
    std::map<uint32_t, BlockDecoder*>   m_decoders;
};

#endif ///< ___BANDFEC_DECODER_H___
