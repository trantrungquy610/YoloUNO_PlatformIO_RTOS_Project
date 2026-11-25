# YoloUNO ESP32-S3 Firmware

Firmware PlatformIO/Arduino cho board YoloUNO (ESP32-S3), chạy các FreeRTOS task cho cảm biến DHT20, Gesture-AI, RGB LED, quạt PWM, MQTT và WebServer.

## Tính năng

- Đọc nhiệt độ, độ ẩm từ DHT20; hiển thị trên LCD và cung cấp dữ liệu cho các task khác.
- Nhận ảnh grayscale 64 × 64 qua HTTP, chạy mô hình TensorFlow Lite Micro và điều khiển RGB LED theo cử chỉ.
- Publish nhiệt độ, độ ẩm, kết quả Gesture-AI và tốc độ quạt lên MQTT Gateway cục bộ.
- Nhận RPC `setFanSpeed` để đặt tốc độ quạt từ 0 đến 100%.
- Cung cấp trang thiết lập Wi-Fi trên cổng 80 và dashboard/API cục bộ trên cổng 8080.

## Cấu trúc chính

| Đường dẫn | Chức năng |
| --- | --- |
| `src/main.cpp` | Khởi tạo và tạo các FreeRTOS task |
| `src/gesture_inference.cpp`, `src/gesture_model_data.cpp` | Mô hình và suy luận Gesture-AI |
| `src/task_gesture.cpp` | Nhận frame, cập nhật kết quả và bộ đếm gesture |
| `src/task_fan.cpp` | Điều khiển quạt PWM qua hàng đợi lệnh |
| `src/task_mqtt_gateway.cpp` | MQTT telemetry và nhận RPC từ Gateway |
| `src/mainserver.cpp` | Trang thiết lập Wi-Fi trên cổng 80 |
| `src/task_webserver.cpp`, `src/task_handler.cpp` | Dashboard/API cục bộ và WebSocket trên cổng 8080 |
| `src/task_wifi.cpp`, `src/task_check_info.cpp` | Kết nối AP/STA và lưu cấu hình vào LittleFS |
| `data/` | Giao diện WebServer được nạp vào LittleFS |
| `boards/yolo_uno.json` | Định nghĩa board YoloUNO ESP32-S3 cho PlatformIO |

## Build và nạp firmware

Cài PlatformIO Core hoặc PlatformIO IDE, sau đó chạy tại thư mục repo:

```sh
pio run -e yolo_uno
pio run -e yolo_uno -t upload
pio run -e yolo_uno -t uploadfs
pio device monitor -b 115200
```

`uploadfs` nạp giao diện trong `data/` vào LittleFS. Khi nạp firmware và filesystem, chọn đúng cổng serial của board. Thiết lập board, thư viện và build flags nằm trong `platformio.ini`.

## Wi-Fi và WebServer

Khi chưa có Wi-Fi đã lưu, thiết bị tạo access point với SSID và mật khẩu đặt trong `build_flags` của `platformio.ini`. Giá trị hiện tại là cấu hình phát triển; hãy đổi mật khẩu trước khi dùng trên mạng dùng chung. Khi đã có Wi-Fi lưu trong LittleFS, thiết bị thử kết nối STA và quay lại AP nếu kết nối thất bại.

- **Cổng 80:** trang chính và thiết lập Wi-Fi (`/settings`); gửi thông tin kết nối qua `/connect` và kiểm tra kết quả tại `/connect-status`.
- **Cổng 8080:** dashboard từ LittleFS (`/`), `/health`, `/api/sensors`, `/api/info`, WebSocket `/ws` và endpoint Gesture-AI `POST /api/gesture/frame`.
- `/api/gesture/frame` yêu cầu `Content-Type: application/octet-stream` và đúng 4096 byte grayscale (64 × 64). Trả `202` khi frame được đưa vào hàng đợi; trả lỗi nếu frame không hợp lệ hoặc queue chưa sẵn sàng.

WebServer hiện không có xác thực đầy đủ. Chỉ dùng trên mạng tin cậy, không mở cổng ra Internet. OTA bị tắt mặc định; chỉ bật sau khi đã đánh giá bảo mật và cấu hình xác thực phù hợp.

Relay Web bị vô hiệu hóa mặc định (`WEB_RELAY_GPIO = -1`). Không bật relay hoặc nối tải vào GPIO trước khi xác nhận sơ đồ chân, mạch driver và cực tính phần cứng. Quạt phải dùng mạch công suất/driver phù hợp, không cấp nguồn trực tiếp từ GPIO ESP32.

## MQTT và CoreIoT Gateway

Firmware publish theo chu kỳ 2 giây tới broker được cấu hình trong `src/task_mqtt_gateway.cpp`:

```text
yolouno/<DEVICE_ID>/telemetry
yolouno/<DEVICE_ID>/status
yolouno/<DEVICE_ID>/command
```

Telemetry gồm:

| Key | Ý nghĩa |
| --- | --- |
| `temperature`, `humidity` | Chỉ được gửi khi mẫu DHT20 hợp lệ |
| `gesture` | Nhãn `fist`, `no_hand`, `open_palm`, `thumbs_up`, `victory` hoặc `unknown` |
| `confidence` | Độ tin cậy của lần suy luận gần nhất, khoảng 0–1 |
| `inference_ms` | Thời gian suy luận, mili giây |
| `gesture_count` | Số lần chuyển vào một gesture hành động tin cậy; được mở khóa lại bởi `no_hand` hoặc `unknown` |
| `fan_speed_percent` | Giá trị tốc độ PWM đã đặt, không phải số đo vòng quay thực tế |

Điều khiển quạt bằng RPC một chiều:

```json
{"method":"setFanSpeed","params":75}
```

`params` phải là số nguyên từ 0 đến 100. Firmware hiện cấu hình broker và `DEVICE_ID` trực tiếp trong `src/task_mqtt_gateway.cpp`; giá trị đang có là broker `192.168.45.124` và thiết bị `esp32_02`. Hãy thay chúng cho đúng mạng, broker và thiết bị trước khi build từng board. Các trường CoreIoT server/token trong trang thiết lập được lưu trong LittleFS nhưng hiện không tự thay thế cấu hình broker MQTT này.

## Kiểm tra

```sh
node test/web_ui_smoke.mjs
node test/source_contract.test.mjs
pio run -e yolo_uno
```

Hai script Node kiểm tra tĩnh giao diện và hợp đồng mã nguồn; chúng không thay thế build firmware hoặc kiểm tra trên board. Sau khi nạp, cần kiểm tra AP/STA, các API, WebSocket, DHT20, endpoint frame, MQTT và điều khiển quạt trên phần cứng.
