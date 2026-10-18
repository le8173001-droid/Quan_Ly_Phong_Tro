(async function () {
  const { el, $, clear } = UI;
  const { me } = await Layout.init('');
  const host = $('#profile-host');
  if (!me) { location.replace('/login.html?next=/profile.html'); return; }

  const inp = (v, attrs) => el('input', Object.assign({ value: v || '', maxlength: 150 }, attrs || {}));
  const field = (label, node, hint) => el('div', { class: 'field' }, el('label', { text: label }), node, hint ? el('small', { class: 'muted', text: hint }) : null);

  // ---- Thông tin tài khoản + thông tin pháp lý (dùng để lập hợp đồng)
  const full = inp(me.fullName, { maxlength: 100 }), phone = inp(me.phone, { maxlength: 20 });
  const idNo = inp(me.idNumber, { maxlength: 20, inputmode: 'numeric' });
  const birth = inp(me.birthDate, { type: 'date' });
  const issueDate = inp(me.idIssueDate, { type: 'date' });
  const issuePlace = inp(me.idIssuePlace);
  const addr = inp(me.address, { maxlength: 250 });
  const bank = inp(me.bankName), bankAcc = inp(me.bankAccount, { maxlength: 30 });
  const msg1 = el('div');
  const f1 = el('form', {}, el('h2', { text: 'Thông tin tài khoản' }), msg1,
    el('div', { class: 'two' }, field('Tên đăng nhập', el('input', { value: me.username, disabled: true })), field('Email', el('input', { value: me.email || '', disabled: true }))),
    el('div', { class: 'two' }, field('Họ và tên', full), field('Số điện thoại', phone)),
    el('h3', { style: 'margin-top:18px', text: 'Thông tin pháp lý' }),
    el('p', { class: 'muted', text: 'Dùng để lập hợp đồng thuê nhà có giá trị pháp lý (CCCD/CMND, địa chỉ thường trú, tài khoản nhận tiền).' }),
    el('div', { class: 'two' }, field('Số CCCD / CMND', idNo), field('Ngày sinh', birth)),
    el('div', { class: 'two' }, field('Ngày cấp', issueDate), field('Nơi cấp', issuePlace)),
    field('Địa chỉ thường trú', addr),
    el('div', { class: 'two' }, field('Ngân hàng', bank), field('Số tài khoản', bankAcc)),
    el('button', { class: 'btn', type: 'submit', text: 'Lưu thay đổi' }));
  f1.addEventListener('submit', async (e) => {
    e.preventDefault(); clear(msg1);
    try {
      await Api.put('/api/auth/me', { fullName: full.value, phone: phone.value, idNumber: idNo.value, birthDate: birth.value, idIssueDate: issueDate.value, idIssuePlace: issuePlace.value, address: addr.value, bankName: bank.value, bankAccount: bankAcc.value });
      msg1.appendChild(UI.alertBox('ok', 'Đã cập nhật'));
    } catch (err) { msg1.appendChild(UI.alertBox('err', err.message)); }
  });

  // ---- Đổi mật khẩu
  const cur = el('input', { type: 'password', autocomplete: 'current-password' });
  const nw = el('input', { type: 'password', autocomplete: 'new-password' });
  const msg2 = el('div');
  const f2 = el('form', {}, el('h2', { style: 'margin-top:24px', text: me.hasPassword ? 'Đổi mật khẩu' : 'Đặt mật khẩu (để đăng nhập không cần Google)' }), msg2,
    me.hasPassword ? field('Mật khẩu hiện tại', cur) : null, field('Mật khẩu mới', nw),
    el('button', { class: 'btn ghost', type: 'submit', text: 'Cập nhật mật khẩu' }));
  f2.addEventListener('submit', async (e) => {
    e.preventDefault(); clear(msg2);
    try { await Api.post('/api/auth/password', { currentPassword: cur.value, newPassword: nw.value }); msg2.appendChild(UI.alertBox('ok', 'Đã đổi mật khẩu')); cur.value = nw.value = ''; }
    catch (err) { msg2.appendChild(UI.alertBox('err', err.message)); }
  });
  host.appendChild(f1); host.appendChild(f2);

  // ---- Quyền quản lý tòa nhà (chỉ MANAGER): thay cho "Tôi là chủ nhà"
  if (me.role === 'MANAGER') {
    const msg3 = el('div'); const pw = el('input', { type: 'password', autocomplete: 'current-password', placeholder: 'Nhập mật khẩu để xác nhận' });
    const btn = el('button', { class: 'btn', style: 'background:#c0392b', type: 'button', text: 'Hủy quyền quản lý tòa nhà' });
    const card = el('div', { id: 'manager', class: 'danger-zone' },
      el('h2', { text: 'Quyền quản lý tòa nhà' }),
      el('p', { class: 'muted', text: 'Bạn đang là chủ / quản lý của ' + (me.buildingCount || 0) + ' tòa nhà. Hủy quyền: các tòa nhà chuyển cho quản trị viên, phòng bị ẩn khỏi trang tìm phòng và tài khoản trở lại người dùng thường.' }),
      msg3, me.hasPassword ? field('Mật khẩu', pw) : null, btn);
    btn.addEventListener('click', async () => {
      clear(msg3); btn.disabled = true;
      try {
        let r = await Api.post('/api/auth/resign-manager', { password: pw.value });
        if (r.needConfirm) {
          if (!confirm(r.message + '\n\nBạn chắc chắn muốn hủy quyền?')) { btn.disabled = false; return; }
          r = await Api.post('/api/auth/resign-manager', { confirm: true, password: pw.value });
        }
        Api.token.set(r.token);
        alert('Đã hủy quyền quản lý tòa nhà.');
        location.href = '/';
      } catch (err) { msg3.appendChild(UI.alertBox('err', err.message)); btn.disabled = false; }
    });
    host.appendChild(card);
    if (location.hash === '#manager') card.scrollIntoView();
  }
})();
