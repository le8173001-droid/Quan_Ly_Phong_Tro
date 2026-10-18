#include "db.hpp"

#include <sqlite3.h>

#include <cmath>
#include <iostream>

namespace {

constexpr double kPi = 3.14159265358979323846;

// vn_fold(text): chữ thường + bỏ dấu, dùng trong câu SQL để tìm kiếm tiếng Việt không dấu
void sql_vn_fold(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    if (argc != 1 || sqlite3_value_type(argv[0]) == SQLITE_NULL) {
        sqlite3_result_null(ctx);
        return;
    }
    const unsigned char* text = sqlite3_value_text(argv[0]);
    std::string folded = util::vn_fold(text ? reinterpret_cast<const char*>(text) : "");
    sqlite3_result_text(ctx, folded.c_str(), static_cast<int>(folded.size()), SQLITE_TRANSIENT);
}

// haversine(lat1, lng1, lat2, lng2): khoảng cách (km) giữa hai toạ độ
void sql_haversine(sqlite3_context* ctx, int argc, sqlite3_value** argv) {
    if (argc != 4) {
        sqlite3_result_null(ctx);
        return;
    }
    for (int i = 0; i < 4; ++i) {
        if (sqlite3_value_type(argv[i]) == SQLITE_NULL) {
            sqlite3_result_null(ctx);
            return;
        }
    }
    double lat1 = sqlite3_value_double(argv[0]) * kPi / 180.0;
    double lng1 = sqlite3_value_double(argv[1]) * kPi / 180.0;
    double lat2 = sqlite3_value_double(argv[2]) * kPi / 180.0;
    double lng2 = sqlite3_value_double(argv[3]) * kPi / 180.0;
    double a = std::pow(std::sin((lat2 - lat1) / 2), 2) +
               std::cos(lat1) * std::cos(lat2) * std::pow(std::sin((lng2 - lng1) / 2), 2);
    sqlite3_result_double(ctx, 6371.0 * 2 * std::atan2(std::sqrt(a), std::sqrt(1 - a)));
}

struct Stmt {
    sqlite3_stmt* s = nullptr;
    ~Stmt() {
        if (s) sqlite3_finalize(s);
    }
};

}  // namespace

Db::Db(const std::string& path) {
    if (sqlite3_open_v2(path.c_str(), &db_, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX, nullptr) != SQLITE_OK) {
        std::string msg = db_ ? sqlite3_errmsg(db_) : "không mở được file database";
        if (db_) sqlite3_close(db_);
        throw std::runtime_error("Không mở được database '" + path + "': " + msg);
    }
    sqlite3_extended_result_codes(db_, 1);
    sqlite3_busy_timeout(db_, 5000);
    sqlite3_create_function(db_, "vn_fold", 1, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, sql_vn_fold, nullptr, nullptr);
    sqlite3_create_function(db_, "haversine", 4, SQLITE_UTF8 | SQLITE_DETERMINISTIC, nullptr, sql_haversine, nullptr, nullptr);
    raw_exec("PRAGMA foreign_keys = ON");
    raw_exec("PRAGMA journal_mode = WAL");
    raw_exec("PRAGMA synchronous = NORMAL");
}

Db::~Db() {
    if (db_) sqlite3_close(db_);
}

void Db::throw_error(const char* where) {
    int code = sqlite3_extended_errcode(db_);
    std::string detail = sqlite3_errmsg(db_);
    switch (code) {
        case SQLITE_CONSTRAINT_UNIQUE:
        case SQLITE_CONSTRAINT_PRIMARYKEY:
            throw HttpError(409, "Dữ liệu bị trùng với bản ghi đã có (" + detail + ")");
        case SQLITE_CONSTRAINT_FOREIGNKEY:
            throw HttpError(409, "Không thực hiện được: dữ liệu đang được bản ghi khác sử dụng hoặc tham chiếu tới bản ghi không tồn tại");
        case SQLITE_CONSTRAINT_CHECK:
            throw HttpError(400, "Dữ liệu không hợp lệ (" + detail + ")");
        case SQLITE_CONSTRAINT_NOTNULL:
            throw HttpError(400, "Thiếu dữ liệu bắt buộc (" + detail + ")");
        default:
            break;
    }
    std::cerr << "[SQLite] " << where << ": " << detail << " (code " << code << ")\n";
    throw HttpError(500, "Lỗi cơ sở dữ liệu");
}

void Db::raw_exec(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "";
        sqlite3_free(err);
        std::cerr << "[SQLite] " << sql << ": " << msg << "\n";
        throw HttpError(500, "Lỗi cơ sở dữ liệu");
    }
}

void Db::exec_script(const std::string& sql) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    char* err = nullptr;
    if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
        std::string msg = err ? err : "";
        sqlite3_free(err);
        throw std::runtime_error("Lỗi chạy script SQL: " + msg);
    }
}

namespace {

void bind_all(sqlite3* db, sqlite3_stmt* st, const std::vector<json>& params) {
    for (size_t i = 0; i < params.size(); ++i) {
        const json& p = params[i];
        int idx = static_cast<int>(i) + 1;
        int rc = SQLITE_OK;
        if (p.is_null()) rc = sqlite3_bind_null(st, idx);
        else if (p.is_boolean()) rc = sqlite3_bind_int(st, idx, p.get<bool>() ? 1 : 0);
        else if (p.is_number_integer()) rc = sqlite3_bind_int64(st, idx, p.get<long long>());
        else if (p.is_number_unsigned()) rc = sqlite3_bind_int64(st, idx, static_cast<long long>(p.get<unsigned long long>()));
        else if (p.is_number_float()) rc = sqlite3_bind_double(st, idx, p.get<double>());
        else if (p.is_string()) {
            const std::string& s = p.get_ref<const std::string&>();
            rc = sqlite3_bind_text(st, idx, s.c_str(), static_cast<int>(s.size()), SQLITE_TRANSIENT);
        } else {
            throw HttpError(500, "Kiểu tham số SQL không được hỗ trợ");
        }
        if (rc != SQLITE_OK) {
            std::cerr << "[SQLite] bind lỗi: " << sqlite3_errmsg(db) << "\n";
            throw HttpError(500, "Lỗi cơ sở dữ liệu");
        }
    }
}

}  // namespace

json Db::query(const std::string& sql, const std::vector<json>& params) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    Stmt st;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st.s, nullptr) != SQLITE_OK) {
        std::cerr << "[SQLite] SQL sai: " << sqlite3_errmsg(db_) << "\n  " << sql << "\n";
        throw HttpError(500, "Lỗi cơ sở dữ liệu");
    }
    bind_all(db_, st.s, params);

    int ncol = sqlite3_column_count(st.s);
    std::vector<std::string> names;
    std::vector<bool> is_bool;
    for (int i = 0; i < ncol; ++i) {
        std::string raw = sqlite3_column_name(st.s, i);
        is_bool.push_back(raw.rfind("is_", 0) == 0 || raw.rfind("has_", 0) == 0);
        names.push_back(util::snake_to_camel(raw));
    }

    json rows = json::array();
    int rc;
    while ((rc = sqlite3_step(st.s)) == SQLITE_ROW) {
        json row = json::object();
        for (int i = 0; i < ncol; ++i) {
            switch (sqlite3_column_type(st.s, i)) {
                case SQLITE_INTEGER: {
                    long long v = sqlite3_column_int64(st.s, i);
                    if (is_bool[i]) row[names[i]] = (v != 0);
                    else row[names[i]] = v;
                    break;
                }
                case SQLITE_FLOAT: {
                    double d = sqlite3_column_double(st.s, i);
                    // số thực nguyên (ví dụ tiền DECIMAL(12,0)) trả về dạng số nguyên cho gọn
                    if (std::isfinite(d) && d == std::floor(d) && std::fabs(d) < 9e15) row[names[i]] = static_cast<long long>(d);
                    else row[names[i]] = d;
                    break;
                }
                case SQLITE_TEXT: {
                    const unsigned char* t = sqlite3_column_text(st.s, i);
                    row[names[i]] = std::string(reinterpret_cast<const char*>(t), static_cast<size_t>(sqlite3_column_bytes(st.s, i)));
                    break;
                }
                default:
                    row[names[i]] = nullptr;
            }
        }
        rows.push_back(std::move(row));
    }
    if (rc != SQLITE_DONE) throw_error("query");
    return rows;
}

json Db::query_one(const std::string& sql, const std::vector<json>& params) {
    json rows = query(sql, params);
    return rows.empty() ? json(nullptr) : rows[0];
}

long long Db::scalar(const std::string& sql, const std::vector<json>& params) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    Stmt st;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st.s, nullptr) != SQLITE_OK) {
        std::cerr << "[SQLite] SQL sai: " << sqlite3_errmsg(db_) << "\n  " << sql << "\n";
        throw HttpError(500, "Lỗi cơ sở dữ liệu");
    }
    bind_all(db_, st.s, params);
    int rc = sqlite3_step(st.s);
    if (rc == SQLITE_ROW) {
        return sqlite3_column_type(st.s, 0) == SQLITE_NULL ? 0 : sqlite3_column_int64(st.s, 0);
    }
    if (rc != SQLITE_DONE) throw_error("scalar");
    return 0;
}

long long Db::execute(const std::string& sql, const std::vector<json>& params) {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    Stmt st;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &st.s, nullptr) != SQLITE_OK) {
        std::cerr << "[SQLite] SQL sai: " << sqlite3_errmsg(db_) << "\n  " << sql << "\n";
        throw HttpError(500, "Lỗi cơ sở dữ liệu");
    }
    bind_all(db_, st.s, params);
    int rc = sqlite3_step(st.s);
    if (rc != SQLITE_DONE && rc != SQLITE_ROW) throw_error("execute");
    return sqlite3_changes(db_);
}

long long Db::last_insert_id() {
    std::lock_guard<std::recursive_mutex> lock(mu_);
    return sqlite3_last_insert_rowid(db_);
}
