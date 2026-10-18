(async function () {
  const { el, $, clear } = UI;
  const { me } = await Layout.init('');
  const q = UI.qparams();
  const next = q.next && q.next.startsWith('/') && !q.next.startsWith('//') ? q.next : '/';
  if (me) { location.replace(next); return; }
  const host = $('#auth-host');
  let tab = q.tab === 'register' ? 'register' : 'login';
  const msg = el('div', {});

  function done(r) { Api.token.set(r.token); location.href = (r.user.role === 'ADMIN' || r.user.role === 'MANAGER') && next === '/' ? '/admin/index.html' : next; }
  const input = (name, label, type, extra) => { const i = el('input', { name, type: type || 'text', required: true, ...(extra || {}) }); return [el('div', { class: 'field' }, el('label', { text: label }), i), i]; };

  function loginForm() {
    const [f1, user] = input('username', 'Tên đăng nhập hoặc email', 'text', { autocomplete: 'username' });
    const [f2, pass] = input('password', 'Mật khẩu', 'password', { autocomplete: 'current-password' });
    const btn = el('button', { class: 'btn block', type: 'submit', text: 'Đăng nhập' });
    const form = el('form', {}, f1, f2, btn);
    form.addEventListener('submit', async (e) => {
      e.preventDefault(); clear(msg); btn.disabled = true;
      try { done(await Api.post('/api/auth/login', { username: user.value, password: pass.value })); }
      catch (err) { msg.appendChild(UI.alertBox('err', err.message)); btn.disabled = false; }
    });
    return form;
  }

  function registerForm() {
    const [f1, user] = input('username', 'Tên đăng nhập (3-30 ký tự: chữ, số, _ .)', 'text', { autocomplete: 'username', maxlength: 30 });
    const [f2, full] = input('fullName', 'Họ và tên', 'text', { maxlength: 100 });
    const [f3, email] = input('email', 'Email', 'email', { maxlength: 100 });
    const [f4, phone] = input('phone', 'Số điện thoại', 'tel', { maxlength: 20 }); phone.required = false;
    const [f5, pass] = input('password', 'Mật khẩu (từ 8 ký tự, có chữ và số)', 'password', { autocomplete: 'new-password' });
    const [f6, pass2] = input('password2', 'Nhập lại mật khẩu', 'password', { autocomplete: 'new-password' });
    const btn = el('button', { class: 'btn orange block', type: 'submit', text: 'Tạo tài khoản' });
    const form = el('form', {}, f1, f2, f3, f4, f5, f6, btn);
    form.addEventListener('submit', async (e) => {
      e.preventDefault(); clear(msg);
      if (pass.value !== pass2.value) { msg.appendChild(UI.alertBox('err', 'Mật khẩu nhập lại không khớp')); return; }
      btn.disabled = true;
      try { done(await Api.post('/api/auth/register', { username: user.value, fullName: full.value, email: email.value, phone: phone.value, password: pass.value })); }
      catch (err) { msg.appendChild(UI.alertBox('err', err.message)); btn.disabled = false; }
    });
    return form;
  }

  const gbox = el('div', { class: 'gbtn-box' });
  function loadGoogle(clientId) {
    const init = () => {
      google.accounts.id.initialize({
        client_id: clientId,
        callback: async (resp) => {
          clear(msg);
          try { done(await Api.post('/api/auth/google', { credential: resp.credential })); }
          catch (err) { msg.appendChild(UI.alertBox('err', err.message)); }
        },
      });
      google.accounts.id.renderButton(gbox, { theme: 'outline', size: 'large', text: 'continue_with', locale: 'vi', width: 320 });
    };
    const s = el('script', { src: 'https://accounts.google.com/gsi/client', async: true, defer: true });
    s.onload = init;
    s.onerror = () => { clear(gbox).appendChild(el('div', { class: 'muted', text: 'Không tải được Google (kiểm tra mạng / trình chặn quảng cáo).' })); };
    document.head.appendChild(s);
  }

  async function draw() {
    const tabs = el('div', { class: 'tabs-auth' },
      el('button', { type: 'button', class: tab === 'login' ? 'on' : '', text: 'Đăng nhập', onclick: () => { tab = 'login'; clear(msg); draw(); } }),
      el('button', { type: 'button', class: tab === 'register' ? 'on' : '', text: 'Tạo tài khoản', onclick: () => { tab = 'register'; clear(msg); draw(); } }));
    clear(host).appendChild(el('div', {}, el('h2', { class: 'center', text: tab === 'login' ? 'Chào mừng trở lại' : 'Tạo tài khoản mới' }), tabs, msg,
      tab === 'login' ? loginForm() : registerForm(), el('div', { class: 'divider', text: 'hoặc' }), gbox));
  }
  await draw();

  try {
    const cfg = await Api.get('/api/auth/config');
    if (cfg.googleEnabled) loadGoogle(cfg.googleClientId);
    else gbox.appendChild(el('div', { class: 'muted center', text: 'Đăng nhập Google chưa được bật (chưa cấu hình GOOGLE_CLIENT_ID – xem README).' }));
  } catch (e) { /* bỏ qua */ }
})();
