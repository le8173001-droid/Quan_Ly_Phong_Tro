(async function () {
  const { el, $, clear } = UI;
  await Layout.init('home');
  

  function goSearch(p) { location.href = '/search.html' + Api.qs(p); }
  SearchBox.mount($('#search-host'), { onSubmit: goSearch }).catch((e) => { clear($('#search-host')).appendChild(UI.alertBox('err', e.message)); });

  // 6 ô chức năng nhanh
  const q = $('#quick');
  const tile = (icon, label, sub, attrs, tag, pro) => {
    const kids = [el('span', { class: 'ico', text: icon }), el('b', { text: label }), el('small', { text: sub }), tag ? el('span', { class: 'tag' + (pro ? ' pro' : ''), text: tag }) : null];
    return attrs.onclick ? el('button', { type: 'button', ...attrs }, kids) : el('a', attrs, kids);
  };
  q.appendChild(tile('📍', 'Tìm trọ gần tôi', 'Phòng quanh vị trí của bạn', { onclick: nearMe }));
  q.appendChild(tile('🙋', 'Hỗ trợ tìm phòng', 'Tư vấn viên giúp miễn phí', { href: '/support.html' }));
  q.appendChild(tile('🧾', 'Tra cứu hóa đơn', 'Xem & kiểm tra tiền phòng', { href: '/invoice.html' }));
  q.appendChild(tile('🎁', 'Ưu đãi & voucher', 'Săn deal giá sốc mỗi ngày', { href: '/promotions.html' }));
  q.appendChild(tile('🤝', 'Tuyển môi giới', 'Kiếm thêm thu nhập', { href: '/broker.html' }, 'HOT'));
  const meNow = Layout.getMe();
  if (meNow && meNow.role === 'MANAGER') { const lm = $('#landlord-more'); if (lm) lm.closest('section').classList.add('hidden'); }
  if (meNow && (meNow.role === 'MANAGER' || meNow.role === 'ADMIN')) q.appendChild(tile('🏢', 'Quản lý tòa nhà', 'Vào trang quản lý của bạn', { href: '/admin/index.html' }, 'PRO', true));
  else q.appendChild(tile('🏠', 'Tôi là chủ nhà', 'Đăng tin & lấp đầy phòng', { href: '/landlord.html' }, 'PRO', true));

  async function nearStrip() {
    const box = clear($('#near-areas'));
    $('#near-btn').disabled = true;
    try {
      const r = await Loc.nearby(5);
      if (!r.areas.length) box.appendChild(el('span', { class: 'muted', text: 'Chưa có khu vực nào gần bạn có phòng trống.' }));
      r.areas.forEach((a) => box.appendChild(el('a', { href: '/search.html' + Api.qs({ provinceId: a.provinceId, districtId: a.districtId, wardId: a.wardId }) },
        a.label, el('small', { text: a.roomCount + ' phòng · cách ' + String(a.distanceKm).replace('.', ',') + ' km · từ ' + UI.price(a.minPrice) }))));
    } catch (e) { box.appendChild(UI.alertBox('err', e.message)); }
    $('#near-btn').disabled = false;
  }
  $('#near-btn').addEventListener('click', nearStrip);

  function nearMe() {
    if (!navigator.geolocation) { UI.toast('Trình duyệt không hỗ trợ định vị', true); return; }
    UI.toast('Đang lấy vị trí của bạn…');
    navigator.geolocation.getCurrentPosition(
      (pos) => goSearch({ lat: pos.coords.latitude.toFixed(5), lng: pos.coords.longitude.toFixed(5), radiusKm: 5, sort: 'distance' }),
      () => UI.toast('Không lấy được vị trí. Hãy cho phép truy cập vị trí hoặc chọn trường / nơi làm việc ở ô tìm kiếm.', true),
      { timeout: 10000 });
  }

  const grid = (host, rooms, empty) => {
    clear(host);
    if (!rooms.length) host.appendChild(el('div', { class: 'empty', text: empty || 'Chưa có phòng phù hợp.' }));
    rooms.forEach((r) => host.appendChild(UI.roomCard(r)));
  };

  const TYPES = [[1, 'Phòng trọ'], [2, 'Ký túc xá'], [3, 'Nhà'], [4, 'Chung cư'], [5, 'Văn phòng'], [6, 'Phòng pass & ở ghép']];

  async function load() {
    const d = await Api.get('/api/home');
    $('#hero-sub').textContent = d.stats.availableRooms + ' phòng đang trống – ' + (d.settings.tagline || '');

    // số liệu trong hero
    const hs = clear($('#hero-stats'));
    [['rooms', 'Phòng đăng'], ['members', 'Thành viên'], ['provinces', 'Tỉnh / thành']].forEach(([k, label]) =>
      hs.appendChild(el('div', {}, el('b', { text: Number(d.stats[k] || 0).toLocaleString('vi-VN') + '+' }), el('span', { text: label }))));

    // chương trình + voucher + đối tác
    const pr = d.promotions;
    const host = clear($('#programs'));
    const items = [].concat(pr.programs, pr.vouchers, pr.partners);
    if (items.length) {
      host.appendChild(el('div', { class: 'section-head' }, el('div', {}, el('h2', { text: '🎁 Chương trình & ưu đãi đang diễn ra' }), el('p', { text: 'Săn ưu đãi mới nhất dành cho bạn' })), el('a', { class: 'more', href: '/promotions.html', text: 'Xem tất cả ›' })));
      const wrap = el('div', { class: 'programs' });
      items.forEach((p) => {
        const left = el('div', { class: 'left' });
        UI.countdown(left, p.secondsLeft);
        wrap.appendChild(el('a', { class: 'program ' + (p.bannerColor || 'green'), href: p.linkUrl && p.linkUrl.startsWith('/') ? p.linkUrl : '/promotions.html' },
          el('div', { class: 'banner' }, el('div', { class: 'big', text: p.discountText || p.title }),
            p.voucherCode ? el('div', { class: 'code' }, 'Mã: ', el('b', { text: p.voucherCode })) : null),
          el('div', { class: 'cap' }, el('b', { text: p.title }), el('span', { text: p.description || '' })), left));
      });
      host.appendChild(wrap);
    }

    // HOTDEAL
    const hsec = $('#hotdeal-sec');
    const hd = clear($('#hotdeals'));
    hsec.classList.toggle('hidden', !pr.hotdeals.length);
    pr.hotdeals.forEach((h, i) => {
      const timer = el('span', { class: 'timer' });
      UI.countdownBoxes(timer, h.secondsLeft);
      const rooms = el('div', { class: 'hscroll' });
      (h.rooms || []).forEach((r) => rooms.appendChild(UI.roomCard(r)));
      const wrapper = el('div', { class: 'hotdeal' },
        el('div', { class: 'hd-head' },
          el('span', { class: 'hd-badge', text: '🔥 HOTDEAL' }),
          el('div', {}, el('div', { class: 'hd-sub', text: 'Độc quyền chỉ có tại NhàTrọ+' }), el('div', { class: 'hd-title', text: h.title })),
          el('div', { class: 'hd-time' }, el('span', { class: 'lbl', text: 'Kết thúc sau' }), timer, el('a', { class: 'btn sm orange', href: '/search.html?hotdeal=true', text: 'Xem tất cả' }))),
        rooms);
      if (i) wrapper.style.marginTop = '22px';
      hd.appendChild(wrapper);
    });

    grid($('#now-rooms'), d.nowRooms.slice(0, 8), 'Hiện chưa có phòng dọn vào ngay.');
    const lt = clear($('#latest'));
    if (!d.latestRooms.length) lt.appendChild(el('div', { class: 'empty', text: 'Chưa có phòng phù hợp.' }));
    d.latestRooms.forEach((r) => lt.appendChild(UI.roomCard(r, { row: true })));
    grid($('#hot'), d.hotRooms);

    // các mục nổi bật: Ký túc xá / Nhà / Chung cư / Phòng trọ nổi bật
    const fh = clear($('#featured'));
    (d.featured || []).forEach((sec) => {
      const wrap = el('div', { class: 'container' },
        el('div', { class: 'section-head' }, el('div', {}, el('h2', { text: sec.title }), el('p', { text: 'Được chọn lọc và xem nhiều' })),
          el('a', { class: 'more', href: '/search.html?typeId=' + sec.type.typeId, text: 'Xem tất cả ›' })));
      const g = el('div', { class: 'grid' });
      sec.rooms.forEach((r) => g.appendChild(UI.roomCard(r)));
      wrap.appendChild(g);
      fh.appendChild(el('section', { class: 'section' }, wrap));
    });

    // địa chỉ (phường/xã, quận/huyện) của các phòng đã đăng
    const ar = clear($('#areas'));
    (d.popularAreas || []).forEach((a) => ar.appendChild(el('a', { href: '/search.html' + Api.qs({ provinceId: a.provinceId, districtId: a.districtId, wardId: a.wardId }), text: '📍 ' + a.label + ' · ' + a.roomCount + ' phòng' })));
    if (!d.popularAreas || !d.popularAreas.length) ar.appendChild(el('span', { class: 'muted', text: 'Chưa có phòng nào được đăng.' }));
    loadLandmarks();

    // thống kê + hotline
    const st = clear($('#stats'));
    [['rooms', 'Phòng đăng'], ['buildings', 'Khu trọ'], ['members', 'Thành viên'], ['provinces', 'Tỉnh / thành']].forEach(([k, label]) =>
      st.appendChild(el('div', { class: 'stat' }, el('b', { text: Number(d.stats[k] || 0).toLocaleString('vi-VN') }), label)));
    const hl = clear($('#hotlines'));
    const hot = (icon, label, value, href, ext) => el('a', Object.assign({ href }, ext ? { target: '_blank', rel: 'noopener' } : {}), el('span', { class: 'hi', text: icon }), el('span', {}, label, el('b', { text: value })));
    hl.appendChild(hot('🎧', 'Hotline hỗ trợ', d.settings.hotline || '', 'tel:' + (d.settings.hotline || '').replace(/[^0-9]/g, '')));
    if (d.settings.zalo) hl.appendChild(hot('💬', 'Zalo', d.settings.zalo, 'https://zalo.me/' + d.settings.zalo.replace(/[^0-9]/g, ''), true));
    if (d.settings.email) hl.appendChild(hot('✉️', 'Email', d.settings.email, 'mailto:' + d.settings.email));
  }

  // trường học / nơi làm việc: mặc định là tất cả, hoặc lọc theo loại và theo vị trí thiết bị
  let lmType = '', lmPos = null;
  async function loadLandmarks() {
    const box = clear($('#landmarks'));
    const note = $('#lm-note');
    try {
      const p = { type: lmType };
      if (lmPos) { p.lat = lmPos.lat; p.lng = lmPos.lng; p.radiusKm = 15; }
      const list = await Api.get('/api/landmarks', p);
      note.textContent = lmPos ? (list.length ? 'Trong bán kính 15 km quanh vị trí của bạn, gần nhất trước:' : 'Không có trường học / nơi làm việc nào trong 15 km quanh bạn.') : '';
      list.forEach((l) => box.appendChild(el('a', { href: '/search.html?landmarkId=' + l.landmarkId + '&radiusKm=3', text: (l.landmarkType === 'school' ? '🎓 ' : '🏢 ') + l.name + (l.distanceKm !== undefined && l.distanceKm !== null ? ' · ' + l.distanceKm.toFixed(1).replace('.', ',') + ' km' : '') })));
    } catch (e) { box.appendChild(UI.alertBox('err', e.message)); }
  }
  document.querySelectorAll('.lm-filter [data-t]').forEach((b) => b.addEventListener('click', () => {
    lmType = b.dataset.t; document.querySelectorAll('.lm-filter [data-t]').forEach((x) => x.classList.toggle('on', x === b)); loadLandmarks();
  }));
  $('#lm-near').addEventListener('click', () => {
    if (!navigator.geolocation) { UI.toast('Trình duyệt không hỗ trợ định vị', true); return; }
    $('#lm-note').textContent = 'Đang lấy vị trí của bạn…';
    navigator.geolocation.getCurrentPosition(
      (pos) => { lmPos = { lat: pos.coords.latitude.toFixed(5), lng: pos.coords.longitude.toFixed(5) }; loadLandmarks(); },
      () => { $('#lm-note').textContent = 'Không lấy được vị trí. Hãy cho phép truy cập vị trí trong trình duyệt.'; },
      { timeout: 10000, maximumAge: 60000 });
  });

  try { await load(); } catch (e) { UI.toast(e.message, true); }
})();
