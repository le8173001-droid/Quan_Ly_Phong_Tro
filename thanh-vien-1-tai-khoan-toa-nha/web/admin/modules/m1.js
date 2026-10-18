/* THÀNH VIÊN 1 – 1 · Tài khoản, phân quyền & tòa nhà.
   File này gồm: (1) danh sách bảng/biểu mẫu của mô-đun, (2) các màn hình riêng của mô-đun. */
(function () {
  const { S, OPT, opts, ST, CAT, money, yes, VEH } = ADMIN_KIT;
  ADMIN_MODULES.push({
    id: 'm1', member: 'Thành viên 1', title: '🔐 1 · Tài khoản, phân quyền & tòa nhà',
    resources: [
    { key: 'owners', label: '🏢 Chủ tòa nhà', path: 'owners', adminOnly: true, readOnly: true, id: 'userId',
      cols: [['fullName', 'Chủ tòa'], ['phone', 'SĐT'], ['email', 'Email'], ['buildingCount', 'Số tòa'], ['roomCount', 'Số phòng'], ['tenantCount', 'Người thuê'], ['activeContracts', 'HĐ hiệu lực'], ['status', 'Trạng thái']],
      rowActions: 'owner' },
    { key: 'users', label: '🔐 Tài khoản người dùng', path: 'users', adminOnly: true, id: 'userId', status: ST.user, statusKey: 'status',
      cols: [['username', 'Tên đăng nhập'], ['fullName', 'Họ tên'], ['email', 'Email'], ['phone', 'SĐT'], ['roleName', 'Vai trò'], ['buildingCount', 'Số tòa'], ['status', 'Trạng thái']],
      fields: [{ name: 'roleId', label: 'Vai trò', type: 'ref', src: 'roles', req: 1 }, { name: 'username', label: 'Tên đăng nhập', req: 1 }, { name: 'fullName', label: 'Họ tên', req: 1 }, { name: 'email', label: 'Email' }, { name: 'phone', label: 'SĐT' },
        { name: 'status', label: 'Trạng thái', type: 'select', options: ST.user }, { name: 'password', label: 'Mật khẩu (bắt buộc khi tạo; khi sửa để trống = giữ nguyên)', type: 'password', full: 1 },
        { section: 'Thông tin pháp lý' }, { name: 'idNumber', label: 'CCCD/CMND' }, { name: 'birthDate', label: 'Ngày sinh', type: 'date' }, { name: 'idIssueDate', label: 'Ngày cấp', type: 'date' }, { name: 'idIssuePlace', label: 'Nơi cấp' },
        { name: 'address', label: 'Địa chỉ thường trú', full: 1 }, { name: 'bankName', label: 'Ngân hàng' }, { name: 'bankAccount', label: 'Số tài khoản' }] },
    { key: 'buildings', label: '🏠 Tòa nhà', path: 'buildings', id: 'buildingId',
      cols: [['name', 'Tòa nhà'], ['fullAddr', 'Địa chỉ', (v, r) => [r.address, r.wardName, r.districtName, r.provinceName].filter(Boolean).join(', ')], ['managerName', 'Chủ tòa', null, 'adminOnly'], ['roomCount', 'Số phòng'], ['tenantCount', 'Người thuê']],
      fields: [{ name: 'managerId', label: 'Chủ tòa nhà', type: 'ref', src: 'managers', req: 1, adminOnly: 1 }, { name: 'name', label: 'Tên tòa nhà', req: 1 },
        { name: 'address', label: 'Số nhà, tên đường', req: 1, full: 1 }, { name: '_loc', type: 'location', label: 'Địa chỉ hành chính', full: 1 },
        { name: 'latitude', label: 'Vĩ độ (để gợi ý khu vực gần bạn)', type: 'number' }, { name: 'longitude', label: 'Kinh độ', type: 'number' },
        { name: 'contactName', label: 'Người liên hệ' }, { name: 'contactPhone', label: 'SĐT liên hệ' },
        { name: 'imageUrls', type: 'images', label: 'Ảnh tòa nhà (ảnh đầu tiên là ảnh bìa)', full: 1, from: 'images' }, { name: 'description', label: 'Mô tả', type: 'textarea', full: 1 }] },
    { key: 'promotions', label: '🎁 Khuyến mãi & % giảm giá', path: 'promotions', id: 'promotionId', adminOnly: true, special: 'promotions',
      cols: [['title', 'Tiêu đề'], ['promoType', 'Loại'], ['discountPercent', '% giảm', (v) => (v ? v + '%' : '')], ['startAt', 'Bắt đầu'], ['endAt', 'Kết thúc'], ['isRunning', 'Đang chạy', yes], ['roomCount', 'Số phòng']],
      fields: [{ name: 'title', label: 'Tiêu đề', req: 1, full: 1 }, { name: 'promoType', label: 'Loại', type: 'select', options: opts([['program', 'Chương trình'], ['hotdeal', 'HOTDEAL (giá sốc theo phòng)'], ['voucher', 'Voucher'], ['partner', 'Đối tác']]) },
        { name: 'discountPercent', label: 'Phần trăm giảm giá (%)', type: 'number' }, { name: 'discountText', label: 'Chữ nổi bật (trống = tự điền -N%)' },
        { name: 'voucherCode', label: 'Mã voucher' }, { name: 'bannerColor', label: 'Màu banner', type: 'select', options: opts([['green', 'Xanh lá'], ['orange', 'Cam'], ['blue', 'Xanh dương'], ['purple', 'Tím'], ['red', 'Đỏ'], ['yellow', 'Vàng'], ['teal', 'Xanh ngọc'], ['pink', 'Hồng']]) },
        { name: 'startAt', label: 'Bắt đầu', type: 'datetime', req: 1 }, { name: 'endAt', label: 'Kết thúc', type: 'datetime', req: 1 }, { name: 'linkUrl', label: 'Liên kết (vd: /search.html?hotdeal=true)' }, { name: 'sortOrder', label: 'Thứ tự', type: 'number' },
        { name: 'isActive', label: 'Đang bật', type: 'bool' }, { name: 'description', label: 'Mô tả', type: 'textarea', full: 1 }] },
    { key: 'settings', label: '⚙️ Hotline, Zalo & cấu hình', special: 'settings', adminOnly: true },
    { key: 'landmarks', label: 'Trường / nơi làm việc', path: 'landmarks', id: 'landmarkId', roRole: true,
      cols: [['name', 'Tên'], ['landmarkType', 'Loại'], ['districtName', 'Quận/huyện'], ['latitude', 'Vĩ độ'], ['longitude', 'Kinh độ']],
      fields: [{ name: 'name', label: 'Tên', req: 1, full: 1 }, { name: 'landmarkType', label: 'Loại', type: 'select', req: 1, options: opts([['school', 'Trường học'], ['work', 'Nơi làm việc']]) }, { name: 'address', label: 'Địa chỉ', full: 1 }, { name: 'latitude', label: 'Vĩ độ', type: 'number', req: 1 }, { name: 'longitude', label: 'Kinh độ', type: 'number', req: 1 }] },
    { key: 'amenities', label: 'Tiện nghi', path: 'amenities', id: 'amenityId', roRole: true,
      cols: [['name', 'Tên'], ['icon', 'Icon']], fields: [{ name: 'name', label: 'Tên', req: 1 }, { name: 'icon', label: 'Icon' }] }
    ],
    init(C) {
      const { el, $, clear, UI, Api, view, me, isAdmin, srcCache, confirmBox, statusText, fetchSrc, loadSrc, uploader, refField, buildForm, formModal, buildingSelect, putRows } = C;
  // ---------- khuyến mãi: chọn phòng áp dụng ----------
  async function promoRooms(id, done) {
    const [p, rooms] = await Promise.all([Api.get('/api/admin/promotions/' + id), fetchSrc('rooms')]);
    const pct = p.discountPercent;
    const cur = Object.fromEntries((p.rooms || []).map((r) => [r.roomId, r.dealPrice]));
    const rows = rooms.map((r) => {
      const cb = el('input', { type: 'checkbox' }); cb.checked = r.v in cur;
      const price = el('input', { type: 'number', placeholder: pct ? 'Tự tính theo ' + pct + '%' : 'Giá ưu đãi', value: pct ? '' : (cur[r.v] || ''), disabled: pct ? true : null });
      return { r, cb, price, node: el('tr', {}, el('td', {}, cb), el('td', { text: r.t }), el('td', { class: 'num', text: UI.vnd(r.raw.basePrice || r.raw.baseRentPrice) }), el('td', {}, price)) };
    });
    const msg = el('div');
    const m = UI.modal(el('div', {}, el('h3', { text: 'Phòng áp dụng: ' + p.title }),
      el('p', { class: 'muted', text: pct ? 'Chương trình giảm ' + pct + '%: giá ưu đãi của từng phòng được tự tính (làm tròn nghìn đồng).' : 'Chọn phòng và nhập giá ưu đãi cho từng phòng. (Hoặc điền "% giảm giá" ở chương trình để tự tính.)' }), msg,
      el('div', { style: 'max-height:50vh;overflow:auto' }, el('table', { class: 'tbl' }, el('thead', {}, el('tr', {}, el('th'), el('th', { text: 'Tòa nhà - Phòng' }), el('th', { class: 'num', text: 'Giá gốc' }), el('th', { text: 'Giá ưu đãi' }))), el('tbody', {}, rows.map((x) => x.node)))),
      el('div', { style: 'text-align:right;margin-top:10px' }, el('button', { class: 'btn', type: 'button', text: 'Lưu', onclick: async () => {
        clear(msg);
        const list = rows.filter((x) => x.cb.checked).map((x) => ({ roomId: x.r.v, dealPrice: x.price.value ? Number(x.price.value) : undefined }));
        try { await Api.put('/api/admin/promotions/' + id + '/rooms', { rooms: list }); UI.toast('Đã lưu'); m.close(); done(); } catch (e) { msg.appendChild(UI.alertBox('err', e.message)); }
      } }))));
    m.root.firstChild.classList.add('big');
  }


  // ---------- cấu hình: hotline, zalo, ... ----------
  async function settingsView() {
    const s = await Api.get('/api/admin/settings');
    const defs = [['site_name', 'Tên website', ''], ['tagline', 'Khẩu hiệu', ''], ['hotline', '☎ Số hotline', 'Hiện ở chân trang và mục hỗ trợ khách hàng. Ví dụ: 1900 6868'], ['zalo', '💬 Số Zalo', 'Số điện thoại Zalo, khách bấm sẽ mở zalo.me. Ví dụ: 0901234567'],
      ['email', '✉ Email', ''], ['facebook', 'Liên kết Facebook (https://…)', ''], ['address', 'Địa chỉ văn phòng', '']];
    const inputs = {}, msg = el('div');
    const form = el('form', { class: 'card set-form' }, msg,
      defs.map(([k, label, hint]) => { inputs[k] = el('input', { value: s[k] || '', class: k === 'hotline' || k === 'zalo' ? 'hl' : '' }); return el('div', { class: 'field' }, el('label', { text: label }), inputs[k], hint ? el('small', { class: 'muted', text: hint }) : null); }),
      el('button', { class: 'btn', type: 'submit', text: 'Lưu cấu hình' }));
    form.addEventListener('submit', async (e) => {
      e.preventDefault(); clear(msg);
      try { await Api.put('/api/admin/settings', Object.fromEntries(Object.entries(inputs).map(([k, i]) => [k, i.value]))); msg.appendChild(UI.alertBox('ok', 'Đã lưu. Trang chủ và chân trang sẽ cập nhật ngay.')); } catch (err) { msg.appendChild(UI.alertBox('err', err.message)); }
    });
    view.appendChild(form);
  }

      
C.fn.promoRooms = promoRooms;      C.views.settings = settingsView;
    },
  });
})();
