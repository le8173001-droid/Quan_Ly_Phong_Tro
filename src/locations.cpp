// Địa giới hành chính Việt Nam (hệ MỚI 34 tỉnh/thành - 2 cấp, hệ CŨ 63 tỉnh/thành - 3 cấp) và gợi ý khu vực gần vị trí người dùng.
#include "m2_rooms_api.hpp"

#include <algorithm>
#include <cmath>
#include <map>

namespace {

std::string avail() { return kPublicRoomCondition; }

std::string system_param(const Req& req) {
    std::string s = util::to_lower_ascii(param(req, "system"));
    if (s.empty() || s == "new") return "NEW";
    if (s == "old") return "OLD";
    bad_request("system phải là new (34 tỉnh, 2 cấp) hoặc old (63 tỉnh, 3 cấp)");
}

}  // namespace

void register_location_routes(httplib::Server& svr, App& app) {
    // Tất cả tỉnh/thành của một hệ (kèm số phòng còn trống)
    add_route(svr, "GET", "/api/locations/provinces", [&app](const Req& req, Res&) {
        std::string sys = system_param(req);
        return app.db.query(
            "SELECT p.province_id, p.name, p.full_name, p.adm_system AS system, COALESCE(c.n, 0) AS room_count FROM provinces p"
            " LEFT JOIN (SELECT b.province_id AS pid, COUNT(*) AS n FROM rooms r JOIN buildings b ON b.building_id = r.building_id"
            "            WHERE " + avail() + " GROUP BY b.province_id) c ON c.pid = p.province_id"
            " WHERE p.adm_system = ? ORDER BY vn_fold(p.name)",
            {sys});
    });

    // Quận/huyện của một tỉnh (chỉ hệ cũ có cấp này)
    add_route(svr, "GET", "/api/locations/districts", [&app](const Req& req, Res&) {
        auto pid = util::parse_ll(param(req, "provinceId"), "provinceId");
        if (!pid) bad_request("Thiếu provinceId");
        return app.db.query(
            "SELECT d.district_id, d.name, d.province_id, COALESCE(c.n, 0) AS room_count FROM districts d"
            " LEFT JOIN (SELECT b.district_id AS did, COUNT(*) AS n FROM rooms r JOIN buildings b ON b.building_id = r.building_id"
            "            WHERE " + avail() + " GROUP BY b.district_id) c ON c.did = d.district_id"
            " WHERE d.province_id = ? ORDER BY CASE WHEN d.name LIKE 'Quận %' THEN 0 WHEN d.name LIKE 'Thành phố %' THEN 1 WHEN d.name LIKE 'Thị xã %' THEN 2 ELSE 3 END,"
            " vn_fold(d.name)",
            {*pid});
    });

    // Phường/xã: theo quận/huyện (hệ cũ) hoặc trực tiếp theo tỉnh (hệ mới), có thể lọc ?q=
    add_route(svr, "GET", "/api/locations/wards", [&app](const Req& req, Res&) {
        Where w;
        if (auto did = util::parse_ll(param(req, "districtId"), "districtId")) w.add("w.district_id = ?", *did);
        else if (auto pid = util::parse_ll(param(req, "provinceId"), "provinceId")) w.add("w.province_id = ?", *pid);
        else bad_request("Thiếu districtId hoặc provinceId");
        std::string q = param(req, "q");
        if (!q.empty()) w.add("vn_fold(w.name) LIKE ? ESCAPE '!'", "%" + util::like_escape(util::vn_fold(q)) + "%");
        return app.db.query(
            "SELECT w.ward_id, w.name, w.province_id, w.district_id, COALESCE(c.n, 0) AS room_count FROM wards w"
            " LEFT JOIN (SELECT b.ward_id AS wid, COUNT(*) AS n FROM rooms r JOIN buildings b ON b.building_id = r.building_id"
            "            WHERE " + avail() + " GROUP BY b.ward_id) c ON c.wid = w.ward_id" + w.sql() +
            " ORDER BY CASE WHEN w.name LIKE 'Phường %' THEN 0 WHEN w.name LIKE 'Thị trấn %' THEN 1 WHEN w.name LIKE 'Xã %' THEN 2 ELSE 3 END, vn_fold(w.name) LIMIT 1500",
            w.params);
    });

    // Gợi ý khu vực gần vị trí thiết bị: gom các tòa nhà còn phòng quanh (lat, lng) theo phường/xã (hoặc quận/tỉnh)
    add_route(svr, "GET", "/api/locations/nearby", [&app](const Req& req, Res&) {
        auto lat = util::parse_double(param(req, "lat"), "lat");
        auto lng = util::parse_double(param(req, "lng"), "lng");
        if (!lat || !lng) bad_request("Cần truyền cả lat và lng");
        if (*lat < -90 || *lat > 90 || *lng < -180 || *lng > 180) bad_request("Toạ độ không hợp lệ");
        double radius = util::parse_double(param(req, "radiusKm"), "radiusKm").value_or(15.0);
        if (radius <= 0 || radius > 500) bad_request("radiusKm phải trong khoảng (0, 500]");

        json rows;
        double used = radius;
        for (double r : {radius, radius * 3, radius * 10}) {
            used = std::min(r, 500.0);
            rows = app.db.query(
                "SELECT * FROM (SELECT b.building_id, b.name AS building_name, b.province_id, b.district_id, b.ward_id,"
                " p.name AS province_name, p.adm_system AS system, d.name AS district_name, w.name AS ward_name,"
                " COUNT(r.room_id) AS room_count, MIN(r.base_rent_price) AS min_price, haversine(?, ?, b.latitude, b.longitude) AS distance_km"
                " FROM buildings b JOIN provinces p ON p.province_id = b.province_id"
                " LEFT JOIN districts d ON d.district_id = b.district_id LEFT JOIN wards w ON w.ward_id = b.ward_id"
                " JOIN rooms r ON r.building_id = b.building_id AND " + avail() +
                " WHERE b.latitude IS NOT NULL AND b.longitude IS NOT NULL GROUP BY b.building_id) WHERE distance_km <= ? ORDER BY distance_km LIMIT 60",
                {*lat, *lng, used});
            if (!rows.empty()) break;
        }

        struct Area {
            json info;
            long long rooms = 0;
            double dist = 1e9;
            long long min_price = 0;
        };
        std::map<std::string, Area> areas;
        std::vector<std::string> order;
        for (const auto& b : rows) {
            std::string level, key, label;
            if (!b["wardId"].is_null()) {
                level = "ward";
                key = "w" + std::to_string(b["wardId"].get<long long>());
                label = b["wardName"].get<std::string>();
                if (!b["districtName"].is_null()) label += ", " + b["districtName"].get<std::string>();
                label += ", " + b["provinceName"].get<std::string>();
            } else if (!b["districtId"].is_null()) {
                level = "district";
                key = "d" + std::to_string(b["districtId"].get<long long>());
                label = b["districtName"].get<std::string>() + ", " + b["provinceName"].get<std::string>();
            } else {
                level = "province";
                key = "p" + std::to_string(b["provinceId"].get<long long>());
                label = b["provinceName"].get<std::string>();
            }
            auto it = areas.find(key);
            if (it == areas.end()) {
                order.push_back(key);
                Area a;
                a.info = {{"level", level}, {"label", label}, {"provinceId", b["provinceId"]}, {"districtId", b["districtId"]},
                          {"wardId", b["wardId"]}, {"system", b["system"]}};
                it = areas.emplace(key, std::move(a)).first;
            }
            it->second.rooms += b["roomCount"].get<long long>();
            it->second.dist = std::min(it->second.dist, b["distanceKm"].get<double>());
            long long mp = b["minPrice"].get<long long>();
            if (it->second.min_price == 0 || mp < it->second.min_price) it->second.min_price = mp;
        }
        json list = json::array();
        for (const auto& k : order) {
            if (list.size() >= 8) break;
            json a = areas[k].info;
            a["roomCount"] = areas[k].rooms;
            a["distanceKm"] = std::round(areas[k].dist * 10) / 10.0;
            a["minPrice"] = areas[k].min_price;
            list.push_back(a);
        }
        return json{{"radiusKm", used}, {"areas", list}, {"lat", *lat}, {"lng", *lng}};
    });
}
