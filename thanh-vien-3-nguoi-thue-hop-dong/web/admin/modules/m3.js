/* THÀNH VIÊN 3 – 3 · Người thuê, xe & hợp đồng.
   File này gồm: (1) danh sách bảng/biểu mẫu của mô-đun, (2) các màn hình riêng của mô-đun. */
(function () {
  const { S, OPT, opts, ST, CAT, money, yes, VEH } = ADMIN_KIT;
  ADMIN_MODULES.push({
    id: 'm3', member: 'Thành viên 3', title: '👥 3 · Người thuê, xe & hợp đồng',
    resources: [
    { key: 'tenants', label: '👥 Người thuê', path: 'tenants', id: 'tenantId', grouped: true, buildingFilter: true,
      cols: [['fullName', 'Họ tên'], ['phone', 'SĐT'], ['idNumber', 'CCCD'], ['activeContracts', 'HĐ'], ['vehicleCount', 'Số xe']],
      fields: [{ name: 'roomId', label: 'Phòng đang thuê', type: 'ref', src: 'rooms', req: 1, full: 1 }, { name: 'fullName', label: 'Họ và tên', req: 1 }, { name: 'phone', label: 'Số điện thoại', req: 1 }, { name: 'email', label: 'Email' },
        { name: 'gender', label: 'Giới tính', type: 'select', options: opts([['Nam', 'Nam'], ['Nữ', 'Nữ'], ['Khác', 'Khác']]) }, { name: 'birthDate', label: 'Ngày sinh', type: 'date' },
        { name: 'idNumber', label: 'CCCD/CMND' }, { name: 'idIssueDate', label: 'Ngày cấp', type: 'date' }, { name: 'idIssuePlace', label: 'Nơi cấp' }, { name: 'permanentAddress', label: 'Địa chỉ thường trú', full: 1 }] },
    { key: 'vehicles', label: '🏍️ Phương tiện', path: 'vehicles', id: 'vehicleId', grouped: true, buildingFilter: true,
      cols: [['licensePlate', 'Biển số'], ['vehicleType', 'Loại xe', (v) => VEH[v] || v], ['brandColor', 'Hãng / màu'], ['tenantName', 'Chủ xe']],
      fields: [{ name: 'tenantId', label: 'Chủ xe (người thuê)', type: 'ref', src: 'tenants', req: 1, full: 1 }, { name: 'licensePlate', label: 'Biển số', req: 1 }, { name: 'vehicleType', label: 'Loại xe', type: 'select', options: opts(Object.entries(VEH)) }, { name: 'brandColor', label: 'Hãng xe / màu sắc' }] },
    { key: 'contracts', label: '📄 Hợp đồng', path: 'contracts', id: 'contractId', grouped: true, buildingFilter: true, status: ST.contract, statusKey: 'status', special: 'contracts',
      cols: [['contractCode', 'Số HĐ'], ['tenantName', 'Người thuê'], ['startDate', 'Từ ngày'], ['endDate', 'Đến ngày'], ['monthlyRent', 'Tiền thuê', money], ['status', 'Trạng thái']],
      fields: [{ section: 'Các bên & phòng' }, { name: 'roomId', label: 'Phòng cho thuê', type: 'ref', src: 'rooms', req: 1 }, { name: 'tenantId', label: 'Bên thuê (người thuê)', type: 'ref', src: 'tenants', req: 1 },
        { section: 'Thời hạn & giá' }, { name: 'startDate', label: 'Ngày bắt đầu', type: 'date', req: 1 }, { name: 'endDate', label: 'Ngày kết thúc', type: 'date', req: 1 },
        { name: 'monthlyRent', label: 'Tiền thuê/tháng (trống = giá phòng)', type: 'number' }, { name: 'depositAmount', label: 'Tiền đặt cọc', type: 'number' },
        { name: 'paymentDay', label: 'Thanh toán trước ngày (1-31) hằng tháng', type: 'number' }, { name: 'paymentMethod', label: 'Hình thức thanh toán', type: 'select', options: opts([['Tiền mặt', 'Tiền mặt'], ['Chuyển khoản', 'Chuyển khoản'], ['Tiền mặt hoặc chuyển khoản', 'Tiền mặt hoặc chuyển khoản']]) },
        { name: 'lateFeePercent', label: 'Phạt trả chậm (%/ngày... nhập số %)', type: 'number' }, { name: 'noticeDays', label: 'Báo trước khi chấm dứt (ngày)', type: 'number' },
        { name: 'electricPrice', label: 'Giá điện (đ/kWh, trống = theo bảng dịch vụ)', type: 'number' }, { name: 'waterPrice', label: 'Giá nước (đ/m³, trống = theo bảng dịch vụ)', type: 'number' },
        { name: 'serviceNote', label: 'Ghi chú dịch vụ (internet, rác, giữ xe…)', full: 1 },
        { section: 'Ký kết' }, { name: 'signDate', label: 'Ngày ký', type: 'date' }, { name: 'signPlace', label: 'Nơi ký' },
        { name: 'stayType', label: 'Hình thức', type: 'select', options: opts([['long_term', 'Dài hạn'], ['short_term', 'Ngắn hạn']]) }, { name: 'bookingSource', label: 'Nguồn đặt', type: 'select', options: opts([['direct', 'Trực tiếp'], ['airbnb', 'Airbnb'], ['booking', 'Booking'], ['agent', 'Môi giới']]) },
        { name: 'status', label: 'Trạng thái', type: 'select', options: ST.contract },
        { section: 'Tài sản & điều khoản' }, { name: 'assetsText', label: 'Tài sản bàn giao – mỗi dòng: Tên | Số lượng | Tình trạng', type: 'textarea', full: 1, rows: 5 },
        { name: 'extraTerms', label: 'Điều khoản bổ sung (nếu có)', type: 'textarea', full: 1, rows: 4 }, { name: 'note', label: 'Ghi chú nội bộ', full: 1 }] }
    ],
    init(C) {
      const { el, $, clear, UI, Api, view, me, isAdmin, srcCache, confirmBox, statusText, fetchSrc, loadSrc, uploader, refField, buildForm, formModal, buildingSelect, putRows } = C;

      

    },
  });
})();
