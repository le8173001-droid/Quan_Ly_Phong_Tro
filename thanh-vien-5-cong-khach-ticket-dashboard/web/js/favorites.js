(async function () {
  const { el, $, clear } = UI;
  const { me } = await Layout.init('');
  const host = $('#results');
  if (!me) { host.appendChild(el('div', { class: 'empty' }, 'Bạn cần đăng nhập để xem phòng đã lưu. ', el('a', { class: 'btn', href: '/login.html?next=/favorites.html', text: 'Đăng nhập' }))); return; }
  try {
    const r = await Api.get('/api/favorites');
    const draw = () => { if (!host.children.length) host.appendChild(el('div', { class: 'empty', text: 'Bạn chưa lưu phòng nào.' })); };
    r.items.forEach((x) => { const c = UI.roomCard(x, { onUnfav: () => { c.remove(); draw(); } }); host.appendChild(c); });
    draw();
  } catch (e) { host.appendChild(UI.alertBox('err', e.message)); }
})();
