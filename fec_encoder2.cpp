#include "fec_encoder2.h"
#include "bandfec.h"

#include <iostream>
#include <cassert>

FecEncoder2::FecEncoder2(IFecEncoderObserver* observer) :
FecEncoderBase(observer) {

}

void
FecEncoder2::encode(const uint8_t* data, int data_len) {
    std::lock_guard<std::mutex> lock(m_mutex);
}

void
FecEncoder2::do_flush() {

}
