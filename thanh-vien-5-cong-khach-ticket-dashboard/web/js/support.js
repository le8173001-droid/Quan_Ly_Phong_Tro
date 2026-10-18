(async function () {
  await Layout.init('support');
  Forms.build(UI.$('#form-host'), {
    url: '/api/support-requests', submit: 'Gửi yêu cầu hỗ trợ', success: 'Nhân viên sẽ gọi lại cho bạn trong thời gian sớm nhất.',
    fields: [
      { name: 'fullName', label: 'Họ và tên', required: true, prefill: 'fullName', max: 100 },
      { name: 'phone', label: 'Số điện thoại', type: 'tel', required: true, prefill: 'phone', max: 20 },
      { name: 'desiredArea', label: 'Khu vực muốn thuê', placeholder: 'vd: Gò Vấp, gần ĐH Công nghiệp', max: 150 },
      { name: 'budgetMax', label: 'Ngân sách tối đa (đồng/tháng)', placeholder: 'vd: 3000000', inputmode: 'numeric', number: true },
      { name: 'note', label: 'Yêu cầu thêm', type: 'textarea', placeholder: 'Số người ở, thời gian dọn vào, tiện nghi cần có…' },
    ],
  });
})();
