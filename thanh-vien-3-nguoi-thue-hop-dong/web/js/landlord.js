(async function () {
  const { me } = await Layout.init('landlord');
  if (me && me.role === 'MANAGER') { location.replace('/admin/index.html'); return; }
  Forms.build(UI.$('#form-host'), {
    url: '/api/support-requests', extra: { requestType: 'landlord' }, submit: 'Gửi đăng ký', success: 'Đội ngũ sẽ liên hệ để hỗ trợ bạn đăng phòng và tạo tài khoản quản lý.',
    fields: [
      { name: 'fullName', label: 'Họ và tên', required: true, prefill: 'fullName', max: 100 },
      { name: 'phone', label: 'Số điện thoại', type: 'tel', required: true, prefill: 'phone', max: 20 },
      { name: 'desiredArea', label: 'Địa chỉ nhà trọ', placeholder: 'vd: 12 Quang Trung, Gò Vấp', max: 150 },
      { name: 'note', label: 'Thông tin thêm', type: 'textarea', placeholder: 'Số phòng, mức giá dự kiến…' },
    ],
  });
})();
