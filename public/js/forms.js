/* Dựng form đơn giản từ danh sách trường, gửi POST, hiện thông báo. */
(function (w) {
  const { el, clear } = UI;
  function build(host, cfg) {
    const inputs = {};
    const msg = el('div', {});
    const me = Layout.getMe();
    const fields = cfg.fields.map((f) => {
      let inp;
      if (f.type === 'textarea') inp = el('textarea', { placeholder: f.placeholder || '', maxlength: f.max || 1000 });
      else if (f.type === 'select') inp = el('select', {}, f.options.map(([v, t]) => el('option', { value: v, text: t })));
      else inp = el('input', { type: f.type || 'text', placeholder: f.placeholder || '', maxlength: f.max || 150, inputmode: f.inputmode || null });
      if (f.prefill && me && me[f.prefill]) inp.value = me[f.prefill];
      inputs[f.name] = inp;
      return el('div', { class: 'field' }, el('label', { text: f.label + (f.required ? ' *' : '') }), inp);
    });
    const btn = el('button', { class: 'btn orange block', type: 'submit', text: cfg.submit });
    const form = el('form', {}, msg, fields, btn);
    form.addEventListener('submit', async (e) => {
      e.preventDefault(); clear(msg); btn.disabled = true;
      const body = Object.assign({}, cfg.extra || {});
      cfg.fields.forEach((f) => {
        let v = inputs[f.name].value.trim();
        if (v === '') return;
        if (f.number) v = Number(v.replace(/[.,\s]/g, ''));
        body[f.name] = v;
      });
      try {
        await Api.post(cfg.url, body);
        clear(host).appendChild(el('div', { class: 'center' }, el('div', { style: 'font-size:48px', text: '✅' }), el('h2', { text: cfg.successTitle || 'Đã gửi thành công' }), el('p', { class: 'muted', text: cfg.success }), el('a', { class: 'btn', href: '/', text: 'Về trang chủ' })));
      } catch (err) { msg.appendChild(UI.alertBox('err', err.message)); btn.disabled = false; }
    });
    clear(host).appendChild(form);
  }
  w.Forms = { build };
})(window);
