// Khung ứng dụng: cấu hình, đối tượng App dùng chung, hàm hỗ trợ viết route và kiểm tra quyền.
#pragma once

#include "crypto.hpp"
#include "db.hpp"
#include "util.hpp"

#include <httplib.h>

#include <functional>
#include <optional>
#include <string>

struct Config {
    std::string host = "127.0.0.1";
    int port = 8080;
    std::string db_path = "data/nhatro.db";
    std::string schema_path;  // database/schema.sqlite.sql
    std::string web_dir;      // thư mục chứa giao diện (public)
    std::string upload_dir;   // thư mục lưu ảnh người dùng tải lên (mặc định: <thư mục database>/uploads)
    std::string jwt_secret;
    std::string google_client_id;
    std::string google_tokeninfo_url = "https://oauth2.googleapis.com/tokeninfo";
    int almost_empty_percent = 50;  // phòng "sắp trống": có người ở nhưng số người <= x% sức chứa
    bool reset = false;             // xóa database và nạp lại dữ liệu mẫu
};

struct User {
    long long id = 0;
    std::string username;
    std::string full_name;
    std::string role;  // ADMIN | MANAGER | TENANT
    bool is_staff() const { return role == "ADMIN" || role == "MANAGER"; }
    bool is_admin() const { return role == "ADMIN"; }
};

struct App {
    Config cfg;
    Db db;
    RateLimiter limiter;
    explicit App(Config c) : cfg(std::move(c)), db(cfg.db_path) {}
};

using Req = httplib::Request;
using Res = httplib::Response;
using Handler = std::function<json(const Req&, Res&)>;

// Đăng ký route: tự bắt lỗi, trả JSON. Handler trả về json (null => 204 No Content), có thể đặt res.status (vd 201).
void add_route(httplib::Server& svr, const std::string& method, const std::string& pattern, Handler h);

json parse_body(const Req& req);
std::string client_ip(const Req& req);
void rate_limit(App& app, const Req& req, const std::string& bucket, size_t max_hits, int window_seconds);

// Quyền
std::optional<User> current_user(App& app, const Req& req);
User require_user(App& app, const Req& req);
User require_staff(App& app, const Req& req);
User require_admin(App& app, const Req& req);
// Chủ tòa nhà: ADMIN được mọi tòa; MANAGER chỉ tòa nhà có buildings.manager_id = id của mình
bool owns_building(App& app, const User& u, long long building_id);
void require_building_owner(App& app, const User& u, long long building_id);

// Phân trang
struct Paging {
    long long page = 1;
    long long size = 12;
    long long offset() const { return (page - 1) * size; }
};
Paging get_paging(const Req& req, long long default_size = 12, long long max_size = 60);
json page_response(json items, long long total, const Paging& p);

std::string param(const Req& req, const char* name);  // giá trị tham số query đã trim, "" nếu không có
long long path_id(const Req& req, const char* name = "id");

// Kiểm tra độ mạnh mật khẩu (ném 400 nếu yếu)
void check_password_strength(const std::string& pw);

// Tính lại tổng số phòng (không tính phòng ẩn) của tòa nhà
void sync_building_total(App& app, long long building_id);

void register_auth_routes(httplib::Server& svr, App& app);
void register_room_routes(httplib::Server& svr, App& app);
void register_public_routes(httplib::Server& svr, App& app);
void register_admin_routes(httplib::Server& svr, App& app);
void register_location_routes(httplib::Server& svr, App& app);
void register_upload_routes(httplib::Server& svr, App& app);

constexpr int kSchemaVersion = 2;  // tăng khi đổi cấu trúc bảng (database cũ sẽ được sao lưu và tạo lại)

// Khởi tạo database (tạo bảng + địa giới VN + dữ liệu mẫu nếu còn trống)
void init_database(App& app);
