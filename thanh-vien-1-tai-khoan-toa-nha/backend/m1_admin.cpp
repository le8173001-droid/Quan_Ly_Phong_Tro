// THÀNH VIÊN 1 – Xác thực, Phân quyền, Tài khoản & Tòa nhà (ROLES, USERS, BUILDINGS).
// Bổ sung: chủ tòa nhà (owners), khuyến mãi, cấu hình website, địa điểm/tiện nghi (danh mục dùng chung của quản trị).
// Phần đăng nhập / đăng ký / Google / hủy quyền quản lý nằm ở thanh-vien-*/backend/m1_auth.cpp.
#include "admin_common.hpp"
#include "m2_rooms_api.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <regex>
#include <set>
#include <sstream>

namespace adm {

std::vector<Resource> resources_m1() {
    std::vector<Resource> rs;

    // ---- users (chỉ ADMIN)
    {
        Resource r;
        r.path = "users"; r.table = "users"; r.pk = "user_id"; r.admin_only = true;
        r.cols = {{"role_id", T::Int, true}, {"username", T::Str, true, {}, 30}, {"status", T::Str, false, {"active", "locked"}},
                  {"full_name", T::Str, true, {}, 100}, {"email", T::Str, false, {}, 100}, {"phone", T::Str, false, {}, 20},
                  {"avatar_url", T::Str, false, {}, 500}, {"id_number", T::Str, false, {}, 20}, {"birth_date", T::Date},
                  {"id_issue_date", T::Date}, {"id_issue_place", T::Str, false, {}, 150}, {"address", T::Str, false, {}, 255},
                  {"bank_name", T::Str, false, {}, 100}, {"bank_account", T::Str, false, {}, 40}};
        r.select = "SELECT t.user_id, t.role_id, t.username, t.status, t.full_name, t.email, t.phone, t.avatar_url, t.id_number, t.birth_date,"
                   " t.id_issue_date, t.id_issue_place, t.address, t.bank_name, t.bank_account, t.last_login_at, t.created_at,"
                   " (t.password_hash IS NOT NULL) AS has_password, ro.role_code, ro.role_name,"
                   " (SELECT COUNT(*) FROM buildings b WHERE b.manager_id = t.user_id) AS building_count"
                   " FROM users t JOIN roles ro ON ro.role_id = t.role_id";
        r.search = {"t.username", "t.full_name", "t.email", "t.phone"};
        r.filters = {"role_id", "status"};
        r.order = "t.user_id";
        r.before = [](HookCtx& c) {
            if (c.values.contains("username") && !util::valid_username(c.values["username"].get<std::string>()))
                bad_request("Tên đăng nhập gồm 3-30 ký tự: chữ, số, dấu gạch dưới hoặc dấu chấm");
            if (c.values.contains("email") && !c.values["email"].is_null() && !util::valid_email(c.values["email"].get<std::string>()))
                bad_request("Email không hợp lệ");
            if (c.values.contains("phone") && !c.values["phone"].is_null() && !util::valid_phone(c.values["phone"].get<std::string>()))
                bad_request("Số điện thoại không hợp lệ");
            std::string pw = util::get_string(c.body, "password", c.creating, 100);
            if (!pw.empty()) {
                check_password_strength(pw);
                c.values["password_hash"] = crypto::hash_password(pw);
            }
            if (c.creating && !c.values.contains("status")) c.values["status"] = "active";
            if (!c.creating && c.id == c.actor.id) {
                bool locking = c.values.contains("status") && c.values["status"] != "active";
                bool role_change = c.values.contains("role_id") && c.values["role_id"] != c.old["roleId"];
                if (locking || role_change) bad_request("Bạn không thể khóa hoặc đổi vai trò của chính mình");
            }
            // Hạ quyền chủ tòa nhà xuống người thuê khi họ còn tòa nhà -> phải chuyển tòa nhà trước
            if (!c.creating && c.values.contains("role_id") && c.values["role_id"] != c.old["roleId"]) {
                json role = c.app.db.query_one("SELECT role_code FROM roles WHERE role_id = ?", {c.values["role_id"]});
                if (role.is_null()) bad_request("Vai trò không tồn tại");
                if (role["roleCode"] == "TENANT" && c.app.db.scalar("SELECT COUNT(*) FROM buildings WHERE manager_id = ?", {c.id}) > 0)
                    bad_request("Người này còn đang là chủ của tòa nhà. Hãy chuyển tòa nhà sang người khác (mục Tòa nhà) trước khi hủy quyền quản lý.");
            }
        };
        r.before_delete = [](App&, const json& old_row, const User& actor) {
            if (old_row["userId"].get<long long>() == actor.id) bad_request("Bạn không thể xóa chính tài khoản của mình");
        };
        rs.push_back(r);
    }


    // ---- owners: danh sách chủ tòa nhà (chỉ ADMIN xem, thống kê theo từng chủ)
    {
        Resource r;
        r.path = "owners"; r.table = "users"; r.pk = "user_id"; r.admin_only = true; r.read_only = true;
        r.base_cond = "ro.role_code = 'MANAGER'";
        r.select = "SELECT t.user_id, t.username, t.full_name, t.email, t.phone, t.status, t.created_at,"
                   " (SELECT COUNT(*) FROM buildings b WHERE b.manager_id = t.user_id) AS building_count,"
                   " (SELECT COUNT(*) FROM rooms rm JOIN buildings b ON b.building_id = rm.building_id WHERE b.manager_id = t.user_id AND rm.status <> 'hidden') AS room_count,"
                   " (SELECT COUNT(*) FROM tenants tt JOIN rooms rm ON rm.room_id = tt.room_id JOIN buildings b ON b.building_id = rm.building_id WHERE b.manager_id = t.user_id) AS tenant_count,"
                   " (SELECT COUNT(*) FROM contracts c JOIN rooms rm ON rm.room_id = c.room_id JOIN buildings b ON b.building_id = rm.building_id WHERE b.manager_id = t.user_id AND c.status = 'active') AS active_contracts"
                   " FROM users t JOIN roles ro ON ro.role_id = t.role_id";
        r.search = {"t.username", "t.full_name", "t.email", "t.phone"};
        r.filters = {"status"};
        r.order = "t.full_name";
        rs.push_back(r);
    }


    // ---- roles (chỉ xem)
    {
        Resource r;
        r.path = "roles"; r.table = "roles"; r.pk = "role_id"; r.read_only = true; r.admin_only = true;
        r.select = "SELECT t.* FROM roles t";
        r.order = "t.role_id";
        rs.push_back(r);
    }


    // ---- buildings
    {
        Resource r;
        r.path = "buildings"; r.table = "buildings"; r.pk = "building_id";
        r.scope = "t.building_id";
        r.cols = {{"manager_id", T::Int, true}, {"name", T::Str, true, {}, 150}, {"address", T::Str, true, {}, 255},
                  {"province_id", T::Int, true}, {"district_id", T::Int}, {"ward_id", T::Int}, {"latitude", T::Real},
                  {"longitude", T::Real}, {"contact_name", T::Str, false, {}, 100}, {"contact_phone", T::Str, false, {}, 20},
                  {"description", T::Str, false, {}, 500}, {"cover_url", T::Str, false, {}, 500}};
        r.select = "SELECT t.*, u.full_name AS manager_name, p.name AS province_name, p.adm_system, d.name AS district_name, w.name AS ward_name,"
                   " (SELECT COUNT(*) FROM rooms rm WHERE rm.building_id = t.building_id AND rm.status <> 'hidden') AS room_count,"
                   " (SELECT COUNT(*) FROM tenants tt JOIN rooms rm ON rm.room_id = tt.room_id WHERE rm.building_id = t.building_id) AS tenant_count"
                   " FROM buildings t JOIN users u ON u.user_id = t.manager_id JOIN provinces p ON p.province_id = t.province_id"
                   " LEFT JOIN districts d ON d.district_id = t.district_id LEFT JOIN wards w ON w.ward_id = t.ward_id";
        r.search = {"t.name", "t.address", "d.name", "p.name", "w.name", "u.full_name"};
        r.filters = {"manager_id", "province_id", "district_id"};
        r.order = "t.building_id";
        r.before = [](HookCtx& c) {
            auto val = [&](const char* k) -> json {
                if (c.values.contains(k)) return c.values[k];
                return c.id ? c.old[util::snake_to_camel(k)] : json(nullptr);
            };
            json lat = val("latitude"), lng = val("longitude");
            if (lat.is_null() != lng.is_null()) bad_request("Cần nhập cả vĩ độ (latitude) và kinh độ (longitude)");
            if (!lat.is_null() && (lat.get<double>() < -90 || lat.get<double>() > 90 || lng.get<double>() < -180 || lng.get<double>() > 180))
                bad_request("Toạ độ không hợp lệ");
            if (c.values.contains("contact_phone") && !c.values["contact_phone"].is_null() &&
                !util::valid_phone(c.values["contact_phone"].get<std::string>()))
                bad_request("Số điện thoại liên hệ không hợp lệ");
            if (c.values.contains("cover_url") && !c.values["cover_url"].is_null()) check_image_url(c.values["cover_url"]);
            if (!c.actor.is_admin()) {
                if (c.values.contains("manager_id") && c.values["manager_id"] != c.actor.id)
                    forbidden(c.creating ? "Bạn chỉ được tạo tòa nhà do chính bạn làm chủ" : "Chỉ ADMIN mới được chuyển quyền chủ tòa nhà cho người khác");
                if (c.creating) c.values["manager_id"] = c.actor.id;
            }
            json mgr = c.app.db.query_one("SELECT ro.role_code FROM users u JOIN roles ro ON ro.role_id = u.role_id WHERE u.user_id = ?", {val("manager_id")});
            if (mgr.is_null()) bad_request("Người quản lý không tồn tại");
            if (mgr["roleCode"] == "TENANT") bad_request("Người quản lý phải có vai trò ADMIN hoặc MANAGER");

            // Địa chỉ theo địa giới: hệ CŨ (tỉnh > quận/huyện > phường/xã) hoặc hệ MỚI (tỉnh > phường/xã)
            long long pid = val("province_id");
            json prov = c.app.db.query_one("SELECT adm_system FROM provinces WHERE province_id = ?", {pid});
            if (prov.is_null()) bad_request("Tỉnh/thành không tồn tại");
            json did = val("district_id"), wid = val("ward_id");
            if (prov["admSystem"] == "OLD") {
                if (did.is_null()) bad_request("Địa giới cũ: vui lòng chọn quận/huyện");
                if (c.app.db.scalar("SELECT COUNT(*) FROM districts WHERE district_id = ? AND province_id = ?", {did, pid}) == 0)
                    bad_request("Quận/huyện không thuộc tỉnh/thành đã chọn");
                if (!wid.is_null() && c.app.db.scalar("SELECT COUNT(*) FROM wards WHERE ward_id = ? AND district_id = ?", {wid, did}) == 0)
                    bad_request("Phường/xã không thuộc quận/huyện đã chọn");
            } else {
                if (!did.is_null()) bad_request("Địa giới mới (34 tỉnh/thành) không có cấp quận/huyện");
                c.values["district_id"] = nullptr;
                if (!wid.is_null() && c.app.db.scalar("SELECT COUNT(*) FROM wards WHERE ward_id = ? AND province_id = ?", {wid, pid}) == 0)
                    bad_request("Phường/xã không thuộc tỉnh/thành đã chọn");
            }
        };
        // Ảnh tòa nhà: imageUrls = danh sách đường dẫn ảnh đã tải lên (/api/uploads); ảnh đầu tiên là ảnh bìa
        r.after_body = [](App& app, long long id, const json& body, const User&) {
            if (body.contains("imageUrls")) {
                if (!body["imageUrls"].is_array()) bad_request("imageUrls phải là mảng");
                if (body["imageUrls"].size() > 30) bad_request("Tối đa 30 ảnh");
                app.db.execute("DELETE FROM building_images WHERE building_id = ?", {id});
                int order = 0;
                for (const auto& u : body["imageUrls"]) {
                    if (!u.is_string()) bad_request("imageUrls chỉ gồm chuỗi");
                    std::string url = util::trim(u.get<std::string>());
                    if (url.empty()) continue;
                    check_image_url(url);
                    app.db.execute("INSERT INTO building_images (building_id, url, sort_order) VALUES (?,?,?)", {id, url, order++});
                }
                app.db.execute("UPDATE buildings SET cover_url = (SELECT url FROM building_images WHERE building_id = ? ORDER BY sort_order, image_id LIMIT 1) WHERE building_id = ?", {id, id});
            }
        };
        r.after = [](App& app, long long id) { sync_building_total(app, id); };
        r.decorate = [](App& app, json& row) {
            row["images"] = app.db.query("SELECT image_id, url FROM building_images WHERE building_id = ? ORDER BY sort_order, image_id", {row["buildingId"]});
        };
        rs.push_back(r);
    }


    // ---- promotions (chỉ ADMIN): chương trình, ưu đãi, % giảm giá
    {
        Resource r;
        r.path = "promotions"; r.table = "promotions"; r.pk = "promotion_id"; r.admin_only = true;
        r.cols = {{"title", T::Str, true, {}, 200}, {"description", T::Str, false, {}, 500},
                  {"promo_type", T::Str, false, {"program", "hotdeal", "voucher", "partner"}}, {"discount_text", T::Str, false, {}, 50},
                  {"discount_percent", T::Real}, {"voucher_code", T::Str, false, {}, 30}, {"link_url", T::Str, false, {}, 300},
                  {"banner_color", T::Str, false, {}, 20}, {"start_at", T::DateTime, true}, {"end_at", T::DateTime, true},
                  {"is_active", T::Bool}, {"sort_order", T::Int}};
        r.select = "SELECT t.*, (SELECT COUNT(*) FROM promotion_rooms pr WHERE pr.promotion_id = t.promotion_id) AS room_count,"
                   " (t.is_active = 1 AND t.start_at <= datetime('now','localtime') AND t.end_at > datetime('now','localtime')) AS is_running FROM promotions t";
        r.search = {"t.title", "t.voucher_code"};
        r.filters = {"promo_type", "is_active"};
        r.order = "t.promo_type, t.sort_order, t.promotion_id DESC";
        r.before = [](HookCtx& c) {
            auto val = [&](const char* k) -> json {
                if (c.values.contains(k)) return c.values[k];
                return c.id ? c.old[util::snake_to_camel(k)] : json(nullptr);
            };
            std::string s = val("start_at"), e = val("end_at");
            if (e <= s) bad_request("Thời gian kết thúc phải sau thời gian bắt đầu");
            json link = val("link_url");
            if (!link.is_null()) {
                std::string l = link;
                bool ok = (l.rfind('/', 0) == 0 && l.rfind("//", 0) != 0) || l.rfind("https://", 0) == 0 || l.rfind("http://", 0) == 0;
                if (!ok) bad_request("Liên kết phải bắt đầu bằng '/' hoặc http(s)://");
            }
            if (c.creating && !c.values.contains("promo_type")) c.values["promo_type"] = "program";
            if (c.values.contains("discount_percent") && !c.values["discount_percent"].is_null()) {
                double p = c.values["discount_percent"];
                if (p <= 0 || p > 90) bad_request("Phần trăm giảm giá phải lớn hơn 0 và tối đa 90");
                // Chưa nhập chữ nổi bật thì tự điền "-N%"
                if (val("discount_text").is_null() || (c.values.contains("discount_text") && c.values["discount_text"].is_null())) {
                    std::string t = std::to_string(p);
                    t.erase(t.find_last_not_of('0') + 1);
                    if (!t.empty() && t.back() == '.') t.pop_back();
                    c.values["discount_text"] = "-" + t + "%";
                }
            }
        };
        // Có % giảm giá: tính lại giá ưu đãi của các phòng đang tham gia (làm tròn nghìn đồng)
        r.after = [](App& app, long long id) {
            app.db.execute(
                "UPDATE promotion_rooms SET deal_price = (SELECT CAST(ROUND(rm.base_rent_price * (100 - p.discount_percent) / 100.0 / 1000.0) * 1000 AS INTEGER)"
                " FROM rooms rm, promotions p WHERE rm.room_id = promotion_rooms.room_id AND p.promotion_id = promotion_rooms.promotion_id)"
                " WHERE promotion_id = ? AND promotion_id IN (SELECT promotion_id FROM promotions WHERE discount_percent IS NOT NULL)", {id});
        };
        r.decorate = [](App& app, json& row) {
            row["rooms"] = app.db.query(
                "SELECT pr.room_id, rm.room_number, rm.title, b.name AS building_name, rm.base_rent_price, pr.deal_price FROM promotion_rooms pr "
                "JOIN rooms rm ON rm.room_id = pr.room_id JOIN buildings b ON b.building_id = rm.building_id WHERE pr.promotion_id = ? ORDER BY b.name, rm.room_number",
                {row["promotionId"]});
        };
        rs.push_back(r);
    }


    // ---- địa giới, địa điểm, tiện nghi
    {
        Resource r;
        r.path = "provinces"; r.table = "provinces"; r.pk = "province_id"; r.read_only = true;
        r.select = "SELECT t.* FROM provinces t";
        r.search = {"t.name", "t.full_name"};
        r.filters = {"adm_system"};
        r.order = "t.adm_system DESC, t.name";
        rs.push_back(r);
    }
    {
        Resource r;
        r.path = "districts"; r.table = "districts"; r.pk = "district_id"; r.read_only = true;
        r.select = "SELECT t.*, p.name AS province_name FROM districts t JOIN provinces p ON p.province_id = t.province_id";
        r.search = {"t.name", "p.name"};
        r.filters = {"province_id"};
        r.order = "t.province_id, t.name";
        rs.push_back(r);
    }
    {
        Resource r;
        r.path = "wards"; r.table = "wards"; r.pk = "ward_id"; r.read_only = true;
        r.select = "SELECT t.*, d.name AS district_name, p.name AS province_name FROM wards t JOIN provinces p ON p.province_id = t.province_id LEFT JOIN districts d ON d.district_id = t.district_id";
        r.search = {"t.name", "d.name", "p.name"};
        r.filters = {"district_id", "province_id"};
        r.order = "t.province_id, t.district_id, t.name";
        rs.push_back(r);
    }
    {
        Resource r;
        r.path = "landmarks"; r.table = "landmarks"; r.pk = "landmark_id"; r.manager_ro = true;
        r.cols = {{"name", T::Str, true, {}, 150}, {"landmark_type", T::Str, true, {"school", "work"}}, {"district_id", T::Int},
                  {"address", T::Str, false, {}, 255}, {"latitude", T::Real, true}, {"longitude", T::Real, true}};
        r.select = "SELECT t.*, d.name AS district_name FROM landmarks t LEFT JOIN districts d ON d.district_id = t.district_id";
        r.search = {"t.name", "d.name"};
        r.filters = {"landmark_type", "district_id"};
        r.order = "t.landmark_type DESC, t.name";
        r.before = [](HookCtx& c) {
            if (c.values.contains("latitude") && (c.values["latitude"].get<double>() < -90 || c.values["latitude"].get<double>() > 90))
                bad_request("Vĩ độ phải trong khoảng -90..90");
            if (c.values.contains("longitude") && (c.values["longitude"].get<double>() < -180 || c.values["longitude"].get<double>() > 180))
                bad_request("Kinh độ phải trong khoảng -180..180");
        };
        rs.push_back(r);
    }
    {
        Resource r;
        r.path = "amenities"; r.table = "amenities"; r.pk = "amenity_id"; r.manager_ro = true;
        r.cols = {{"name", T::Str, true, {}, 100}, {"icon", T::Str, false, {}, 30}};
        r.select = "SELECT t.* FROM amenities t";
        r.search = {"t.name"};
        r.order = "t.amenity_id";
        rs.push_back(r);
    }
    return rs;
}

void register_m1_routes(httplib::Server& svr, App& app) {
    // ---- Phòng tham gia chương trình khuyến mãi (ADMIN): thay toàn bộ danh sách.
    //      Chương trình có % giảm giá thì giá ưu đãi tự tính theo %, không cần nhập dealPrice.
    add_route(svr, "PUT", "/api/admin/promotions/:id/rooms", [&app](const Req& req, Res&) {
        require_admin(app, req);
        long long id = path_id(req);
        json b = parse_body(req);
        if (!b.contains("rooms") || !b["rooms"].is_array()) bad_request("Cần gửi mảng 'rooms': [{roomId, dealPrice?}]");
        if (b["rooms"].size() > 300) bad_request("Tối đa 300 phòng cho một chương trình");
        json promo = app.db.query_one("SELECT discount_percent FROM promotions WHERE promotion_id = ?", {id});
        if (promo.is_null()) not_found();
        app.db.tx([&] {
            app.db.execute("DELETE FROM promotion_rooms WHERE promotion_id = ?", {id});
            for (const auto& item : b["rooms"]) {
                auto rid = util::get_int(item, "roomId");
                if (!rid) bad_request("Mỗi phòng cần có roomId");
                json room = app.db.query_one("SELECT base_rent_price FROM rooms WHERE room_id = ?", {*rid});
                if (room.is_null()) bad_request("Phòng #" + std::to_string(*rid) + " không tồn tại");
                long long deal;
                if (!promo["discountPercent"].is_null()) {
                    double pct = promo["discountPercent"];
                    deal = static_cast<long long>(std::llround(room["baseRentPrice"].get<double>() * (100 - pct) / 100.0 / 1000.0)) * 1000;
                } else {
                    auto price = util::get_double(item, "dealPrice");
                    if (!price) bad_request("Chương trình chưa có % giảm giá nên mỗi phòng cần có dealPrice");
                    if (*price < 0) bad_request("Giá ưu đãi không được âm");
                    deal = static_cast<long long>(std::llround(*price));
                }
                app.db.execute("INSERT OR REPLACE INTO promotion_rooms (promotion_id, room_id, deal_price) VALUES (?,?,?)", {id, *rid, deal});
            }
        });
        return json{{"message", "Đã cập nhật danh sách phòng"}};
    });


    // ---- Cấu hình website (hotline, zalo, ...)
    add_route(svr, "GET", "/api/admin/settings", [&app](const Req& req, Res&) {
        require_admin(app, req);
        json out = json::object();
        for (const auto& r : app.db.query("SELECT setting_key, setting_value FROM site_settings ORDER BY setting_key"))
            out[r["settingKey"].get<std::string>()] = r["settingValue"];
        return out;
    });
    add_route(svr, "PUT", "/api/admin/settings", [&app](const Req& req, Res&) {
        require_admin(app, req);
        json b = parse_body(req);
        static const std::regex key_re("^[a-z_]{1,50}$");
        static const std::regex phone_re("^[0-9 .+()-]{5,20}$");
        for (auto it = b.begin(); it != b.end(); ++it) {
            if (!std::regex_match(it.key(), key_re)) bad_request("Tên cấu hình không hợp lệ: " + it.key());
            if (!it.value().is_string() || it.value().get<std::string>().size() > 500) bad_request("Giá trị của '" + it.key() + "' phải là chuỗi (tối đa 500 ký tự)");
            std::string v = util::trim(it.value().get<std::string>());
            const std::string& k = it.key();
            if ((k == "hotline" || k == "zalo") && !std::regex_match(v, phone_re))
                bad_request(std::string(k == "hotline" ? "Hotline" : "Số Zalo") + " chỉ gồm chữ số, dấu cách, dấu chấm, dấu + - ( ) (5-20 ký tự)");
            if (k == "email" && !v.empty() && !util::valid_email(v)) bad_request("Email không hợp lệ");
            if (k == "facebook" && !v.empty() && v.rfind("https://", 0) != 0 && v.rfind("http://", 0) != 0) bad_request("Liên kết Facebook phải bắt đầu bằng https://");
            if (k == "featured_mode" && v != "manual" && v != "auto" && v != "mixed") bad_request("featured_mode phải là manual, auto hoặc mixed");
            if (k == "featured_count") {
                auto n = util::parse_ll(v, "featured_count");
                if (!n || *n < 1 || *n > 12) bad_request("Số phòng nổi bật mỗi mục phải từ 1 đến 12");
            }
            if (k == "site_name" && v.empty()) bad_request("Tên website không được để trống");
        }
        app.db.tx([&] {
            for (auto it = b.begin(); it != b.end(); ++it) {
                app.db.execute("INSERT INTO site_settings (setting_key, setting_value) VALUES (?, ?) "
                               "ON CONFLICT(setting_key) DO UPDATE SET setting_value = excluded.setting_value",
                               {it.key(), util::trim(it.value().get<std::string>())});
            }
        });
        return json{{"message", "Đã lưu cấu hình"}};
    });

}

}  // namespace adm
