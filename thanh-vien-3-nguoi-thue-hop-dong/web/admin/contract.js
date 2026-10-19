/* Dựng bản Hợp đồng thuê nhà (A4) từ /api/admin/contracts/:id/document. In bằng trình duyệt → "Lưu thành PDF". */
(async function () {
  const { el, clear } = UI;
  const doc = document.getElementById('doc');
  const id = UI.qparams().id;
  document.getElementById('btn-print').addEventListener('click', () => window.print());
  if (!Api.token.get()) { location.replace('/login.html?next=' + encodeURIComponent(location.pathname + location.search)); return; }
  let d;
  try { d = await Api.get('/api/admin/contracts/' + id + '/document'); }
  catch (e) { clear(doc).appendChild(el('p', { class: 'loading', text: e.message })); return; }

  const c = d.contract, room = d.room, b = d.building, L = d.landlord, T = d.tenant;
  const dmy = (s) => { if (!s) return '…/…/……'; const p = String(s).slice(0, 10).split('-'); return p.length === 3 ? p[2] + '/' + p[1] + '/' + p[0] : s; };
  const dparts = (s) => { const p = String(s || '').slice(0, 10).split('-'); return p.length === 3 ? { d: p[2], m: p[1], y: p[0] } : { d: '…', m: '…', y: '……' }; };
  const vnd = (n) => Number(n || 0).toLocaleString('vi-VN');
  const dot = (v, w) => (v ? String(v) : '.'.repeat(w || 20));
  const diffMonths = (a, z) => { const x = new Date(a), y = new Date(z); y.setDate(y.getDate() + 1); return Math.max(0, (y.getFullYear() - x.getFullYear()) * 12 + (y.getMonth() - x.getMonth()) - (y.getDate() < x.getDate() ? 1 : 0)); };

  /* Đọc số tiền thành chữ tiếng Việt */
  function words(n) {
    n = Math.round(Number(n) || 0);
    if (n === 0) return 'Không đồng';
    const dg = ['không', 'một', 'hai', 'ba', 'bốn', 'năm', 'sáu', 'bảy', 'tám', 'chín'];
    const three = (x, full) => {
      const tr = Math.floor(x / 100), ch = Math.floor((x % 100) / 10), dv = x % 10;
      let s = '';
      if (tr > 0 || full) s += dg[tr] + ' trăm';
      if (ch > 1) { s += ' ' + dg[ch] + ' mươi'; if (dv === 1) s += ' mốt'; else if (dv === 5) s += ' lăm'; else if (dv > 0) s += ' ' + dg[dv]; }
      else if (ch === 1) { s += ' mười'; if (dv === 5) s += ' lăm'; else if (dv > 0) s += ' ' + dg[dv]; }
      else if (dv > 0) { if (tr > 0 || full) s += ' lẻ'; s += ' ' + dg[dv]; }
      return s.trim();
    };
    const units = ['', ' nghìn', ' triệu', ' tỷ'];
    const groups = [];
    while (n > 0) { groups.push(n % 1000); n = Math.floor(n / 1000); }
    const out = [];
    for (let i = groups.length - 1; i >= 0; i--) {
      if (groups[i] === 0) continue;
      const full = i < groups.length - 1;
      out.push(three(groups[i], full) + units[i % 4]);
    }
    const s = out.join(' ').replace(/\s+/g, ' ').trim();
    return s.charAt(0).toUpperCase() + s.slice(1) + ' đồng';
  }

  const months = diffMonths(c.startDate, c.endDate);
  const sign = dparts(c.signDate || c.startDate);
  const rent = c.monthlyRent != null ? c.monthlyRent : room.baseRentPrice;
  const VEH = { motorbike: 'Xe máy', car: 'Ô tô', bicycle: 'Xe đạp' };
  const payDay = c.paymentDay || 5;
  const notice = c.noticeDays || 30;
  const late = c.lateFeePercent != null ? c.lateFeePercent : 0;
  const elec = c.electricPrice, water = c.waterPrice;

  const P = (...k) => el('p', {}, ...k);
  const H = (t) => el('h2', { text: t });
  const B = (t) => el('b', { text: t });
  function partyBlock(title, rows) {
    return el('div', {}, el('p', {}, B(title)),
      el('table', { class: 'party' }, el('tbody', {}, rows.map(([k, v]) => el('tr', {}, el('td', { text: k }), el('td', { text: ': ' + v }))))));
  }
  const landAddr = L.address, tenAddr = T.permanentAddress;

  const parts = [];
  parts.push(el('p', { class: 'nation', text: 'CỘNG HÒA XÃ HỘI CHỦ NGHĨA VIỆT NAM' }), el('p', { class: 'motto', text: 'Độc lập – Tự do – Hạnh phúc' }), el('p', { class: 'c', text: '—————————' }));
  parts.push(el('h1', { text: 'Hợp đồng thuê nhà ở' }), el('p', { class: 'num', text: 'Số: ' + (c.contractCode || ('HD-' + c.contractId)) }));
  parts.push(P('Căn cứ Bộ luật Dân sự năm 2015;'), P('Căn cứ Luật Nhà ở hiện hành và các văn bản hướng dẫn thi hành;'), P('Căn cứ nhu cầu và khả năng của các bên.'));
  parts.push(P('Hôm nay, ngày ' + sign.d + ' tháng ' + sign.m + ' năm ' + sign.y + (c.signPlace ? ', tại ' + c.signPlace : '') + ', chúng tôi gồm:'));

  parts.push(partyBlock('BÊN CHO THUÊ (Bên A):', [
    ['Ông/Bà', (L.fullName || '').toUpperCase()], ['Ngày sinh', dmy(L.birthDate)], ['CCCD/CMND số', dot(L.idNumber, 14) + (L.idIssueDate ? ', cấp ngày ' + dmy(L.idIssueDate) : '') + (L.idIssuePlace ? ' tại ' + L.idIssuePlace : '')],
    ['Địa chỉ thường trú', dot(landAddr, 40)], ['Điện thoại', dot(L.phone, 14)], ['Tài khoản ngân hàng', L.bankAccount ? L.bankAccount + (L.bankName ? ' – ' + L.bankName : '') : dot('', 25)]]));
  parts.push(partyBlock('BÊN THUÊ (Bên B):', [
    ['Ông/Bà', (T.fullName || '').toUpperCase()], ['Giới tính / Ngày sinh', (T.gender || '…') + ' / ' + dmy(T.birthDate)], ['CCCD/CMND số', dot(T.idNumber, 14) + (T.idIssueDate ? ', cấp ngày ' + dmy(T.idIssueDate) : '') + (T.idIssuePlace ? ' tại ' + T.idIssuePlace : '')],
    ['Địa chỉ thường trú', dot(tenAddr, 40)], ['Điện thoại', dot(T.phone, 14)], ['Email', T.email || '…']]));
  parts.push(P('Hai bên cùng thỏa thuận và thống nhất ký kết hợp đồng thuê nhà ở với các điều khoản sau đây:'));

  parts.push(H('Điều 1. Đối tượng và mục đích thuê'));
  parts.push(P('1.1. Bên A đồng ý cho Bên B thuê ', B(room.typeName + ' – phòng ' + room.roomNumber), ' thuộc tòa nhà ', B(b.name), ', địa chỉ: ' + b.fullAddress + '.'));
  parts.push(P('1.2. Diện tích sử dụng: ' + (room.areaM2 ? String(room.areaM2).replace('.', ',') + ' m²' : '……… m²') + '; số người ở tối đa: ' + room.maxOccupants + ' người.'));
  parts.push(P('1.3. Mục đích thuê: để ở. Bên B không được sử dụng vào mục đích kinh doanh, sản xuất hoặc hoạt động trái pháp luật khi chưa có sự đồng ý bằng văn bản của Bên A.'));

  parts.push(H('Điều 2. Thời hạn thuê'));
  parts.push(P('Thời hạn thuê là ' + months + ' tháng, kể từ ngày ' + dmy(c.startDate) + ' đến hết ngày ' + dmy(c.endDate) + '. Khi hết hạn, nếu hai bên có nhu cầu tiếp tục thì thỏa thuận gia hạn bằng phụ lục hợp đồng; Bên B có quyền được ưu tiên thuê tiếp nếu đã thực hiện đúng hợp đồng.'));

  parts.push(H('Điều 3. Giá thuê, tiền đặt cọc và phương thức thanh toán'));
  parts.push(P('3.1. Giá thuê: ', B(vnd(rent) + ' đồng/tháng'), ' (Bằng chữ: ' + words(rent).toLowerCase() + ').'));
  parts.push(P('3.2. Tiền đặt cọc: ', B(vnd(c.depositAmount) + ' đồng'), ' (Bằng chữ: ' + words(c.depositAmount).toLowerCase() + '). Bên A hoàn trả tiền cọc cho Bên B khi chấm dứt hợp đồng đúng quy định, sau khi trừ các khoản Bên B còn nợ hoặc chi phí sửa chữa hư hỏng do lỗi của Bên B (nếu có).'));
  parts.push(P('3.3. Tiền thuê và các chi phí phát sinh được thanh toán một lần mỗi tháng, chậm nhất vào ngày ' + payDay + ' hằng tháng, bằng hình thức ' + (c.paymentMethod ? c.paymentMethod.toLowerCase() : 'tiền mặt hoặc chuyển khoản') + '.'));
  parts.push(P('3.4. Thanh toán chậm quá hạn: Bên B chịu phạt ' + (late ? late + '%' : '……%') + ' trên số tiền chậm thanh toán cho mỗi tháng chậm (tính theo số ngày thực tế); quá 15 ngày không thanh toán, Bên A có quyền đơn phương chấm dứt hợp đồng theo Điều 8.'));
  parts.push(P('3.5. Bên A có nghĩa vụ lập hóa đơn / biên nhận cho mỗi lần thanh toán của Bên B.'));

  parts.push(H('Điều 4. Chi phí điện, nước và dịch vụ'));
  const svcRows = [['Điện', elec != null ? vnd(elec) + ' đ/kWh' : 'Theo bảng giá niêm yết', 'Theo chỉ số công tơ chốt hằng tháng'], ['Nước', water != null ? vnd(water) + ' đ/m³' : 'Theo bảng giá niêm yết', 'Theo chỉ số đồng hồ chốt hằng tháng']];
  (d.services || []).filter((s) => s.calcType !== 'METER_ELECTRIC' && s.calcType !== 'METER_WATER').forEach((s) => svcRows.push([s.serviceName, vnd(s.unitPrice) + ' đ/' + (s.unit || 'tháng'), s.calcType === 'PER_VEHICLE' ? 'Theo số phương tiện' : 'Cố định hằng tháng']));
  parts.push(el('table', {}, el('thead', {}, el('tr', {}, ['Khoản mục', 'Đơn giá', 'Cách tính'].map((h) => el('th', { text: h })))), el('tbody', {}, svcRows.map((r) => el('tr', {}, r.map((x) => el('td', { text: x })))))));
  if (c.serviceNote) parts.push(P('Ghi chú: ' + c.serviceNote));
  parts.push(P('Bên A được điều chỉnh đơn giá dịch vụ khi nhà cung cấp thay đổi giá, nhưng phải thông báo cho Bên B trước ít nhất 15 ngày.'));

  parts.push(H('Điều 5. Quyền và nghĩa vụ của Bên A'));
  ['Bàn giao phòng và tài sản đúng tình trạng thỏa thuận (Phụ lục 1); bảo đảm quyền sử dụng ổn định cho Bên B trong thời hạn thuê.',
   'Bảo trì, sửa chữa kết cấu và các hư hỏng không do lỗi của Bên B trong thời gian hợp lý kể từ khi nhận thông báo.',
   'Hướng dẫn Bên B thực hiện quy định về an ninh trật tự, phòng cháy chữa cháy, vệ sinh; thực hiện đăng ký tạm trú cho Bên B theo quy định pháp luật về cư trú.',
   'Chỉ vào phòng khi cần thiết (sửa chữa, kiểm tra an toàn) và phải báo trước cho Bên B, trừ trường hợp khẩn cấp.',
   'Được nhận đủ tiền thuê, tiền dịch vụ đúng hạn; được yêu cầu Bên B bồi thường thiệt hại do lỗi của Bên B gây ra; được lấy lại phòng khi hết hạn hoặc khi Bên B vi phạm nghiêm trọng hợp đồng.'
  ].forEach((t, i) => parts.push(P('5.' + (i + 1) + '. ' + t)));

  parts.push(H('Điều 6. Quyền và nghĩa vụ của Bên B'));
  ['Sử dụng phòng đúng mục đích, giữ gìn tài sản; hư hỏng do lỗi của Bên B thì Bên B tự sửa chữa hoặc bồi thường.',
   'Thanh toán đầy đủ, đúng hạn tiền thuê và các chi phí khác theo Điều 3 và Điều 4.',
   'Không tự ý sửa chữa, cải tạo, thay đổi kết cấu; không cho người khác thuê lại hoặc chuyển nhượng quyền thuê khi chưa có đồng ý bằng văn bản của Bên A.',
   'Chấp hành quy định về an ninh trật tự, phòng cháy chữa cháy, vệ sinh môi trường, giờ giấc sinh hoạt chung; không tàng trữ chất cháy nổ, chất cấm; không gây ồn ào ảnh hưởng người xung quanh.',
   'Khai báo đầy đủ người ở cùng và phương tiện (Phụ lục 2); cung cấp giấy tờ tùy thân để Bên A thực hiện đăng ký tạm trú.',
   'Trả lại phòng và tài sản trong tình trạng ban đầu (trừ hao mòn tự nhiên) khi hết hạn hoặc chấm dứt hợp đồng.'
  ].forEach((t, i) => parts.push(P('6.' + (i + 1) + '. ' + t)));

  parts.push(H('Điều 7. Bàn giao tài sản, người ở cùng và phương tiện'));
  parts.push(P('Hai bên cùng xác nhận danh mục tài sản bàn giao, người ở cùng và phương tiện được liệt kê tại Phụ lục 1 và Phụ lục 2 đính kèm hợp đồng này.'));

  parts.push(H('Điều 8. Chấm dứt hợp đồng'));
  parts.push(P('8.1. Hợp đồng chấm dứt khi: hết thời hạn thuê mà không gia hạn; hai bên thỏa thuận chấm dứt; một bên đơn phương chấm dứt theo quy định dưới đây; phòng bị hư hỏng nặng hoặc bị giải tỏa, phá dỡ theo quyết định của cơ quan có thẩm quyền.'));
  parts.push(P('8.2. Bên muốn đơn phương chấm dứt hợp đồng khi chưa hết hạn phải thông báo bằng văn bản cho bên kia trước ít nhất ' + notice + ' ngày.'));
  parts.push(P('8.3. Bên A được đơn phương chấm dứt, không hoàn tiền cọc nếu Bên B: (a) chậm thanh toán quá 15 ngày; (b) sử dụng phòng sai mục đích; (c) tự ý cho thuê lại; (d) vi phạm nghiêm trọng quy định an ninh, phòng cháy chữa cháy hoặc pháp luật.'));
  parts.push(P('8.4. Bên B đơn phương chấm dứt không đúng thời hạn báo trước hoặc không có căn cứ chính đáng thì không được hoàn lại tiền cọc. Nếu Bên A vi phạm nghĩa vụ bàn giao, bảo đảm quyền sử dụng ổn định cho Bên B, Bên A hoàn trả tiền cọc và bồi thường theo thiệt hại thực tế.'));

  parts.push(H('Điều 9. Cam kết chung và giải quyết tranh chấp'));
  parts.push(P('9.1. Hai bên cam kết thực hiện đúng các điều khoản của hợp đồng; mọi sửa đổi, bổ sung phải lập thành văn bản và có chữ ký của cả hai bên.'));
  parts.push(P('9.2. Tranh chấp phát sinh được giải quyết trước hết bằng thương lượng, hòa giải. Nếu không thỏa thuận được, mỗi bên có quyền yêu cầu Tòa án nhân dân có thẩm quyền nơi có nhà cho thuê giải quyết theo quy định pháp luật.'));
  parts.push(P('9.3. Trường hợp bất khả kháng (thiên tai, dịch bệnh, quyết định của cơ quan nhà nước…) thì bên bị ảnh hưởng được miễn trách nhiệm nếu đã thông báo kịp thời cho bên kia.'));
  parts.push(P('9.4. Hợp đồng gồm các phụ lục là bộ phận không tách rời, lập thành 02 bản có giá trị pháp lý như nhau, mỗi bên giữ 01 bản, có hiệu lực kể từ ngày ký.'));
  if (c.extraTerms) { parts.push(H('Điều 10. Điều khoản bổ sung')); c.extraTerms.split(/\r?\n/).filter((x) => x.trim()).forEach((x) => parts.push(P(x.trim()))); }

  parts.push(el('div', { class: 'sign' },
    el('div', {}, el('p', {}, B('ĐẠI DIỆN BÊN B')), el('p', {}, el('i', { text: '(Ký và ghi rõ họ tên)' })), el('div', { class: 'sp' }), el('p', {}, B(T.fullName))),
    el('div', {}, el('p', {}, B('ĐẠI DIỆN BÊN A')), el('p', {}, el('i', { text: '(Ký và ghi rõ họ tên)' })), el('div', { class: 'sp' }), el('p', {}, B(L.fullName)))));

  // Phụ lục 1: tài sản
  const pb = el('div', { style: 'page-break-before:always' });
  pb.appendChild(el('h1', { text: 'Phụ lục 1' })); pb.appendChild(el('p', { class: 'c', text: 'BIÊN BẢN BÀN GIAO TÀI SẢN' }));
  pb.appendChild(P('Kèm theo Hợp đồng số ' + (c.contractCode || c.contractId) + ' – phòng ' + room.roomNumber + ', ' + b.name + '.'));
  const assets = d.assets && d.assets.length ? d.assets : [{ name: '', quantity: '', condition: '' }, { name: '', quantity: '', condition: '' }, { name: '', quantity: '', condition: '' }];
  pb.appendChild(el('table', {}, el('thead', {}, el('tr', {}, ['STT', 'Tên tài sản', 'Số lượng', 'Tình trạng'].map((h) => el('th', { text: h })))),
    el('tbody', {}, assets.map((a, i) => el('tr', {}, el('td', { class: 'c', text: String(i + 1) }), el('td', { text: a.name || ' ' }), el('td', { class: 'c', text: a.quantity || '' }), el('td', { text: a.condition || '' }))))));
  pb.appendChild(P('Chỉ số công tơ điện, đồng hồ nước khi bàn giao: Điện ………… kWh; Nước ………… m³.'));

  // Phụ lục 2: người ở cùng và phương tiện
  pb.appendChild(el('h1', { style: 'margin-top:22px', text: 'Phụ lục 2' })); pb.appendChild(el('p', { class: 'c', text: 'DANH SÁCH NGƯỜI Ở CÙNG VÀ PHƯƠNG TIỆN' }));
  const occ = [{ fullName: T.fullName + ' (Bên B)', phone: T.phone, idNumber: T.idNumber, permanentAddress: T.permanentAddress }].concat(d.occupants || []);
  pb.appendChild(el('table', {}, el('thead', {}, el('tr', {}, ['STT', 'Họ và tên', 'CCCD/CMND', 'Điện thoại', 'Thường trú'].map((h) => el('th', { text: h })))),
    el('tbody', {}, occ.map((o, i) => el('tr', {}, el('td', { class: 'c', text: String(i + 1) }), el('td', { text: o.fullName }), el('td', { text: o.idNumber || '' }), el('td', { text: o.phone || '' }), el('td', { text: o.permanentAddress || '' }))))));
  pb.appendChild(P(B('Phương tiện:')));
  const veh = d.vehicles && d.vehicles.length ? d.vehicles : [];
  pb.appendChild(el('table', {}, el('thead', {}, el('tr', {}, ['STT', 'Biển số', 'Loại xe', 'Hãng / màu', 'Chủ xe'].map((h) => el('th', { text: h })))),
    el('tbody', {}, veh.length ? veh.map((v, i) => el('tr', {}, el('td', { class: 'c', text: String(i + 1) }), el('td', { text: v.licensePlate }), el('td', { text: VEH[v.vehicleType] || v.vehicleType || '' }), el('td', { text: v.brandColor || '' }), el('td', { text: v.ownerName }))) : el('tr', {}, el('td', { colspan: 5, class: 'c', text: 'Không có phương tiện đăng ký' })))));
  pb.appendChild(el('div', { class: 'sign' },
    el('div', {}, el('p', {}, B('BÊN B')), el('div', { class: 'sp' }), el('p', {}, B(T.fullName))), el('div', {}, el('p', {}, B('BÊN A')), el('div', { class: 'sp' }), el('p', {}, B(L.fullName)))));
  pb.appendChild(el('p', { class: 'note', text: 'Mẫu hợp đồng do hệ thống NhàTrọ+ lập tự động từ dữ liệu quản lý; các bên nên rà soát nội dung (hoặc tham khảo ý kiến luật sư / công chứng nếu cần) trước khi ký.' }));
  parts.push(pb);

  clear(doc);
  parts.forEach((p) => doc.appendChild(p));
  document.title = 'Hop dong ' + (c.contractCode || c.contractId) + ' - ' + (T.fullName || '');
  if (d.missingInfo && d.missingInfo.length) {
    document.getElementById('warn').appendChild(el('div', { class: 'alert' }, B('Còn thiếu thông tin pháp lý: '), d.missingInfo.join('; '), '. Hãy bổ sung (Người thuê / Tài khoản chủ nhà) để hợp đồng đầy đủ; phần thiếu đang để dấu chấm cho bạn điền tay.'));
  }
})();
