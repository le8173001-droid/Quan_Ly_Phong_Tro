(async function () {
  const { el, $, clear } = UI;
  await Layout.init('invoice');
  const q = UI.qparams();
  const phone = el('input', { type: 'tel', placeholder: 'Số điện thoại đã đăng ký thuê', maxlength: 20, value: q.phone || '' });
  const room = el('input', { placeholder: 'Số phòng (vd: P102)', maxlength: 30, value: q.room || '' });
  const month = el('input', { type: 'month', value: q.month || '' });
  const msg = el('div', {});
  const btn = el('button', { class: 'btn orange block', type: 'submit', text: '🔍 Tra cứu' });
  const form = el('form', {}, msg,
    el('div', { class: 'field' }, el('label', { text: 'Số điện thoại *' }), phone),
    el('div', { class: 'field' }, el('label', { text: 'Số phòng *' }), room),
    el('div', { class: 'field' }, el('label', { text: 'Tháng (không bắt buộc)' }), month),
    btn,
    el('p', { class: 'muted', text: 'Gợi ý dùng thử: SĐT 0987654321, phòng P102.' }));
  $('#form-host').appendChild(form);

  const STATUS = { unpaid: 'Chưa thanh toán', partial: 'Thanh toán một phần', paid: 'Đã thanh toán', cancelled: 'Đã hủy' };
  const METHOD = { cash: 'Tiền mặt', bank_transfer: 'Chuyển khoản', momo: 'MoMo', other: 'Khác' };

  function invoiceCard(i) {
    const rows = i.details.map((d) => el('tr', {}, el('td', { text: d.description || d.serviceName }), el('td', { class: 'num', text: d.quantity }), el('td', { class: 'num', text: UI.vnd(d.unitPrice) }), el('td', { class: 'num', text: UI.vnd(d.subtotal) })));
    return el('div', { class: 'card', style: 'margin-bottom:16px' },
      el('div', { style: 'display:flex;justify-content:space-between;flex-wrap:wrap;gap:8px' },
        el('div', {}, el('h3', { text: 'Hóa đơn ' + i.invoiceCode }), el('div', { class: 'muted', text: 'Tháng ' + i.billingMonth + ' • Phòng ' + i.roomNumber + ' • ' + i.buildingName }), el('div', { class: 'muted', text: 'Người thuê: ' + i.tenantName })),
        el('div', {}, el('span', { class: 'pill ' + i.status, text: STATUS[i.status] || i.status }), el('div', { class: 'muted', text: 'Hạn thanh toán: ' + i.dueDate }))),
      el('table', { class: 'tbl' }, el('thead', {}, el('tr', {}, el('th', { text: 'Khoản mục' }), el('th', { class: 'num', text: 'SL' }), el('th', { class: 'num', text: 'Đơn giá' }), el('th', { class: 'num', text: 'Thành tiền' }))), el('tbody', {}, rows)),
      el('div', { style: 'text-align:right;margin-top:10px' },
        el('div', {}, 'Tổng cộng: ', el('b', { text: UI.vnd(i.totalAmount) })),
        el('div', {}, 'Đã thanh toán: ', el('b', { text: UI.vnd(i.paidAmount) })),
        el('div', { class: 'price' }, 'Còn lại: ' + UI.vnd(i.remaining))),
      i.payments.length ? [el('h4', { text: 'Lịch sử thanh toán' }), el('ul', {}, i.payments.map((p) => el('li', { text: (p.paidAt || '') + ' – ' + UI.vnd(p.amountPaid) + ' (' + (METHOD[p.method] || p.method) + ')' })))] : null);
  }

  async function search() {
    clear(msg); btn.disabled = true;
    const out = clear($('#result'));
    try {
      const r = await Api.get('/api/invoices/lookup', { phone: phone.value, room: room.value, month: month.value });
      r.invoices.forEach((i) => out.appendChild(invoiceCard(i)));
    } catch (e) { msg.appendChild(UI.alertBox('err', e.message)); }
    btn.disabled = false;
  }
  form.addEventListener('submit', (e) => { e.preventDefault(); search(); });
  if (q.phone && q.room) search();
})();
