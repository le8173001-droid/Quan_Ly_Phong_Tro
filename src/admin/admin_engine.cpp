// Bộ máy CRUD chung của khu quản trị: mỗi bảng là một "Resource" (do 5 mô-đun thành viên định nghĩa trong thanh-vien-*/backend/m*.cpp);
// register_resource() tự sinh 5 API: GET danh sách, GET theo id, POST, PUT/PATCH, DELETE.
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

std::string now_text(App& app) { return app.db.query_one("SELECT datetime('now','localtime') AS n")["n"].get<std::string>(); }

std::string month_label(const std::string& ym) { return ym.substr(5, 2) + "/" + ym.substr(0, 4); }  // 2026-10 -> 10/2026

// ------------------------------------------------------------------ đọc & kiểm tra dữ liệu gửi lên
json collect_values(const Resource& res, const json& body, bool creating) {
    json values = json::object();
    for (const auto& c : res.cols) {
        std::string key = util::snake_to_camel(c.name);
        if (!body.contains(key)) {
            if (creating && c.required) bad_request("Thiếu trường bắt buộc: " + key);
            continue;
        }
        const char* k = key.c_str();
        json v;
        switch (c.type) {
            case T::Int: {
                auto x = util::get_int(body, k);
                v = x ? json(*x) : json(nullptr);
                break;
            }
            case T::Real: {
                auto x = util::get_double(body, k);
                v = x ? json(*x) : json(nullptr);
                break;
            }
            case T::Money: {
                auto x = util::get_double(body, k);
                if (x && *x < 0) bad_request("Trường '" + key + "' không được âm");
                v = x ? json(static_cast<long long>(std::llround(*x))) : json(nullptr);
                break;
            }
            case T::Bool: {
                auto x = util::get_bool(body, k);
                v = x ? json(*x ? 1 : 0) : json(nullptr);
                break;
            }
            case T::Str: {
                std::string s = util::get_string(body, k, false, c.max_len);
                v = s.empty() ? json(nullptr) : json(s);
                if (!c.enums.empty() && !s.empty() && std::find(c.enums.begin(), c.enums.end(), s) == c.enums.end()) {
                    std::string list;
                    for (const auto& e : c.enums) list += (list.empty() ? "" : ", ") + e;
                    bad_request("Trường '" + key + "' phải là một trong: " + list);
                }
                break;
            }
            case T::Date: {
                std::string s = util::get_string(body, k, false, 10);
                if (!s.empty() && !util::valid_date(s)) bad_request("Trường '" + key + "' phải có dạng YYYY-MM-DD");
                v = s.empty() ? json(nullptr) : json(s);
                break;
            }
            case T::Month: {
                std::string s = util::get_string(body, k, false, 7);
                if (!s.empty() && !util::valid_month(s)) bad_request("Trường '" + key + "' phải có dạng YYYY-MM");
                v = s.empty() ? json(nullptr) : json(s);
                break;
            }
            case T::DateTime: {
                std::string s = util::get_string(body, k, false, 25);
                if (!s.empty() && !util::valid_datetime(s)) bad_request("Trường '" + key + "' phải có dạng YYYY-MM-DD HH:MM");
                v = s.empty() ? json(nullptr) : json(util::normalize_datetime(s));
                break;
            }
        }
        if (v.is_null() && c.required) bad_request("Trường '" + key + "' không được để trống");
        values[c.name] = v;
    }
    return values;
}

json fetch_one(App& app, const Resource& res, long long id) {
    return app.db.query_one(res.select + " WHERE t." + res.pk + " = ?", {id});
}

json get_one(App& app, const Resource& res, long long id) {
    json row = fetch_one(app, res, id);
    if (row.is_null()) not_found();
    if (res.decorate) res.decorate(app, row);
    return row;
}

// Bản ghi có thuộc tòa nhà của người dùng không (ADMIN: luôn đúng)
bool in_scope(App& app, const Resource& r, const User& u, long long id) {
    if (r.scope.empty() || u.is_admin()) return true;
    return app.db.scalar("SELECT COUNT(*) FROM " + r.table + " t WHERE t." + r.pk + " = ? AND (" + r.scope +
                         ") IN (SELECT building_id FROM buildings WHERE manager_id = ?)", {id, u.id}) > 0;
}

void require_scope(App& app, const Resource& r, const User& u, long long id) {
    if (!in_scope(app, r, u, id))
        forbidden("Dữ liệu này thuộc tòa nhà không do bạn quản lý nên bạn không được xem / sửa / xóa (chỉ chủ tòa nhà hoặc ADMIN)");
}


// Người thuê có thuộc tòa nhà của chủ tòa này không (ADMIN: luôn đúng)
void require_tenant_in_scope(App& app, const User& u, long long tenant_id) {
    if (u.is_admin()) return;
    long long n = app.db.scalar(
        "SELECT COUNT(*) FROM tenants t WHERE t.tenant_id = ? AND (" + std::string(kTenantBuilding) + ") IN (SELECT building_id FROM buildings WHERE manager_id = ?)",
        {tenant_id, u.id});
    if (n == 0) forbidden("Người thuê này không thuộc tòa nhà do bạn quản lý");
}

void check_image_url(const std::string& url) {
    bool ok = url.rfind("/uploads/", 0) == 0 || url.rfind("/img/", 0) == 0 || url.rfind("https://", 0) == 0 || url.rfind("http://", 0) == 0;
    if (!ok || url.size() > 500) bad_request("Đường dẫn ảnh không hợp lệ");
}


void register_resource(httplib::Server& svr, App& app, const Resource& r) {
    auto guard = [&app, &r](const Req& req) { return r.admin_only ? require_admin(app, req) : require_staff(app, req); };
    // Ghi dữ liệu: bảng "chủ tòa chỉ xem" thì chỉ ADMIN được thêm / sửa / xóa
    auto wguard = [&app, &r, guard](const Req& req) {
        User u = guard(req);
        if (r.manager_ro && !u.is_admin()) forbidden("Dữ liệu dùng chung của hệ thống, chỉ ADMIN mới được thêm / sửa / xóa");
        return u;
    };
    std::string base = "/api/admin/" + r.path;

    // Danh sách: ?q= tìm kiếm, ?<cột>= lọc, ?page= &size=
    add_route(svr, "GET", base, [&app, &r, guard](const Req& req, Res&) {
        User actor = guard(req);
        Where w;
        std::string q = param(req, "q");
        if (!q.empty() && !r.search.empty()) {
            std::string cond;
            std::string pat = "%" + util::like_escape(util::vn_fold(q)) + "%";
            for (const auto& expr : r.search) {
                if (!cond.empty()) cond += " OR ";
                cond += "vn_fold(COALESCE(" + expr + ", '')) LIKE ? ESCAPE '!'";
                w.params.push_back(pat);
            }
            w.conds.push_back(cond);
        }
        if (!r.base_cond.empty()) w.conds.push_back(r.base_cond);
        for (const auto& f : r.filters) {
            // "b.building_id" = cột của bảng JOIN (tham số: buildingId); "room_id" = cột của chính bảng (t.room_id)
            auto dot = f.find('.');
            std::string col = dot == std::string::npos ? "t." + f : f;
            std::string name = dot == std::string::npos ? f : f.substr(dot + 1);
            std::string v = param(req, util::snake_to_camel(name).c_str());
            if (!v.empty()) w.add(col + " = ?", v);
        }
        if (!r.scope.empty() && !actor.is_admin()) {
            w.conds.push_back("(" + r.scope + ") IN (SELECT building_id FROM buildings WHERE manager_id = ?)");
            w.params.push_back(actor.id);
        }
        Paging p = get_paging(req, 20, 200);
        long long total = app.db.scalar("SELECT COUNT(*) FROM (" + r.select + w.sql() + ")", w.params);
        std::vector<json> ps = w.params;
        ps.push_back(p.size);
        ps.push_back(p.offset());
        json items = app.db.query(r.select + w.sql() + " ORDER BY " + r.order + " LIMIT ? OFFSET ?", ps);
        return page_response(std::move(items), total, p);
    });

    add_route(svr, "GET", base + "/:id", [&app, &r, guard](const Req& req, Res&) {
        User actor = guard(req);
        require_scope(app, r, actor, path_id(req));
        return get_one(app, r, path_id(req));
    });
    if (r.read_only) return;

    add_route(svr, "POST", base, [&app, &r, wguard](const Req& req, Res& res) {
        User actor = wguard(req);
        json body = parse_body(req);
        json values = collect_values(r, body, true);
        json none = nullptr;
        long long id = app.db.tx([&] {
            HookCtx ctx{app, body, values, true, 0, none, actor};
            if (r.before) r.before(ctx);
            std::string cols, marks;
            std::vector<json> ps;
            for (auto it = values.begin(); it != values.end(); ++it) {
                cols += (cols.empty() ? "" : ", ") + it.key();
                marks += marks.empty() ? "?" : ", ?";
                ps.push_back(it.value());
            }
            app.db.execute("INSERT INTO " + r.table + " (" + cols + ") VALUES (" + marks + ")", ps);
            long long new_id = app.db.last_insert_id();
            if (r.after) r.after(app, new_id);
            if (r.after_body) r.after_body(app, new_id, body, actor);
            require_scope(app, r, actor, new_id);
            return new_id;
        });
        res.status = 201;
        return get_one(app, r, id);
    });

    auto update = [&app, &r, wguard](const Req& req, Res&) {
        User actor = wguard(req);
        long long id = path_id(req);
        json body = parse_body(req);
        json old = fetch_one(app, r, id);
        if (old.is_null()) not_found();
        require_scope(app, r, actor, id);
        json values = collect_values(r, body, false);
        app.db.tx([&] {
            HookCtx ctx{app, body, values, false, id, old, actor};
            if (r.before) r.before(ctx);
            if (values.empty() && !(r.after_body && !body.empty())) bad_request("Không có trường nào để cập nhật");
            std::string sets;
            std::vector<json> ps;
            for (auto it = values.begin(); it != values.end(); ++it) {
                sets += (sets.empty() ? "" : ", ") + it.key() + " = ?";
                ps.push_back(it.value());
            }
            ps.push_back(id);
            if (!sets.empty()) app.db.execute("UPDATE " + r.table + " SET " + sets + " WHERE " + r.pk + " = ?", ps);
            if (r.after) r.after(app, id);
            if (r.after_body) r.after_body(app, id, body, actor);
            require_scope(app, r, actor, id);
        });
        return get_one(app, r, id);
    };
    add_route(svr, "PUT", base + "/:id", update);
    add_route(svr, "PATCH", base + "/:id", update);

    add_route(svr, "DELETE", base + "/:id", [&app, &r, wguard](const Req& req, Res&) {
        User actor = wguard(req);
        long long id = path_id(req);
        json old = fetch_one(app, r, id);
        if (old.is_null()) not_found();
        require_scope(app, r, actor, id);
        if (r.before_delete) r.before_delete(app, old, actor);
        app.db.tx([&] {
            app.db.execute("DELETE FROM " + r.table + " WHERE " + r.pk + " = ?", {id});
            if (r.after_delete) r.after_delete(app, old);
        });
        return json{{"message", "Đã xóa"}};
    });
}

}  // namespace adm

void register_admin_routes(httplib::Server& svr, App& app) {
    using namespace adm;
    register_m5_routes(svr, app);  // dashboard, options
    register_m3_routes(svr, app);  // tài liệu hợp đồng
    register_m4_routes(svr, app);  // tạo / hủy hóa đơn
    register_m1_routes(svr, app);  // khuyến mãi, cấu hình website
    register_m2_routes(svr, app);  // phòng nổi bật

    // CRUD cho các bảng (đăng ký sau các route cụ thể ở trên)
    static std::vector<Resource> resources;
    for (auto fn : {resources_m1, resources_m2, resources_m3, resources_m4, resources_m5})
        for (auto& r : fn()) resources.push_back(std::move(r));
    for (const auto& r : resources) register_resource(svr, app, r);
}
