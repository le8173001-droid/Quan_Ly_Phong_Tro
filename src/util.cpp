#include "util.hpp"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <regex>

namespace util {

std::string trim(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

std::string to_lower_ascii(std::string s) {
    for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

// ---------------------------------------------------------------- UTF-8
namespace {

// Giải mã một chuỗi UTF-8 thành danh sách code point (byte lỗi được giữ nguyên dưới dạng code point 0xFFFD).
std::u32string decode_utf8(const std::string& s) {
    std::u32string out;
    size_t i = 0;
    while (i < s.size()) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0xFFFD;
        int len = 1;
        if (c < 0x80) {
            cp = c;
        } else if ((c >> 5) == 0x6 && i + 1 < s.size()) {
            cp = ((c & 0x1F) << 6) | (s[i + 1] & 0x3F);
            len = 2;
        } else if ((c >> 4) == 0xE && i + 2 < s.size()) {
            cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
            len = 3;
        } else if ((c >> 3) == 0x1E && i + 3 < s.size()) {
            cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
            len = 4;
        }
        out.push_back(cp);
        i += len;
    }
    return out;
}

void append_utf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

// Bảng chữ cái tiếng Việt có dấu (cả hoa lẫn thường) -> chữ cái gốc
const std::unordered_map<char32_t, char>& fold_table() {
    static const std::unordered_map<char32_t, char> table = [] {
        std::unordered_map<char32_t, char> t;
        struct Group { char base; const char* letters; };
        const Group groups[] = {
            {'a', "àáạảãâầấậẩẫăằắặẳẵÀÁẠẢÃÂẦẤẬẨẪĂẰẮẶẲẴ"},
            {'e', "èéẹẻẽêềếệểễÈÉẸẺẼÊỀẾỆỂỄ"},
            {'i', "ìíịỉĩÌÍỊỈĨ"},
            {'o', "òóọỏõôồốộổỗơờớợởỡÒÓỌỎÕÔỒỐỘỔỖƠỜỚỢỞỠ"},
            {'u', "ùúụủũưừứựửữÙÚỤỦŨƯỪỨỰỬỮ"},
            {'y', "ỳýỵỷỹỲÝỴỶỸ"},
            {'d', "đĐ"},
        };
        for (const auto& g : groups) {
            for (char32_t cp : decode_utf8(g.letters)) t[cp] = g.base;
        }
        return t;
    }();
    return table;
}

}  // namespace

std::string vn_fold(const std::string& s) {
    const auto& table = fold_table();
    std::string out;
    out.reserve(s.size());
    for (char32_t cp : decode_utf8(s)) {
        if (cp >= 0x300 && cp <= 0x36F) continue;  // dấu rời (chuỗi NFD)
        auto it = table.find(cp);
        if (it != table.end()) {
            out.push_back(it->second);
        } else if (cp < 0x80) {
            out.push_back(static_cast<char>(std::tolower(static_cast<int>(cp))));
        } else {
            append_utf8(out, cp);
        }
    }
    return out;
}

std::string like_escape(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '!' || c == '%' || c == '_') out.push_back('!');
        out.push_back(c);
    }
    return out;
}

std::string digits_only(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isdigit(static_cast<unsigned char>(c))) out.push_back(c);
    }
    return out;
}

bool valid_username(const std::string& s) {
    static const std::regex re("^[A-Za-z0-9_.]{3,30}$");
    return std::regex_match(s, re);
}

bool valid_email(const std::string& s) {
    static const std::regex re("^[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\\.[A-Za-z]{2,}$");
    return s.size() <= 100 && std::regex_match(s, re);
}

bool valid_phone(const std::string& s) {
    std::string d = digits_only(s);
    if (d.rfind("84", 0) == 0 && d.size() >= 11) d = "0" + d.substr(2);
    return d.size() >= 9 && d.size() <= 11 && d[0] == '0';
}

bool valid_date(const std::string& s) {
    static const std::regex re("^(\\d{4})-(\\d{2})-(\\d{2})$");
    std::smatch m;
    if (!std::regex_match(s, m, re)) return false;
    int mo = std::stoi(m[2]), d = std::stoi(m[3]);
    return mo >= 1 && mo <= 12 && d >= 1 && d <= 31;
}

bool valid_month(const std::string& s) {
    static const std::regex re("^\\d{4}-(0[1-9]|1[0-2])$");
    return std::regex_match(s, re);
}

bool valid_datetime(const std::string& s) {
    static const std::regex re("^\\d{4}-\\d{2}-\\d{2}[ T]\\d{2}:\\d{2}(:\\d{2})?$");
    return std::regex_match(s, re) && valid_date(s.substr(0, 10));
}

std::string normalize_datetime(const std::string& s) {
    std::string r = s;
    if (r.size() > 10 && r[10] == 'T') r[10] = ' ';
    if (r.size() == 16) r += ":00";
    return r;
}

std::string snake_to_camel(const std::string& s) {
    std::string out;
    bool up = false;
    for (char c : s) {
        if (c == '_') {
            up = true;
        } else if (up) {
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            up = false;
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string camel_to_snake(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (std::isupper(static_cast<unsigned char>(c))) {
            out.push_back('_');
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        } else {
            out.push_back(c);
        }
    }
    return out;
}

std::string pad_int(long long v, int width) {
    std::string s = std::to_string(v);
    while (static_cast<int>(s.size()) < width) s = "0" + s;
    return s;
}

std::string mask_name(const std::string& name) {
    std::string out;
    bool first_word = true;
    size_t i = 0;
    while (i < name.size()) {
        while (i < name.size() && name[i] == ' ') ++i;
        size_t j = i;
        while (j < name.size() && name[j] != ' ') ++j;
        if (j > i) {
            std::string word = name.substr(i, j - i);
            if (!out.empty()) out.push_back(' ');
            if (first_word) {
                out += word;  // họ giữ nguyên
                first_word = false;
            } else {
                auto cps = decode_utf8(word);
                std::string masked;
                if (!cps.empty()) append_utf8(masked, cps[0]);
                masked += std::string(cps.size() > 1 ? 2 : 0, '*');
                out += masked;
            }
        }
        i = j;
    }
    return out;
}

std::string get_string(const json& b, const char* key, bool required, size_t max_len) {
    auto it = b.find(key);
    if (it == b.end() || it->is_null()) {
        if (required) bad_request(std::string("Thiếu trường bắt buộc: ") + key);
        return "";
    }
    if (!it->is_string()) bad_request(std::string("Trường '") + key + "' phải là chuỗi");
    std::string v = trim(it->get<std::string>());
    if (required && v.empty()) bad_request(std::string("Trường '") + key + "' không được để trống");
    if (v.size() > max_len) bad_request(std::string("Trường '") + key + "' quá dài (tối đa " + std::to_string(max_len) + " ký tự)");
    return v;
}

std::optional<long long> get_int(const json& b, const char* key) {
    auto it = b.find(key);
    if (it == b.end() || it->is_null()) return std::nullopt;
    if (it->is_number_integer()) return it->get<long long>();
    if (it->is_number_float()) {
        double d = it->get<double>();
        if (d == std::floor(d)) return static_cast<long long>(d);
    }
    if (it->is_string()) {
        std::string s = trim(it->get<std::string>());
        if (s.empty()) return std::nullopt;
        return parse_ll(s, key);
    }
    bad_request(std::string("Trường '") + key + "' phải là số nguyên");
}

std::optional<double> get_double(const json& b, const char* key) {
    auto it = b.find(key);
    if (it == b.end() || it->is_null()) return std::nullopt;
    if (it->is_number()) return it->get<double>();
    if (it->is_string()) {
        std::string s = trim(it->get<std::string>());
        if (s.empty()) return std::nullopt;
        return parse_double(s, key);
    }
    bad_request(std::string("Trường '") + key + "' phải là số");
}

std::optional<bool> get_bool(const json& b, const char* key) {
    auto it = b.find(key);
    if (it == b.end() || it->is_null()) return std::nullopt;
    if (it->is_boolean()) return it->get<bool>();
    if (it->is_number_integer()) return it->get<long long>() != 0;
    if (it->is_string()) {
        std::string s = to_lower_ascii(it->get<std::string>());
        if (s == "true" || s == "1") return true;
        if (s == "false" || s == "0") return false;
    }
    bad_request(std::string("Trường '") + key + "' phải là true/false");
}

std::optional<long long> parse_ll(const std::string& s, const char* name) {
    if (s.empty()) return std::nullopt;
    char* end = nullptr;
    errno = 0;
    long long v = std::strtoll(s.c_str(), &end, 10);
    if (errno != 0 || end == s.c_str() || *end != '\0') bad_request(std::string("Tham số '") + name + "' phải là số nguyên");
    return v;
}

std::optional<double> parse_double(const std::string& s, const char* name) {
    if (s.empty()) return std::nullopt;
    char* end = nullptr;
    errno = 0;
    double v = std::strtod(s.c_str(), &end);
    if (errno != 0 || end == s.c_str() || *end != '\0' || std::isnan(v) || std::isinf(v))
        bad_request(std::string("Tham số '") + name + "' phải là số");
    return v;
}

}  // namespace util

bool RateLimiter::allow(const std::string& key, size_t max_hits, std::chrono::seconds window) {
    std::lock_guard<std::mutex> lock(mu_);
    auto now = std::chrono::steady_clock::now();
    auto& q = hits_[key];
    while (!q.empty() && now - q.front() > window) q.pop_front();
    if (q.size() >= max_hits) return false;
    q.push_back(now);
    // dọn bớt khóa cũ để map không phình mãi
    if (hits_.size() > 5000) {
        for (auto it = hits_.begin(); it != hits_.end();) {
            if (it->second.empty() || now - it->second.back() > window) it = hits_.erase(it);
            else ++it;
        }
    }
    return true;
}
