# NhàTrọ+ – Web tìm phòng & quản lý nhà trọ (backend C++)

Giao diện giống Lozido: tìm phòng trọ / ký túc xá / nhà / chung cư / văn phòng / phòng pass & ở ghép, kèm trang quản trị (phòng, hợp đồng, điện nước, hóa đơn, thanh toán…).

- **Backend:** C++17 (cpp-httplib + SQLite) – một file chạy duy nhất, đã kèm sẵn thư viện trong `third_party/`.
- **Frontend:** web động, `public/*.html` + `public/css/*.css` + `public/js/*.js` (tách riêng từng file), gọi API bằng `fetch`.
- **Database:** SQLite theo đúng sơ đồ ERD (14 bảng) + các bảng bổ sung cho các chức năng trên giao diện. Tự tạo + nạp dữ liệu mẫu ở lần chạy đầu.

## 1. Cài công cụ (Windows)

1. Cài **VS Code**: https://code.visualstudio.com
2. Cài **MSYS2**: https://www.msys2.org (cài mặc định vào `C:\msys64`).
3. Mở **MSYS2 UCRT64** và chạy:
   ```
   pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-sqlite3 mingw-w64-ucrt-x86_64-gdb
   ```
4. Thêm `C:\msys64\ucrt64\bin` vào biến môi trường **Path** (Windows → "Edit environment variables"), rồi **mở lại VS Code**.
5. Trong VS Code cài các extension được gợi ý (C/C++, CMake Tools, REST Client).

(Linux/macOS: cài `g++`, `cmake`, `ninja`, `libsqlite3-dev`.)

## 2. Chạy trên VS Code

Mở thư mục dự án trong VS Code, rồi bấm **Terminal → Run Task…**:

| Task | Việc làm |
|---|---|
| `Cấu hình (CMake)` | chạy một lần đầu |
| `Build` (Ctrl+Shift+B) | biên dịch |
| `Chạy server` | chạy web tại **http://localhost:8080** |
| `Test: unit test (C++)` / `Test: API (Python)` / `Test: giao diện (Playwright)` | các bộ kiểm thử |

Hoặc gõ ở terminal:
```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\nhatro.exe          (Linux/macOS: ./build/nhatro)
```
Mở http://localhost:8080. Muốn xóa dữ liệu làm lại từ đầu: `nhatro --reset`. Debug: F5 (cấu hình "Debug server").

Tùy chọn: `--port 9000`, `--db đường_dẫn.db`. Dữ liệu lưu ở `data/nhatro.db`.

## 3. Tài khoản mẫu

| Vai trò | Tên đăng nhập | Mật khẩu |
|---|---|---|
| Quản trị (ADMIN) | `admin` | `Admin@123` |
| Quản lý (MANAGER) | `levinh` | `Manager@123` |
| Người thuê (TENANT) | `minhanh`, `daniel` | `Tenant@123` |

Trang quản trị: http://localhost:8080/admin/index.html (ADMIN thấy tất cả; MANAGER chỉ thấy tòa nhà của mình). Thử **Tra cứu hóa đơn** với SĐT `0987654321`, phòng `P102`.

## 4. Đăng nhập bằng Google

Đăng ký / đăng nhập thường luôn dùng được. Để bật nút Google:
1. Vào https://console.cloud.google.com → APIs & Services → Credentials → **Create OAuth client ID** (Web application).
2. Thêm **Authorized JavaScript origins**: `http://localhost:8080`.
3. Chạy server với biến môi trường (PowerShell):
   ```
   $env:GOOGLE_CLIENT_ID="xxxx.apps.googleusercontent.com"; build\nhatro.exe
   ```
Máy cần có lệnh `curl` (Windows 10/11 có sẵn) để server xác thực token với Google. Tài khoản Google trùng email với tài khoản có sẵn sẽ được liên kết.

## 5. Chức năng chính

**Khách / người dùng**
- **Địa chỉ đầy đủ cả nước, hai hệ:** *Mới* (34 tỉnh/thành → phường/xã, sau sáp nhập 2025) và *Cũ* (63 tỉnh/thành → quận/huyện → phường/xã). Chọn ở ô tìm kiếm và khi thêm tòa nhà; có ô gõ để lọc nhanh. Gõ tên không dấu cũng gợi ý được phường/xã.
- **Gợi ý khu vực gần bạn:** bấm *Dùng vị trí của tôi* → trình duyệt xin quyền định vị → hệ thống gợi ý các phường/quận gần nhất đang có phòng trống.
- **Trang chủ động:** mục *Ký túc xá / Nhà / Chung cư / Phòng trọ nổi bật* (Admin ghim tay và/hoặc tự động theo lượt xem), chương trình khuyến mãi, HOTDEAL đếm ngược, phòng NOW, phòng mới, hotline/Zalo.
- Tìm kiếm, lưu yêu thích, xem SĐT/Zalo, đặt lịch xem phòng, hỗ trợ tìm phòng, tra cứu hóa đơn, đăng ký môi giới.

**Quản trị – tách theo vai trò** (`/admin/index.html`)
- **ADMIN (rộng nhất):** thấy toàn bộ chủ tòa nhà, người dùng, tòa nhà, phòng, người thuê, xe, hợp đồng, sự cố, hóa đơn… của mọi chủ; sửa **hotline / Zalo / email / Facebook**; **khuyến mãi + phần trăm giảm giá** (giá ưu đãi tự tính theo %); **mục nổi bật** (ghim thủ công, tự động theo lượt xem, hay kết hợp).
- **Chủ tòa nhà (MANAGER):** mỗi chủ có trang quản lý riêng, **chỉ thấy dữ liệu tòa nhà của mình** (server lọc, không chỉ ẩn trên giao diện). Người dùng sau khi lên chủ tòa **không còn mục "Tôi là chủ nhà"**; thay vào đó là mục **"Hủy quyền quản lý tòa"** (menu tài khoản / trang Tài khoản).
- **Danh sách luôn nhóm Tòa nhà → Phòng:** người thuê (người ở chung phòng nằm cùng nhóm), phương tiện, sự cố/yêu cầu, hợp đồng, điện nước, hóa đơn, thanh toán, lịch xem phòng; có bộ lọc theo tòa nhà.
- **Tải ảnh từ thiết bị** cho tòa nhà và phòng (JPG/PNG/GIF/WEBP ≤ 5 MB): file lưu trong thư mục `uploads/` cạnh file database, đường dẫn lưu vào database (`building_images`, `room_images`, `rooms.thumbnail_url`, `buildings.cover_url`).
- **Hợp đồng thuê nhà chi tiết, có giá trị pháp lý:** đủ thông tin hai bên (CCCD, ngày cấp, nơi cấp, địa chỉ thường trú, tài khoản), thời hạn, giá bằng số và bằng chữ, đặt cọc, ngày thanh toán, phạt trả chậm, điện/nước/dịch vụ, quyền – nghĩa vụ, chấm dứt, giải quyết tranh chấp, phụ lục tài sản bàn giao, người ở cùng, phương tiện, chữ ký. Bấm **"📄 Xem / In PDF"** ở danh sách hợp đồng → bấm **"🖨 In / Lưu PDF"** → chọn *Lưu thành PDF* (hoặc in giấy A4).
  > Hợp đồng được lập tự động từ dữ liệu; hãy điền đủ CCCD/địa chỉ của hai bên (trang sẽ báo phần còn thiếu) và nên tham khảo luật sư/công chứng trước khi ký bản chính thức.

### Cách sửa hotline và Zalo
Đăng nhập **admin** → menu trái **⚙️ Hotline, Zalo & cấu hình** → sửa ô *Số hotline* / *Số Zalo* → **Lưu cấu hình**. Trang chủ, chân trang và nút Zalo nổi cập nhật ngay.

### Cách đưa phòng lên mục nổi bật
Admin → **⭐ Mục nổi bật**: chọn chế độ *Kết hợp* (phòng ghim trước, còn lại theo lượt xem) / *Thủ công* / *Tự động theo lượt xem*, số phòng mỗi mục, các mục hiển thị; ghim thêm phòng bằng ô tìm kiếm. Lượt xem chỉ tính 1 lần / người / phòng / 30 phút (chống bấm nhiều lần).

### Cách tạo khuyến mãi theo %
Admin → **🎁 Khuyến mãi & % giảm giá** → *Thêm mới* → nhập *Phần trăm giảm giá* → bấm **Phòng áp dụng** để chọn phòng. Giá ưu đãi từng phòng được tự tính (làm tròn nghìn đồng) và tự tính lại khi bạn đổi %.

## 6. Cấu trúc

```
src/            lõi backend C++ dùng chung (app, db, util, crypto, public, locations, uploads) + src/admin (bộ máy CRUD)
thanh-vien-1…5-*/  mỗi thành viên 1 thư mục: backend/ (C++) + web/ (HTML/CSS/JS) – xem MODULES.md
public/         giao diện dùng chung: css/, js/ (api, ui, layout…), img/, admin/ (khung SPA quản trị)
database/       schema.sqlite.sql (cấu trúc) + seed_vn_admin.sql (địa giới VN) + seed_demo.sql (dữ liệu mẫu)
                schema.mysql.sql chỉ là bản MySQL tham khảo, không còn đồng bộ
tools/          make_admin_units.py (tạo lại seed_vn_admin.sql từ dữ liệu địa giới)
tests/          unit_tests.cpp, api_test.py, e2e_test.py, api.http
third_party/    cpp-httplib, nlohmann/json
```
Database: 14 bảng ERD (`roles, users, buildings, room_types, rooms, tenants, vehicles, contracts, tickets, meter_readings, invoices, services, invoice_details, payments`) + `user_identities` (Google), `provinces, districts, wards` (2 hệ địa giới), `landmarks, amenities, room_amenities, room_images, building_images, uploads, favorites, promotions, promotion_rooms, support_requests, broker_applications, viewing_requests, site_settings`.

**Nâng cấp từ bản cũ:** cấu trúc database đã đổi (`PRAGMA user_version = 2`). Lần chạy đầu server tự **sao lưu** file cũ thành `nhatro.db.old-<thời gian>` rồi tạo database mới (dữ liệu cũ nằm trong file sao lưu, không bị mất).

**Nguồn dữ liệu địa giới:** hệ cũ lấy từ dữ liệu Tổng cục Thống kê (qua dự án mã nguồn mở *dvhcvn*, tình trạng 01/03/2025); hệ mới (34 tỉnh/thành, 2 cấp) theo Nghị quyết sáp nhập 2025. Dữ liệu hành chính có thể thay đổi — chạy lại `tools/make_admin_units.py` khi có bản mới.

## 7. Docker (tùy chọn)
```
docker compose up --build
```
rồi mở http://localhost:8080.

> Đây là dự án học tập, dữ liệu mẫu. Khi đưa lên mạng thật hãy đổi mật khẩu mẫu, đặt `JWT_SECRET`, dùng HTTPS.

## Phân quyền

- **ADMIN**: xem và sửa mọi dữ liệu; quản lý tài khoản, hotline/Zalo, khuyến mãi, mục nổi bật, danh mục chung.
- **MANAGER (chủ tòa nhà)**: chỉ xem / thêm / sửa dữ liệu thuộc **tòa nhà mình làm chủ** (`buildings.manager_id`): phòng, người thuê, phương tiện, hợp đồng, sự cố, điện nước, hóa đơn, thanh toán, lịch xem phòng. Dữ liệu của chủ khác **không trả về** từ API (không chỉ ẩn trên giao diện). Danh mục chung (loại hình, dịch vụ, trường học, tiện nghi) chỉ xem.
- **TENANT**: người dùng thường.


## Phân chia theo 5 thành viên
Xem [MODULES.md](MODULES.md): mỗi thành viên có thư mục riêng `thanh-vien-N-*/` gồm `backend/` và `web/`.
