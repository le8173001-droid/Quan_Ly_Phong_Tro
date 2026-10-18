-- Cấu trúc database SQLite của NhàTrọ+ (nguồn chính mà ứng dụng C++ dùng).
-- Thứ tự nạp khi tạo database mới: schema.sqlite.sql -> seed_vn_admin.sql (địa giới VN) -> seed_demo.sql (dữ liệu mẫu).
-- (schema.mysql.sql chỉ là bản ERD gốc để tham khảo, không còn đồng bộ.)
CREATE TABLE IF NOT EXISTS roles (
    role_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    role_code VARCHAR(20)  NOT NULL,
    role_name VARCHAR(100) NOT NULL,
    UNIQUE (role_code)
);

CREATE TABLE IF NOT EXISTS users (
    user_id       INTEGER PRIMARY KEY AUTOINCREMENT,
    role_id       INT          NOT NULL,
    username      VARCHAR(50)  NOT NULL,
    password_hash VARCHAR(255) NULL,
    status        VARCHAR(20)  NOT NULL DEFAULT 'active',      
    
    full_name     VARCHAR(100) NOT NULL,
    email         VARCHAR(100) NULL,
    phone         VARCHAR(20)  NULL,
    avatar_url    VARCHAR(500) NULL,
    id_number     VARCHAR(20)  NULL,
    birth_date    DATE         NULL,
    id_issue_date DATE         NULL,
    id_issue_place VARCHAR(150) NULL,
    address       VARCHAR(255) NULL,
    bank_name     VARCHAR(100) NULL,
    bank_account  VARCHAR(40)  NULL,
    last_login_at DATETIME     NULL,
    created_at    DATETIME     NOT NULL DEFAULT (datetime('now','localtime')),
    updated_at    DATETIME     NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (username),
    UNIQUE (email),
    CONSTRAINT fk_users_role FOREIGN KEY (role_id) REFERENCES roles (role_id),
    CONSTRAINT chk_users_status CHECK (status IN ('active', 'locked'))
);

CREATE TABLE IF NOT EXISTS user_identities (
    identity_id      INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id          INT          NOT NULL,
    provider         VARCHAR(20)  NOT NULL,                    
    provider_user_id VARCHAR(100) NOT NULL,
    email            VARCHAR(100) NULL,
    created_at       DATETIME     NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (provider, provider_user_id),
    UNIQUE (user_id, provider),
    CONSTRAINT fk_identity_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE CASCADE
);

-- Địa giới hành chính Việt Nam, có CẢ HAI hệ:
--   adm_system = 'NEW': 34 tỉnh/thành, 2 cấp (tỉnh -> phường/xã), áp dụng từ 01/07/2025 (cập nhật theo các nghị quyết mới nhất)
--   adm_system = 'OLD': 63 tỉnh/thành, 3 cấp (tỉnh -> quận/huyện -> phường/xã), trước 01/07/2025
CREATE TABLE IF NOT EXISTS provinces (
    province_id INTEGER PRIMARY KEY AUTOINCREMENT,
    adm_system  VARCHAR(3)   NOT NULL DEFAULT 'NEW',
    code        VARCHAR(10)  NULL,
    name        VARCHAR(100) NOT NULL,
    full_name   VARCHAR(120) NULL,
    UNIQUE (adm_system, name),
    CONSTRAINT chk_provinces_system CHECK (adm_system IN ('NEW', 'OLD'))
);

CREATE TABLE IF NOT EXISTS districts (
    district_id INTEGER PRIMARY KEY AUTOINCREMENT,
    province_id INT          NOT NULL,
    code        VARCHAR(10)  NULL,
    name        VARCHAR(100) NOT NULL,
    UNIQUE (province_id, name),
    CONSTRAINT fk_districts_province FOREIGN KEY (province_id) REFERENCES provinces (province_id)
);

CREATE TABLE IF NOT EXISTS wards (
    ward_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    province_id INT          NOT NULL,
    district_id INT          NULL,                             -- NULL với hệ NEW (không có cấp quận/huyện)
    code        VARCHAR(10)  NULL,
    name        VARCHAR(100) NOT NULL,
    CONSTRAINT fk_wards_province FOREIGN KEY (province_id) REFERENCES provinces (province_id),
    CONSTRAINT fk_wards_district FOREIGN KEY (district_id) REFERENCES districts (district_id)
);

CREATE TABLE IF NOT EXISTS buildings (
    building_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    manager_id    INT           NOT NULL,
    name          VARCHAR(150)  NOT NULL,
    address       VARCHAR(255)  NOT NULL,
    total_rooms   INT           NOT NULL DEFAULT 0,
    
    province_id   INT           NOT NULL,
    district_id   INT           NULL,                      
    ward_id       INT           NULL,
    cover_url     VARCHAR(500)  NULL,
    latitude      DECIMAL(9, 6) NULL,
    longitude     DECIMAL(9, 6) NULL,
    contact_name  VARCHAR(100)  NULL,
    contact_phone VARCHAR(20)   NULL,
    description   VARCHAR(500)  NULL,
    created_at    DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    updated_at    DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    CONSTRAINT fk_buildings_manager  FOREIGN KEY (manager_id) REFERENCES users (user_id),
    CONSTRAINT fk_buildings_province FOREIGN KEY (province_id) REFERENCES provinces (province_id),
    CONSTRAINT fk_buildings_district FOREIGN KEY (district_id) REFERENCES districts (district_id),
    CONSTRAINT fk_buildings_ward     FOREIGN KEY (ward_id)     REFERENCES wards (ward_id),
    CONSTRAINT chk_buildings_total   CHECK (total_rooms >= 0)
);

CREATE TABLE IF NOT EXISTS building_images (
    image_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    building_id INT          NOT NULL,
    url         VARCHAR(500) NOT NULL,
    sort_order  INT          NOT NULL DEFAULT 0,
    created_at  DATETIME     NOT NULL DEFAULT (datetime('now','localtime')),
    CONSTRAINT fk_building_images_building FOREIGN KEY (building_id) REFERENCES buildings (building_id) ON DELETE CASCADE
);

-- Mỗi ảnh tải lên từ thiết bị: file lưu trong thư mục data/uploads, thông tin lưu ở đây (url dùng cho room_images / building_images)
CREATE TABLE IF NOT EXISTS uploads (
    upload_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id     INT          NOT NULL,
    file_name   VARCHAR(255) NOT NULL,
    stored_name VARCHAR(100) NOT NULL,
    mime_type   VARCHAR(50)  NOT NULL,
    size_bytes  INT          NOT NULL,
    url         VARCHAR(200) NOT NULL,
    created_at  DATETIME     NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (stored_name),
    CONSTRAINT fk_uploads_user FOREIGN KEY (user_id) REFERENCES users (user_id)
);

CREATE TABLE IF NOT EXISTS landmarks (
    landmark_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    name          VARCHAR(150)  NOT NULL,
    landmark_type VARCHAR(20)   NOT NULL,                      
    district_id   INT           NULL,
    address       VARCHAR(255)  NULL,
    latitude      DECIMAL(9, 6) NOT NULL,
    longitude     DECIMAL(9, 6) NOT NULL,
    UNIQUE (name),
    CONSTRAINT fk_landmarks_district FOREIGN KEY (district_id) REFERENCES districts (district_id),
    CONSTRAINT chk_landmarks_type CHECK (landmark_type IN ('school', 'work'))
);

CREATE TABLE IF NOT EXISTS room_types (
    type_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    type_name   VARCHAR(100) NOT NULL,
    slug        VARCHAR(50)  NOT NULL,
    icon        VARCHAR(30)  NULL,                             
    description VARCHAR(255) NULL,
    sort_order  INT          NOT NULL DEFAULT 0,
    UNIQUE (type_name),
    UNIQUE (slug)
);

CREATE TABLE IF NOT EXISTS rooms (
    room_id           INTEGER PRIMARY KEY AUTOINCREMENT,
    building_id       INT           NOT NULL,
    type_id           INT           NOT NULL,
    room_number       VARCHAR(20)   NOT NULL,
    status            VARCHAR(20)   NOT NULL DEFAULT 'available',
    base_rent_price   DECIMAL(12,0) NOT NULL DEFAULT 0,        
    
    title             VARCHAR(200)  NOT NULL,
    description       TEXT          NULL,
    area_m2           DECIMAL(7,1)  NULL,                      
    max_occupants     INT           NOT NULL DEFAULT 2,        
    current_occupants INT           NOT NULL DEFAULT 0,        
    is_hot            TINYINT(1)    NOT NULL DEFAULT 0,
    is_new            TINYINT(1)    NOT NULL DEFAULT 0,
    is_now            TINYINT(1)    NOT NULL DEFAULT 0,        
    thumbnail_url     VARCHAR(500)  NULL,
    view_count        INT           NOT NULL DEFAULT 0,
    is_featured       TINYINT(1)    NOT NULL DEFAULT 0,        
    featured_order    INT           NOT NULL DEFAULT 0,

    
    create_date       DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    update_date       DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    create_by         INT           NOT NULL,
    update_by         INT           NOT NULL,
    UNIQUE (building_id, room_number),
    CONSTRAINT fk_rooms_building  FOREIGN KEY (building_id) REFERENCES buildings (building_id),
    CONSTRAINT fk_rooms_type      FOREIGN KEY (type_id)     REFERENCES room_types (type_id),
    CONSTRAINT fk_rooms_create_by FOREIGN KEY (create_by)   REFERENCES users (user_id),
    CONSTRAINT fk_rooms_update_by FOREIGN KEY (update_by)   REFERENCES users (user_id),
    CONSTRAINT chk_rooms_price     CHECK (base_rent_price >= 0),
    CONSTRAINT chk_rooms_area      CHECK (area_m2 IS NULL OR area_m2 > 0),
    CONSTRAINT chk_rooms_occupants CHECK (current_occupants >= 0 AND current_occupants <= max_occupants),
    CONSTRAINT chk_rooms_status    CHECK (status IN ('available', 'occupied', 'maintenance', 'hidden'))
);

CREATE TABLE IF NOT EXISTS amenities (
    amenity_id INTEGER PRIMARY KEY AUTOINCREMENT,
    name       VARCHAR(100) NOT NULL,
    icon       VARCHAR(30)  NULL,
    UNIQUE (name)
);

CREATE TABLE IF NOT EXISTS room_amenities (
    room_id    INT NOT NULL,
    amenity_id INT NOT NULL,
    PRIMARY KEY (room_id, amenity_id),
    CONSTRAINT fk_room_amenities_room    FOREIGN KEY (room_id)    REFERENCES rooms (room_id)       ON DELETE CASCADE,
    CONSTRAINT fk_room_amenities_amenity FOREIGN KEY (amenity_id) REFERENCES amenities (amenity_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS room_images (
    image_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id    INT          NOT NULL,
    url        VARCHAR(500) NOT NULL,
    sort_order INT          NOT NULL DEFAULT 0,
    created_at DATETIME     NOT NULL DEFAULT (datetime('now','localtime')),
    CONSTRAINT fk_room_images_room FOREIGN KEY (room_id) REFERENCES rooms (room_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS favorites (
    user_id    INT      NOT NULL,
    room_id    INT      NOT NULL,
    created_at DATETIME NOT NULL DEFAULT (datetime('now','localtime')),
    PRIMARY KEY (user_id, room_id),
    CONSTRAINT fk_favorites_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE CASCADE,
    CONSTRAINT fk_favorites_room FOREIGN KEY (room_id) REFERENCES rooms (room_id) ON DELETE CASCADE
);

CREATE TABLE IF NOT EXISTS tenants (
    tenant_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id    INT          NULL,
    full_name  VARCHAR(100) NOT NULL,
    phone      VARCHAR(20)  NOT NULL,
    email      VARCHAR(100) NULL,
    id_number  VARCHAR(20)  NULL,                              
    room_id    INT          NULL,                              
    birth_date DATE         NULL,
    gender     VARCHAR(10)  NULL,
    id_issue_date  DATE         NULL,
    id_issue_place VARCHAR(150) NULL,
    permanent_address VARCHAR(255) NULL,
    created_at DATETIME     NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (user_id),
    CONSTRAINT fk_tenants_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE SET NULL,
    CONSTRAINT fk_tenants_room FOREIGN KEY (room_id) REFERENCES rooms (room_id) ON DELETE SET NULL
);

CREATE TABLE IF NOT EXISTS vehicles (
    vehicle_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    tenant_id     INT         NOT NULL,
    license_plate VARCHAR(20) NOT NULL,
    vehicle_type  VARCHAR(20) NOT NULL DEFAULT 'motorbike',    
    brand_color   VARCHAR(60) NULL,
    UNIQUE (license_plate),
    CONSTRAINT fk_vehicles_tenant FOREIGN KEY (tenant_id) REFERENCES tenants (tenant_id),
    CONSTRAINT chk_vehicles_type CHECK (vehicle_type IN ('motorbike', 'car', 'bicycle'))
);

CREATE TABLE IF NOT EXISTS contracts (
    contract_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id        INT           NOT NULL,
    tenant_id      INT           NOT NULL,
    start_date     DATE          NOT NULL,
    end_date       DATE          NOT NULL,
    stay_type      VARCHAR(20)   NOT NULL DEFAULT 'long_term',
    booking_source VARCHAR(20)   NOT NULL DEFAULT 'direct',
    deposit_amount DECIMAL(12,0) NOT NULL DEFAULT 0,
    status         VARCHAR(20)   NOT NULL DEFAULT 'active',
    
    monthly_rent   DECIMAL(12,0) NOT NULL DEFAULT 0,
    note           VARCHAR(500)  NULL,
    contract_code  VARCHAR(30)   NULL,
    sign_date      DATE          NULL,
    sign_place     VARCHAR(150)  NULL,
    payment_day    INT           NOT NULL DEFAULT 5,
    payment_method VARCHAR(100)  NOT NULL DEFAULT 'Tiền mặt hoặc chuyển khoản',
    late_fee_percent DECIMAL(5,2) NOT NULL DEFAULT 0,
    notice_days    INT           NOT NULL DEFAULT 30,
    electric_price DECIMAL(12,0) NULL,
    water_price    DECIMAL(12,0) NULL,
    service_note   VARCHAR(500)  NULL,
    assets_text    TEXT          NULL,
    extra_terms    TEXT          NULL,
    created_at     DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (contract_code),
    CONSTRAINT fk_contracts_room   FOREIGN KEY (room_id)   REFERENCES rooms (room_id),
    CONSTRAINT fk_contracts_tenant FOREIGN KEY (tenant_id) REFERENCES tenants (tenant_id),
    CONSTRAINT chk_contracts_dates   CHECK (end_date >= start_date),
    CONSTRAINT chk_contracts_payday  CHECK (payment_day BETWEEN 1 AND 31),
    CONSTRAINT chk_contracts_stay    CHECK (stay_type IN ('short_term', 'long_term')),
    CONSTRAINT chk_contracts_source  CHECK (booking_source IN ('direct', 'airbnb', 'booking', 'agent')),
    CONSTRAINT chk_contracts_status  CHECK (status IN ('pending', 'active', 'ended', 'cancelled'))
);

CREATE TABLE IF NOT EXISTS tickets (
    ticket_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id     INT          NOT NULL,
    tenant_id   INT          NOT NULL,
    category    VARCHAR(30)  NOT NULL,
    status      VARCHAR(20)  NOT NULL DEFAULT 'open',
    
    title       VARCHAR(200) NOT NULL,
    description VARCHAR(1000) NULL,
    created_at  DATETIME     NOT NULL DEFAULT (datetime('now','localtime')),
    resolved_at DATETIME     NULL,
    CONSTRAINT fk_tickets_room   FOREIGN KEY (room_id)   REFERENCES rooms (room_id),
    CONSTRAINT fk_tickets_tenant FOREIGN KEY (tenant_id) REFERENCES tenants (tenant_id),
    CONSTRAINT chk_tickets_category CHECK (category IN ('hygiene', 'maintenance', 'electric_water', 'security', 'other')),
    CONSTRAINT chk_tickets_status   CHECK (status IN ('open', 'in_progress', 'done', 'cancelled'))
);

CREATE TABLE IF NOT EXISTS meter_readings (
    reading_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id       INT         NOT NULL,
    billing_month CHAR(7)     NOT NULL,
    old_electric  INT         NOT NULL DEFAULT 0,
    new_electric  INT         NOT NULL DEFAULT 0,
    
    old_water     INT         NOT NULL DEFAULT 0,
    new_water     INT         NOT NULL DEFAULT 0,
    recorded_at   DATETIME    NOT NULL DEFAULT (datetime('now','localtime')),
    UNIQUE (room_id, billing_month),
    CONSTRAINT fk_meter_room FOREIGN KEY (room_id) REFERENCES rooms (room_id),
    CONSTRAINT chk_meter_electric CHECK (new_electric >= old_electric),
    CONSTRAINT chk_meter_water    CHECK (new_water >= old_water)
);

CREATE TABLE IF NOT EXISTS services (
    service_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    service_name VARCHAR(100)  NOT NULL,
    
    calc_type    VARCHAR(20)   NOT NULL DEFAULT 'FIXED',
    unit         VARCHAR(30)   NOT NULL DEFAULT 'tháng',
    unit_price   DECIMAL(12,0) NOT NULL DEFAULT 0,
    is_active    TINYINT(1)    NOT NULL DEFAULT 1,
    UNIQUE (service_name),
    CONSTRAINT chk_services_calc CHECK (calc_type IN ('RENT', 'METER_ELECTRIC', 'METER_WATER', 'PER_VEHICLE', 'FIXED')),
    CONSTRAINT chk_services_price CHECK (unit_price >= 0)
);

CREATE TABLE IF NOT EXISTS invoices (
    invoice_id    INTEGER PRIMARY KEY AUTOINCREMENT,
    contract_id   INT           NOT NULL,
    billing_month CHAR(7)       NOT NULL,
    total_amount  DECIMAL(12,0) NOT NULL DEFAULT 0,
    status        VARCHAR(20)   NOT NULL DEFAULT 'unpaid',
    due_date      DATE          NOT NULL,
    
    invoice_code  VARCHAR(30)   NOT NULL,
    issued_at     DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    note          VARCHAR(500)  NULL,
    UNIQUE (invoice_code),
    UNIQUE (contract_id, billing_month),
    CONSTRAINT fk_invoices_contract FOREIGN KEY (contract_id) REFERENCES contracts (contract_id),
    CONSTRAINT chk_invoices_total  CHECK (total_amount >= 0),
    CONSTRAINT chk_invoices_status CHECK (status IN ('unpaid', 'partial', 'paid', 'cancelled'))
);

CREATE TABLE IF NOT EXISTS invoice_details (
    detail_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    invoice_id  INT           NOT NULL,
    service_id  INT           NOT NULL,
    subtotal    DECIMAL(12,0) NOT NULL DEFAULT 0,
    
    description VARCHAR(200)  NULL,
    quantity    DECIMAL(10,2) NOT NULL DEFAULT 1,
    unit_price  DECIMAL(12,0) NOT NULL DEFAULT 0,
    CONSTRAINT fk_invoice_details_invoice FOREIGN KEY (invoice_id) REFERENCES invoices (invoice_id) ON DELETE CASCADE,
    CONSTRAINT fk_invoice_details_service FOREIGN KEY (service_id) REFERENCES services (service_id)
);

CREATE TABLE IF NOT EXISTS payments (
    payment_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    invoice_id  INT           NOT NULL,
    amount_paid DECIMAL(12,0) NOT NULL,
    
    paid_at     DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    method      VARCHAR(20)   NOT NULL DEFAULT 'cash',
    note        VARCHAR(255)  NULL,
    CONSTRAINT fk_payments_invoice FOREIGN KEY (invoice_id) REFERENCES invoices (invoice_id),
    CONSTRAINT chk_payments_amount CHECK (amount_paid > 0),
    CONSTRAINT chk_payments_method CHECK (method IN ('cash', 'bank_transfer', 'momo', 'other'))
);

CREATE TABLE IF NOT EXISTS promotions (
    promotion_id  INTEGER PRIMARY KEY AUTOINCREMENT,
    title         VARCHAR(200)  NOT NULL,
    description   VARCHAR(500)  NULL,
    promo_type    VARCHAR(20)   NOT NULL DEFAULT 'program',
    discount_text VARCHAR(50)   NULL,                          
    discount_percent DECIMAL(5,2) NULL,                        
    voucher_code  VARCHAR(30)   NULL,
    link_url      VARCHAR(300)  NULL,
    banner_color  VARCHAR(20)   NULL,                          
    start_at      DATETIME      NOT NULL,
    end_at        DATETIME      NOT NULL,
    is_active     TINYINT(1)    NOT NULL DEFAULT 1,
    sort_order    INT           NOT NULL DEFAULT 0,
    created_at    DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    CONSTRAINT chk_promotions_type  CHECK (promo_type IN ('program', 'hotdeal', 'voucher', 'partner')),
    CONSTRAINT chk_promotions_dates CHECK (end_at > start_at)
);

CREATE TABLE IF NOT EXISTS promotion_rooms (
    promotion_id INT           NOT NULL,
    room_id      INT           NOT NULL,
    deal_price   DECIMAL(12,0) NOT NULL,
    PRIMARY KEY (promotion_id, room_id),
    CONSTRAINT fk_promo_rooms_promo FOREIGN KEY (promotion_id) REFERENCES promotions (promotion_id) ON DELETE CASCADE,
    CONSTRAINT fk_promo_rooms_room  FOREIGN KEY (room_id)      REFERENCES rooms (room_id)           ON DELETE CASCADE,
    CONSTRAINT chk_promo_rooms_price CHECK (deal_price >= 0)
);

CREATE TABLE IF NOT EXISTS support_requests (
    request_id   INTEGER PRIMARY KEY AUTOINCREMENT,
    user_id      INT           NULL,
    full_name    VARCHAR(100)  NOT NULL,
    phone        VARCHAR(20)   NOT NULL,
    email        VARCHAR(100)  NULL,
    request_type VARCHAR(20)   NOT NULL DEFAULT 'find_room',
    desired_area VARCHAR(150)  NULL,
    budget_max   DECIMAL(12,0) NULL,
    note         VARCHAR(1000) NULL,
    status       VARCHAR(20)   NOT NULL DEFAULT 'new',
    created_at   DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    CONSTRAINT fk_support_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE SET NULL,
    CONSTRAINT chk_support_type   CHECK (request_type IN ('find_room', 'landlord', 'other')),
    CONSTRAINT chk_support_status CHECK (status IN ('new', 'processing', 'done'))
);

CREATE TABLE IF NOT EXISTS broker_applications (
    application_id INTEGER PRIMARY KEY AUTOINCREMENT,
    full_name      VARCHAR(100)  NOT NULL,
    phone          VARCHAR(20)   NOT NULL,
    email          VARCHAR(100)  NULL,
    work_area      VARCHAR(150)  NULL,
    experience     VARCHAR(1000) NULL,
    status         VARCHAR(20)   NOT NULL DEFAULT 'new',
    created_at     DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    CONSTRAINT chk_broker_status CHECK (status IN ('new', 'approved', 'rejected'))
);

CREATE TABLE IF NOT EXISTS viewing_requests (
    viewing_id     INTEGER PRIMARY KEY AUTOINCREMENT,
    room_id        INT           NOT NULL,
    user_id        INT           NULL,
    full_name      VARCHAR(100)  NOT NULL,
    phone          VARCHAR(20)   NOT NULL,
    message        VARCHAR(1000) NULL,
    preferred_time VARCHAR(100)  NULL,
    status         VARCHAR(20)   NOT NULL DEFAULT 'new',
    created_at     DATETIME      NOT NULL DEFAULT (datetime('now','localtime')),
    CONSTRAINT fk_viewing_room FOREIGN KEY (room_id) REFERENCES rooms (room_id) ON DELETE CASCADE,
    CONSTRAINT fk_viewing_user FOREIGN KEY (user_id) REFERENCES users (user_id) ON DELETE SET NULL,
    CONSTRAINT chk_viewing_status CHECK (status IN ('new', 'contacted', 'done', 'cancelled'))
);

CREATE TABLE IF NOT EXISTS site_settings (
    setting_key   VARCHAR(50)  NOT NULL,
    setting_value VARCHAR(500) NOT NULL,
    PRIMARY KEY (setting_key)
);

CREATE INDEX IF NOT EXISTS idx_districts_province ON districts (province_id);
CREATE INDEX IF NOT EXISTS idx_wards_province ON wards (province_id);
CREATE INDEX IF NOT EXISTS idx_wards_district ON wards (district_id);
CREATE INDEX IF NOT EXISTS idx_buildings_district ON buildings (district_id);
CREATE INDEX IF NOT EXISTS idx_buildings_manager ON buildings (manager_id);
CREATE INDEX IF NOT EXISTS idx_building_images_building ON building_images (building_id);
CREATE INDEX IF NOT EXISTS idx_rooms_building ON rooms (building_id);
CREATE INDEX IF NOT EXISTS idx_rooms_featured ON rooms (is_featured, featured_order);
CREATE INDEX IF NOT EXISTS idx_rooms_views ON rooms (view_count);
CREATE INDEX IF NOT EXISTS idx_tenants_room ON tenants (room_id);
CREATE INDEX IF NOT EXISTS idx_buildings_province ON buildings (province_id);
CREATE INDEX IF NOT EXISTS idx_buildings_geo ON buildings (latitude, longitude);
CREATE INDEX IF NOT EXISTS idx_rooms_type ON rooms (type_id);
CREATE INDEX IF NOT EXISTS idx_rooms_price ON rooms (base_rent_price);
CREATE INDEX IF NOT EXISTS idx_rooms_area ON rooms (area_m2);
CREATE INDEX IF NOT EXISTS idx_rooms_status ON rooms (status);
CREATE INDEX IF NOT EXISTS idx_room_images_room ON room_images (room_id);
CREATE INDEX IF NOT EXISTS idx_tenants_phone ON tenants (phone);
CREATE INDEX IF NOT EXISTS idx_contracts_room ON contracts (room_id);
CREATE INDEX IF NOT EXISTS idx_contracts_tenant ON contracts (tenant_id);
CREATE INDEX IF NOT EXISTS idx_tickets_room ON tickets (room_id);
CREATE INDEX IF NOT EXISTS idx_tickets_status ON tickets (status);
CREATE INDEX IF NOT EXISTS idx_invoices_status ON invoices (status);
CREATE INDEX IF NOT EXISTS idx_invoice_details_invoice ON invoice_details (invoice_id);
CREATE INDEX IF NOT EXISTS idx_payments_invoice ON payments (invoice_id);
CREATE INDEX IF NOT EXISTS idx_promotions_active ON promotions (is_active, start_at, end_at);
CREATE INDEX IF NOT EXISTS idx_support_status ON support_requests (status);
CREATE INDEX IF NOT EXISTS idx_viewing_status ON viewing_requests (status);

INSERT INTO roles (role_id, role_code, role_name) VALUES
    (1, 'ADMIN',   'Quản trị viên'),
    (2, 'MANAGER', 'Quản lý vận hành'),
    (3, 'TENANT',  'Người thuê / Khách hàng');


INSERT INTO users (user_id, role_id, username, password_hash, status, full_name, email, phone) VALUES
    (1, 1, 'admin',   'pbkdf2_sha256$100000$61646d696e2d73616c74$112828e8e1b0c5ecd4bfa58e79a254a53d3d6910039c80e49fb8e664bb249e3f', 'active', 'Quản trị viên', 'admin@nhatro.vn',   '0900000001'),
    (2, 2, 'levinh',  'pbkdf2_sha256$100000$6d616e616765722d73616c74$ec548e8658101078f5053e95d95474099c2b104dc0e3c331e0b770ec50d900d7', 'active', 'Lê Vinh',       'levinh@nhatro.vn',  '0900000002'),
    (3, 3, 'minhanh', 'pbkdf2_sha256$100000$74656e616e742d73616c74$c690b92364f54667ee4cb10d4a4471c7164e99ba3be87fed863b01991422de76', 'active', 'Minh Anh',      'minhanh@gmail.com', '0901234567'),
    (4, 3, 'daniel',  'pbkdf2_sha256$100000$74656e616e742d73616c74$c690b92364f54667ee4cb10d4a4471c7164e99ba3be87fed863b01991422de76', 'active', 'Daniel Lee',    'daniel@example.com','0912345678');

INSERT INTO room_types (type_id, type_name, slug, icon, description, sort_order) VALUES
    (1, 'Phòng trọ',            'phong-tro',  'bed',       'Phòng trọ, nhà trọ cho sinh viên và người đi làm', 1),
    (2, 'Ký túc xá',            'ky-tuc-xa',  'dorm',      'Giường ký túc xá, phòng nhiều người ở',            2),
    (3, 'Nhà',                  'nha',        'house',     'Nhà nguyên căn cho thuê',                           3),
    (4, 'Chung cư',             'chung-cu',   'building',  'Căn hộ chung cư, căn hộ mini, studio',              4),
    (5, 'Văn phòng',            'van-phong',  'briefcase', 'Mặt bằng, văn phòng cho thuê',                      5),
    (6, 'Phòng pass & ở ghép',  'pass-o-ghep','users',     'Pass lại phòng, tìm người ở ghép',                  6);

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

INSERT INTO site_settings (setting_key, setting_value) VALUES
    ('site_name',  'NhàTrọ+'),
    ('tagline',    'Tìm phòng trọ, ký túc xá, nhà, chung cư nhanh chóng'),
    ('hotline',    '1900 6868'),
    ('email',      'hotro@nhatro.vn'),
    ('address',    'TP. Hồ Chí Minh, Việt Nam'),
    ('zalo',       '0900000002'),
    ('facebook',   'https://facebook.com/nhatro'),
    ('featured_mode',  'mixed'),
    ('featured_count', '4');
