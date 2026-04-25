#ifndef ___FEC_ADAPTER_H___
#define ___FEC_ADAPTER_H___

#include <sstream>
#include <string>
#include <cstdint>
#include <vector>
#include <functional>

struct FecConfig {
    int32_t   block_size{ 0 };
    int32_t   blocks{ 0 };
    int32_t   red_blocks{ 0 };

    bool is_equal(const FecConfig& config) {
        return block_size == config.block_size && blocks == config.blocks && red_blocks == config.red_blocks;
    }

    std::string to_string() {
        std::ostringstream os;
        os << "[s: " << block_size << ", n: " << blocks << ", k: " << red_blocks << "]";

        return os.str();
    }
};

struct FecHeaderInfo {
    uint16_t   s;
    uint16_t   n;
    uint16_t   k;
    uint32_t   i;

    uint16_t   header_size;
    std::function<std::vector<uint8_t>(const FecHeaderInfo&)> pack;
};

class IFecEncoderAdapter;
class IFecEncoderAdapterObserver {
public:
    virtual ~IFecEncoderAdapterObserver() = default;

    virtual void on_encoder_output(IFecEncoderAdapter* adapter, uint16_t sequence, int32_t index, bool red, const uint8_t* data, int32_t len) = 0;
};

class IFecEncoderAdapter {
public:
    virtual ~IFecEncoderAdapter() = default;

    virtual bool create(FecConfig& config, uint16_t sequence, IFecEncoderAdapterObserver* observer) = 0;

    virtual void destroy() = 0;

    virtual void encode(const void* block, bool& done) = 0;
};

class IFecDecoderAdapter;
class IFecDecoderAdapterObserver {
public:
    virtual ~IFecDecoderAdapterObserver() = default;

    virtual void on_decoder_output(IFecDecoderAdapter* adapter, uint16_t sequence_number, int32_t index, const uint8_t* data, int32_t len) = 0;
};

class IFecDecoderAdapter {
public:
    virtual ~IFecDecoderAdapter() = default;

    virtual bool create(uint16_t sequence, IFecDecoderAdapterObserver* observer) = 0;

    virtual void destroy() = 0;

    virtual void decode(const void* block) = 0;

    virtual void flush() = 0;
};

#endif ///< ___FEC_ADAPTER_H___
