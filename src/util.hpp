// Tiện ích dùng chung: lỗi HTTP, xử lý chuỗi UTF-8 (tìm kiếm không dấu), kiểm tra dữ liệu, giới hạn tần suất.
#pragma once

#include <nlohmann/json.hpp>

#include <chrono>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>

using json = nlohmann::json;

// Lỗi có mã HTTP. Ném ở bất cứ đâu trong handler, tầng route sẽ đổi thành JSON {"error": "..."}.
struct HttpError : std::runtime_error {
    int status;
    HttpError(int s, const std::string& msg) : std::runtime_error(msg), status(s) {}
};

[[noreturn]] inline void bad_request(const std::string& m) { throw HttpError(400, m); }
[[noreturn]] inline void unauthorized(const std::string& m = "Bạn cần đăng nhập") { throw HttpError(401, m); }
[[noreturn]] inline void forbidden(const std::string& m = "Bạn không có quyền thực hiện thao tác này") { throw HttpError(403, m); }
[[noreturn]] inline void not_found(const std::string& m = "Không tìm thấy dữ liệu") { throw HttpError(404, m); }
[[noreturn]] inline void conflict(const std::string& m) { throw HttpError(409, m); }

namespace util {

std::string trim(const std::string& s);
std::string to_lower_ascii(std::string s);

// Chữ thường + bỏ dấu tiếng Việt ("Gò Vấp" -> "go vap"), dùng cho tìm kiếm không phân biệt hoa/thường và dấu.
std::string vn_fold(const std::string& s);

// Thoát ký tự đặc biệt của LIKE, dùng kèm "ESCAPE '!'".
std::string like_escape(const std::string& s);

std::string digits_only(const std::string& s);
bool valid_username(const std::string& s);
bool valid_email(const std::string& s);
bool valid_phone(const std::string& s);   // số điện thoại Việt Nam (9-11 chữ số, có thể có +84)
bool valid_date(const std::string& s);    // YYYY-MM-DD
bool valid_month(const std::string& s);   // YYYY-MM
bool valid_datetime(const std::string& s);
std::string normalize_datetime(const std::string& s);

std::string snake_to_camel(const std::string& s);
std::string camel_to_snake(const std::string& s);

std::string pad_int(long long v, int width);

// "Trần Văn Bình" -> "Trần V** B***" (che bớt tên khi tra cứu hóa đơn công khai)
std::string mask_name(const std::string& name);

// Lấy giá trị từ body JSON
std::string get_string(const json& b, const char* key, bool required = false, size_t max_len = 500);
std::optional<long long> get_int(const json& b, const char* key);
std::optional<double> get_double(const json& b, const char* key);
std::optional<bool> get_bool(const json& b, const char* key);

// Đọc tham số query (?key=...), ném 400 nếu sai định dạng
std::optional<long long> parse_ll(const std::string& s, const char* name);
std::optional<double> parse_double(const std::string& s, const char* name);

}  // namespace util

// Giới hạn số lần gọi theo khóa (thường là IP) trong một khoảng thời gian.
class RateLimiter {
public:
    // Trả về true nếu còn được phép; false nếu đã vượt giới hạn.
    bool allow(const std::string& key, size_t max_hits, std::chrono::seconds window);

private:
    std::mutex mu_;
    std::unordered_map<std::string, std::deque<std::chrono::steady_clock::time_point>> hits_;
};
