#include "crypto.hpp"

#include <array>
#include <chrono>
#include <cstring>
#include <random>
#include <stdexcept>

namespace crypto {

namespace {

constexpr uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }

// Trạng thái SHA-256 có thể sao chép (để HMAC/PBKDF2 tái sử dụng phần pad cố định, chạy nhanh hơn)
class Sha256 {
public:
    Sha256() { reset(); }

    void reset() {
        h_ = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
        len_ = 0;
        buf_len_ = 0;
    }

    void update(const uint8_t* data, size_t n) {
        len_ += n;
        while (n > 0) {
            size_t take = std::min(n, size_t(64) - buf_len_);
            std::memcpy(buf_ + buf_len_, data, take);
            buf_len_ += take;
            data += take;
            n -= take;
            if (buf_len_ == 64) {
                compress(buf_);
                buf_len_ = 0;
            }
        }
    }
    void update(const std::string& s) { update(reinterpret_cast<const uint8_t*>(s.data()), s.size()); }

    std::string finish() {
        uint64_t bit_len = len_ * 8;
        uint8_t pad = 0x80;
        update(&pad, 1);
        uint8_t zero = 0;
        while (buf_len_ != 56) update(&zero, 1);
        uint8_t lenbuf[8];
        for (int i = 0; i < 8; ++i) lenbuf[i] = static_cast<uint8_t>(bit_len >> (56 - 8 * i));
        update(lenbuf, 8);
        std::string out(32, '\0');
        for (int i = 0; i < 8; ++i) {
            out[i * 4] = static_cast<char>(h_[i] >> 24);
            out[i * 4 + 1] = static_cast<char>(h_[i] >> 16);
            out[i * 4 + 2] = static_cast<char>(h_[i] >> 8);
            out[i * 4 + 3] = static_cast<char>(h_[i]);
        }
        return out;
    }

private:
    void compress(const uint8_t* block) {
        uint32_t w[64];
        for (int i = 0; i < 16; ++i) {
            w[i] = (uint32_t(block[i * 4]) << 24) | (uint32_t(block[i * 4 + 1]) << 16) |
                   (uint32_t(block[i * 4 + 2]) << 8) | uint32_t(block[i * 4 + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h_[0], b = h_[1], c = h_[2], d = h_[3], e = h_[4], f = h_[5], g = h_[6], h = h_[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t t1 = h + S1 + ch + K[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t t2 = S0 + maj;
            h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h_[0] += a; h_[1] += b; h_[2] += c; h_[3] += d; h_[4] += e; h_[5] += f; h_[6] += g; h_[7] += h;
    }

    std::array<uint32_t, 8> h_{};
    uint64_t len_ = 0;
    uint8_t buf_[64]{};
    size_t buf_len_ = 0;
};

// HMAC với phần pad đã tính sẵn
class Hmac {
public:
    explicit Hmac(const std::string& key) {
        std::string k = key;
        if (k.size() > 64) {
            Sha256 s;
            s.update(k);
            k = s.finish();
        }
        k.resize(64, '\0');
        std::string ipad(64, '\0'), opad(64, '\0');
        for (int i = 0; i < 64; ++i) {
            ipad[i] = static_cast<char>(k[i] ^ 0x36);
            opad[i] = static_cast<char>(k[i] ^ 0x5c);
        }
        inner_.update(ipad);
        outer_.update(opad);
    }

    std::string compute(const std::string& msg) const {
        Sha256 in = inner_;
        in.update(msg);
        std::string ih = in.finish();
        Sha256 out = outer_;
        out.update(ih);
        return out.finish();
    }

private:
    Sha256 inner_, outer_;
};

const char* B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

}  // namespace

std::string to_hex(const std::string& bin) {
    static const char* d = "0123456789abcdef";
    std::string out;
    out.reserve(bin.size() * 2);
    for (unsigned char c : bin) {
        out.push_back(d[c >> 4]);
        out.push_back(d[c & 15]);
    }
    return out;
}

static std::string from_hex(const std::string& hex) {
    if (hex.size() % 2 != 0) throw std::runtime_error("hex không hợp lệ");
    auto val = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        throw std::runtime_error("hex không hợp lệ");
    };
    std::string out;
    for (size_t i = 0; i < hex.size(); i += 2) out.push_back(static_cast<char>(val(hex[i]) * 16 + val(hex[i + 1])));
    return out;
}

std::string sha256_hex(const std::string& data) {
    Sha256 s;
    s.update(data);
    return to_hex(s.finish());
}

std::string hmac_sha256(const std::string& key, const std::string& msg) { return Hmac(key).compute(msg); }

std::string b64url_encode(const std::string& in) {
    std::string out;
    size_t i = 0;
    while (i + 2 < in.size()) {
        uint32_t v = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8) | uint8_t(in[i + 2]);
        out.push_back(B64[v >> 18]);
        out.push_back(B64[(v >> 12) & 63]);
        out.push_back(B64[(v >> 6) & 63]);
        out.push_back(B64[v & 63]);
        i += 3;
    }
    if (i + 1 == in.size()) {
        uint32_t v = uint8_t(in[i]) << 16;
        out.push_back(B64[v >> 18]);
        out.push_back(B64[(v >> 12) & 63]);
    } else if (i + 2 == in.size()) {
        uint32_t v = (uint8_t(in[i]) << 16) | (uint8_t(in[i + 1]) << 8);
        out.push_back(B64[v >> 18]);
        out.push_back(B64[(v >> 12) & 63]);
        out.push_back(B64[(v >> 6) & 63]);
    }
    return out;
}

std::string b64url_decode(const std::string& text) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '-') return 62;
        if (c == '_') return 63;
        throw std::runtime_error("base64url không hợp lệ");
    };
    std::string out;
    uint32_t acc = 0;
    int bits = 0;
    for (char c : text) {
        acc = (acc << 6) | static_cast<uint32_t>(val(c));
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((acc >> bits) & 0xFF));
        }
    }
    return out;
}

std::string random_hex(size_t bytes) {
    std::random_device rd;
    std::string bin;
    while (bin.size() < bytes) {
        uint32_t v = rd();
        for (int i = 0; i < 4 && bin.size() < bytes; ++i) bin.push_back(static_cast<char>((v >> (8 * i)) & 0xFF));
    }
    return to_hex(bin);
}

bool constant_time_equal(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (size_t i = 0; i < a.size(); ++i) diff |= static_cast<unsigned char>(a[i] ^ b[i]);
    return diff == 0;
}

std::string pbkdf2_sha256_hex(const std::string& password, const std::string& salt, int iterations) {
    Hmac mac(password);
    // 32 byte đầu ra = đúng 1 khối của PBKDF2 (i = 1)
    std::string u = mac.compute(salt + std::string("\x00\x00\x00\x01", 4));
    std::string t = u;
    for (int i = 1; i < iterations; ++i) {
        u = mac.compute(u);
        for (size_t j = 0; j < 32; ++j) t[j] = static_cast<char>(t[j] ^ u[j]);
    }
    return to_hex(t);
}

namespace {
constexpr int kIterations = 100000;
}

std::string hash_password(const std::string& password) {
    std::string salt_hex = random_hex(16);
    return "pbkdf2_sha256$" + std::to_string(kIterations) + "$" + salt_hex + "$" +
           pbkdf2_sha256_hex(password, from_hex(salt_hex), kIterations);
}

bool verify_password(const std::string& password, const std::string& stored) {
    // tách 4 phần bởi '$'
    std::string parts[4];
    size_t start = 0;
    for (int i = 0; i < 4; ++i) {
        size_t pos = stored.find('$', start);
        if (i < 3 && pos == std::string::npos) return false;
        parts[i] = stored.substr(start, pos == std::string::npos ? std::string::npos : pos - start);
        start = pos + 1;
    }
    if (parts[0] != "pbkdf2_sha256") return false;
    int iter = 0;
    try {
        iter = std::stoi(parts[1]);
        if (iter < 1000 || iter > 5000000) return false;
        return constant_time_equal(pbkdf2_sha256_hex(password, from_hex(parts[2]), iter), parts[3]);
    } catch (...) {
        return false;
    }
}

std::string jwt_sign(const json& payload, const std::string& secret) {
    std::string head = b64url_encode(R"({"alg":"HS256","typ":"JWT"})");
    std::string body = b64url_encode(payload.dump());
    std::string sig = b64url_encode(hmac_sha256(secret, head + "." + body));
    return head + "." + body + "." + sig;
}

std::optional<json> jwt_verify(const std::string& token, const std::string& secret) {
    try {
        size_t p1 = token.find('.');
        size_t p2 = token.find('.', p1 == std::string::npos ? 0 : p1 + 1);
        if (p1 == std::string::npos || p2 == std::string::npos) return std::nullopt;
        std::string head_b64 = token.substr(0, p1), body_b64 = token.substr(p1 + 1, p2 - p1 - 1), sig_b64 = token.substr(p2 + 1);
        json head = json::parse(b64url_decode(head_b64));
        if (head.value("alg", "") != "HS256") return std::nullopt;  // chặn alg=none và các thuật toán khác
        std::string expected = b64url_encode(hmac_sha256(secret, head_b64 + "." + body_b64));
        if (!constant_time_equal(expected, sig_b64)) return std::nullopt;
        json payload = json::parse(b64url_decode(body_b64));
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        if (!payload.contains("exp") || !payload["exp"].is_number() || payload["exp"].get<long long>() < now) return std::nullopt;
        return payload;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace crypto
