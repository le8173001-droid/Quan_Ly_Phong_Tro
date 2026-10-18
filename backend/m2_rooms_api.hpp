// Truy vấn phòng dùng chung cho tìm kiếm, trang chủ, yêu thích, khuyến mãi.
#pragma once

#include "app.hpp"

#include <optional>
#include <utility>
#include <vector>

// Điều kiện lọc: ghép bằng AND. Tham số được bind theo đúng thứ tự xuất hiện.
struct Where {
    std::vector<std::string> conds;
    std::vector<json> params;
    void add(const std::string& cond) { conds.push_back(cond); }
    void add(const std::string& cond, json p) {
        conds.push_back(cond);
        params.push_back(std::move(p));
    }
    std::string sql() const;
};

// Điều kiện của trang công khai: chỉ phòng còn chỗ
inline const char* kPublicRoomCondition = "r.status = 'available' AND r.current_occupants < r.max_occupants";

struct RoomFetch {
    Where where;
    std::string order_by = "r.create_date DESC, r.room_id DESC";
    long long limit = 12;
    long long offset = 0;
    long long user_id = 0;                                   // >0: thêm cờ isFavorite
    std::optional<std::pair<double, double>> near_pt;           // (lat, lng): thêm distanceKm
};

json fetch_rooms(App& app, const RoomFetch& f);
long long count_rooms(App& app, const Where& w);

// Thêm các trường tính sẵn (price, originalPrice, discountPercent, fullAddress, occupancyState...)
void decorate_room(App& app, json& room);

// Các mục "nổi bật" trên trang chủ (theo cấu hình featured_mode / featured_count / featured_types)
json featured_sections_public(App& app, long long user_id);
