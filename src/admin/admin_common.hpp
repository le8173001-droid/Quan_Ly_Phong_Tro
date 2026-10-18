// Khai báo dùng chung cho khu quản trị (/api/admin/*): kiểu Resource + hàm của 5 mô-đun thành viên.
#pragma once
#include "app.hpp"

#include <functional>
#include <string>
#include <vector>

namespace adm {

enum class T { Int, Str, Real, Money, Bool, Date, Month, DateTime };

struct Col {
    std::string name;  // tên cột snake_case; body JSON dùng camelCase
    T type;
    bool required;
    std::vector<std::string> enums;
    size_t max_len;
    Col(std::string n, T t = T::Str, bool req = false, std::vector<std::string> e = {}, size_t m = 500)
        : name(std::move(n)), type(t), required(req), enums(std::move(e)), max_len(m) {}
};

struct HookCtx {
    App& app;
    const json& body;
    json& values;       // giá trị sẽ ghi (khóa snake_case); hook được phép sửa/thêm
    bool creating;
    long long id;       // 0 khi tạo mới
    const json& old;    // bản ghi hiện tại (null khi tạo mới), khóa camelCase
    const User& actor;
};

struct Resource {
    std::string path, table, pk;
    std::vector<Col> cols;
    std::string select;                    // SELECT ... FROM <table> t JOIN ... (không có WHERE)
    std::vector<std::string> search;       // biểu thức SQL dùng cho ô tìm kiếm ?q=
    std::vector<std::string> filters;      // cột cho phép lọc bằng ?camelCase=giá trị
    std::string order;
    bool admin_only = false;               // chỉ ADMIN
    bool manager_ro = false;               // MANAGER (chủ tòa nhà) chỉ được xem, chỉ ADMIN được thêm / sửa / xóa
    std::string base_cond;                 // điều kiện cố định thêm vào WHERE khi liệt kê (vd: chỉ người dùng vai trò MANAGER)
    bool read_only = false;
    std::string scope;                     // biểu thức SQL (alias t) trả về building_id của bản ghi; MANAGER chỉ thao tác tòa nhà của mình
    std::function<void(HookCtx&)> before;
    std::function<void(App&, long long id)> after;                // sau khi thêm / sửa
    std::function<void(App&, long long id, const json& body, const User&)> after_body;  // như after nhưng có body gửi lên
    std::function<void(App&, const json& old_row, const User&)> before_delete;
    std::function<void(App&, const json& old_row)> after_delete;
    std::function<void(App&, json& row)> decorate;                // bổ sung dữ liệu khi xem 1 bản ghi
};

// Tòa nhà của người thuê: theo phòng đang ở, nếu chưa gán phòng thì theo hợp đồng gần nhất
inline const char* kTenantBuilding =
    "COALESCE((SELECT building_id FROM rooms WHERE room_id = t.room_id),"
    " (SELECT rm.building_id FROM contracts c JOIN rooms rm ON rm.room_id = c.room_id WHERE c.tenant_id = t.tenant_id ORDER BY c.contract_id DESC LIMIT 1))";
inline const char* kVehicleBuilding =
    "(SELECT COALESCE((SELECT building_id FROM rooms WHERE room_id = tn.room_id),"
    " (SELECT rm.building_id FROM contracts c JOIN rooms rm ON rm.room_id = c.room_id WHERE c.tenant_id = tn.tenant_id ORDER BY c.contract_id DESC LIMIT 1))"
    " FROM tenants tn WHERE tn.tenant_id = t.tenant_id)";
inline const char* kRoomBuilding = "(SELECT building_id FROM rooms WHERE room_id = t.room_id)";
inline const char* kInvoiceBuilding = "(SELECT rm.building_id FROM contracts c JOIN rooms rm ON rm.room_id = c.room_id WHERE c.contract_id = t.contract_id)";
inline const char* kPayBuilding = "(SELECT rm.building_id FROM invoices i JOIN contracts c ON c.contract_id = i.contract_id JOIN rooms rm ON rm.room_id = c.room_id WHERE i.invoice_id = t.invoice_id)";

// ---- hàm dùng chung (admin_engine.cpp)
std::string now_text(App& app);
std::string month_label(const std::string& ym);
json fetch_one(App& app, const Resource& res, long long id);
json get_one(App& app, const Resource& res, long long id);
bool in_scope(App& app, const Resource& r, const User& u, long long id);
void require_scope(App& app, const Resource& r, const User& u, long long id);
void require_tenant_in_scope(App& app, const User& u, long long tenant_id);
void check_image_url(const std::string& url);
void register_resource(httplib::Server& svr, App& app, const Resource& r);

// ---- hàm của mô-đun Thành viên 4 mà mô-đun khác cần gọi
void recalc_invoice(App& app, long long invoice_id, bool recompute_total);

// ---- 5 mô-đun theo phân công (xem MODULES.md)
std::vector<Resource> resources_m1();  // ROLES, USERS, BUILDINGS (+ owners, khuyến mãi, địa điểm, tiện nghi)
std::vector<Resource> resources_m2();  // ROOM_TYPES
std::vector<Resource> resources_m3();  // TENANTS, VEHICLES, CONTRACTS
std::vector<Resource> resources_m4();  // SERVICES, METER_READINGS, INVOICES, INVOICE_DETAILS, PAYMENTS
std::vector<Resource> resources_m5();  // TICKETS (+ yêu cầu từ khách)
void register_m1_routes(httplib::Server& svr, App& app);
void register_m2_routes(httplib::Server& svr, App& app);
void register_m3_routes(httplib::Server& svr, App& app);
void register_m4_routes(httplib::Server& svr, App& app);
void register_m5_routes(httplib::Server& svr, App& app);

}  // namespace adm
