/* Hàm dựng giao diện an toàn (không dùng innerHTML với dữ liệu người dùng). */
(function (w) {
  function el(tag, attrs, ...kids) {
    const n = document.createElement(tag);
    if (attrs) {
      for (const [k, v] of Object.entries(attrs)) {
        if (v === undefined || v === null || v === false) continue;
        if (k === 'class') n.className = v;
        else if (k === 'text') n.textContent = v;
        else if (k.startsWith('on') && typeof v === 'function') n.addEventListener(k.slice(2), v);
        else if (k === 'dataset') Object.assign(n.dataset, v);
        else n.setAttribute(k, v === true ? '' : v);
      }
    }
    kids.flat().forEach((c) => {
      if (c === null || c === undefined || c === false) return;
      n.appendChild(typeof c === 'string' || typeof c === 'number' ? document.createTextNode(String(c)) : c);
    });
    return n;
  }

  const $ = (s, r) => (r || document).querySelector(s);
  const $$ = (s, r) => Array.from((r || document).querySelectorAll(s));
  const clear = (n) => { while (n.firstChild) n.removeChild(n.firstChild); return n; };

  /* 2500000 -> "2,5 triệu"; 850000 -> "850 nghìn" */
  function price(n) {
    if (n === null || n === undefined) return '—';
    if (n >= 1e6) return (Math.round(n / 1e5) / 10).toString().replace('.', ',') + ' triệu';
    if (n >= 1e3) return Math.round(n / 1e3) + ' nghìn';
    return String(n);
  }
  const vnd = (n) => (n === null || n === undefined ? '—' : Number(n).toLocaleString('vi-VN') + 'đ');
  const area = (n) => (n === null || n === undefined ? '' : String(n).replace('.', ',') + ' m²');

  function qparams() { return Object.fromEntries(new URLSearchParams(location.search)); }

  function toast(msg, isErr) {
    let box = $('.toasts');
    if (!box) { box = el('div', { class: 'toasts' }); document.body.appendChild(box); }
    const t = el('div', { class: 'toast' + (isErr ? ' err' : ''), text: msg });
    box.appendChild(t);
    setTimeout(() => t.remove(), 3800);
  }

  function modal(content) {
    const bg = el('div', { class: 'modal-bg', onclick: (e) => { if (e.target === bg) bg.remove(); } }, el('div', { class: 'modal' }, content));
    document.body.appendChild(bg);
    return { close: () => bg.remove(), root: bg };
  }

  function alertBox(kind, msg) { return el('div', { class: 'alert ' + kind, text: msg }); }

  /* Đếm ngược "01 ngày 02:03:04" */
  function fmtLeft(sec) {
    if (sec <= 0) return 'Đã kết thúc';
    const d = Math.floor(sec / 86400), h = Math.floor((sec % 86400) / 3600), m = Math.floor((sec % 3600) / 60), s = sec % 60;
    const p = (x) => String(x).padStart(2, '0');
    return (d > 0 ? d + ' ngày ' : '') + p(h) + ':' + p(m) + ':' + p(s);
  }
  function countdown(node, seconds) {
    let left = Math.max(0, Math.floor(seconds));
    node.textContent = fmtLeft(left);
    const id = setInterval(() => {
      left -= 1;
      node.textContent = fmtLeft(left);
      if (left <= 0) clearInterval(id);
    }, 1000);
  }

  function safeUrl(u) { return typeof u === 'string' && (u.startsWith('/') && !u.startsWith('//') || /^https?:\/\//i.test(u)) ? u : '/img/types/phong-tro.svg'; }

  function pager(container, page, totalPages, onGo) {
    clear(container);
    if (totalPages <= 1) return;
    const btn = (label, p, cur, dis) => el('button', { class: cur ? 'cur' : '', disabled: dis, text: label, onclick: () => onGo(p) });
    container.appendChild(btn('‹', page - 1, false, page <= 1));
    const pages = new Set([1, totalPages, page - 1, page, page + 1]);
    let last = 0;
    [...pages].filter((p) => p >= 1 && p <= totalPages).sort((a, b) => a - b).forEach((p) => {
      if (p - last > 1) container.appendChild(el('span', { class: 'muted', text: '…' }));
      container.appendChild(btn(String(p), p, p === page, false));
      last = p;
    });
    container.appendChild(btn('›', page + 1, false, page >= totalPages));
  }

  /* "5 ngày trước" */
  function relTime(str) {
    if (!str) return '';
    const t = new Date(String(str).replace(' ', 'T'));
    if (isNaN(t)) return '';
    const sec = Math.max(0, (Date.now() - t.getTime()) / 1000);
    if (sec < 3600) return Math.max(1, Math.floor(sec / 60)) + ' phút trước';
    if (sec < 86400) return Math.floor(sec / 3600) + ' giờ trước';
    if (sec < 86400 * 30) return Math.floor(sec / 86400) + ' ngày trước';
    if (sec < 86400 * 365) return Math.floor(sec / 86400 / 30) + ' tháng trước';
    return Math.floor(sec / 86400 / 365) + ' năm trước';
  }
  const initial = (name) => { const p = String(name || '?').trim().split(/\s+/); return (p[p.length - 1] || '?').charAt(0).toUpperCase(); };

  /* Đồng hồ đếm ngược dạng ô số đen: [01] ngày [02] : [03] : [04] */
  function countdownBoxes(node, seconds) {
    let left = Math.max(0, Math.floor(seconds));
    const p = (x) => String(x).padStart(2, '0');
    const draw = () => {
      clear(node);
      if (left <= 0) { node.appendChild(el('span', { class: 'tb-end', text: 'Đã kết thúc' })); return; }
      const d = Math.floor(left / 86400), h = Math.floor((left % 86400) / 3600), m = Math.floor((left % 3600) / 60), s = left % 60;
      if (d > 0) node.appendChild(el('span', { class: 'tb' }, p(d)));
      if (d > 0) node.appendChild(el('span', { class: 'tb-sep', text: 'ngày' }));
      [h, m, s].forEach((v, i) => { if (i) node.appendChild(el('span', { class: 'tb-sep', text: ':' })); node.appendChild(el('span', { class: 'tb' }, p(v))); });
    };
    draw();
    const id = setInterval(() => { left -= 1; draw(); if (left <= 0) clearInterval(id); }, 1000);
  }

  /* Thẻ phòng giống Lozido: ảnh bo góc, "Đã xác minh", số ảnh, chủ nhà, giá + % OFF, Zalo + Xem SĐT.
     opts.row = true → dạng danh sách ngang (ảnh bên trái) kèm avatar chủ nhà + thời gian đăng. */
  function roomCard(r, opts) {
    opts = opts || {};
    const link = '/room.html?id=' + r.roomId;
    const badges = el('div', { class: 'badges' }, el('span', { class: 'badge ver', text: '✔ Đã xác minh' }));
    if (r.isNow) badges.appendChild(el('span', { class: 'badge now', text: 'NOW' }));
    if (r.isHot) badges.appendChild(el('span', { class: 'badge hot', text: 'HOT' }));
    if (r.isNew) badges.appendChild(el('span', { class: 'badge new', text: 'MỚI' }));

    const fav = el('button', {
      class: 'fav' + (r.isFavorite ? ' on' : ''), title: 'Lưu phòng', 'aria-label': 'Lưu phòng', text: '♥',
      onclick: async (e) => {
        e.preventDefault();
        if (!Api.token.get()) { location.href = '/login.html?next=' + encodeURIComponent(location.pathname + location.search); return; }
        try {
          if (fav.classList.contains('on')) { await Api.del('/api/favorites/' + r.roomId); fav.classList.remove('on'); if (opts.onUnfav) opts.onUnfav(r); }
          else { await Api.post('/api/favorites/' + r.roomId); fav.classList.add('on'); toast('Đã lưu phòng'); }
        } catch (err) { toast(err.message, true); }
      },
    });

    const priceBox = el('div', { class: 'price' }, price(r.price), el('small', { text: '/tháng' }));
    if (r.discountPercent && r.originalPrice) {
      priceBox.appendChild(el('del', { text: price(r.originalPrice) }));
      priceBox.appendChild(el('span', { class: 'off', text: r.discountPercent + '% OFF' }));
    }

    const place = [r.districtName, r.provinceName].filter(Boolean).join(', ');
    const ownerLine = el('div', { class: 'owner-line' }, r.contactName ? el('span', { text: '👤 ' + r.contactName }) : null, place ? el('span', { text: '📍 ' + place }) : null);
    const meta = [r.typeName, area(r.areaM2)].filter(Boolean).join(' • ');

    let phoneNum = null;
    const getContact = async () => { const c = await Api.get('/api/rooms/' + r.roomId + '/contact'); phoneNum = c; return c; };
    const phoneBtn = el('button', { class: 'btn sm dark', type: 'button', text: 'Xem SĐT' });
    phoneBtn.addEventListener('click', async () => {
      phoneBtn.disabled = true;
      try {
        const c = phoneNum || await getContact();
        const phone = c.contactPhone || '';
        phoneBtn.replaceWith(el('a', { class: 'btn sm dark', href: 'tel:' + phone.replace(/[^0-9+]/g, ''), text: '📞 ' + phone }));
      } catch (err) { toast(err.message, true); phoneBtn.disabled = false; }
    });
    const zaloBtn = el('button', { class: 'btn sm zalo-o', type: 'button', text: 'Zalo' });
    zaloBtn.addEventListener('click', async () => {
      try { const c = phoneNum || await getContact(); window.open('https://zalo.me/' + (c.zalo || c.contactPhone || '').replace(/[^0-9]/g, ''), '_blank', 'noopener'); } catch (err) { toast(err.message, true); }
    });

    const thumb = el('a', { class: 'thumb', href: link }, el('img', { src: safeUrl(r.thumbnailUrl), alt: r.title, loading: 'lazy' }), badges,
      r.imageCount ? el('span', { class: 'imgcount', text: '📷 ' + r.imageCount }) : null);

    const body = el('div', { class: 'body' },
      el('div', { class: 'type', text: meta }),
      el('h3', {}, el('a', { href: link, text: r.title })),
      ownerLine,
      priceBox,
      r.distanceKm !== undefined && r.distanceKm !== null ? el('div', { class: 'meta', text: '📍 Cách ' + r.distanceKm.toFixed(1).replace('.', ',') + ' km' }) : null,
      opts.row ? el('div', { class: 'by' }, el('span', { class: 'avatar g', text: initial(r.contactName) }), el('span', { text: r.contactName || '' }), el('span', { class: 'when', text: relTime(r.createDate) })) : null,
      opts.noContact ? null : el('div', { class: 'card-actions' }, zaloBtn, phoneBtn));

    return el('article', { class: 'room-card' + (opts.row ? ' row-card' : '') }, thumb, fav, body);
  }

  w.UI = { el, $, $$, clear, price, vnd, area, qparams, toast, modal, alertBox, countdown, countdownBoxes, relTime, initial, fmtLeft, pager, roomCard, safeUrl };
})(window);
