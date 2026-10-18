#!/usr/bin/env python3
"""Tạo database/seed_vn_admin.sql (địa giới hành chính Việt Nam, CẢ hệ cũ lẫn mới) từ 2 bộ dữ liệu mở:

  --old  dvhcvn.json  của https://github.com/daohoangson/dvhcvn   (dữ liệu gốc của Tổng cục Thống kê, cập nhật 01/03/2025)  -> 63 tỉnh, quận/huyện, phường/xã (trước 01/07/2025)
  --new  simplified_json_generated_data_vn_units.json của
         https://github.com/thanglequoc/vietnamese-provinces-database (MIT, theo nghị quyết sáp nhập) -> 34 tỉnh/thành, phường/xã (sau sáp nhập)

Chạy:  python tools/make_admin_units.py --old dvhcvn.json --new simplified_json_generated_data_vn_units.json
(File seed_vn_admin.sql đã được tạo sẵn trong repo, chỉ chạy lại khi muốn cập nhật dữ liệu.)
"""
import argparse, json, os

ap = argparse.ArgumentParser()
ap.add_argument('--old', required=True)
ap.add_argument('--new', required=True)
a = ap.parse_args()
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, 'database', 'seed_vn_admin.sql')

def q(s):
    return "'" + str(s).replace("'", "''") + "'"

def short(full):
    for p in ('Thành phố ', 'Tỉnh '):
        if full.startswith(p):
            return full[len(p):]
    return full

old = json.load(open(a.old, encoding='utf-8'))['data']
new = json.load(open(a.new, encoding='utf-8'))
out = ['-- Địa giới hành chính Việt Nam (sinh tự động bởi tools/make_admin_units.py, đừng sửa tay)',
       '-- NEW: 34 tỉnh/thành + phường/xã (2 cấp, sau sáp nhập)   OLD: 63 tỉnh/thành + quận/huyện + phường/xã (3 cấp)',
       'BEGIN;']

def chunks(rows, head, n=400):
    for i in range(0, len(rows), n):
        out.append(head + '\n' + ',\n'.join(rows[i:i + n]) + ';')

# ---- NEW
prov_rows, ward_rows = [], []
pid = 0
new_ids = {}
for p in sorted(new, key=lambda x: x['Code']):
    pid += 1
    new_ids[p['Code']] = pid
    prov_rows.append(f"({pid}, 'NEW', {q(p['Code'])}, {q(p['Name'])}, {q(p['FullName'])})")
chunks(prov_rows, 'INSERT INTO provinces (province_id, adm_system, code, name, full_name) VALUES')
for p in sorted(new, key=lambda x: x['Code']):
    for w in sorted(p['Wards'], key=lambda x: x['FullName']):
        ward_rows.append(f"({new_ids[p['Code']]}, NULL, {q(w['Code'])}, {q(w['FullName'])})")
chunks(ward_rows, 'INSERT INTO wards (province_id, district_id, code, name) VALUES')
n_new_w = len(ward_rows)

# ---- OLD
prov_rows, dist_rows, ward_rows = [], [], []
pid = 100
did = 0
for p in sorted(old, key=lambda x: x['level1_id']):
    pid += 1
    prov_rows.append(f"({pid}, 'OLD', {q(p['level1_id'])}, {q(short(p['name']))}, {q(p['name'])})")
    for d in sorted(p['level2s'], key=lambda x: x['level2_id']):
        did += 1
        dist_rows.append(f"({did}, {pid}, {q(d['level2_id'])}, {q(d['name'])})")
        for w in sorted(d['level3s'], key=lambda x: x['level3_id']):
            ward_rows.append(f"({pid}, {did}, {q(w['level3_id'])}, {q(w['name'])})")
chunks(prov_rows, 'INSERT INTO provinces (province_id, adm_system, code, name, full_name) VALUES')
chunks(dist_rows, 'INSERT INTO districts (district_id, province_id, code, name) VALUES')
chunks(ward_rows, 'INSERT INTO wards (province_id, district_id, code, name) VALUES')
out.append('COMMIT;')
open(OUT, 'w', encoding='utf-8').write('\n'.join(out) + '\n')
print(f'NEW: {len(new)} tỉnh, {n_new_w} phường/xã | OLD: {len(old)} tỉnh, {len(dist_rows)} quận/huyện, {len(ward_rows)} phường/xã -> {OUT}')
