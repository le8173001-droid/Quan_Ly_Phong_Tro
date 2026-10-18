/* Header + footer dùng chung, trạng thái đăng nhập. */
(function (w) {
  const { el, $ } = UI;
  const NAV = [
    ['/', 'Trang chủ', 'home'], ['/search.html', 'Tìm phòng', 'search'], ['/promotions.html', 'Chương trình', 'promotions'],
    ['/invoice.html', 'Tra cứu hóa đơn', 'invoice'], ['/support.html', 'Hỗ trợ tìm phòng', 'support'], ['/landlord.html', 'Tôi là chủ nhà', 'landlord'],
  ];
  let me = null;

  async function loadMe() {
    if (!Api.token.get()) return null;
    try { me = await Api.get('/api/auth/me'); } catch (e) { me = null; Api.token.clear(); }
    return me;
  }

  function logout() { Api.token.clear(); location.href = '/'; }

  function initials(name) {
    const parts = String(name || '?').trim().split(/\s+/);
    return (parts[parts.length - 1] || '?').charAt(0).toUpperCase();
  }

  function renderHeader(active, settings) {
    const host = $('#site-header');
    if (!host) return;
    const isMgr = me && me.role === 'MANAGER';
    const NAVF = NAV.filter((n) => !(isMgr && n[2] === 'landlord'));
    const nav = el('nav', { class: 'nav', id: 'main-nav' }, NAVF.map(([href, label, key]) => el('a', { href, class: key === active ? 'active' : '', text: label })));
    const name = (settings && settings.site_name) || 'NhàTrọ+';
    const navLinks = NAVF.map(([href, label]) => el('a', { class: 'only-m', href, text: label }));

    let items;
    let avatar;
    if (me) {
      avatar = el('span', { class: 'avatar', text: initials(me.fullName || me.username) });
      items = [el('div', { class: 'who', text: me.fullName || me.username }),
        el('a', { href: '/favorites.html', text: '♥ Phòng đã lưu' }),
        el('a', { href: '/profile.html', text: 'Tài khoản của tôi' }),
        me.role === 'ADMIN' ? el('a', { href: '/admin/index.html', text: 'Trang quản trị (Admin)' }) : null,
        me.role === 'MANAGER' ? el('a', { href: '/admin/index.html', text: 'Quản lý tòa nhà của tôi' }) : null,
        me.role === 'MANAGER' ? el('a', { href: '/profile.html#manager', text: 'Hủy quyền quản lý tòa' }) : null,
        el('hr', { class: 'only-m' }), ...navLinks,
        el('hr'), el('button', { type: 'button', text: 'Đăng xuất', onclick: logout })];
    } else {
      avatar = el('span', { class: 'avatar g', text: '👤' });
      const next = encodeURIComponent(location.pathname + location.search);
      items = [el('a', { href: '/login.html?next=' + next, text: 'Đăng nhập' }), el('a', { href: '/login.html?tab=register', text: 'Đăng ký tài khoản' }),
        el('a', { href: '/favorites.html', text: '♥ Phòng đã lưu' }), el('hr', { class: 'only-m' }), ...navLinks];
    }
    const drop = el('div', { class: 'drop' }, items);
    const menu = el('div', { class: 'user-menu' }, el('button', { class: 'user-pill', type: 'button', 'aria-label': 'Menu tài khoản' }, el('span', { class: 'burger', text: '☰' }), avatar), drop);
    menu.firstChild.addEventListener('click', (e) => { e.stopPropagation(); menu.classList.toggle('open'); });
    document.addEventListener('click', () => menu.classList.remove('open'));

    const right = el('div', { class: 'hdr-right' },
      isMgr ? el('a', { class: 'landlord-link', href: '/admin/index.html' }, 'Quản lý tòa nhà', el('small', { text: 'Trang của chủ nhà' }))
        : (me && me.role === 'ADMIN') ? el('a', { class: 'landlord-link', href: '/admin/index.html' }, 'Quản trị', el('small', { text: 'Toàn hệ thống' }))
        : el('a', { class: 'landlord-link', href: '/landlord.html' }, 'Tôi là chủ nhà', el('small', { text: 'Đăng tin & Lấp phòng' })),
      el('a', { class: 'icon-btn', href: '/favorites.html', title: 'Phòng đã lưu', 'aria-label': 'Phòng đã lưu', text: '♥' }),
      menu);
    const bar = el('div', { class: 'container bar' },
      el('a', { class: 'logo', href: '/' }, el('img', { src: '/img/logo.svg', alt: '' }), el('span', { text: name })),
      nav, right);
    host.className = 'site-header';
    UI.clear(host).appendChild(el('div', { class: 'notice' }, el('span', { text: '🎁 Đăng tin cho thuê miễn phí – lấp đầy phòng trống nhanh chóng.' }), isMgr ? el('a', { href: '/admin/index.html', text: 'Vào trang quản lý ›' }) : el('a', { href: '/landlord.html', text: 'Tìm hiểu ›' })));
    host.appendChild(el('div', { class: 'hdr' }, bar));
  }

  function renderFooter(s) {
    const host = $('#site-footer');
    if (!host) return;
    s = s || {};
    host.className = 'site-footer';
    const link = (href, text) => el('a', { href, text });
    UI.clear(host).appendChild(el('div', { class: 'container' },
      el('div', { class: 'cols' },
        el('div', {}, el('h4', { text: s.site_name || 'NhàTrọ+' }), el('div', { class: 'muted', text: s.tagline || '' }), el('div', { class: 'muted', text: '📍 ' + (s.address || '') })),
        el('div', {}, el('h4', { text: 'Khám phá' }), link('/search.html', 'Tìm phòng'), link('/promotions.html', 'Chương trình & ưu đãi'), link('/favorites.html', 'Phòng đã lưu')),
        el('div', {}, el('h4', { text: 'Dịch vụ' }), link('/support.html', 'Hỗ trợ tìm phòng'), link('/invoice.html', 'Tra cứu hóa đơn'), link('/broker.html', 'Tuyển môi giới'), (me && me.role === 'MANAGER') ? link('/profile.html#manager', 'Quyền quản lý tòa') : link('/landlord.html', 'Dành cho chủ nhà')),
        el('div', {}, el('h4', { text: 'Liên hệ' }), el('div', { class: 'muted', text: '☎ Hotline: ' + (s.hotline || '') }), el('div', { class: 'muted', text: '✉ ' + (s.email || '') }), s.zalo ? el('a', { href: 'https://zalo.me/' + s.zalo.replace(/[^0-9]/g, ''), target: '_blank', rel: 'noopener', text: 'Zalo: ' + s.zalo }) : null)),
      el('div', { class: 'links-block' },
        link('/search.html?typeId=1', 'Phòng trọ'), link('/search.html?typeId=2', 'Ký túc xá'), link('/search.html?typeId=3', 'Nhà nguyên căn'), link('/search.html?typeId=4', 'Chung cư'),
        link('/search.html?typeId=5', 'Văn phòng'), link('/search.html?typeId=6', 'Pass & ở ghép'), link('/search.html?sort=price_asc', 'Phòng giá rẻ'), link('/search.html?isNow=true', 'Dọn vào ở ngay')),
      el('div', { class: 'copy', text: '© ' + new Date().getFullYear() + ' ' + (s.site_name || 'NhàTrọ+') + '. Dự án học tập – dữ liệu mẫu.' })));
    if (!$('.float-btns')) {
      const zalo = s.zalo ? 'https://zalo.me/' + s.zalo.replace(/[^0-9]/g, '') : '/support.html';
      const top = el('button', { class: 'float-top hidden', type: 'button', 'aria-label': 'Lên đầu trang', text: '↑', onclick: () => window.scrollTo({ top: 0, behavior: 'smooth' }) });
      document.body.appendChild(el('div', { class: 'float-btns' }, top, el('a', { class: 'float-zalo', href: zalo, target: '_blank', rel: 'noopener', title: 'Chat Zalo', text: 'Zalo' })));
      window.addEventListener('scroll', () => top.classList.toggle('hidden', window.scrollY < 400), { passive: true });
    }
  }

  async function init(active) {
    const [settings] = await Promise.all([Api.get('/api/settings').catch(() => ({})), loadMe()]);
    renderHeader(active, settings);
    renderFooter(settings);
    if (settings.site_name) document.title = document.title.replace('NhàTrọ+', settings.site_name);
    return { me, settings };
  }

  w.Layout = { init, getMe: () => me, loadMe };
})(window);
