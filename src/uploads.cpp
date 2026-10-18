// Tải ảnh từ thiết bị lên (ảnh tòa nhà / ảnh phòng): kiểm tra loại file bằng nội dung thật, lưu file vào thư mục uploads
// và ghi thông tin vào bảng uploads. Đường dẫn trả về (/uploads/...) được lưu vào room_images / building_images / thumbnail.
#include "app.hpp"

#include <filesystem>
#include <fstream>

namespace {

constexpr size_t kMaxImageBytes = 5 * 1024 * 1024;

// Nhận diện định dạng theo byte đầu của file (không tin đuôi file hay Content-Type do client gửi)
bool sniff_image(const std::string& d, std::string& ext, std::string& mime) {
    auto starts = [&](const char* sig, size_t n) { return d.size() >= n && d.compare(0, n, sig, n) == 0; };
    if (starts("\xFF\xD8\xFF", 3)) { ext = "jpg"; mime = "image/jpeg"; return true; }
    if (starts("\x89PNG\r\n\x1a\n", 8)) { ext = "png"; mime = "image/png"; return true; }
    if (starts("GIF87a", 6) || starts("GIF89a", 6)) { ext = "gif"; mime = "image/gif"; return true; }
    if (d.size() >= 12 && d.compare(0, 4, "RIFF") == 0 && d.compare(8, 4, "WEBP") == 0) { ext = "webp"; mime = "image/webp"; return true; }
    return false;
}

std::string clean_name(const std::string& n) {
    std::string out;
    for (unsigned char c : n) {
        if (c == '/' || c == '\\' || c < 32) continue;
        out += static_cast<char>(c);
    }
    if (out.size() > 200) out.resize(200);
    return out.empty() ? "anh" : out;
}

}  // namespace

void register_upload_routes(httplib::Server& svr, App& app) {
    // POST /api/uploads  (multipart/form-data, trường "file", có thể gửi nhiều file một lần)
    add_route(svr, "POST", "/api/uploads", [&app](const Req& req, Res& res) {
        User u = require_staff(app, req);
        rate_limit(app, req, "upload:" + std::to_string(u.id), 60, 60);
        if (!req.form.has_file("file")) bad_request("Hãy chọn ảnh để tải lên (trường 'file')");
        auto files = req.form.get_files("file");
        if (files.size() > 10) bad_request("Mỗi lần tải tối đa 10 ảnh");
        json out = json::array();
        for (const auto& f : files) {
            if (f.content.empty()) bad_request("File rỗng");
            if (f.content.size() > kMaxImageBytes) bad_request("Ảnh \"" + clean_name(f.filename) + "\" lớn hơn 5 MB");
            std::string ext, mime;
            if (!sniff_image(f.content, ext, mime)) bad_request("File \"" + clean_name(f.filename) + "\" không phải ảnh JPG, PNG, GIF hoặc WEBP");
            std::string stored = crypto::random_hex(12) + "." + ext;
            std::filesystem::path path = std::filesystem::path(app.cfg.upload_dir) / stored;
            {
                std::ofstream o(path, std::ios::binary);
                if (!o) throw std::runtime_error("Không ghi được file vào " + path.string());
                o.write(f.content.data(), static_cast<std::streamsize>(f.content.size()));
            }
            std::string url = "/uploads/" + stored;
            app.db.execute("INSERT INTO uploads (user_id, file_name, stored_name, mime_type, size_bytes, url) VALUES (?,?,?,?,?,?)",
                           {u.id, clean_name(f.filename), stored, mime, static_cast<long long>(f.content.size()), url});
            out.push_back({{"uploadId", app.db.last_insert_id()}, {"url", url}, {"fileName", clean_name(f.filename)}, {"size", f.content.size()}});
        }
        res.status = 201;
        return json{{"files", out}};
    });
}
