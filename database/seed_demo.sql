-- Dữ liệu mẫu (chạy sau schema.sqlite.sql và seed_vn_admin.sql)
INSERT INTO buildings (building_id, manager_id, name, address, province_id, district_id, ward_id, latitude, longitude, contact_name, contact_phone, description) VALUES
    (1, 2, 'Nhà trọ Hoàng Gia', '123 Quang Trung', (SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh'), (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Gò Vấp'), (SELECT ward_id FROM wards WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND district_id=(SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Gò Vấp') AND name='Phường 12'), 10.838600, 106.665200, 'Lê Vinh', '0900000002', 'Khu trọ an ninh, có camera, giờ giấc tự do'),
    (2, 2, 'Saigon Pearl Residence', 'Nguyễn Hữu Cảnh', (SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh'), (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Bình Thạnh'), (SELECT ward_id FROM wards WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND district_id=(SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Bình Thạnh') AND name='Phường 22'), 10.795000, 106.721800, 'Lê Vinh', '0900000002', 'Căn hộ cao cấp ven sông Sài Gòn'),
    (3, 2, 'The Metropolis Studio', 'Nguyễn Thị Thập', (SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh'), (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận 7'), (SELECT ward_id FROM wards WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND district_id=(SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận 7') AND name='Phường Tân Phong'), 10.738000, 106.721000, 'Lê Vinh', '0900000002', 'Studio và căn hộ mini khu Nam Sài Gòn'),
    (4, 2, 'Ký túc xá Sinh viên Thủ Đức', 'Đường Linh Trung', (SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh'), (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Thành phố Thủ Đức'), (SELECT ward_id FROM wards WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND district_id=(SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Thành phố Thủ Đức') AND name='Phường Linh Trung'), 10.870000, 106.790000, 'Lê Vinh', '0900000002', 'Ký túc xá gần Đại học Quốc gia'),
    (5, 2, 'Nhà Phú Nhuận Green', '45 Phan Xích Long', (SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh'), (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Phú Nhuận'), (SELECT ward_id FROM wards WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND district_id=(SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Phú Nhuận') AND name='Phường 7'), 10.799000, 106.680000, 'Lê Vinh', '0900000002', 'Nhà nguyên căn và phòng trọ gần sân bay'),
    (6, 2, 'Central Office Nguyễn Huệ', '8 Nguyễn Huệ', (SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh'), (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận 1'), (SELECT ward_id FROM wards WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND district_id=(SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận 1') AND name='Phường Bến Nghé'), 10.774000, 106.704000, 'Lê Vinh', '0900000002', 'Văn phòng trung tâm Quận 1'),
    (7, 2, 'Cầu Giấy Home', 'Số 10 Trần Thái Tông', (SELECT province_id FROM provinces WHERE adm_system='NEW' AND name='Hà Nội'), NULL, (SELECT ward_id FROM wards WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='NEW' AND name='Hà Nội') AND name='Phường Cầu Giấy'), 21.030000, 105.790000, 'Lê Vinh', '0900000002', 'Phòng trọ và căn hộ mini gần ĐH Quốc gia Hà Nội'),
    (8, 2, 'Nhà trọ Biển Xanh', '56 Lê Duẩn', (SELECT province_id FROM provinces WHERE adm_system='NEW' AND name='Đà Nẵng'), NULL, (SELECT ward_id FROM wards WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='NEW' AND name='Đà Nẵng') AND name='Phường Hải Châu'), 16.060000, 108.220000, 'Lê Vinh', '0900000002', 'Nhà trọ và căn hộ trung tâm Đà Nẵng');

INSERT INTO landmarks (landmark_id, name, landmark_type, district_id, address, latitude, longitude) VALUES
    (1, 'Đại học Bách Khoa TP.HCM', 'school', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận 10'), '268 Lý Thường Kiệt', 10.773100, 106.660600),
    (2, 'Đại học Kinh tế TP.HCM', 'school', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận 3'), '59C Nguyễn Đình Chiểu', 10.782600, 106.695400),
    (3, 'Đại học Công nghiệp TP.HCM', 'school', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Gò Vấp'), '12 Nguyễn Văn Bảo', 10.822200, 106.686600),
    (4, 'Đại học Quốc gia TP.HCM', 'school', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Thành phố Thủ Đức'), 'Khu phố 6, Linh Trung', 10.870000, 106.803000),
    (5, 'Đại học Tôn Đức Thắng', 'school', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận 7'), '19 Nguyễn Hữu Thọ', 10.732400, 106.699400),
    (6, 'Landmark 81', 'work', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Bình Thạnh'), '720A Điện Biên Phủ', 10.795100, 106.721800),
    (7, 'Khu công nghệ cao TP.HCM', 'work', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Thành phố Thủ Đức'), 'Xa lộ Hà Nội', 10.853000, 106.791000),
    (8, 'Phú Mỹ Hưng', 'work', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận 7'), 'Nguyễn Lương Bằng', 10.729000, 106.719000),
    (9, 'Sân bay Tân Sơn Nhất', 'work', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hồ Chí Minh') AND name='Quận Tân Bình'), 'Trường Sơn', 10.818800, 106.651900),
    (10, 'Đại học Quốc gia Hà Nội', 'school', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hà Nội') AND name='Quận Cầu Giấy'), '144 Xuân Thủy', 21.037600, 105.782600),
    (11, 'Đại học Bách Khoa Hà Nội', 'school', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Hà Nội') AND name='Quận Hai Bà Trưng'), '1 Đại Cồ Việt', 21.005300, 105.843300),
    (12, 'Đại học Bách Khoa Đà Nẵng', 'school', (SELECT district_id FROM districts WHERE province_id=(SELECT province_id FROM provinces WHERE adm_system='OLD' AND name='Đà Nẵng') AND name='Quận Liên Chiểu'), '54 Nguyễn Lương Bằng', 16.073600, 108.149900);

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

UPDATE rooms SET thumbnail_url = '/img/types/' || (SELECT slug FROM room_types t WHERE t.type_id = rooms.type_id) || '.svg';

UPDATE buildings AS b
SET total_rooms = (SELECT COUNT(*) FROM rooms r WHERE r.building_id = b.building_id AND r.status <> 'hidden');

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

INSERT INTO tenants (tenant_id, user_id, full_name, phone, email, id_number, room_id, birth_date, gender, id_issue_date, id_issue_place, permanent_address) VALUES
    (1, 3,    'Minh Anh',        '0901234567', 'minhanh@gmail.com',   '079200001234', (SELECT room_id FROM rooms WHERE room_number = 'A2.05'), '2000-05-12', 'Nữ',  '2021-04-20', 'Cục Cảnh sát QLHC về TTXH', 'Xã Đại Thắng, Huyện Tiên Lãng, TP. Hải Phòng'),
    (2, 4,    'Daniel Lee',      '0912345678', 'daniel@example.com',  'P1234567',     (SELECT room_id FROM rooms WHERE room_number = 'S1.07'), '1995-09-03', 'Nam',  '2022-01-10', 'Hộ chiếu - Hàn Quốc',       'Seoul, Hàn Quốc'),
    (3, NULL, 'Trần Văn Bình',   '0987654321', NULL,                  '079198005678', (SELECT room_id FROM rooms WHERE room_number = 'P102'), '1998-02-27', 'Nam',  '2020-08-15', 'Cục Cảnh sát QLHC về TTXH', 'Phường Hòa Khánh, Quận Liên Chiểu, TP. Đà Nẵng'),
    (4, NULL, 'Nguyễn Thu Hà',   '0933444555', 'thuha@example.com',   '079199009999', NULL,                                                     '1999-11-30', 'Nữ',  '2019-12-05', 'Cục Cảnh sát QLHC về TTXH', 'Xã Tân Hiệp, Huyện Hóc Môn, TP. Hồ Chí Minh'),
    (5, NULL, 'Lê Thị Mai',      '0977888999', NULL,                  '079201112222', (SELECT room_id FROM rooms WHERE room_number = 'P102'), '2001-07-19', 'Nữ',  '2021-09-09', 'Cục Cảnh sát QLHC về TTXH', 'Xã Phước Hưng, Huyện Long Điền, Tỉnh Bà Rịa - Vũng Tàu');

INSERT INTO vehicles (vehicle_id, tenant_id, license_plate, vehicle_type) VALUES
    (1, 1, '59A-123.45',  'motorbike'),
    (2, 2, '51H-888.88',  'car'),
    (3, 3, '59F1-678.90', 'motorbike'),
    (4, 5, '59C2-111.22', 'motorbike');

INSERT INTO contracts
    (contract_id, room_id, tenant_id, start_date, end_date, stay_type, booking_source, deposit_amount, status, monthly_rent, note)
VALUES
    (1, (SELECT room_id FROM rooms WHERE room_number = 'P102'),  3, '2026-01-01', '2027-01-01', 'long_term',  'direct',  2800000, 'active', 2800000, 'Hợp đồng 12 tháng'),
    (2, (SELECT room_id FROM rooms WHERE room_number = 'A2.05'), 1, '2026-03-01', '2027-03-01', 'long_term',  'direct',  9500000, 'active', 9500000, NULL),
    (3, (SELECT room_id FROM rooms WHERE room_number = 'S1.07'), 2, '2026-10-12', '2026-10-31', 'short_term', 'airbnb',  2000000, 'active', 7500000, 'Khách Airbnb'),
    (4, (SELECT room_id FROM rooms WHERE room_number = 'P201'),  4, '2025-10-01', '2026-04-01', 'long_term',  'direct',  3000000, 'ended',  3000000, 'Đã trả phòng');

INSERT INTO tickets (room_id, tenant_id, category, status, title, description, resolved_at) VALUES
    ((SELECT room_id FROM rooms WHERE room_number = 'A2.05'), 1, 'maintenance',    'in_progress', 'Điều hòa không mát',        'Điều hòa phòng ngủ chạy nhưng không mát, nhờ kỹ thuật kiểm tra.', NULL),
    ((SELECT room_id FROM rooms WHERE room_number = 'P102'),  3, 'hygiene',        'done',        'Dọn vệ sinh hành lang',     'Hành lang tầng 1 có rác chưa được dọn.',                           '2026-10-02 09:00:00'),
    ((SELECT room_id FROM rooms WHERE room_number = 'S1.07'), 2, 'electric_water', 'open',        'Mất nước buổi sáng',        'Sáng nay phòng bị mất nước khoảng 2 tiếng.',                       NULL);

INSERT INTO meter_readings (room_id, billing_month, old_electric, new_electric, old_water, new_water) VALUES
    ((SELECT room_id FROM rooms WHERE room_number = 'P102'),  '2026-08', 1100, 1200, 92,  100),
    ((SELECT room_id FROM rooms WHERE room_number = 'P102'),  '2026-09', 1200, 1290, 100, 108),
    ((SELECT room_id FROM rooms WHERE room_number = 'A2.05'), '2026-09', 2000, 2150, 50,  60),
    ((SELECT room_id FROM rooms WHERE room_number = 'S1.07'), '2026-10', 500,  540,  20,  23);

INSERT INTO services (service_id, service_name, calc_type, unit, unit_price) VALUES
    (1, 'Tiền thuê phòng', 'RENT',           'tháng', 0),
    (2, 'Điện',            'METER_ELECTRIC', 'kWh',   3500),
    (3, 'Nước',            'METER_WATER',    'm³',    20000),
    (4, 'Internet / Wifi', 'FIXED',          'tháng', 100000),
    (5, 'Phí dọn dẹp',     'FIXED',          'tháng', 150000),
    (6, 'Gửi xe',          'PER_VEHICLE',    'xe',    100000);

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

INSERT INTO promotions (promotion_id, title, description, promo_type, discount_text, voucher_code, link_url, banner_color, start_at, end_at, sort_order) VALUES
    (1, 'Thuê nhanh – nhận ngay voucher 200K',      'Đặt cọc phòng qua NhàTrọ+ trong tháng này và nhận voucher 200.000đ.', 'program', 'Voucher 200K', NULL,        '/search.html?isNow=true', 'orange', datetime('now','localtime','-5 days'), datetime('now','localtime','30 days'), 1),
    (2, 'Sinh viên 2026 – giảm 10% tiền cọc',       'Dành cho sinh viên có thẻ sinh viên, áp dụng cho phòng trọ và ký túc xá.', 'program', '-10% cọc',  NULL,        '/search.html?typeId=2',   'blue',   datetime('now','localtime','-10 days'), datetime('now','localtime','60 days'), 2),
    (3, 'Chủ nhà mới – đăng tin miễn phí 3 tháng',  'Chủ nhà đăng ký mới được đăng tin và quản lý phòng miễn phí 3 tháng đầu.', 'program', 'Miễn phí',   NULL,        '/login.html?role=landlord','green', datetime('now','localtime','-2 days'), datetime('now','localtime','90 days'), 3),
    (4, 'HOTDEAL cuối tuần – giảm đến 500K',        'Những phòng giá tốt nhất trong tuần, số lượng có hạn.',                   'hotdeal', 'Đến -500K', NULL,        '/search.html?hotdeal=true','red',   datetime('now','localtime','-1 days'), datetime('now','localtime','2 days'), 1),
    (5, 'Flash sale phòng trọ Gò Vấp',              'Giảm sâu cho phòng khu vực Gò Vấp, kết thúc sớm.',                         'hotdeal', 'Giá sốc',   NULL,        '/search.html?districtId=5','red',   datetime('now','localtime','-2 hours'), datetime('now','localtime','30 hours'), 2),
    (6, 'Mã NHATRO50 – giảm 50K tiền phòng tháng đầu', 'Nhập mã khi ký hợp đồng để được giảm 50.000đ.',                        'voucher', '-50K',      'NHATRO50',  NULL,                      'purple', datetime('now','localtime','-3 days'), datetime('now','localtime','45 days'), 1),
    (7, 'Đổi gas ưu đãi',                           'Đối tác cung cấp gas giao tận phòng, giảm 15% cho khách thuê trọ.',        'partner', '-15%',      NULL,        NULL,                      'yellow', datetime('now','localtime','-7 days'), datetime('now','localtime','120 days'), 1);

INSERT INTO promotion_rooms (promotion_id, room_id, deal_price) VALUES
    (4, (SELECT room_id FROM rooms WHERE room_number = 'P101'),  2200000),
    (4, (SELECT room_id FROM rooms WHERE room_number = 'M303'),  4900000),
    (4, (SELECT room_id FROM rooms WHERE room_number = 'K103'),  950000),
    (4, (SELECT room_id FROM rooms WHERE room_number = 'H101'),  2600000),
    (5, (SELECT room_id FROM rooms WHERE room_number = 'P201'),  2700000),
    (5, (SELECT room_id FROM rooms WHERE room_number = 'P103'),  1000000);

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

-- Thông tin bổ sung: bên cho thuê (chủ tòa nhà) dùng cho hợp đồng
UPDATE users SET id_number = '079080001111', birth_date = '1980-03-15', id_issue_date = '2021-06-01',
       id_issue_place = 'Cục Cảnh sát QLHC về TTXH', address = '12 Nguyễn Văn Bảo, Phường 4, Quận Gò Vấp, TP. Hồ Chí Minh',
       bank_name = 'Vietcombank - CN TP.HCM', bank_account = '0071001234567' WHERE user_id = 2;

UPDATE contracts SET sign_date = start_date, sign_place = 'TP. Hồ Chí Minh', payment_day = 5, late_fee_percent = 2, notice_days = 30,
       contract_code = 'HD-' || strftime('%Y', start_date) || '-' || printf('%04d', contract_id),
       electric_price = 3500, water_price = 20000,
       service_note = 'Internet 100.000đ/tháng; phí dọn dẹp 150.000đ/tháng; gửi xe 100.000đ/xe/tháng.',
       assets_text = 'Giường | 1 | Tốt' || char(10) || 'Tủ quần áo | 1 | Tốt' || char(10) || 'Máy lạnh | 1 | Hoạt động bình thường' || char(10) || 'Chìa khóa phòng | 2 | Đủ';

-- Phòng nổi bật (admin ghim) + khuyến mãi theo phần trăm
UPDATE rooms SET is_featured = 1, featured_order = 1 WHERE room_number IN ('P101', 'K101', 'N01', 'A1.18');
UPDATE rooms SET view_count = 120 WHERE room_number = 'P201';
UPDATE rooms SET view_count = 95  WHERE room_number = 'M303';
UPDATE rooms SET view_count = 80  WHERE room_number = 'K103';
UPDATE rooms SET view_count = 60  WHERE room_number = 'S1.07';
UPDATE promotions SET discount_percent = 10 WHERE promotion_id = 2;
UPDATE promotions SET discount_percent = 12 WHERE promotion_id = 4;
UPDATE promotions SET discount_percent = 15 WHERE promotion_id = 5;
UPDATE promotion_rooms SET deal_price = (
    SELECT CAST(ROUND(r.base_rent_price * (100 - p.discount_percent) / 100.0 / 1000.0) * 1000 AS INTEGER)
    FROM rooms r, promotions p
    WHERE r.room_id = promotion_rooms.room_id AND p.promotion_id = promotion_rooms.promotion_id)
WHERE promotion_id IN (SELECT promotion_id FROM promotions WHERE discount_percent IS NOT NULL);
