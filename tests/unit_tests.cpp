// Unit test đơn giản (không cần thư viện test): chạy bằng "ctest" hoặc ./unit_tests
#include "crypto.hpp"
#include "db.hpp"
#include "util.hpp"

#include <fstream>
#include <iostream>
#include <sstream>

static int g_fail = 0, g_pass = 0;

#define CHECK(cond)                                                                    \
    do {                                                                               \
        if (cond) ++g_pass;                                                            \
        else {                                                                         \
            ++g_fail;                                                                  \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n";    \
        }                                                                              \
    } while (0)

#define CHECK_EQ(a, b) CHECK((a) == (b))

static void test_crypto() {
    CHECK_EQ(crypto::sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK_EQ(crypto::to_hex(crypto::hmac_sha256("key", "The quick brown fox jumps over the lazy dog")),
             "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
    // RFC 7914 / giá trị chuẩn PBKDF2-HMAC-SHA256
    CHECK_EQ(crypto::pbkdf2_sha256_hex("password", "salt", 1), "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b");
    CHECK_EQ(crypto::pbkdf2_sha256_hex("password", "salt", 4096), "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a");

    std::string h = crypto::hash_password("Mat-khau-123");
    CHECK(crypto::verify_password("Mat-khau-123", h));
    CHECK(!crypto::verify_password("mat-khau-123", h));
    CHECK(!crypto::verify_password("x", "khong-phai-hash"));
    // mật khẩu của tài khoản mẫu trong database
    CHECK(crypto::verify_password("Admin@123",
          "pbkdf2_sha256$100000$61646d696e2d73616c74$112828e8e1b0c5ecd4bfa58e79a254a53d3d6910039c80e49fb8e664bb249e3f"));

    auto t = crypto::jwt_sign({{"sub", 7}, {"exp", 9999999999LL}}, "secret-key");
    auto ok = crypto::jwt_verify(t, "secret-key");
    CHECK(ok.has_value() && (*ok)["sub"] == 7);
    CHECK(!crypto::jwt_verify(t, "other-key").has_value());
    CHECK(!crypto::jwt_verify(t + "x", "secret-key").has_value());
    auto expired = crypto::jwt_sign({{"sub", 7}, {"exp", 100}}, "secret-key");
    CHECK(!crypto::jwt_verify(expired, "secret-key").has_value());
    // chặn alg=none
    std::string none_tok = crypto::b64url_encode(R"({"alg":"none","typ":"JWT"})") + "." +
                           crypto::b64url_encode(R"({"sub":1,"exp":9999999999})") + ".";
    CHECK(!crypto::jwt_verify(none_tok, "secret-key").has_value());
    CHECK_EQ(crypto::b64url_decode(crypto::b64url_encode("hello!?~")), "hello!?~");
}

static void test_util() {
    CHECK_EQ(util::vn_fold("Gò Vấp"), "go vap");
    CHECK_EQ(util::vn_fold("ĐẠI HỌC BÁCH KHOA"), "dai hoc bach khoa");
    CHECK_EQ(util::vn_fold("Phòng pass & ở ghép"), "phong pass & o ghep");
    CHECK_EQ(util::vn_fold("Nguyễn Hữu Cảnh"), "nguyen huu canh");
    CHECK_EQ(util::like_escape("50%_x!"), "50!%!_x!!");
    CHECK(util::valid_username("levinh_01"));
    CHECK(!util::valid_username("a b"));
    CHECK(util::valid_email("a.b@example.com"));
    CHECK(!util::valid_email("abc@"));
    CHECK(util::valid_phone("0901234567"));
    CHECK(util::valid_phone("+84 901 234 567"));
    CHECK(!util::valid_phone("12345"));
    CHECK(util::valid_month("2026-10"));
    CHECK(!util::valid_month("2026-13"));
    CHECK(util::valid_date("2026-10-14"));
    CHECK(!util::valid_date("2026-14-01"));
    CHECK_EQ(util::snake_to_camel("room_type_id"), "roomTypeId");
    CHECK_EQ(util::camel_to_snake("roomTypeId"), "room_type_id");
    CHECK_EQ(util::mask_name("Trần Văn Bình"), "Trần V** B**");
    CHECK_EQ(util::pad_int(7, 4), "0007");
}

static void test_db_schema(const std::string& schema_path) {
    // schema + địa giới hành chính (mới/cũ) + dữ liệu mẫu, nạp theo đúng thứ tự như khi chạy thật
    Db db(":memory:");
    const std::string dir = schema_path.substr(0, schema_path.find_last_of("/\\") == std::string::npos ? 0 : schema_path.find_last_of("/\\"));
    for (const std::string& f : {schema_path, dir + "/seed_vn_admin.sql", dir + "/seed_demo.sql"}) {
        std::ifstream in(f, std::ios::binary);
        CHECK(static_cast<bool>(in));
        if (!in) return;
        std::stringstream ss;
        ss << in.rdbuf();
        db.exec_script(ss.str());
    }
    CHECK_EQ(db.scalar("SELECT COUNT(*) FROM provinces WHERE adm_system = 'NEW'"), 34);
    CHECK_EQ(db.scalar("SELECT COUNT(*) FROM provinces WHERE adm_system = 'OLD'"), 63);
    CHECK_EQ(db.scalar("SELECT COUNT(*) FROM districts d JOIN provinces p ON p.province_id = d.province_id WHERE p.adm_system = 'NEW'"), 0);
    CHECK(db.scalar("SELECT COUNT(*) FROM wards w JOIN provinces p ON p.province_id = w.province_id WHERE p.adm_system = 'NEW' AND w.district_id IS NULL") > 3000);
    CHECK(db.scalar("SELECT COUNT(*) FROM wards w JOIN provinces p ON p.province_id = w.province_id WHERE p.adm_system = 'OLD' AND w.district_id IS NOT NULL") > 10000);
    CHECK_EQ(db.scalar("SELECT COUNT(*) FROM roles"), 3);
    CHECK_EQ(db.scalar("SELECT COUNT(*) FROM room_types"), 6);
    CHECK(db.scalar("SELECT COUNT(*) FROM rooms") >= 20);

    // tìm kiếm không dấu / không phân biệt hoa thường
    CHECK(db.scalar("SELECT COUNT(*) FROM districts WHERE vn_fold(name) LIKE '%go vap%'") == 1);
    CHECK_EQ(db.scalar("SELECT COUNT(*) FROM wards WHERE name = 'Phường Bến Nghé'"), 1);
    // khoảng cách Haversine: Landmark 81 -> tòa Saigon Pearl ~ 0 km
    json d = db.query_one("SELECT haversine(10.7951, 106.7218, latitude, longitude) AS km FROM buildings WHERE building_id = 2");
    CHECK(d["km"].get<double>() < 0.2);

    // camelCase + bool
    json r = db.query_one("SELECT room_id, is_hot FROM rooms WHERE room_number = 'P102'");
    CHECK(r.contains("roomId") && r["isHot"] == true);

    // ràng buộc: trùng mã phòng trong cùng tòa nhà -> 409; khóa ngoại sai -> 409
    bool threw = false;
    try {
        db.execute("INSERT INTO rooms (building_id,type_id,room_number,title,create_by,update_by) VALUES (1,1,'P101','trùng',1,1)");
    } catch (const HttpError& e) {
        threw = (e.status == 409);
    }
    CHECK(threw);
    threw = false;
    try {
        db.execute("INSERT INTO rooms (building_id,type_id,room_number,title,create_by,update_by) VALUES (999,1,'ZZ','x',1,1)");
    } catch (const HttpError& e) {
        threw = (e.status == 409);
    }
    CHECK(threw);

    // transaction rollback
    try {
        db.tx([&] {
            db.execute("UPDATE rooms SET title = 'sửa thử' WHERE room_number = 'P101'");
            throw std::runtime_error("hủy");
        });
    } catch (const std::runtime_error&) {
    }
    CHECK(db.query_one("SELECT title FROM rooms WHERE room_number = 'P101'")["title"] != "sửa thử");

    // dữ liệu mẫu nhất quán: tổng hóa đơn = tổng chi tiết
    CHECK_EQ(db.scalar("SELECT COUNT(*) FROM invoices i WHERE i.total_amount <> (SELECT SUM(subtotal) FROM invoice_details d WHERE d.invoice_id = i.invoice_id)"), 0);
}

int main(int argc, char** argv) {
    test_crypto();
    test_util();
    test_db_schema(argc > 1 ? argv[1] : "database/schema.sqlite.sql");
    std::cout << g_pass << " kiểm tra đạt, " << g_fail << " lỗi\n";
    return g_fail == 0 ? 0 : 1;
}
