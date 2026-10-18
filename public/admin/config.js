/* Cấu hình các bảng quản trị. Mỗi field: name (camelCase), label, type, req, options, src, full.
   Cờ của bảng: adminOnly (chỉ ADMIN thấy), roRole (chủ tòa chỉ xem), grouped (nhóm Tòa nhà → Phòng), buildingFilter. */
(function (w) {
  const S = (url, id, label) => ({ url, id, label });
  const OPT = (n) => S('/api/admin/options/' + n, 'id', 'label');
  const SRC = {
    rooms: OPT('rooms'), tenants: OPT('tenants'), managers: OPT('managers'), contracts: OPT('contracts'), buildings: OPT('buildings'), invoices: OPT('invoices'),
    roles: S('/api/admin/roles?size=100', 'roleId', 'roleName'),
    roomTypes: S('/api/admin/room-types?size=100', 'typeId', 'typeName'),
    services: S('/api/admin/services?size=100', 'serviceId', 'serviceName'),
  };
  const opts = (pairs) => pairs.map(([v, t]) => ({ v, t }));
  const ST = {
    invoice: opts([['unpaid', 'Chưa thanh toán'], ['partial', 'Thanh toán một phần'], ['paid', 'Đã thanh toán'], ['cancelled', 'Đã hủy']]),
    contract: opts([['pending', 'Chờ hiệu lực'], ['active', 'Đang hiệu lực'], ['ended', 'Đã kết thúc'], ['cancelled', 'Đã hủy']]),
    ticket: opts([['open', 'Mới'], ['in_progress', 'Đang xử lý'], ['done', 'Hoàn thành'], ['cancelled', 'Đã hủy']]),
    req: opts([['new', 'Mới'], ['processing', 'Đang xử lý'], ['done', 'Xong']]),
    viewing: opts([['new', 'Mới'], ['contacted', 'Đã liên hệ'], ['done', 'Xong'], ['cancelled', 'Hủy']]),
    broker: opts([['new', 'Mới'], ['approved', 'Đã duyệt'], ['rejected', 'Từ chối']]),
    user: opts([['active', 'Hoạt động'], ['locked', 'Khóa']]),
  };
  const CAT = opts([['hygiene', 'Vệ sinh'], ['maintenance', 'Bảo trì'], ['electric_water', 'Điện nước'], ['security', 'An ninh'], ['other', 'Khác']]);
  const money = (v) => (v === null || v === undefined ? '' : UI.vnd(v));
  const yes = (v) => (v ? '✔' : '—');
  const VEH = { motorbike: 'Xe máy', car: 'Ô tô', bicycle: 'Xe đạp' };

  w.ADMIN_SRC = SRC;
  w.ADMIN_STATUS = ST;
  w.ADMIN_KIT = { S, OPT, opts, ST, CAT, money, yes, VEH };
  w.ADMIN_MODULES = [];  // 5 mô-đun nạp từ /admin/modules/m1.js … m5.js
})(window);
