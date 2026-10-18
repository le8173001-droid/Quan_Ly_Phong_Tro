// NhàTrọ+ - web server quản lý / tìm thuê phòng trọ viết bằng C++17.
// Chạy:  nhatro [--port 8080] [--host 127.0.0.1] [--db data/nhatro.db] [--web public] [--reset]
#include "app.hpp"

#include <csignal>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <iostream>

#include <sqlite3.h>

namespace fs = std::filesystem;

namespace {

httplib::Server* g_server = nullptr;

void on_signal(int) {
    if (g_server) g_server->stop();
}

std::string env_or(const char* name, const std::string& fallback) {
    const char* v = std::getenv(name);
    return (v && *v) ? v : fallback;
}

// Tìm đường dẫn đầu tiên tồn tại trong danh sách ứng viên (thử theo thư mục chạy và theo vị trí file exe)
std::string find_existing(const std::vector<std::string>& candidates, const fs::path& exe_dir) {
    for (const auto& c : candidates) {
        fs::path p(c);
        std::error_code ec;
        if (fs::exists(p, ec)) return fs::absolute(p).string();
        if (p.is_relative() && fs::exists(exe_dir / p, ec)) return fs::absolute(exe_dir / p).string();
    }
    return "";
}

void print_help() {
    std::cout << "NhàTrọ+ server\n"
                 "  --port N      cổng web (mặc định 8080, hoặc biến môi trường PORT)\n"
                 "  --host H      địa chỉ lắng nghe (mặc định 127.0.0.1; dùng 0.0.0.0 để máy khác trong mạng truy cập)\n"
                 "  --db FILE     file database SQLite (mặc định data/nhatro.db)\n"
                 "  --web DIR     thư mục giao diện (mặc định tự tìm thư mục 'public')\n"
                 "  --reset       xóa database cũ và nạp lại dữ liệu mẫu\n"
                 "Biến môi trường: GOOGLE_CLIENT_ID, GOOGLE_TOKENINFO_URL, JWT_SECRET, DB_PATH, WEB_DIR, HOST, PORT\n";
}

// Database tạo bởi phiên bản cũ (cấu trúc bảng khác) -> đổi tên thành *.old-<giờ> để tạo database mới, không làm mất dữ liệu cũ
void backup_if_outdated(const fs::path& db_path) {
    std::error_code ec;
    if (!fs::exists(db_path, ec)) return;
    sqlite3* h = nullptr;
    if (sqlite3_open_v2(db_path.string().c_str(), &h, SQLITE_OPEN_READONLY, nullptr) != SQLITE_OK) {
        if (h) sqlite3_close(h);
        return;
    }
    int version = 0;
    bool has_tables = false;
    sqlite3_stmt* st = nullptr;
    if (sqlite3_prepare_v2(h, "PRAGMA user_version", -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW) version = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    st = nullptr;
    if (sqlite3_prepare_v2(h, "SELECT COUNT(*) FROM sqlite_master WHERE type='table'", -1, &st, nullptr) == SQLITE_OK && sqlite3_step(st) == SQLITE_ROW)
        has_tables = sqlite3_column_int(st, 0) > 0;
    sqlite3_finalize(st);
    sqlite3_close(h);
    if (!has_tables || version >= kSchemaVersion) return;
    char stamp[32];
    std::time_t now = std::time(nullptr);
    std::strftime(stamp, sizeof stamp, "%Y%m%d-%H%M%S", std::localtime(&now));
    for (const char* suffix : {"", "-wal", "-shm"}) {
        fs::path from = db_path.string() + suffix;
        if (fs::exists(from, ec)) fs::rename(from, db_path.string() + ".old-" + stamp + suffix, ec);
    }
    std::cout << "Database cũ có cấu trúc khác phiên bản này -> đã sao lưu thành " << db_path.filename().string() << ".old-" << stamp
              << " và tạo database mới.\n";
}

}  // namespace

int main(int argc, char** argv) {
    Config cfg;
    cfg.host = env_or("HOST", cfg.host);
    cfg.port = std::atoi(env_or("PORT", "8080").c_str());
    cfg.db_path = env_or("DB_PATH", cfg.db_path);
    cfg.web_dir = env_or("WEB_DIR", "");
    cfg.jwt_secret = env_or("JWT_SECRET", "");
    cfg.google_client_id = env_or("GOOGLE_CLIENT_ID", "");
    cfg.google_tokeninfo_url = env_or("GOOGLE_TOKENINFO_URL", cfg.google_tokeninfo_url);

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto next = [&](const char* name) -> std::string {
            if (i + 1 >= argc) {
                std::cerr << "Thiếu giá trị cho " << name << "\n";
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--port") cfg.port = std::atoi(next("--port").c_str());
        else if (a == "--host") cfg.host = next("--host");
        else if (a == "--db") cfg.db_path = next("--db");
        else if (a == "--web") cfg.web_dir = next("--web");
        else if (a == "--reset") cfg.reset = true;
        else if (a == "--help" || a == "-h") {
            print_help();
            return 0;
        } else {
            std::cerr << "Tham số không hợp lệ: " << a << "\n";
            print_help();
            return 2;
        }
    }
    if (cfg.port < 1 || cfg.port > 65535) {
        std::cerr << "Cổng không hợp lệ\n";
        return 2;
    }

    try {
        fs::path exe_dir = fs::absolute(fs::path(argv[0])).parent_path();
        if (cfg.web_dir.empty()) cfg.web_dir = find_existing({"public", "../public", "../../public"}, exe_dir);
        cfg.schema_path = find_existing({"database/schema.sqlite.sql", "../database/schema.sqlite.sql", "../../database/schema.sqlite.sql"}, exe_dir);
        if (cfg.web_dir.empty() || !fs::is_directory(cfg.web_dir)) {
            std::cerr << "Không tìm thấy thư mục giao diện 'public'. Hãy chạy từ thư mục gốc của dự án hoặc dùng --web <thư mục>.\n";
            return 1;
        }
        if (cfg.schema_path.empty()) {
            std::cerr << "Không tìm thấy database/schema.sqlite.sql. Hãy chạy từ thư mục gốc của dự án.\n";
            return 1;
        }

        fs::path db_path = fs::absolute(cfg.db_path);
        cfg.db_path = db_path.string();
        fs::create_directories(db_path.parent_path());
        if (cfg.upload_dir.empty()) cfg.upload_dir = (db_path.parent_path() / "uploads").string();
        fs::create_directories(cfg.upload_dir);
        if (!cfg.reset) backup_if_outdated(db_path);
        if (cfg.reset) {
            for (const char* suffix : {"", "-wal", "-shm"}) {
                std::error_code ec;
                fs::remove(db_path.string() + suffix, ec);
            }
            std::cout << "Đã xóa database cũ.\n";
        }

        // Khóa ký JWT: lấy từ biến môi trường, nếu không có thì tạo một lần và lưu cạnh file database
        if (cfg.jwt_secret.empty()) {
            fs::path secret_file = db_path.parent_path() / "jwt.secret";
            std::ifstream in(secret_file);
            if (!std::getline(in, cfg.jwt_secret) || cfg.jwt_secret.size() < 32) {
                cfg.jwt_secret = crypto::random_hex(32);
                std::ofstream(secret_file) << cfg.jwt_secret << "\n";
            }
        } else if (cfg.jwt_secret.size() < 16) {
            std::cerr << "JWT_SECRET phải có ít nhất 16 ký tự\n";
            return 1;
        }

        App app(cfg);
        init_database(app);

        httplib::Server svr;
        g_server = &svr;
        std::signal(SIGINT, on_signal);
        std::signal(SIGTERM, on_signal);

        svr.set_payload_max_length(12 * 1024 * 1024);  // tối đa 12 MB mỗi request (ảnh tải lên tối đa 5 MB/ảnh)
        svr.new_task_queue = [] { return new httplib::ThreadPool(16); };

        register_auth_routes(svr, app);
        register_room_routes(svr, app);
        register_public_routes(svr, app);
        register_location_routes(svr, app);
        register_upload_routes(svr, app);
        register_admin_routes(svr, app);

        if (!svr.set_mount_point("/uploads", cfg.upload_dir)) {
            std::cerr << "Không mount được thư mục ảnh tải lên: " << cfg.upload_dir << "\n";
            return 1;
        }
        if (!svr.set_mount_point("/", cfg.web_dir)) {
            std::cerr << "Không mount được thư mục giao diện: " << cfg.web_dir << "\n";
            return 1;
        }
        // Giao diện của 5 thành viên nằm ở thanh-vien-N-*/web (cùng cấp với thư mục public); URL giữ nguyên như khi chung một thư mục.
        {
            std::error_code ec;
            fs::path root = fs::absolute(cfg.web_dir, ec).parent_path();
            std::vector<fs::path> member_dirs;
            for (const auto& e : fs::directory_iterator(root, ec))
                if (e.is_directory() && e.path().filename().string().rfind("thanh-vien-", 0) == 0 && fs::is_directory(e.path() / "web"))
                    member_dirs.push_back(e.path() / "web");
            std::sort(member_dirs.begin(), member_dirs.end());
            for (const auto& d : member_dirs) svr.set_mount_point("/", d.string());
        }

        svr.set_error_handler([](const Req& req, Res& res) {
            if (req.path.rfind("/api/", 0) == 0) {
                if (!res.body.empty()) return;  // handler đã tự trả lỗi JSON
                json err = {{"error", res.status == 404 ? "Không tìm thấy API này" : (res.status == 405 ? "Phương thức không được hỗ trợ" : "Lỗi")},
                            {"status", res.status}};
                res.set_content(err.dump(), "application/json; charset=utf-8");
            } else if (res.status == 404) {
                res.set_content("<!doctype html><meta charset=utf-8><title>404</title><h1>404</h1><p>Không tìm thấy trang.</p><a href=\"/\">Về trang chủ</a>",
                                "text/html; charset=utf-8");
            }
        });

        svr.set_exception_handler([](const Req&, Res& res, std::exception_ptr ep) {
            try {
                std::rethrow_exception(ep);
            } catch (const std::exception& e) {
                std::cerr << "[LỖI] " << e.what() << "\n";
            } catch (...) {
            }
            res.status = 500;
            res.set_content(R"({"error":"Lỗi máy chủ","status":500})", "application/json; charset=utf-8");
        });

        svr.set_post_routing_handler([](const Req& req, Res& res) {
            res.set_header("X-Content-Type-Options", "nosniff");
            res.set_header("Referrer-Policy", "strict-origin-when-cross-origin");
            res.set_header("X-Frame-Options", "SAMEORIGIN");
            res.set_header("Cache-Control", req.path.rfind("/api/", 0) == 0 ? "no-store" : "no-cache");
            if (res.get_header_value("Content-Type").rfind("text/html", 0) == 0) {
                res.set_header("Content-Security-Policy",
                               "default-src 'self'; script-src 'self' https://accounts.google.com; "
                               "style-src 'self' 'unsafe-inline' https://accounts.google.com; img-src 'self' data: https:; "
                               "connect-src 'self' https://accounts.google.com; frame-src https://accounts.google.com; "
                               "base-uri 'self'; form-action 'self'");
            }
        });

        std::cout << "NhàTrọ+ đang chạy:  http://" << (cfg.host == "0.0.0.0" ? "localhost" : cfg.host) << ":" << cfg.port << "\n"
                  << "  Giao diện : " << cfg.web_dir << "\n"
                  << "  Database  : " << cfg.db_path << "\n"
                  << "  Ảnh tải lên: " << cfg.upload_dir << "\n"
                  << "  Google    : " << (cfg.google_client_id.empty() ? "chưa cấu hình (đặt biến GOOGLE_CLIENT_ID để bật đăng nhập Google)" : "đã bật") << "\n"
                  << "Nhấn Ctrl+C để dừng.\n";
        if (!svr.listen(cfg.host, cfg.port)) {
            std::cerr << "Không mở được cổng " << cfg.port << " (có thể đang bị chương trình khác dùng). Thử: --port 8081\n";
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Lỗi khởi động: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
