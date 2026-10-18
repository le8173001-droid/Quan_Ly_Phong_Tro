# Build
FROM debian:12-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends g++ cmake ninja-build libsqlite3-dev ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /src
COPY . .
RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build --target nhatro

# Run
FROM debian:12-slim
RUN apt-get update && apt-get install -y --no-install-recommends libsqlite3-0 curl ca-certificates && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY --from=build /src/build/nhatro /app/nhatro
COPY public /app/public
COPY thanh-vien-1-tai-khoan-toa-nha/web /app/thanh-vien-1-tai-khoan-toa-nha/web
COPY thanh-vien-2-loai-phong-phong/web /app/thanh-vien-2-loai-phong-phong/web
COPY thanh-vien-3-nguoi-thue-hop-dong/web /app/thanh-vien-3-nguoi-thue-hop-dong/web
COPY thanh-vien-4-dien-nuoc-hoa-don/web /app/thanh-vien-4-dien-nuoc-hoa-don/web
COPY thanh-vien-5-cong-khach-ticket-dashboard/web /app/thanh-vien-5-cong-khach-ticket-dashboard/web
COPY database /app/database
ENV PORT=8080 HOST=0.0.0.0 DB_PATH=/data/nhatro.db WEB_DIR=/app/public
VOLUME /data
EXPOSE 8080
CMD ["/app/nhatro"]
