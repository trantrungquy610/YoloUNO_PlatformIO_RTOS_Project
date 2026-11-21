# PHẦN 1 — WebServer YOLO UNO (cổng 8080)

## Phạm vi thay đổi
- `data/index.html`, `data/styles.css`, `data/script.js`: FE hiển thị dữ liệu cảm biến thật; trạng thái kết nối; thông tin thiết bị; phản hồi lệnh; không dùng CDN để hoạt động ở Wi-Fi AP không Internet.
- `src/task_webserver.cpp`: hai API đọc **chỉ xem** `/api/sensors`, `/api/info`, WebSocket giữ nguyên `/ws`, phản hồi JSON và cơ chế khởi động lại sau lưu.
- `src/task_handler.cpp`, `include/task_handler.h`: kiểm tra dữ liệu WebSocket, chỉ cho bật/tắt relay khi chân GPIO đã được thành viên phần cứng cho phép, gửi ACK cho các lệnh, lưu cấu hình vào LittleFS trước khi thông báo thành công.
- `test/web_ui_smoke.mjs`: kiểm thử JS bằng dữ liệu giả lập, không thay thế test trên thiết bị thật.

**Không thay đổi** task đọc cảm biến, task MQTT, task quạt, task RGB, MainServer cổng 80, hoặc phần cứng.

## Cách nạp lên YOLO UNO bằng VS Code / PlatformIO
1. Sao lưu project và thông tin Wi-Fi/Core IoT trước khi thao tác. Đóng VS Code nếu đang mở project cũ; mở thư mục `YoloUNO_PlatformIO_RTOS_Project` chứa `platformio.ini` trong VS Code.
2. Cắm bo YOLO UNO bằng USB. Cài extension PlatformIO IDE và chờ dự án có thể build.
3. Terminal ở **thư mục chứa `platformio.ini`**:

```powershell
pio run -e yolo_uno
pio run -e yolo_uno -t upload
pio run -e yolo_uno -t uploadfs
pio device monitor -b 115200
```

**Lưu ý:** `uploadfs` có thể ghi đè phân vùng LittleFS, làm mất cấu hình Wi-Fi/Core IoT đang lưu trong `/info.dat`. Hãy sao lưu và chuẩn bị nhập lại. Nếu `pio` không được nhận trong PowerShell, dùng **PlatformIO → Project Tasks → yolo_uno → Build, Upload, Upload Filesystem Image, Monitor** trong VS Code. Khi Monitor chiếm cổng COM, tắt Monitor trước khi upload. Đôi khi cần nhấn RESET sau nạp.

4. Theo dõi Serial Monitor để lấy IP thực của bo. Nếu không có cấu hình Wi-Fi, firmware có thể bật AP theo `platformio.ini`: SSID `ESP32 TEAM 007`, mật khẩu mặc định `12345678`. Kết nối máy tính/điện thoại vào AP này. IP AP ESP32 thường là `192.168.4.1`, **hãy ưu tiên IP trong Serial Monitor**.
5. Mở `http://<IP-CỦA-BO>:8080/` (**không dùng cổng 80**). Ví dụ: `http://192.168.4.1:8080/`.

## Test 1 — Dữ liệu DHT20 THẬT
1. Trên cùng Wi-Fi, mở `http://<IP>:8080/api/sensors`.
2. Đáp án mẫu về **cấu trúc** (không phải giá trị mong đợi cố định):

```json
{"valid": true, "sensor_error": false, "temperature": 27.2, "humidity": 66.5, "uptime_ms": 12345}
```

3. Mở trang Chủ ở cổng 8080. Nhiệt độ/độ ẩm hiển thị theo dữ liệu JSON và cập nhật sau mỗi 3 giây. Chạm/ấm nhẹ cảm biến DHT20 (tránh làm hỏng phần cứng), đợi dữ liệu biến đổi và đối chiếu với LCD hoặc Serial.
4. Nếu `valid:false` hoặc `sensor_error:true`, giao diện phải hiển thị `—` và cảnh báo; **không tự sinh số**. Nếu API trả 404, firmware mới chưa nạp đúng. Nếu dashboard chưa đổi nhưng API đã đổi, mở F12 → Network, kiểm tra `/api/sensors` HTTP 200 và tải lại Ctrl+F5.

## Test 2 — Trạng thái và thông tin thiết bị
- Mở `http://<IP>:8080/health` → `AsyncWebServer is working!`.
- Mở `http://<IP>:8080/api/info`. Phải có các khóa `wifi_mode`, `wifi_connected`, `ip`, `uptime_seconds`, `core_iot_configured`, `relay_gpio`.
- Chọn mục **Thông tin**; đối chiếu IP, mode Wi-Fi, uptime với JSON. Token, mật khẩu **không được xuất hiện** trong API.
- Rút nguồn/đổi mạng để kiểm tra UI hiển thị mất kết nối; sau khi mạng/thiết bị hoạt động lại, WebSocket tự thử kết nối lại khoảng 2,5 giây/lần. **Không rút nguồn trong lúc bo đang ghi flash.**

## Test 3 — Điều khiển relay: CẦN XÁC NHẬN PHẦN CỨNG
**Mặc định WEB_RELAY_GPIO = -1, cố ý tắt lệnh relay để không ghi nhầm vào GPIO dành cho quạt, RGB, RS485, I2C, boot, USB, LED.** Mục Thiết bị phải hiển thị "Relay Web chưa được cấu hình". Đây là kết quả đúng khi chưa có sơ đồ đấu nối xác thực.

Chỉ sau khi thành viên phần cứng xác nhận một GPIO output **trống** đã nối vào relay điện áp thấp và chưa được bất cứ task nào dùng:
1. Thêm vào `platformio.ini` dưới `build_flags`: `-DWEB_RELAY_GPIO=<CHÂN_ĐÃ_XÁC_NHẬN>` (thay bằng số chân thực, tuyệt đối không nhập nguyên chữ `<...>`).
2. Nạp lại firmware (`pio run -t upload`).
3. Mở `/api/info`, xác nhận `relay_gpio` đúng. Vào tab Thiết bị; nút Bật/Tắt chỉ hoạt động khi WebSocket kết nối.
4. Bật → UI chờ phản hồi `device_result status:ok`, sau đó hiện mức HIGH. Quan sát **thiết bị vật lý** và log Serial để chắc chắn relay thực sự hoạt động. Tắt → trở lại LOW.
5. Ngắt Wi-Fi, nút phải bị khóa; không được tự đổi trạng thái khi không nhận ACK. Nếu GPIO trong firmware nằm trong danh sách cấm, `/api/info` trả `relay_gpio:-1`.

**Chú ý**: Một số module relay kích mức LOW, cần thống nhất phần cứng/logic điều khiển trước khi demo; chỉ ACK ghi mức GPIO, không đảm bảo điện tải đã đóng/ngắt. KHÔNG test điện lưới.

## Test 4 — Lưu cấu hình Wi-Fi / Core IoT
1. Chuẩn bị SSID, mật khẩu Wi-Fi 2.4 GHz, token Core IoT, server và port **do nhóm cung cấp** (không đoán giá trị). Dự phòng khả năng mất kết nối AP khi chuyển STA.
2. Vào **Cài đặt** → điền thông số → **Lưu cấu hình**.
3. UI phải hiển thị "đang đợi thiết bị xác nhận". Chỉ khi ESP32 ghi LittleFS thành công, UI mới nhận `setting_saved status:ok`.
4. Sau khoảng 2,5 giây ESP32 tự khởi động lại. Kết nối lại theo IP STA mới trong Serial Monitor. Mở `/api/info` và xem `core_iot_configured` thành `true`, đồng thời kiểm tra bo kết nối mạng.
5. Nhập `port` không hợp lệ (0 hoặc >65535) → form phải chặn. Khi mất WebSocket, UI phải báo chưa gửi/không được xác nhận. Không được tuyên bố 'kết nối Core IoT thành công' chỉ dựa vào việc lưu token.
6. **Mật khẩu và token nhạy cảm**: không chụp ảnh màn hình có các thông tin này, không dán lên nhóm chat, không đưa vào GitHub.

## Test 5 — Kiểm tra JS tự động trên máy tính (không cần bo)
Yêu cầu Node.js 18+ (ưu tiên 20/22). Trong thư mục project:

```powershell
node --check data/script.js
node test/web_ui_smoke.mjs
```

Kỳ vọng dòng `PASS: DOM references, real-sensor flow, invalid sensor, relay ACK, settings ACK, offline/reconnect status`.

## Khi bàn giao lead
- Ảnh chụp trang Chủ và `/api/sensors` cho thấy giá trị giống nhau.
- Ảnh tab Thông tin, trạng thái WebSocket và trường cấu hình Core IoT (không lộ token).
- Video relay trên phần cứng nếu GPIO đã được xác nhận, hoặc báo rõ "chờ sơ đồ chân relay".
- Log test gồm: test JS PASS, nạp firmware PASS, nạp LittleFS PASS, sensor PASS, cấu hình PASS. Nếu chưa thử trên bo, ghi "chưa test thiết bị thật", không đánh dấu PASS.

## Giới hạn và an toàn
- Firmware phần này đã được thay đổi nhỏ để cung cấp API, ACK và bảo vệ GPIO; cần review/cherry-pick với team firmware trước khi merge.
- Ở lần nạp mới, hệ thống vẫn chạy 2 webserver: `80` (MainServer gốc), `8080` (FE sửa). Không nhập URL cổng 80 để test FE.
- Máy chủ này chưa có cơ chế xác thực HTTP/WebSocket; **chỉ chạy trên mạng nội bộ tin cậy, không mở port ra Internet**.
- Đã kiểm thử JS giả lập; **chưa có phần cứng để xác nhận build PlatformIO, nạp board, đọc cảm biến/relay thực tế**.
