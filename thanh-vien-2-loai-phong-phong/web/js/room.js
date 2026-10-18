(async function () {
  const { el, $, clear } = UI;
  await Layout.init('search');
  const id = UI.qparams().id;
  const host = $('#room-host');
  if (!id) { clear(host).appendChild(UI.alertBox('err', 'Thiếu mã phòng')); return; }
  let r;
  try { r = await Api.get('/api/rooms/' + id); } catch (e) { clear(host).appendChild(el('div', { class: 'empty' }, UI.alertBox('err', e.message), el('a', { class: 'btn', href: '/search.html', text: 'Về trang tìm phòng' }))); return; }
  document.title = r.title + ' – NhàTrọ+';

  const imgs = (r.images || []).map((i) => UI.safeUrl(i.imageUrl || i.url || i));
  if (!imgs.length) imgs.push(UI.safeUrl(r.thumbnailUrl));
  const main = el('img', { src: imgs[0], alt: r.title });
  const side = el('div', { class: 'side' });
  imgs.slice(0, 4).forEach((u) => side.appendChild(el('img', { src: u, alt: '', onclick: () => { main.src = u; } })));
  const gallery = el('div', { class: 'gallery' }, el('div', { class: 'main' }, main), imgs.length > 1 ? side : null);

  const badges = [];
  if (r.isNow) badges.push(el('span', { class: 'badge now', text: 'NOW' }));
  if (r.isHot) badges.push(el('span', { class: 'badge hot', text: 'HOT' }));
  if (r.isNew) badges.push(el('span', { class: 'badge new', text: 'MỚI' }));
  if (r.discountPercent) badges.push(el('span', { class: 'badge deal', text: 'GIẢM ' + r.discountPercent + '%' }));

  const priceBox = el('div', { class: 'big-price' }, UI.vnd(r.price), el('small', { class: 'muted', text: ' /tháng' }));
  if (r.discountPercent) priceBox.appendChild(el('del', { text: UI.vnd(r.originalPrice) }));
  if (r.dealSecondsLeft) { const t = el('span', { class: 'timer' }); UI.countdown(t, r.dealSecondsLeft); priceBox.after; }

  const fav = el('button', { class: 'btn light block', type: 'button', text: r.isFavorite ? '♥ Đã lưu' : '♡ Lưu phòng' });
  fav.addEventListener('click', async () => {
    if (!Api.token.get()) { location.href = '/login.html?next=' + encodeURIComponent(location.pathname + location.search); return; }
    try {
      if (r.isFavorite) { await Api.del('/api/favorites/' + r.roomId); r.isFavorite = false; } else { await Api.post('/api/favorites/' + r.roomId); r.isFavorite = true; }
      fav.textContent = r.isFavorite ? '♥ Đã lưu' : '♡ Lưu phòng';
    } catch (e) { UI.toast(e.message, true); }
  });

  const contact = el('div', {});
  const showBtn = el('button', { class: 'btn orange block', type: 'button', text: '📞 Xem số điện thoại' });
  showBtn.addEventListener('click', async () => {
    try {
      const c = await Api.get('/api/rooms/' + r.roomId + '/contact');
      clear(contact).appendChild(el('div', {}, el('div', { class: 'muted', text: 'Liên hệ: ' + (c.contactName || '') }),
        el('a', { class: 'btn orange block', href: 'tel:' + c.contactPhone.replace(/[^0-9+]/g, ''), text: '📞 ' + c.contactPhone }),
        el('a', { class: 'btn zalo block', style: 'margin-top:8px', target: '_blank', rel: 'noopener', href: 'https://zalo.me/' + (c.zalo || c.contactPhone).replace(/[^0-9]/g, ''), text: 'Nhắn Zalo' })));
    } catch (e) { UI.toast(e.message, true); }
  });
  contact.appendChild(showBtn);

  // đặt lịch xem phòng
  const f = { name: el('input', { placeholder: 'Họ và tên', maxlength: 100 }), phone: el('input', { type: 'tel', placeholder: 'Số điện thoại', maxlength: 20 }), time: el('input', { placeholder: 'Thời gian mong muốn (vd: tối nay 19h)', maxlength: 100 }) };
  const msg = el('div', {});
  const form = el('form', {}, el('h3', { text: 'Đặt lịch xem phòng' }), msg,
    el('div', { class: 'field' }, f.name), el('div', { class: 'field' }, f.phone), el('div', { class: 'field' }, f.time),
    el('button', { class: 'btn block', type: 'submit', text: 'Gửi yêu cầu xem phòng' }));
  const me = Layout.getMe();
  if (me) { f.name.value = me.fullName || ''; f.phone.value = me.phone || ''; }
  form.addEventListener('submit', async (e) => {
    e.preventDefault(); clear(msg);
    try {
      await Api.post('/api/viewing-requests', { roomId: r.roomId, fullName: f.name.value, phone: f.phone.value, preferredTime: f.time.value });
      msg.appendChild(UI.alertBox('ok', 'Đã gửi yêu cầu. Chủ trọ sẽ liên hệ lại với bạn sớm.')); form.querySelector('button[type=submit]').disabled = true;
    } catch (err) { msg.appendChild(UI.alertBox('err', err.message)); }
  });

  const kv = (k, v) => v === null || v === undefined || v === '' ? null : el('div', {}, el('small', { text: k }), el('b', { text: String(v) }));
  const left = el('div', {},
    gallery,
    el('div', { class: 'tag-row' }, el('span', { class: 'tg', text: r.typeName }), badges),
    el('h1', { text: r.title }),
    el('div', { class: 'muted', text: '📍 ' + r.fullAddress }),
    el('div', { class: 'kv' }, kv('Diện tích', UI.area(r.areaM2)), kv('Sức chứa', r.maxOccupants + ' người'), kv('Đang ở', r.currentOccupants + ' người'), kv('Tòa nhà', r.buildingName), kv('Mã phòng', r.roomNumber), kv('Lượt xem', r.viewCount)),
    el('h3', { text: 'Mô tả' }), el('p', { text: r.description || 'Chưa có mô tả.', style: 'white-space:pre-wrap' }),
    r.amenities && r.amenities.length ? [el('h3', { text: 'Tiện nghi' }), el('div', { class: 'amen' }, r.amenities.map((a) => el('span', { text: a.name })))] : null,
    r.nearbyLandmarks && r.nearbyLandmarks.length ? [el('h3', { style: 'margin-top:18px', text: 'Gần đây' }), el('ul', {}, r.nearbyLandmarks.map((l) => el('li', { text: (l.landmarkType === 'school' ? '🎓 ' : '🏢 ') + l.name + ' – ' + l.distanceKm.toFixed(1).replace('.', ',') + ' km' })))] : null);
  const right = el('aside', { class: 'side-card card' }, priceBox, r.dealSecondsLeft ? el('div', { class: 'muted' }, 'Ưu đãi kết thúc sau ', (() => { const t = el('b', {}); UI.countdown(t, r.dealSecondsLeft); return t; })()) : null,
    el('div', { style: 'margin:14px 0' }, contact), fav, el('hr', { style: 'border:0;border-top:1px solid var(--line);margin:16px 0' }), form);

  clear(host).appendChild(el('div', { style: 'padding:20px 0' }, el('div', { class: 'meta' }, el('a', { href: '/', text: 'Trang chủ' }), ' › ', el('a', { href: '/search.html?typeId=' + r.typeId, text: r.typeName }), ' › ', r.districtName),
    el('div', { class: 'detail' }, left, right),
    r.similarRooms && r.similarRooms.length ? [el('h2', { style: 'margin-top:30px', text: 'Phòng tương tự' }), el('div', { class: 'grid' }, r.similarRooms.map((x) => UI.roomCard(x)))] : null));
})();
