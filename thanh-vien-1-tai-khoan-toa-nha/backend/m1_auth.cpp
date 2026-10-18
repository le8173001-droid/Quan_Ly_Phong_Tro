// THÀNH VIÊN 1 – Xác thực: đăng ký / đăng nhập / Google / đổi mật khẩu / hồ sơ cá nhân / hủy quyền quản lý.
// Đăng ký, đăng nhập thường, đăng nhập bằng Google, thông tin tài khoản.
#include "app.hpp"

#include <cstdio>
#include <iostream>

#ifdef _WIN32
#define NHATRO_POPEN _popen
#define NHATRO_PCLOSE _pclose
#else
#define NHATRO_POPEN popen
#define NHATRO_PCLOSE pclose
#endif

namespace {

constexpr long long kTokenLifetimeSeconds = 7LL * 24 * 3600;

long long now_epoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}

std::string issue_token(App& app, long long user_id, const std::string& role) {
    long long now = now_epoch();
    return crypto::jwt_sign(json{{"sub", user_id}, {"role", role}, {"iat", now}, {"exp", now + kTokenLifetimeSeconds}},
                            app.cfg.jwt_secret);
}

json load_user_json(App& app, long long user_id) {
    json u = app.db.query_one(
        "SELECT u.user_id AS id, u.username, u.full_name, u.email, u.phone, u.avatar_url, u.id_number, u.birth_date, u.id_issue_date,"
        "       u.id_issue_place, u.address, u.bank_name, u.bank_account,"
        "       r.role_code AS role, r.role_name, (u.password_hash IS NOT NULL) AS has_password,"
        "       (SELECT COUNT(*) FROM buildings b WHERE b.manager_id = u.user_id) AS building_count "
        "FROM users u JOIN roles r ON r.role_id = u.role_id WHERE u.user_id = ?",
        {user_id});
    if (u.is_null()) not_found("Không tìm thấy tài khoản");
    {
        const json& hp = u["hasPassword"];
        u["hasPassword"] = hp.is_boolean() ? hp.get<bool>() : (hp.is_number() && hp.get<double>() != 0);
    }
    json providers = app.db.query("SELECT provider FROM user_identities WHERE user_id = ?", {user_id});
    u["providers"] = json::array();
    for (const auto& p : providers) u["providers"].push_back(p["provider"]);
    return u;
}

json auth_result(App& app, long long user_id, const std::string& role) {
    app.db.execute("UPDATE users SET last_login_at = datetime('now','localtime') WHERE user_id = ?", {user_id});
    return json{{"token", issue_token(app, user_id, role)}, {"user", load_user_json(app, user_id)}};
}

long long role_id_of(App& app, const std::string& code) {
    json r = app.db.query_one("SELECT role_id FROM roles WHERE role_code = ?", {code});
    if (r.is_null()) throw HttpError(500, "Thiếu vai trò " + code + " trong database");
    return r["roleId"].get<long long>();
}

}  // namespace

void check_password_strength(const std::string& pw) {
    if (pw.size() < 8) bad_request("Mật khẩu phải có ít nhất 8 ký tự");
    if (pw.size() > 100) bad_request("Mật khẩu quá dài (tối đa 100 ký tự)");
    bool letter = false, digit = false;
    for (unsigned char c : pw) {
        if (std::isalpha(c)) letter = true;
        if (std::isdigit(c)) digit = true;
    }
    if (!letter || !digit) bad_request("Mật khẩu phải gồm cả chữ và số");
}

namespace {

// ------------------------------------------------------------------ Google
std::string run_capture(const std::string& cmd) {
    FILE* pipe = NHATRO_POPEN(cmd.c_str(), "r");
    if (!pipe) throw HttpError(502, "Không chạy được lệnh curl để xác thực Google");
    std::string out;
    char buf[1024];
    size_t n;
    while ((n = fread(buf, 1, sizeof buf, pipe)) > 0 && out.size() < 65536) out.append(buf, n);
    NHATRO_PCLOSE(pipe);
    return out;
}

// Xác thực Google ID token bằng endpoint tokeninfo của Google (gọi qua curl, không cần thư viện HTTPS).
json verify_google_token(App& app, const std::string& token) {
    if (app.cfg.google_client_id.empty())
        throw HttpError(503, "Máy chủ chưa cấu hình GOOGLE_CLIENT_ID nên chưa dùng được đăng nhập Google");
    if (token.empty() || token.size() > 4096) bad_request("Mã đăng nhập Google không hợp lệ");
    for (unsigned char c : token) {
        // ID token là JWT: chỉ gồm A-Z a-z 0-9 - _ . (chặn ký tự lạ để không thể chèn lệnh vào shell)
        if (!(std::isalnum(c) || c == '-' || c == '_' || c == '.')) bad_request("Mã đăng nhập Google không hợp lệ");
    }
    std::string url = app.cfg.google_tokeninfo_url + "?id_token=" + token;
    std::string body = run_capture("curl -sS -m 10 \"" + url + "\"");
    json info = json::parse(body, nullptr, false);
    if (info.is_discarded() || !info.is_object() || !info.contains("sub"))
        throw HttpError(401, "Google không xác nhận mã đăng nhập này (hết hạn hoặc không hợp lệ)");

    auto as_string = [&](const char* k) -> std::string {
        if (!info.contains(k) || info[k].is_null()) return "";
        return info[k].is_string() ? info[k].get<std::string>() : info[k].dump();
    };
    if (as_string("aud") != app.cfg.google_client_id) throw HttpError(401, "Mã đăng nhập Google không dành cho ứng dụng này");
    std::string iss = as_string("iss");
    if (iss != "accounts.google.com" && iss != "https://accounts.google.com") throw HttpError(401, "Nguồn phát hành mã Google không hợp lệ");
    try {
        if (std::stoll(as_string("exp")) < now_epoch()) throw HttpError(401, "Mã đăng nhập Google đã hết hạn");
    } catch (const std::logic_error&) {
        throw HttpError(401, "Mã đăng nhập Google không hợp lệ");
    }
    std::string verified = as_string("email_verified");
    if (as_string("email").empty() || (verified != "true" && verified != "1"))
        throw HttpError(401, "Email Google chưa được xác minh");
    return info;
}

std::string make_username_from_email(App& app, const std::string& email) {
    std::string base;
    for (char c : email.substr(0, email.find('@'))) {
        char l = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        if (std::isalnum(static_cast<unsigned char>(l)) || l == '_' || l == '.') base.push_back(l);
    }
    if (base.size() > 24) base.resize(24);
    while (base.size() < 3) base.push_back('x');
    std::string name = base;
    while (app.db.scalar("SELECT COUNT(*) FROM users WHERE username = ?", {name}) > 0)
        name = base + crypto::random_hex(2);
    return name;
}

}  // namespace

void register_auth_routes(httplib::Server& svr, App& app) {
    // Cấu hình công khai cho giao diện (có bật Google hay không, tên website)
    add_route(svr, "GET", "/api/auth/config", [&app](const Req&, Res&) {
        json s = app.db.query_one("SELECT setting_value FROM site_settings WHERE setting_key = 'site_name'");
        return json{{"googleClientId", app.cfg.google_client_id},
                    {"googleEnabled", !app.cfg.google_client_id.empty()},
                    {"siteName", s.is_null() ? "NhàTrọ+" : s["settingValue"]}};
    });

    // Đăng ký tài khoản thường
    add_route(svr, "POST", "/api/auth/register", [&app](const Req& req, Res& res) {
        rate_limit(app, req, "register", 8, 600);
        json b = parse_body(req);
        std::string username = util::get_string(b, "username", true, 30);
        std::string password = util::get_string(b, "password", true, 100);
        std::string full_name = util::get_string(b, "fullName", true, 100);
        std::string email = util::get_string(b, "email", false, 100);
        std::string phone = util::get_string(b, "phone", false, 20);
        if (!util::valid_username(username)) bad_request("Tên đăng nhập gồm 3-30 ký tự: chữ, số, dấu gạch dưới hoặc dấu chấm");
        check_password_strength(password);
        if (!email.empty() && !util::valid_email(email)) bad_request("Email không hợp lệ");
        if (!phone.empty() && !util::valid_phone(phone)) bad_request("Số điện thoại không hợp lệ");

        if (app.db.scalar("SELECT COUNT(*) FROM users WHERE lower(username) = lower(?)", {username}) > 0)
            conflict("Tên đăng nhập đã được sử dụng");
        if (!email.empty() && app.db.scalar("SELECT COUNT(*) FROM users WHERE lower(email) = lower(?)", {email}) > 0)
            conflict("Email đã được sử dụng");

        std::string hash = crypto::hash_password(password);
        long long id = app.db.tx([&] {
            app.db.execute(
                "INSERT INTO users (role_id, username, password_hash, status, full_name, email, phone) VALUES (?,?,?,?,?,?,?)",
                {role_id_of(app, "TENANT"), username, hash, "active", full_name,
                 email.empty() ? json(nullptr) : json(email), phone.empty() ? json(nullptr) : json(phone)});
            return app.db.last_insert_id();
        });
        res.status = 201;
        return auth_result(app, id, "TENANT");
    });

    // Đăng nhập thường (tên đăng nhập hoặc email + mật khẩu)
    add_route(svr, "POST", "/api/auth/login", [&app](const Req& req, Res&) {
        rate_limit(app, req, "login", 15, 60);
        json b = parse_body(req);
        std::string login = util::get_string(b, "username", true, 100);
        std::string password = util::get_string(b, "password", true, 100);
        json u = app.db.query_one(
            "SELECT u.user_id, u.password_hash, u.status, r.role_code FROM users u JOIN roles r ON r.role_id = u.role_id "
            "WHERE lower(u.username) = lower(?) OR lower(u.email) = lower(?)",
            {login, login});
        // Luôn băm một lần dù tài khoản có tồn tại hay không, để thời gian phản hồi không lộ tài khoản nào có thật
        static const std::string dummy = crypto::hash_password("dummy-password-1");
        bool ok = false;
        if (!u.is_null() && !u["passwordHash"].is_null()) ok = crypto::verify_password(password, u["passwordHash"].get<std::string>());
        else crypto::verify_password(password, dummy);
        if (!ok) throw HttpError(401, "Sai tên đăng nhập hoặc mật khẩu");
        if (u["status"] != "active") forbidden("Tài khoản đã bị khóa, vui lòng liên hệ quản trị viên");
        return auth_result(app, u["userId"].get<long long>(), u["roleCode"].get<std::string>());
    });

    // Đăng nhập bằng Google: giao diện gửi "credential" (Google ID token)
    add_route(svr, "POST", "/api/auth/google", [&app](const Req& req, Res&) {
        rate_limit(app, req, "google", 20, 60);
        json b = parse_body(req);
        json info = verify_google_token(app, util::get_string(b, "credential", true, 4096));
        std::string sub = info["sub"].get<std::string>();
        std::string email = info["email"].get<std::string>();
        std::string name = info.value("name", "");
        std::string picture = info.value("picture", "");

        long long user_id = app.db.tx([&]() -> long long {
            json ident = app.db.query_one("SELECT user_id FROM user_identities WHERE provider = 'google' AND provider_user_id = ?", {sub});
            if (!ident.is_null()) return ident["userId"].get<long long>();

            // Chưa liên kết: gắn vào tài khoản có cùng email (đã xác minh), nếu không có thì tạo mới
            json existing = app.db.query_one("SELECT user_id FROM users WHERE lower(email) = lower(?)", {email});
            long long uid;
            if (!existing.is_null()) {
                uid = existing["userId"].get<long long>();
                if (!picture.empty())
                    app.db.execute("UPDATE users SET avatar_url = COALESCE(avatar_url, ?) WHERE user_id = ?", {picture, uid});
            } else {
                app.db.execute(
                    "INSERT INTO users (role_id, username, password_hash, status, full_name, email, avatar_url) VALUES (?,?,NULL,'active',?,?,?)",
                    {role_id_of(app, "TENANT"), make_username_from_email(app, email), name.empty() ? email : name, email,
                     picture.empty() ? json(nullptr) : json(picture)});
                uid = app.db.last_insert_id();
            }
            app.db.execute("INSERT INTO user_identities (user_id, provider, provider_user_id, email) VALUES (?, 'google', ?, ?)",
                           {uid, sub, email});
            return uid;
        });

        json row = app.db.query_one("SELECT u.status, r.role_code FROM users u JOIN roles r ON r.role_id = u.role_id WHERE u.user_id = ?", {user_id});
        if (row["status"] != "active") forbidden("Tài khoản đã bị khóa, vui lòng liên hệ quản trị viên");
        return auth_result(app, user_id, row["roleCode"].get<std::string>());
    });

    add_route(svr, "GET", "/api/auth/me", [&app](const Req& req, Res&) {
        return load_user_json(app, require_user(app, req).id);
    });

    // Cập nhật hồ sơ cá nhân
    add_route(svr, "PUT", "/api/auth/me", [&app](const Req& req, Res&) {
        User u = require_user(app, req);
        json b = parse_body(req);
        std::string full_name = util::get_string(b, "fullName", true, 100);
        std::string phone = util::get_string(b, "phone", false, 20);
        if (!phone.empty() && !util::valid_phone(phone)) bad_request("Số điện thoại không hợp lệ");
        app.db.execute("UPDATE users SET full_name = ?, phone = ? WHERE user_id = ?",
                       {full_name, phone.empty() ? json(nullptr) : json(phone), u.id});
        // Thông tin pháp lý (dùng khi lập hợp đồng với tư cách bên cho thuê): chỉ cập nhật trường nào được gửi lên
        struct F { const char* key; const char* col; size_t len; bool date; };
        for (const F& f : {F{"idNumber", "id_number", 20, false}, F{"birthDate", "birth_date", 10, true}, F{"idIssueDate", "id_issue_date", 10, true},
                           F{"idIssuePlace", "id_issue_place", 150, false}, F{"address", "address", 255, false}, F{"bankName", "bank_name", 100, false},
                           F{"bankAccount", "bank_account", 40, false}}) {
            if (!b.contains(f.key)) continue;
            std::string v = util::get_string(b, f.key, false, f.len);
            if (f.date && !v.empty() && !util::valid_date(v)) bad_request(std::string("Trường '") + f.key + "' phải có dạng YYYY-MM-DD");
            app.db.execute(std::string("UPDATE users SET ") + f.col + " = ? WHERE user_id = ?", {v.empty() ? json(nullptr) : json(v), u.id});
        }
        return load_user_json(app, u.id);
    });

    // Chủ tòa nhà tự hủy quyền quản lý (trở về tài khoản người thuê).
    //   - Còn hợp đồng đang hiệu lực trong tòa nhà -> không cho hủy (409).
    //   - Còn tòa nhà -> phải xác nhận confirm=true: tòa nhà được chuyển cho quản trị viên và các phòng được ẩn khỏi trang người dùng.
    add_route(svr, "POST", "/api/auth/resign-manager", [&app](const Req& req, Res&) {
        User u = require_user(app, req);
        if (u.role != "MANAGER") bad_request("Tài khoản của bạn không phải là chủ tòa nhà / quản lý");
        rate_limit(app, req, "resign", 5, 600);
        json b = parse_body(req);
        long long buildings = app.db.scalar("SELECT COUNT(*) FROM buildings WHERE manager_id = ?", {u.id});
        long long active = app.db.scalar(
            "SELECT COUNT(*) FROM contracts c JOIN rooms r ON r.room_id = c.room_id JOIN buildings b ON b.building_id = r.building_id "
            "WHERE b.manager_id = ? AND c.status = 'active'", {u.id});
        if (active > 0)
            conflict("Các tòa nhà của bạn còn " + std::to_string(active) + " hợp đồng đang hiệu lực. Hãy kết thúc / hủy các hợp đồng này trước khi hủy quyền quản lý.");
        auto confirm = util::get_bool(b, "confirm");
        if (buildings > 0 && !(confirm && *confirm))
            return json{{"needConfirm", true}, {"buildingCount", buildings},
                        {"message", "Bạn đang quản lý " + std::to_string(buildings) + " tòa nhà. Nếu hủy quyền, các tòa nhà sẽ chuyển cho quản trị viên và các phòng bị ẩn khỏi trang tìm phòng."}};
        // Nếu tài khoản có mật khẩu thì yêu cầu nhập lại để xác nhận
        json row = app.db.query_one("SELECT password_hash FROM users WHERE user_id = ?", {u.id});
        if (!row["passwordHash"].is_null()) {
            std::string pw = util::get_string(b, "password", true, 100);
            if (!crypto::verify_password(pw, row["passwordHash"].get<std::string>())) throw HttpError(400, "Mật khẩu không đúng");
        }
        app.db.tx([&] {
            if (buildings > 0) {
                long long admin_id = app.db.scalar("SELECT u.user_id FROM users u JOIN roles r ON r.role_id = u.role_id WHERE r.role_code = 'ADMIN' AND u.status = 'active' ORDER BY u.user_id LIMIT 1");
                if (admin_id == 0) throw HttpError(500, "Không có tài khoản quản trị viên để nhận lại các tòa nhà");
                app.db.execute("UPDATE rooms SET status = 'hidden', update_date = datetime('now','localtime') WHERE building_id IN (SELECT building_id FROM buildings WHERE manager_id = ?)", {u.id});
                app.db.execute("UPDATE buildings SET manager_id = ?, updated_at = datetime('now','localtime') WHERE manager_id = ?", {admin_id, u.id});
                app.db.execute("UPDATE buildings SET total_rooms = 0 WHERE manager_id = ?", {admin_id});
                app.db.execute("UPDATE buildings SET total_rooms = (SELECT COUNT(*) FROM rooms r WHERE r.building_id = buildings.building_id AND r.status <> 'hidden')");
            }
            app.db.execute("UPDATE users SET role_id = ? WHERE user_id = ?", {role_id_of(app, "TENANT"), u.id});
        });
        return auth_result(app, u.id, "TENANT");
    });

    // Đổi mật khẩu; tài khoản tạo bằng Google (chưa có mật khẩu) dùng chức năng này để đặt mật khẩu lần đầu
    add_route(svr, "POST", "/api/auth/password", [&app](const Req& req, Res&) {
        User u = require_user(app, req);
        rate_limit(app, req, "password", 10, 600);
        json b = parse_body(req);
        std::string new_pw = util::get_string(b, "newPassword", true, 100);
        check_password_strength(new_pw);
        json row = app.db.query_one("SELECT password_hash FROM users WHERE user_id = ?", {u.id});
        if (!row["passwordHash"].is_null()) {
            std::string current = util::get_string(b, "currentPassword", true, 100);
            if (!crypto::verify_password(current, row["passwordHash"].get<std::string>()))
                throw HttpError(400, "Mật khẩu hiện tại không đúng");
        }
        app.db.execute("UPDATE users SET password_hash = ? WHERE user_id = ?", {crypto::hash_password(new_pw), u.id});
        return json{{"message", "Đã cập nhật mật khẩu"}};
    });
}
