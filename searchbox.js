/* Ô tìm kiếm kiểu Lozido: gợi ý loại hình + Khu vực / Gần nơi học tập-làm việc / Giá / Diện tích + nút Tìm kiếm.
   Dùng ở trang chủ (hero) và trang kết quả (thanh lọc). */
(function (w) {
  const { el } = UI;

  const PRICES = [['', 'Mọi mức giá'], ['0-1500000', 'Dưới 1,5 triệu'], ['1500000-3000000', '1,5 – 3 triệu'], ['3000000-5000000', '3 – 5 triệu'], ['5000000-10000000', '5 – 10 triệu'], ['10000000-', 'Trên 10 triệu']];
  const AREAS = [['', 'Mọi diện tích'], ['0-20', 'Dưới 20 m²'], ['20-30', '20 – 30 m²'], ['30-50', '30 – 50 m²'], ['50-80', '50 – 80 m²'], ['80-', 'Trên 80 m²']];
  const RADIUS = [['2', '2 km'], ['3', '3 km'], ['5', '5 km'], ['10', '10 km']];

  function opt(value, label, selected) { return el('option', { value, selected: selected ? true : null, text: label }); }
  function field(label, node, wide) { return el('div', { class: 'sb-seg' + (wide ? ' wide' : '') }, el('label', { text: label }), node); }

  function rangeValue(min, max) {
    if (min || max) return (min || '0') + '-' + (max || '');
    return '';
  }

  /* meta: kết quả /api/meta; opts: {initial, onSubmit(params), compact} */
  async function mount(host, opts) {
    opts = opts || {};
    const init = opts.initial || {};
    const [meta, landmarks] = await Promise.all([Api.get('/api/meta'), Api.get('/api/landmarks')]);
    if (!window.Loc) throw new Error('Thiếu location.js');
    let typeId = init.typeId || '';
    const state = { lat: init.lat || '', lng: init.lng || '' };

    // loại hình (chip gợi ý)
    const chips = el('div', { class: 'sb-chips' });
    function drawChips() {
      UI.clear(chips);
      chips.appendChild(el('button', { type: 'button', class: 'chip' + (!typeId ? ' on' : ''), text: 'Tất cả', onclick: () => { typeId = ''; drawChips(); } }));
      meta.roomTypes.forEach((t) => chips.appendChild(el('button', { type: 'button', class: 'chip' + (String(typeId) === String(t.typeId) ? ' on' : ''), text: t.typeName, onclick: () => { typeId = t.typeId; drawChips(); } })));
    }
    drawChips();

    // từ khoá + gợi ý
    const kw = el('input', { type: 'search', placeholder: 'Nhập tên đường, khu vực, tên trường…', autocomplete: 'off', value: init.keyword || '' });
    const sugg = el('div', { class: 'sb-sugg hidden' });
    let timer = null;
    kw.addEventListener('input', () => {
      clearTimeout(timer);
      const q = kw.value.trim();
      if (q.length < 1) { sugg.classList.add('hidden'); return; }
      timer = setTimeout(async () => {
        try {
          const r = await Api.get('/api/search/suggest', { q });
          UI.clear(sugg);
          const add = (icon, text, fn) => sugg.appendChild(el('button', { type: 'button', class: 'sg', onclick: fn }, el('span', { text: icon }), el('span', { text })));
          (r.roomTypes || []).forEach((t) => add('🏠', t.typeName, () => { typeId = t.typeId; drawChips(); kw.value = ''; sugg.classList.add('hidden'); }));
          (r.areas || []).forEach((x) => add('📍', x.label + (x.system === 'OLD' ? ' (cũ)' : ''), () => {
            Object.assign(area, { provinceId: x.provinceId || '', districtId: x.districtId || '', wardId: x.wardId || '', label: x.label }); paintArea(); kw.value = ''; sugg.classList.add('hidden'); }));
          (r.landmarks || []).forEach((l) => add('🎓', l.name, () => { near.value = l.landmarkId; kw.value = ''; sugg.classList.add('hidden'); }));
          sugg.classList.toggle('hidden', !sugg.children.length);
        } catch (e) { sugg.classList.add('hidden'); }
      }, 200);
    });
    document.addEventListener('click', (e) => { if (!host.contains(e.target)) sugg.classList.add('hidden'); });

    // khu vực: nút mở bảng chọn Tỉnh → Quận → Phường (hệ Mới / Cũ) + gợi ý gần bạn
    const area = { system: '', provinceId: init.provinceId || '', districtId: init.districtId || '', wardId: init.wardId || '', label: '' };
    const areaBtn = el('button', { type: 'button', class: 'sb-area', text: 'Chọn tỉnh, quận, phường…' });
    const pop = el('div', { class: 'loc-pop hidden' });
    const pickHost = el('div', {});
    const nearBox = el('div', { class: 'loc-near' });
    const nearBtn = el('button', { type: 'button', class: 'btn sm light', text: '📍 Gợi ý khu vực gần tôi' });
    const doneBtn = el('button', { type: 'button', class: 'btn sm', text: 'Áp dụng' });
    const resetBtn = el('button', { type: 'button', class: 'btn sm ghost', text: 'Bỏ chọn' });
    pop.appendChild(pickHost);
    pop.appendChild(el('div', { class: 'loc-actions' }, nearBtn, el('span', { class: 'sp' }), resetBtn, doneBtn));
    pop.appendChild(nearBox);
    const areaWrap = el('div', { class: 'sb-areawrap' }, areaBtn, pop);
    let picker = null;
    function paintArea() { areaBtn.textContent = area.label || 'Chọn tỉnh, quận, phường…'; areaBtn.classList.toggle('has', !!area.label); }
    Loc.picker(pickHost, {
      initial: init, allowAll: true,
      onChange: (v) => { Object.assign(area, v); paintArea(); },
    }).then((p) => { picker = p; const v = p.get(); if (v.label) { Object.assign(area, v); paintArea(); } });
    areaBtn.addEventListener('click', () => pop.classList.toggle('hidden'));
    doneBtn.addEventListener('click', () => pop.classList.add('hidden'));
    resetBtn.addEventListener('click', () => { Object.assign(area, { provinceId: '', districtId: '', wardId: '', label: '' }); picker && picker.set(null); paintArea(); });
    document.addEventListener('click', (e) => { if (!areaWrap.contains(e.target)) pop.classList.add('hidden'); });
    nearBtn.addEventListener('click', async () => {
      nearBtn.disabled = true; UI.clear(nearBox).appendChild(el('div', { class: 'muted', text: 'Đang lấy vị trí…' }));
      try {
        const r = await Loc.nearby(5);
        state.lat = r.lat; state.lng = r.lng;
        UI.clear(nearBox);
        if (!r.areas.length) nearBox.appendChild(el('div', { class: 'muted', text: 'Chưa có khu vực nào gần bạn có phòng trống.' }));
        else nearBox.appendChild(el('div', { class: 'muted', text: 'Khu vực gần bạn có phòng trống:' }));
        r.areas.forEach((a) => nearBox.appendChild(el('button', { type: 'button', class: 'chip', onclick: () => {
          Object.assign(area, { provinceId: a.provinceId || '', districtId: a.districtId || '', wardId: a.wardId || '', label: a.label });
          paintArea(); pop.classList.add('hidden');
        } }, a.label + ' · ' + a.roomCount + ' phòng · ' + a.distanceKm.toString().replace('.', ',') + ' km')));
      } catch (e) { UI.clear(nearBox).appendChild(UI.alertBox('err', e.message)); }
      nearBtn.disabled = false;
    });

    // gần nơi học tập / làm việc
    const near = el('select', {}, opt('', 'Chọn trường / nơi làm việc'),
      el('optgroup', { label: 'Trường học' }, landmarks.filter((l) => l.landmarkType === 'school').map((l) => opt(l.landmarkId, l.name, String(init.landmarkId) === String(l.landmarkId)))),
      el('optgroup', { label: 'Nơi làm việc' }, landmarks.filter((l) => l.landmarkType !== 'school').map((l) => opt(l.landmarkId, l.name, String(init.landmarkId) === String(l.landmarkId)))));
    const radius = el('select', {}, RADIUS.map(([v, t]) => opt(v, t, String(init.radiusKm || '5') === v)));

    const price = el('select', {}, PRICES.map(([v, t]) => opt(v, t, v === rangeValue(init.minPrice, init.maxPrice))));
    const areaSel = el('select', {}, AREAS.map(([v, t]) => opt(v, t, v === rangeValue(init.minArea, init.maxArea))));

    function collect() {
      const p = { typeId, keyword: kw.value.trim(), provinceId: area.provinceId, districtId: area.districtId, wardId: area.wardId };
      if (near.value) { p.landmarkId = near.value; p.radiusKm = radius.value; }
      else if (state.lat && state.lng && !area.label && !kw.value.trim()) { p.lat = state.lat; p.lng = state.lng; p.radiusKm = radius.value; }
      const [pmin, pmax] = price.value ? price.value.split('-') : ['', ''];
      const [amin, amax] = areaSel.value ? areaSel.value.split('-') : ['', ''];
      p.minPrice = pmin; p.maxPrice = pmax; p.minArea = amin; p.maxArea = amax;
      return p;
    }

    const form = el('form', { class: 'search-form', autocomplete: 'off' },
      chips,
      el('div', { class: 'sb-pill' },
        el('div', { class: 'sb-seg wide' }, el('label', { text: 'Tìm trọ' }), el('div', { class: 'sb-kw' }, kw, sugg)),
        field('Khu vực', areaWrap, true),
        field('Gần nơi học tập / làm việc', el('div', { class: 'sb-pair' }, near, radius), true),
        field('Giá', price),
        field('Diện tích', areaSel),
        el('div', { class: 'sb-submit' }, el('button', { class: 'btn orange', type: 'submit', text: '🔍 Tìm kiếm' }))));
    form.addEventListener('submit', (e) => { e.preventDefault(); sugg.classList.add('hidden'); opts.onSubmit(collect()); });
    UI.clear(host).appendChild(form);
    return { collect, setLocation: (lat, lng) => { state.lat = lat; state.lng = lng; near.value = ''; } };
  }

  w.SearchBox = { mount };
})(window);
