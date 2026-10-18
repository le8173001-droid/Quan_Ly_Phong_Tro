(async function () {
  const { el, $, clear } = UI;
  await Layout.init('promotions');
  const host = $('#list');
  try {
    const p = await Api.get('/api/promotions');
    clear(host);
    const card = (x) => {
      const left = el('div', { class: 'left' }); UI.countdown(left, x.secondsLeft);
      const href = x.linkUrl && x.linkUrl.startsWith('/') ? x.linkUrl : null;
      return el(href ? 'a' : 'div', { class: 'program ' + (x.bannerColor || 'green'), href },
        el('div', { class: 'big', text: x.discountText || '' }), el('b', { text: x.title }), el('div', { text: x.description || '' }),
        x.voucherCode ? el('div', {}, 'Mã: ', el('b', { text: x.voucherCode })) : null,
        el('div', { class: 'left' }, 'Còn lại: ', (() => { const t = el('b'); UI.countdown(t, x.secondsLeft); return t; })()));
    };
    const section = (title, list) => list.length ? el('section', {}, el('h2', { text: title }), el('div', { class: 'programs' }, list.map(card))) : null;
    host.appendChild(section('Chương trình', p.programs));
    host.appendChild(section('Voucher', p.vouchers));
    host.appendChild(section('Đối tác ưu đãi', p.partners));
    if (p.hotdeals.length) {
      host.appendChild(el('h2', { text: '🔥 HOTDEAL' }));
      p.hotdeals.forEach((h) => {
        const t = el('span', { class: 'timer' }); UI.countdown(t, h.secondsLeft);
        host.appendChild(el('div', { class: 'hotdeal' }, el('div', { class: 'hd-head' }, el('b', { text: h.title }), el('span', { text: 'Kết thúc sau' }), t),
          el('div', { class: 'grid' }, (h.rooms || []).map((r) => UI.roomCard(r)))));
      });
    }
    if (!host.children.length) host.appendChild(el('div', { class: 'empty', text: 'Hiện chưa có chương trình nào.' }));
  } catch (e) { clear(host).appendChild(UI.alertBox('err', e.message)); }
})();
