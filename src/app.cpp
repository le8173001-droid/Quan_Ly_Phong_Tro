#include "app.hpp"

#include <filesystem>

#include <fstream>
#include <iostream>
#include <sstream>

void add_route(httplib::Server& svr, const std::string& method, const std::string& pattern, Handler h) {
    auto wrapper = [h](const Req& req, Res& res) {
        try {
            json out = h(req, res);
            if (out.is_null()) {
                res.status = 204;
                return;
            }
            res.set_content(out.dump(-1, ' ', false, json::error_handler_t::replace), "application/json; charset=utf-8");
        } catch (const HttpError& e) {
            res.status = e.status;
            res.set_content(json{{"error", e.what()}, {"status", e.status}}.dump(), "application/json; charset=utf-8");
        } catch (const json::exception& e) {
            std::cerr << "[JSON] " << req.method << " " << req.path << ": " << e.what() << "\n";
            res.status = 400;
            res.set_content(json{{"error", "Dữ liệu gửi lên không hợp lệ"}, {"status", 400}}.dump(), "application/json; charset=utf-8");
        } catch (const std::exception& e) {
            std::cerr << "[LỖI] " << req.method << " " << req.path << ": " << e.what() << "\n";
            res.status = 500;
            res.set_content(json{{"error", "Lỗi máy chủ"}, {"status", 500}}.dump(), "application/json; charset=utf-8");
        }
    };
    if (method == "GET") svr.Get(pattern, wrapper);
    else if (method == "POST") svr.Post(pattern, wrapper);
    else if (method == "PUT") svr.Put(pattern, wrapper);
    else if (method == "PATCH") svr.Patch(pattern, wrapper);
    else if (method == "DELETE") svr.Delete(pattern, wrapper);
    else throw std::logic_error("method không hỗ trợ: " + method);
}

json parse_body(const Req& req) {
    if (req.body.empty()) bad_request("Thiếu nội dung JSON");
    json b = json::parse(req.body, nullptr, false);
    if (b.is_discarded() || !b.is_object()) bad_request("Nội dung gửi lên phải là một object JSON hợp lệ");
    return b;
}

std::string client_ip(const Req& req) { return req.remote_addr; }

void rate_limit(App& app, const Req& req, const std::string& bucket, size_t max_hits, int window_seconds) {
    if (!app.limiter.allow(bucket + "|" + client_ip(req), max_hits, std::chrono::seconds(window_seconds))) {
        throw HttpError(429, "Bạn thao tác quá nhanh, vui lòng thử lại sau ít phút");
    }
}

std::optional<User> current_user(App& app, const Req& req) {
    std::string h = req.get_header_value("Authorization");
    if (h.rfind("Bearer ", 0) != 0) return std::nullopt;
    auto claims = crypto::jwt_verify(h.substr(7), app.cfg.jwt_secret);
    if (!claims || !claims->contains("sub") || !(*claims)["sub"].is_number_integer()) return std::nullopt;
    json row = app.db.query_one(
        "SELECT u.user_id, u.username, u.full_name, u.status, r.role_code "
        "FROM users u JOIN roles r ON r.role_id = u.role_id WHERE u.user_id = ?",
        {(*claims)["sub"].get<long long>()});
    if (row.is_null() || row["status"] != "active") return std::nullopt;
    User u;
    u.id = row["userId"].get<long long>();
    u.username = row["username"].get<std::string>();
    u.full_name = row["fullName"].get<std::string>();
    u.role = row["roleCode"].get<std::string>();
    return u;
}

User require_user(App& app, const Req& req) {
    auto u = current_user(app, req);
    if (!u) unauthorized("Phiên đăng nhập không hợp lệ hoặc đã hết hạn, vui lòng đăng nhập lại");
    return *u;
}

User require_staff(App& app, const Req& req) {
    User u = require_user(app, req);
    if (!u.is_staff()) forbidden("Chỉ quản trị viên / quản lý mới được thực hiện thao tác này");
    return u;
}

User require_admin(App& app, const Req& req) {
    User u = require_user(app, req);
    if (!u.is_admin()) forbidden("Chỉ quản trị viên (ADMIN) mới được thực hiện thao tác này");
    return u;
}

bool owns_building(App& app, const User& u, long long building_id) {
    if (u.is_admin()) return true;
    if (!u.is_staff()) return false;
    return app.db.scalar("SELECT COUNT(*) FROM buildings WHERE building_id = ? AND manager_id = ?", {building_id, u.id}) > 0;
}

void require_building_owner(App& app, const User& u, long long building_id) {
    if (!owns_building(app, u, building_id))
        forbidden("Bạn không phải chủ của tòa nhà này nên không được thêm / sửa / xóa dữ liệu của tòa nhà (chỉ chủ tòa nhà hoặc ADMIN)");
}

Paging get_paging(const Req& req, long long default_size, long long max_size) {
    Paging p;
    p.size = default_size;
    if (auto v = util::parse_ll(param(req, "size"), "size")) p.size = *v;
    if (auto v = util::parse_ll(param(req, "page"), "page")) p.page = *v;
    if (p.page < 1) p.page = 1;
    if (p.size < 1) p.size = default_size;
    if (p.size > max_size) p.size = max_size;
    return p;
}

json page_response(json items, long long total, const Paging& p) {
    long long pages = total == 0 ? 0 : (total + p.size - 1) / p.size;
    return json{{"items", std::move(items)}, {"page", p.page}, {"size", p.size}, {"totalItems", total}, {"totalPages", pages}};
}

std::string param(const Req& req, const char* name) {
    if (!req.has_param(name)) return "";
    return util::trim(req.get_param_value(name));
}

long long path_id(const Req& req, const char* name) {
    auto it = req.path_params.find(name);
    if (it == req.path_params.end()) bad_request("Thiếu mã trong đường dẫn");
    auto v = util::parse_ll(it->second, name);
    if (!v) bad_request("Mã không hợp lệ");
    return *v;
}

void sync_building_total(App& app, long long building_id) {
    app.db.execute(
        "UPDATE buildings SET total_rooms = (SELECT COUNT(*) FROM rooms r WHERE r.building_id = buildings.building_id AND r.status <> 'hidden') "
        "WHERE building_id = ?",
        {building_id});
}

namespace {
std::string read_text_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("Không đọc được file: " + path);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}
}  // namespace

void init_database(App& app) {
    long long tables = app.db.scalar("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name='roles'");
    if (tables > 0) return;
    const std::string dir = std::filesystem::path(app.cfg.schema_path).parent_path().string();
    std::cout << "Đang tạo database mới từ " << app.cfg.schema_path << " ...\n";
    app.db.exec_script(read_text_file(app.cfg.schema_path));
    for (const char* name : {"seed_vn_admin.sql", "seed_demo.sql"}) {
        std::filesystem::path f = std::filesystem::path(dir) / name;
        if (!std::filesystem::exists(f)) {
            std::cout << "  (bỏ qua " << name << " vì không tìm thấy)\n";
            continue;
        }
        std::cout << "  nạp " << name << " ...\n";
        app.db.exec_script(read_text_file(f.string()));
    }
    app.db.exec_script("PRAGMA user_version = " + std::to_string(kSchemaVersion) + ";");
}
