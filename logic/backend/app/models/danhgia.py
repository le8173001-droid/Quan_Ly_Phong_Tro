from datetime import datetime
from sqlalchemy import Column, Integer, SmallInteger, Text, Boolean, ForeignKey, DateTime
from sqlalchemy.orm import relationship
from app.database import Base

class DanhGia(Base):
    __tablename__ = "danh_gia"

    id = Column(Integer, primary_key=True)
    nguoi_dung_id = Column(Integer, ForeignKey("nguoi_dung.id"), nullable=False)
    bac_si_id = Column(Integer, ForeignKey("bac_si.id"), nullable=False)
    lich_hen_id = Column(Integer, ForeignKey("lich_hen.id"))
    diem = Column(SmallInteger, nullable=False)  # 1 → 5 sao
    nhan_xet = Column(Text)
    hien_thi = Column(Boolean, default=True)
    created_at = Column(DateTime, default=datetime.utcnow)

    # Liên kết
    nguoi_dung = relationship("NguoiDung")
    bac_si = relationship("BacSi")