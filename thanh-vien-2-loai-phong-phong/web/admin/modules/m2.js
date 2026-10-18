/* THÀNH VIÊN 2 –  2 · Loại phòng & phòng.
   File này gồm: (1) danh sách bảng/biểu mẫu của mô-đun, (2) các màn hình riêng của mô-đun. */
(function () {
  const { S, OPT, opts, ST, CAT, money, yes, VEH } = ADMIN_KIT;
  ADMIN_MODULES.push({
    id: 'm2', member: 'Thành viên 2', title: '🛏️ 2 · Loại phòng & phòng',
    resources: [
    { key: 'rooms', label: '🛏️ Phòng', special: 'rooms' },
    { key: 'room-types', label: 'Loại hình', path: 'room-types', id: 'typeId', roRole: true,
      cols: [['typeName', 'Tên'], ['slug', 'Slug'], ['sortOrder', 'Thứ tự'], ['roomCount', 'Số phòng']],
      fields: [{ name: 'typeName', label: 'Tên loại hình', req: 1 }, { name: 'slug', label: 'Slug (a-z, 0-9, -)', req: 1 }, { name: 'icon', label: 'Icon' }, { name: 'sortOrder', label: 'Thứ tự', type: 'number' }, { name: 'description', label: 'Mô tả', full: 1 }] },
    { key: 'featured', label: '⭐ Mục nổi bật', special: 'featured', adminOnly: true }
    ],
    init(C) {
      const { el, $, clear, UI, Api, view, me, isAdmin, srcCache, confirmBox, statusText, fetchSrc, loadSrc, uploader, refField, buildForm, formModal, buildingSelect, putRows } = C;
  // ---------- phòng ----------
  const ROOM_FIELDS = [
    { name: 'buildingId', label: 'Tòa nhà', type: 'ref', src: 'buildings', req: 1 }, { name: 'typeId', label: 'Loại hình', type: 'ref', src: 'roomTypes', req: 1 },
    { name: 'roomNumber', label: 'Mã phòng', req: 1 }, { name: 'title', label: 'Tiêu đề', req: 1 }, { name: 'baseRentPrice', label: 'Giá thuê/tháng', type: 'number', req: 1 }, { name: 'areaM2', label: 'Diện tích (m²)', type: 'number' },
    { name: 'maxOccupants', label: 'Sức chứa tối đa', type: 'number' }, { name: 'currentOccupants', label: 'Số người đang ở', type: 'number' },
    { name: 'status', label: 'Trạng thái', type: 'select', options: [{ v: 'available', t: 'Còn trống' }, { v: 'occupied', t: 'Đã thuê' }, { v: 'maintenance', t: 'Sửa chữa' }, { v: 'hidden', t: 'Ẩn' }] },
    { name: 'isNew', label: 'Mới đăng', type: 'bool' }, { name: 'isHot', label: 'HOT', type: 'bool' }, { name: 'isNow', label: 'NOW – dọn vào ở ngay', type: 'bool' },
    { name: 'imageUrls', type: 'images', label: 'Ảnh phòng', full: 1, from: 'images' },
    { name: 'description', label: 'Mô tả', type: 'textarea', full: 1 },
  ];
  const OCC = { empty: 'Ế (0 người)', almost_empty: 'Sắp trống', full: 'Đã kín' };
  async function roomsView(init) {
    let page = 1, q = '', status = '', occupancy = '', buildingId = (init && init.buildingId) || '';
    const bSel = await buildingSelect(buildingId);
    const search = el('input', { type: 'search', placeholder: 'Tìm mã phòng, tiêu đề…' });
    const stSel = el('select', {}, el('option', { value: '', text: 'Phòng đang hoạt động' }), [['available', 'Còn trống'], ['occupied', 'Đã thuê'], ['maintenance', 'Sửa chữa'], ['hidden', 'Đã ẩn (khôi phục được)']].map(([v, t]) => el('option', { value: v, text: t })));
    const occSel = el('select', {}, el('option', { value: '', text: 'Mọi mức lấp đầy' }), Object.entries(OCC).map(([v, t]) => el('option', { value: v, text: t })));
    const tbody = el('tbody'), pagerBox = el('div', { class: 'pager' }), count = el('span', { class: 'muted' });
    view.appendChild(el('div', { class: 'bar' }, bSel, search, stSel, occSel, count, el('span', { style: 'flex:1' }), el('button', { class: 'btn', type: 'button', text: '＋ Thêm phòng', onclick: () => editRoom(null) })));
    const heads = ['', 'Mã', 'Tiêu đề', 'Loại', 'Giá', 'Người ở', 'Lượt xem', 'Trạng thái', ''];
    view.appendChild(el('div', { class: 'tbl-wrap' }, el('table', { class: 'tbl' }, el('thead', {}, el('tr', {}, heads.map((h) => el('th', { text: h })))), tbody)));
    view.appendChild(pagerBox);

    async function editRoom(row) {
      const edit = !!row;
      let full = {}, amenities = [];
      try { amenities = (await Api.get('/api/meta')).amenities; if (edit) full = await Api.get('/api/rooms/' + row.roomId); } catch (e) { UI.toast(e.message, true); return; }
      const sel = new Set((full.amenities || []).map((a) => a.amenityId));
      const checks = el('div', { class: 'checks' }, amenities.map((a) => { const c = el('input', { type: 'checkbox', value: a.amenityId }); c.checked = sel.has(a.amenityId); return el('label', {}, c, a.name); }));
      const { form } = await formModal(edit ? 'Sửa phòng ' + row.roomNumber : 'Thêm phòng', ROOM_FIELDS, full, async (body) => {
        body.amenityIds = [...checks.querySelectorAll('input:checked')].map((c) => Number(c.value));
        if (edit) { delete body.roomNumber; await Api.put('/api/rooms/' + row.roomId, body); } else await Api.post('/api/rooms', body);
        delete srcCache.rooms; UI.toast('Đã lưu phòng'); load();
      }, { edit, lock: edit ? ['roomNumber'] : [] });
      form.insertBefore(el('div', { class: 'field' }, el('label', { text: 'Tiện nghi' }), checks), form.lastChild);
    }
    async function remove(row, perm) {
      if (!confirmBox(perm ? 'XÓA VĨNH VIỄN phòng ' + row.roomNumber + '? Không thể hoàn tác.' : 'Ẩn phòng ' + row.roomNumber + '? (có thể khôi phục)')) return;
      try { const r = await Api.del('/api/rooms/' + row.roomId + '?type=' + (perm ? 1 : 2)); UI.toast(r.message); load(); } catch (e) { UI.toast(e.message, true); }
    }
    async function restore(row) { try { await Api.post('/api/rooms/' + row.roomId + '/restore'); UI.toast('Đã khôi phục phòng'); load(); } catch (e) { UI.toast(e.message, true); } }
    async function pin(row) {
      try { await Api.post('/api/admin/rooms/' + row.roomId + '/feature', { isFeatured: !row.isFeatured, featuredOrder: 0 }); UI.toast(row.isFeatured ? 'Đã bỏ khỏi mục nổi bật' : 'Đã đưa lên mục nổi bật'); load(); } catch (e) { UI.toast(e.message, true); }
    }
    async function load() {
      clear(tbody);
      try {
        const r = await Api.get('/api/rooms', { admin: 1, q, status, occupancy, buildingId, page, size: 30, sort: 'building' });
        count.textContent = r.totalItems + ' phòng';
        if (!r.items.length) tbody.appendChild(el('tr', {}, el('td', { colspan: heads.length, class: 'empty', text: 'Không có phòng' })));
        let last = null;
        r.items.forEach((x) => {
          if (x.buildingName !== last) { tbody.appendChild(el('tr', { class: 'grp-b' }, el('td', { colspan: heads.length, text: '🏢 ' + x.buildingName }))); last = x.buildingName; }
          const acts = el('td', { class: 'acts' });
          if (isAdmin || x.managerId === me.id) {
            acts.appendChild(el('button', { class: 'btn sm light', type: 'button', text: 'Sửa', onclick: () => editRoom(x) }));
            if (x.status === 'hidden') acts.appendChild(el('button', { class: 'btn sm orange', type: 'button', text: 'Khôi phục', onclick: () => restore(x) }));
            else acts.appendChild(el('button', { class: 'btn sm light', type: 'button', text: 'Ẩn', onclick: () => remove(x, false) }));
            if (isAdmin) { acts.appendChild(el('button', { class: 'btn sm light', type: 'button', text: x.isFeatured ? '★ Bỏ nổi bật' : '☆ Nổi bật', onclick: () => pin(x) })); acts.appendChild(el('button', { class: 'btn sm light', type: 'button', text: 'Xóa hẳn', onclick: () => remove(x, true) })); }
          } else acts.appendChild(el('span', { class: 'muted', text: 'Chỉ xem' }));
          tbody.appendChild(el('tr', {}, el('td', {}, el('img', { class: 'mini', src: UI.safeUrl(x.thumbnailUrl), alt: '' })), el('td', { text: x.roomNumber }), el('td', { text: x.title + (x.isFeatured ? ' ★' : '') }), el('td', { text: x.typeName }), el('td', { class: 'num', text: UI.vnd(x.baseRentPrice) }),
            el('td', { text: x.currentOccupants + '/' + x.maxOccupants + (OCC[x.occupancyState] ? ' • ' + OCC[x.occupancyState] : '') }), el('td', { class: 'num', text: x.viewCount }), el('td', { text: x.status }), acts));
        });
        UI.pager(pagerBox, r.page, r.totalPages, (p) => { page = p; load(); });
      } catch (e) { tbody.appendChild(el('tr', {}, el('td', { colspan: heads.length }, UI.alertBox('err', e.message)))); }
    }
    let t; search.addEventListener('input', () => { clearTimeout(t); t = setTimeout(() => { q = search.value.trim(); page = 1; load(); }, 300); });
    stSel.addEventListener('change', () => { status = stSel.value; page = 1; load(); });
    occSel.addEventListener('change', () => { occupancy = occSel.value; page = 1; load(); });
    bSel.addEventListener('change', () => { buildingId = bSel.value; page = 1; load(); });
    load();
  }


  // ---------- mục nổi bật ----------
  async function featuredView() {
    const d = await Api.get('/api/admin/featured');
    const st = d.settings || {};
    const mode = el('select', {}, [['mixed', 'Kết hợp: phòng ghim trước, còn lại lấy theo lượt xem'], ['manual', 'Thủ công: chỉ phòng do Admin ghim'], ['auto', 'Tự động: theo số lượt xem']].map(([v, t]) => el('option', { value: v, text: t })));
    mode.value = st.featured_mode || 'mixed';
    const count = el('input', { type: 'number', min: 1, max: 12, value: st.featured_count || 4 });
    const TYPES = [['ky-tuc-xa', 'Ký túc xá nổi bật'], ['nha', 'Nhà nổi bật'], ['chung-cu', 'Chung cư nổi bật'], ['phong-tro', 'Phòng trọ nổi bật'], ['van-phong', 'Văn phòng nổi bật'], ['pass-phong', 'Pass phòng & ở ghép nổi bật']];
    const on = new Set((st.featured_types || 'ky-tuc-xa,nha,chung-cu,phong-tro').split(',').map((x) => x.trim()).filter(Boolean));
    const checks = el('div', { class: 'checks' }, TYPES.map(([slug, label]) => { const c = el('input', { type: 'checkbox', value: slug }); c.checked = on.has(slug); return el('label', {}, c, label); }));
    const msg = el('div');
    const form = el('form', { class: 'card set-form' }, el('h3', { text: 'Cách chọn phòng nổi bật trên trang chủ' }), msg,
      el('div', { class: 'field' }, el('label', { text: 'Chế độ' }), mode), el('div', { class: 'field' }, el('label', { text: 'Số phòng mỗi mục (1–12)' }), count),
      el('div', { class: 'field' }, el('label', { text: 'Các mục hiển thị' }), checks), el('button', { class: 'btn', type: 'submit', text: 'Lưu cấu hình nổi bật' }));
    form.addEventListener('submit', async (e) => {
      e.preventDefault(); clear(msg);
      try { await Api.put('/api/admin/settings', { featured_mode: mode.value, featured_count: String(count.value), featured_types: [...checks.querySelectorAll('input:checked')].map((c) => c.value).join(',') }); msg.appendChild(UI.alertBox('ok', 'Đã lưu')); C.route(); } catch (err) { msg.appendChild(UI.alertBox('err', err.message)); }
    });
    view.appendChild(form);

    // phòng đã ghim
    const pinned = el('tbody');
    const drawPinned = (rows) => { clear(pinned); if (!rows.length) pinned.appendChild(el('tr', {}, el('td', { colspan: 5, class: 'muted', text: 'Chưa ghim phòng nào' })));
      rows.forEach((r) => {
        const ord = el('input', { type: 'number', value: r.featuredOrder || 0, style: 'width:70px' });
        pinned.appendChild(el('tr', {}, el('td', { text: r.buildingName + ' / ' + r.roomNumber + ' – ' + r.title }), el('td', { text: r.typeName }), el('td', { class: 'num', text: r.viewCount }), el('td', {}, ord),
          el('td', { class: 'acts' }, el('button', { class: 'btn sm light', type: 'button', text: 'Lưu thứ tự', onclick: async () => { try { await Api.post('/api/admin/rooms/' + r.roomId + '/feature', { isFeatured: true, featuredOrder: Number(ord.value) || 0 }); UI.toast('Đã lưu'); } catch (e) { UI.toast(e.message, true); } } }),
            el('button', { class: 'btn sm light', type: 'button', text: 'Bỏ ghim', onclick: async () => { try { await Api.post('/api/admin/rooms/' + r.roomId + '/feature', { isFeatured: false }); C.route(); } catch (e) { UI.toast(e.message, true); } } }))));
      }); };
    drawPinned(d.pinned);
    view.appendChild(el('h3', { style: 'margin-top:20px', text: 'Phòng đã ghim (xuất hiện đầu tiên trong mục theo loại hình)' }));
    view.appendChild(el('div', { class: 'tbl-wrap' }, el('table', { class: 'tbl' }, el('thead', {}, el('tr', {}, ['Phòng', 'Loại hình', 'Lượt xem', 'Thứ tự (nhỏ lên trước)', ''].map((h) => el('th', { text: h })))), pinned)));

    // ghim thêm
    const q = el('input', { type: 'search', placeholder: 'Tìm phòng để ghim (mã, tên tòa…)' }), res = el('div', { class: 'tbl-wrap', style: 'margin-top:10px' });
    let t;
    q.addEventListener('input', () => { clearTimeout(t); t = setTimeout(async () => {
      const r = await Api.get('/api/rooms', { admin: 1, q: q.value.trim(), size: 8, sort: 'popular' }).catch(() => ({ items: [] }));
      clear(res).appendChild(el('table', { class: 'tbl' }, el('tbody', {}, r.items.map((x) => el('tr', {}, el('td', { text: x.buildingName + ' / ' + x.roomNumber + ' – ' + x.title }), el('td', { text: x.typeName }), el('td', { class: 'num', text: x.viewCount + ' lượt xem' }),
        el('td', {}, x.isFeatured ? el('span', { class: 'muted', text: 'Đã ghim' }) : el('button', { class: 'btn sm', type: 'button', text: '☆ Ghim', onclick: async () => { try { await Api.post('/api/admin/rooms/' + x.roomId + '/feature', { isFeatured: true, featuredOrder: 0 }); C.route(); } catch (e) { UI.toast(e.message, true); } } })))))));
    }, 300); });
    view.appendChild(el('h3', { style: 'margin-top:20px', text: 'Ghim thêm phòng' }));
    view.appendChild(el('div', { class: 'bar' }, q)); view.appendChild(res);

    // xem trước
    view.appendChild(el('h3', { style: 'margin-top:20px', text: 'Đang hiển thị trên trang chủ' }));
    (d.sections || []).forEach((s) => view.appendChild(el('p', {}, el('b', { text: s.title + ': ' }), s.rooms.map((r) => r.roomNumber + (r.featuredSource === 'pinned' ? ' ★' : ' (' + r.viewCount + ' lượt xem)')).join(', '))));
  }

      
C.views.rooms = roomsView;      C.views.featured = featuredView;
    },
  });
})();
