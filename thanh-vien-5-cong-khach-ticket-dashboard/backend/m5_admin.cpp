// THÀNH VIÊN 5 – Tenant Portal, Ticket sự cố & Dashboard báo cáo (TICKETS + yêu cầu từ khách + thống kê).
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

std::vector<Resource> resources_m5() {
    std::vector<Resource> rs;

    // ---- tickets: sự cố / yêu cầu theo tòa nhà -> phòng
    {
        Resource r;
        r.path = "tickets"; r.table = "tickets"; r.pk = "ticket_id";
        r.scope = kRoomBuilding;
        r.cols = {{"room_id", T::Int, true}, {"tenant_id", T::Int, true},
                  {"category", T::Str, true, {"hygiene", "maintenance", "electric_water", "security", "other"}},
                  {"status", T::Str, false, {"open", "in_progress", "done", "cancelled"}}, {"title", T::Str, true, {}, 200},
                  {"description", T::Str, false, {}, 1000}};
        r.select = "SELECT t.*, rm.room_number, b.building_id, b.name AS building_name, tn.full_name AS tenant_name FROM tickets t"
                   " JOIN rooms rm ON rm.room_id = t.room_id JOIN buildings b ON b.building_id = rm.building_id JOIN tenants tn ON tn.tenant_id = t.tenant_id";
        r.search = {"t.title", "rm.room_number", "tn.full_name", "b.name"};
        r.filters = {"b.building_id", "room_id", "tenant_id", "status", "category"};
        r.order = "b.name, rm.room_number, t.ticket_id DESC";
        r.before = [](HookCtx& c) {
            if (c.values.contains("tenant_id")) require_tenant_in_scope(c.app, c.actor, c.values["tenant_id"]);
            if (!c.values.contains("status")) return;
            if (c.values["status"] == "done") {
                if (c.creating || c.old["resolvedAt"].is_null()) c.values["resolved_at"] = now_text(c.app);
            } else {
                c.values["resolved_at"] = nullptr;
            }
        };
        rs.push_back(r);
    }


    // ---- yêu cầu từ khách
    {
        Resource r;
        r.path = "support-requests"; r.table = "support_requests"; r.pk = "request_id"; r.admin_only = true;
        r.cols = {{"full_name", T::Str, true, {}, 100}, {"phone", T::Str, true, {}, 20}, {"email", T::Str, false, {}, 100},
                  {"request_type", T::Str, false, {"find_room", "landlord", "other"}}, {"desired_area", T::Str, false, {}, 150},
                  {"budget_max", T::Money}, {"note", T::Str, false, {}, 1000}, {"status", T::Str, false, {"new", "processing", "done"}}};
        r.select = "SELECT t.*, u.username FROM support_requests t LEFT JOIN users u ON u.user_id = t.user_id";
        r.search = {"t.full_name", "t.phone", "t.desired_area"};
        r.filters = {"status", "request_type"};
        r.order = "t.request_id DESC";
        rs.push_back(r);
    }
    {
        Resource r;
        r.path = "broker-applications"; r.table = "broker_applications"; r.pk = "application_id"; r.admin_only = true;
        r.cols = {{"full_name", T::Str, true, {}, 100}, {"phone", T::Str, true, {}, 20}, {"email", T::Str, false, {}, 100},
                  {"work_area", T::Str, false, {}, 150}, {"experience", T::Str, false, {}, 1000},
                  {"status", T::Str, false, {"new", "approved", "rejected"}}};
        r.select = "SELECT t.* FROM broker_applications t";
        r.search = {"t.full_name", "t.phone", "t.work_area"};
        r.filters = {"status"};
        r.order = "t.application_id DESC";
        rs.push_back(r);
    }
    {
        Resource r;
        r.path = "viewing-requests"; r.table = "viewing_requests"; r.pk = "viewing_id";
        r.scope = kRoomBuilding;
        r.cols = {{"room_id", T::Int, true}, {"full_name", T::Str, true, {}, 100}, {"phone", T::Str, true, {}, 20}, {"message", T::Str, false, {}, 1000},
                  {"preferred_time", T::Str, false, {}, 100}, {"status", T::Str, false, {"new", "contacted", "done", "cancelled"}}};
        r.select = "SELECT t.*, rm.room_number, rm.title AS room_title, b.building_id, b.name AS building_name FROM viewing_requests t"
                   " JOIN rooms rm ON rm.room_id = t.room_id JOIN buildings b ON b.building_id = rm.building_id";
        r.search = {"t.full_name", "t.phone", "rm.room_number", "b.name"};
        r.filters = {"b.building_id", "status", "room_id"};
        r.order = "b.name, rm.room_number, t.viewing_id DESC";
        rs.push_back(r);
    }

    return rs;
}

// ------------------------------------------------------------------ danh sách chọn nhanh cho form
json options_for(App& app, const User& u, const std::string& name, const std::string& q) {
    const std::string own = u.is_admin() ? "" : " AND b.manager_id = " + std::to_string(u.id);
    std::string pat = "%" + util::like_escape(util::vn_fold(q)) + "%";
    if (name == "rooms")
        return app.db.query(
            "SELECT r.room_id AS id, b.name || ' - ' || r.room_number AS label, r.base_rent_price, b.building_id FROM rooms r JOIN buildings b ON b.building_id = r.building_id "
            "WHERE r.status <> 'hidden'" + own + " AND (vn_fold(r.room_number) LIKE ? ESCAPE '!' OR vn_fold(b.name) LIKE ? ESCAPE '!' OR vn_fold(r.title) LIKE ? ESCAPE '!') "
            "ORDER BY b.name, r.room_number LIMIT 80", {pat, pat, pat});
    if (name == "tenants") {
        std::string scope = u.is_admin() ? "" : " AND (" + std::string(kTenantBuilding) + ") IN (SELECT building_id FROM buildings WHERE manager_id = " + std::to_string(u.id) + ")";
        return app.db.query(
            "SELECT t.tenant_id AS id, COALESCE(b.name || ' - ' || rm.room_number || ' - ', '') || t.full_name || ' - ' || t.phone AS label, t.room_id FROM tenants t "
            "LEFT JOIN rooms rm ON rm.room_id = t.room_id LEFT JOIN buildings b ON b.building_id = rm.building_id "
            "WHERE (vn_fold(t.full_name) LIKE ? ESCAPE '!' OR t.phone LIKE ? ESCAPE '!' OR vn_fold(COALESCE(rm.room_number,'')) LIKE ? ESCAPE '!')" + scope +
            " ORDER BY COALESCE(b.name,'~'), rm.room_number, t.full_name LIMIT 80", {pat, pat, pat});
    }
    if (name == "buildings")
        return app.db.query("SELECT building_id AS id, name AS label FROM buildings b WHERE 1=1" + own + " AND vn_fold(name) LIKE ? ESCAPE '!' ORDER BY name LIMIT 80", {pat});
    if (name == "contracts")
        return app.db.query(
            "SELECT c.contract_id AS id, b.name || ' - ' || r.room_number || ' - ' || tn.full_name || ' (#' || c.contract_id || ')' AS label FROM contracts c "
            "JOIN rooms r ON r.room_id = c.room_id JOIN buildings b ON b.building_id = r.building_id JOIN tenants tn ON tn.tenant_id = c.tenant_id "
            "WHERE 1=1" + own + " AND (vn_fold(r.room_number) LIKE ? ESCAPE '!' OR vn_fold(tn.full_name) LIKE ? ESCAPE '!' OR vn_fold(b.name) LIKE ? ESCAPE '!') ORDER BY b.name, r.room_number, c.contract_id DESC LIMIT 80", {pat, pat, pat});
    if (name == "invoices")
        return app.db.query(
            "SELECT i.invoice_id AS id, b.name || ' - ' || r.room_number || ' - ' || i.invoice_code AS label, i.total_amount FROM invoices i "
            "JOIN contracts c ON c.contract_id = i.contract_id JOIN rooms r ON r.room_id = c.room_id JOIN buildings b ON b.building_id = r.building_id "
            "WHERE i.status <> 'cancelled'" + own + " AND (vn_fold(i.invoice_code) LIKE ? ESCAPE '!' OR vn_fold(r.room_number) LIKE ? ESCAPE '!' OR vn_fold(b.name) LIKE ? ESCAPE '!') ORDER BY i.invoice_id DESC LIMIT 80", {pat, pat, pat});
    if (name == "managers") {
        if (!u.is_admin()) forbidden("Chỉ ADMIN mới xem được danh sách này");
        return app.db.query(
            "SELECT u.user_id AS id, u.full_name || ' (' || ro.role_code || ')' AS label FROM users u JOIN roles ro ON ro.role_id = u.role_id "
            "WHERE ro.role_code IN ('ADMIN','MANAGER') AND u.status = 'active' ORDER BY u.full_name");
    }
    if (name == "services")
        return app.db.query("SELECT service_id AS id, service_name AS label FROM services WHERE is_active = 1 ORDER BY service_id");
    not_found("Không có danh sách '" + name + "'");
}

// Thống kê theo phạm vi: ADMIN = toàn hệ thống, chủ tòa nhà = chỉ các tòa nhà của mình
json build_dashboard(App& app, const User& u) {
    auto& db = app.db;
    const bool admin = u.is_admin();
    const std::string rb = admin ? " WHERE 1=1" : " WHERE b.manager_id = ?";
    std::vector<json> sp;
    if (!admin) sp.push_back(u.id);
    const std::string room_from = " FROM rooms r JOIN buildings b ON b.building_id = r.building_id" + rb;
    auto room_count = [&](const std::string& extra) { return db.scalar("SELECT COUNT(*)" + room_from + extra, sp); };
    const long long almost = app.cfg.almost_empty_percent;

    json out;
    out["scope"] = admin ? "admin" : "manager";
    out["rooms"] = {
        {"total", room_count(" AND r.status <> 'hidden'")},
        {"available", room_count(" AND r.status = 'available'")},
        {"occupied", room_count(" AND r.status = 'occupied'")},
        {"maintenance", room_count(" AND r.status = 'maintenance'")},
        {"hidden", room_count(" AND r.status = 'hidden'")},
        {"empty", room_count(" AND r.status <> 'hidden' AND r.current_occupants = 0")},
        {"almostEmpty", db.scalar("SELECT COUNT(*)" + room_from + " AND r.status <> 'hidden' AND r.current_occupants > 0 AND r.current_occupants * 100 <= r.max_occupants * " + std::to_string(almost), sp)},
        {"full", room_count(" AND r.status <> 'hidden' AND r.current_occupants >= r.max_occupants")},
    };
    out["buildings"] = db.scalar(std::string("SELECT COUNT(*) FROM buildings b") + rb, sp);
    out["tenants"] = db.scalar("SELECT COUNT(*) FROM tenants t JOIN rooms r ON r.room_id = t.room_id JOIN buildings b ON b.building_id = r.building_id" + rb, sp);
    const std::string c_from = " FROM contracts c JOIN rooms r ON r.room_id = c.room_id JOIN buildings b ON b.building_id = r.building_id" + rb;
    out["activeContracts"] = db.scalar("SELECT COUNT(*)" + c_from + " AND c.status = 'active'", sp);
    out["expiringContracts"] = db.query(
        "SELECT c.contract_id, b.name AS building_name, r.room_number, tn.full_name AS tenant_name, c.end_date" + c_from.substr(0, c_from.find(" WHERE")) +
        " JOIN tenants tn ON tn.tenant_id = c.tenant_id" + rb + " AND c.status = 'active' AND c.end_date BETWEEN date('now','localtime') AND date('now','localtime','+30 days') ORDER BY c.end_date LIMIT 10", sp);
    out["openTickets"] = db.scalar("SELECT COUNT(*) FROM tickets k JOIN rooms r ON r.room_id = k.room_id JOIN buildings b ON b.building_id = r.building_id" + rb + " AND k.status IN ('open','in_progress')", sp);
    const std::string i_from = " FROM invoices i JOIN contracts c ON c.contract_id = i.contract_id JOIN rooms r ON r.room_id = c.room_id JOIN buildings b ON b.building_id = r.building_id" + rb;
    out["unpaidInvoices"] = db.scalar("SELECT COUNT(*)" + i_from + " AND i.status IN ('unpaid','partial')", sp);
    out["unpaidAmount"] = db.scalar(
        "SELECT COALESCE(SUM(i.total_amount - COALESCE((SELECT SUM(p.amount_paid) FROM payments p WHERE p.invoice_id = i.invoice_id),0)),0)" + i_from + " AND i.status IN ('unpaid','partial')", sp);
    const std::string p_from = " FROM payments p JOIN invoices i ON i.invoice_id = p.invoice_id JOIN contracts c ON c.contract_id = i.contract_id JOIN rooms r ON r.room_id = c.room_id JOIN buildings b ON b.building_id = r.building_id" + rb;
    out["revenueThisMonth"] = db.scalar("SELECT COALESCE(SUM(p.amount_paid),0)" + p_from + " AND strftime('%Y-%m', p.paid_at) = strftime('%Y-%m','now','localtime')", sp);
    out["revenueByMonth"] = db.query("SELECT strftime('%Y-%m', p.paid_at) AS month, SUM(p.amount_paid) AS amount" + p_from + " GROUP BY month ORDER BY month DESC LIMIT 6", sp);
    out["topViewedRooms"] = db.query(
        "SELECT r.room_id, r.room_number, r.title, b.name AS building_name, r.view_count" + room_from + " AND r.status <> 'hidden' ORDER BY r.view_count DESC, r.room_id LIMIT 5", sp);
    out["byBuilding"] = db.query(
        "SELECT b.building_id, b.name, u.full_name AS manager_name,"
        " (SELECT COUNT(*) FROM rooms r WHERE r.building_id = b.building_id AND r.status <> 'hidden') AS room_count,"
        " (SELECT COUNT(*) FROM rooms r WHERE r.building_id = b.building_id AND r.status <> 'hidden' AND r.current_occupants > 0) AS occupied_rooms,"
        " (SELECT COUNT(*) FROM contracts c JOIN rooms r ON r.room_id = c.room_id WHERE r.building_id = b.building_id AND c.status = 'active') AS active_contracts,"
        " (SELECT COUNT(*) FROM tickets k JOIN rooms r ON r.room_id = k.room_id WHERE r.building_id = b.building_id AND k.status IN ('open','in_progress')) AS open_tickets"
        " FROM buildings b JOIN users u ON u.user_id = b.manager_id" + rb + " ORDER BY room_count DESC, b.name LIMIT 12", sp);
    out["newRequests"] = {
        {"viewing", db.scalar("SELECT COUNT(*) FROM viewing_requests v JOIN rooms r ON r.room_id = v.room_id JOIN buildings b ON b.building_id = r.building_id" + rb + " AND v.status = 'new'", sp)},
    };
    if (admin) {
        out["newRequests"]["support"] = db.scalar("SELECT COUNT(*) FROM support_requests WHERE status = 'new'");
        out["newRequests"]["brokers"] = db.scalar("SELECT COUNT(*) FROM broker_applications WHERE status = 'new'");
        out["owners"] = db.scalar("SELECT COUNT(*) FROM users u JOIN roles ro ON ro.role_id = u.role_id WHERE ro.role_code = 'MANAGER'");
        out["users"] = db.scalar("SELECT COUNT(*) FROM users");
        out["runningPromotions"] = db.scalar("SELECT COUNT(*) FROM promotions WHERE is_active = 1 AND start_at <= datetime('now','localtime') AND end_at > datetime('now','localtime')");
        out["featuredRooms"] = db.scalar("SELECT COUNT(*) FROM rooms WHERE is_featured = 1 AND status <> 'hidden'");
    }
    return out;
}

// Dữ liệu đầy đủ để dựng bản hợp đồng thuê có giá trị pháp lý (in / lưu PDF ở trình duyệt)

void register_m5_routes(httplib::Server& svr, App& app) {
    // ---- Thống kê tổng quan (ADMIN: toàn hệ thống; chủ tòa nhà: chỉ tòa nhà của mình)
    add_route(svr, "GET", "/api/admin/dashboard", [&app](const Req& req, Res&) {
        User u = require_staff(app, req);
        return build_dashboard(app, u);
    });


    // ---- Danh sách chọn nhanh cho form (autocomplete)
    add_route(svr, "GET", "/api/admin/options/:name", [&app](const Req& req, Res&) {
        User u = require_staff(app, req);
        return options_for(app, u, req.path_params.at("name"), param(req, "q"));
    });

}

}  // namespace adm
