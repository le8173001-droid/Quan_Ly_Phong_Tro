/* Gọi API backend C++ (cùng origin). Token đăng nhập lưu ở localStorage. */
(function (w) {
  const KEY = 'nt_token';
  const store = {
    get() { try { return localStorage.getItem(KEY); } catch (e) { return null; } },
    set(t) { try { localStorage.setItem(KEY, t); } catch (e) {} },
    clear() { try { localStorage.removeItem(KEY); } catch (e) {} },
  };

  function qs(params) {
    const p = new URLSearchParams();
    Object.entries(params || {}).forEach(([k, v]) => {
      if (v !== undefined && v !== null && v !== '' && v !== false) p.append(k, v === true ? 'true' : v);
    });
    const s = p.toString();
    return s ? '?' + s : '';
  }

  async function request(method, path, body) {
    const headers = {};
    if (body !== undefined) headers['Content-Type'] = 'application/json';
    const t = store.get();
    if (t) headers['Authorization'] = 'Bearer ' + t;
    let res;
    try {
      res = await fetch(path, { method, headers, body: body === undefined ? undefined : JSON.stringify(body) });
    } catch (e) {
      throw Object.assign(new Error('Không kết nối được máy chủ'), { status: 0 });
    }
    let data = null;
    const text = await res.text();
    if (text) { try { data = JSON.parse(text); } catch (e) { data = null; } }
    if (!res.ok) {
      if (res.status === 401 && t && !path.startsWith('/api/auth/login')) store.clear();
      throw Object.assign(new Error((data && data.error) || 'Có lỗi xảy ra (' + res.status + ')'), { status: res.status });
    }
    return data;
  }

  /* Tải ảnh lên (multipart). files: FileList | File[] -> mảng {url, fileName, size} */
  async function upload(files) {
    const fd = new FormData();
    Array.from(files).forEach((f) => fd.append('file', f));
    const headers = {};
    const t = store.get();
    if (t) headers['Authorization'] = 'Bearer ' + t;
    let res;
    try { res = await fetch('/api/uploads', { method: 'POST', headers, body: fd }); }
    catch (e) { throw Object.assign(new Error('Không kết nối được máy chủ'), { status: 0 }); }
    const text = await res.text();
    let data = null; try { data = JSON.parse(text); } catch (e) { /* không phải JSON */ }
    if (!res.ok) throw Object.assign(new Error((data && data.error) || 'Tải ảnh thất bại (' + res.status + ')'), { status: res.status });
    return data.files;
  }

  w.Api = {
    upload,
    token: store,
    qs,
    get: (p, params) => request('GET', p + qs(params)),
    post: (p, b) => request('POST', p, b === undefined ? {} : b),
    put: (p, b) => request('PUT', p, b),
    del: (p) => request('DELETE', p),
  };
})(window);
