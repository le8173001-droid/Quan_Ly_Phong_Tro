// Mã hóa: SHA-256, HMAC, PBKDF2 (băm mật khẩu), JWT HS256. Tự cài đặt để không cần thư viện ngoài (OpenSSL...).
#pragma once

#include <nlohmann/json.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace crypto {

using json = nlohmann::json;

std::string sha256_hex(const std::string& data);
std::string hmac_sha256(const std::string& key, const std::string& msg);  // trả về 32 byte nhị phân

std::string to_hex(const std::string& bin);
std::string b64url_encode(const std::string& bin);
std::string b64url_decode(const std::string& text);  // ném std::runtime_error nếu sai định dạng

std::string random_hex(size_t bytes);
bool constant_time_equal(const std::string& a, const std::string& b);

// Định dạng: pbkdf2_sha256$<số vòng>$<salt hex>$<hash hex>
std::string hash_password(const std::string& password);
bool verify_password(const std::string& password, const std::string& stored);
std::string pbkdf2_sha256_hex(const std::string& password, const std::string& salt_bin, int iterations);

// JWT HS256. Payload nên có "exp" (giây kể từ epoch).
std::string jwt_sign(const json& payload, const std::string& secret);
std::optional<json> jwt_verify(const std::string& token, const std::string& secret);

}  // namespace crypto
