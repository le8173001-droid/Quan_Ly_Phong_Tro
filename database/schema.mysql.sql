-- !!! LƯU Ý: file này chỉ là bản MySQL THAM KHẢO (đời cũ theo ERD), KHÔNG còn được đồng bộ.
-- Ứng dụng dùng database/schema.sqlite.sql (+ seed_vn_admin.sql, seed_demo.sql) làm nguồn chính.
-- =====================================================================
-- NHÀ TRỌ+ - DATABASE SCHEMA (MySQL 8.0.16+ / 8.4)
-- Thiết kế theo sơ đồ ERD (14 bảng) + các bảng bổ sung cho giao diện
-- (tìm phòng, khu vực, địa điểm học tập/làm việc, khuyến mãi, hỗ trợ...).
-- =====================================================================
-- Báo cho MySQL biết file này là UTF-8 (thiếu dòng này tiếng Việt sẽ bị lỗi font khi chạy trong Docker).
SET NAMES utf8mb4;

CREATE DATABASE IF NOT EXISTS nha_tro
    CHARACTER SET utf8mb4
    COLLATE utf8mb4_unicode_ci;

USE nha_tro;

-- =====================================================================
-- PHẦN A. 14 BẢNG THEO SƠ ĐỒ ERD
-- =====================================================================

-- ---------------------------------------------------------------------
-- 1. ROLES: phân quyền (1 role - n users)
--    role_code dùng trong code (ADMIN / MANAGER / TENANT), role_name là tên hiển thị
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS roles (
    role_id   INT          NOT NULL AUTO_INCREMENT,
    role_code VARCHAR(20)  NOT NULL,
    role_name VARCHAR(100) NOT NULL,
    PRIMARY KEY (role_id),
    UNIQUE KEY uk_roles_code (role_code)
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 2. USERS: tài khoản đăng nhập
--    password_hash cho phép NULL: tài khoản tạo bằng Google chưa có mật khẩu.
--    user_id cũng là giá trị lưu vào create_by / update_by của rooms.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS users (
    user_id       INT          NOT NULL AUTO_INCREMENT,
    role_id       INT          NOT NULL,
    username      VARCHAR(50)  NOT NULL,
    password_hash VARCHAR(255) NULL,
    status        VARCHAR(20)  NOT NULL DEFAULT 'active',      -- active | locked
    -- bổ sung
    full_name     VARCHAR(100) NOT NULL,
    email         VARCHAR(100) NULL,
    phone         VARCHAR(20)  NULL,
    avatar_url    VARCHAR(500) NULL,
    last_login_at DATETIME     NULL,
    created_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at    DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id),
    UNIQUE KEY uk_users_username (username),
    UNIQUE KEY uk_users_email (email),
    CONSTRAINT fk_users_role FOREIGN KEY (role_id) REFERENCES roles (role_id),
    CONSTRAINT chk_users_status CHECK (status IN ('active', 'locked'))
) ENGINE = InnoDB;

-- Đăng nhập bằng nhà cung cấp bên ngoài (Google). provider_user_id = "sub" trong ID token của Google.
CREATE TABLE IF NOT EXISTS user_identities (
    identity_id      INT          NOT NULL AUTO_INCREMENT,
    user_id          INT          NOT NULL,
    provider         VARCHAR(20)  NOT NULL,                    -- google
    provider_user_id VARCHAR(100) NOT NULL,
    email            VARCHAR(100) NULL,
    created_at       DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (identity_id),
    UNIQUE KEY uk_identity_provider_user (provider, provider_user_id),
    UNIQUE KEY uk_identity_user_provider (user_id, provider),
    CONSTRAINT fk_identity_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE CASCADE
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- Khu vực: tỉnh/thành -> quận/huyện -> phường/xã (dùng cho ô "Khu vực" khi tìm phòng)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS provinces (
    province_id INT          NOT NULL AUTO_INCREMENT,
    name        VARCHAR(100) NOT NULL,
    PRIMARY KEY (province_id),
    UNIQUE KEY uk_provinces_name (name)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS districts (
    district_id INT          NOT NULL AUTO_INCREMENT,
    province_id INT          NOT NULL,
    name        VARCHAR(100) NOT NULL,
    PRIMARY KEY (district_id),
    UNIQUE KEY uk_districts_name (province_id, name),
    CONSTRAINT fk_districts_province FOREIGN KEY (province_id) REFERENCES provinces (province_id)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS wards (
    ward_id     INT          NOT NULL AUTO_INCREMENT,
    district_id INT          NOT NULL,
    name        VARCHAR(100) NOT NULL,
    PRIMARY KEY (ward_id),
    UNIQUE KEY uk_wards_name (district_id, name),
    CONSTRAINT fk_wards_district FOREIGN KEY (district_id) REFERENCES districts (district_id)
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 3. BUILDINGS: tòa nhà / khu trọ (user quản lý 1-n buildings)
--    total_rooms được hệ thống tự cập nhật mỗi khi thêm / xóa / ẩn phòng.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS buildings (
    building_id   INT           NOT NULL AUTO_INCREMENT,
    manager_id    INT           NOT NULL,
    name          VARCHAR(150)  NOT NULL,
    address       VARCHAR(255)  NOT NULL,
    total_rooms   INT           NOT NULL DEFAULT 0,
    -- bổ sung: vị trí + liên hệ (phục vụ tìm theo khu vực, tìm trọ gần tôi, gần trường/nơi làm việc)
    province_id   INT           NOT NULL,
    district_id   INT           NOT NULL,
    ward_id       INT           NULL,
    latitude      DECIMAL(9, 6) NULL,
    longitude     DECIMAL(9, 6) NULL,
    contact_name  VARCHAR(100)  NULL,
    contact_phone VARCHAR(20)   NULL,
    description   VARCHAR(500)  NULL,
    created_at    DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    updated_at    DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    PRIMARY KEY (building_id),
    CONSTRAINT fk_buildings_manager  FOREIGN KEY (manager_id) REFERENCES users (user_id),
    CONSTRAINT fk_buildings_province FOREIGN KEY (province_id) REFERENCES provinces (province_id),
    CONSTRAINT fk_buildings_district FOREIGN KEY (district_id) REFERENCES districts (district_id),
    CONSTRAINT fk_buildings_ward     FOREIGN KEY (ward_id)     REFERENCES wards (ward_id),
    CONSTRAINT chk_buildings_total   CHECK (total_rooms >= 0),
    INDEX idx_buildings_district (district_id),
    INDEX idx_buildings_province (province_id),
    INDEX idx_buildings_geo (latitude, longitude)
) ENGINE = InnoDB;

-- Địa điểm học tập / làm việc (trường đại học, khu công nghiệp...) cho ô "Gần nơi học tập, làm việc"
CREATE TABLE IF NOT EXISTS landmarks (
    landmark_id   INT           NOT NULL AUTO_INCREMENT,
    name          VARCHAR(150)  NOT NULL,
    landmark_type VARCHAR(20)   NOT NULL,                      -- school | work
    district_id   INT           NULL,
    address       VARCHAR(255)  NULL,
    latitude      DECIMAL(9, 6) NOT NULL,
    longitude     DECIMAL(9, 6) NOT NULL,
    PRIMARY KEY (landmark_id),
    UNIQUE KEY uk_landmarks_name (name),
    CONSTRAINT fk_landmarks_district FOREIGN KEY (district_id) REFERENCES districts (district_id),
    CONSTRAINT chk_landmarks_type CHECK (landmark_type IN ('school', 'work'))
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 4. ROOM_TYPES: loại hình cho thuê (1 type - n rooms)
--    Phòng trọ, Ký túc xá, Nhà, Chung cư, Văn phòng, Phòng pass & ở ghép
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS room_types (
    type_id     INT          NOT NULL AUTO_INCREMENT,
    type_name   VARCHAR(100) NOT NULL,
    slug        VARCHAR(50)  NOT NULL,
    icon        VARCHAR(30)  NULL,                             -- tên biểu tượng (front end tự vẽ)
    description VARCHAR(255) NULL,
    sort_order  INT          NOT NULL DEFAULT 0,
    PRIMARY KEY (type_id),
    UNIQUE KEY uk_room_types_name (type_name),
    UNIQUE KEY uk_room_types_slug (slug)
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 5. ROOMS: phòng (1 building - n rooms, 1 type - n rooms)
--    room_id INT tự tăng theo ERD; room_number (P101, A1.18...) là mã phòng nhìn thấy, duy nhất trong 1 tòa nhà.
--    status: available | occupied | maintenance | hidden (hidden = xóa mềm)
--    4 trường audit: create_date, update_date, create_by, update_by
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS rooms (
    room_id           INT           NOT NULL AUTO_INCREMENT,
    building_id       INT           NOT NULL,
    type_id           INT           NOT NULL,
    room_number       VARCHAR(20)   NOT NULL,
    status            VARCHAR(20)   NOT NULL DEFAULT 'available',
    base_rent_price   DECIMAL(12,0) NOT NULL DEFAULT 0,        -- giá thuê / tháng (VND)
    -- bổ sung cho tin đăng
    title             VARCHAR(200)  NOT NULL,
    description       TEXT          NULL,
    area_m2           DECIMAL(7,1)  NULL,                      -- diện tích
    max_occupants     INT           NOT NULL DEFAULT 2,        -- sức chứa tối đa (để xác định "đã kín")
    current_occupants INT           NOT NULL DEFAULT 0,        -- số người đang ở
    is_hot            TINYINT(1)    NOT NULL DEFAULT 0,
    is_new            TINYINT(1)    NOT NULL DEFAULT 0,
    is_now            TINYINT(1)    NOT NULL DEFAULT 0,        -- có thể dọn vào ở ngay
    thumbnail_url     VARCHAR(500)  NULL,
    view_count        INT           NOT NULL DEFAULT 0,

    -- Audit
    create_date       DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    update_date       DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP,
    create_by         INT           NOT NULL,
    update_by         INT           NOT NULL,

    PRIMARY KEY (room_id),
    UNIQUE KEY uk_rooms_building_number (building_id, room_number),
    CONSTRAINT fk_rooms_building  FOREIGN KEY (building_id) REFERENCES buildings (building_id),
    CONSTRAINT fk_rooms_type      FOREIGN KEY (type_id)     REFERENCES room_types (type_id),
    CONSTRAINT fk_rooms_create_by FOREIGN KEY (create_by)   REFERENCES users (user_id),
    CONSTRAINT fk_rooms_update_by FOREIGN KEY (update_by)   REFERENCES users (user_id),
    CONSTRAINT chk_rooms_price     CHECK (base_rent_price >= 0),
    CONSTRAINT chk_rooms_area      CHECK (area_m2 IS NULL OR area_m2 > 0),
    CONSTRAINT chk_rooms_occupants CHECK (current_occupants >= 0 AND current_occupants <= max_occupants),
    CONSTRAINT chk_rooms_status    CHECK (status IN ('available', 'occupied', 'maintenance', 'hidden')),
    INDEX idx_rooms_type (type_id),
    INDEX idx_rooms_price (base_rent_price),
    INDEX idx_rooms_area (area_m2),
    INDEX idx_rooms_status (status)
) ENGINE = InnoDB;

-- Tiện nghi (wifi, máy lạnh...) và hình ảnh của phòng
CREATE TABLE IF NOT EXISTS amenities (
    amenity_id INT          NOT NULL AUTO_INCREMENT,
    name       VARCHAR(100) NOT NULL,
    icon       VARCHAR(30)  NULL,
    PRIMARY KEY (amenity_id),
    UNIQUE KEY uk_amenities_name (name)
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS room_amenities (
    room_id    INT NOT NULL,
    amenity_id INT NOT NULL,
    PRIMARY KEY (room_id, amenity_id),
    CONSTRAINT fk_room_amenities_room    FOREIGN KEY (room_id)    REFERENCES rooms (room_id)       ON DELETE CASCADE,
    CONSTRAINT fk_room_amenities_amenity FOREIGN KEY (amenity_id) REFERENCES amenities (amenity_id) ON DELETE CASCADE
) ENGINE = InnoDB;

CREATE TABLE IF NOT EXISTS room_images (
    image_id   INT          NOT NULL AUTO_INCREMENT,
    room_id    INT          NOT NULL,
    url        VARCHAR(500) NOT NULL,
    sort_order INT          NOT NULL DEFAULT 0,
    created_at DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (image_id),
    INDEX idx_room_images_room (room_id),
    CONSTRAINT fk_room_images_room FOREIGN KEY (room_id) REFERENCES rooms (room_id) ON DELETE CASCADE
) ENGINE = InnoDB;

-- Phòng yêu thích của người dùng
CREATE TABLE IF NOT EXISTS favorites (
    user_id    INT      NOT NULL,
    room_id    INT      NOT NULL,
    created_at DATETIME NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (user_id, room_id),
    CONSTRAINT fk_favorites_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE CASCADE,
    CONSTRAINT fk_favorites_room FOREIGN KEY (room_id) REFERENCES rooms (room_id) ON DELETE CASCADE
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 6. TENANTS: người thuê (user có hồ sơ 1-1 tenant)
--    user_id cho phép NULL: người thuê chưa có tài khoản vẫn lưu được hồ sơ.
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS tenants (
    tenant_id  INT          NOT NULL AUTO_INCREMENT,
    user_id    INT          NULL,
    full_name  VARCHAR(100) NOT NULL,
    phone      VARCHAR(20)  NOT NULL,
    email      VARCHAR(100) NULL,
    id_number  VARCHAR(20)  NULL,                              -- CCCD / hộ chiếu
    created_at DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (tenant_id),
    UNIQUE KEY uk_tenants_user (user_id),
    INDEX idx_tenants_phone (phone),
    CONSTRAINT fk_tenants_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE SET NULL
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 7. VEHICLES: xe của người thuê (1 tenant - n vehicles)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS vehicles (
    vehicle_id    INT         NOT NULL AUTO_INCREMENT,
    tenant_id     INT         NOT NULL,
    license_plate VARCHAR(20) NOT NULL,
    vehicle_type  VARCHAR(20) NOT NULL DEFAULT 'motorbike',    -- motorbike | car | bicycle
    PRIMARY KEY (vehicle_id),
    UNIQUE KEY uk_vehicles_plate (license_plate),
    CONSTRAINT fk_vehicles_tenant FOREIGN KEY (tenant_id) REFERENCES tenants (tenant_id),
    CONSTRAINT chk_vehicles_type CHECK (vehicle_type IN ('motorbike', 'car', 'bicycle'))
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 8. CONTRACTS: hợp đồng thuê (1 room - n contracts, 1 tenant - n contracts)
--    stay_type: short_term | long_term; booking_source: direct | airbnb | booking | agent
--    status: pending | active | ended | cancelled
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS contracts (
    contract_id    INT           NOT NULL AUTO_INCREMENT,
    room_id        INT           NOT NULL,
    tenant_id      INT           NOT NULL,
    start_date     DATE          NOT NULL,
    end_date       DATE          NOT NULL,
    stay_type      VARCHAR(20)   NOT NULL DEFAULT 'long_term',
    booking_source VARCHAR(20)   NOT NULL DEFAULT 'direct',
    deposit_amount DECIMAL(12,0) NOT NULL DEFAULT 0,
    status         VARCHAR(20)   NOT NULL DEFAULT 'active',
    -- bổ sung
    monthly_rent   DECIMAL(12,0) NOT NULL DEFAULT 0,
    note           VARCHAR(500)  NULL,
    created_at     DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (contract_id),
    INDEX idx_contracts_room (room_id),
    INDEX idx_contracts_tenant (tenant_id),
    CONSTRAINT fk_contracts_room   FOREIGN KEY (room_id)   REFERENCES rooms (room_id),
    CONSTRAINT fk_contracts_tenant FOREIGN KEY (tenant_id) REFERENCES tenants (tenant_id),
    CONSTRAINT chk_contracts_dates   CHECK (end_date >= start_date),
    CONSTRAINT chk_contracts_stay    CHECK (stay_type IN ('short_term', 'long_term')),
    CONSTRAINT chk_contracts_source  CHECK (booking_source IN ('direct', 'airbnb', 'booking', 'agent')),
    CONSTRAINT chk_contracts_status  CHECK (status IN ('pending', 'active', 'ended', 'cancelled'))
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 9. TICKETS: yêu cầu / sự cố (1 room - n tickets, 1 tenant - n tickets)
--    category: hygiene | maintenance | electric_water | security | other
--    status: open | in_progress | done | cancelled
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS tickets (
    ticket_id   INT          NOT NULL AUTO_INCREMENT,
    room_id     INT          NOT NULL,
    tenant_id   INT          NOT NULL,
    category    VARCHAR(30)  NOT NULL,
    status      VARCHAR(20)  NOT NULL DEFAULT 'open',
    -- bổ sung
    title       VARCHAR(200) NOT NULL,
    description VARCHAR(1000) NULL,
    created_at  DATETIME     NOT NULL DEFAULT CURRENT_TIMESTAMP,
    resolved_at DATETIME     NULL,
    PRIMARY KEY (ticket_id),
    INDEX idx_tickets_room (room_id),
    INDEX idx_tickets_status (status),
    CONSTRAINT fk_tickets_room   FOREIGN KEY (room_id)   REFERENCES rooms (room_id),
    CONSTRAINT fk_tickets_tenant FOREIGN KEY (tenant_id) REFERENCES tenants (tenant_id),
    CONSTRAINT chk_tickets_category CHECK (category IN ('hygiene', 'maintenance', 'electric_water', 'security', 'other')),
    CONSTRAINT chk_tickets_status   CHECK (status IN ('open', 'in_progress', 'done', 'cancelled'))
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 10. METER_READINGS: chốt điện nước theo tháng (1 room - n readings)
--     billing_month lưu dạng 'YYYY-MM' (dễ sắp xếp), giao diện hiển thị MM/YYYY
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS meter_readings (
    reading_id    INT         NOT NULL AUTO_INCREMENT,
    room_id       INT         NOT NULL,
    billing_month CHAR(7)     NOT NULL,
    old_electric  INT         NOT NULL DEFAULT 0,
    new_electric  INT         NOT NULL DEFAULT 0,
    -- bổ sung: nước
    old_water     INT         NOT NULL DEFAULT 0,
    new_water     INT         NOT NULL DEFAULT 0,
    recorded_at   DATETIME    NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (reading_id),
    UNIQUE KEY uk_meter_room_month (room_id, billing_month),
    CONSTRAINT fk_meter_room FOREIGN KEY (room_id) REFERENCES rooms (room_id),
    CONSTRAINT chk_meter_electric CHECK (new_electric >= old_electric),
    CONSTRAINT chk_meter_water    CHECK (new_water >= old_water)
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 11. SERVICES: danh mục dịch vụ / loại phí
--     calc_type quyết định cách tính khi tạo hóa đơn tự động:
--       RENT           = tiền thuê (lấy từ hợp đồng)
--       METER_ELECTRIC = (số mới - số cũ) x đơn giá, lấy từ meter_readings
--       METER_WATER    = (nước mới - nước cũ) x đơn giá
--       PER_VEHICLE    = số xe của người thuê x đơn giá
--       FIXED          = 1 x đơn giá
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS services (
    service_id   INT           NOT NULL AUTO_INCREMENT,
    service_name VARCHAR(100)  NOT NULL,
    -- bổ sung
    calc_type    VARCHAR(20)   NOT NULL DEFAULT 'FIXED',
    unit         VARCHAR(30)   NOT NULL DEFAULT 'tháng',
    unit_price   DECIMAL(12,0) NOT NULL DEFAULT 0,
    is_active    TINYINT(1)    NOT NULL DEFAULT 1,
    PRIMARY KEY (service_id),
    UNIQUE KEY uk_services_name (service_name),
    CONSTRAINT chk_services_calc CHECK (calc_type IN ('RENT', 'METER_ELECTRIC', 'METER_WATER', 'PER_VEHICLE', 'FIXED')),
    CONSTRAINT chk_services_price CHECK (unit_price >= 0)
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 12. INVOICES: hóa đơn tháng (1 contract - n invoices)
--     status: unpaid | partial | paid | cancelled
--     invoice_code (INV-202610-0001) dùng để tra cứu hóa đơn
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS invoices (
    invoice_id    INT           NOT NULL AUTO_INCREMENT,
    contract_id   INT           NOT NULL,
    billing_month CHAR(7)       NOT NULL,
    total_amount  DECIMAL(12,0) NOT NULL DEFAULT 0,
    status        VARCHAR(20)   NOT NULL DEFAULT 'unpaid',
    due_date      DATE          NOT NULL,
    -- bổ sung
    invoice_code  VARCHAR(30)   NOT NULL,
    issued_at     DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    note          VARCHAR(500)  NULL,
    PRIMARY KEY (invoice_id),
    UNIQUE KEY uk_invoices_code (invoice_code),
    UNIQUE KEY uk_invoices_contract_month (contract_id, billing_month),
    INDEX idx_invoices_status (status),
    CONSTRAINT fk_invoices_contract FOREIGN KEY (contract_id) REFERENCES contracts (contract_id),
    CONSTRAINT chk_invoices_total  CHECK (total_amount >= 0),
    CONSTRAINT chk_invoices_status CHECK (status IN ('unpaid', 'partial', 'paid', 'cancelled'))
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 13. INVOICE_DETAILS: chi tiết phí của hóa đơn (1 invoice - n details, 1 service - n details)
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS invoice_details (
    detail_id   INT           NOT NULL AUTO_INCREMENT,
    invoice_id  INT           NOT NULL,
    service_id  INT           NOT NULL,
    subtotal    DECIMAL(12,0) NOT NULL DEFAULT 0,
    -- bổ sung
    description VARCHAR(200)  NULL,
    quantity    DECIMAL(10,2) NOT NULL DEFAULT 1,
    unit_price  DECIMAL(12,0) NOT NULL DEFAULT 0,
    PRIMARY KEY (detail_id),
    INDEX idx_invoice_details_invoice (invoice_id),
    CONSTRAINT fk_invoice_details_invoice FOREIGN KEY (invoice_id) REFERENCES invoices (invoice_id) ON DELETE CASCADE,
    CONSTRAINT fk_invoice_details_service FOREIGN KEY (service_id) REFERENCES services (service_id)
) ENGINE = InnoDB;

-- ---------------------------------------------------------------------
-- 14. PAYMENTS: thanh toán (1 invoice - n payments)
--     method: cash | bank_transfer | momo | other
-- ---------------------------------------------------------------------
CREATE TABLE IF NOT EXISTS payments (
    payment_id  INT           NOT NULL AUTO_INCREMENT,
    invoice_id  INT           NOT NULL,
    amount_paid DECIMAL(12,0) NOT NULL,
    -- bổ sung
    paid_at     DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    method      VARCHAR(20)   NOT NULL DEFAULT 'cash',
    note        VARCHAR(255)  NULL,
    PRIMARY KEY (payment_id),
    INDEX idx_payments_invoice (invoice_id),
    CONSTRAINT fk_payments_invoice FOREIGN KEY (invoice_id) REFERENCES invoices (invoice_id),
    CONSTRAINT chk_payments_amount CHECK (amount_paid > 0),
    CONSTRAINT chk_payments_method CHECK (method IN ('cash', 'bank_transfer', 'momo', 'other'))
) ENGINE = InnoDB;

-- =====================================================================
-- PHẦN B. BẢNG BỔ SUNG CHO CÁC CHỨC NĂNG TRÊN GIAO DIỆN
-- =====================================================================

-- Chương trình & ưu đãi đang diễn ra (banner chương trình, HOTDEAL đếm ngược, voucher, đối tác)
--   promo_type: program | hotdeal | voucher | partner
CREATE TABLE IF NOT EXISTS promotions (
    promotion_id  INT           NOT NULL AUTO_INCREMENT,
    title         VARCHAR(200)  NOT NULL,
    description   VARCHAR(500)  NULL,
    promo_type    VARCHAR(20)   NOT NULL DEFAULT 'program',
    discount_text VARCHAR(50)   NULL,                          -- ví dụ "-10%", "Giảm 200K"
    voucher_code  VARCHAR(30)   NULL,
    link_url      VARCHAR(300)  NULL,
    banner_color  VARCHAR(20)   NULL,                          -- màu nền banner (mã màu hoặc tên)
    start_at      DATETIME      NOT NULL,
    end_at        DATETIME      NOT NULL,
    is_active     TINYINT(1)    NOT NULL DEFAULT 1,
    sort_order    INT           NOT NULL DEFAULT 0,
    created_at    DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (promotion_id),
    INDEX idx_promotions_active (is_active, start_at, end_at),
    CONSTRAINT chk_promotions_type  CHECK (promo_type IN ('program', 'hotdeal', 'voucher', 'partner')),
    CONSTRAINT chk_promotions_dates CHECK (end_at > start_at)
) ENGINE = InnoDB;

-- Phòng tham gia HOTDEAL và giá ưu đãi
CREATE TABLE IF NOT EXISTS promotion_rooms (
    promotion_id INT           NOT NULL,
    room_id      INT           NOT NULL,
    deal_price   DECIMAL(12,0) NOT NULL,
    PRIMARY KEY (promotion_id, room_id),
    CONSTRAINT fk_promo_rooms_promo FOREIGN KEY (promotion_id) REFERENCES promotions (promotion_id) ON DELETE CASCADE,
    CONSTRAINT fk_promo_rooms_room  FOREIGN KEY (room_id)      REFERENCES rooms (room_id)           ON DELETE CASCADE,
    CONSTRAINT chk_promo_rooms_price CHECK (deal_price >= 0)
) ENGINE = InnoDB;

-- "Hỗ trợ tìm phòng": khách để lại nhu cầu, nhân viên gọi lại
--   request_type: find_room | landlord | other ; status: new | processing | done
CREATE TABLE IF NOT EXISTS support_requests (
    request_id   INT           NOT NULL AUTO_INCREMENT,
    user_id      INT           NULL,
    full_name    VARCHAR(100)  NOT NULL,
    phone        VARCHAR(20)   NOT NULL,
    email        VARCHAR(100)  NULL,
    request_type VARCHAR(20)   NOT NULL DEFAULT 'find_room',
    desired_area VARCHAR(150)  NULL,
    budget_max   DECIMAL(12,0) NULL,
    note         VARCHAR(1000) NULL,
    status       VARCHAR(20)   NOT NULL DEFAULT 'new',
    created_at   DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (request_id),
    INDEX idx_support_status (status),
    CONSTRAINT fk_support_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE SET NULL,
    CONSTRAINT chk_support_type   CHECK (request_type IN ('find_room', 'landlord', 'other')),
    CONSTRAINT chk_support_status CHECK (status IN ('new', 'processing', 'done'))
) ENGINE = InnoDB;

-- "Tuyển môi giới": đơn đăng ký làm môi giới.  status: new | approved | rejected
CREATE TABLE IF NOT EXISTS broker_applications (
    application_id INT           NOT NULL AUTO_INCREMENT,
    full_name      VARCHAR(100)  NOT NULL,
    phone          VARCHAR(20)   NOT NULL,
    email          VARCHAR(100)  NULL,
    work_area      VARCHAR(150)  NULL,
    experience     VARCHAR(1000) NULL,
    status         VARCHAR(20)   NOT NULL DEFAULT 'new',
    created_at     DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (application_id),
    CONSTRAINT chk_broker_status CHECK (status IN ('new', 'approved', 'rejected'))
) ENGINE = InnoDB;

-- Yêu cầu xem phòng / liên hệ chủ nhà từ trang chi tiết phòng.  status: new | contacted | done | cancelled
CREATE TABLE IF NOT EXISTS viewing_requests (
    viewing_id     INT           NOT NULL AUTO_INCREMENT,
    room_id        INT           NOT NULL,
    user_id        INT           NULL,
    full_name      VARCHAR(100)  NOT NULL,
    phone          VARCHAR(20)   NOT NULL,
    message        VARCHAR(1000) NULL,
    preferred_time VARCHAR(100)  NULL,
    status         VARCHAR(20)   NOT NULL DEFAULT 'new',
    created_at     DATETIME      NOT NULL DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (viewing_id),
    INDEX idx_viewing_status (status),
    CONSTRAINT fk_viewing_room FOREIGN KEY (room_id) REFERENCES rooms (room_id) ON DELETE CASCADE,
    CONSTRAINT fk_viewing_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE SET NULL,
    CONSTRAINT chk_viewing_status CHECK (status IN ('new', 'contacted', 'done', 'cancelled'))
) ENGINE = InnoDB;

-- Cấu hình trang web (tên website, hotline, email, địa chỉ...)
CREATE TABLE IF NOT EXISTS site_settings (
    setting_key   VARCHAR(50)  NOT NULL,
    setting_value VARCHAR(500) NOT NULL,
    PRIMARY KEY (setting_key)
) ENGINE = InnoDB;

-- =====================================================================
-- PHẦN C. DỮ LIỆU MẪU
-- (Chạy trên database trống. Muốn nạp lại từ đầu: docker compose down -v)
-- =====================================================================

-- Vai trò
INSERT INTO roles (role_id, role_code, role_name) VALUES
    (1, 'ADMIN',   'Quản trị viên'),
    (2, 'MANAGER', 'Quản lý vận hành'),
    (3, 'TENANT',  'Người thuê / Khách hàng');

-- Tài khoản mẫu (mật khẩu đã mã hóa BCrypt, KHÔNG lưu mật khẩu thật trong database)
--   admin   / Admin@123
--   levinh  / Manager@123
--   minhanh, daniel / Tenant@123
INSERT INTO users (user_id, role_id, username, password_hash, status, full_name, email, phone) VALUES
    (1, 1, 'admin',   '$2a$10$yRU9BibLyIWdQpAscddMdeED3Oj9KAseK6LJRV98Zrn/THx8e0f/u', 'active', 'Quản trị viên', 'admin@nhatro.vn',   '0900000001'),
    (2, 2, 'levinh',  '$2a$10$6CgA6uwClcLpnvKe.9SKwu465isgxhDYMnk001odrei/QkfVc3a3K', 'active', 'Lê Vinh',       'levinh@nhatro.vn',  '0900000002'),
    (3, 3, 'minhanh', '$2a$10$sBpAOZODtT8CWBde4khE3O1vDg/pdAQcjrw1gyOfwed.q79Gh.dtW', 'active', 'Minh Anh',      'minhanh@gmail.com', '0901234567'),
    (4, 3, 'daniel',  '$2a$10$sBpAOZODtT8CWBde4khE3O1vDg/pdAQcjrw1gyOfwed.q79Gh.dtW', 'active', 'Daniel Lee',    'daniel@example.com','0912345678');

-- Khu vực
INSERT INTO provinces (province_id, name) VALUES
    (1, 'TP. Hồ Chí Minh'), (2, 'Hà Nội'), (3, 'Đà Nẵng');

INSERT INTO districts (district_id, province_id, name) VALUES
    (1, 1, 'Quận 1'), (2, 1, 'Quận 3'), (3, 1, 'Quận 7'), (4, 1, 'Bình Thạnh'), (5, 1, 'Gò Vấp'),
    (6, 1, 'Thủ Đức'), (7, 1, 'Tân Bình'), (8, 1, 'Phú Nhuận'), (9, 1, 'Quận 10'),
    (10, 2, 'Cầu Giấy'), (11, 2, 'Đống Đa'), (12, 2, 'Thanh Xuân'),
    (13, 3, 'Hải Châu'), (14, 3, 'Sơn Trà'), (15, 3, 'Ngũ Hành Sơn');

INSERT INTO wards (ward_id, district_id, name) VALUES
    (1, 1, 'Bến Nghé'), (2, 1, 'Đa Kao'),
    (3, 3, 'Tân Phong'), (4, 3, 'Phú Mỹ'),
    (5, 4, 'Phường 22'), (6, 4, 'Phường 25'),
    (7, 5, 'Phường 5'), (8, 5, 'Phường 12'),
    (9, 6, 'Linh Trung'), (10, 6, 'Hiệp Bình Chánh'),
    (11, 8, 'Phường 7'),
    (12, 10, 'Dịch Vọng'), (13, 10, 'Mai Dịch'),
    (14, 13, 'Thạch Thang');

-- Loại hình cho thuê
INSERT INTO room_types (type_id, type_name, slug, icon, description, sort_order) VALUES
    (1, 'Phòng trọ',            'phong-tro',  'bed',       'Phòng trọ, nhà trọ cho sinh viên và người đi làm', 1),
    (2, 'Ký túc xá',            'ky-tuc-xa',  'dorm',      'Giường ký túc xá, phòng nhiều người ở',            2),
    (3, 'Nhà',                  'nha',        'house',     'Nhà nguyên căn cho thuê',                           3),
    (4, 'Chung cư',             'chung-cu',   'building',  'Căn hộ chung cư, căn hộ mini, studio',              4),
    (5, 'Văn phòng',            'van-phong',  'briefcase', 'Mặt bằng, văn phòng cho thuê',                      5),
    (6, 'Phòng pass & ở ghép',  'pass-o-ghep','users',     'Pass lại phòng, tìm người ở ghép',                  6);

-- Tòa nhà (người quản lý: Lê Vinh - user 2)
INSERT INTO buildings (building_id, manager_id, name, address, province_id, district_id, ward_id, latitude, longitude, contact_name, contact_phone, description) VALUES
    (1, 2, 'Nhà trọ Hoàng Gia',            '123 Quang Trung',               1, 5,  8,  10.838600, 106.665200, 'Lê Vinh',  '0900000002', 'Khu trọ an ninh, có camera, giờ giấc tự do'),
    (2, 2, 'Saigon Pearl Residence',       'Nguyễn Hữu Cảnh',               1, 4,  5,  10.795000, 106.721800, 'Lê Vinh',  '0900000002', 'Căn hộ cao cấp ven sông Sài Gòn'),
    (3, 2, 'The Metropolis Studio',        'Nguyễn Thị Thập',               1, 3,  3,  10.738000, 106.721000, 'Lê Vinh',  '0900000002', 'Studio và căn hộ mini khu Nam Sài Gòn'),
    (4, 2, 'Ký túc xá Sinh viên Thủ Đức',  'Đường Linh Trung',              1, 6,  9,  10.870000, 106.790000, 'Lê Vinh',  '0900000002', 'Ký túc xá gần Đại học Quốc gia'),
    (5, 2, 'Nhà Phú Nhuận Green',          '45 Phan Xích Long',             1, 8,  11, 10.799000, 106.680000, 'Lê Vinh',  '0900000002', 'Nhà nguyên căn và phòng trọ gần sân bay'),
    (6, 2, 'Central Office Nguyễn Huệ',    '8 Nguyễn Huệ',                  1, 1,  1,  10.774000, 106.704000, 'Lê Vinh',  '0900000002', 'Văn phòng trung tâm Quận 1'),
    (7, 2, 'Cầu Giấy Home',                'Số 10 Trần Thái Tông',          2, 10, 12, 21.030000, 105.790000, 'Lê Vinh',  '0900000002', 'Phòng trọ và căn hộ mini gần ĐH Quốc gia Hà Nội'),
    (8, 2, 'Nhà trọ Biển Xanh',            '56 Lê Duẩn',                    3, 13, 14, 16.060000, 108.220000, 'Lê Vinh',  '0900000002', 'Nhà trọ và căn hộ trung tâm Đà Nẵng');

-- Địa điểm học tập / làm việc (toạ độ gần đúng)
INSERT INTO landmarks (landmark_id, name, landmark_type, district_id, address, latitude, longitude) VALUES
    (1,  'Đại học Bách Khoa TP.HCM',          'school', 9,  '268 Lý Thường Kiệt',          10.773100, 106.660600),
    (2,  'Đại học Kinh tế TP.HCM',            'school', 2,  '59C Nguyễn Đình Chiểu',       10.782600, 106.695400),
    (3,  'Đại học Công nghiệp TP.HCM',        'school', 5,  '12 Nguyễn Văn Bảo',           10.822200, 106.686600),
    (4,  'Đại học Quốc gia TP.HCM',           'school', 6,  'Khu phố 6, Linh Trung',       10.870000, 106.803000),
    (5,  'Đại học Tôn Đức Thắng',             'school', 3,  '19 Nguyễn Hữu Thọ',           10.732400, 106.699400),
    (6,  'Landmark 81',                       'work',   4,  '720A Điện Biên Phủ',          10.795100, 106.721800),
    (7,  'Khu công nghệ cao TP.HCM',          'work',   6,  'Xa lộ Hà Nội',                10.853000, 106.791000),
    (8,  'Phú Mỹ Hưng',                       'work',   3,  'Nguyễn Lương Bằng',           10.729000, 106.719000),
    (9,  'Sân bay Tân Sơn Nhất',              'work',   7,  'Trường Sơn',                  10.818800, 106.651900),
    (10, 'Đại học Quốc gia Hà Nội',           'school', 10, '144 Xuân Thủy',               21.037600, 105.782600),
    (11, 'Đại học Bách Khoa Hà Nội',          'school', 12, '1 Đại Cồ Việt',               21.005300, 105.843300),
    (12, 'Đại học Bách Khoa Đà Nẵng',         'school', 13, '54 Nguyễn Lương Bằng',        16.073600, 108.149900);

-- Tiện nghi
INSERT INTO amenities (amenity_id, name, icon) VALUES
    (1, 'Wifi',                 'wifi'),
    (2, 'Máy lạnh',             'snow'),
    (3, 'Máy giặt',             'wash'),
    (4, 'Nóng lạnh',            'hot'),
    (5, 'Gác lửng',             'loft'),
    (6, 'Bếp riêng',            'kitchen'),
    (7, 'Ban công',             'balcony'),
    (8, 'Chỗ để xe',            'parking'),
    (9, 'Thang máy',            'elevator'),
    (10,'Bảo vệ 24/7',          'shield'),
    (11,'Giờ giấc tự do',       'clock'),
    (12,'Nội thất đầy đủ',      'sofa'),
    (13,'Camera an ninh',       'camera'),
    (14,'Cho nuôi thú cưng',    'pet');

-- Phòng
INSERT INTO rooms
    (building_id, type_id, room_number, title, description, status, base_rent_price, area_m2,
     max_occupants, current_occupants, is_hot, is_new, is_now, create_by, update_by)
VALUES
    (1, 1, 'P101', 'Phòng trọ có gác lửng gần ĐH Công nghiệp',
        'Phòng mới sơn, thoáng mát, có gác lửng, toilet riêng, giờ giấc tự do. Gần chợ, trường học và trạm xe buýt.',
        'available', 2500000, 20, 2, 0, 0, 1, 1, 1, 1),
    (1, 1, 'P102', 'Phòng trọ 22m² có ban công',
        'Phòng có ban công phơi đồ, an ninh tốt, camera 24/7.',
        'occupied', 2800000, 22, 2, 2, 1, 0, 0, 1, 1),
    (1, 6, 'P103', 'Pass phòng ở ghép nữ gần Gò Vấp',
        'Cần tìm bạn nữ ở ghép, phòng 2 giường tầng, đầy đủ tiện nghi. Có thể dọn vào ngay.',
        'available', 1200000, 18, 4, 2, 0, 0, 1, 1, 1),
    (1, 1, 'P201', 'Phòng trọ tầng 2 yên tĩnh',
        'Phòng rộng rãi, cửa sổ lớn, phù hợp sinh viên và nhân viên văn phòng.',
        'available', 3000000, 25, 3, 1, 0, 0, 0, 1, 1),
    (1, 1, 'P202', 'Phòng trọ đang sửa chữa',
        'Phòng đang sửa chữa, sẽ mở lại sớm.',
        'maintenance', 2600000, 20, 2, 0, 0, 0, 0, 1, 1),
    (1, 1, 'P999', 'Phòng đã ẩn (xóa mềm)',
        'Phòng này đang ẩn khỏi trang người dùng. Admin có thể khôi phục lại.',
        'hidden', 2000000, 15, 1, 0, 0, 0, 0, 1, 1),

    (2, 4, 'A1.18', 'Căn hộ cao cấp 2PN view sông Sài Gòn',
        'Căn hộ 70m² 2 phòng ngủ, nội thất cao cấp, hồ bơi, phòng gym, bảo vệ 24/7.',
        'available', 15000000, 70, 4, 0, 1, 1, 0, 1, 1),
    (2, 4, 'A2.05', 'Căn hộ 1PN full nội thất',
        'Căn hộ 45m², ban công, thang máy, gần Landmark 81.',
        'occupied', 9500000, 45, 3, 3, 0, 0, 0, 1, 1),
    (2, 4, 'S1.07', 'Studio hiện đại gần Landmark 81',
        'Studio 35m² đầy đủ nội thất, có thể thuê ngắn hạn hoặc dài hạn.',
        'available', 7500000, 35, 2, 1, 1, 0, 0, 1, 1),

    (3, 1, 'M301', 'Phòng trọ Phú Mỹ Hưng có máy lạnh',
        'Phòng 28m² có máy lạnh, nóng lạnh, gần ĐH Tôn Đức Thắng.',
        'available', 3500000, 28, 2, 1, 0, 0, 0, 1, 1),
    (3, 4, 'M303', 'Căn hộ mini Quận 7 mới xây',
        'Căn hộ mini 32m², bếp riêng, ban công, thang máy.',
        'available', 5500000, 32, 2, 0, 0, 1, 1, 1, 1),
    (3, 1, 'M304', 'Phòng trọ giá rẻ gần ĐH Tôn Đức Thắng',
        'Phòng 18m² cho 1 người, sạch sẽ, an ninh.',
        'available', 2200000, 18, 1, 0, 0, 0, 0, 1, 1),

    (4, 2, 'K101', 'Ký túc xá nữ – giường tầng gần ĐHQG',
        'Phòng 6 giường, tủ cá nhân, wifi, giờ giấc tự do, gần ĐHQG TP.HCM.',
        'available', 900000, 25, 6, 3, 1, 0, 0, 1, 1),
    (4, 2, 'K102', 'Ký túc xá nam – phòng 6 giường',
        'Phòng thoáng mát, có quạt, nhà vệ sinh chung sạch sẽ.',
        'occupied', 900000, 25, 6, 6, 0, 0, 0, 1, 1),
    (4, 2, 'K103', 'Ký túc xá mới – phòng 4 giường',
        'Phòng mới, có máy lạnh, tủ cá nhân.',
        'available', 1100000, 20, 4, 0, 0, 1, 0, 1, 1),
    (4, 1, 'T201', 'Phòng trọ sinh viên gần khu công nghệ cao',
        'Phòng 16m², gần khu công nghệ cao, giá sinh viên.',
        'available', 1800000, 16, 1, 0, 0, 1, 1, 1, 1),

    (5, 3, 'N01', 'Nhà nguyên căn 3 tầng hẻm xe hơi',
        'Nhà 120m² 3 tầng, 4 phòng ngủ, phù hợp ở nhóm hoặc làm văn phòng nhỏ.',
        'available', 18000000, 120, 8, 0, 1, 0, 0, 1, 1),
    (5, 3, 'N02', 'Nhà hẻm 1 trệt 1 lầu',
        'Nhà 60m² 2 phòng ngủ, hẻm yên tĩnh, gần chợ Phú Nhuận.',
        'available', 9000000, 60, 5, 0, 0, 0, 0, 1, 1),
    (5, 1, 'P301', 'Phòng trọ Phú Nhuận gần sân bay',
        'Phòng 24m², gần sân bay Tân Sơn Nhất, thuận tiện đi làm.',
        'available', 3200000, 24, 2, 1, 0, 0, 0, 1, 1),

    (6, 5, 'V01', 'Văn phòng nhỏ 40m² trung tâm Quận 1',
        'Văn phòng đẹp, có điều hòa, lễ tân, phòng họp chung.',
        'available', 12000000, 40, 8, 0, 1, 0, 0, 1, 1),
    (6, 5, 'V02', 'Văn phòng 80m² có ban công',
        'Không gian mở, phù hợp 10–15 nhân sự.',
        'available', 20000000, 80, 15, 6, 0, 1, 0, 1, 1),
    (6, 5, 'V03', 'Văn phòng 20m² cho startup',
        'Phòng nhỏ, đầy đủ tiện ích, thuê linh hoạt theo tháng.',
        'occupied', 6500000, 20, 4, 4, 0, 0, 0, 1, 1),

    (7, 1, 'H101', 'Phòng trọ Cầu Giấy gần ĐH Quốc gia Hà Nội',
        'Phòng 22m², có gác xép, điều hòa, nóng lạnh.',
        'available', 2900000, 22, 2, 0, 0, 1, 0, 1, 1),
    (7, 6, 'H102', 'Pass phòng ở ghép Cầu Giấy – dọn vào ngay',
        'Cần 1 bạn ở ghép, phòng đẹp, giá chia đôi.',
        'available', 1500000, 15, 3, 1, 0, 0, 1, 1, 1),
    (7, 4, 'H201', 'Căn hộ mini Cầu Giấy 40m²',
        'Căn hộ mini đầy đủ nội thất, gần trường học và công ty.',
        'available', 6000000, 40, 3, 1, 0, 0, 0, 1, 1),

    (8, 1, 'D01', 'Phòng trọ Hải Châu gần biển',
        'Phòng 20m² thoáng mát, cách biển khoảng 1,5 km.',
        'available', 2000000, 20, 2, 0, 0, 0, 0, 1, 1),
    (8, 4, 'D02', 'Căn hộ Đà Nẵng 45m² view sông Hàn',
        'Căn hộ đầy đủ nội thất, gần cầu Rồng.',
        'available', 5000000, 45, 3, 1, 1, 0, 0, 1, 1);

-- Ảnh đại diện mặc định theo loại hình (file nằm trong static/img/types)
UPDATE rooms r
    JOIN room_types t ON t.type_id = r.type_id
SET r.thumbnail_url = CONCAT('/img/types/', t.slug, '.svg');

-- Tổng số phòng của mỗi tòa nhà (không tính phòng đã ẩn)
UPDATE buildings b
SET b.total_rooms = (SELECT COUNT(*) FROM rooms r WHERE r.building_id = b.building_id AND r.status <> 'hidden');

-- Tiện nghi theo luật đơn giản (người quản trị có thể chỉnh lại trong trang admin)
INSERT INTO room_amenities (room_id, amenity_id)
SELECT r.room_id, a.amenity_id
FROM rooms r
         JOIN amenities a
WHERE (a.name = 'Wifi')
   OR (a.name = 'Chỗ để xe')
   OR (a.name = 'Máy lạnh'        AND (r.type_id IN (3, 4, 5) OR r.base_rent_price >= 3000000))
   OR (a.name = 'Nóng lạnh'       AND r.type_id IN (1, 4))
   OR (a.name = 'Gác lửng'        AND r.type_id = 1 AND r.area_m2 >= 20)
   OR (a.name = 'Máy giặt'        AND (r.type_id IN (3, 4) OR r.building_id IN (2, 3)))
   OR (a.name = 'Bảo vệ 24/7'     AND r.type_id IN (2, 4, 5))
   OR (a.name = 'Thang máy'       AND r.type_id IN (4, 5))
   OR (a.name = 'Nội thất đầy đủ' AND (r.type_id = 4 OR r.base_rent_price >= 3500000))
   OR (a.name = 'Bếp riêng'       AND r.type_id IN (3, 4))
   OR (a.name = 'Ban công'        AND r.type_id IN (1, 4) AND r.area_m2 >= 22)
   OR (a.name = 'Giờ giấc tự do'  AND r.type_id IN (1, 2, 6))
   OR (a.name = 'Camera an ninh'  AND r.building_id IN (1, 2, 4, 6));

-- Người thuê, xe, hợp đồng
INSERT INTO tenants (tenant_id, user_id, full_name, phone, email, id_number) VALUES
    (1, 3,    'Minh Anh',        '0901234567', 'minhanh@gmail.com',   '079200001234'),
    (2, 4,    'Daniel Lee',      '0912345678', 'daniel@example.com',  'P1234567'),
    (3, NULL, 'Trần Văn Bình',   '0987654321', NULL,                  '079198005678'),
    (4, NULL, 'Nguyễn Thu Hà',   '0933444555', 'thuha@example.com',   '079199009999');

INSERT INTO vehicles (vehicle_id, tenant_id, license_plate, vehicle_type) VALUES
    (1, 1, '59A-123.45',  'motorbike'),
    (2, 2, '51H-888.88',  'car'),
    (3, 3, '59F1-678.90', 'motorbike');

INSERT INTO contracts
    (contract_id, room_id, tenant_id, start_date, end_date, stay_type, booking_source, deposit_amount, status, monthly_rent, note)
VALUES
    (1, (SELECT room_id FROM rooms WHERE room_number = 'P102'),  3, '2026-01-01', '2027-01-01', 'long_term',  'direct',  2800000, 'active', 2800000, 'Hợp đồng 12 tháng'),
    (2, (SELECT room_id FROM rooms WHERE room_number = 'A2.05'), 1, '2026-03-01', '2027-03-01', 'long_term',  'direct',  9500000, 'active', 9500000, NULL),
    (3, (SELECT room_id FROM rooms WHERE room_number = 'S1.07'), 2, '2026-10-12', '2026-10-31', 'short_term', 'airbnb',  2000000, 'active', 7500000, 'Khách Airbnb'),
    (4, (SELECT room_id FROM rooms WHERE room_number = 'P201'),  4, '2025-10-01', '2026-04-01', 'long_term',  'direct',  3000000, 'ended',  3000000, 'Đã trả phòng');

-- Yêu cầu / sự cố
INSERT INTO tickets (room_id, tenant_id, category, status, title, description, resolved_at) VALUES
    ((SELECT room_id FROM rooms WHERE room_number = 'A2.05'), 1, 'maintenance',    'in_progress', 'Điều hòa không mát',        'Điều hòa phòng ngủ chạy nhưng không mát, nhờ kỹ thuật kiểm tra.', NULL),
    ((SELECT room_id FROM rooms WHERE room_number = 'P102'),  3, 'hygiene',        'done',        'Dọn vệ sinh hành lang',     'Hành lang tầng 1 có rác chưa được dọn.',                           '2026-10-02 09:00:00'),
    ((SELECT room_id FROM rooms WHERE room_number = 'S1.07'), 2, 'electric_water', 'open',        'Mất nước buổi sáng',        'Sáng nay phòng bị mất nước khoảng 2 tiếng.',                       NULL);

-- Chốt điện nước
INSERT INTO meter_readings (room_id, billing_month, old_electric, new_electric, old_water, new_water) VALUES
    ((SELECT room_id FROM rooms WHERE room_number = 'P102'),  '2026-08', 1100, 1200, 92,  100),
    ((SELECT room_id FROM rooms WHERE room_number = 'P102'),  '2026-09', 1200, 1290, 100, 108),
    ((SELECT room_id FROM rooms WHERE room_number = 'A2.05'), '2026-09', 2000, 2150, 50,  60),
    ((SELECT room_id FROM rooms WHERE room_number = 'S1.07'), '2026-10', 500,  540,  20,  23);

-- Dịch vụ / loại phí
INSERT INTO services (service_id, service_name, calc_type, unit, unit_price) VALUES
    (1, 'Tiền thuê phòng', 'RENT',           'tháng', 0),
    (2, 'Điện',            'METER_ELECTRIC', 'kWh',   3500),
    (3, 'Nước',            'METER_WATER',    'm³',    20000),
    (4, 'Internet / Wifi', 'FIXED',          'tháng', 100000),
    (5, 'Phí dọn dẹp',     'FIXED',          'tháng', 150000),
    (6, 'Gửi xe',          'PER_VEHICLE',    'xe',    100000);

-- Hóa đơn + chi tiết + thanh toán
INSERT INTO invoices (invoice_id, contract_id, billing_month, total_amount, status, due_date, invoice_code, issued_at) VALUES
    (1, 1, '2026-09', 3625000,  'paid',    '2026-10-05', 'INV-202609-0001', '2026-10-01 08:00:00'),
    (2, 2, '2026-09', 10575000, 'partial', '2026-10-05', 'INV-202609-0002', '2026-10-01 08:00:00'),
    (3, 1, '2026-10', 3150000,  'unpaid',  '2026-11-05', 'INV-202610-0001', '2026-10-10 08:00:00');

INSERT INTO invoice_details (invoice_id, service_id, description, quantity, unit_price, subtotal) VALUES
    (1, 1, 'Tiền thuê phòng 09/2026',        1,   2800000, 2800000),
    (1, 2, 'Điện 09/2026 (1200 → 1290)',     90,  3500,    315000),
    (1, 3, 'Nước 09/2026 (100 → 108)',       8,   20000,   160000),
    (1, 4, 'Internet / Wifi',                1,   100000,  100000),
    (1, 5, 'Phí dọn dẹp',                    1,   150000,  150000),
    (1, 6, 'Gửi xe (1 xe)',                  1,   100000,  100000),
    (2, 1, 'Tiền thuê phòng 09/2026',        1,   9500000, 9500000),
    (2, 2, 'Điện 09/2026 (2000 → 2150)',     150, 3500,    525000),
    (2, 3, 'Nước 09/2026 (50 → 60)',         10,  20000,   200000),
    (2, 4, 'Internet / Wifi',                1,   100000,  100000),
    (2, 5, 'Phí dọn dẹp',                    1,   150000,  150000),
    (2, 6, 'Gửi xe (1 xe)',                  1,   100000,  100000),
    (3, 1, 'Tiền thuê phòng 10/2026',        1,   2800000, 2800000),
    (3, 4, 'Internet / Wifi',                1,   100000,  100000),
    (3, 5, 'Phí dọn dẹp',                    1,   150000,  150000),
    (3, 6, 'Gửi xe (1 xe)',                  1,   100000,  100000);

INSERT INTO payments (invoice_id, amount_paid, paid_at, method, note) VALUES
    (1, 3625000, '2026-10-03 10:15:00', 'bank_transfer', 'Chuyển khoản đủ tiền tháng 09'),
    (2, 5000000, '2026-10-06 18:30:00', 'cash',          'Trả trước một phần');

-- Chương trình & ưu đãi (thời gian tính từ lúc nạp dữ liệu để đồng hồ đếm ngược luôn còn hiệu lực)
INSERT INTO promotions (promotion_id, title, description, promo_type, discount_text, voucher_code, link_url, banner_color, start_at, end_at, sort_order) VALUES
    (1, 'Thuê nhanh – nhận ngay voucher 200K',      'Đặt cọc phòng qua NhàTrọ+ trong tháng này và nhận voucher 200.000đ.', 'program', 'Voucher 200K', NULL,        '/search.html?isNow=true', 'orange', DATE_ADD(NOW(), INTERVAL -5 DAY), DATE_ADD(NOW(), INTERVAL 30 DAY), 1),
    (2, 'Sinh viên 2026 – giảm 10% tiền cọc',       'Dành cho sinh viên có thẻ sinh viên, áp dụng cho phòng trọ và ký túc xá.', 'program', '-10% cọc',  NULL,        '/search.html?typeId=2',   'blue',   DATE_ADD(NOW(), INTERVAL -10 DAY), DATE_ADD(NOW(), INTERVAL 60 DAY), 2),
    (3, 'Chủ nhà mới – đăng tin miễn phí 3 tháng',  'Chủ nhà đăng ký mới được đăng tin và quản lý phòng miễn phí 3 tháng đầu.', 'program', 'Miễn phí',   NULL,        '/login.html?role=landlord','green', DATE_ADD(NOW(), INTERVAL -2 DAY), DATE_ADD(NOW(), INTERVAL 90 DAY), 3),
    (4, 'HOTDEAL cuối tuần – giảm đến 500K',        'Những phòng giá tốt nhất trong tuần, số lượng có hạn.',                   'hotdeal', 'Đến -500K', NULL,        '/search.html?hotdeal=true','red',   DATE_ADD(NOW(), INTERVAL -1 DAY), DATE_ADD(NOW(), INTERVAL 2 DAY), 1),
    (5, 'Flash sale phòng trọ Gò Vấp',              'Giảm sâu cho phòng khu vực Gò Vấp, kết thúc sớm.',                         'hotdeal', 'Giá sốc',   NULL,        '/search.html?districtId=5','red',   DATE_ADD(NOW(), INTERVAL -2 HOUR), DATE_ADD(NOW(), INTERVAL 30 HOUR), 2),
    (6, 'Mã NHATRO50 – giảm 50K tiền phòng tháng đầu', 'Nhập mã khi ký hợp đồng để được giảm 50.000đ.',                        'voucher', '-50K',      'NHATRO50',  NULL,                      'purple', DATE_ADD(NOW(), INTERVAL -3 DAY), DATE_ADD(NOW(), INTERVAL 45 DAY), 1),
    (7, 'Đổi gas ưu đãi',                           'Đối tác cung cấp gas giao tận phòng, giảm 15% cho khách thuê trọ.',        'partner', '-15%',      NULL,        NULL,                      'yellow', DATE_ADD(NOW(), INTERVAL -7 DAY), DATE_ADD(NOW(), INTERVAL 120 DAY), 1);

INSERT INTO promotion_rooms (promotion_id, room_id, deal_price) VALUES
    (4, (SELECT room_id FROM rooms WHERE room_number = 'P101'),  2200000),
    (4, (SELECT room_id FROM rooms WHERE room_number = 'M303'),  4900000),
    (4, (SELECT room_id FROM rooms WHERE room_number = 'K103'),  950000),
    (4, (SELECT room_id FROM rooms WHERE room_number = 'H101'),  2600000),
    (5, (SELECT room_id FROM rooms WHERE room_number = 'P201'),  2700000),
    (5, (SELECT room_id FROM rooms WHERE room_number = 'P103'),  1000000);

-- Phòng yêu thích, yêu cầu hỗ trợ, đơn môi giới, yêu cầu xem phòng
INSERT INTO favorites (user_id, room_id) VALUES
    (3, (SELECT room_id FROM rooms WHERE room_number = 'P101')),
    (3, (SELECT room_id FROM rooms WHERE room_number = 'M303')),
    (3, (SELECT room_id FROM rooms WHERE room_number = 'K101'));

INSERT INTO support_requests (user_id, full_name, phone, email, request_type, desired_area, budget_max, note, status) VALUES
    (NULL, 'Nguyễn Văn An', '0911222333', 'an@example.com', 'find_room', 'Gò Vấp, Bình Thạnh', 3000000, 'Cần phòng cho 2 người, có chỗ để xe máy.', 'new');

INSERT INTO broker_applications (full_name, phone, email, work_area, experience, status) VALUES
    ('Phạm Quốc Huy', '0944555666', 'huy@example.com', 'Quận 7, Thủ Đức', 'Đã làm môi giới nhà trọ 2 năm.', 'new');

INSERT INTO viewing_requests (room_id, user_id, full_name, phone, message, preferred_time, status) VALUES
    ((SELECT room_id FROM rooms WHERE room_number = 'P101'), NULL, 'Hoàng Mai', '0900111222', 'Cho mình xem phòng vào cuối tuần nhé.', 'Thứ 7, buổi sáng', 'new');

-- Cấu hình website
INSERT INTO site_settings (setting_key, setting_value) VALUES
    ('site_name',  'NhàTrọ+'),
    ('tagline',    'Tìm phòng trọ, ký túc xá, nhà, chung cư nhanh chóng'),
    ('hotline',    '1900 6868'),
    ('email',      'hotro@nhatro.vn'),
    ('address',    'TP. Hồ Chí Minh, Việt Nam'),
    ('zalo',       '0900000002');
