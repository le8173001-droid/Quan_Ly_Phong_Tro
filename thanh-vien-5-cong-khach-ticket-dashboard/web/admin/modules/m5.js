/* THÀNH VIÊN 5 – 5 · Cổng khách, sự cố & báo cáo.
   File này gồm: (1) danh sách bảng/biểu mẫu của mô-đun, (2) các màn hình riêng của mô-đun. */
(function () {
  const { S, OPT, opts, ST, CAT, money, yes, VEH } = ADMIN_KIT;
  ADMIN_MODULES.push({
    id: 'm5', member: 'Thành viên 5', title: '📊 5 · Cổng khách, sự cố & báo cáo',
    resources: [
    { key: 'dashboard', label: '📊 Tổng quan', special: 'dashboard' },
    { key: 'tickets', label: '🛠️ Sự cố / yêu cầu', path: 'tickets', id: 'ticketId', grouped: true, buildingFilter: true, status: ST.ticket, statusKey: 'status',
      cols: [['title', 'Tiêu đề'], ['tenantName', 'Người gửi'], ['category', 'Loại', (v) => (CAT.find((o) => o.v === v) || {}).t || v], ['status', 'Trạng thái'], ['createdAt', 'Tạo lúc']],
      fields: [{ name: 'roomId', label: 'Phòng', type: 'ref', src: 'rooms', req: 1 }, { name: 'tenantId', label: 'Người thuê', type: 'ref', src: 'tenants', req: 1 }, { name: 'category', label: 'Loại', type: 'select', req: 1, options: CAT },
        { name: 'status', label: 'Trạng thái', type: 'select', options: ST.ticket }, { name: 'title', label: 'Tiêu đề', req: 1, full: 1 }, { name: 'description', label: 'Mô tả', type: 'textarea', full: 1 }] },
    { key: 'viewing-requests', label: '👀 Đặt lịch xem phòng', path: 'viewing-requests', id: 'viewingId', grouped: true, buildingFilter: true, status: ST.viewing, statusKey: 'status', noCreate: true,
      cols: [['fullName', 'Khách'], ['phone', 'SĐT'], ['preferredTime', 'Thời gian'], ['status', 'Trạng thái'], ['createdAt', 'Gửi lúc']],
      fields: [{ name: 'status', label: 'Trạng thái', type: 'select', options: ST.viewing }] },
    { key: 'support-requests', label: '🙋 Yêu cầu hỗ trợ', path: 'support-requests', id: 'requestId', adminOnly: true, status: ST.req, statusKey: 'status', noCreate: true,
      cols: [['fullName', 'Họ tên'], ['phone', 'SĐT'], ['requestType', 'Loại'], ['desiredArea', 'Khu vực'], ['budgetMax', 'Ngân sách', money], ['status', 'Trạng thái'], ['createdAt', 'Gửi lúc']],
      fields: [{ name: 'status', label: 'Trạng thái', type: 'select', options: ST.req }, { name: 'note', label: 'Ghi chú', type: 'textarea', full: 1 }] },
    { key: 'broker-applications', label: '🤝 Đơn môi giới', path: 'broker-applications', id: 'applicationId', adminOnly: true, status: ST.broker, statusKey: 'status', noCreate: true,
      cols: [['fullName', 'Họ tên'], ['phone', 'SĐT'], ['workArea', 'Khu vực'], ['status', 'Trạng thái'], ['createdAt', 'Gửi lúc']],
      fields: [{ name: 'status', label: 'Trạng thái', type: 'select', options: ST.broker }] }
    ],
    init(C) {
      const { el, $, clear, UI, Api, view, me, isAdmin, srcCache, confirmBox, statusText, fetchSrc, loadSrc, uploader, refField, buildForm, formModal, buildingSelect, putRows } = C;
  // ---------- tổng quan ----------
  async function dashboardView() {
    const d = await Api.get('/api/admin/dashboard');
    const kpi = (label, val, href) => el(href ? 'a' : 'div', { class: 'kpi', href }, el('small', { text: label }), el('b', { text: val }));
    const k = [kpi('Tòa nhà', d.buildings, '#buildings'), kpi('Tổng số phòng', d.rooms.total, '#rooms'), kpi('Phòng còn trống', d.rooms.available), kpi('Phòng ế (0 người)', d.rooms.empty), kpi('Phòng đã kín', d.rooms.full),
      kpi('Người thuê', d.tenants, '#tenants'), kpi('Hợp đồng hiệu lực', d.activeContracts, '#contracts'), kpi('Sự cố đang mở', d.openTickets, '#tickets'),
      kpi('Hóa đơn chưa thu', d.unpaidInvoices + ' • ' + UI.vnd(d.unpaidAmount), '#invoices'), kpi('Doanh thu tháng này', UI.vnd(d.revenueThisMonth)), kpi('Lịch xem phòng mới', d.newRequests.viewing, '#viewing-requests')];
    if (isAdmin) k.push(kpi('Chủ tòa nhà', d.owners, '#owners'), kpi('Tài khoản', d.users, '#users'), kpi('Khuyến mãi đang chạy', d.runningPromotions, '#promotions'), kpi('Phòng ghim nổi bật', d.featuredRooms, '#featured'),
      kpi('Yêu cầu hỗ trợ mới', d.newRequests.support, '#support-requests'), kpi('Đơn môi giới mới', d.newRequests.brokers, '#broker-applications'));
    view.appendChild(el('div', { class: 'cards' }, k));
    const tbl = (head, rows) => el('div', { class: 'tbl-wrap' }, el('table', { class: 'tbl' }, el('thead', {}, el('tr', {}, head.map((h) => el('th', { text: h })))), el('tbody', {}, rows.length ? rows : el('tr', {}, el('td', { colspan: head.length, class: 'muted', text: 'Không có' })))));
    view.appendChild(el('h3', { text: isAdmin ? 'Theo từng tòa nhà' : 'Các tòa nhà của bạn' }));
    view.appendChild(tbl(['Tòa nhà', isAdmin ? 'Chủ tòa' : '', 'Số phòng', 'Có người ở', 'HĐ hiệu lực', 'Sự cố mở'], d.byBuilding.map((b) => el('tr', {}, el('td', {}, el('a', { href: '#rooms?buildingId=' + b.buildingId, text: b.name })), el('td', { text: isAdmin ? b.managerName : '' }), el('td', { class: 'num', text: b.roomCount }), el('td', { class: 'num', text: b.occupiedRooms }), el('td', { class: 'num', text: b.activeContracts }), el('td', { class: 'num', text: b.openTickets })))));
    view.appendChild(el('div', { class: 'two', style: 'margin-top:16px' },
      tbl(['Hợp đồng sắp hết hạn', 'Hết hạn'], d.expiringContracts.map((c) => el('tr', {}, el('td', { text: c.buildingName + ' / ' + c.roomNumber + ' – ' + c.tenantName }), el('td', { text: c.endDate })))),
      tbl(['Phòng được xem nhiều', 'Lượt xem'], d.topViewedRooms.map((r) => el('tr', {}, el('td', { text: r.buildingName + ' / ' + r.roomNumber + ' – ' + r.title }), el('td', { class: 'num', text: r.viewCount }))))));
    view.appendChild(el('div', { style: 'margin-top:16px' }, tbl(['Tháng', 'Doanh thu'], d.revenueByMonth.map((x) => el('tr', {}, el('td', { text: x.month }), el('td', { class: 'num', text: UI.vnd(x.amount) }))))));
  }

      
C.views.dashboard = dashboardView;
    },
  });
})();
