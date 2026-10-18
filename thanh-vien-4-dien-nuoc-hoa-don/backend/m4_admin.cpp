// THÀNH VIÊN 4 – Dịch vụ, Chỉ số điện nước, Hóa đơn & Thanh toán (SERVICES, METER_READINGS, INVOICES, INVOICE_DETAILS, PAYMENTS).
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

std::vector<Resource> resources_m4() {
    std::vector<Resource> rs;

    // ---- meter_readings
    {
        Resource r;
        r.path = "meter-readings"; r.table = "meter_readings"; r.pk = "reading_id";
        r.scope = kRoomBuilding;
        r.cols = {{"room_id", T::Int, true}, {"billing_month", T::Month, true}, {"old_electric", T::Int}, {"new_electric", T::Int},
                  {"old_water", T::Int}, {"new_water", T::Int}};
        r.select = "SELECT t.*, rm.room_number, b.building_id, b.name AS building_name FROM meter_readings t"
                   " JOIN rooms rm ON rm.room_id = t.room_id JOIN buildings b ON b.building_id = rm.building_id";
        r.search = {"rm.room_number", "b.name"};
        r.filters = {"b.building_id", "room_id", "billing_month"};
        r.order = "b.name, rm.room_number, t.billing_month DESC, t.reading_id DESC";
        r.before = [](HookCtx& c) {
            // Tạo mới mà không nhập số cũ: lấy số mới của tháng liền trước làm số cũ
            if (c.creating) {
                json prev = c.app.db.query_one(
                    "SELECT new_electric, new_water FROM meter_readings WHERE room_id = ? AND billing_month < ? ORDER BY billing_month DESC LIMIT 1",
                    {c.values["room_id"], c.values["billing_month"]});
                if (!c.values.contains("old_electric")) c.values["old_electric"] = prev.is_null() ? json(0) : prev["newElectric"];
                if (!c.values.contains("old_water")) c.values["old_water"] = prev.is_null() ? json(0) : prev["newWater"];
                for (const char* k : {"new_electric", "new_water"})
                    if (!c.values.contains(k)) c.values[k] = c.values[std::string(k) == "new_electric" ? "old_electric" : "old_water"];
            }
            auto val = [&](const char* k) -> long long {
                json v = c.values.contains(k) ? c.values[k] : (c.id ? c.old[util::snake_to_camel(k)] : json(0));
                return v.is_null() ? 0 : v.get<long long>();
            };
            for (const char* k : {"old_electric", "new_electric", "old_water", "new_water"})
                if (val(k) < 0) bad_request("Chỉ số điện nước không được âm");
            if (val("new_electric") < val("old_electric")) bad_request("Số điện mới không được nhỏ hơn số cũ");
            if (val("new_water") < val("old_water")) bad_request("Số nước mới không được nhỏ hơn số cũ");
        };
        rs.push_back(r);
    }


    // ---- services (bảng giá dịch vụ chung: chủ tòa chỉ xem)
    {
        Resource r;
        r.path = "services"; r.table = "services"; r.pk = "service_id"; r.manager_ro = true;
        r.cols = {{"service_name", T::Str, true, {}, 100},
                  {"calc_type", T::Str, false, {"RENT", "METER_ELECTRIC", "METER_WATER", "PER_VEHICLE", "FIXED"}},
                  {"unit", T::Str, false, {}, 30}, {"unit_price", T::Money}, {"is_active", T::Bool}};
        r.select = "SELECT t.* FROM services t";
        r.search = {"t.service_name"};
        r.filters = {"calc_type", "is_active"};
        r.order = "t.service_id";
        rs.push_back(r);
    }


    // ---- invoices
    {
        Resource r;
        r.path = "invoices"; r.table = "invoices"; r.pk = "invoice_id";
        r.scope = kInvoiceBuilding;
        r.cols = {{"contract_id", T::Int, true}, {"billing_month", T::Month, true}, {"total_amount", T::Money},
                  {"status", T::Str, false, {"unpaid", "partial", "paid", "cancelled"}}, {"due_date", T::Date, true},
                  {"invoice_code", T::Str, false, {}, 30}, {"note", T::Str, false, {}, 500}};
        r.select = "SELECT t.*, c.room_id, rm.room_number, b.building_id, b.name AS building_name, tn.full_name AS tenant_name, tn.phone AS tenant_phone,"
                   " COALESCE((SELECT SUM(p.amount_paid) FROM payments p WHERE p.invoice_id = t.invoice_id), 0) AS paid_amount"
                   " FROM invoices t JOIN contracts c ON c.contract_id = t.contract_id JOIN rooms rm ON rm.room_id = c.room_id"
                   " JOIN buildings b ON b.building_id = rm.building_id JOIN tenants tn ON tn.tenant_id = c.tenant_id";
        r.search = {"t.invoice_code", "rm.room_number", "tn.full_name", "tn.phone", "b.name"};
        r.filters = {"b.building_id", "contract_id", "status", "billing_month"};
        r.order = "b.name, rm.room_number, t.billing_month DESC, t.invoice_id DESC";
        r.before = [](HookCtx& c) {
            if (c.creating && !c.values.contains("invoice_code")) {
                std::string ym = c.values["billing_month"];
                c.values["invoice_code"] = "INV-" + ym.substr(0, 4) + ym.substr(5, 2) + "-" + util::pad_int(c.values["contract_id"].get<long long>(), 4);
            }
            if (c.creating && !c.values.contains("status")) c.values["status"] = "unpaid";
        };
        r.after = [](App& app, long long id) { recalc_invoice(app, id, false); };
        r.decorate = [](App& app, json& row) {
            long long id = row["invoiceId"];
            row["details"] = app.db.query(
                "SELECT d.detail_id, d.service_id, s.service_name, d.description, d.quantity, d.unit_price, d.subtotal FROM invoice_details d "
                "JOIN services s ON s.service_id = d.service_id WHERE d.invoice_id = ? ORDER BY d.detail_id", {id});
            row["payments"] = app.db.query("SELECT payment_id, amount_paid, paid_at, method, note FROM payments WHERE invoice_id = ? ORDER BY paid_at", {id});
            row["remaining"] = std::max<long long>(0, row["totalAmount"].get<long long>() - row["paidAmount"].get<long long>());
        };
        rs.push_back(r);
    }


    // ---- invoice_details
    {
        Resource r;
        r.path = "invoice-details"; r.table = "invoice_details"; r.pk = "detail_id";
        r.scope = kPayBuilding;
        r.cols = {{"invoice_id", T::Int, true}, {"service_id", T::Int, true}, {"description", T::Str, false, {}, 200}, {"quantity", T::Real},
                  {"unit_price", T::Money}, {"subtotal", T::Money}};
        r.select = "SELECT t.*, s.service_name FROM invoice_details t JOIN services s ON s.service_id = t.service_id";
        r.filters = {"invoice_id"};
        r.order = "t.detail_id";
        r.before = [](HookCtx& c) {
            if (c.creating) {
                if (!c.values.contains("quantity")) c.values["quantity"] = 1;
                if (!c.values.contains("unit_price")) {
                    json s = c.app.db.query_one("SELECT unit_price FROM services WHERE service_id = ?", {c.values["service_id"]});
                    if (s.is_null()) bad_request("Dịch vụ không tồn tại");
                    c.values["unit_price"] = s["unitPrice"];
                }
            }
            auto val = [&](const char* k) -> json {
                if (c.values.contains(k)) return c.values[k];
                return c.id ? c.old[util::snake_to_camel(k)] : json(nullptr);
            };
            double q = val("quantity").is_null() ? 1 : val("quantity").get<double>();
            if (q < 0) bad_request("Số lượng không được âm");
            if (!c.values.contains("subtotal") || c.values["subtotal"].is_null())
                c.values["subtotal"] = static_cast<long long>(std::llround(q * val("unit_price").get<double>()));
            if (c.values.contains("invoice_id") && c.app.db.scalar("SELECT COUNT(*) FROM invoices WHERE invoice_id = ?", {c.values["invoice_id"]}) == 0)
                bad_request("Hóa đơn không tồn tại");
        };
        r.after = [](App& app, long long id) {
            json d = app.db.query_one("SELECT invoice_id FROM invoice_details WHERE detail_id = ?", {id});
            if (!d.is_null()) recalc_invoice(app, d["invoiceId"], true);
        };
        r.after_delete = [](App& app, const json& old_row) { recalc_invoice(app, old_row["invoiceId"], true); };
        rs.push_back(r);
    }


    // ---- payments
    {
        Resource r;
        r.path = "payments"; r.table = "payments"; r.pk = "payment_id";
        r.scope = kPayBuilding;
        r.cols = {{"invoice_id", T::Int, true}, {"amount_paid", T::Money, true}, {"paid_at", T::DateTime},
                  {"method", T::Str, false, {"cash", "bank_transfer", "momo", "other"}}, {"note", T::Str, false, {}, 255}};
        r.select = "SELECT t.*, i.invoice_code, i.billing_month, rm.room_number, b.building_id, b.name AS building_name FROM payments t JOIN invoices i ON i.invoice_id = t.invoice_id"
                   " JOIN contracts c ON c.contract_id = i.contract_id JOIN rooms rm ON rm.room_id = c.room_id JOIN buildings b ON b.building_id = rm.building_id";
        r.search = {"i.invoice_code", "rm.room_number", "b.name"};
        r.filters = {"b.building_id", "invoice_id", "method"};
        r.order = "t.paid_at DESC, t.payment_id DESC";
        r.before = [](HookCtx& c) {
            long long inv_id = c.values.contains("invoice_id") ? c.values["invoice_id"].get<long long>() : c.old["invoiceId"].get<long long>();
            json inv = c.app.db.query_one("SELECT total_amount, status FROM invoices WHERE invoice_id = ?", {inv_id});
            if (inv.is_null()) bad_request("Hóa đơn không tồn tại");
            if (inv["status"] == "cancelled") bad_request("Hóa đơn đã bị hủy, không thể ghi nhận thanh toán");
            long long amount = c.values.contains("amount_paid") ? c.values["amount_paid"].get<long long>() : c.old["amountPaid"].get<long long>();
            if (amount <= 0) bad_request("Số tiền thanh toán phải lớn hơn 0");
            long long others = c.app.db.scalar("SELECT COALESCE(SUM(amount_paid),0) FROM payments WHERE invoice_id = ? AND payment_id <> ?", {inv_id, c.id});
            if (others + amount > inv["totalAmount"].get<long long>())
                bad_request("Số tiền vượt quá số còn nợ (" + std::to_string(inv["totalAmount"].get<long long>() - others) + " đ)");
            if (c.creating && !c.values.contains("paid_at")) c.values["paid_at"] = now_text(c.app);
        };
        r.after = [](App& app, long long id) {
            json p = app.db.query_one("SELECT invoice_id FROM payments WHERE payment_id = ?", {id});
            if (!p.is_null()) recalc_invoice(app, p["invoiceId"], false);
        };
        r.after_delete = [](App& app, const json& old_row) { recalc_invoice(app, old_row["invoiceId"], false); };
        rs.push_back(r);
    }

    return rs;
}

// ------------------------------------------------------------------ tính lại hóa đơn
void recalc_invoice(App& app, long long invoice_id, bool recompute_total) {
    json inv = app.db.query_one("SELECT total_amount, status FROM invoices WHERE invoice_id = ?", {invoice_id});
    if (inv.is_null()) return;
    long long total = inv["totalAmount"];
    if (recompute_total) {
        total = app.db.scalar("SELECT COALESCE(SUM(subtotal),0) FROM invoice_details WHERE invoice_id = ?", {invoice_id});
        app.db.execute("UPDATE invoices SET total_amount = ? WHERE invoice_id = ?", {total, invoice_id});
    }
    if (inv["status"] == "cancelled") return;
    long long paid = app.db.scalar("SELECT COALESCE(SUM(amount_paid),0) FROM payments WHERE invoice_id = ?", {invoice_id});
    std::string st = (total > 0 && paid >= total) ? "paid" : (paid > 0 ? "partial" : "unpaid");
    app.db.execute("UPDATE invoices SET status = ? WHERE invoice_id = ?", {st, invoice_id});
}


// ------------------------------------------------------------------ tạo hóa đơn tự động
std::string default_due_date(const std::string& ym) {
    int y = std::stoi(ym.substr(0, 4)), m = std::stoi(ym.substr(5, 2));
    if (++m > 12) { m = 1; ++y; }
    return std::to_string(y) + "-" + util::pad_int(m, 2) + "-05";
}

json generate_invoice(App& app, long long contract_id, const std::string& month, std::string due_date) {
    if (!util::valid_month(month)) bad_request("billingMonth phải có dạng YYYY-MM");
    if (due_date.empty()) due_date = default_due_date(month);
    if (!util::valid_date(due_date)) bad_request("dueDate phải có dạng YYYY-MM-DD");

    json warnings = json::array();
    long long invoice_id = app.db.tx([&]() -> long long {
        json c = app.db.query_one("SELECT contract_id, room_id, tenant_id, monthly_rent, status FROM contracts WHERE contract_id = ?", {contract_id});
        if (c.is_null()) not_found("Không tìm thấy hợp đồng");
        if (c["status"] == "cancelled" || c["status"] == "pending") bad_request("Chỉ tạo hóa đơn cho hợp đồng đang hiệu lực hoặc đã kết thúc");
        if (app.db.scalar("SELECT COUNT(*) FROM invoices WHERE contract_id = ? AND billing_month = ?", {contract_id, month}) > 0)
            conflict("Hợp đồng này đã có hóa đơn tháng " + month_label(month));

        json reading = app.db.query_one("SELECT * FROM meter_readings WHERE room_id = ? AND billing_month = ?", {c["roomId"], month});
        long long vehicles = app.db.scalar("SELECT COUNT(*) FROM vehicles WHERE tenant_id = ?", {c["tenantId"]});

        struct Line { long long service_id; std::string desc; double qty; long long price; };
        std::vector<Line> lines;
        for (const auto& s : app.db.query("SELECT * FROM services WHERE is_active = 1 ORDER BY service_id")) {
            std::string type = s["calcType"];
            long long sid = s["serviceId"];
            std::string name = s["serviceName"];
            long long unit_price = s["unitPrice"];
            if (type == "RENT") {
                lines.push_back({sid, name + " " + month_label(month), 1, c["monthlyRent"]});
            } else if (type == "METER_ELECTRIC" || type == "METER_WATER") {
                if (reading.is_null()) {
                    if (type == "METER_ELECTRIC") warnings.push_back("Chưa có chỉ số điện nước tháng " + month_label(month) + " nên chưa tính tiền điện, nước");
                    continue;
                }
                bool elec = type == "METER_ELECTRIC";
                long long o = reading[elec ? "oldElectric" : "oldWater"], n = reading[elec ? "newElectric" : "newWater"];
                if (n - o <= 0) continue;
                lines.push_back({sid, name + " " + month_label(month) + " (" + std::to_string(o) + " → " + std::to_string(n) + ")",
                                 static_cast<double>(n - o), unit_price});
            } else if (type == "PER_VEHICLE") {
                if (vehicles == 0) continue;
                lines.push_back({sid, name + " (" + std::to_string(vehicles) + " xe)", static_cast<double>(vehicles), unit_price});
            } else {
                lines.push_back({sid, name, 1, unit_price});
            }
        }
        long long total = 0;
        for (const auto& l : lines) total += static_cast<long long>(std::llround(l.qty * l.price));

        std::string code = "INV-" + month.substr(0, 4) + month.substr(5, 2) + "-" + util::pad_int(contract_id, 4);
        app.db.execute("INSERT INTO invoices (contract_id, billing_month, total_amount, status, due_date, invoice_code) VALUES (?,?,?,?,?,?)",
                       {contract_id, month, total, "unpaid", due_date, code});
        long long id = app.db.last_insert_id();
        for (const auto& l : lines) {
            app.db.execute("INSERT INTO invoice_details (invoice_id, service_id, description, quantity, unit_price, subtotal) VALUES (?,?,?,?,?,?)",
                           {id, l.service_id, l.desc, l.qty, l.price, static_cast<long long>(std::llround(l.qty * l.price))});
        }
        return id;
    });
    return json{{"invoiceId", invoice_id}, {"warnings", warnings}};
}


void register_m4_routes(httplib::Server& svr, App& app) {
    // ---- Tạo hóa đơn tự động từ hợp đồng + chỉ số điện nước + bảng dịch vụ
    add_route(svr, "POST", "/api/admin/invoices/generate", [&app](const Req& req, Res& res) {
        User u = require_staff(app, req);
        json b = parse_body(req);
        auto contract_id = util::get_int(b, "contractId");
        if (!contract_id) bad_request("Thiếu trường bắt buộc: contractId");
        if (!u.is_admin() && app.db.scalar("SELECT COUNT(*) FROM contracts c JOIN rooms r ON r.room_id = c.room_id JOIN buildings b ON b.building_id = r.building_id "
                                           "WHERE c.contract_id = ? AND b.manager_id = ?", {*contract_id, u.id}) == 0)
            forbidden("Hợp đồng này thuộc tòa nhà không do bạn quản lý");
        json r = generate_invoice(app, *contract_id, util::get_string(b, "billingMonth", true, 7), util::get_string(b, "dueDate", false, 10));
        res.status = 201;
        return r;
    });


    // ---- Hủy hóa đơn
    add_route(svr, "POST", "/api/admin/invoices/:id/cancel", [&app](const Req& req, Res&) {
        User u = require_staff(app, req);
        long long id = path_id(req);
        if (!u.is_admin() && app.db.scalar("SELECT COUNT(*) FROM invoices t WHERE t.invoice_id = ? AND (" + std::string(kInvoiceBuilding) +
                                           ") IN (SELECT building_id FROM buildings WHERE manager_id = ?)", {id, u.id}) == 0)
            forbidden("Hóa đơn này thuộc tòa nhà không do bạn quản lý");
        if (app.db.scalar("SELECT COUNT(*) FROM payments WHERE invoice_id = ?", {id}) > 0)
            conflict("Hóa đơn đã có thanh toán nên không thể hủy. Hãy xóa các khoản thanh toán trước.");
        if (app.db.execute("UPDATE invoices SET status = 'cancelled' WHERE invoice_id = ?", {id}) == 0) not_found();
        return json{{"message", "Đã hủy hóa đơn"}};
    });

}

}  // namespace adm
