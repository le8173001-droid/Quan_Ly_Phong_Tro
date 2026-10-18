# Phân chia mã nguồn theo 5 thành viên (theo Biên bản cuộc họp – Nhóm 7)

Dự án có **5 thư mục thành viên** ở thư mục gốc, mỗi thư mục có `backend/` (C++) và `web/` (HTML, CSS, JS) của phần việc đó:

## `thanh-vien-1-tai-khoan-toa-nha/` – Thành viên 1
Xác thực, Phân quyền, Tài khoản & Tòa nhà (ROLES, USERS, BUILDINGS) + khuyến mãi, hotline/Zalo, địa điểm, tiện nghi

- `backend/m1_admin.cpp`
- `backend/m1_auth.cpp`
- `web/admin/modules/m1.js`
- `web/js/login.js`
- `web/js/profile.js`
- `web/login.html`
- `web/profile.html`

## `thanh-vien-2-loai-phong-phong/` – Thành viên 2
Loại phòng & Phòng (ROOM_TYPES, ROOMS), trang chủ, tìm phòng, chi tiết phòng, phòng nổi bật

- `backend/m2_admin.cpp`
- `backend/m2_rooms_api.cpp`
- `backend/m2_rooms_api.hpp`
- `web/admin/modules/m2.js`
- `web/index.html`
- `web/js/home.js`
- `web/js/room.js`
- `web/js/search.js`
- `web/js/searchbox.js`
- `web/room.html`
- `web/search.html`

## `thanh-vien-3-nguoi-thue-hop-dong/` – Thành viên 3
Người thuê, Xe cộ & Hợp đồng (TENANTS, VEHICLES, CONTRACTS), hợp đồng in PDF, trang chủ nhà

- `backend/m3_admin.cpp`
- `web/admin/contract.css`
- `web/admin/contract.html`
- `web/admin/contract.js`
- `web/admin/modules/m3.js`
- `web/js/landlord.js`
- `web/landlord.html`

## `thanh-vien-4-dien-nuoc-hoa-don/` – Thành viên 4
Dịch vụ, Chỉ số điện nước, Hóa đơn & Thanh toán (SERVICES, METER_READINGS, INVOICES, INVOICE_DETAILS, PAYMENTS)

- `backend/m4_admin.cpp`
- `web/admin/modules/m4.js`
- `web/invoice.html`
- `web/js/invoice.js`

## `thanh-vien-5-cong-khach-ticket-dashboard/` – Thành viên 5
Tenant Portal, Ticket sự cố & Dashboard (TICKETS, yêu cầu từ khách, thống kê)

- `backend/m5_admin.cpp`
- `web/admin/modules/m5.js`
- `web/broker.html`
- `web/favorites.html`
- `web/js/broker.js`
- `web/js/favorites.js`
- `web/js/promotions.js`
- `web/js/support.js`
- `web/promotions.html`
- `web/support.html`

## Phần dùng chung (cả nhóm)
- `src/` – lõi: `main.cpp`, `app.*`, `db.*`, `util.*`, `crypto.*`, `public.cpp`, `locations.cpp`, `uploads.cpp`; `src/admin/` – bộ máy CRUD quản trị.
- `public/` – giao diện dùng chung: `css/`, `js/{api,ui,layout,forms,location}.js`, `img/`, `admin/{index.html,admin.js,config.js,admin.css}`.
- `database/` – schema + dữ liệu mẫu; `tests/` – kiểm thử.

Máy chủ tự gộp `public/` và 5 thư mục `thanh-vien-*/web` thành một website (URL không đổi), nên mở `http://localhost:8080` như bình thường.

## Thêm chức năng mới
1. Backend: thêm vào `resources_mN()` / `register_mN_routes()` trong `thanh-vien-N-*/backend/mN_admin.cpp`.
2. Giao diện quản trị: thêm dòng vào mảng `resources` trong `thanh-vien-N-*/web/admin/modules/mN.js`.
3. File `.cpp` mới → thêm vào `CMakeLists.txt`. Thư mục `thanh-vien-*` mới có `web/` sẽ được máy chủ tự nhận.

## Đối chiếu với biên bản (hiện trạng)
- Tự cộng tiền hóa đơn (`trg_TuDongCongTienHoaDon`): có dưới dạng `recalc_invoice()` trong `m4_admin.cpp`; gạch nợ theo thanh toán: có.
- Chưa làm: `trg_TuDongCapNhatTrangThaiPhong` (tự đổi trạng thái phòng theo hợp đồng), Audit Logs, Đổi phòng, Gia hạn/Thanh lý hợp đồng + hoàn cọc, cảnh báo hợp đồng sắp hết hạn 30 ngày.
