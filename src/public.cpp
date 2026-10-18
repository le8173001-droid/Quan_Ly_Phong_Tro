// API công khai cho giao diện người dùng: danh mục, gợi ý tìm kiếm, trang chủ, khuyến mãi, hỗ trợ, tra cứu hóa đơn, yêu thích.
#include "m2_rooms_api.hpp"

#include <algorithm>
#include <set>
#include <sstream>

namespace {

// Điều kiện "phòng còn chỗ" cho truy vấn con có bảng rooms r
std::string avail() { return kPublicRoomCondition; }

std::string normalize_phone(const std::string& s) {
    std::string d = util::digits_only(s);
    if (d.rfind("84", 0) == 0 && d.size() >= 11) d = "0" + d.substr(2);
    return d;
}

json one_setting_map(App& app) {
    json out = json::object();
    for (const auto& r : app.db.query("SELECT setting_key, setting_value FROM site_settings"))
        out[r["settingKey"].get<std::string>()] = r["settingValue"];
    return out;
}

const char* kPromoCols =
    "promotion_id, title, description, promo_type, discount_text, discount_percent, voucher_code, link_url, banner_color, start_at, end_at,"
    " CAST(strftime('%s', end_at) AS INTEGER) - CAST(strftime('%s','now','localtime') AS INTEGER) AS seconds_left";

// Chương trình & ưu đãi đang diễn ra, nhóm theo loại
json active_promotions(App& app, long long user_id) {
    json rows = app.db.query(std::string("SELECT ") + kPromoCols +
                             " FROM promotions WHERE is_active = 1 AND start_at <= datetime('now','localtime') AND end_at > datetime('now','localtime')"
                             " ORDER BY promo_type, sort_order, end_at");
    json out = {{"programs", json::array()}, {"hotdeals", json::array()}, {"vouchers", json::array()}, {"partners", json::array()}};
    for (auto& p : rows) {
        std::string type = p["promoType"];
        if (type == "hotdeal") {
            RoomFetch f;
            f.where.add(avail());
            f.where.add("r.room_id IN (SELECT room_id FROM promotion_rooms WHERE promotion_id = ?)", p["promotionId"]);
            f.limit = 12;
            f.user_id = user_id;
            p["rooms"] = fetch_rooms(app, f);
            out["hotdeals"].push_back(p);
        } else if (type == "program") {
            out["programs"].push_back(p);
        } else if (type == "voucher") {
            out["vouchers"].push_back(p);
        } else {
            out["partners"].push_back(p);
        }
    }
    return out;
}

json compute_stats(App& app) {
    return json{
        {"buildings", app.db.scalar("SELECT COUNT(*) FROM buildings")},
        {"rooms", app.db.scalar("SELECT COUNT(*) FROM rooms WHERE status <> 'hidden'")},
        {"availableRooms", app.db.scalar("SELECT COUNT(*) FROM rooms r WHERE " + avail())},
        {"tenants", app.db.scalar("SELECT COUNT(*) FROM tenants")},
        {"members", app.db.scalar("SELECT COUNT(*) FROM users")},
        {"brokers", app.db.scalar("SELECT COUNT(*) FROM broker_applications WHERE status = 'approved'")},
        {"provinces", app.db.scalar("SELECT COUNT(DISTINCT province_id) FROM buildings")},
    };
}

// Danh sách phòng "nổi bật" của một loại hình: phòng admin ghim (is_featured) + phòng được xem nhiều nhất (tự động theo lượt xem)
//   featured_mode: manual = chỉ phòng ghim | auto = chỉ theo lượt xem | mixed = ghim trước, còn thiếu thì bù bằng phòng nhiều lượt xem
json featured_rooms(App& app, long long type_id, long long user_id, const std::string& mode, long long count) {
    json out = json::array();
    std::set<long long> used;
    if (mode != "auto") {
        RoomFetch f;
        f.where.add(avail());
        f.where.add("r.type_id = ? AND r.is_featured = 1", type_id);
        f.order_by = "r.featured_order, r.view_count DESC, r.room_id DESC";
        f.limit = count;
        f.user_id = user_id;
        for (auto& r : fetch_rooms(app, f)) {
            r["featuredSource"] = "pinned";
            used.insert(r["roomId"].get<long long>());
            out.push_back(std::move(r));
        }
    }
    if (mode != "manual" && static_cast<long long>(out.size()) < count) {
        RoomFetch f;
        f.where.add(avail());
        f.where.add("r.type_id = ?", type_id);
        f.order_by = "r.view_count DESC, r.room_id DESC";
        f.limit = count + static_cast<long long>(used.size());
        f.user_id = user_id;
        for (auto& r : fetch_rooms(app, f)) {
            if (static_cast<long long>(out.size()) >= count) break;
            if (used.count(r["roomId"].get<long long>())) continue;
            r["featuredSource"] = "views";
            out.push_back(std::move(r));
        }
    }
    return out;
}

json featured_sections(App& app, long long user_id) {
    json settings = one_setting_map(app);
    auto get = [&](const char* k, const std::string& def) { return settings.contains(k) ? settings[k].get<std::string>() : def; };
    std::string mode = get("featured_mode", "mixed");
    long long count = util::parse_ll(get("featured_count", "4"), "featured_count").value_or(4);
    if (count < 1 || count > 12) count = 4;
    std::stringstream ss(get("featured_types", "ky-tuc-xa,nha,chung-cu,phong-tro"));
    std::string slug;
    json sections = json::array();
    while (std::getline(ss, slug, ',')) {
        slug = util::trim(slug);
        json t = app.db.query_one("SELECT type_id, type_name, slug, icon FROM room_types WHERE slug = ?", {slug});
        if (t.is_null()) continue;
        json rooms = featured_rooms(app, t["typeId"], user_id, mode, count);
        if (rooms.empty()) continue;
        sections.push_back({{"type", t}, {"title", t["typeName"].get<std::string>() + " nổi bật"}, {"rooms", rooms}});
    }
    return sections;
}

// Gợi ý khu vực theo từ khóa: tỉnh/thành, quận/huyện, phường/xã của CẢ HAI hệ địa giới
json area_suggestions(App& app, const std::string& pat) {
    json out = json::array();
    auto add = [&](json rows, const char* level) {
        for (auto& r : rows) {
            r["level"] = level;
            out.push_back(std::move(r));
        }
    };
    const std::string counts_b = " (SELECT COUNT(*) FROM rooms r JOIN buildings b ON b.building_id = r.building_id WHERE ";
    add(app.db.query("SELECT p.province_id, p.name AS label, p.adm_system AS system," + counts_b + "b.province_id = p.province_id AND " + avail() +
                     ") AS room_count FROM provinces p WHERE vn_fold(p.name) LIKE ? ESCAPE '!' ORDER BY room_count DESC, p.adm_system DESC, vn_fold(p.name) LIMIT 5", {pat}),
        "province");
    add(app.db.query("SELECT d.district_id, d.province_id, d.name || ', ' || p.name AS label, p.adm_system AS system," + counts_b + "b.district_id = d.district_id AND " + avail() +
                     ") AS room_count FROM districts d JOIN provinces p ON p.province_id = d.province_id WHERE vn_fold(d.name) LIKE ? ESCAPE '!' "
                     "ORDER BY room_count DESC, vn_fold(d.name) LIMIT 6", {pat}),
        "district");
    add(app.db.query("SELECT w.ward_id, w.district_id, w.province_id, w.name || COALESCE(', ' || d.name, '') || ', ' || p.name AS label, p.adm_system AS system,"
                     + counts_b + "b.ward_id = w.ward_id AND " + avail() + ") AS room_count FROM wards w JOIN provinces p ON p.province_id = w.province_id "
                     "LEFT JOIN districts d ON d.district_id = w.district_id WHERE vn_fold(w.name) LIKE ? ESCAPE '!' "
                     "ORDER BY room_count DESC, p.adm_system DESC, length(w.name), vn_fold(w.name) LIMIT 8", {pat}),
        "ward");
    return out;
}

void require_phone(const std::string& phone) {
    if (!util::valid_phone(phone)) bad_request("Số điện thoại không hợp lệ (ví dụ 0901234567)");
}

}  // namespace

json featured_sections_public(App& app, long long user_id) { return featured_sections(app, user_id); }

void register_public_routes(httplib::Server& svr, App& app) {
    // ---- Danh mục dùng cho ô tìm kiếm
    add_route(svr, "GET", "/api/meta", [&app](const Req&, Res&) {
        json out;
        out["roomTypes"] = app.db.query(
            "SELECT t.type_id, t.type_name, t.slug, t.icon, t.description,"
            " (SELECT COUNT(*) FROM rooms r WHERE r.type_id = t.type_id AND " + avail() + ") AS room_count"
            " FROM room_types t ORDER BY t.sort_order, t.type_id");
        out["amenities"] = app.db.query("SELECT amenity_id, name, icon FROM amenities ORDER BY amenity_id");
        out["settings"] = one_setting_map(app);
        return out;
    });

    add_route(svr, "GET", "/api/settings", [&app](const Req&, Res&) { return one_setting_map(app); });
    add_route(svr, "GET", "/api/stats", [&app](const Req&, Res&) { return compute_stats(app); });

    // Địa điểm học tập / làm việc
    add_route(svr, "GET", "/api/landmarks", [&app](const Req& req, Res&) {
        Where w;
        if (auto pid = util::parse_ll(param(req, "provinceId"), "provinceId"))
            w.add("d.province_id IN (SELECT province_id FROM provinces WHERE name = (SELECT name FROM provinces WHERE province_id = ?))", *pid);
        std::string type = param(req, "type");
        if (!type.empty()) {
            if (type != "school" && type != "work") bad_request("type phải là school hoặc work");
            w.add("l.landmark_type = ?", type);
        }
        std::string q = param(req, "q");
        if (!q.empty()) w.add("vn_fold(l.name) LIKE ? ESCAPE '!'", "%" + util::like_escape(util::vn_fold(q)) + "%");
        // Có toạ độ thiết bị: chỉ lấy địa điểm trong bán kính, gần nhất trước (kèm distanceKm)
        auto lat = util::parse_double(param(req, "lat"), "lat");
        auto lng = util::parse_double(param(req, "lng"), "lng");
        if (lat.has_value() != lng.has_value()) bad_request("Cần truyền cả lat và lng");
        std::string dist_col, order = "l.landmark_type DESC, l.name";
        std::vector<json> pre;
        if (lat && lng) {
            if (*lat < -90 || *lat > 90 || *lng < -180 || *lng > 180) bad_request("Toạ độ không hợp lệ");
            double radius = util::parse_double(param(req, "radiusKm"), "radiusKm").value_or(10.0);
            if (radius <= 0 || radius > 500) bad_request("radiusKm phải trong khoảng (0, 500]");
            dist_col = ", haversine(?, ?, l.latitude, l.longitude) AS distance_km";
            pre = {*lat, *lng};
            w.add("haversine(?, ?, l.latitude, l.longitude) <= ?", *lat);
            w.params.push_back(*lng);
            w.params.push_back(radius);
            order = "distance_km, l.name";
        }
        std::vector<json> ps = pre;
        ps.insert(ps.end(), w.params.begin(), w.params.end());
        ps.push_back(60);
        return app.db.query(
            "SELECT l.landmark_id, l.name, l.landmark_type, l.address, l.latitude, l.longitude, l.district_id, d.name AS district_name, d.province_id" + dist_col +
            " FROM landmarks l LEFT JOIN districts d ON d.district_id = l.district_id" + w.sql() + " ORDER BY " + order + " LIMIT ?",
            ps);
    });

    // ---- Gợi ý khi gõ vào ô tìm kiếm
    add_route(svr, "GET", "/api/search/suggest", [&app](const Req& req, Res&) {
        std::string q = param(req, "q");
        if (q.size() > 100) bad_request("Từ khóa quá dài");
        std::string pat = "%" + util::like_escape(util::vn_fold(q)) + "%";
        json out;
        out["roomTypes"] = app.db.query(
            "SELECT type_id, type_name, slug, icon FROM room_types WHERE vn_fold(type_name) LIKE ? ESCAPE '!' ORDER BY sort_order", {pat});
        out["areas"] = q.empty() ? json::array() : area_suggestions(app, pat);
        out["landmarks"] = app.db.query(
            "SELECT landmark_id, name, landmark_type, latitude, longitude FROM landmarks WHERE vn_fold(name) LIKE ? ESCAPE '!' ORDER BY name LIMIT 6", {pat});
        out["rooms"] = q.empty() ? json::array()
                                  : app.db.query("SELECT r.room_id, r.title, r.room_number FROM rooms r WHERE " + avail() +
                                                     " AND (vn_fold(r.title) LIKE ? ESCAPE '!' OR vn_fold(r.room_number) LIKE ? ESCAPE '!') ORDER BY r.create_date DESC LIMIT 5",
                                                 {pat, pat});
        return out;
    });

    // ---- Chương trình & ưu đãi
    add_route(svr, "GET", "/api/promotions", [&app](const Req& req, Res&) {
        auto u = current_user(app, req);
        return active_promotions(app, u ? u->id : 0);
    });

    // ---- Trang chủ: gom dữ liệu trong một lần gọi
    add_route(svr, "GET", "/api/home", [&app](const Req& req, Res&) {
        auto user = current_user(app, req);
        long long uid = user ? user->id : 0;

        long long province_id = util::parse_ll(param(req, "provinceId"), "provinceId").value_or(0);
        json out;
        out["stats"] = compute_stats(app);
        out["settings"] = one_setting_map(app);
        out["promotions"] = active_promotions(app, uid);
        out["featured"] = featured_sections(app, uid);

        auto section = [&](const std::string& extra, const std::string& order, long long limit) {
            RoomFetch f;
            f.where.add(avail());
            if (!extra.empty()) f.where.add(extra);
            if (province_id > 0)
                f.where.add("b.province_id IN (SELECT province_id FROM provinces WHERE name = (SELECT name FROM provinces WHERE province_id = ?))", province_id);
            f.order_by = order;
            f.limit = limit;
            f.user_id = uid;
            return fetch_rooms(app, f);
        };
        out["nowRooms"] = section("r.is_now = 1", "r.create_date DESC, r.room_id DESC", 8);
        out["latestRooms"] = section("", "r.create_date DESC, r.room_id DESC", 8);
        out["hotRooms"] = section("r.is_hot = 1", "r.view_count DESC, r.room_id DESC", 8);

        // Khu vực nhiều phòng nhất (phường/xã hoặc quận/huyện) để gợi ý nhanh
        out["popularAreas"] = app.db.query(
            "SELECT * FROM (SELECT COALESCE(w.name, d.name, p.name) || CASE WHEN w.name IS NOT NULL AND d.name IS NOT NULL THEN ', ' || d.name ELSE '' END"
            "  || CASE WHEN w.name IS NOT NULL OR d.name IS NOT NULL THEN ', ' || p.name ELSE '' END AS label,"
            " b.province_id, b.district_id, b.ward_id, COUNT(*) AS room_count"
            " FROM rooms r JOIN buildings b ON b.building_id = r.building_id JOIN provinces p ON p.province_id = b.province_id"
            " LEFT JOIN districts d ON d.district_id = b.district_id LEFT JOIN wards w ON w.ward_id = b.ward_id"
            " WHERE " + avail() + " GROUP BY b.province_id, b.district_id, b.ward_id) ORDER BY room_count DESC, label LIMIT 12");
        return out;
    });

    // ---- Hỗ trợ tìm phòng
    add_route(svr, "POST", "/api/support-requests", [&app](const Req& req, Res& res) {
        rate_limit(app, req, "support", 6, 600);
        json b = parse_body(req);
        std::string name = util::get_string(b, "fullName", true, 100);
        std::string phone = util::get_string(b, "phone", true, 20);
        std::string email = util::get_string(b, "email", false, 100);
        std::string type = util::get_string(b, "requestType", false, 20);
        if (type.empty()) type = "find_room";
        if (type != "find_room" && type != "landlord" && type != "other") bad_request("requestType không hợp lệ");
        std::string area = util::get_string(b, "desiredArea", false, 150);
        std::string note = util::get_string(b, "note", false, 1000);
        require_phone(phone);
        if (!email.empty() && !util::valid_email(email)) bad_request("Email không hợp lệ");
        auto budget = util::get_double(b, "budgetMax");
        if (budget && *budget < 0) bad_request("Ngân sách không được âm");
        auto user = current_user(app, req);
        app.db.execute(
            "INSERT INTO support_requests (user_id, full_name, phone, email, request_type, desired_area, budget_max, note) VALUES (?,?,?,?,?,?,?,?)",
            {user ? json(user->id) : json(nullptr), name, phone, email.empty() ? json(nullptr) : json(email), type,
             area.empty() ? json(nullptr) : json(area), budget ? json(static_cast<long long>(*budget)) : json(nullptr),
             note.empty() ? json(nullptr) : json(note)});
        res.status = 201;
        return json{{"message", "Đã gửi yêu cầu. Nhân viên sẽ liên hệ với bạn sớm nhất có thể."}};
    });

    // ---- Tuyển môi giới
    add_route(svr, "POST", "/api/broker-applications", [&app](const Req& req, Res& res) {
        rate_limit(app, req, "broker", 4, 600);
        json b = parse_body(req);
        std::string name = util::get_string(b, "fullName", true, 100);
        std::string phone = util::get_string(b, "phone", true, 20);
        std::string email = util::get_string(b, "email", false, 100);
        std::string area = util::get_string(b, "workArea", false, 150);
        std::string exp = util::get_string(b, "experience", false, 1000);
        require_phone(phone);
        if (!email.empty() && !util::valid_email(email)) bad_request("Email không hợp lệ");
        app.db.execute("INSERT INTO broker_applications (full_name, phone, email, work_area, experience) VALUES (?,?,?,?,?)",
                       {name, phone, email.empty() ? json(nullptr) : json(email), area.empty() ? json(nullptr) : json(area),
                        exp.empty() ? json(nullptr) : json(exp)});
        res.status = 201;
        return json{{"message", "Đã nhận đơn đăng ký. Chúng tôi sẽ liên hệ để trao đổi thêm."}};
    });

    // ---- Đặt lịch xem phòng / liên hệ chủ nhà
    add_route(svr, "POST", "/api/viewing-requests", [&app](const Req& req, Res& res) {
        rate_limit(app, req, "viewing", 8, 600);
        json b = parse_body(req);
        auto room_id = util::get_int(b, "roomId");
        if (!room_id) bad_request("Thiếu trường bắt buộc: roomId");
        std::string name = util::get_string(b, "fullName", true, 100);
        std::string phone = util::get_string(b, "phone", true, 20);
        std::string msg = util::get_string(b, "message", false, 1000);
        std::string when = util::get_string(b, "preferredTime", false, 100);
        require_phone(phone);
        if (app.db.scalar("SELECT COUNT(*) FROM rooms WHERE room_id = ? AND status <> 'hidden'", {*room_id}) == 0) not_found("Không tìm thấy phòng");
        auto user = current_user(app, req);
        app.db.execute("INSERT INTO viewing_requests (room_id, user_id, full_name, phone, message, preferred_time) VALUES (?,?,?,?,?,?)",
                       {*room_id, user ? json(user->id) : json(nullptr), name, phone, msg.empty() ? json(nullptr) : json(msg),
                        when.empty() ? json(nullptr) : json(when)});
        res.status = 201;
        return json{{"message", "Đã gửi yêu cầu xem phòng. Chủ nhà sẽ liên hệ với bạn."}};
    });

    // ---- Tra cứu hóa đơn: số điện thoại người thuê + số phòng
    add_route(svr, "GET", "/api/invoices/lookup", [&app](const Req& req, Res&) {
        rate_limit(app, req, "lookup", 20, 60);
        std::string phone = normalize_phone(param(req, "phone"));
        std::string room = param(req, "room");
        std::string month = param(req, "month");
        if (phone.size() < 9) bad_request("Vui lòng nhập số điện thoại đã đăng ký thuê phòng");
        if (room.empty()) bad_request("Vui lòng nhập số phòng (ví dụ P102)");
        if (room.size() > 20) bad_request("Số phòng quá dài");
        if (!month.empty() && !util::valid_month(month)) bad_request("month phải có dạng YYYY-MM");

        std::vector<json> ps = {phone, room};
        std::string cond;
        if (!month.empty()) {
            cond = " AND i.billing_month = ?";
            ps.push_back(month);
        }
        json invoices = app.db.query(
            "SELECT i.invoice_id, i.invoice_code, i.billing_month, i.total_amount, i.status, i.due_date, i.issued_at,"
            " r.room_number, b.name AS building_name, tn.full_name AS tenant_name,"
            " COALESCE((SELECT SUM(p.amount_paid) FROM payments p WHERE p.invoice_id = i.invoice_id), 0) AS paid_amount"
            " FROM invoices i JOIN contracts c ON c.contract_id = i.contract_id JOIN rooms r ON r.room_id = c.room_id"
            " JOIN buildings b ON b.building_id = r.building_id JOIN tenants tn ON tn.tenant_id = c.tenant_id"
            " WHERE REPLACE(REPLACE(REPLACE(tn.phone,' ',''),'.',''),'-','') = ? AND lower(r.room_number) = lower(?)"
            " AND i.status <> 'cancelled'" + cond + " ORDER BY i.billing_month DESC LIMIT 24",
            ps);
        if (invoices.empty())
            not_found("Không tìm thấy hóa đơn. Hãy kiểm tra lại số điện thoại (số đã đăng ký với chủ nhà) và số phòng.");
        for (auto& inv : invoices) {
            long long id = inv["invoiceId"];
            inv["tenantName"] = util::mask_name(inv["tenantName"].get<std::string>());
            inv["remaining"] = std::max<long long>(0, inv["totalAmount"].get<long long>() - inv["paidAmount"].get<long long>());
            inv["details"] = app.db.query(
                "SELECT d.description, s.service_name, d.quantity, d.unit_price, d.subtotal FROM invoice_details d "
                "JOIN services s ON s.service_id = d.service_id WHERE d.invoice_id = ? ORDER BY d.detail_id", {id});
            inv["payments"] = app.db.query("SELECT amount_paid, paid_at, method FROM payments WHERE invoice_id = ? ORDER BY paid_at", {id});
            inv.erase("invoiceId");
        }
        return json{{"invoices", invoices}};
    });

    // ---- Phòng yêu thích (cần đăng nhập)
    add_route(svr, "GET", "/api/favorites", [&app](const Req& req, Res&) {
        User u = require_user(app, req);
        RoomFetch f;
        f.where.add("r.status <> 'hidden'");
        f.where.add("r.room_id IN (SELECT room_id FROM favorites WHERE user_id = ?)", u.id);
        f.order_by = "r.room_id DESC";
        f.limit = 100;
        f.user_id = u.id;
        return json{{"items", fetch_rooms(app, f)}};
    });
    add_route(svr, "POST", "/api/favorites/:id", [&app](const Req& req, Res&) {
        User u = require_user(app, req);
        long long id = path_id(req);
        if (app.db.scalar("SELECT COUNT(*) FROM rooms WHERE room_id = ? AND status <> 'hidden'", {id}) == 0) not_found("Không tìm thấy phòng");
        app.db.execute("INSERT OR IGNORE INTO favorites (user_id, room_id) VALUES (?, ?)", {u.id, id});
        return json{{"favorite", true}};
    });
    add_route(svr, "DELETE", "/api/favorites/:id", [&app](const Req& req, Res&) {
        User u = require_user(app, req);
        app.db.execute("DELETE FROM favorites WHERE user_id = ? AND room_id = ?", {u.id, path_id(req)});
        return json{{"favorite", false}};
    });
}
