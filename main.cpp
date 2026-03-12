#include "network_conditioner.h"
#include "bandfec_encoder.h"
#include "bandfec_decoder.h"

#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <windows.h>

RandomLossTool traffic(LossRateType::LOSS_30_PERCENT);

const int kFecParamS = 1024;
const int kFecParamN = 10;
const int kFecParamK = 10;

int num_pass = 0, num_rejects = 0, out_packets = 0, out_size = 0, recv_packets = 0, packet_lost = 0;

class Foo : public IBandFecEncoderObserver,
            public IBandFecDecoderObserver {
public:
    void start() {
        m_encoder = new BandFecEncoder(this);
        m_decoder = new BandFecDecoder(this);

        m_encoder->set_param(kFecParamS, kFecParamN, kFecParamK);
    }

    void stop() {
        delete m_encoder;
        m_encoder = nullptr;

        delete m_decoder;
        m_decoder = nullptr;
    }

    void push_data(const std::vector<uint8_t>& data) {
        m_encoder->encode(data.data(), data.size());
    }

private:
    void on_encoder_output(BandFecEncoder* encoder, uint32_t sequence, const uint8_t* data, int len) override {
        auto block_idx = get_block_index(data, len);

        ++out_packets;
        out_size += len;

		if (!traffic.is_packet_lost()) {
            ++num_pass;
            std::cout << u8"\u2714" << " [" << std::setw(7) << block_idx << "]: ";

            m_decoder->decode(sequence, data, len);
        } else {
            ++num_rejects;
            std::cout << u8"\u2718" << " [" << std::setw(7) << block_idx << "]: ";
        }

        if (!m_new_line) {
            std::cout << std::endl;
        }
        m_new_line = false;
    }

    void on_decoder_output(BandFecDecoder* decoder, uint32_t sequence, int32_t pos, const uint8_t* data, int len) override {
        if (m_new_line) {
            std::cout << std::string(13, ' ');
        }

        std::cout << "recv[" << recv_packets << "]: " << pos << "(" << len << ")" << std::endl;

        m_new_line = true;
        ++recv_packets;
    }

private:

    bool                 m_new_line{ false };
    BandFecEncoder*      m_encoder{ nullptr };
    BandFecDecoder*      m_decoder{ nullptr };
};

int main() {
    system("chcp 65001");

    const int kReadSize = 1000; ///< kFecParamS
    std::vector<uint8_t> data(kReadSize);

    auto f = fopen("news.264", "rb");
    fseek(f, 0, SEEK_END);
    auto file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    int in_packets = 0, in_size = 0, progress = -1;

    Foo foo;
    foo.start();
    do {
        if (fread(data.data(), 1, kReadSize, f) < kReadSize) {
            break;
        }

        foo.push_data(data);
        in_size += kReadSize;

        auto new_progress = (int)(in_size * 100.f / file_size);
        if (new_progress >= progress + 1) {
            SetConsoleTitleA((std::to_string(new_progress) + "%").c_str());
            progress = new_progress;
        }
    } while (true);

    foo.stop();

    in_packets = in_size / kFecParamS;
    fclose(f);

    std::cout << "\n==================================================\n";
    std::cout << "in_packets  : " << in_packets << ", in_size : " << in_size << std::endl;
    std::cout << "out_packets : " << out_packets << ", out_size: " << out_size << std::endl;
    std::cout << "recv_packets: " << recv_packets << std::endl;
    std::cout << "\n--------------------------------------------------\n";
    std::cout << "redundancy: " << (out_size - in_size) * 100. / in_size << "\%" << std::endl;
    std::cout << "recovered: " << num_rejects - (in_packets - recv_packets) << std::endl;

    std::cout << "pass: " << num_pass << ", lost: " << num_rejects << std::endl;
    std::cout << "loss rate before fec: " << std::setprecision(3) << num_rejects * 100. / (num_pass + num_rejects) << "\%" << std::endl;
    std::cout << "loss rate after  fec: " << std::setprecision(3) << (in_packets - recv_packets) * 100. / in_packets << "\%" << std::endl;
}
