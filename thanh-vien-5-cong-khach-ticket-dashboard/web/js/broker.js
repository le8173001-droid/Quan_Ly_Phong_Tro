(async function () {
  await Layout.init('support');
  Forms.build(UI.$('#form-host'), {
    url: '/api/broker-applications', submit: 'Đăng ký làm môi giới', success: 'Chúng tôi sẽ liên hệ để hướng dẫn bạn các bước tiếp theo.',
    fields: [
      { name: 'fullName', label: 'Họ và tên', required: true, prefill: 'fullName', max: 100 },
      { name: 'phone', label: 'Số điện thoại', type: 'tel', required: true, prefill: 'phone', max: 20 },
      { name: 'email', label: 'Email', type: 'email', prefill: 'email', max: 100 },
      { name: 'workArea', label: 'Khu vực hoạt động', placeholder: 'vd: Quận 7, Nhà Bè', max: 150 },
      { name: 'experience', label: 'Kinh nghiệm', type: 'textarea', placeholder: 'Bạn đã làm môi giới / cho thuê nhà bao lâu?' },
    ],
  });
})();
