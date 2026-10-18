/* Trang quản trị NhàTrọ+: ADMIN thấy toàn hệ thống; chủ tòa nhà (MANAGER) chỉ thấy dữ liệu của tòa nhà mình (server lọc). */
(async function () {
  const { el, $, clear } = UI;
  const { me } = await Layout.init('');
  const view = $('#view'), side = $('#side');
  if (!me) { location.replace('/login.html?next=/admin/index.html'); return; }
  if (me.role !== 'ADMIN' && me.role !== 'MANAGER') { location.replace('/'); return; }
  const isAdmin = me.role === 'ADMIN';
  const srcCache = {};
  const confirmBox = (t) => window.confirm(t);
  const statusText = (list, v) => { const o = list && list.find((x) => x.v === v); return o ? o.t : v; };

  // ---------- nguồn dữ liệu cho ô chọn ----------
  async function fetchSrc(name, q) {
    const s = ADMIN_SRC[name];
    const r = await Api.get(s.url, q ? { q } : undefined);
    const list = Array.isArray(r) ? r : r.items;
    return list.map((x) => ({ v: x[s.id], t: x[s.label], raw: x }));
  }
  async function loadSrc(name) { return srcCache[name] || (srcCache[name] = await fetchSrc(name)); }

  // ---------- tải ảnh lên ----------
  function uploader(initial, max) {
    let urls = (initial || []).slice();
    const grid = el('div', { class: 'upload-grid' });
    const file = el('input', { type: 'file', accept: 'image/jpeg,image/png,image/gif,image/webp', multiple: true, class: 'hidden' });
    const msg = el('div', { class: 'muted' });
    const add = el('button', { type: 'button', class: 'up-add', text: '＋ Tải ảnh' });
    function draw() {
      clear(grid);
      urls.forEach((u, i) => grid.appendChild(el('div', { class: 'up-item' + (i === 0 ? ' cover' : '') }, el('img', { src: UI.safeUrl(u), alt: '' }),
        el('button', { type: 'button', title: 'Xóa ảnh', text: '×', onclick: () => { urls.splice(i, 1); draw(); } }))));
      if (urls.length < (max || 30)) grid.appendChild(add);
    }
    add.addEventListener('click', () => file.click());
    file.addEventListener('change', async () => {
      if (!file.files.length) return;
      msg.textContent = 'Đang tải ' + file.files.length + ' ảnh…';
      try {
        const res = await Api.upload(file.files);
        res.forEach((f) => urls.push(f.url));
        msg.textContent = 'Đã tải xong. Ảnh sẽ được lưu vào hệ thống khi bấm Lưu.';
      } catch (e) { msg.textContent = ''; UI.toast(e.message, true); }
      file.value = ''; draw();
    });
    draw();
    return { node: el('div', {}, grid, file, msg, el('small', { class: 'muted', text: 'JPG, PNG, GIF, WEBP – tối đa 5 MB mỗi ảnh. Ảnh đầu tiên là ảnh đại diện.' })), get: () => urls.slice() };
  }

  // ---------- ô chọn có tìm kiếm ----------
  function refField(f, val) {
    const sel = el('select', {});
    const search = ADMIN_SRC[f.src].url.includes('/options/') ? el('input', { type: 'search', placeholder: 'Gõ để tìm…', class: 'ref-q' }) : null;
    let list = [];
    const fill = (items, keep) => {
      clear(sel).appendChild(el('option', { value: '', text: '— chọn —' }));
      items.forEach((o) => sel.appendChild(el('option', { value: o.v, text: o.t })));
      if (keep !== undefined && keep !== null && keep !== '') {
        if (!items.some((o) => String(o.v) === String(keep))) sel.appendChild(el('option', { value: keep, text: '#' + keep }));
        sel.value = String(keep);
      }
    };
    return loadSrc(f.src).then((items) => {
      list = items; fill(items, val);
      if (search) {
        let t;
        search.addEventListener('input', () => { clearTimeout(t); t = setTimeout(async () => { const keep = sel.value; try { fill(await fetchSrc(f.src, search.value.trim()), keep); } catch (e) { /* bỏ qua */ } }, 250); });
      }
      return { node: el('div', {}, search, sel), input: sel };
    });
  }

  // ---------- form ----------
  async function buildForm(fields, values, opts) {
    opts = opts || {};
    const inputs = {}, custom = {};
    const grid = el('div', { class: 'form-grid' });
    for (const f of fields) {
      if (f.section) { grid.appendChild(el('h4', { class: 'form-sec full', text: f.section })); continue; }
      if (f.adminOnly && !isAdmin) continue;
      const val = values ? values[f.name] : undefined;
      let node, inp;
      if (f.type === 'ref') {
        const r = await refField(f, val); node = r.node; inp = r.input;
      } else if (f.type === 'select') {
        inp = el('select', {}, el('option', { value: '', text: '— chọn —' }), f.options.map((o) => el('option', { value: o.v, text: o.t })));
        if (val !== undefined && val !== null) inp.value = String(val);
        node = inp;
      } else if (f.type === 'bool') {
        inp = el('input', { type: 'checkbox' }); inp.checked = val === undefined ? true : !!val; node = inp;
      } else if (f.type === 'textarea') {
        inp = el('textarea', { rows: f.rows || 3 }); inp.value = val || ''; node = inp;
      } else if (f.type === 'images') {
        const up = uploader((values && (values[f.from || f.name]) || []).map((i) => (typeof i === 'string' ? i : i.url || i.imageUrl)), 30);
        custom[f.name] = up; node = up.node;
      } else if (f.type === 'location') {
        const host = el('div', { class: 'loc-inline' }); node = host;
        custom[f.name] = { picker: null, host, init: values };
      } else {
        const t = { number: 'number', date: 'date', month: 'month', password: 'password', datetime: 'datetime-local' }[f.type] || 'text';
        inp = el('input', { type: t, step: f.type === 'number' ? 'any' : null, autocomplete: t === 'password' ? 'new-password' : 'off' });
        if (val !== undefined && val !== null) inp.value = f.type === 'datetime' ? String(val).replace(' ', 'T').slice(0, 16) : val;
        node = inp;
      }
      if (inp) { if (opts.lock && opts.lock.includes(f.name)) inp.disabled = true; inputs[f.name] = { el: inp, f }; }
      grid.appendChild(f.type === 'bool'
        ? el('div', { class: 'field' + (f.full ? ' full' : '') }, el('label', { class: 'chk' }, inp, f.label))
        : el('div', { class: 'field' + (f.full ? ' full' : '') }, el('label', { text: f.label + (f.req ? ' *' : '') }), node));
    }
    function collect(forEdit) {
      const body = {}, missing = [];
      Object.values(inputs).forEach(({ el: i, f }) => {
        if (i.disabled) return;
        if (f.req && f.type !== 'bool' && i.value.trim() === '' && !(forEdit && f.type === 'password')) missing.push(f.label);
        if (f.type === 'bool') { body[f.name] = i.checked; return; }
        let v = i.value.trim();
        if (v === '') { if (forEdit && f.type !== 'password') body[f.name] = null; return; }
        if (f.type === 'number' || f.type === 'ref') v = Number(v);
        if (f.type === 'datetime') v = v.replace('T', ' ');
        body[f.name] = v;
      });
      Object.entries(custom).forEach(([name, c]) => {
        if (c.get) body[name] = c.get();
        else if (c.picker) {
          const v = c.picker.get();
          body.provinceId = v.provinceId ? Number(v.provinceId) : null;
          body.districtId = v.districtId ? Number(v.districtId) : null;
          body.wardId = v.wardId ? Number(v.wardId) : null;
          if (!v.provinceId) missing.push('Tỉnh / Thành phố (hãy bấm chọn trong danh sách)');
          else if (v.system === 'old' && !v.districtId) missing.push('Quận / Huyện (hệ Cũ)');
        }
      });
      // chủ nhà thêm tòa nhà: tự gán chính mình làm người quản lý
      if (!forEdit && !isAdmin && fields.some((f) => f.name === 'managerId')) body.managerId = me.id;
      if (missing.length) throw new Error('Còn thiếu: ' + missing.join('; ') + '.');
      return body;
    }
    return { node: grid, inputs, custom, collect, mountLocation: async () => {
      for (const c of Object.values(custom)) if (c.host) c.picker = await Loc.picker(c.host, { initial: c.init ? { provinceId: c.init.provinceId, districtId: c.init.districtId, wardId: c.init.wardId } : {} });
    } };
  }

  async function formModal(title, fields, values, onSave, o) {
    const f = await buildForm(fields, values, o);
    const msg = el('div');
    const save = el('button', { class: 'btn', type: 'submit', text: 'Lưu' });
    const form = el('form', {}, el('h3', { text: title }), msg, f.node, el('div', { class: 'form-actions' }, el('button', { class: 'btn light', type: 'button', text: 'Hủy', onclick: () => m.close() }), save));
    const m = UI.modal(form); m.root.firstChild.classList.add('big');
    await f.mountLocation();
    form.addEventListener('submit', async (e) => {
      e.preventDefault(); clear(msg); save.disabled = true;
      try { await onSave(f.collect(!!(o && o.edit)), f); m.close(); } catch (err) { msg.appendChild(UI.alertBox('err', err.message)); save.disabled = false; msg.scrollIntoView({ block: 'nearest' }); }
    });
    return { m, form, f };
  }

  // ---------- bộ lọc theo tòa nhà ----------
  async function buildingSelect(initial) {
    const list = await loadSrc('buildings');
    const sel = el('select', {}, el('option', { value: '', text: 'Tất cả tòa nhà' }), list.map((o) => el('option', { value: o.v, text: o.t })));
    if (initial) sel.value = String(initial);
    return sel;
  }

  // ---------- bảng chung (nhóm Tòa nhà → Phòng) ----------
  function putRows(tbody, items, cols, grouped, rowFn) {
    let lastB = null, lastR = null;
    const span = cols.length + 1;
    items.forEach((row) => {
      if (grouped) {
        if (row.buildingName !== lastB) { tbody.appendChild(el('tr', { class: 'grp-b' }, el('td', { colspan: span, text: '🏢 ' + (row.buildingName || 'Chưa gán tòa nhà') }))); lastB = row.buildingName; lastR = null; }
        const rk = row.roomNumber || null;
        if (rk !== lastR) { tbody.appendChild(el('tr', { class: 'grp-r' }, el('td', { colspan: span, text: rk ? '🚪 Phòng ' + rk : 'Chưa gán phòng' }))); lastR = rk; }
      }
      tbody.appendChild(rowFn(row));
    });
  }

  const visibleCols = (res) => res.cols.filter((c) => !(c[3] === 'adminOnly' && !isAdmin));
  const IDS = (res) => res.id;

  async function listView(res, init) {
    const idKey = IDS(res);
    let page = 1, q = '', status = '', buildingId = (init && init.buildingId) || '';
    const extraQ = Object.assign({}, init);
    delete extraQ.buildingId;
    const readOnly = res.readOnly || (res.roRole && !isAdmin);
    const cols = visibleCols(res);
    const search = el('input', { type: 'search', placeholder: 'Tìm kiếm…' });
    const stSel = res.status ? el('select', {}, el('option', { value: '', text: 'Tất cả trạng thái' }), res.status.map((o) => el('option', { value: o.v, text: o.t }))) : null;
    const bSel = res.buildingFilter ? await buildingSelect(buildingId) : null;
    const tbody = el('tbody'), pagerBox = el('div', { class: 'pager' }), count = el('span', { class: 'muted' }), extra = el('span');
    const bar = el('div', { class: 'bar' }, bSel, search, stSel, count, el('span', { style: 'flex:1' }), extra,
      (res.noCreate || readOnly) ? null : el('button', { class: 'btn', type: 'button', text: '＋ Thêm mới', onclick: () => editRow(null) }));
    view.appendChild(bar);
    view.appendChild(el('div', { class: 'tbl-wrap' }, el('table', { class: 'tbl' }, el('thead', {}, el('tr', {}, cols.map((c) => el('th', { text: c[1] })), el('th', { text: '' }))), tbody)));
    view.appendChild(pagerBox);
    if (!isAdmin && res.grouped) view.appendChild(el('p', { class: 'muted', text: 'Chỉ hiển thị dữ liệu thuộc các tòa nhà do bạn làm chủ.' }));

    if (res.special === 'invoices') extra.appendChild(el('button', { class: 'btn orange', type: 'button', text: '⚙ Tạo hóa đơn tự động', onclick: fn.genInvoice }));

    async function editRow(row) {
      const edit = !!row;
      let full = row;
      if (edit) { try { full = await Api.get('/api/admin/' + res.path + '/' + row[idKey]); } catch (e) { UI.toast(e.message, true); return; } }
      let fields = edit && res.path === 'users' ? res.fields.filter((f) => f.name !== 'username') : res.fields;
      const lockRoom = edit && res.path === 'contracts';
      await formModal((edit ? 'Sửa: ' : 'Thêm: ') + res.label.replace(/^\S+\s/, ''), fields, full, async (body) => {
        if (edit) await Api.put('/api/admin/' + res.path + '/' + row[idKey], body); else await Api.post('/api/admin/' + res.path, body);
        UI.toast('Đã lưu'); delete srcCache.rooms; delete srcCache.tenants; delete srcCache.buildings; delete srcCache.contracts; load();
      }, { edit, lock: lockRoom ? [] : [] });
    }
    async function del(row) {
      if (!confirmBox('Xóa bản ghi này?')) return;
      try { await Api.del('/api/admin/' + res.path + '/' + row[idKey]); UI.toast('Đã xóa'); load(); } catch (e) { UI.toast(e.message, true); }
    }
    function rowNode(row) {
      const tds = cols.map(([k, , fmt]) => {
        const raw = row[k];
        let text = fmt ? fmt(raw, row) : (k === res.statusKey ? statusText(res.status, raw) : (raw === null || raw === undefined ? '' : String(raw)));
        return el('td', { class: typeof raw === 'number' ? 'num' : '', text });
      });
      const acts = el('td', { class: 'acts' });
      if (res.special === 'invoices') acts.appendChild(el('button', { class: 'btn sm orange', type: 'button', text: 'Thanh toán', onclick: () => fn.invoiceDialog(row.invoiceId, load) }));
      if (res.special === 'promotions') acts.appendChild(el('button', { class: 'btn sm orange', type: 'button', text: 'Phòng áp dụng', onclick: () => fn.promoRooms(row.promotionId, load) }));
      if (res.special === 'contracts') {
        acts.appendChild(el('a', { class: 'btn sm orange', href: '/admin/contract.html?id=' + row.contractId, target: '_blank', rel: 'noopener', text: '📄 Xem / In PDF' }));
      }
      if (res.rowActions === 'owner') {
        acts.appendChild(el('a', { class: 'btn sm light', href: '#buildings?managerId=' + row.userId, text: 'Tòa nhà' }));
      }
      if (!readOnly) {
        acts.appendChild(el('button', { class: 'btn sm light', type: 'button', text: 'Sửa', onclick: () => editRow(row) }));
        acts.appendChild(el('button', { class: 'btn sm light', type: 'button', text: 'Xóa', onclick: () => del(row) }));
      }
      tds.push(acts);
      return el('tr', {}, tds);
    }
    async function load() {
      clear(tbody).appendChild(el('tr', {}, el('td', { colspan: cols.length + 1, class: 'muted', text: 'Đang tải…' })));
      try {
        const r = await Api.get('/api/admin/' + res.path, Object.assign({ q, status, buildingId, page, size: res.grouped ? 30 : 15 }, extraQ));
        clear(tbody);
        count.textContent = r.totalItems + ' bản ghi';
        if (!r.items.length) tbody.appendChild(el('tr', {}, el('td', { colspan: cols.length + 1, class: 'empty', text: 'Không có dữ liệu' })));
        putRows(tbody, r.items, cols, res.grouped, rowNode);
        UI.pager(pagerBox, r.page, r.totalPages, (p) => { page = p; load(); });
      } catch (e) { clear(tbody).appendChild(el('tr', {}, el('td', { colspan: cols.length + 1 }, UI.alertBox('err', e.message)))); }
    }
    let t; search.addEventListener('input', () => { clearTimeout(t); t = setTimeout(() => { q = search.value.trim(); page = 1; load(); }, 300); });
    if (stSel) stSel.addEventListener('change', () => { status = stSel.value; page = 1; load(); });
    if (bSel) bSel.addEventListener('change', () => { buildingId = bSel.value; page = 1; load(); });
    load();
  }

  // ---------- nạp 5 mô-đun theo phân công (admin/modules/m1.js … m5.js) ----------
  const fn = {}, views = {};
  const C = { el, $, clear, UI, Api, view, me, isAdmin, srcCache, confirmBox, statusText, fetchSrc, loadSrc, uploader, refField, buildForm, formModal, buildingSelect, putRows, visibleCols, listView, fn, views, route: () => route() };
  const ADMIN_RES = [];
  ADMIN_MODULES.forEach((m) => { m.init(C); m.resources.forEach((r) => { r.group = m.title; r.moduleId = m.id; ADMIN_RES.push(r); }); });
  // Tổng quan luôn đứng đầu thanh bên
  const dash = ADMIN_RES.findIndex((r) => r.key === 'dashboard');
  if (dash > 0) { const [d] = ADMIN_RES.splice(dash, 1); d.group = 'Tổng quan'; ADMIN_RES.unshift(d); }

  // ---------- router ----------
  function parseHash() {
    const h = location.hash.slice(1) || 'dashboard';
    const [key, qs] = h.split('?');
    return { key, params: Object.fromEntries(new URLSearchParams(qs || '')) };
  }
  function drawSide(active) {
    clear(side);
    side.appendChild(el('div', { class: 'who-box' }, el('b', { text: me.fullName }), el('small', { text: isAdmin ? 'Quản trị viên – xem toàn hệ thống' : 'Chủ tòa nhà – chỉ dữ liệu của bạn' })));
    let lastGroup = null;
    ADMIN_RES.filter((r) => isAdmin || !r.adminOnly).forEach((r) => {
      if (r.group && r.group !== lastGroup) { side.appendChild(el('h4', { text: r.group })); lastGroup = r.group; }
      side.appendChild(el('a', { href: '#' + r.key, class: r.key === active ? 'on' : '', text: r.label }));
    });
    if (!isAdmin) side.appendChild(el('a', { href: '/profile.html#manager', class: 'quit', text: '⚠ Hủy quyền quản lý' }));
  }
  async function route() {
    const { key, params } = parseHash();
    const res = ADMIN_RES.find((r) => r.key === key && (isAdmin || !r.adminOnly)) || ADMIN_RES[0];
    document.querySelectorAll('.modal-bg').forEach((n) => n.remove());
    drawSide(res.key);
    clear(view).appendChild(el('h1', { text: res.label }));
    try {
      if (res.special && views[res.special]) await views[res.special](params);
      else await listView(res, params);
    } catch (e) { view.appendChild(UI.alertBox('err', e.message)); }
  }
  window.addEventListener('hashchange', route);
  route();
})();
