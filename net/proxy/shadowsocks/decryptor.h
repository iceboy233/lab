#ifndef _NET_PROXY_SHADOWSOCKS_DECRYPTOR_H
#define _NET_PROXY_SHADOWSOCKS_DECRYPTOR_H

#include <cstdint>
#include <vector>

#include "absl/container/fixed_array.h"
#include "base/types.h"
#include "net/proxy/shadowsocks/pre-shared-key.h"
#include "net/proxy/shadowsocks/session-subkey.h"

namespace net {
namespace proxy {
namespace shadowsocks {

class Decryptor {
public:
    Decryptor();

    bool init(const PreSharedKey &pre_shared_key);
    bool start_chunk(size_t size);
    uint8_t pop_u8();
    uint16_t pop_big_u16();
    uint8_t *pop_buffer(size_t size);
    void finish_chunk();
    void advance(size_t size) { buffer_last_ += size; }

    BufferSpan buffer();

private:
    SessionSubkey session_subkey_;
    absl::FixedArray<uint8_t, 0> buffer_;
    size_t buffer_first_ = 0;
    size_t buffer_last_ = 0;
};

}  // namespace shadowsocks
}  // namespace proxy
}  // namespace net

#endif  // _NET_PROXY_SHADOWSOCKS_DECRYPTOR_H
