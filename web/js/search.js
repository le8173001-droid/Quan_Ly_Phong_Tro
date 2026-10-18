(async function () {
  const { el, $, clear } = UI;
  await Layout.init('search');
  const q = UI.qparams();
  let params = { ...q };
  let page = parseInt(q.page || '1', 10) || 1;

  const sort = $('#sort');
  sort.value = q.sort || (q.lat || q.landmarkId ? 'distance' : 'newest');
  if (![...sort.options].some((o) => o.value === sort.value)) sort.value = 'newest';

  const sb = await SearchBox.mount($('#search-host'), {
    initial: q,
    onSubmit: (p) => { params = { ...p, isNow: params.isNow, hotdeal: params.hotdeal, isNew: params.isNew, isHot: params.isHot }; page = 1; run(); },
  });

  sort.addEventListener('change', () => { page = 1; run(); });

  function tags() {
    const t = clear($('#active-tags'));
    const add = (txt) => t.appendChild(el('span', { class: 'tg', text: txt }));
    if (params.isNow) add('⚡ Dọn vào ở ngay');
    if (params.hotdeal) add('🔥 Hotdeal');
    if (params.isNew) add('Mới đăng');
    if (params.isHot) add('Hot');
    if (params.lat) add('📍 Gần vị trí của bạn');
  }

  async function run() {
    const out = clear($('#results'));
    out.appendChild(el('div', { class: 'spinner', text: 'Đang tìm phòng…' }));
    let s = sort.value;
    if (s === 'distance' && !(params.lat || params.landmarkId)) { s = 'newest'; }
    const query = { ...params, sort: s, page, size: 12 };
    const url = '/search.html' + Api.qs({ ...query, size: undefined });
    history.replaceState(null, '', url);
    tags();
    try {
      const r = await Api.get('/api/rooms', query);
      clear(out);
      $('#result-count').textContent = r.totalItems + ' phòng phù hợp';
      if (!r.items.length) out.appendChild(el('div', { class: 'empty' }, 'Không tìm thấy phòng nào. ', el('a', { href: '/support.html', style: 'color:var(--green);font-weight:700', text: 'Nhờ chúng tôi hỗ trợ tìm phòng' })));
      r.items.forEach((x) => out.appendChild(UI.roomCard(x)));
      UI.pager($('#pager'), r.page, r.totalPages, (p) => { page = p; run(); window.scrollTo({ top: 0, behavior: 'smooth' }); });
    } catch (e) {
      clear(out).appendChild(UI.alertBox('err', e.message));
    }
  }
  run();
})();
