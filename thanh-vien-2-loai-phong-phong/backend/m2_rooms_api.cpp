// THÀNH VIÊN 2 – API phòng: tìm kiếm / chi tiết phòng công khai, sơ đồ phòng và logic trạng thái phòng (rooms).
// Tìm kiếm / xem / thêm / sửa / xóa phòng.
#include "m2_rooms_api.hpp"

#include <algorithm>
#include <cmath>
#include <ctime>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <sstream>

std::string Where::sql() const {
    if (conds.empty()) return "";
    std::string s = " WHERE ";
    for (size_t i = 0; i < conds.size(); ++i) {
        if (i) s += " AND ";
        s += "(" + conds[i] + ")";
    }
    return s;
}

namespace {

const char* kFrom =
    " FROM rooms r"
    " JOIN buildings b ON b.building_id = r.building_id"
    " JOIN room_types t ON t.type_id = r.type_id"
    " JOIN provinces p ON p.province_id = b.province_id"
    " LEFT JOIN districts d ON d.district_id = b.district_id"
    " LEFT JOIN wards w ON w.ward_id = b.ward_id"
    // phòng đang tham gia HOTDEAL còn hiệu lực (giá ưu đãi + số giây còn lại)
    " LEFT JOIN (SELECT pr.room_id AS room_id,"
    // giá ưu đãi: nếu chương trình có % giảm thì tính theo % của giá phòng hiện tại (làm tròn nghìn), không thì dùng giá đã nhập
    "        MIN(CASE WHEN pm.discount_percent IS NOT NULL"
    "                 THEN CAST(ROUND(rr.base_rent_price * (100 - pm.discount_percent) / 100.0 / 1000.0) * 1000 AS INTEGER)"
    "                 ELSE pr.deal_price END) AS deal_price,"
    "        MIN(CAST(strftime('%s', pm.end_at) AS INTEGER) - CAST(strftime('%s','now','localtime') AS INTEGER)) AS deal_seconds_left"
    "      FROM promotion_rooms pr JOIN promotions pm ON pm.promotion_id = pr.promotion_id"
    "        JOIN rooms rr ON rr.room_id = pr.room_id"
    "      WHERE pm.promo_type = 'hotdeal' AND pm.is_active = 1"
    "        AND pm.start_at <= datetime('now','localtime') AND pm.end_at > datetime('now','localtime')"
    "      GROUP BY pr.room_id) dl ON dl.room_id = r.room_id";

const char* kColumns =
    "r.room_id, r.room_number, r.title, r.status, r.base_rent_price, r.area_m2, r.max_occupants, r.current_occupants,"
    " r.is_hot, r.is_new, r.is_now, r.is_featured, r.featured_order, r.thumbnail_url, r.view_count, r.create_date, r.update_date,"
    " r.type_id, t.type_name, t.slug AS type_slug,"
    " r.building_id, b.manager_id, b.name AS building_name, b.cover_url AS building_cover, b.address AS street_address, p.adm_system, b.latitude, b.longitude, b.contact_name,"
    " b.district_id, d.name AS district_name, b.province_id, p.name AS province_name, w.name AS ward_name,"
    " dl.deal_price, dl.deal_seconds_left,"
    " (SELECT COUNT(*) FROM room_images i WHERE i.room_id = r.room_id) AS image_count";

const std::set<std::string> kStatuses = {"available", "occupied", "maintenance", "hidden"};

std::vector<long long> parse_id_list(const std::string& s, const char* name) {
    std::vector<long long> ids;
    std::stringstream ss(s);
    std::string part;
    while (std::getline(ss, part, ',')) {
        part = util::trim(part);
        if (part.empty()) continue;
        ids.push_back(*util::parse_ll(part, name));
        if (ids.size() > 20) bad_request("Chọn quá nhiều tiện nghi");
    }
    return ids;
}

}  // namespace

json fetch_rooms(App& app, const RoomFetch& f) {
    std::string sql = std::string("SELECT ") + kColumns;
    std::vector<json> params;
    if (f.near_pt) {
        sql += ", haversine(?, ?, b.latitude, b.longitude) AS distance_km";
        params.push_back(f.near_pt->first);
        params.push_back(f.near_pt->second);
    }
    if (f.user_id > 0) {
        sql += ", EXISTS(SELECT 1 FROM favorites fv WHERE fv.room_id = r.room_id AND fv.user_id = ?) AS is_favorite";
        params.push_back(f.user_id);
    }
    sql += kFrom + f.where.sql() + " ORDER BY " + f.order_by + " LIMIT ? OFFSET ?";
    for (const auto& p : f.where.params) params.push_back(p);
    params.push_back(f.limit);
    params.push_back(f.offset);

    json rows = app.db.query(sql, params);
    for (auto& r : rows) decorate_room(app, r);
    return rows;
}

long long count_rooms(App& app, const Where& w) {
    return app.db.scalar(std::string("SELECT COUNT(*)") + kFrom + w.sql(), w.params);
}

void decorate_room(App& app, json& r) {
    long long base = r["baseRentPrice"].get<long long>();
    r["price"] = base;
    if (!r["dealPrice"].is_null()) {
        long long deal = r["dealPrice"].get<long long>();
        r["price"] = deal;
        r["originalPrice"] = base;
        r["discountPercent"] = base > 0 && deal < base ? static_cast<long long>(std::llround((base - deal) * 100.0 / base)) : 0;
    }
    std::string addr = r["streetAddress"].get<std::string>();
    for (const char* k : {"wardName", "districtName", "provinceName"}) {
        if (!r[k].is_null()) addr += ", " + r[k].get<std::string>();
    }
    r["fullAddress"] = addr;

    long long cur = r["currentOccupants"].get<long long>(), mx = r["maxOccupants"].get<long long>();
    std::string state = "normal";
    if (cur == 0) state = "empty";
    else if (cur >= mx) state = "full";
    else if (cur * 100 <= mx * app.cfg.almost_empty_percent) state = "almost_empty";
    r["occupancyState"] = state;
}

// ====================================================================== tìm kiếm
namespace {

struct SortSpec {
    const char* key;
    const char* sql;
};

std::string sort_sql(const std::string& key, bool has_near, bool staff) {
    static const SortSpec specs[] = {
        {"newest", "r.create_date DESC, r.room_id DESC"},
        {"oldest", "r.create_date ASC, r.room_id ASC"},
        {"price_asc", "COALESCE(dl.deal_price, r.base_rent_price) ASC, r.room_id ASC"},
        {"price_desc", "COALESCE(dl.deal_price, r.base_rent_price) DESC, r.room_id ASC"},
        {"area_desc", "r.area_m2 DESC, r.room_id ASC"},
        {"area_asc", "r.area_m2 ASC, r.room_id ASC"},
        {"name", "vn_fold(r.title) ASC, r.room_id ASC"},
        {"popular", "r.view_count DESC, r.room_id ASC"},
        {"category", "t.type_name ASC, r.room_id ASC"},
        {"occupants", "r.current_occupants ASC, r.room_id ASC"},
        {"building", "b.name ASC, r.room_number ASC, r.room_id ASC"},
    };
    if (key.empty()) return has_near ? "distance_km ASC, r.room_id ASC" : specs[0].sql;
    if (key == "distance") {
        if (!has_near) bad_request("Sắp xếp theo khoảng cách cần có lat/lng hoặc landmarkId");
        return "distance_km ASC, r.room_id ASC";
    }
    if (key == "roomNumber" && staff) return "r.room_number ASC, r.room_id ASC";
    for (const auto& s : specs)
        if (key == s.key) return s.sql;
    bad_request("Không hỗ trợ sắp xếp theo '" + key +
                "'. Hợp lệ: newest, oldest, price_asc, price_desc, area_desc, area_asc, name, popular, category, occupants, building, distance");
}

}  // namespace

// Điều kiện tìm kiếm dùng chung cho /api/rooms (người dùng và admin)
static void build_filters(App& app, const Req& req, const std::optional<User>& user, bool staff_mode, Where& w,
                          std::optional<std::pair<double, double>>& near_pt, double& radius_km) {
    // Từ khóa: tên tin, mã phòng, tòa nhà, địa chỉ, khu vực, loại hình (không phân biệt hoa/thường và dấu)
    std::string kw = param(req, "keyword");
    if (kw.empty()) kw = param(req, "q");
    if (!kw.empty()) {
        std::string pat = "%" + util::like_escape(util::vn_fold(kw)) + "%";
        w.conds.push_back(
            "vn_fold(r.title) LIKE ? ESCAPE '!' OR vn_fold(r.room_number) LIKE ? ESCAPE '!' OR vn_fold(b.name) LIKE ? ESCAPE '!'"
            " OR vn_fold(b.address) LIKE ? ESCAPE '!' OR vn_fold(COALESCE(d.name,'')) LIKE ? ESCAPE '!' OR vn_fold(COALESCE(w.name,'')) LIKE ? ESCAPE '!'"
            " OR vn_fold(p.name) LIKE ? ESCAPE '!' OR vn_fold(t.type_name) LIKE ? ESCAPE '!'");
        for (int i = 0; i < 8; ++i) w.params.push_back(pat);
    }

    // Loại hình (id hoặc slug), khu vực
    if (auto v = util::parse_ll(param(req, "typeId"), "typeId")) w.add("r.type_id = ?", *v);
    std::string slug = param(req, "type");
    if (!slug.empty()) w.add("t.slug = ?", slug);
    // Tỉnh/thành: chọn ở hệ cũ hay mới đều tìm được các tòa nhà cùng tên tỉnh (vd "Hà Nội" cũ và "Hà Nội" mới)
    if (auto v = util::parse_ll(param(req, "provinceId"), "provinceId"))
        w.add("b.province_id IN (SELECT province_id FROM provinces WHERE name = (SELECT name FROM provinces WHERE province_id = ?))", *v);
    if (auto v = util::parse_ll(param(req, "districtId"), "districtId")) w.add("b.district_id = ?", *v);
    if (auto v = util::parse_ll(param(req, "wardId"), "wardId")) w.add("b.ward_id = ?", *v);
    if (auto v = util::parse_ll(param(req, "buildingId"), "buildingId")) w.add("r.building_id = ?", *v);

    // Giá, diện tích
    auto min_price = util::parse_double(param(req, "minPrice"), "minPrice");
    auto max_price = util::parse_double(param(req, "maxPrice"), "maxPrice");
    if (min_price && max_price && *min_price > *max_price) bad_request("minPrice không được lớn hơn maxPrice");
    if (min_price) w.add("COALESCE(dl.deal_price, r.base_rent_price) >= ?", *min_price);
    if (max_price) w.add("COALESCE(dl.deal_price, r.base_rent_price) <= ?", *max_price);
    auto min_area = util::parse_double(param(req, "minArea"), "minArea");
    auto max_area = util::parse_double(param(req, "maxArea"), "maxArea");
    if (min_area && max_area && *min_area > *max_area) bad_request("minArea không được lớn hơn maxArea");
    if (min_area) w.add("r.area_m2 >= ?", *min_area);
    if (max_area) w.add("r.area_m2 <= ?", *max_area);

    // Cờ
    for (auto [key, col] : {std::pair<const char*, const char*>{"isNew", "r.is_new"}, {"isHot", "r.is_hot"}, {"isNow", "r.is_now"}}) {
        std::string v = util::to_lower_ascii(param(req, key));
        if (v == "true" || v == "1") w.add(std::string(col) + " = 1");
        else if (v == "false" || v == "0") w.add(std::string(col) + " = 0");
    }
    std::string deal = util::to_lower_ascii(param(req, "hotdeal"));
    if (deal == "true" || deal == "1") w.add("dl.deal_price IS NOT NULL");

    // Tiện nghi: phòng phải có đủ tất cả tiện nghi được chọn
    for (long long id : parse_id_list(param(req, "amenityIds"), "amenityIds"))
        w.add("EXISTS (SELECT 1 FROM room_amenities ra WHERE ra.room_id = r.room_id AND ra.amenity_id = ?)", id);

    // Gần một địa điểm (trường học, nơi làm việc) hoặc gần vị trí của tôi
    radius_km = 3.0;
    if (auto v = util::parse_double(param(req, "radiusKm"), "radiusKm")) {
        if (*v <= 0 || *v > 100) bad_request("radiusKm phải trong khoảng (0, 100]");
        radius_km = *v;
    }
    if (auto lid = util::parse_ll(param(req, "landmarkId"), "landmarkId")) {
        json lm = app.db.query_one("SELECT latitude, longitude FROM landmarks WHERE landmark_id = ?", {*lid});
        if (lm.is_null()) bad_request("Địa điểm không tồn tại");
        near_pt = std::make_pair(lm["latitude"].get<double>(), lm["longitude"].get<double>());
    } else {
        auto lat = util::parse_double(param(req, "lat"), "lat");
        auto lng = util::parse_double(param(req, "lng"), "lng");
        if (lat.has_value() != lng.has_value()) bad_request("Cần truyền cả lat và lng");
        if (lat) {
            if (*lat < -90 || *lat > 90 || *lng < -180 || *lng > 180) bad_request("Toạ độ không hợp lệ");
            near_pt = std::make_pair(*lat, *lng);
        }
    }
    if (near_pt) {
        w.add("b.latitude IS NOT NULL AND b.longitude IS NOT NULL AND haversine(?, ?, b.latitude, b.longitude) <= ?");
        w.params.push_back(near_pt->first);
        w.params.push_back(near_pt->second);
        w.params.push_back(radius_km);
    }

    // Trạng thái
    std::string status = param(req, "status");
    if (staff_mode) {
        // Chủ tòa nhà (MANAGER) chỉ thấy phòng thuộc tòa nhà của mình; ADMIN thấy tất cả
        if (user && !user->is_admin()) w.add("b.manager_id = ?", user->id);
        if (!status.empty()) {
            if (!kStatuses.count(status)) bad_request("status phải là: available, occupied, maintenance hoặc hidden");
            w.add("r.status = ?", status);
        } else {
            w.add("r.status <> 'hidden'");  // mặc định ẩn phòng đã xóa mềm
        }
        // Lọc theo số người ở (dành cho admin): ế / sắp trống / đã kín
        std::string occ = util::to_lower_ascii(param(req, "occupancy"));
        if (occ == "empty") w.add("r.current_occupants = 0");
        else if (occ == "almost_empty")
            w.add("r.current_occupants > 0 AND r.current_occupants * 100 <= r.max_occupants * ?", (long long)app.cfg.almost_empty_percent);
        else if (occ == "full") w.add("r.current_occupants >= r.max_occupants");
        else if (!occ.empty()) bad_request("occupancy phải là: empty, almost_empty hoặc full");
    } else {
        w.add(kPublicRoomCondition);
    }
}

// ====================================================================== thêm / sửa
namespace {

struct RoomInput {
    long long building_id = 0, type_id = 0;
    std::string room_number, title, description, status = "available", thumbnail_url;
    long long price = 0;
    std::optional<double> area;
    long long max_occ = 2, cur_occ = 0;
    bool hot = false, is_new = false, now = false;
};

void check_room(App& app, const RoomInput& in) {
    if (app.db.scalar("SELECT COUNT(*) FROM buildings WHERE building_id = ?", {in.building_id}) == 0) bad_request("Tòa nhà không tồn tại");
    if (app.db.scalar("SELECT COUNT(*) FROM room_types WHERE type_id = ?", {in.type_id}) == 0) bad_request("Loại hình không tồn tại");
    if (in.title.empty()) bad_request("Tiêu đề không được để trống");
    if (in.price < 0) bad_request("Giá thuê không được âm");
    if (in.area && *in.area <= 0) bad_request("Diện tích phải lớn hơn 0");
    if (in.max_occ < 1 || in.max_occ > 500) bad_request("Sức chứa tối đa phải từ 1 đến 500");
    if (in.cur_occ < 0) bad_request("Số người đang ở không được âm");
    if (in.cur_occ > in.max_occ)
        bad_request("Số người đang ở (" + std::to_string(in.cur_occ) + ") không được vượt quá sức chứa (" + std::to_string(in.max_occ) + ")");
    if (!kStatuses.count(in.status)) bad_request("status phải là: available, occupied, maintenance hoặc hidden");
    if (!in.thumbnail_url.empty()) {
        bool ok = in.thumbnail_url.rfind("http://", 0) == 0 || in.thumbnail_url.rfind("https://", 0) == 0 || in.thumbnail_url.rfind('/', 0) == 0;
        if (!ok) bad_request("Ảnh đại diện phải là đường dẫn http(s) hoặc bắt đầu bằng '/'");
    }
}

void apply_body(const json& b, RoomInput& in, bool creating) {
    if (creating || b.contains("buildingId")) {
        auto v = util::get_int(b, "buildingId");
        if (!v) bad_request("Thiếu trường bắt buộc: buildingId");
        in.building_id = *v;
    }
    if (creating || b.contains("typeId")) {
        auto v = util::get_int(b, "typeId");
        if (!v) bad_request("Thiếu trường bắt buộc: typeId");
        in.type_id = *v;
    }
    if (creating) {
        in.room_number = util::get_string(b, "roomNumber", true, 20);
        static const std::regex re("^[A-Za-z0-9._-]+$");
        if (!std::regex_match(in.room_number, re)) bad_request("Mã phòng chỉ gồm chữ, số và các ký tự . _ -");
    }
    if (creating || b.contains("title")) in.title = util::get_string(b, "title", true, 200);
    if (b.contains("description")) in.description = util::get_string(b, "description", false, 5000);
    if (b.contains("status")) in.status = util::get_string(b, "status", true, 20);
    if (b.contains("thumbnailUrl")) in.thumbnail_url = util::get_string(b, "thumbnailUrl", false, 500);
    if (creating || b.contains("baseRentPrice")) {
        auto v = util::get_double(b, "baseRentPrice");
        if (!v) bad_request("Thiếu trường bắt buộc: baseRentPrice");
        in.price = static_cast<long long>(std::llround(*v));
    }
    if (b.contains("areaM2")) in.area = util::get_double(b, "areaM2");
    if (auto v = util::get_int(b, "maxOccupants")) in.max_occ = *v;
    if (auto v = util::get_int(b, "currentOccupants")) in.cur_occ = *v;
    if (auto v = util::get_bool(b, "isHot")) in.hot = *v;
    if (auto v = util::get_bool(b, "isNew")) in.is_new = *v;
    if (auto v = util::get_bool(b, "isNow")) in.now = *v;
}

void save_amenities_and_images(App& app, long long room_id, const json& b) {
    if (b.contains("amenityIds")) {
        if (!b["amenityIds"].is_array()) bad_request("amenityIds phải là mảng");
        app.db.execute("DELETE FROM room_amenities WHERE room_id = ?", {room_id});
        for (const auto& a : b["amenityIds"]) {
            if (!a.is_number_integer()) bad_request("amenityIds chỉ gồm số");
            app.db.execute("INSERT OR IGNORE INTO room_amenities (room_id, amenity_id) VALUES (?, ?)", {room_id, a.get<long long>()});
        }
    }
    if (b.contains("imageUrls")) {
        if (!b["imageUrls"].is_array()) bad_request("imageUrls phải là mảng");
        if (b["imageUrls"].size() > 30) bad_request("Tối đa 30 ảnh");
        app.db.execute("DELETE FROM room_images WHERE room_id = ?", {room_id});
        int order = 0;
        for (const auto& u : b["imageUrls"]) {
            if (!u.is_string()) bad_request("imageUrls chỉ gồm chuỗi");
            std::string url = util::trim(u.get<std::string>());
            if (url.empty()) continue;
            bool ok = url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0 || url.rfind('/', 0) == 0;
            if (!ok || url.size() > 500) bad_request("Đường dẫn ảnh không hợp lệ");
            app.db.execute("INSERT INTO room_images (room_id, url, sort_order) VALUES (?, ?, ?)", {room_id, url, order++});
        }
        // Ảnh đầu tiên làm ảnh đại diện (thay cho ảnh minh họa mặc định)
        app.db.execute(
            "UPDATE rooms SET thumbnail_url = COALESCE((SELECT url FROM room_images WHERE room_id = ? ORDER BY sort_order, image_id LIMIT 1),"
            " '/img/types/' || (SELECT slug FROM room_types t WHERE t.type_id = rooms.type_id) || '.svg') WHERE room_id = ?",
            {room_id, room_id});
    }
}

// Mỗi người (IP) chỉ tính 1 lượt xem / phòng / 30 phút: tránh bấm F5 liên tục làm tăng "nổi bật theo lượt xem"
bool should_count_view(const std::string& ip, long long room_id) {
    static std::mutex mu;
    static std::map<std::string, std::time_t> seen;
    std::lock_guard<std::mutex> lock(mu);
    std::time_t now = std::time(nullptr);
    if (seen.size() > 20000) seen.clear();
    std::string key = ip + "|" + std::to_string(room_id);
    auto it = seen.find(key);
    if (it != seen.end() && now - it->second < 1800) return false;
    seen[key] = now;
    return true;
}

json room_detail(App& app, long long id, const std::optional<User>& user, bool staff, bool count_view) {
    RoomFetch f;
    f.where.add("r.room_id = ?", id);
    if (!staff) f.where.add("r.status <> 'hidden'");
    f.limit = 1;
    f.user_id = user ? user->id : 0;
    json rows = fetch_rooms(app, f);
    if (rows.empty()) not_found("Không tìm thấy phòng");
    json room = rows[0];

    json extra = app.db.query_one(
        "SELECT r.description, r.create_by, r.update_by, b.description AS building_description FROM rooms r JOIN buildings b ON b.building_id = r.building_id WHERE r.room_id = ?",
        {id});
    room["description"] = extra["description"];
    room["buildingDescription"] = extra["buildingDescription"];
    if (staff) {
        room["createBy"] = extra["createBy"];
        room["updateBy"] = extra["updateBy"];
    }
    room["images"] = app.db.query("SELECT image_id, url FROM room_images WHERE room_id = ? ORDER BY sort_order, image_id", {id});
    room["buildingImages"] = app.db.query(
        "SELECT image_id, url FROM building_images WHERE building_id = ? ORDER BY sort_order, image_id", {room["buildingId"]});
    room["amenities"] = app.db.query(
        "SELECT a.amenity_id, a.name, a.icon FROM room_amenities ra JOIN amenities a ON a.amenity_id = ra.amenity_id WHERE ra.room_id = ? ORDER BY a.amenity_id",
        {id});

    // Trường/nơi làm việc gần tòa nhà (trong bán kính 5 km)
    if (!room["latitude"].is_null()) {
        room["nearbyLandmarks"] = app.db.query(
            "SELECT * FROM (SELECT landmark_id, name, landmark_type, haversine(?, ?, latitude, longitude) AS distance_km FROM landmarks) "
            "WHERE distance_km <= 5 ORDER BY distance_km LIMIT 6",
            {room["latitude"], room["longitude"]});
    } else {
        room["nearbyLandmarks"] = json::array();
    }

    // Phòng tương tự: cùng khu vực
    RoomFetch sim;
    sim.where.add(kPublicRoomCondition);
    sim.where.add("r.room_id <> ?", id);
    if (!room["districtId"].is_null()) sim.where.add("b.district_id = ?", room["districtId"]);
    else sim.where.add("b.province_id = ?", room["provinceId"]);
    sim.limit = 4;
    sim.user_id = f.user_id;
    room["similarRooms"] = fetch_rooms(app, sim);

    if (count_view) app.db.execute("UPDATE rooms SET view_count = view_count + 1 WHERE room_id = ?", {id});
    return room;
}

}  // namespace

void register_room_routes(httplib::Server& svr, App& app) {
    // GET /api/rooms: tìm kiếm + lọc + sắp xếp + phân trang
    //   Người dùng: chỉ thấy phòng còn chỗ. Quản lý (admin=1 + đăng nhập): thấy mọi phòng, có thêm lọc status/occupancy.
    add_route(svr, "GET", "/api/rooms", [&app](const Req& req, Res&) {
        auto user = current_user(app, req);
        bool staff_mode = false;
        if (param(req, "admin") == "1") {
            if (!user) unauthorized();
            if (!user->is_staff()) forbidden("Chỉ quản trị viên / quản lý mới dùng được chế độ admin");
            staff_mode = true;
        }
        RoomFetch f;
        double radius = 3;
        build_filters(app, req, user, staff_mode, f.where, f.near_pt, radius);
        Paging p = get_paging(req, 12, 100);
        f.limit = p.size;
        f.offset = p.offset();
        f.user_id = user ? user->id : 0;
        f.order_by = sort_sql(param(req, "sort"), f.near_pt.has_value(), staff_mode);
        long long total = count_rooms(app, f.where);
        json items = fetch_rooms(app, f);
        json out = page_response(std::move(items), total, p);
        if (f.near_pt) out["radiusKm"] = radius;
        return out;
    });

    // GET /api/rooms/{id}: chi tiết phòng
    add_route(svr, "GET", "/api/rooms/:id", [&app](const Req& req, Res&) {
        auto user = current_user(app, req);
        long long id = path_id(req);
        // Chỉ ADMIN hoặc chủ tòa nhà chứa phòng này mới xem được bản dành cho quản lý (kể cả phòng ẩn)
        bool staff = false;
        if (user && user->is_staff()) {
            long long b = app.db.scalar("SELECT building_id FROM rooms WHERE room_id = ?", {id});
            staff = b > 0 && owns_building(app, *user, b);
        }
        bool count = !staff && should_count_view(client_ip(req), id);
        return room_detail(app, id, user, staff, count);
    });

    // GET /api/rooms/{id}/contact: bấm "Xem SĐT"
    add_route(svr, "GET", "/api/rooms/:id/contact", [&app](const Req& req, Res&) {
        rate_limit(app, req, "contact", 60, 60);
        json row = app.db.query_one(
            "SELECT b.contact_name, b.contact_phone FROM rooms r JOIN buildings b ON b.building_id = r.building_id "
            "WHERE r.room_id = ? AND r.status <> 'hidden'",
            {path_id(req)});
        if (row.is_null()) not_found("Không tìm thấy phòng");
        json zalo = app.db.query_one("SELECT setting_value FROM site_settings WHERE setting_key = 'zalo'");
        row["zalo"] = zalo.is_null() ? row["contactPhone"] : zalo["settingValue"];
        return row;
    });

    // POST /api/rooms: thêm phòng
    add_route(svr, "POST", "/api/rooms", [&app](const Req& req, Res& res) {
        User u = require_staff(app, req);
        json b = parse_body(req);
        RoomInput in;
        apply_body(b, in, true);
        require_building_owner(app, u, in.building_id);
        check_room(app, in);
        long long id = app.db.tx([&] {
            app.db.execute(
                "INSERT INTO rooms (building_id, type_id, room_number, status, base_rent_price, title, description, area_m2, max_occupants,"
                " current_occupants, is_hot, is_new, is_now, thumbnail_url, create_by, update_by) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?)",
                {in.building_id, in.type_id, in.room_number, in.status, in.price, in.title,
                 in.description.empty() ? json(nullptr) : json(in.description), in.area ? json(*in.area) : json(nullptr), in.max_occ,
                 in.cur_occ, in.hot, in.is_new, in.now, in.thumbnail_url.empty() ? json(nullptr) : json(in.thumbnail_url), u.id, u.id});
            long long rid = app.db.last_insert_id();
            save_amenities_and_images(app, rid, b);
            sync_building_total(app, in.building_id);
            return rid;
        });
        res.status = 201;
        res.set_header("Location", "/api/rooms/" + std::to_string(id));
        return room_detail(app, id, u, true, false);
    });

    // PUT /api/rooms/{id}: sửa phòng (chỉ gửi các trường cần đổi; mã phòng roomNumber không sửa được)
    auto update = [&app](const Req& req, Res&) {
        User u = require_staff(app, req);
        long long id = path_id(req);
        json b = parse_body(req);
        json cur = app.db.query_one("SELECT * FROM rooms WHERE room_id = ?", {id});
        if (cur.is_null()) not_found("Không tìm thấy phòng");
        require_building_owner(app, u, cur["buildingId"].get<long long>());
        if (b.contains("roomNumber") && b["roomNumber"] != cur["roomNumber"])
            bad_request("Không được sửa mã phòng (roomNumber)");

        RoomInput in;
        in.building_id = cur["buildingId"];
        in.type_id = cur["typeId"];
        in.room_number = cur["roomNumber"];
        in.title = cur["title"];
        in.description = cur["description"].is_null() ? "" : cur["description"].get<std::string>();
        in.status = cur["status"];
        in.thumbnail_url = cur["thumbnailUrl"].is_null() ? "" : cur["thumbnailUrl"].get<std::string>();
        in.price = cur["baseRentPrice"];
        if (!cur["areaM2"].is_null()) in.area = cur["areaM2"].get<double>();
        in.max_occ = cur["maxOccupants"];
        in.cur_occ = cur["currentOccupants"];
        in.hot = cur["isHot"];
        in.is_new = cur["isNew"];
        in.now = cur["isNow"];
        apply_body(b, in, false);
        if (in.building_id != cur["buildingId"].get<long long>()) require_building_owner(app, u, in.building_id);
        check_room(app, in);

        app.db.tx([&] {
            app.db.execute(
                "UPDATE rooms SET building_id=?, type_id=?, status=?, base_rent_price=?, title=?, description=?, area_m2=?, max_occupants=?,"
                " current_occupants=?, is_hot=?, is_new=?, is_now=?, thumbnail_url=?, update_by=?, update_date=datetime('now','localtime') WHERE room_id=?",
                {in.building_id, in.type_id, in.status, in.price, in.title, in.description.empty() ? json(nullptr) : json(in.description),
                 in.area ? json(*in.area) : json(nullptr), in.max_occ, in.cur_occ, in.hot, in.is_new, in.now,
                 in.thumbnail_url.empty() ? json(nullptr) : json(in.thumbnail_url), u.id, id});
            save_amenities_and_images(app, id, b);
            sync_building_total(app, in.building_id);
            sync_building_total(app, cur["buildingId"].get<long long>());
        });
        return room_detail(app, id, u, true, false);
    };
    add_route(svr, "PUT", "/api/rooms/:id", update);
    add_route(svr, "PATCH", "/api/rooms/:id", update);

    // DELETE /api/rooms/{id}?type=1 xóa vĩnh viễn (ADMIN) | type=2 ẩn phòng (mặc định)
    add_route(svr, "DELETE", "/api/rooms/:id", [&app](const Req& req, Res&) {
        long long id = path_id(req);
        std::string type = param(req, "type");
        if (type.empty()) type = "2";
        if (type != "1" && type != "2") bad_request("type phải là 1 (xóa vĩnh viễn) hoặc 2 (ẩn phòng)");
        User u = type == "1" ? require_admin(app, req) : require_staff(app, req);
        json cur = app.db.query_one("SELECT building_id, room_number FROM rooms WHERE room_id = ?", {id});
        if (cur.is_null()) not_found("Không tìm thấy phòng");
        std::string number = cur["roomNumber"];
        long long building = cur["buildingId"];
        require_building_owner(app, u, building);
        if (type == "1") {
            try {
                app.db.tx([&] {
                    app.db.execute("DELETE FROM rooms WHERE room_id = ?", {id});
                    sync_building_total(app, building);
                });
            } catch (const HttpError& e) {
                if (e.status == 409)
                    conflict("Phòng " + number + " đã có hợp đồng / sự cố / chỉ số điện nước liên quan nên không thể xóa vĩnh viễn. Hãy ẩn phòng (type=2).");
                throw;
            }
            return json{{"message", "Đã xóa vĩnh viễn phòng " + number}};
        }
        app.db.tx([&] {
            app.db.execute("UPDATE rooms SET status='hidden', update_by=?, update_date=datetime('now','localtime') WHERE room_id=?", {u.id, id});
            sync_building_total(app, building);
        });
        return json{{"message", "Đã ẩn phòng " + number}};
    });

    // POST /api/rooms/{id}/restore: khôi phục phòng đã ẩn
    add_route(svr, "POST", "/api/rooms/:id/restore", [&app](const Req& req, Res&) {
        User u = require_staff(app, req);
        long long id = path_id(req);
        json cur = app.db.query_one("SELECT status, current_occupants, max_occupants, building_id FROM rooms WHERE room_id = ?", {id});
        if (cur.is_null()) not_found("Không tìm thấy phòng");
        require_building_owner(app, u, cur["buildingId"].get<long long>());
        if (cur["status"] != "hidden") bad_request("Phòng này không ở trạng thái ẩn");
        std::string st = cur["currentOccupants"].get<long long>() >= cur["maxOccupants"].get<long long>() ? "occupied" : "available";
        app.db.tx([&] {
            app.db.execute("UPDATE rooms SET status=?, update_by=?, update_date=datetime('now','localtime') WHERE room_id=?", {st, u.id, id});
            sync_building_total(app, cur["buildingId"].get<long long>());
        });
        return json{{"message", "Đã khôi phục phòng"}, {"status", st}};
    });
}
