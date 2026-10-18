#!/usr/bin/env python3
"""Kiểm thử API: tự chạy server thật với database tạm rồi gọi thử toàn bộ chức năng.

Cách chạy (từ thư mục gốc dự án, sau khi đã build):
    python tests/api_test.py                      # tự tìm file chạy trong build/
    python tests/api_test.py build/nhatro         # chỉ định file chạy
Chỉ dùng thư viện chuẩn của Python, không cần cài thêm gì.
"""
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import urllib.error
import urllib.request
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import parse_qs, urlparse

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
PORT = int(os.environ.get("TEST_PORT", "18765"))
MOCK_PORT = PORT + 1
BASE = f"http://127.0.0.1:{PORT}"
CLIENT_ID = "test-client-id.apps.googleusercontent.com"

passed = failed = 0


def check(cond, label):
    global passed, failed
    if cond:
        passed += 1
    else:
        failed += 1
        print("  LỖI:", label)


def call(method, path, body=None, token=None, raw=False):
    data = None if body is None else json.dumps(body).encode()
    req = urllib.request.Request(BASE + path, data=data, method=method)
    if data is not None:
        req.add_header("Content-Type", "application/json")
    if token:
        req.add_header("Authorization", "Bearer " + token)
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            text = r.read().decode()
            return r.status, (text if raw else (json.loads(text) if text else None)), r.headers
    except urllib.error.HTTPError as e:
        text = e.read().decode()
        try:
            return e.code, json.loads(text), e.headers
        except ValueError:
            return e.code, text, e.headers


def api(method, path, body=None, token=None):
    s, j, _ = call(method, path, body, token)
    return s, j


# ---------------------------------------------------------------- Google giả (tokeninfo)
GOOGLE_TOKENS = {}


class MockGoogle(BaseHTTPRequestHandler):
    def do_GET(self):
        q = parse_qs(urlparse(self.path).query)
        tok = q.get("id_token", [""])[0]
        info = GOOGLE_TOKENS.get(tok)
        body = json.dumps(info if info else {"error_description": "Invalid Value"}).encode()
        self.send_response(200 if info else 400)
        self.send_header("Content-Type", "application/json")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, *a):
        pass


def start_mock():
    srv = HTTPServer(("127.0.0.1", MOCK_PORT), MockGoogle)
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    return srv


def find_binary():
    if len(sys.argv) > 1:
        return sys.argv[1]
    for c in ["build/nhatro", "build/nhatro.exe", "build/Release/nhatro.exe", "build/Debug/nhatro.exe"]:
        p = os.path.join(ROOT, c)
        if os.path.exists(p):
            return p
    sys.exit("Không tìm thấy file chạy. Hãy build trước (xem README) hoặc truyền đường dẫn: python tests/api_test.py <file>")


_TOKENS = {}


def login(username, password):
    if (username, password) in _TOKENS:
        return _TOKENS[(username, password)]
    for _ in range(5):
        s, j = api("POST", "/api/auth/login", {"username": username, "password": password})
        if s != 429:  # giới hạn 15 lần đăng nhập/phút: đợi rồi thử lại
            break
        time.sleep(15)
    assert s == 200, (s, j)
    _TOKENS[(username, password)] = j["token"]
    return j["token"]


_LOC = {}


def loc():
    """Tra mã tỉnh/quận/phường theo tên (mã tự tăng nên không đoán trước)."""
    if _LOC:
        return _LOC
    old = {p["name"]: p["provinceId"] for p in api("GET", "/api/locations/provinces?system=old")[1]}
    new = {p["name"]: p["provinceId"] for p in api("GET", "/api/locations/provinces?system=new")[1]}
    hcm = old["Hồ Chí Minh"]
    ds = {d["name"]: d["districtId"] for d in api("GET", f"/api/locations/districts?provinceId={hcm}")[1]}
    govap = ds["Quận Gò Vấp"]
    wards = {w["name"]: w["wardId"] for w in api("GET", f"/api/locations/wards?districtId={govap}")[1]}
    _LOC.update(hcm=hcm, hn=new["Hà Nội"], dn=new["Đà Nẵng"], govap=govap, q1=ds["Quận 1"], p12=wards["Phường 12"], old_hn=old["Hà Nội"],
                hn_wards={w["name"]: w["wardId"] for w in api("GET", f"/api/locations/wards?provinceId={new['Hà Nội']}")[1]})
    return _LOC


def items(path, token=None):
    s, j = api("GET", path, token=token)
    assert s == 200, (path, s, j)
    return j["items"]


# ================================================================ các nhóm kiểm thử
def test_basic():
    print("• Cơ bản / tệp tĩnh / bảo mật")
    s, j = api("GET", "/api/auth/config")
    check(s == 200 and j["googleEnabled"] is True and j["googleClientId"] == CLIENT_ID, "config Google")
    s, text, h = call("GET", "/", raw=True)
    check(s == 200 and ("<html" in text.lower() or "<h1" in text.lower()), "trang chủ được phục vụ")
    check("default-src 'self'" in (h.get("Content-Security-Policy") or ""), "có CSP")
    check(h.get("X-Content-Type-Options") == "nosniff", "nosniff")
    s, j = api("GET", "/api/khong-co")
    check(s == 404 and "error" in j, "API lạ trả JSON 404")
    s, j = api("POST", "/api/auth/login", None)
    check(s == 400, "thiếu body -> 400")
    s, _, _ = call("POST", "/api/auth/login", None)
    # JSON hỏng
    req = urllib.request.Request(BASE + "/api/auth/login", data=b"{hong", method="POST")
    try:
        urllib.request.urlopen(req)
        check(False, "JSON hỏng phải bị từ chối")
    except urllib.error.HTTPError as e:
        check(e.code == 400, "JSON hỏng -> 400")


def test_meta_home():
    print("• Danh mục / trang chủ / khuyến mãi")
    s, j = api("GET", "/api/meta")
    names = [t["typeName"] for t in j["roomTypes"]]
    check(names == ["Phòng trọ", "Ký túc xá", "Nhà", "Chung cư", "Văn phòng", "Phòng pass & ở ghép"], f"6 loại hình theo thứ tự ({names})")
    check("provinces" not in j and len(j["amenities"]) == 14, "meta không còn danh sách tỉnh (đã có /api/locations)")
    s, h = api("GET", "/api/home")
    check(s == 200, "home 200")
    check(len(h["promotions"]["programs"]) == 3, "3 chương trình")
    check(len(h["promotions"]["hotdeals"]) == 2, "2 hotdeal đang chạy")
    check(len(h["promotions"]["vouchers"]) == 1 and len(h["promotions"]["partners"]) == 1, "voucher + đối tác")
    deal = h["promotions"]["hotdeals"][0]
    check(deal["secondsLeft"] > 0 and len(deal["rooms"]) >= 1, "hotdeal có đồng hồ đếm ngược và phòng")
    check(all(r["dealPrice"] is not None and r["price"] == r["dealPrice"] for r in deal["rooms"]), "phòng hotdeal có giá ưu đãi")
    check(all(r["status"] == "available" for r in h["nowRooms"]) and len(h["nowRooms"]) >= 3, "phòng NOW")
    check([x["title"] for x in h["featured"]] == ["Ký túc xá nổi bật", "Nhà nổi bật", "Chung cư nổi bật", "Phòng trọ nổi bật"], "4 mục nổi bật đúng thứ tự")
    check(all(sec["rooms"][0]["featuredSource"] == "pinned" for sec in h["featured"]), "phòng Admin ghim đứng đầu mục nổi bật")
    check(len(h["popularAreas"]) >= 3 and h["popularAreas"][0]["roomCount"] >= h["popularAreas"][-1]["roomCount"], "khu vực nhiều phòng nhất")
    check(h["settings"]["hotline"] and h["settings"]["zalo"], "home trả hotline / zalo")
    check(h["stats"]["rooms"] == 26 and h["stats"]["buildings"] == 8, f"thống kê tính từ DB ({h['stats']})")
    s, p = api("GET", "/api/promotions")
    check(s == 200 and len(p["hotdeals"]) == 2, "/api/promotions")
    s, lm = api("GET", f"/api/landmarks?type=school&provinceId={loc()['hcm']}")
    check(s == 200 and len(lm) == 5 and all(x["landmarkType"] == "school" for x in lm), "địa điểm học tập")
    s, lm = api("GET", "/api/landmarks?lat=10.8&lng=106.7&radiusKm=15")
    check(s == 200 and lm and all("distanceKm" in x and x["distanceKm"] <= 15 for x in lm) and [x["distanceKm"] for x in lm] == sorted(x["distanceKm"] for x in lm), "địa điểm gần vị trí: có distanceKm, sắp gần→xa")
    s, lm2 = api("GET", "/api/landmarks?type=school&lat=10.8&lng=106.7&radiusKm=15")
    check(s == 200 and all(x["landmarkType"] == "school" for x in lm2), "lọc loại + vị trí")
    check(api("GET", "/api/landmarks?lat=10.8")[0] == 400, "thiếu lng -> 400")
    s, lm = api("GET", "/api/landmarks?q=bach%20khoa")
    check({x["name"] for x in lm} == {"Đại học Bách Khoa TP.HCM", "Đại học Bách Khoa Hà Nội", "Đại học Bách Khoa Đà Nẵng"}, "tìm địa điểm không dấu")
    s, sg = api("GET", "/api/search/suggest?q=go")
    check(any(a["level"] == "district" and "Gò Vấp" in a["label"] for a in sg["areas"]), "gợi ý khu vực 'go' -> Gò Vấp")
    sw = api("GET", "/api/search/suggest?q=cau%20giay")[1]
    check(any(a["level"] == "ward" and a["system"] == "NEW" and "Cầu Giấy" in a["label"] for a in sw["areas"]), "gợi ý phường hệ mới theo tên không dấu")
    s, sg = api("GET", "/api/search/suggest?q=chung")
    check(any(t["typeName"] == "Chung cư" for t in sg["roomTypes"]), "gợi ý loại hình")
    s, sg = api("GET", "/api/search/suggest")
    check(len(sg["roomTypes"]) == 6, "gợi ý mặc định có đủ 6 loại hình")


def test_search():
    print("• Tìm kiếm phòng")
    s, r = api("GET", "/api/rooms?size=100")
    check(s == 200 and r["totalItems"] == 21, f"người dùng chỉ thấy phòng còn chỗ: {r['totalItems']}")
    statuses = {x["status"] for x in r["items"]}
    check(statuses == {"available"}, "không lộ phòng đã kín / sửa chữa / ẩn")
    check(all(x["currentOccupants"] < x["maxOccupants"] for x in r["items"]), "không có phòng đã kín")
    check("contactPhone" not in r["items"][0], "danh sách không lộ số điện thoại")

    s, r = api("GET", "/api/rooms?keyword=g%C3%B2%20v%E1%BA%A5p")  # "gò vấp"
    check(r["totalItems"] >= 3 and all("Gò Vấp" in x["districtName"] for x in r["items"]), "tìm 'gò vấp' có dấu")
    s, r2 = api("GET", "/api/rooms?keyword=GO%20VAP")
    check(r2["totalItems"] == r["totalItems"], "tìm 'GO VAP' không dấu/viết hoa cho cùng kết quả")
    s, r = api("GET", "/api/rooms?typeId=6")
    check(r["totalItems"] == 2 and all(x["typeSlug"] == "pass-o-ghep" for x in r["items"]), "lọc Phòng pass & ở ghép")
    s, r = api("GET", "/api/rooms?type=ky-tuc-xa")
    check(r["totalItems"] == 2, "lọc theo slug ký túc xá")
    L = loc()
    s, r = api("GET", f"/api/rooms?provinceId={L['hn']}")
    check(r["totalItems"] == 3 and all(x["provinceName"] == "Hà Nội" for x in r["items"]), "lọc theo tỉnh (hệ mới)")
    s, r2 = api("GET", f"/api/rooms?provinceId={L['old_hn']}")
    check(r2["totalItems"] == 3, "lọc theo tỉnh hệ cũ cùng tên cho kết quả tương đương")
    s, r = api("GET", f"/api/rooms?districtId={L['govap']}&minPrice=2000000&maxPrice=3500000")
    check(r["totalItems"] >= 1 and all(2000000 <= x["price"] <= 3500000 for x in r["items"]), "lọc khu vực + khoảng giá")
    s, r = api("GET", "/api/rooms?minArea=30&maxArea=80")
    check(r["totalItems"] >= 3 and all(30 <= x["areaM2"] <= 80 for x in r["items"]), "lọc diện tích")
    s, r = api("GET", f"/api/rooms?wardId={L['p12']}")
    check(r["totalItems"] >= 1 and all(x["wardName"] == "Phường 12" for x in r["items"]), "lọc phường/xã (hệ cũ)")
    s, r = api("GET", f"/api/rooms?wardId={L['hn_wards']['Phường Cầu Giấy']}")
    check(r["totalItems"] == 3 and all(x["admSystem"] == "NEW" for x in r["items"]), "lọc phường/xã (hệ mới, không có quận)")

    # sắp xếp + phân trang
    s, r = api("GET", "/api/rooms?sort=price_asc&size=100")
    prices = [x["price"] for x in r["items"]]
    check(prices == sorted(prices), "sắp xếp giá tăng dần")
    s, r = api("GET", "/api/rooms?sort=price_desc&size=100")
    prices = [x["price"] for x in r["items"]]
    check(prices == sorted(prices, reverse=True), "sắp xếp giá giảm dần")
    s, r = api("GET", "/api/rooms?sort=name&size=100")
    check(s == 200, "sắp xếp theo tên")
    s, p1 = api("GET", "/api/rooms?size=7&page=1&sort=price_asc")
    s, p3 = api("GET", "/api/rooms?size=7&page=3&sort=price_asc")
    check(p1["totalPages"] == 3 and len(p1["items"]) == 7 and len(p3["items"]) == 7, "phân trang (21 phòng / 7 = 3 trang)")
    ids = {x["roomId"] for x in p1["items"]} & {x["roomId"] for x in p3["items"]}
    check(not ids, "các trang không trùng nhau")
    s, r = api("GET", "/api/rooms?sort=hack;drop")
    check(s == 400, "sort lạ bị từ chối")
    s, r = api("GET", "/api/rooms?minPrice=5000000&maxPrice=1000000")
    check(s == 400, "minPrice > maxPrice -> 400")
    s, r = api("GET", "/api/rooms?minPrice=abc")
    check(s == 400, "giá không phải số -> 400")
    s, r = api("GET", "/api/rooms?size=100000")
    check(s == 200 and r["size"] == 100, "giới hạn size tối đa")

    # cờ
    s, r = api("GET", "/api/rooms?isNow=true&size=100")
    check(r["totalItems"] >= 3 and all(x["isNow"] for x in r["items"]), "lọc NOW")
    s, r = api("GET", "/api/rooms?isNew=true&isHot=true")
    check(all(x["isNew"] and x["isHot"] for x in r["items"]), "lọc mới + hot")
    s, r = api("GET", "/api/rooms?hotdeal=true")
    check(r["totalItems"] == 6 and all(x["discountPercent"] > 0 and x["originalPrice"] > x["price"] for x in r["items"]), "lọc hotdeal có % giảm")
    s, r = api("GET", "/api/rooms?amenityIds=9")  # thang máy
    check(r["totalItems"] >= 3, "lọc theo tiện nghi")
    s, r = api("GET", "/api/rooms?amenityIds=9,14")  # thang máy + thú cưng (không phòng nào có)
    check(r["totalItems"] == 0, "nhiều tiện nghi phải có đủ tất cả")

    # gần nơi học tập / vị trí của tôi
    s, r = api("GET", "/api/rooms?landmarkId=3&radiusKm=3")  # ĐH Công nghiệp, Gò Vấp
    check(r["totalItems"] >= 3 and "distanceKm" in r["items"][0], "gần ĐH Công nghiệp")
    d = [x["distanceKm"] for x in r["items"]]
    check(d == sorted(d) and max(d) <= 3, "mặc định sắp theo khoảng cách và trong bán kính")
    s, r = api("GET", "/api/rooms?lat=10.8222&lng=106.6866&radiusKm=0.5")
    check(r["totalItems"] == 0, "bán kính nhỏ không có phòng")
    s, r = api("GET", "/api/rooms?lat=10.8")
    check(s == 400, "thiếu lng -> 400")
    s, r = api("GET", "/api/rooms?lat=99&lng=0")
    check(s == 400, "toạ độ sai -> 400")
    s, r = api("GET", "/api/rooms?sort=distance")
    check(s == 400, "sort=distance cần toạ độ")

    # injection
    s, r = api("GET", "/api/rooms?keyword=%27%3B%20DROP%20TABLE%20rooms%3B--")
    check(s == 200 and r["totalItems"] == 0, "từ khóa độc hại không phá được truy vấn")
    s, r = api("GET", "/api/rooms?keyword=100%25")
    check(s == 200 and r["totalItems"] == 0, "ký tự % trong từ khóa được thoát")
    check(api("GET", "/api/rooms?size=1")[1]["totalItems"] == 21, "dữ liệu còn nguyên sau khi thử injection")

    # chi tiết
    s, rooms = api("GET", "/api/rooms?keyword=P101")
    rid = rooms["items"][0]["roomId"]
    s, d = api("GET", f"/api/rooms/{rid}")
    check(s == 200 and d["roomNumber"] == "P101" and d["description"], "chi tiết phòng")
    check(len(d["amenities"]) >= 3 and "nearbyLandmarks" in d and len(d["similarRooms"]) >= 1, "chi tiết có tiện nghi / địa điểm gần / phòng tương tự")
    check("contactPhone" not in d, "chi tiết không lộ SĐT")
    s, c = api("GET", f"/api/rooms/{rid}/contact")
    check(s == 200 and c["contactPhone"] == "0900000002", "bấm 'Xem SĐT'")
    hid = api("GET", "/api/rooms?admin=1&status=hidden", token=ADMIN)[1]["items"][0]["roomId"]
    check(api("GET", f"/api/rooms/{hid}")[0] == 404, "phòng ẩn: người dùng không xem được")
    check(api("GET", f"/api/rooms/{hid}/contact")[0] == 404, "phòng ẩn: không lấy được SĐT")
    check(api("GET", f"/api/rooms/{hid}", token=ADMIN)[0] == 200, "phòng ẩn: admin xem được")
    check(api("GET", "/api/rooms/999999")[0] == 404, "phòng không tồn tại -> 404")
    check(api("GET", "/api/rooms/abc")[0] == 400, "mã phòng không phải số -> 400")
    other = api("GET", "/api/rooms?keyword=K101")[1]["items"][0]["roomId"]
    v1 = api("GET", f"/api/rooms/{other}", token=ADMIN)[1]["viewCount"]
    api("GET", f"/api/rooms/{other}")
    v2 = api("GET", f"/api/rooms/{other}", token=ADMIN)[1]["viewCount"]
    check(v2 == v1 + 1, "lượt xem tăng khi người dùng xem (admin xem không tính)")
    api("GET", f"/api/rooms/{other}")
    v3 = api("GET", f"/api/rooms/{other}", token=ADMIN)[1]["viewCount"]
    check(v3 == v2, "cùng một người xem lại trong 30 phút không tính thêm lượt")


def test_auth():
    print("• Đăng ký / đăng nhập")
    s, j = api("POST", "/api/auth/register", {"username": "ab", "password": "Abcdef12", "fullName": "A"})
    check(s == 400, "tên đăng nhập quá ngắn")
    s, j = api("POST", "/api/auth/register", {"username": "nguoidung1", "password": "12345678", "fullName": "Nguyễn Văn A"})
    check(s == 400, "mật khẩu chỉ có số bị từ chối")
    s, j = api("POST", "/api/auth/register", {"username": "nguoidung1", "password": "short1", "fullName": "Nguyễn Văn A"})
    check(s == 400, "mật khẩu quá ngắn bị từ chối")
    s, j = api("POST", "/api/auth/register", {"username": "nguoidung1", "password": "Matkhau123", "fullName": "Nguyễn Văn A", "email": "a@@x"})
    check(s == 400, "email sai")
    s, j = api("POST", "/api/auth/register", {"username": "nguoidung1", "password": "Matkhau123", "fullName": "Nguyễn Văn A", "email": "nva@example.com", "phone": "0911000111"})
    check(s == 201 and j["token"] and j["user"]["role"] == "TENANT" and j["user"]["username"] == "nguoidung1", "đăng ký thành công")
    check("passwordHash" not in json.dumps(j), "không trả về hash mật khẩu")
    s, j2 = api("POST", "/api/auth/register", {"username": "NguoiDung1", "password": "Matkhau123", "fullName": "Trùng tên"})
    check(s == 409, "trùng tên đăng nhập (không phân biệt hoa thường)")
    s, j2 = api("POST", "/api/auth/register", {"username": "nguoidung2", "password": "Matkhau123", "fullName": "Trùng email", "email": "NVA@example.com"})
    check(s == 409, "trùng email")
    # không thể tự đăng ký làm admin
    s, j2 = api("POST", "/api/auth/register", {"username": "hacker", "password": "Matkhau123", "fullName": "H", "roleId": 1, "role": "ADMIN"})
    check(s == 201 and j2["user"]["role"] == "TENANT", "đăng ký không thể tự đặt vai trò ADMIN")

    s, j = api("POST", "/api/auth/login", {"username": "nguoidung1", "password": "sai-mat-khau-1"})
    check(s == 401, "sai mật khẩu")
    s, j = api("POST", "/api/auth/login", {"username": "khong-ton-tai", "password": "Matkhau123"})
    check(s == 401 and j["error"] == "Sai tên đăng nhập hoặc mật khẩu", "tài khoản không tồn tại: cùng thông báo")
    s, j = api("POST", "/api/auth/login", {"username": "NVA@example.com", "password": "Matkhau123"})
    check(s == 200 and j["user"]["username"] == "nguoidung1", "đăng nhập bằng email")
    token = j["token"]
    s, me = api("GET", "/api/auth/me", token=token)
    check(s == 200 and me["email"] == "nva@example.com" and me["hasPassword"] is True, "xem hồ sơ")
    check(api("GET", "/api/auth/me")[0] == 401, "chưa đăng nhập -> 401")
    check(api("GET", "/api/auth/me", token="abc.def.ghi")[0] == 401, "token sai -> 401")
    check(api("GET", "/api/auth/me", token=token[:-3] + "xxx")[0] == 401, "token bị sửa -> 401")
    s, me = api("PUT", "/api/auth/me", {"fullName": "Nguyễn Văn An", "phone": "0911000222"}, token)
    check(s == 200 and me["fullName"] == "Nguyễn Văn An", "cập nhật hồ sơ")
    s, j = api("POST", "/api/auth/password", {"currentPassword": "sai", "newPassword": "Matkhau456"}, token)
    check(s == 400, "đổi mật khẩu: sai mật khẩu hiện tại")
    s, j = api("POST", "/api/auth/password", {"currentPassword": "Matkhau123", "newPassword": "Matkhau456"}, token)
    check(s == 200, "đổi mật khẩu")
    check(api("POST", "/api/auth/login", {"username": "nguoidung1", "password": "Matkhau456"})[0] == 200, "đăng nhập bằng mật khẩu mới")
    check(api("POST", "/api/auth/login", {"username": "nguoidung1", "password": "Matkhau123"})[0] == 401, "mật khẩu cũ không còn dùng được")

    # quyền: người thuê không dùng được chức năng quản trị
    s, j = api("POST", "/api/rooms", {"buildingId": 1, "typeId": 1, "roomNumber": "X1", "title": "x", "baseRentPrice": 1}, token)
    check(s == 403, "TENANT không thêm được phòng")
    check(api("GET", "/api/admin/dashboard", token=token)[0] == 403, "TENANT không xem được dashboard")
    check(api("GET", "/api/admin/tenants", token=token)[0] == 403, "TENANT không xem được danh sách người thuê")
    check(api("GET", "/api/rooms?admin=1", token=token)[0] == 403, "TENANT không dùng được chế độ admin")
    check(api("GET", "/api/admin/dashboard")[0] == 401, "chưa đăng nhập không xem được dashboard")
    return token


def test_google():
    print("• Đăng nhập Google")
    now = int(time.time())
    good = {"sub": "google-sub-123", "email": "gg.user@gmail.com", "email_verified": "true", "name": "Google User",
            "picture": "https://example.com/a.png", "aud": CLIENT_ID, "iss": "https://accounts.google.com", "exp": str(now + 3000)}
    GOOGLE_TOKENS["good.token.value"] = good
    s, j = api("POST", "/api/auth/google", {"credential": "good.token.value"})
    check(s == 200 and j["user"]["email"] == "gg.user@gmail.com" and j["user"]["role"] == "TENANT", f"Google: tạo tài khoản mới ({s} {j})")
    check(j["user"]["hasPassword"] is False and j["user"]["providers"] == ["google"], "tài khoản Google chưa có mật khẩu")
    uid = j["user"]["id"]
    s, j = api("POST", "/api/auth/google", {"credential": "good.token.value"})
    check(s == 200 and j["user"]["id"] == uid, "Google: lần sau đăng nhập đúng tài khoản cũ")
    tok = j["token"]
    # đặt mật khẩu lần đầu để đăng nhập thường
    s, j = api("POST", "/api/auth/password", {"newPassword": "Matkhau789"}, tok)
    check(s == 200, "tài khoản Google đặt mật khẩu lần đầu không cần mật khẩu cũ")
    check(api("POST", "/api/auth/login", {"username": "gg.user@gmail.com", "password": "Matkhau789"})[0] == 200, "sau đó đăng nhập thường được")

    # liên kết vào tài khoản đã có cùng email
    GOOGLE_TOKENS["link.token"] = dict(good, sub="google-sub-999", email="minhanh@gmail.com", name="Minh Anh G")
    s, j = api("POST", "/api/auth/google", {"credential": "link.token"})
    check(s == 200 and j["user"]["username"] == "minhanh" and "google" in j["user"]["providers"], "Google: gắn vào tài khoản có sẵn cùng email")

    # các trường hợp bị từ chối
    GOOGLE_TOKENS["bad.aud"] = dict(good, sub="s-aud", email="x1@gmail.com", aud="client-khac")
    check(api("POST", "/api/auth/google", {"credential": "bad.aud"})[0] == 401, "sai audience (token của ứng dụng khác)")
    GOOGLE_TOKENS["bad.iss"] = dict(good, sub="s-iss", email="x2@gmail.com", iss="https://evil.example.com")
    check(api("POST", "/api/auth/google", {"credential": "bad.iss"})[0] == 401, "sai issuer")
    GOOGLE_TOKENS["expired.tok"] = dict(good, sub="s-exp", email="x3@gmail.com", exp=str(now - 10))
    check(api("POST", "/api/auth/google", {"credential": "expired.tok"})[0] == 401, "token hết hạn")
    GOOGLE_TOKENS["unverified.t"] = dict(good, sub="s-unv", email="x4@gmail.com", email_verified="false")
    check(api("POST", "/api/auth/google", {"credential": "unverified.t"})[0] == 401, "email chưa xác minh")
    check(api("POST", "/api/auth/google", {"credential": "khong.hop.le"})[0] == 401, "token Google không xác nhận")
    check(api("POST", "/api/auth/google", {"credential": 'x"; echo hacked; "'})[0] == 400, "ký tự lạ bị chặn (không chèn được lệnh)")
    check(api("POST", "/api/auth/google", {"credential": "a b"})[0] == 400, "khoảng trắng bị chặn")
    check(api("POST", "/api/auth/google", {})[0] == 400, "thiếu credential")
    check(api("GET", "/api/rooms?size=1")[0] == 200, "server vẫn bình thường sau các lần thử")


def test_rooms_admin():
    print("• Quản lý phòng (CRUD)")
    mgr = login("levinh", "Manager@123")
    base = {"buildingId": 1, "typeId": 1, "roomNumber": "T901", "title": "Phòng thử nghiệm", "baseRentPrice": 2750000, "areaM2": 21.5,
            "maxOccupants": 3, "currentOccupants": 1, "isNew": True, "amenityIds": [1, 2], "imageUrls": ["/img/a.jpg", "https://example.com/b.jpg"]}
    total_before = api("GET", "/api/admin/buildings/1", token=ADMIN)[1]["totalRooms"]
    s, j = api("POST", "/api/rooms", dict(base, currentOccupants=5), ADMIN)
    check(s == 400 and "vượt quá sức chứa" in j["error"], "số người > sức chứa")
    s, j = api("POST", "/api/rooms", dict(base, baseRentPrice=-1), ADMIN)
    check(s == 400, "giá âm")
    s, j = api("POST", "/api/rooms", dict(base, buildingId=999), ADMIN)
    check(s == 400, "tòa nhà không tồn tại")
    s, j = api("POST", "/api/rooms", dict(base, status="abc"), ADMIN)
    check(s == 400, "trạng thái sai")
    s, j = api("POST", "/api/rooms", dict(base, roomNumber="a b"), ADMIN)
    check(s == 400, "mã phòng có ký tự lạ")
    s, j = api("POST", "/api/rooms", dict(base, thumbnailUrl="javascript:alert(1)"), ADMIN)
    check(s == 400, "ảnh đại diện kiểu javascript: bị chặn")
    s, j = api("POST", "/api/rooms", {k: v for k, v in base.items() if k != "title"}, ADMIN)
    check(s == 400, "thiếu tiêu đề")
    s, j = api("POST", "/api/rooms", dict(base, roomNumber="P101"), ADMIN)
    check(s == 409, "trùng mã phòng trong cùng tòa nhà")
    s, room = api("POST", "/api/rooms", base, mgr)
    check(s == 201 and room["roomNumber"] == "T901" and room["status"] == "available", "tạo phòng (MANAGER)")
    rid = room["roomId"]
    check(room["createBy"] == 2 and room["updateBy"] == 2, "trường audit create_by/update_by lấy từ người đăng nhập")
    check(len(room["amenities"]) == 2 and len(room["images"]) == 2, "lưu tiện nghi + ảnh")
    check(api("GET", "/api/admin/buildings/1", token=ADMIN)[1]["totalRooms"] == total_before + 1, "tổng số phòng của tòa nhà tự tăng")
    s, pub = api("GET", "/api/rooms?keyword=T901")
    check(pub["totalItems"] == 1, "phòng mới hiện ở trang người dùng")

    s, j = api("PUT", f"/api/rooms/{rid}", {"roomNumber": "KHAC"}, ADMIN)
    check(s == 400, "không sửa được mã phòng")
    s, j = api("PUT", f"/api/rooms/{rid}", {"currentOccupants": 9}, ADMIN)
    check(s == 400, "sửa số người vượt sức chứa")
    s, j = api("PUT", f"/api/rooms/{rid}", {"title": "Phòng đã sửa", "baseRentPrice": 3000000, "currentOccupants": 3, "isHot": True, "amenityIds": [1]}, ADMIN)
    check(s == 200 and j["title"] == "Phòng đã sửa" and j["baseRentPrice"] == 3000000 and j["isHot"] is True, "sửa phòng")
    check(j["areaM2"] == 21.5 and j["isNew"] is True and len(j["amenities"]) == 1 and len(j["images"]) == 2, "trường không gửi được giữ nguyên")
    check(j["occupancyState"] == "full", "3/3 người -> đã kín")
    s, pub = api("GET", "/api/rooms?keyword=T901")
    check(pub["totalItems"] == 0, "phòng đã kín không còn ở trang người dùng")
    s, adm = api("GET", "/api/rooms?admin=1&keyword=T901&occupancy=full", token=ADMIN)
    check(adm["totalItems"] == 1, "admin lọc 'đã kín'")

    # xóa mềm / khôi phục
    s, j = api("DELETE", f"/api/rooms/{rid}?type=3", token=ADMIN)
    check(s == 400, "type không hợp lệ")
    s, j = api("DELETE", f"/api/rooms/{rid}", token=mgr)
    check(s == 200 and "Đã ẩn" in j["message"], "xóa mềm mặc định (type=2)")
    check(api("GET", f"/api/rooms/{rid}")[0] == 404, "phòng ẩn không xem được")
    check(api("GET", "/api/admin/buildings/1", token=ADMIN)[1]["totalRooms"] == total_before, "tổng số phòng giảm khi ẩn")
    check(api("GET", "/api/rooms?admin=1&keyword=T901", token=ADMIN)[1]["totalItems"] == 0, "admin mặc định không thấy phòng ẩn")
    check(api("GET", "/api/rooms?admin=1&keyword=T901&status=hidden", token=ADMIN)[1]["totalItems"] == 1, "admin lọc status=hidden thấy phòng ẩn")
    s, j = api("POST", f"/api/rooms/{rid}/restore", token=mgr)
    check(s == 200 and j["status"] == "occupied", "khôi phục (3/3 người -> occupied)")
    check(api("POST", f"/api/rooms/{rid}/restore", token=mgr)[0] == 400, "khôi phục phòng không bị ẩn -> 400")

    # xóa vĩnh viễn
    check(api("DELETE", f"/api/rooms/{rid}?type=1", token=mgr)[0] == 403, "MANAGER không xóa vĩnh viễn được")
    s, j = api("DELETE", f"/api/rooms/{rid}?type=1", token=ADMIN)
    check(s == 200 and "vĩnh viễn" in j["message"], "ADMIN xóa vĩnh viễn")
    check(api("GET", f"/api/rooms/{rid}", token=ADMIN)[0] == 404, "phòng đã xóa hẳn")
    check(api("GET", "/api/admin/buildings/1", token=ADMIN)[1]["totalRooms"] == total_before, "tổng số phòng đúng sau khi xóa")
    p102 = api("GET", "/api/rooms?admin=1&keyword=P102", token=ADMIN)[1]["items"][0]["roomId"]
    s, j = api("DELETE", f"/api/rooms/{p102}?type=1", token=ADMIN)
    check(s == 409 and "ẩn phòng" in j["error"], "phòng có hợp đồng không xóa vĩnh viễn được")
    check(api("DELETE", "/api/rooms/999999", token=ADMIN)[0] == 404, "xóa phòng không tồn tại")

    # bộ lọc admin theo số người ở (dữ liệu mẫu)
    s, e = api("GET", "/api/rooms?admin=1&occupancy=empty&size=100", token=ADMIN)
    check(all(x["currentOccupants"] == 0 for x in e["items"]) and e["totalItems"] >= 8, "lọc 'ế' (0 người)")
    s, a = api("GET", "/api/rooms?admin=1&occupancy=almost_empty&size=100", token=ADMIN)
    check(a["totalItems"] >= 3 and all(0 < x["currentOccupants"] and x["currentOccupants"] * 100 <= x["maxOccupants"] * 50 for x in a["items"]), "lọc 'sắp trống'")
    s, f = api("GET", "/api/rooms?admin=1&occupancy=full&size=100", token=ADMIN)
    check(f["totalItems"] >= 4 and all(x["currentOccupants"] >= x["maxOccupants"] for x in f["items"]), "lọc 'đã kín'")
    check(api("GET", "/api/rooms?admin=1&occupancy=xyz", token=ADMIN)[0] == 400, "occupancy sai -> 400")
    s, m = api("GET", "/api/rooms?admin=1&status=maintenance", token=ADMIN)
    check(m["totalItems"] == 1, "lọc phòng đang sửa chữa")
    s, srt = api("GET", "/api/rooms?admin=1&sort=roomNumber&size=100", token=ADMIN)
    nums = [x["roomNumber"] for x in srt["items"]]
    check(nums == sorted(nums), "admin sắp theo mã phòng")


def test_admin_crud():
    print("• Admin: các bảng trong ERD")
    # tenants + vehicles
    s, t = api("POST", "/api/admin/tenants", {"fullName": "Phạm Thị Thử", "phone": "0977111222", "email": "thu@example.com"}, ADMIN)
    check(s == 201 and t["fullName"] == "Phạm Thị Thử", "tạo người thuê")
    tid = t["tenantId"]
    check(api("POST", "/api/admin/tenants", {"fullName": "X", "phone": "123"}, ADMIN)[0] == 400, "SĐT sai")
    check(api("POST", "/api/admin/tenants", {"phone": "0977111222"}, ADMIN)[0] == 400, "thiếu họ tên")
    s, v = api("POST", "/api/admin/vehicles", {"tenantId": tid, "licensePlate": "59X1-999.99", "vehicleType": "motorbike"}, ADMIN)
    check(s == 201 and v["tenantName"] == "Phạm Thị Thử", "thêm xe")
    check(api("POST", "/api/admin/vehicles", {"tenantId": tid, "licensePlate": "59X1-999.99"}, ADMIN)[0] == 409, "trùng biển số")
    check(api("POST", "/api/admin/vehicles", {"tenantId": tid, "licensePlate": "A", "vehicleType": "boat"}, ADMIN)[0] == 400, "loại xe sai")
    check(api("POST", "/api/admin/vehicles", {"tenantId": 9999, "licensePlate": "ZZ1"}, ADMIN)[0] == 409, "người thuê không tồn tại -> lỗi khóa ngoại")
    s, lst = api("GET", "/api/admin/tenants?q=pham%20thi", token=ADMIN)
    check(s == 200 and lst["totalItems"] == 1, "tìm người thuê không dấu")
    s, one = api("GET", f"/api/admin/tenants/{tid}", token=ADMIN)
    check(one["vehicleCount"] == 1, "đếm số xe")

    # contracts
    room_id = api("GET", "/api/rooms?admin=1&keyword=K103", token=ADMIN)[1]["items"][0]["roomId"]
    check(api("POST", "/api/admin/contracts", {"roomId": room_id, "tenantId": tid, "startDate": "2026-12-01", "endDate": "2026-11-01"}, ADMIN)[0] == 400, "ngày kết thúc trước ngày bắt đầu")
    check(api("POST", "/api/admin/contracts", {"roomId": room_id, "tenantId": tid, "startDate": "01/12/2026", "endDate": "2027-01-01"}, ADMIN)[0] == 400, "ngày sai định dạng")
    s, c = api("POST", "/api/admin/contracts", {"roomId": room_id, "tenantId": tid, "startDate": "2026-10-01", "endDate": "2027-04-01", "depositAmount": 1100000}, ADMIN)
    check(s == 201 and c["monthlyRent"] == 1100000 and c["status"] == "active" and c["roomNumber"] == "K103", "tạo hợp đồng (giá thuê lấy mặc định từ phòng)")
    cid = c["contractId"]
    s, c2 = api("PUT", f"/api/admin/contracts/{cid}", {"note": "Gia hạn", "stayType": "short_term"}, ADMIN)
    check(s == 200 and c2["note"] == "Gia hạn" and c2["stayType"] == "short_term", "sửa hợp đồng")
    check(api("PUT", f"/api/admin/contracts/{cid}", {"endDate": "2020-01-01"}, ADMIN)[0] == 400, "sửa ngày kết thúc sai")
    check(api("PUT", f"/api/admin/contracts/{cid}", {}, ADMIN)[0] == 400, "không có trường để sửa")
    check(api("DELETE", f"/api/admin/tenants/{tid}", token=ADMIN)[0] == 409, "không xóa người thuê đang có hợp đồng")

    # tickets
    s, tk = api("POST", "/api/admin/tickets", {"roomId": room_id, "tenantId": tid, "category": "maintenance", "title": "Hỏng vòi nước"}, ADMIN)
    check(s == 201 and tk["status"] == "open" and tk["resolvedAt"] is None, "tạo sự cố")
    s, tk = api("PUT", f"/api/admin/tickets/{tk['ticketId']}", {"status": "done"}, ADMIN)
    check(tk["status"] == "done" and tk["resolvedAt"], "đóng sự cố tự ghi thời gian hoàn thành")
    check(api("PUT", f"/api/admin/tickets/{tk['ticketId']}", {"status": "open"}, ADMIN)[1]["resolvedAt"] is None, "mở lại sự cố xóa thời gian hoàn thành")
    check(api("POST", "/api/admin/tickets", {"roomId": room_id, "tenantId": tid, "category": "xyz", "title": "t"}, ADMIN)[0] == 400, "loại sự cố sai")

    # meter readings + invoice generation
    s, m = api("POST", "/api/admin/meter-readings", {"roomId": room_id, "billingMonth": "2026-10", "newElectric": 130, "newWater": 4}, ADMIN)
    check(s == 201 and m["oldElectric"] == 0 and m["newElectric"] == 130, "chốt điện nước tháng đầu (số cũ = 0)")
    s, m2 = api("POST", "/api/admin/meter-readings", {"roomId": room_id, "billingMonth": "2026-11", "newElectric": 200, "newWater": 9}, ADMIN)
    check(m2["oldElectric"] == 130 and m2["oldWater"] == 4, "tháng sau tự lấy số mới của tháng trước làm số cũ")
    check(api("POST", "/api/admin/meter-readings", {"roomId": room_id, "billingMonth": "2026-11", "newElectric": 300}, ADMIN)[0] == 409, "trùng phòng + tháng")
    check(api("POST", "/api/admin/meter-readings", {"roomId": room_id, "billingMonth": "2026-12", "oldElectric": 50, "newElectric": 10}, ADMIN)[0] == 400, "số điện mới < số cũ")
    check(api("POST", "/api/admin/meter-readings", {"roomId": room_id, "billingMonth": "2026-13", "newElectric": 1}, ADMIN)[0] == 400, "tháng sai")

    s, g = api("POST", "/api/admin/invoices/generate", {"contractId": cid, "billingMonth": "2026-10"}, ADMIN)
    check(s == 201, f"tạo hóa đơn tự động ({s} {g})")
    s, inv = api("GET", f"/api/admin/invoices/{g['invoiceId']}", token=ADMIN)
    # thuê 1.100.000 + điện 130*3500=455.000 + nước 4*20000=80.000 + internet 100.000 + dọn dẹp 150.000 + gửi xe 1*100.000
    check(inv["totalAmount"] == 1100000 + 455000 + 80000 + 100000 + 150000 + 100000, f"tổng tiền hóa đơn đúng ({inv['totalAmount']})")
    check(len(inv["details"]) == 6 and inv["status"] == "unpaid" and inv["dueDate"] == "2026-11-05", "6 dòng chi tiết, hạn thanh toán mặc định ngày 5 tháng sau")
    check(inv["invoiceCode"] == f"INV-202610-{cid:04d}", "mã hóa đơn")
    check(api("POST", "/api/admin/invoices/generate", {"contractId": cid, "billingMonth": "2026-10"}, ADMIN)[0] == 409, "không tạo trùng hóa đơn")
    s, g2 = api("POST", "/api/admin/invoices/generate", {"contractId": cid, "billingMonth": "2026-09"}, ADMIN)
    check(s == 201 and len(g2["warnings"]) == 1, "thiếu chỉ số điện nước: vẫn tạo nhưng có cảnh báo")
    check(api("POST", "/api/admin/invoices/generate", {"contractId": 9999, "billingMonth": "2026-10"}, ADMIN)[0] == 404, "hợp đồng không tồn tại")
    check(api("POST", "/api/admin/invoices/generate", {"contractId": cid, "billingMonth": "10/2026"}, ADMIN)[0] == 400, "tháng sai định dạng")

    # payments
    iid = inv["invoiceId"]
    total = inv["totalAmount"]
    check(api("POST", "/api/admin/payments", {"invoiceId": iid, "amountPaid": 0}, ADMIN)[0] == 400, "thanh toán 0 đồng")
    check(api("POST", "/api/admin/payments", {"invoiceId": iid, "amountPaid": total + 1}, ADMIN)[0] == 400, "thanh toán vượt số còn nợ")
    s, p1 = api("POST", "/api/admin/payments", {"invoiceId": iid, "amountPaid": 1000000, "method": "momo"}, ADMIN)
    check(s == 201 and p1["method"] == "momo" and p1["paidAt"], "ghi nhận thanh toán một phần")
    s, inv = api("GET", f"/api/admin/invoices/{iid}", token=ADMIN)
    check(inv["status"] == "partial" and inv["remaining"] == total - 1000000, "hóa đơn chuyển 'thanh toán một phần'")
    # tra cứu hóa đơn công khai (SĐT + số phòng)
    s, lk = api("GET", "/api/invoices/lookup?phone=0977111222&room=k103")
    check(s == 200 and lk["invoices"][0]["remaining"] == total - 1000000 and lk["invoices"][0]["tenantName"].startswith("Phạm") and "*" in lk["invoices"][0]["tenantName"], "tra cứu hóa đơn, tên được che bớt")
    check(len(lk["invoices"][0]["details"]) == 6 and len(lk["invoices"][0]["payments"]) == 1, "tra cứu có chi tiết + lịch sử thanh toán")
    s, lk2 = api("GET", "/api/invoices/lookup?phone=%2B84977111222&room=K103&month=2026-10")
    check(s == 200 and len(lk2["invoices"]) == 1, "tra cứu với số +84 và lọc theo tháng")
    check(api("GET", "/api/invoices/lookup?phone=0977111222&room=SAI")[0] == 404, "sai số phòng -> 404")
    check(api("GET", "/api/invoices/lookup?phone=0900000000&room=K103")[0] == 404, "sai số điện thoại -> 404")
    check(api("GET", "/api/invoices/lookup?phone=0977111222")[0] == 400, "thiếu số phòng")
    check("invoiceId" not in json.dumps(lk), "tra cứu công khai không lộ id nội bộ")
    s, p2 = api("POST", "/api/admin/payments", {"invoiceId": iid, "amountPaid": total - 1000000}, ADMIN)
    check(s == 201 and api("GET", f"/api/admin/invoices/{iid}", token=ADMIN)[1]["status"] == "paid", "thanh toán đủ -> 'đã thanh toán'")
    check(api("DELETE", f"/api/admin/payments/{p2['paymentId']}", token=ADMIN)[0] == 200 and
          api("GET", f"/api/admin/invoices/{iid}", token=ADMIN)[1]["status"] == "partial", "xóa khoản thanh toán -> quay lại 'một phần'")
    check(api("POST", f"/api/admin/invoices/{iid}/cancel", token=ADMIN)[0] == 409, "hóa đơn đã có thanh toán không hủy được")
    # chi tiết hóa đơn: thêm / sửa / xóa làm tổng tiền đổi theo
    s, d = api("POST", "/api/admin/invoice-details", {"invoiceId": iid, "serviceId": 5, "description": "Phụ thu", "quantity": 2, "unitPrice": 50000}, ADMIN)
    check(s == 201 and d["subtotal"] == 100000, "thêm dòng chi tiết (subtotal = số lượng x đơn giá)")
    check(api("GET", f"/api/admin/invoices/{iid}", token=ADMIN)[1]["totalAmount"] == total + 100000, "tổng hóa đơn tự cập nhật")
    check(api("DELETE", f"/api/admin/invoice-details/{d['detailId']}", token=ADMIN)[0] == 200 and
          api("GET", f"/api/admin/invoices/{iid}", token=ADMIN)[1]["totalAmount"] == total, "xóa dòng chi tiết -> tổng quay về cũ")
    # hóa đơn tháng 9 chưa có thanh toán: hủy được
    s, j = api("POST", f"/api/admin/invoices/{g2['invoiceId']}/cancel", token=ADMIN)
    check(s == 200 and api("GET", f"/api/admin/invoices/{g2['invoiceId']}", token=ADMIN)[1]["status"] == "cancelled", "hủy hóa đơn")
    check(api("POST", "/api/admin/payments", {"invoiceId": g2["invoiceId"], "amountPaid": 10}, ADMIN)[0] == 400, "không thanh toán cho hóa đơn đã hủy")
    check(api("GET", "/api/invoices/lookup?phone=0977111222&room=K103&month=2026-09")[0] == 404, "hóa đơn đã hủy không hiện khi tra cứu")

    # dịch vụ, loại hình, tòa nhà
    s, sv = api("POST", "/api/admin/services", {"serviceName": "Rác", "calcType": "FIXED", "unit": "tháng", "unitPrice": 20000}, ADMIN)
    check(s == 201 and sv["isActive"] is True, "thêm dịch vụ")
    check(api("PUT", f"/api/admin/services/{sv['serviceId']}", {"isActive": False}, ADMIN)[1]["isActive"] is False, "tắt dịch vụ")
    check(api("POST", "/api/admin/services", {"serviceName": "X", "unitPrice": -5}, ADMIN)[0] == 400, "đơn giá âm")
    check(api("DELETE", f"/api/admin/services/{sv['serviceId']}", token=ADMIN)[0] == 200, "xóa dịch vụ chưa dùng")
    check(api("DELETE", "/api/admin/services/1", token=ADMIN)[0] == 409, "không xóa dịch vụ đã có trong hóa đơn")
    s, rt = api("POST", "/api/admin/room-types", {"typeName": "Homestay", "slug": "home-stay", "sortOrder": 7}, ADMIN)
    check(s == 201 and rt["roomCount"] == 0, "thêm loại hình")
    check(api("POST", "/api/admin/room-types", {"typeName": "Y", "slug": "Sai Slug"}, ADMIN)[0] == 400, "slug sai định dạng")
    check("Homestay" in [x["typeName"] for x in api("GET", "/api/meta")[1]["roomTypes"]], "loại hình mới hiện ở ô tìm kiếm")
    check(api("DELETE", f"/api/admin/room-types/{rt['typeId']}", token=ADMIN)[0] == 200, "xóa loại hình")
    check(api("DELETE", "/api/admin/room-types/1", token=ADMIN)[0] == 409, "không xóa loại hình đang có phòng")
    L = loc()
    s, b = api("POST", "/api/admin/buildings", {"managerId": 2, "name": "Khu trọ thử", "address": "1 Đường Thử", "provinceId": L["hcm"], "districtId": L["govap"], "wardId": L["p12"], "latitude": 10.83, "longitude": 106.66}, ADMIN)
    check(s == 201 and b["totalRooms"] == 0 and b["districtName"] == "Quận Gò Vấp" and b["wardName"] == "Phường 12", "thêm tòa nhà (hệ cũ: tỉnh > quận > phường)")
    s, bn = api("POST", "/api/admin/buildings", {"managerId": 2, "name": "Tòa hệ mới", "address": "2 Đường Mới", "provinceId": L["hn"], "wardId": L["hn_wards"]["Phường Cầu Giấy"]}, ADMIN)
    check(s == 201 and bn["districtName"] is None and bn["wardName"] == "Phường Cầu Giấy", "thêm tòa nhà hệ mới (tỉnh > phường, không quận)")
    api("DELETE", f"/api/admin/buildings/{bn['buildingId']}", token=ADMIN)
    check(api("POST", "/api/admin/buildings", {"managerId": 2, "name": "X", "address": "Y", "provinceId": L["hn"], "districtId": L["govap"]}, ADMIN)[0] == 400, "hệ mới không có quận/huyện")
    check(api("POST", "/api/admin/buildings", {"managerId": 2, "name": "X", "address": "Y", "provinceId": L["hcm"]}, ADMIN)[0] == 400, "hệ cũ bắt buộc quận/huyện")
    check(api("POST", "/api/admin/buildings", {"managerId": 2, "name": "X", "address": "Y", "provinceId": L["old_hn"], "districtId": L["govap"]}, ADMIN)[0] == 400, "quận không thuộc tỉnh")
    check(api("POST", "/api/admin/buildings", {"managerId": 3, "name": "X", "address": "Y", "provinceId": L["hcm"], "districtId": L["govap"]}, ADMIN)[0] == 400, "người quản lý là TENANT")
    check(api("POST", "/api/admin/buildings", {"managerId": 2, "name": "X", "address": "Y", "provinceId": L["hcm"], "districtId": L["govap"], "latitude": 10}, ADMIN)[0] == 400, "thiếu kinh độ")
    check(api("DELETE", f"/api/admin/buildings/{b['buildingId']}", token=ADMIN)[0] == 200, "xóa tòa nhà trống")
    check(api("DELETE", "/api/admin/buildings/1", token=ADMIN)[0] == 409, "không xóa tòa nhà đang có phòng")

    # khu vực, địa điểm, tiện nghi
    s, lm = api("POST", "/api/admin/landmarks", {"name": "ĐH Thử Nghiệm", "landmarkType": "school", "districtId": loc()["govap"], "latitude": 10.84, "longitude": 106.67}, ADMIN)
    check(s == 201, "thêm địa điểm")
    check(api("POST", "/api/admin/landmarks", {"name": "Sai", "landmarkType": "school", "latitude": 200, "longitude": 0}, ADMIN)[0] == 400, "vĩ độ sai")
    check(api("DELETE", f"/api/admin/landmarks/{lm['landmarkId']}", token=ADMIN)[0] == 200, "xóa địa điểm")
    s, am = api("POST", "/api/admin/amenities", {"name": "Hồ bơi", "icon": "pool"}, ADMIN)
    check(s == 201 and api("DELETE", f"/api/admin/amenities/{am['amenityId']}", token=ADMIN)[0] == 200, "thêm / xóa tiện nghi")
    for path in ("provinces", "districts", "wards"):
        check(api("POST", f"/api/admin/{path}", {"name": "X"}, ADMIN)[0] in (404, 405), f"{path}: dữ liệu địa giới chỉ đọc (không thêm tay)")
        check(api("GET", f"/api/admin/{path}?size=3", token=ADMIN)[0] == 200, f"{path}: admin vẫn tra cứu được")
    s, pl = api("GET", "/api/admin/provinces?adm_system=NEW&size=100", token=ADMIN)
    check(s == 200, "lọc tỉnh theo hệ")

    # dọn dẹp dữ liệu thử của hợp đồng
    for ep, i in (("invoices", iid),):
        pass
    s, rl = api("GET", "/api/admin/roles", token=ADMIN)
    check(rl["totalItems"] == 3, "danh sách vai trò")
    check(api("POST", "/api/admin/roles", {"roleCode": "X"}, ADMIN)[0] in (404, 405), "vai trò chỉ đọc")
    s, opt = api("GET", "/api/admin/options/rooms?q=k103", token=ADMIN)
    check(s == 200 and len(opt) == 1, "danh sách chọn nhanh phòng")
    check(api("GET", "/api/admin/options/khong-co", token=ADMIN)[0] == 404, "options lạ")
    s, d = api("GET", "/api/admin/dashboard", token=ADMIN)
    check(s == 200 and d["rooms"]["total"] >= 20 and d["buildings"] == 8 and d["unpaidInvoices"] >= 2, "dashboard")
    check(d["revenueThisMonth"] >= 1000000, "doanh thu tháng này tính từ thanh toán")


def test_promotions():
    print("• Chương trình & ưu đãi (admin)")
    now = time.strftime("%Y-%m-%d %H:%M:%S")
    later = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(time.time() + 7200))
    past = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(time.time() - 7200))
    s, p = api("POST", "/api/admin/promotions", {"title": "Hotdeal thử", "promoType": "hotdeal", "discountText": "-20%", "startAt": past, "endAt": later}, ADMIN)
    check(s == 201 and p["isRunning"] is True, "tạo hotdeal đang chạy")
    pid = p["promotionId"]
    check(api("POST", "/api/admin/promotions", {"title": "Sai", "startAt": later, "endAt": past}, ADMIN)[0] == 400, "kết thúc trước bắt đầu")
    check(api("POST", "/api/admin/promotions", {"title": "Sai", "startAt": past, "endAt": later, "linkUrl": "javascript:alert(1)"}, ADMIN)[0] == 400, "liên kết javascript: bị chặn")
    check(api("POST", "/api/admin/promotions", {"title": "Sai", "startAt": past, "endAt": later, "linkUrl": "//evil.com"}, ADMIN)[0] == 400, "liên kết // bị chặn")
    room = api("GET", "/api/rooms?keyword=D01")[1]["items"][0]
    check(api("PUT", f"/api/admin/promotions/{pid}/rooms", {"rooms": [{"roomId": room["roomId"]}]}, ADMIN)[0] == 400, "thiếu giá ưu đãi")
    s, j = api("PUT", f"/api/admin/promotions/{pid}/rooms", {"rooms": [{"roomId": room["roomId"], "dealPrice": 1500000}]}, ADMIN)
    check(s == 200, "gán phòng cho hotdeal")
    d = api("GET", f"/api/rooms/{room['roomId']}")[1]
    check(d["dealPrice"] == 1500000 and d["discountPercent"] == 25 and d["originalPrice"] == 2000000, "phòng hiện giá ưu đãi và % giảm (25%)")
    check(len(api("GET", "/api/promotions")[1]["hotdeals"]) == 3, "hotdeal mới hiện trong danh sách công khai")
    # hết hạn / tắt
    api("PUT", f"/api/admin/promotions/{pid}", {"endAt": now}, ADMIN)
    check(len(api("GET", "/api/promotions")[1]["hotdeals"]) == 2, "hotdeal hết hạn biến mất")
    check(api("GET", f"/api/rooms/{room['roomId']}")[1]["dealPrice"] is None, "hết hạn thì phòng về giá gốc")
    check(api("PUT", f"/api/admin/promotions/{pid}", {"endAt": later, "isActive": False}, ADMIN)[0] == 200, "gia hạn + tắt")
    check(len(api("GET", "/api/promotions")[1]["hotdeals"]) == 2, "chương trình đang tắt không hiện")
    check(api("DELETE", f"/api/admin/promotions/{pid}", token=ADMIN)[0] == 200, "xóa chương trình")
    check(api("GET", f"/api/rooms/{room['roomId']}")[1]["dealPrice"] is None, "xóa chương trình xóa luôn giá ưu đãi")


def test_forms_and_favorites(user_token):
    print("• Hỗ trợ / môi giới / xem phòng / yêu thích")
    s, j = api("POST", "/api/support-requests", {"fullName": "Khách Thử", "phone": "0912000111", "desiredArea": "Gò Vấp", "budgetMax": 3000000, "note": "Cần phòng gần ĐH Công nghiệp"})
    check(s == 201, "gửi yêu cầu hỗ trợ tìm phòng")
    check(api("POST", "/api/support-requests", {"fullName": "K", "phone": "abc"})[0] == 400, "SĐT sai")
    check(api("POST", "/api/support-requests", {"phone": "0912000111"})[0] == 400, "thiếu họ tên")
    check(api("POST", "/api/support-requests", {"fullName": "K", "phone": "0912000111", "requestType": "xyz"})[0] == 400, "loại yêu cầu sai")
    check(api("POST", "/api/broker-applications", {"fullName": "Môi Giới Thử", "phone": "0912000222", "workArea": "Quận 7"})[0] == 201, "đăng ký làm môi giới")
    room = api("GET", "/api/rooms?keyword=M303")[1]["items"][0]
    s, j = api("POST", "/api/viewing-requests", {"roomId": room["roomId"], "fullName": "Khách Xem", "phone": "0912000333", "preferredTime": "Sáng mai"})
    check(s == 201, "đặt lịch xem phòng")
    check(api("POST", "/api/viewing-requests", {"roomId": 99999, "fullName": "K", "phone": "0912000333"})[0] == 404, "phòng không tồn tại")
    check(api("POST", "/api/viewing-requests", {"fullName": "K", "phone": "0912000333"})[0] == 400, "thiếu roomId")
    # admin thấy các yêu cầu
    s, l = api("GET", "/api/admin/support-requests?status=new", token=ADMIN)
    check(l["totalItems"] == 2, "admin thấy yêu cầu hỗ trợ mới")
    rid = l["items"][0]["requestId"]
    check(api("PUT", f"/api/admin/support-requests/{rid}", {"status": "done"}, ADMIN)[1]["status"] == "done", "xử lý xong yêu cầu")
    check(api("GET", "/api/admin/viewing-requests?status=new", token=ADMIN)[1]["totalItems"] == 2, "admin thấy yêu cầu xem phòng")
    check(api("GET", "/api/admin/broker-applications", token=ADMIN)[1]["totalItems"] == 2, "admin thấy đơn môi giới")
    bid = api("GET", "/api/admin/broker-applications", token=ADMIN)[1]["items"][0]["applicationId"]
    check(api("PUT", f"/api/admin/broker-applications/{bid}", {"status": "approved"}, ADMIN)[1]["status"] == "approved", "duyệt đơn môi giới")
    check(api("GET", "/api/stats")[1]["brokers"] == 1, "thống kê môi giới đã duyệt")

    # yêu thích
    check(api("GET", "/api/favorites")[0] == 401, "yêu thích cần đăng nhập")
    check(api("POST", f"/api/favorites/{room['roomId']}", token=user_token)[0] == 200, "thêm yêu thích")
    check(api("POST", f"/api/favorites/{room['roomId']}", token=user_token)[0] == 200, "thêm lần 2 không lỗi")
    check(api("POST", "/api/favorites/99999", token=user_token)[0] == 404, "phòng không tồn tại")
    favs = api("GET", "/api/favorites", token=user_token)[1]["items"]
    check(len(favs) == 1 and favs[0]["isFavorite"] is True, "danh sách yêu thích")
    check(api("GET", f"/api/rooms/{room['roomId']}", token=user_token)[1]["isFavorite"] is True, "chi tiết phòng có cờ isFavorite")
    check(api("GET", f"/api/rooms/{room['roomId']}")[1].get("isFavorite") in (None, False), "người chưa đăng nhập không có cờ yêu thích")
    check(api("DELETE", f"/api/favorites/{room['roomId']}", token=user_token)[0] == 200 and len(api("GET", "/api/favorites", token=user_token)[1]["items"]) == 0, "bỏ yêu thích")


def test_users_and_settings():
    print("• Tài khoản / cấu hình")
    mgr = login("levinh", "Manager@123")
    check(api("GET", "/api/admin/users", token=mgr)[0] == 403, "MANAGER không xem được danh sách tài khoản")
    s, l = api("GET", "/api/admin/users?q=minh", token=ADMIN)
    check(s == 200 and l["totalItems"] >= 1 and "passwordHash" not in json.dumps(l), "ADMIN xem danh sách, không lộ hash")
    s, u = api("POST", "/api/admin/users", {"roleId": 2, "username": "quanly2", "password": "Quanly1234", "fullName": "Quản Lý Hai", "email": "ql2@example.com"}, ADMIN)
    check(s == 201 and u["roleCode"] == "MANAGER" and u["hasPassword"] is True, "ADMIN tạo tài khoản quản lý")
    check(api("POST", "/api/admin/users", {"roleId": 2, "username": "quanly3", "fullName": "Thiếu mật khẩu"}, ADMIN)[0] == 400, "tạo tài khoản bắt buộc có mật khẩu")
    check(api("POST", "/api/admin/users", {"roleId": 2, "username": "quanly3", "password": "yeu", "fullName": "Yếu"}, ADMIN)[0] == 400, "mật khẩu yếu")
    check(api("POST", "/api/auth/login", {"username": "quanly2", "password": "Quanly1234"})[0] == 200, "tài khoản mới đăng nhập được")
    s, u2 = api("PUT", f"/api/admin/users/{u['userId']}", {"status": "locked"}, ADMIN)
    check(s == 200 and u2["status"] == "locked", "khóa tài khoản")
    s, j = api("POST", "/api/auth/login", {"username": "quanly2", "password": "Quanly1234"})
    check(s == 403, "tài khoản bị khóa không đăng nhập được")
    old_token = None
    check(api("PUT", f"/api/admin/users/{u['userId']}", {"status": "active", "password": "Quanly5678"}, ADMIN)[0] == 200, "mở khóa + đặt lại mật khẩu")
    tk = login("quanly2", "Quanly5678")
    api("PUT", f"/api/admin/users/{u['userId']}", {"status": "locked"}, ADMIN)
    check(api("GET", "/api/admin/dashboard", token=tk)[0] == 401, "token của tài khoản đã khóa bị vô hiệu ngay")
    check(api("PUT", "/api/admin/users/1", {"status": "locked"}, ADMIN)[0] == 400, "admin không tự khóa mình")
    check(api("PUT", "/api/admin/users/1", {"roleId": 3}, ADMIN)[0] == 400, "admin không tự hạ quyền mình")
    check(api("DELETE", "/api/admin/users/1", token=ADMIN)[0] == 400, "admin không tự xóa mình")
    check(api("DELETE", f"/api/admin/users/{u['userId']}", token=ADMIN)[0] == 200, "xóa tài khoản chưa dùng")
    check(api("DELETE", "/api/admin/users/2", token=ADMIN)[0] == 409, "không xóa tài khoản đang quản lý tòa nhà")

    s, st = api("GET", "/api/admin/settings", token=ADMIN)
    check(st["site_name"] == "NhàTrọ+" and st["hotline"], "xem cấu hình")
    check(api("PUT", "/api/admin/settings", {"hotline": "1900 1111"}, mgr)[0] == 403, "MANAGER không sửa được cấu hình")
    check(api("PUT", "/api/admin/settings", {"hotline": "1900 1111"}, ADMIN)[0] == 200, "ADMIN sửa cấu hình")
    check(api("GET", "/api/settings")[1]["hotline"] == "1900 1111", "cấu hình mới hiện ở trang công khai")
    check(api("PUT", "/api/admin/settings", {"Bad Key!": "x"}, ADMIN)[0] == 400, "tên cấu hình sai")
    check(api("PUT", "/api/admin/settings", {"hotline": 123}, ADMIN)[0] == 400, "giá trị cấu hình không phải chuỗi")


def test_building_owner():
    print("• Chỉ chủ tòa nhà mới được sửa phòng / dữ liệu của tòa nhà")
    levinh = login("levinh", "Manager@123")
    s, u = api("POST", "/api/admin/users", {"roleId": 2, "username": "chu2", "password": "Chutro1234", "fullName": "Chủ Hai", "email": "chu2@example.com"}, ADMIN)
    check(s == 201, "tạo quản lý thứ hai")
    chu2 = login("chu2", "Chutro1234")
    s, b2 = api("POST", "/api/admin/buildings", {"managerId": u["userId"], "name": "Tòa của chủ 2", "address": "9 Đường Hai", "provinceId": loc()["hcm"], "districtId": loc()["govap"]}, ADMIN)
    check(s == 201, "ADMIN tạo tòa nhà cho quản lý thứ hai")
    bid = b2["buildingId"]
    # chu2 tạo phòng trong tòa của mình, không tạo được ở tòa của levinh
    room = {"buildingId": bid, "typeId": 1, "roomNumber": "O1", "title": "Phòng chủ 2", "baseRentPrice": 2000000, "maxOccupants": 2}
    s, r = api("POST", "/api/rooms", room, chu2)
    check(s == 201, "chủ tòa nhà thêm phòng vào tòa của mình")
    rid = r["roomId"]
    s, j = api("POST", "/api/rooms", dict(room, buildingId=1, roomNumber="O2"), chu2)
    check(s == 403 and "chủ" in j["error"], "quản lý khác KHÔNG thêm được phòng vào tòa nhà người khác")
    s, j = api("POST", "/api/rooms", dict(room, roomNumber="O3"), levinh)
    check(s == 403, "levinh KHÔNG thêm được phòng vào tòa của chủ 2")
    # sửa / ẩn / khôi phục / xóa
    check(api("PUT", f"/api/rooms/{rid}", {"title": "Hack"}, levinh)[0] == 403, "levinh KHÔNG sửa được phòng của chủ 2 (PUT)")
    check(api("PATCH", f"/api/rooms/{rid}", {"baseRentPrice": 1}, levinh)[0] == 403, "levinh KHÔNG sửa được phòng của chủ 2 (PATCH)")
    check(api("DELETE", f"/api/rooms/{rid}", token=levinh)[0] == 403, "levinh KHÔNG ẩn được phòng của chủ 2")
    check(api("PUT", f"/api/rooms/{rid}", {"title": "Phòng chủ 2 (sửa)"}, chu2)[0] == 200, "chủ 2 sửa được phòng của mình")
    check(api("PUT", f"/api/rooms/{rid}", {"buildingId": 1}, chu2)[0] == 403, "không chuyển phòng sang tòa nhà người khác")
    check(api("PUT", "/api/rooms/1", {"title": "Hack"}, chu2)[0] == 403, "chủ 2 KHÔNG sửa được phòng tòa của levinh")
    check(api("DELETE", f"/api/rooms/{rid}", token=chu2)[0] == 200, "chủ 2 ẩn phòng của mình")
    check(api("POST", f"/api/rooms/{rid}/restore", token=levinh)[0] == 403, "levinh KHÔNG khôi phục được phòng của chủ 2")
    check(api("POST", f"/api/rooms/{rid}/restore", token=chu2)[0] == 200, "chủ 2 khôi phục phòng")
    check(api("PUT", f"/api/rooms/{rid}", {"title": "ADMIN sửa"}, ADMIN)[0] == 200, "ADMIN sửa được mọi phòng")
    check(api("PUT", "/api/rooms/1", {"title": api("GET", "/api/rooms/1")[1]["title"]}, levinh)[0] == 200, "levinh vẫn sửa được phòng tòa của mình")
    # tòa nhà
    check(api("PUT", f"/api/admin/buildings/{bid}", {"name": "Hack"}, levinh)[0] == 403, "levinh KHÔNG sửa được tòa của chủ 2")
    check(api("PUT", f"/api/admin/buildings/{bid}", {"managerId": 2}, chu2)[0] == 403, "chủ không tự chuyển tòa cho người khác")
    check(api("DELETE", f"/api/admin/buildings/{bid}", token=levinh)[0] == 403, "levinh KHÔNG xóa được tòa của chủ 2")
    s, nb = api("POST", "/api/admin/buildings", {"managerId": 2, "name": "Giả mạo", "address": "x", "provinceId": 1, "districtId": 5}, chu2)
    check(s == 403, "quản lý không tạo tòa nhà đứng tên người khác")
    # dữ liệu liên quan: hợp đồng, hóa đơn, phòng chọn nhanh
    s, l = api("GET", "/api/admin/contracts", token=chu2)
    check(s == 200 and l["totalItems"] == 0, "chủ 2 không thấy hợp đồng của tòa nhà khác")
    s, l = api("GET", "/api/admin/contracts", token=levinh)
    check(s == 200 and l["totalItems"] > 0, "levinh thấy hợp đồng tòa của mình")
    cid = l["items"][0]["contractId"]
    check(api("GET", f"/api/admin/contracts/{cid}", token=chu2)[0] == 403, "chủ 2 KHÔNG xem được hợp đồng tòa khác")
    check(api("PUT", f"/api/admin/contracts/{cid}", {"note": "x"}, chu2)[0] == 403, "chủ 2 KHÔNG sửa được hợp đồng tòa khác")
    check(api("POST", "/api/admin/invoices/generate", {"contractId": cid, "billingMonth": "2026-09"}, chu2)[0] == 403, "chủ 2 KHÔNG tạo được hóa đơn cho hợp đồng tòa khác")
    s, o = api("GET", "/api/admin/options/buildings", token=chu2)
    check(s == 200 and [x["id"] for x in o] == [bid], "danh sách chọn tòa nhà chỉ gồm tòa của mình")
    s, o = api("GET", "/api/admin/options/rooms", token=chu2)
    check(s == 200 and all("Tòa của chủ 2" in x["label"] for x in o), "danh sách chọn phòng chỉ gồm phòng của mình")


def test_rate_limit():
    print("• Giới hạn tần suất")
    codes = [api("GET", "/api/invoices/lookup?phone=0900000000&room=ZZ")[0] for _ in range(30)]
    check(429 in codes and codes[0] == 404, "tra cứu hóa đơn bị giới hạn 20 lần / phút")
    codes = [api("POST", "/api/auth/login", {"username": "x", "password": "y"})[0] for _ in range(20)]
    check(429 in codes, "đăng nhập sai nhiều lần bị giới hạn")



PNG = bytes.fromhex("89504e470d0a1a0a0000000d49484452000000010000000108060000001f15c4890000000d49444154789c6360000002000001e221bc330000000049454e44ae426082")


def upload(files, token):
    """Gửi multipart/form-data tới /api/uploads. files: [(tên, bytes)]"""
    boundary = "----nhatrotest"
    body = b""
    for name, data in files:
        body += (f"--{boundary}\r\nContent-Disposition: form-data; name=\"file\"; filename=\"{name}\"\r\nContent-Type: application/octet-stream\r\n\r\n").encode() + data + b"\r\n"
    body += f"--{boundary}--\r\n".encode()
    req = urllib.request.Request(BASE + "/api/uploads", data=body, method="POST")
    req.add_header("Content-Type", "multipart/form-data; boundary=" + boundary)
    if token:
        req.add_header("Authorization", "Bearer " + token)
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            return r.status, json.loads(r.read().decode())
    except urllib.error.HTTPError as e:
        try:
            return e.code, json.loads(e.read().decode())
        except ValueError:
            return e.code, None


def test_locations():
    print("• Địa giới hành chính Việt Nam (mới + cũ), gợi ý gần tôi")
    new = api("GET", "/api/locations/provinces?system=new")[1]
    old = api("GET", "/api/locations/provinces?system=old")[1]
    check(len(new) == 34 and len(old) == 63, f"34 tỉnh/thành hệ mới, 63 hệ cũ ({len(new)}/{len(old)})")
    check(all(p["system"] == "NEW" for p in new) and all(p["system"] == "OLD" for p in old), "đúng nhãn hệ")
    L = loc()
    ds = api("GET", f"/api/locations/districts?provinceId={L['hcm']}")[1]
    check(len(ds) == 22 and any(d["name"] == "Quận 1" for d in ds) and any(d["name"] == "Thành phố Thủ Đức" for d in ds), f"TP.HCM (cũ) có đủ quận/huyện ({len(ds)})")
    check(api("GET", f"/api/locations/districts?provinceId={L['hn']}")[1] == [], "hệ mới không có cấp quận/huyện")
    w_old = api("GET", f"/api/locations/wards?districtId={L['q1']}")[1]
    check(len(w_old) == 10, f"Quận 1 có 10 phường ({len(w_old)})")
    w_new = api("GET", f"/api/locations/wards?provinceId={L['hn']}")[1]
    check(len(w_new) > 100 and all(w["districtId"] is None for w in w_new), f"Hà Nội (mới) có danh sách phường/xã không qua quận ({len(w_new)})")
    s, r = api("GET", f"/api/locations/wards?provinceId={L['hn']}&q=cau%20giay")
    check(any(w["name"] == "Phường Cầu Giấy" for w in r), "tìm phường không dấu")
    check(api("GET", "/api/locations/districts")[0] == 400, "thiếu provinceId -> 400")
    check(api("GET", "/api/locations/provinces?system=xyz")[0] == 400, "system sai -> 400")
    # gần tôi: Gò Vấp
    s, n = api("GET", "/api/locations/nearby?lat=10.8385&lng=106.6652&radiusKm=5")
    check(s == 200 and n["areas"] and n["areas"][0]["roomCount"] >= 1 and n["areas"][0]["distanceKm"] <= 5, "gợi ý khu vực gần vị trí thiết bị")
    check("Gò Vấp" in n["areas"][0]["label"], "khu vực gần nhất là Gò Vấp: " + n["areas"][0]["label"])
    s, n2 = api("GET", "/api/locations/nearby?lat=21.0285&lng=105.8542&radiusKm=3")
    check(s == 200 and n2["areas"] and "Hà Nội" in n2["areas"][0]["label"], "vị trí ở Hà Nội gợi ý khu vực Hà Nội")
    check(api("GET", "/api/locations/nearby?lat=1")[0] == 400 and api("GET", "/api/locations/nearby?lat=99&lng=0")[0] == 400, "toạ độ thiếu/sai -> 400")


def test_uploads(mgr):
    print("• Tải ảnh từ thiết bị")
    check(upload([("a.png", PNG)], None)[0] == 401, "chưa đăng nhập không tải được")
    s, j = upload([("a.png", PNG), ("b.png", PNG)], ADMIN)
    check(s == 201 and len(j["files"]) == 2 and j["files"][0]["url"].startswith("/uploads/"), "tải 2 ảnh PNG")
    url = j["files"][0]["url"]
    with urllib.request.urlopen(BASE + url, timeout=10) as r:
        check(r.status == 200 and r.headers.get("Content-Type", "").startswith("image/") and r.read() == PNG, "ảnh tải lên truy cập được và giữ nguyên nội dung")
    check(upload([("x.png", b"<script>alert(1)</script>")], ADMIN)[0] == 400, "file không phải ảnh bị từ chối")
    check(upload([("big.png", PNG + b"0" * (5 * 1024 * 1024 + 10))], ADMIN)[0] == 400, "ảnh > 5MB bị từ chối")
    check(upload([("a.png", PNG)] * 11, ADMIN)[0] == 400, "tối đa 10 ảnh mỗi lần")
    tenant_tok = login("minhanh", "Tenant@123") if False else None
    # lưu ảnh vào tòa nhà + phòng
    L = loc()
    s, b = api("POST", "/api/admin/buildings", {"managerId": 2, "name": "Tòa ảnh", "address": "5 Ảnh", "provinceId": L["hcm"], "districtId": L["govap"],
                                                "imageUrls": [url, j["files"][1]["url"]]}, ADMIN)
    check(s == 201, f"tạo tòa nhà có ảnh ({s})")
    s, bd = api("GET", f"/api/admin/buildings/{b['buildingId']}", token=ADMIN)
    check(len(bd["images"]) == 2 and bd["coverUrl"] == url, "ảnh tòa nhà lưu trong database, ảnh đầu là ảnh bìa")
    check(api("PUT", f"/api/admin/buildings/{b['buildingId']}", {"imageUrls": ["http://evil.example/x.js"]}, ADMIN)[0] in (200, 400), "URL ảnh ngoài được kiểm tra")
    check(api("PUT", f"/api/admin/buildings/{b['buildingId']}", {"imageUrls": ["javascript:alert(1)"]}, ADMIN)[0] == 400, "URL ảnh javascript: bị chặn")
    room = {"buildingId": b["buildingId"], "typeId": 1, "roomNumber": "IMG1", "title": "Phòng có ảnh", "baseRentPrice": 2000000, "maxOccupants": 2, "imageUrls": [url]}
    s, r = api("POST", "/api/rooms", room, ADMIN)
    check(s == 201 and r["thumbnailUrl"] == url and len(r["images"]) == 1, "ảnh phòng lưu và tự làm ảnh đại diện")
    api("DELETE", f"/api/rooms/{r['roomId']}?type=1", token=ADMIN)
    api("DELETE", f"/api/admin/buildings/{b['buildingId']}", token=ADMIN)


def test_scoping_contract_resign():
    print("• Mỗi chủ tòa chỉ thấy dữ liệu của mình; hợp đồng; hủy quyền quản lý")
    L = loc()
    levinh = login("levinh", "Manager@123")
    s, u = api("POST", "/api/admin/users", {"roleId": 2, "username": "chu3", "password": "Chutro3456", "fullName": "Chủ Ba", "email": "chu3@example.com"}, ADMIN)
    check(s == 201, "tạo chủ tòa thứ ba")
    chu3 = login("chu3", "Chutro3456")
    s, b3 = api("POST", "/api/admin/buildings", {"managerId": u["userId"], "name": "Tòa của chủ 3", "address": "3 Đường Ba", "provinceId": L["hn"], "wardId": L["hn_wards"]["Phường Cầu Giấy"]}, ADMIN)
    bid = b3["buildingId"]
    s, rm = api("POST", "/api/rooms", {"buildingId": bid, "typeId": 1, "roomNumber": "C301", "title": "Phòng C301", "baseRentPrice": 3000000, "maxOccupants": 3}, chu3)
    check(s == 201, "chủ 3 thêm phòng")
    rid = rm["roomId"]
    s, t1 = api("POST", "/api/admin/tenants", {"fullName": "Khách Ba A", "phone": "0911000001", "roomId": rid, "idNumber": "079111111111", "permanentAddress": "Hà Nội"}, chu3)
    s2, t2 = api("POST", "/api/admin/tenants", {"fullName": "Khách Ba B", "phone": "0911000002", "roomId": rid}, chu3)
    check(s == 201 and s2 == 201, "chủ 3 thêm 2 người thuê vào cùng phòng")
    check(api("POST", "/api/admin/tenants", {"fullName": "Lạ", "phone": "0911000003"}, chu3)[0] == 400, "người thuê của chủ tòa phải gán phòng")
    check(api("POST", "/api/admin/tenants", {"fullName": "Lạ", "phone": "0911000003", "roomId": 1}, chu3)[0] == 403, "không thêm người thuê vào phòng của chủ khác")
    s, v = api("POST", "/api/admin/vehicles", {"tenantId": t1["tenantId"], "licensePlate": "30A-999.99", "vehicleType": "motorbike", "brandColor": "Honda đỏ"}, chu3)
    check(s == 201, "chủ 3 thêm xe")
    check(api("POST", "/api/admin/vehicles", {"tenantId": 1, "licensePlate": "51A-1"}, chu3)[0] in (403, 404), "không thêm xe cho người thuê của chủ khác")
    s, tk = api("POST", "/api/admin/tickets", {"roomId": rid, "tenantId": t1["tenantId"], "category": "maintenance", "title": "Hỏng vòi nước"}, chu3)
    check(s == 201, "chủ 3 tạo sự cố")
    s, ct = api("POST", "/api/admin/contracts", {"roomId": rid, "tenantId": t1["tenantId"], "startDate": "2026-10-01", "endDate": "2027-09-30", "depositAmount": 6000000, "status": "active",
                                                  "paymentDay": 10, "lateFeePercent": 2, "assetsText": "Giường | 1 | Tốt\nTủ | 1 | Tốt"}, chu3)
    check(s == 201 and ct["contractCode"].startswith("HD-2026-") and ct["monthlyRent"] == 3000000, "tạo hợp đồng chi tiết, tự sinh số hợp đồng và giá thuê")

    # phạm vi nhìn thấy
    def ids(path, tok, key):
        s, j = api("GET", path, token=tok)
        assert s == 200, (path, s, j)
        return {x[key] for x in j["items"]}
    for path, key, mine in (("tenants", "tenantId", t1["tenantId"]), ("vehicles", "vehicleId", v["vehicleId"]), ("tickets", "ticketId", tk["ticketId"]), ("contracts", "contractId", ct["contractId"])):
        a = ids(f"/api/admin/{path}?size=100", ADMIN, key); l = ids(f"/api/admin/{path}?size=100", levinh, key); c = ids(f"/api/admin/{path}?size=100", chu3, key)
        check(mine in a and mine in c and mine not in l, f"{path}: dữ liệu của chủ 3 — admin & chủ 3 thấy, chủ 1 (levinh) không thấy")
        check(c and not (c & (l - {mine})) , f"{path}: chủ 3 không thấy dữ liệu của chủ khác")
        check(len(a) >= len(l) + len(c) - 0, f"{path}: admin thấy tất cả")
    check(api("GET", f"/api/admin/tenants/{t1['tenantId']}", token=levinh)[0] in (403, 404), "chủ 1 không mở được người thuê của chủ 3")
    check(api("GET", f"/api/admin/contracts/{ct['contractId']}/document", token=levinh)[0] == 403, "chủ 1 không xem được hợp đồng của chủ 3")
    check(api("GET", "/api/admin/owners", token=levinh)[0] == 403 and api("GET", "/api/admin/users", token=chu3)[0] == 403, "owners / users chỉ ADMIN")
    s, ow = api("GET", "/api/admin/owners", token=ADMIN)
    me3 = [o for o in ow["items"] if o["username"] == "chu3"][0]
    check(me3["buildingCount"] == 1 and me3["roomCount"] == 1 and me3["tenantCount"] == 2 and me3["activeContracts"] == 1, "thống kê theo từng chủ tòa")
    # nhóm theo tòa -> phòng (đã sắp xếp)
    s, tl = api("GET", "/api/admin/tenants?size=100", token=ADMIN)
    keys = [(x["buildingName"] or "~", x["roomNumber"] or "") for x in tl["items"]]
    check(keys == sorted(keys), "người thuê sắp theo tòa nhà rồi đến phòng")
    check(api("GET", f"/api/admin/tenants?buildingId={bid}&size=100", token=ADMIN)[1]["totalItems"] == 2, "lọc người thuê theo tòa nhà")
    d3 = api("GET", "/api/admin/dashboard", token=chu3)[1]
    check(d3["scope"] == "manager" and d3["buildings"] == 1 and d3["rooms"]["total"] == 1, "dashboard chủ tòa chỉ tính tòa của mình")
    da = api("GET", "/api/admin/dashboard", token=ADMIN)[1]
    check(da["scope"] == "admin" and da["buildings"] > 1 and "owners" in da, "dashboard admin tính toàn hệ thống")
    check(api("GET", "/api/admin/dashboard", token=chu3)[1].get("owners") is None, "dashboard chủ tòa không lộ số liệu toàn hệ thống")

    # hợp đồng pháp lý
    s, doc = api("GET", f"/api/admin/contracts/{ct['contractId']}/document", token=chu3)
    check(s == 200 and doc["landlord"]["fullName"] == "Chủ Ba" and doc["tenant"]["fullName"] == "Khách Ba A" and doc["building"]["fullAddress"].endswith("Phường Cầu Giấy, Hà Nội"), "tài liệu hợp đồng đủ hai bên + địa chỉ")
    check(len(doc["occupants"]) == 1 and len(doc["vehicles"]) == 1 and len(doc["assets"]) == 2 and len(doc["missingInfo"]) >= 1, "có người ở cùng, xe, tài sản, cảnh báo thiếu thông tin pháp lý")
    check(api("PUT", "/api/auth/me", {"fullName": "Chủ Ba", "idNumber": "001090000001", "birthDate": "1990-01-02", "address": "Hà Nội", "bankName": "VCB", "bankAccount": "0011"}, chu3)[0] == 200, "chủ tòa cập nhật hồ sơ pháp lý")
    check(api("PUT", f"/api/admin/contracts/{ct['contractId']}", {"paymentDay": 40}, chu3)[0] == 400, "ngày thanh toán phải 1-31")
    check(api("PUT", f"/api/admin/contracts/{ct['contractId']}", {"lateFeePercent": 150}, chu3)[0] == 400, "phạt trả chậm tối đa 100%")
    s, page, h = call("GET", "/admin/contract.html", raw=True)
    check(s == 200 and "contract.js" in page, "trang in hợp đồng được phục vụ")

    # hủy quyền quản lý
    check(api("POST", "/api/auth/resign-manager", {}, ADMIN)[0] == 400, "ADMIN không dùng chức năng hủy quyền quản lý")
    s, j = api("POST", "/api/auth/resign-manager", {"password": "Chutro3456", "confirm": True}, chu3)
    check(s == 409 and "hợp đồng" in j["error"], "còn hợp đồng hiệu lực thì không hủy được quyền")
    api("PUT", f"/api/admin/contracts/{ct['contractId']}", {"status": "ended"}, chu3)
    s, j = api("POST", "/api/auth/resign-manager", {}, chu3)
    check(s == 200 and j.get("needConfirm") is True, "hỏi xác nhận trước khi hủy quyền")
    s, j = api("POST", "/api/auth/resign-manager", {"confirm": True, "password": "sai-mat-khau"}, chu3)
    check(s == 400, "sai mật khẩu không hủy được")
    s, j = api("POST", "/api/auth/resign-manager", {"confirm": True, "password": "Chutro3456"}, chu3)
    check(s == 200 and j["user"]["role"] == "TENANT" and j["token"], "hủy quyền thành công, trở lại người dùng thường")
    check(api("GET", "/api/admin/tenants", token=chu3)[0] == 403, "sau khi hủy không vào được trang quản lý")
    bb = api("GET", f"/api/admin/buildings/{bid}", token=ADMIN)[1]
    check(bb["managerId"] == 1, "tòa nhà chuyển về quản trị viên")
    check(api("GET", f"/api/rooms/{rid}")[0] == 404, "phòng bị ẩn khỏi trang công khai")


def test_settings_featured_promos():
    print("• Hotline/Zalo, khuyến mãi %, mục nổi bật")
    levinh = login("levinh", "Manager@123")
    check(api("GET", "/api/admin/settings", token=levinh)[0] == 403 and api("PUT", "/api/admin/settings", {"hotline": "1900 1"}, levinh)[0] == 403, "chủ tòa không sửa được hotline/Zalo")
    check(api("PUT", "/api/admin/settings", {"hotline": "abc"}, ADMIN)[0] == 400, "hotline sai định dạng")
    check(api("PUT", "/api/admin/settings", {"zalo": "<script>"}, ADMIN)[0] == 400, "zalo sai định dạng")
    s, _ = api("PUT", "/api/admin/settings", {"hotline": "1900 7777", "zalo": "0909 000 111"}, ADMIN)
    check(s == 200, "admin lưu hotline + zalo")
    st = api("GET", "/api/settings")[1]
    check(st["hotline"] == "1900 7777" and st["zalo"] == "0909 000 111", "trang công khai hiện hotline/zalo mới")
    check(api("PUT", "/api/admin/settings", {"featured_mode": "bậy"}, ADMIN)[0] == 400 and api("PUT", "/api/admin/settings", {"featured_count": "50"}, ADMIN)[0] == 400, "kiểm tra cấu hình nổi bật")

    # % khuyến mãi
    now = time.strftime("%Y-%m-%d %H:%M:%S")
    end = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(time.time() + 86400 * 5))
    s, p = api("POST", "/api/admin/promotions", {"title": "Giảm 20% thử", "promoType": "hotdeal", "discountPercent": 20, "startAt": now, "endAt": end}, ADMIN)
    check(s == 201 and p["discountText"] == "-20%", "tạo khuyến mãi theo %, tự điền chữ nổi bật")
    check(api("POST", "/api/admin/promotions", {"title": "Sai", "discountPercent": 95, "startAt": now, "endAt": end}, ADMIN)[0] == 400, "% giảm tối đa 90")
    check(api("POST", "/api/admin/promotions", {"title": "Sai", "discountPercent": 10, "startAt": now, "endAt": end}, levinh)[0] == 403, "chủ tòa không tạo được khuyến mãi")
    room = api("GET", "/api/rooms?keyword=P101")[1]["items"][0]
    price = room["originalPrice"] if room.get("discountPercent") else room["price"]
    s, _ = api("PUT", f"/api/admin/promotions/{p['promotionId']}/rooms", {"rooms": [{"roomId": room["roomId"]}]}, ADMIN)
    check(s == 200, "gán phòng vào khuyến mãi % (không cần nhập giá)")
    pd = api("GET", f"/api/admin/promotions/{p['promotionId']}", token=ADMIN)[1]
    base = pd["rooms"][0]["baseRentPrice"]
    check(pd["rooms"][0]["dealPrice"] == round(base * 0.8 / 1000) * 1000, "giá ưu đãi tự tính theo %")
    api("PUT", f"/api/admin/promotions/{p['promotionId']}", {"discountPercent": 50}, ADMIN)
    pd = api("GET", f"/api/admin/promotions/{p['promotionId']}", token=ADMIN)[1]
    check(pd["rooms"][0]["dealPrice"] == round(base * 0.5 / 1000) * 1000, "đổi % thì giá các phòng được tính lại")
    api("DELETE", f"/api/admin/promotions/{p['promotionId']}", token=ADMIN)

    # nổi bật
    rooms = api("GET", "/api/rooms?typeId=1&size=100")[1]["items"]
    target = rooms[-1]
    check(api("POST", f"/api/admin/rooms/{target['roomId']}/feature", {"isFeatured": True}, levinh)[0] == 403, "chủ tòa không ghim nổi bật được")
    check(api("POST", f"/api/admin/rooms/{target['roomId']}/feature", {"isFeatured": True, "featuredOrder": -5}, ADMIN)[0] == 200, "admin ghim phòng nổi bật")
    h = api("GET", "/api/home")[1]
    sec = [x for x in h["featured"] if x["type"]["slug"] == "phong-tro"][0]
    check(sec["rooms"][0]["roomId"] == target["roomId"] and sec["rooms"][0]["featuredSource"] == "pinned", "phòng được ghim lên đầu mục Phòng trọ nổi bật")
    api("PUT", "/api/admin/settings", {"featured_mode": "manual"}, ADMIN)
    h = api("GET", "/api/home")[1]
    sec = [x for x in h["featured"] if x["type"]["slug"] == "phong-tro"][0]
    check(all(r["featuredSource"] == "pinned" for r in sec["rooms"]), "chế độ thủ công chỉ hiện phòng được ghim")
    api("PUT", "/api/admin/settings", {"featured_mode": "auto"}, ADMIN)
    h = api("GET", "/api/home")[1]
    sec = [x for x in h["featured"] if x["type"]["slug"] == "phong-tro"][0]
    vc = [r["viewCount"] for r in sec["rooms"]]
    check(all(r["featuredSource"] == "views" for r in sec["rooms"]) and vc == sorted(vc, reverse=True), "chế độ tự động xếp theo lượt xem")
    api("PUT", "/api/admin/settings", {"featured_mode": "mixed"}, ADMIN)
    api("POST", f"/api/admin/rooms/{target['roomId']}/feature", {"isFeatured": False}, ADMIN)
    fd = api("GET", "/api/admin/featured", token=ADMIN)[1]
    check("sections" in fd and "pinned" in fd and fd["settings"].get("featured_mode") == "mixed", "API quản lý nổi bật")


ADMIN = None


def main():
    global ADMIN
    binary = find_binary()
    tmp = tempfile.mkdtemp(prefix="nhatro-test-")
    mock = start_mock()
    env = dict(os.environ, GOOGLE_CLIENT_ID=CLIENT_ID, GOOGLE_TOKENINFO_URL=f"http://127.0.0.1:{MOCK_PORT}/tokeninfo")
    log = open(os.path.join(tmp, "server.log"), "w")
    proc = subprocess.Popen([binary, "--port", str(PORT), "--db", os.path.join(tmp, "test.db"), "--reset"],
                            cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT)
    try:
        for _ in range(100):
            try:
                urllib.request.urlopen(BASE + "/api/auth/config", timeout=1)
                break
            except Exception:
                time.sleep(0.1)
        else:
            sys.exit("Server không khởi động được:\n" + open(os.path.join(tmp, "server.log")).read())

        ADMIN = login("admin", "Admin@123")
        def run(fn, *a):
            try:
                return fn(*a)
            except Exception as e:  # một nhóm lỗi không làm dừng cả bộ kiểm thử
                import traceback
                check(False, f"{fn.__name__} bị ngắt: {e!r} ở dòng {traceback.extract_tb(e.__traceback__)[-1].lineno}")
        run(test_basic)
        run(test_meta_home)
        run(test_search)
        user_token = run(test_auth)
        run(test_google)
        run(test_rooms_admin)
        run(test_admin_crud)
        run(test_promotions)
        run(test_forms_and_favorites, user_token)
        run(test_users_and_settings)
        run(test_building_owner)
        run(test_locations)
        run(test_uploads, None)
        run(test_scoping_contract_resign)
        run(test_settings_featured_promos)
        run(test_rate_limit)
    finally:
        proc.terminate()
        try:
            proc.wait(5)
        except Exception:
            proc.kill()
        mock.shutdown()
        log.close()
        server_log = open(os.path.join(tmp, "server.log")).read()
        shutil.rmtree(tmp, ignore_errors=True)
    errs = [l for l in server_log.splitlines() if "[SQLite]" in l or "[LỖI]" in l]
    check(not errs, "log server không có lỗi nội bộ: " + "; ".join(errs[:3]))
    print(f"\n{passed} kiểm tra đạt, {failed} lỗi")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
