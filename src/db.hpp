// Lớp bọc SQLite: một kết nối duy nhất, có khóa mutex nên dùng an toàn từ nhiều luồng của web server.
#pragma once

#include "util.hpp"

#include <functional>
#include <mutex>
#include <string>
#include <vector>

struct sqlite3;

class Db {
public:
    explicit Db(const std::string& path);
    ~Db();
    Db(const Db&) = delete;
    Db& operator=(const Db&) = delete;

    // Chạy nhiều câu lệnh SQL (dùng khi nạp schema).
    void exec_script(const std::string& sql);

    // Trả về mảng các object. Tên cột snake_case được đổi sang camelCase; cột "is_*" đổi thành true/false.
    json query(const std::string& sql, const std::vector<json>& params = {});
    // Trả về hàng đầu tiên hoặc null.
    json query_one(const std::string& sql, const std::vector<json>& params = {});
    // Giá trị ô đầu tiên của hàng đầu tiên (số nguyên), mặc định 0.
    long long scalar(const std::string& sql, const std::vector<json>& params = {});
    // INSERT/UPDATE/DELETE, trả về số dòng bị ảnh hưởng.
    long long execute(const std::string& sql, const std::vector<json>& params = {});
    long long last_insert_id();

    // Chạy f() trong một transaction; lỗi thì ROLLBACK. Cho phép lồng nhau.
    template <class F>
    auto tx(F&& f) -> decltype(f()) {
        std::lock_guard<std::recursive_mutex> lock(mu_);
        bool outer = (depth_ == 0);
        if (outer) raw_exec("BEGIN IMMEDIATE");
        ++depth_;
        try {
            if constexpr (std::is_void_v<decltype(f())>) {
                f();
                --depth_;
                if (outer) raw_exec("COMMIT");
            } else {
                auto r = f();
                --depth_;
                if (outer) raw_exec("COMMIT");
                return r;
            }
        } catch (...) {
            --depth_;
            if (outer) {
                try { raw_exec("ROLLBACK"); } catch (...) {}
            }
            throw;
        }
    }

private:
    void raw_exec(const char* sql);
    [[noreturn]] void throw_error(const char* where);

    sqlite3* db_ = nullptr;
    std::recursive_mutex mu_;
    int depth_ = 0;
};
