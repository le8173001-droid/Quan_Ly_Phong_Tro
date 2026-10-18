// THÀNH VIÊN 2 – Loại phòng & Phòng (ROOM_TYPES, ROOMS). API phòng/tìm kiếm công khai ở rooms.cpp.
// Bổ sung: ghim phòng nổi bật.
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

std::vector<Resource> resources_m2() {
    std::vector<Resource> rs;

    // ---- room_types
    {
        Resource r;
        r.path = "room-types"; r.table = "room_types"; r.pk = "type_id"; r.manager_ro = true;
        r.cols = {{"type_name", T::Str, true, {}, 100}, {"slug", T::Str, true, {}, 50}, {"icon", T::Str, false, {}, 30},
                  {"description", T::Str, false, {}, 255}, {"sort_order", T::Int}};
        r.select = "SELECT t.*, (SELECT COUNT(*) FROM rooms rm WHERE rm.type_id = t.type_id AND rm.status <> 'hidden') AS room_count FROM room_types t";
        r.search = {"t.type_name", "t.slug"};
        r.order = "t.sort_order, t.type_id";
        r.before = [](HookCtx& c) {
            static const std::regex re("^[a-z0-9-]+$");
            if (c.values.contains("slug") && !std::regex_match(c.values["slug"].get<std::string>(), re))
                bad_request("Slug chỉ gồm chữ thường không dấu, số và dấu gạch ngang");
        };
        rs.push_back(r);
    }

    return rs;
}

void register_m2_routes(httplib::Server& svr, App& app) {
    // ---- Phòng nổi bật (ADMIN): xem cấu hình + danh sách, ghim / bỏ ghim một phòng
    add_route(svr, "GET", "/api/admin/featured", [&app](const Req& req, Res&) {
        require_admin(app, req);
        json settings = json::object();
        for (const auto& r : app.db.query("SELECT setting_key, setting_value FROM site_settings WHERE setting_key LIKE 'featured_%'"))
            settings[r["settingKey"].get<std::string>()] = r["settingValue"];
        json out;
        out["settings"] = settings;
        out["sections"] = featured_sections_public(app, 0);
        out["pinned"] = app.db.query(
            "SELECT r.room_id, r.room_number, r.title, t.type_name, b.name AS building_name, r.featured_order, r.view_count FROM rooms r "
            "JOIN room_types t ON t.type_id = r.type_id JOIN buildings b ON b.building_id = r.building_id WHERE r.is_featured = 1 ORDER BY t.sort_order, r.featured_order, r.room_id");
        return out;
    });
    add_route(svr, "POST", "/api/admin/rooms/:id/feature", [&app](const Req& req, Res&) {
        require_admin(app, req);
        long long id = path_id(req);
        json b = parse_body(req);
        auto flag = util::get_bool(b, "isFeatured");
        if (!flag) bad_request("Thiếu trường bắt buộc: isFeatured");
        auto order = util::get_int(b, "featuredOrder");
        if (app.db.scalar("SELECT COUNT(*) FROM rooms WHERE room_id = ?", {id}) == 0) not_found("Không tìm thấy phòng");
        app.db.execute("UPDATE rooms SET is_featured = ?, featured_order = ? WHERE room_id = ?", {*flag ? 1 : 0, order.value_or(0), id});
        return json{{"message", *flag ? "Đã đưa phòng lên mục nổi bật" : "Đã bỏ phòng khỏi mục nổi bật"}};
    });

}

}  // namespace adm
