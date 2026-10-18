#!/usr/bin/env python3
"""Kiểm thử giao diện bằng trình duyệt thật (Playwright). Tự chạy server với DB tạm.
Cần: pip install playwright && playwright install chromium
Chạy: python tests/e2e_test.py [đường_dẫn_file_chạy]   (ảnh chụp lưu ở tests/shots/)"""
import re, json, os, subprocess, sys, tempfile, time, threading, urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlparse, parse_qs
from playwright.sync_api import sync_playwright, expect

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT, MOCK = 18777, 18778
BASE = f"http://127.0.0.1:{PORT}"
CID = "e2e-client.apps.googleusercontent.com"
fails = []
def check(c, m):
    if not c: fails.append(m); print("  LỖI:", m)

class G(BaseHTTPRequestHandler):
    def do_GET(self):
        t = parse_qs(urlparse(self.path).query).get("id_token", [""])[0]
        ok = t == "fake-google-token"
        body = json.dumps({"sub": "g-e2e", "email": "e2e.google@gmail.com", "email_verified": "true", "name": "E2E Google", "aud": CID,
                           "iss": "https://accounts.google.com", "exp": str(int(time.time()) + 3000)} if ok else {"error": "bad"}).encode()
        self.send_response(200 if ok else 400); self.end_headers(); self.wfile.write(body)
    def log_message(self, *a): pass

def main():
    binary = sys.argv[1] if len(sys.argv) > 1 else next(p for p in (os.path.join(ROOT, "build", n) for n in ("nhatro", "nhatro.exe")) if os.path.exists(p))
    shots = os.path.join(ROOT, "tests", "shots"); os.makedirs(shots, exist_ok=True)
    tmp = tempfile.mkdtemp()
    srv = HTTPServer(("127.0.0.1", MOCK), G); threading.Thread(target=srv.serve_forever, daemon=True).start()
    env = dict(os.environ, GOOGLE_CLIENT_ID=CID, GOOGLE_TOKENINFO_URL=f"http://127.0.0.1:{MOCK}/t")
    proc = subprocess.Popen([binary, "--port", str(PORT), "--db", os.path.join(tmp, "e.db"), "--reset"], cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        for _ in range(100):
            try: urllib.request.urlopen(BASE + "/api/auth/config", timeout=1); break
            except Exception: time.sleep(0.1)
        with sync_playwright() as p:
            exe = os.environ.get("CHROMIUM_PATH")
            b = p.chromium.launch(executable_path=exe, args=["--no-sandbox"]) if exe else p.chromium.launch(args=["--no-sandbox"])
            ctx = b.new_context(viewport={"width": 1280, "height": 900}); pg = ctx.new_page()
            errs = []
            pg.on("pageerror", lambda e: errs.append(str(e)))
            pg.on("console", lambda m: errs.append(m.text) if m.type == "error" and "accounts.google" not in m.text and "ERR_" not in m.text and "409" not in m.text else None)

            print("• Trang chủ")
            pg.goto(BASE + "/"); pg.wait_for_selector(".quick a, .quick button"); pg.wait_for_selector(".room-card")
            check(pg.locator(".quick > *").count() == 6, "6 ô chức năng nhanh")
            check(pg.locator(".hero .chip").count() == 7, "7 chip loại hình (Tất cả + 6)")
            check(pg.locator(".program").count() >= 4, "có chương trình đang diễn ra")
            check(pg.locator(".hotdeal .timer").count() >= 1, "HOTDEAL có đồng hồ đếm ngược")
            pg.screenshot(path=f"{shots}/home.png", full_page=True)

            print("• Tìm kiếm từ ô hero")
            pg.click(".chip:has-text('Ký túc xá')"); pg.click("button:has-text('Tìm kiếm')")
            pg.wait_for_url("**/search.html*"); pg.wait_for_selector(".room-card")
            check(pg.locator(".room-card").count() >= 2, "lọc ký túc xá có kết quả")
            pg.fill(".sb-kw input", "go vap"); pg.click(".chip:has-text('Tất cả')"); pg.click("button:has-text('Tìm kiếm')")
            pg.wait_for_timeout(800)
            check(pg.locator(".room-card").count() >= 3, "tìm 'go vap' không dấu có kết quả")
            pg.select_option("#sort", "price_asc"); pg.wait_for_timeout(600)
            pg.screenshot(path=f"{shots}/search.png", full_page=True)

            print("• Xem SĐT + chi tiết")
            pg.locator(".room-card button:has-text('Xem SĐT')").first.click(); pg.wait_for_timeout(500)
            check(pg.locator(".room-card a[href^='tel:']").count() >= 1, "bấm Xem SĐT hiện số điện thoại")
            pg.locator(".room-card h3 a").first.click(); pg.wait_for_selector(".detail")
            pg.fill("input[placeholder='Họ và tên']", "Khách E2E"); pg.fill("input[placeholder='Số điện thoại']", "0911222333")
            pg.click("button:has-text('Gửi yêu cầu xem phòng')"); pg.wait_for_selector(".alert.ok")
            pg.screenshot(path=f"{shots}/room.png", full_page=True)

            print("• Tra cứu hóa đơn")
            pg.goto(BASE + "/invoice.html"); pg.wait_for_selector("form input"); pg.fill("input[type=tel]", "0987654321"); pg.fill("input[placeholder^='Số phòng']", "P102")
            pg.click("button:has-text('Tra cứu')"); pg.wait_for_selector("text=INV-202610-0001")
            check(pg.locator("text=Trần V**").count() >= 1, "tên người thuê được che")

            print("• Hỗ trợ tìm phòng")
            pg.goto(BASE + "/support.html"); pg.wait_for_selector("form input"); pg.fill("form input >> nth=0", "Khách Hỗ Trợ"); pg.fill("input[type=tel]", "0912345600")
            pg.click("button:has-text('Gửi yêu cầu')"); pg.wait_for_selector("text=Đã gửi thành công")

            print("• Đăng ký + yêu thích + đăng xuất")
            pg.goto(BASE + "/login.html?tab=register")
            for n, v in (("username", "e2euser"), ("fullName", "Người Dùng E2E"), ("email", "e2e@example.com"), ("password", "Matkhau123"), ("password2", "Matkhau123")):
                pg.fill(f"input[name={n}]", v)
            pg.click("button:has-text('Tạo tài khoản') >> nth=-1"); pg.wait_for_url(BASE + "/"); pg.wait_for_selector(".user-menu")
            pg.locator(".room-card .fav").first.click(); pg.wait_for_timeout(500)
            pg.goto(BASE + "/favorites.html"); pg.wait_for_selector(".room-card")
            check(pg.locator(".room-card").count() == 1, "phòng đã lưu hiện ở trang yêu thích")
            pg.click(".user-menu > button"); pg.click("text=Đăng xuất"); pg.wait_for_url(BASE + "/")
            pg.goto(BASE + "/admin/index.html"); pg.wait_for_url(re.compile("login\\.html"))
            check(True, "chưa đăng nhập vào admin thì bị chuyển về login")

            print("• Đăng nhập thường (người thuê không vào được admin)")
            pg.fill("input[name=username]", "e2euser"); pg.fill("input[name=password]", "Matkhau123"); pg.click("button[type=submit]"); pg.wait_for_url(BASE + "/")
            pg.goto(BASE + "/admin/index.html"); pg.wait_for_url(BASE + "/")
            check(True, "TENANT bị đẩy ra khỏi admin")

            print("• Đăng nhập Google (server Google giả)")
            ctx.clear_cookies(); pg.evaluate("localStorage.clear()")
            pg.goto(BASE + "/login.html"); pg.wait_for_selector(".tabs-auth")
            tok = pg.evaluate("""async () => { const r = await fetch('/api/auth/google',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({credential:'fake-google-token'})}); return (await r.json()); }""")
            check(tok.get("user", {}).get("email") == "e2e.google@gmail.com", "API Google trả về người dùng")

            print("• Quản trị (ADMIN)")
            pg.evaluate("localStorage.clear()"); pg.goto(BASE + "/login.html")
            pg.fill("input[name=username]", "admin"); pg.fill("input[name=password]", "Admin@123"); pg.click("button[type=submit]")
            pg.wait_for_url("**/admin/index.html"); pg.wait_for_selector(".kpi")
            pg.screenshot(path=f"{shots}/admin_dashboard.png")
            check(pg.locator(".adm-side a:has-text('Chủ tòa nhà')").count() == 1, "ADMIN thấy mục Chủ tòa nhà")
            pg.goto(BASE + "/admin/index.html#rooms"); pg.wait_for_selector("tbody tr")
            check(pg.locator("tr.grp-b").count() >= 3, "danh sách phòng nhóm theo tòa nhà")
            pg.click("button:has-text('Thêm phòng')"); pg.wait_for_selector(".modal form")
            pg.select_option(".modal .field:has(label:has-text('Tòa nhà')) select", index=1)
            pg.select_option(".modal .field:has(label:has-text('Loại hình')) select", index=1)
            pg.fill(".modal .field:has(label:has-text('Mã phòng')) input", "E2E1"); pg.fill(".modal .field:has(label:has-text('Tiêu đề')) input", "Phòng thử E2E")
            pg.fill(".modal .field:has(label:has-text('Giá thuê')) input", "2100000")
            png = os.path.join(tmp, "a.png")
            open(png, "wb").write(bytes.fromhex("89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4890000000d49444154789c6360000002000001e221bc330000000049454e44ae426082"))
            pg.set_input_files(".modal input[type=file]", png); pg.wait_for_selector(".modal .up-item img")
            check(pg.locator(".modal .up-item").count() == 1, "tải ảnh phòng từ thiết bị lên")
            pg.click(".modal button:has-text('Lưu')"); pg.wait_for_timeout(900)
            pg.fill(".bar input[type=search]", "E2E1"); pg.wait_for_timeout(800)
            check(pg.locator("tbody tr:has-text('E2E1')").count() == 1, "tạo phòng mới trong admin")
            src = pg.locator("tbody tr:has-text('E2E1') img.mini").get_attribute("src")
            check(src and src.startswith("/uploads/"), "ảnh upload được lưu làm ảnh đại diện: " + str(src))
            code = pg.evaluate("async s => (await fetch(s)).status", src)
            check(code == 200, "file ảnh upload truy cập được")
            pg.once("dialog", lambda d: d.accept()); pg.click("tbody tr:has-text('E2E1') button:has-text('Ẩn')"); pg.wait_for_timeout(700)
            pg.select_option(".bar select >> nth=1", "hidden"); pg.wait_for_timeout(700)
            check(pg.locator("tbody tr:has-text('E2E1') button:has-text('Khôi phục')").count() == 1, "phòng ẩn có nút Khôi phục")
            pg.click("tbody tr:has-text('E2E1') button:has-text('Khôi phục')"); pg.wait_for_timeout(700)

            print("• Thêm tòa nhà: chọn địa chỉ Cũ + ảnh")
            pg.goto(BASE + "/admin/index.html#buildings"); pg.wait_for_selector("tbody tr")
            pg.click("button:has-text('Thêm mới')"); pg.wait_for_selector(".modal .loc")
            pg.fill(".modal .field:has(label:has-text('Tên tòa nhà')) input", "Tòa E2E"); pg.fill(".modal .field:has(label:has-text('Số nhà')) input", "1 Đường Thử")
            pg.select_option(".modal .field:has(label:has-text('Chủ tòa nhà')) select", index=1)
            pg.click(".modal .seg-b:has-text('Cũ')")
            pg.click(".modal .loc .cb-input >> nth=0"); pg.click(".modal .cb-opt:has-text('Hồ Chí Minh')")
            pg.click(".modal .loc .cb-input >> nth=1"); pg.click(".modal .cb-opt span:text-is('Quận 1')")
            pg.click(".modal .loc .cb-input >> nth=2"); pg.fill(".modal .loc .cb-input >> nth=2", "Bến Nghé"); pg.click(".modal .cb-opt:has-text('Bến Nghé')")
            pg.set_input_files(".modal input[type=file]", png); pg.wait_for_selector(".modal .up-item img")
            pg.click(".modal button:has-text('Lưu')"); pg.wait_for_timeout(1000)
            pg.fill(".bar input[type=search]", "Tòa E2E"); pg.wait_for_timeout(800)
            check(pg.locator("tbody tr:has-text('Tòa E2E'):has-text('Bến Nghé')").count() == 1, "tạo tòa nhà có phường/xã hệ cũ")
            bj = pg.evaluate("""async () => { const t = localStorage.getItem('nt_token'); const r = await fetch('/api/admin/buildings?q=' + encodeURIComponent('Tòa E2E'), {headers:{Authorization:'Bearer '+t}}); const it = (await r.json()).items[0]; const r2 = await fetch('/api/admin/buildings/' + it.buildingId, {headers:{Authorization:'Bearer '+t}}); return await r2.json(); }""")
            check(bj and bj.get("coverUrl", "").startswith("/uploads/") and len(bj.get("images", [])) == 1, "ảnh tòa nhà lưu vào database")

            print("• Sửa hotline / Zalo")
            pg.goto(BASE + "/admin/index.html#settings"); pg.wait_for_selector(".set-form input.hl")
            pg.locator(".set-form input.hl").nth(0).fill("1900 1234"); pg.locator(".set-form input.hl").nth(1).fill("0909 123 456")
            pg.click(".set-form button:has-text('Lưu')"); pg.wait_for_selector(".alert.ok")
            pg.goto(BASE + "/"); pg.wait_for_selector(".hotlines a")
            check("1900 1234" in pg.inner_text(".hotlines") and "0909 123 456" in pg.inner_text(".hotlines"), "hotline / Zalo mới hiện ở trang chủ")

            print("• Khuyến mãi % + mục nổi bật")
            pg.goto(BASE + "/admin/index.html#featured"); pg.wait_for_selector(".set-form")
            pg.fill("input[placeholder^='Tìm phòng để ghim']", "E2E1"); pg.wait_for_timeout(900)
            pg.click("button:has-text('☆ Ghim')"); pg.wait_for_timeout(900)
            check(pg.locator("tbody tr:has-text('E2E1')").count() >= 1, "ghim phòng vào mục nổi bật")
            pg.goto(BASE + "/admin/index.html#promotions"); pg.wait_for_selector("tbody tr")
            check(pg.locator("th:has-text('% giảm')").count() == 1, "bảng khuyến mãi có cột % giảm")

            print("• Hợp đồng in PDF")
            pg.goto(BASE + "/admin/contract.html?id=1"); pg.wait_for_selector(".sheet h1")
            check("HỢP ĐỒNG THUÊ NHÀ Ở" in pg.inner_text(".sheet").upper() and "Phụ lục 1" in pg.inner_text(".sheet"), "trang hợp đồng có nội dung pháp lý + phụ lục")
            pg.pdf(path=f"{shots}/contract.pdf", format="A4", print_background=True, prefer_css_page_size=True)
            check(os.path.getsize(f"{shots}/contract.pdf") > 20000, "xuất được file PDF hợp đồng")
            pg.screenshot(path=f"{shots}/contract.png")

            for h in ("owners", "contracts", "invoices", "promotions", "users", "settings", "tenants", "vehicles", "tickets", "services"):
                pg.goto(BASE + "/admin/index.html#" + h); pg.wait_for_selector("h1"); pg.wait_for_timeout(500)
                check(pg.locator(".alert.err").count() == 0, f"trang admin '{h}' tải không lỗi")
            pg.goto(BASE + "/admin/index.html#tenants"); pg.wait_for_selector("tr.grp-r")
            check(pg.locator("tr.grp-b").count() >= 2 and pg.locator("tr.grp-r").count() >= 3, "người thuê nhóm theo tòa → phòng")
            pg.screenshot(path=f"{shots}/admin_tenants.png")

            print("• Chủ tòa nhà (MANAGER)")
            pg.evaluate("localStorage.clear()"); pg.goto(BASE + "/login.html")
            pg.fill("input[name=username]", "levinh"); pg.fill("input[name=password]", "Manager@123"); pg.click("button[type=submit]")
            pg.wait_for_url("**/admin/index.html"); pg.wait_for_selector(".kpi")
            check(pg.locator(".adm-side a:has-text('Chủ tòa nhà')").count() == 0 and pg.locator(".adm-side a:has-text('Hotline')").count() == 0, "MANAGER không thấy mục dành cho ADMIN")
            check(pg.locator("#main-nav a:has-text('Tôi là chủ nhà')").count() == 0 and pg.locator(".landlord-link:has-text('Tôi là chủ nhà')").count() == 0, "MANAGER không còn mục 'Tôi là chủ nhà'")
            pg.goto(BASE + "/profile.html#manager"); pg.wait_for_selector("#manager")
            pg.fill("#manager input[type=password]", "Manager@123"); pg.click("#manager button:has-text('Hủy quyền')"); pg.wait_for_selector("#manager .alert.err")
            check("hợp đồng" in pg.inner_text("#manager .alert.err"), "không hủy được quyền khi còn hợp đồng hiệu lực")
            pg.goto(BASE + "/admin/index.html#settings"); pg.wait_for_url(re.compile("index\\.html"))
            check(pg.locator("input.hl").count() == 0, "MANAGER không sửa được hotline / Zalo")

            print("• Gợi ý khu vực gần vị trí thiết bị")
            ctx2 = b.new_context(viewport={"width": 1280, "height": 900}, geolocation={"latitude": 10.8385, "longitude": 106.6652}, permissions=["geolocation"])
            pg2 = ctx2.new_page(); pg2.goto(BASE + "/"); pg2.wait_for_selector("#near-btn"); pg2.click("#near-btn"); pg2.wait_for_selector(".near-areas a", timeout=8000)
            check(pg2.locator(".near-areas a").count() >= 1, "gợi ý khu vực gần vị trí thiết bị có phòng")
            pg2.goto(BASE + "/search.html"); pg2.wait_for_selector(".sb-area"); pg2.click(".sb-area"); pg2.click(".loc-pop button:has-text('Gợi ý khu vực gần tôi')")
            pg2.wait_for_selector(".loc-near .chip", timeout=8000)
            pg2.click(".loc-near .chip >> nth=0"); pg2.click("button:has-text('Tìm kiếm')"); pg2.wait_for_timeout(900)
            check(pg2.locator(".room-card").count() >= 1, "chọn khu vực gợi ý ra phòng")
            ctx2.close()

            print("• Chủ nhà thêm tòa nhà (gõ tên tỉnh, không bấm chọn) + khung địa chỉ không bị cắt + mục gần học tập/làm việc")
            pg.goto(BASE + "/admin/index.html#buildings"); pg.wait_for_selector("tbody tr")
            pg.click("button:has-text('Thêm mới')"); pg.wait_for_selector(".modal .loc")
            pg.fill(".modal .field:has(label:has-text('Tên tòa nhà')) input", "Tòa E2E"); pg.fill(".modal .field:has(label:has-text('Số nhà')) input", "9 Đường Thử")
            pg.fill(".modal .loc .cb-input >> nth=0", "Hà Nội"); pg.click(".modal h3")
            pg.click(".modal button:has-text('Lưu')"); pg.wait_for_timeout(900)
            check(pg.locator(".modal").count() == 0 and pg.locator("tbody tr:has-text('Tòa E2E')").count() >= 1, "chủ nhà thêm tòa nhà thành công, tự nhận tỉnh đã gõ")
            pg.click("button:has-text('Thêm mới')"); pg.wait_for_selector(".modal .loc")
            pg.click(".modal button:has-text('Lưu')"); pg.wait_for_timeout(500)
            check("Còn thiếu" in pg.inner_text(".modal .alert") and "Tên tòa nhà" in pg.inner_text(".modal .alert"), "form trống báo rõ trường còn thiếu")
            pg.goto(BASE + "/"); pg.wait_for_selector(".sb-area"); pg.click(".sb-area"); pg.wait_for_timeout(500)
            vis = pg.eval_on_selector(".loc-pop", "e=>{const b=e.getBoundingClientRect();return e.contains(document.elementFromPoint(b.left+20,b.bottom-10))}")
            check(vis, "khung chọn địa chỉ ở ô tìm kiếm hiện đủ, không bị cắt")
            pg.wait_for_selector("#areas a"); check(pg.locator("#areas a").count() >= 1, "mục gần học tập/làm việc liệt kê địa chỉ phòng đã đăng")
            pg.click(".lm-filter [data-t=school]"); pg.wait_for_timeout(500)
            check(pg.locator("#landmarks a").count() >= 1 and pg.locator("#landmarks a:has-text('🏢')").count() == 0, "lọc Trường học")
            ctx3 = b.new_context(geolocation={"latitude": 10.7769, "longitude": 106.7009}, permissions=["geolocation"]); pg3 = ctx3.new_page()
            pg3.goto(BASE + "/"); pg3.wait_for_selector("#lm-near"); pg3.click("#lm-near"); pg3.wait_for_selector("#landmarks a:has-text(' km')", timeout=8000)
            check(pg3.locator("#landmarks a:has-text(' km')").count() >= 1, "lọc theo vị trí thiết bị hiện khoảng cách km")
            ctx3.close()

            print("• Chọn địa chỉ Mới / Cũ ở ô tìm kiếm")
            pg.goto(BASE + "/search.html"); pg.wait_for_selector(".sb-area"); pg.click(".sb-area")
            pg.click(".loc-pop .seg-b:has-text('Mới')")
            n_new = pg.evaluate("async () => (await (await fetch('/api/locations/provinces?system=new')).json()).length")
            n_old = pg.evaluate("async () => (await (await fetch('/api/locations/provinces?system=old')).json()).length")
            check(n_new == 34 and n_old == 63, f"đủ 34 tỉnh mới và 63 tỉnh cũ ({n_new}/{n_old})")
            pg.click(".loc-pop .cb-input >> nth=0"); check(pg.locator(".loc-pop .cb-opt").count() == 34, "danh sách tỉnh hệ mới đủ 34")
            pg.click(".loc-pop .cb-opt:has-text('Đà Nẵng')"); pg.click(".loc-pop .cb-input >> nth=2")
            check(pg.locator(".loc-pop .cb-opt").count() > 20, "có danh sách phường/xã hệ mới của Đà Nẵng")
            pg.screenshot(path=f"{shots}/location_picker.png")
            check(not errs, "không có lỗi JS/console: " + "; ".join(errs[:3]))
            b.close()
    finally:
        proc.terminate(); srv.shutdown()
    print("\nTất cả đạt" if not fails else f"\n{len(fails)} lỗi")
    sys.exit(1 if fails else 0)
main()
