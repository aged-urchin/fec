#ifndef ___FEC_PACKET_H___
#define ___FEC_PACKET_H___

#include "fec_codec.h"
#include <vector>

class FecPacket : public IFecPacket {
public:
    static FecPacket* create_instance(const uint8_t* data, int len, uint16_t sequence_number);

    static FecPacket* parse_from_buffer(const void* data, int len);

    bool get_header(FecHeader& header) const override;

    const void* get_buffer() const override;

    uint32_t get_buffer_size() const override;

    const void* get_payload() const override;

    uint32_t get_payload_size() const override;

private:
    FecPacket(const uint8_t* data, int len, uint16_t sequence_number);

private:

    FecHeader               m_header;
    std::vector<uint8_t>    m_data;
};

#endif ///< ___FEC_PACKET_H___
