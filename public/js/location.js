/* Chọn địa chỉ hành chính Việt Nam: Tỉnh/Thành → Quận/Huyện (chỉ hệ CŨ) → Phường/Xã.
   Hai hệ: "Mới" (34 tỉnh, 2 cấp, sau sáp nhập 2025) và "Cũ" (63 tỉnh, 3 cấp).
   Dùng:  const pk = await Loc.picker(host, { initial:{provinceId,districtId,wardId}, allowAll:true, onChange(v){} });
          pk.get() -> { system, provinceId, districtId, wardId, label } */
(function (w) {
  const { el, clear } = UI;
  const collator = new Intl.Collator('vi', { numeric: true, sensitivity: 'base' });
  const fold = (s) => String(s || '').normalize('NFD').replace(/[̀-ͯ]/g, '').replace(/đ/g, 'd').replace(/Đ/g, 'D').toLowerCase();
  const cache = {};
  async function cached(url, params) {
    const k = url + Api.qs(params);
    if (!cache[k]) cache[k] = Api.get(url, params);
    return cache[k];
  }
  const natural = (arr) => arr.slice().sort((a, b) => collator.compare(a.name, b.name));

  /* Ô chọn có tìm kiếm: input + danh sách lọc theo gõ. */
  function combo(placeholder, onPick) {
    let items = [];
    let value = null;
    const input = el('input', { type: 'text', placeholder, autocomplete: 'off', class: 'cb-input' });
    const list = el('div', { class: 'cb-list hidden', role: 'listbox' });
    const box = el('div', { class: 'cb' }, input, list);
    const clearBtn = el('button', { type: 'button', class: 'cb-x hidden', 'aria-label': 'Xóa', text: '×' });
    box.appendChild(clearBtn);

    function draw(filter) {
      clear(list);
      const f = fold(filter);
      const shown = items.filter((i) => !f || fold(i.name).includes(f)).slice(0, 300);
      if (!shown.length) list.appendChild(el('div', { class: 'cb-empty', text: 'Không có kết quả' }));
      shown.forEach((i) => list.appendChild(el('button', { type: 'button', class: 'cb-opt' + (value && value.id === i.id ? ' on' : ''), onclick: () => pick(i) },
        el('span', { text: i.name }), i.roomCount ? el('small', { text: i.roomCount + ' phòng' }) : null)));
    }
    function pick(i) {
      value = i; input.value = i ? i.name : ''; list.classList.add('hidden'); clearBtn.classList.toggle('hidden', !i);
      if (onPick) onPick(i);
    }
    input.addEventListener('focus', () => { input.select(); draw(''); list.classList.remove('hidden'); });
    input.addEventListener('input', () => { draw(input.value); list.classList.remove('hidden'); });
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') { e.preventDefault(); const first = list.querySelector('.cb-opt'); if (first && !list.classList.contains('hidden')) first.click(); }
      if (e.key === 'Escape') list.classList.add('hidden');
    });
    clearBtn.addEventListener('click', () => pick(null));
    // gõ xong mà chưa bấm chọn: tự chọn nếu trùng đúng tên hoặc chỉ có 1 kết quả
    function commit() {
      const t = fold(input.value).trim();
      list.classList.add('hidden');
      if (!t) { if (value && input.value.trim() === '') pick(null); return; }
      if (value && fold(value.name) === t) { input.value = value.name; return; }
      const exact = items.filter((i) => fold(i.name) === t);
      const part = items.filter((i) => fold(i.name).includes(t));
      const m = exact.length === 1 ? exact[0] : (part.length === 1 ? part[0] : null);
      if (m) pick(m); else input.value = value ? value.name : '';
    }
    document.addEventListener('click', (e) => { if (!box.contains(e.target) && !list.classList.contains('hidden')) commit(); });
    input.addEventListener('blur', () => setTimeout(() => { if (!box.contains(document.activeElement) && !list.classList.contains('hidden')) commit(); }, 150));
    return {
      box,
      setItems(arr) { items = arr; draw(''); },
      set(i) { value = i; input.value = i ? i.name : ''; clearBtn.classList.toggle('hidden', !i); },
      get: () => value,
      disable(d) { input.disabled = d; box.classList.toggle('off', d); },
      hide(h) { box.parentNode && box.parentNode.classList.toggle('hidden', h); },
    };
  }

  async function picker(host, opts) {
    opts = opts || {};
    const init = opts.initial || {};
    const allowAll = opts.allowAll !== false;
    let system = (init.system || 'new').toLowerCase();

    const sysNew = el('button', { type: 'button', class: 'seg-b', text: 'Mới (34 tỉnh)' });
    const sysOld = el('button', { type: 'button', class: 'seg-b', text: 'Cũ (63 tỉnh)' });
    const sw = el('div', { class: 'seg', role: 'group', 'aria-label': 'Hệ đơn vị hành chính' }, sysNew, sysOld);

    const fire = () => { if (opts.onChange) opts.onChange(api.get()); };
    const cp = combo('Tỉnh / Thành phố', async (p) => { cd.set(null); cw.set(null); await loadDistricts(); await loadWards(); fire(); });
    const cd = combo('Quận / Huyện', async (d) => { cw.set(null); await loadWards(); fire(); });
    const cw = combo('Phường / Xã', () => fire());
    const rowD = el('div', { class: 'loc-f' }, el('label', { text: 'Quận / Huyện' }), cd.box);
    const rowW = el('div', { class: 'loc-f' }, el('label', { text: 'Phường / Xã / Thị trấn' }), cw.box);
    const rowP = el('div', { class: 'loc-f' }, el('label', { text: 'Tỉnh / Thành phố' }), cp.box);
    const hint = el('div', { class: 'loc-hint' });
    const root = el('div', { class: 'loc' }, el('div', { class: 'loc-f' }, el('label', { text: 'Đơn vị hành chính' }), sw), rowP, rowD, rowW, hint);
    clear(host).appendChild(root);

    function paintSys() {
      sysNew.classList.toggle('on', system === 'new');
      sysOld.classList.toggle('on', system === 'old');
      rowD.classList.toggle('hidden', system === 'new');
      hint.textContent = system === 'new' ? 'Hệ mới: Tỉnh/Thành → Phường/Xã (không còn cấp Quận/Huyện).' : 'Hệ cũ: Tỉnh/Thành → Quận/Huyện → Phường/Xã (dữ liệu trước 07/2025).';
    }
    async function loadProvinces() {
      const ps = await cached('/api/locations/provinces', { system });
      cp.setItems(ps.map((p) => ({ id: p.provinceId, name: p.name, roomCount: p.roomCount, system: p.system })));
    }
    async function loadDistricts() {
      const p = cp.get();
      cd.setItems([]); cd.disable(!p || system === 'new');
      if (!p || system === 'new') return;
      const ds = await cached('/api/locations/districts', { provinceId: p.id });
      cd.setItems(natural(ds.map((d) => ({ id: d.districtId, name: d.name, roomCount: d.roomCount }))));
    }
    async function loadWards() {
      const p = cp.get(), d = cd.get();
      cw.setItems([]);
      const needDistrict = system === 'old';
      cw.disable(!p || (needDistrict && !d));
      if (!p || (needDistrict && !d)) return;
      const ws = await cached('/api/locations/wards', d ? { districtId: d.id } : { provinceId: p.id });
      cw.setItems(natural(ws.map((x) => ({ id: x.wardId, name: x.name, roomCount: x.roomCount }))));
    }
    async function setSystem(s, keep) {
      system = s; paintSys();
      if (!keep) { cp.set(null); cd.set(null); cw.set(null); }
      await loadProvinces(); await loadDistricts(); await loadWards();
      if (!keep) fire();
    }
    sysNew.addEventListener('click', () => system !== 'new' && setSystem('new'));
    sysOld.addEventListener('click', () => system !== 'old' && setSystem('old'));

    // khôi phục giá trị ban đầu (từ URL)
    paintSys();
    if (init.provinceId && !init.system) {
      try {
        for (const s of ['new', 'old']) {
          const ps = await cached('/api/locations/provinces', { system: s });
          if (ps.some((p) => String(p.provinceId) === String(init.provinceId))) { system = s; break; }
        }
        paintSys();
      } catch (e) { /* bỏ qua */ }
    }
    await loadProvinces();
    if (init.provinceId) {
      const ps = await cached('/api/locations/provinces', { system });
      const p = ps.find((x) => String(x.provinceId) === String(init.provinceId));
      if (p) {
        cp.set({ id: p.provinceId, name: p.name });
        await loadDistricts();
        if (init.districtId && system === 'old') {
          const ds = await cached('/api/locations/districts', { provinceId: p.provinceId });
          const d = ds.find((x) => String(x.districtId) === String(init.districtId));
          if (d) cd.set({ id: d.districtId, name: d.name });
        }
        await loadWards();
        if (init.wardId) {
          const ws = await cached('/api/locations/wards', cd.get() ? { districtId: cd.get().id } : { provinceId: p.provinceId });
          const x = ws.find((q) => String(q.wardId) === String(init.wardId));
          if (x) cw.set({ id: x.wardId, name: x.name });
        }
      }
    } else { await loadDistricts(); await loadWards(); }

    const api = {
      get() {
        const p = cp.get(), d = cd.get(), x = cw.get();
        const parts = [x && x.name, d && system === 'old' && d.name, p && p.name].filter(Boolean);
        return { system, provinceId: p ? p.id : '', districtId: d && system === 'old' ? d.id : '', wardId: x ? x.id : '', label: parts.join(', ') };
      },
      set(v) { /* dùng khi cần gán lại từ ngoài */ if (!v) { cp.set(null); cd.set(null); cw.set(null); } },
    };
    return api;
  }

  /* Gợi ý khu vực gần bạn (cần cho phép định vị). cb(areas) */
  function nearby(radiusKm) {
    return new Promise((resolve, reject) => {
      if (!navigator.geolocation) return reject(new Error('Trình duyệt không hỗ trợ định vị'));
      navigator.geolocation.getCurrentPosition(
        async (pos) => {
          try {
            const lat = pos.coords.latitude.toFixed(5), lng = pos.coords.longitude.toFixed(5);
            const r = await Api.get('/api/locations/nearby', { lat, lng, radiusKm });
            resolve({ lat, lng, areas: r.areas || r.items || r });
          } catch (e) { reject(e); }
        },
        () => reject(new Error('Không lấy được vị trí. Hãy cho phép truy cập vị trí trong trình duyệt.')),
        { timeout: 10000, maximumAge: 60000 });
    });
  }

  w.Loc = { picker, nearby, fold, natural };
})(window);
