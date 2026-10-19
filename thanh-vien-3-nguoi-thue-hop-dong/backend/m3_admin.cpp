// THÀNH VIÊN 3 – Người thuê, Xe cộ & Hợp đồng (TENANTS, VEHICLES, CONTRACTS) + bản hợp đồng để in PDF.
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

std::vector<Resource> resources_m3() {
    std::vector<Resource> rs;

    // ---- tenants: nhóm theo tòa nhà -> phòng
    {
        Resource r;
        r.path = "tenants"; r.table = "tenants"; r.pk = "tenant_id";
        r.scope = kTenantBuilding;
        r.cols = {{"user_id", T::Int}, {"full_name", T::Str, true, {}, 100}, {"phone", T::Str, true, {}, 20}, {"email", T::Str, false, {}, 100},
                  {"id_number", T::Str, false, {}, 20}, {"room_id", T::Int}, {"birth_date", T::Date}, {"gender", T::Str, false, {"Nam", "Nữ", "Khác"}},
                  {"id_issue_date", T::Date}, {"id_issue_place", T::Str, false, {}, 150}, {"permanent_address", T::Str, false, {}, 255}};
        r.select = "SELECT t.*, rm.room_number, b.building_id, b.name AS building_name,"
                   " (SELECT COUNT(*) FROM vehicles v WHERE v.tenant_id = t.tenant_id) AS vehicle_count,"
                   " (SELECT COUNT(*) FROM contracts c WHERE c.tenant_id = t.tenant_id AND c.status = 'active') AS active_contracts"
                   " FROM tenants t LEFT JOIN rooms rm ON rm.room_id = t.room_id LEFT JOIN buildings b ON b.building_id = rm.building_id";
        r.search = {"t.full_name", "t.phone", "t.email", "t.id_number", "rm.room_number", "b.name"};
        r.filters = {"b.building_id", "room_id", "user_id"};
        r.order = "COALESCE(b.name, '~'), rm.room_number, t.full_name";
        r.before = [](HookCtx& c) {
            if (c.values.contains("phone") && !util::valid_phone(c.values["phone"].get<std::string>())) bad_request("Số điện thoại không hợp lệ");
            if (c.values.contains("email") && !c.values["email"].is_null() && !util::valid_email(c.values["email"].get<std::string>()))
                bad_request("Email không hợp lệ");
            if (!c.actor.is_admin()) {
                json room = c.values.contains("room_id") ? c.values["room_id"] : (c.id ? c.old["roomId"] : json(nullptr));
                if (room.is_null()) bad_request("Vui lòng chọn phòng (tòa nhà của bạn) mà người thuê đang ở");
                long long b = c.app.db.scalar("SELECT building_id FROM rooms WHERE room_id = ?", {room});
                if (b == 0) bad_request("Phòng không tồn tại");
                require_building_owner(c.app, c.actor, b);
            } else if (c.values.contains("room_id") && !c.values["room_id"].is_null() &&
                       c.app.db.scalar("SELECT COUNT(*) FROM rooms WHERE room_id = ?", {c.values["room_id"]}) == 0) {
                bad_request("Phòng không tồn tại");
            }
        };
        rs.push_back(r);
    }


    // ---- vehicles: nhóm theo tòa nhà -> phòng (qua người thuê)
    {
        Resource r;
        r.path = "vehicles"; r.table = "vehicles"; r.pk = "vehicle_id";
        r.scope = kVehicleBuilding;
        r.cols = {{"tenant_id", T::Int, true}, {"license_plate", T::Str, true, {}, 20}, {"vehicle_type", T::Str, false, {"motorbike", "car", "bicycle"}},
                  {"brand_color", T::Str, false, {}, 60}};
        r.select = "SELECT t.*, tn.full_name AS tenant_name, tn.phone AS tenant_phone, tn.room_id, rm.room_number, b.building_id, b.name AS building_name"
                   " FROM vehicles t JOIN tenants tn ON tn.tenant_id = t.tenant_id LEFT JOIN rooms rm ON rm.room_id = tn.room_id"
                   " LEFT JOIN buildings b ON b.building_id = rm.building_id";
        r.search = {"t.license_plate", "tn.full_name", "rm.room_number", "b.name"};
        r.filters = {"b.building_id", "tenant_id", "vehicle_type", "tn.room_id"};
        r.order = "COALESCE(b.name, '~'), rm.room_number, t.license_plate";
        r.before = [](HookCtx& c) {
            if (c.values.contains("tenant_id")) require_tenant_in_scope(c.app, c.actor, c.values["tenant_id"]);
        };
        rs.push_back(r);
    }


    // ---- contracts: hợp đồng thuê chi tiết (dùng để in / xuất PDF)
    {
        Resource r;
        r.path = "contracts"; r.table = "contracts"; r.pk = "contract_id";
        r.scope = kRoomBuilding;
        r.cols = {{"room_id", T::Int, true}, {"tenant_id", T::Int, true}, {"start_date", T::Date, true}, {"end_date", T::Date, true},
                  {"stay_type", T::Str, false, {"short_term", "long_term"}}, {"booking_source", T::Str, false, {"direct", "airbnb", "booking", "agent"}},
                  {"deposit_amount", T::Money}, {"status", T::Str, false, {"pending", "active", "ended", "cancelled"}},
                  {"monthly_rent", T::Money}, {"note", T::Str, false, {}, 500}, {"sign_date", T::Date}, {"sign_place", T::Str, false, {}, 150},
                  {"payment_day", T::Int}, {"payment_method", T::Str, false, {}, 100}, {"late_fee_percent", T::Real}, {"notice_days", T::Int},
                  {"electric_price", T::Money}, {"water_price", T::Money}, {"service_note", T::Str, false, {}, 500},
                  {"assets_text", T::Str, false, {}, 4000}, {"extra_terms", T::Str, false, {}, 4000}};
        r.select = "SELECT t.*, rm.room_number, rm.title AS room_title, b.building_id, b.name AS building_name, tn.full_name AS tenant_name, tn.phone AS tenant_phone"
                   " FROM contracts t JOIN rooms rm ON rm.room_id = t.room_id JOIN buildings b ON b.building_id = rm.building_id"
                   " JOIN tenants tn ON tn.tenant_id = t.tenant_id";
        r.search = {"rm.room_number", "tn.full_name", "tn.phone", "b.name", "t.contract_code"};
        r.filters = {"b.building_id", "room_id", "tenant_id", "status", "stay_type", "booking_source"};
        r.order = "b.name, rm.room_number, t.contract_id DESC";
        r.before = [](HookCtx& c) {
            auto val = [&](const char* k) -> json {
                if (c.values.contains(k)) return c.values[k];
                return c.id ? c.old[util::snake_to_camel(k)] : json(nullptr);
            };
            std::string s = val("start_date"), e = val("end_date");
            if (e < s) bad_request("Ngày kết thúc không được trước ngày bắt đầu");
            if (c.values.contains("tenant_id")) require_tenant_in_scope(c.app, c.actor, c.values["tenant_id"]);
            if (c.values.contains("payment_day") && !c.values["payment_day"].is_null()) {
                long long d = c.values["payment_day"];
                if (d < 1 || d > 31) bad_request("Ngày thanh toán hằng tháng phải từ 1 đến 31");
            } else if (c.values.contains("payment_day")) {
                c.values.erase("payment_day");
            }
            if (c.values.contains("notice_days") && c.values["notice_days"].is_null()) c.values.erase("notice_days");
            if (c.values.contains("late_fee_percent")) {
                if (c.values["late_fee_percent"].is_null()) c.values.erase("late_fee_percent");
                else if (c.values["late_fee_percent"].get<double>() < 0 || c.values["late_fee_percent"].get<double>() > 100) bad_request("Phạt trả chậm phải từ 0 đến 100 (%)");
            }
            if (c.values.contains("payment_method") && c.values["payment_method"].is_null()) c.values.erase("payment_method");
            if (c.creating && !c.values.contains("monthly_rent")) {
                json room = c.app.db.query_one("SELECT base_rent_price FROM rooms WHERE room_id = ?", {val("room_id")});
                if (room.is_null()) bad_request("Phòng không tồn tại");
                c.values["monthly_rent"] = room["baseRentPrice"];
            }
        };
        r.after = [](App& app, long long id) {
            app.db.execute(
                "UPDATE contracts SET contract_code = COALESCE(contract_code, 'HD-' || strftime('%Y', start_date) || '-' || printf('%04d', contract_id)),"
                " sign_date = COALESCE(sign_date, start_date),"
                " electric_price = COALESCE(electric_price, (SELECT unit_price FROM services WHERE calc_type = 'METER_ELECTRIC' AND is_active = 1 ORDER BY service_id LIMIT 1)),"
                " water_price = COALESCE(water_price, (SELECT unit_price FROM services WHERE calc_type = 'METER_WATER' AND is_active = 1 ORDER BY service_id LIMIT 1))"
                " WHERE contract_id = ?", {id});
            // Người thuê chưa gán phòng -> gán vào phòng của hợp đồng (để nhóm theo tòa nhà / phòng)
            app.db.execute("UPDATE tenants SET room_id = (SELECT room_id FROM contracts WHERE contract_id = ?) "
                           "WHERE room_id IS NULL AND tenant_id = (SELECT tenant_id FROM contracts WHERE contract_id = ?)", {id, id});
        };
        rs.push_back(r);
    }

    return rs;
}

json build_contract_document(App& app, const User& u, long long id) {
    json c = app.db.query_one("SELECT * FROM contracts WHERE contract_id = ?", {id});
    if (c.is_null()) not_found("Không tìm thấy hợp đồng");
    json room = app.db.query_one(
        "SELECT r.room_id, r.room_number, r.title, r.area_m2, r.max_occupants, r.base_rent_price, t.type_name, r.building_id FROM rooms r "
        "JOIN room_types t ON t.type_id = r.type_id WHERE r.room_id = ?", {c["roomId"]});
    long long building_id = room["buildingId"];
    if (!owns_building(app, u, building_id)) forbidden("Hợp đồng này thuộc tòa nhà không do bạn quản lý");

    json b = app.db.query_one(
        "SELECT b.building_id, b.name, b.address, b.contact_name, b.contact_phone, b.manager_id, p.name AS province_name, p.adm_system, d.name AS district_name, w.name AS ward_name "
        "FROM buildings b JOIN provinces p ON p.province_id = b.province_id LEFT JOIN districts d ON d.district_id = b.district_id "
        "LEFT JOIN wards w ON w.ward_id = b.ward_id WHERE b.building_id = ?", {building_id});
    std::string addr = b["address"];
    for (const char* k : {"wardName", "districtName", "provinceName"})
        if (!b[k].is_null()) addr += ", " + b[k].get<std::string>();
    b["fullAddress"] = addr;

    json landlord = app.db.query_one(
        "SELECT full_name, phone, email, id_number, birth_date, id_issue_date, id_issue_place, address, bank_name, bank_account FROM users WHERE user_id = ?", {b["managerId"]});
    json tenant = app.db.query_one("SELECT * FROM tenants WHERE tenant_id = ?", {c["tenantId"]});
    json occupants = app.db.query(
        "SELECT full_name, phone, id_number, birth_date, gender, permanent_address FROM tenants WHERE room_id = ? AND tenant_id <> ? ORDER BY full_name", {c["roomId"], c["tenantId"]});
    json vehicles = app.db.query(
        "SELECT v.license_plate, v.vehicle_type, v.brand_color, tn.full_name AS owner_name FROM vehicles v JOIN tenants tn ON tn.tenant_id = v.tenant_id "
        "WHERE tn.tenant_id = ? OR tn.room_id = ? ORDER BY v.vehicle_id", {c["tenantId"], c["roomId"]});
    json services = app.db.query("SELECT service_name, calc_type, unit, unit_price FROM services WHERE is_active = 1 AND calc_type <> 'RENT' ORDER BY service_id");

    json assets = json::array();
    if (!c["assetsText"].is_null()) {
        std::stringstream ss(c["assetsText"].get<std::string>());
        std::string line;
        while (std::getline(ss, line)) {
            line = util::trim(line);
            if (line.empty()) continue;
            std::vector<std::string> parts;
            std::stringstream ls(line);
            std::string part;
            while (std::getline(ls, part, '|')) parts.push_back(util::trim(part));
            assets.push_back({{"name", parts.size() > 0 ? parts[0] : ""}, {"quantity", parts.size() > 1 ? parts[1] : ""}, {"condition", parts.size() > 2 ? parts[2] : ""}});
        }
    }
    json missing = json::array();
    if (landlord["idNumber"].is_null()) missing.push_back("Số CCCD của bên cho thuê (Tài khoản > Hồ sơ)");
    if (landlord["address"].is_null()) missing.push_back("Địa chỉ thường trú của bên cho thuê");
    if (tenant["idNumber"].is_null()) missing.push_back("Số CCCD/CMND của bên thuê");
    if (tenant["permanentAddress"].is_null()) missing.push_back("Địa chỉ thường trú của bên thuê");
    return json{{"contract", c}, {"room", room}, {"building", b}, {"landlord", landlord}, {"tenant", tenant}, {"occupants", occupants},
                {"vehicles", vehicles}, {"services", services}, {"assets", assets}, {"missingInfo", missing}};
}


void register_m3_routes(httplib::Server& svr, App& app) {
    // ---- Tài liệu hợp đồng (để in / lưu PDF)
    add_route(svr, "GET", "/api/admin/contracts/:id/document", [&app](const Req& req, Res&) {
        User u = require_staff(app, req);
        return build_contract_document(app, u, path_id(req));
    });

}

}  // namespace adm
