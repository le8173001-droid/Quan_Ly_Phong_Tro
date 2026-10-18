/* THÀNH VIÊN 4 – 4 · Điện nước, hóa đơn & thanh toán.
   File này gồm: (1) danh sách bảng/biểu mẫu của mô-đun, (2) các màn hình riêng của mô-đun. */
(function () {
  const { S, OPT, opts, ST, CAT, money, yes, VEH } = ADMIN_KIT;
  ADMIN_MODULES.push({
    id: 'm4', member: 'Thành viên 4', title: '💰 4 · Điện nước, hóa đơn & thanh toán',
    resources: [
    { key: 'meter-readings', label: '⚡ Điện nước', path: 'meter-readings', id: 'readingId', grouped: true, buildingFilter: true,
      cols: [['billingMonth', 'Tháng'], ['oldElectric', 'Điện cũ'], ['newElectric', 'Điện mới'], ['oldWater', 'Nước cũ'], ['newWater', 'Nước mới']],
      fields: [{ name: 'roomId', label: 'Phòng', type: 'ref', src: 'rooms', req: 1 }, { name: 'billingMonth', label: 'Tháng', type: 'month', req: 1 }, { name: 'oldElectric', label: 'Số điện cũ (trống = lấy tháng trước)', type: 'number' }, { name: 'newElectric', label: 'Số điện mới', type: 'number' }, { name: 'oldWater', label: 'Số nước cũ', type: 'number' }, { name: 'newWater', label: 'Số nước mới', type: 'number' }] },
    { key: 'services', label: 'Bảng giá dịch vụ', path: 'services', id: 'serviceId', roRole: true,
      cols: [['serviceName', 'Tên'], ['calcType', 'Cách tính'], ['unit', 'Đơn vị'], ['unitPrice', 'Đơn giá', money], ['isActive', 'Đang dùng', yes]],
      fields: [{ name: 'serviceName', label: 'Tên dịch vụ', req: 1 }, { name: 'calcType', label: 'Cách tính', type: 'select', options: opts([['FIXED', 'Cố định/tháng'], ['RENT', 'Tiền phòng'], ['METER_ELECTRIC', 'Theo số điện'], ['METER_WATER', 'Theo số nước'], ['PER_VEHICLE', 'Theo số xe']]) }, { name: 'unit', label: 'Đơn vị' }, { name: 'unitPrice', label: 'Đơn giá', type: 'number' }, { name: 'isActive', label: 'Đang sử dụng', type: 'bool' }] },
    { key: 'invoices', label: '💰 Hóa đơn', path: 'invoices', id: 'invoiceId', grouped: true, buildingFilter: true, status: ST.invoice, statusKey: 'status', special: 'invoices',
      cols: [['invoiceCode', 'Mã'], ['billingMonth', 'Tháng'], ['tenantName', 'Người thuê'], ['totalAmount', 'Tổng', money], ['dueDate', 'Hạn'], ['status', 'Trạng thái']],
      fields: [{ name: 'contractId', label: 'Hợp đồng', type: 'ref', src: 'contracts', req: 1, full: 1 }, { name: 'billingMonth', label: 'Tháng', type: 'month', req: 1 }, { name: 'dueDate', label: 'Hạn thanh toán', type: 'date', req: 1 }, { name: 'status', label: 'Trạng thái', type: 'select', options: ST.invoice }, { name: 'note', label: 'Ghi chú', full: 1 }] },
    { key: 'payments', label: '💳 Thanh toán', path: 'payments', id: 'paymentId', grouped: true, buildingFilter: true,
      cols: [['invoiceCode', 'Hóa đơn'], ['amountPaid', 'Số tiền', money], ['method', 'Hình thức'], ['paidAt', 'Lúc']],
      fields: [{ name: 'invoiceId', label: 'Hóa đơn', type: 'ref', src: 'invoices', req: 1, full: 1 }, { name: 'amountPaid', label: 'Số tiền', type: 'number', req: 1 }, { name: 'method', label: 'Hình thức', type: 'select', options: opts([['cash', 'Tiền mặt'], ['bank_transfer', 'Chuyển khoản'], ['momo', 'MoMo'], ['other', 'Khác']]) }, { name: 'note', label: 'Ghi chú', full: 1 }] }
    ],
    init(C) {
      const { el, $, clear, UI, Api, view, me, isAdmin, srcCache, confirmBox, statusText, fetchSrc, loadSrc, uploader, refField, buildForm, formModal, buildingSelect, putRows } = C;
  // ---------- hóa đơn ----------
  function genInvoice() {
    formModal('Tạo hóa đơn tự động', [
      { name: 'contractId', label: 'Hợp đồng', type: 'ref', src: 'contracts', req: 1, full: 1 },
      { name: 'billingMonth', label: 'Tháng', type: 'month', req: 1 },
      { name: 'dueDate', label: 'Hạn thanh toán (trống = ngày 5 tháng sau)', type: 'date' }], {}, async (body) => {
      const r = await Api.post('/api/admin/invoices/generate', body);
      UI.toast('Đã tạo hóa đơn' + (r.warnings && r.warnings.length ? ' – ' + r.warnings.join('; ') : ''));
      C.route();
    });
  }
  async function invoiceDialog(id, done) {
    const inv = await Api.get('/api/admin/invoices/' + id);
    const box = el('div', {},
      el('h3', { text: 'Hóa đơn ' + inv.invoiceCode + ' – ' + inv.buildingName + ' / ' + inv.roomNumber }),
      el('div', { class: 'muted', text: inv.tenantName + ' • tháng ' + inv.billingMonth + ' • hạn ' + inv.dueDate }),
      el('table', { class: 'tbl' }, el('tbody', {}, inv.details.map((d) => el('tr', {}, el('td', { text: d.description || d.serviceName }), el('td', { class: 'num', text: UI.vnd(d.subtotal) }))))),
      el('p', {}, 'Tổng: ', el('b', { text: UI.vnd(inv.totalAmount) }), ' • Còn nợ: ', el('b', { text: UI.vnd(inv.remaining) }), ' • ', statusText(ADMIN_STATUS.invoice, inv.status)),
      inv.payments.length ? el('div', {}, el('b', { text: 'Đã thanh toán' }), el('ul', {}, inv.payments.map((p) => el('li', { text: p.paidAt + ' – ' + UI.vnd(p.amountPaid) + ' (' + p.method + ')' })))) : null);
    const amount = el('input', { type: 'number', placeholder: 'Số tiền', value: inv.remaining > 0 ? inv.remaining : '' });
    const method = el('select', {}, [['cash', 'Tiền mặt'], ['bank_transfer', 'Chuyển khoản'], ['momo', 'MoMo'], ['other', 'Khác']].map(([v, t]) => el('option', { value: v, text: t })));
    const msg = el('div');
    const m = UI.modal(box); m.root.firstChild.classList.add('big');
    if (inv.status !== 'paid' && inv.status !== 'cancelled') {
      box.appendChild(el('div', { class: 'bar' }, amount, method, el('button', { class: 'btn', type: 'button', text: 'Ghi nhận thanh toán', onclick: async () => {
        clear(msg); try { await Api.post('/api/admin/payments', { invoiceId: id, amountPaid: Number(amount.value), method: method.value }); UI.toast('Đã ghi nhận'); m.close(); done(); } catch (e) { msg.appendChild(UI.alertBox('err', e.message)); } } })));
      if (!inv.payments.length) box.appendChild(el('button', { class: 'btn sm light', type: 'button', text: 'Hủy hóa đơn', onclick: async () => { if (!confirmBox('Hủy hóa đơn này?')) return; try { await Api.post('/api/admin/invoices/' + id + '/cancel'); m.close(); done(); } catch (e) { msg.appendChild(UI.alertBox('err', e.message)); } } }));
    }
    box.appendChild(msg);
  }

      
C.fn.genInvoice = genInvoice;      C.fn.invoiceDialog = invoiceDialog;
    },
  });
})();
