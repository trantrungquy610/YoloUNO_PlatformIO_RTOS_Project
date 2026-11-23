# WebServer cho YOLO UNO that

**Branch:** `feature/webserver-device`

**Co the nap len YOLO UNO:** Chua duoc nghiem thu tren board that. Day la phien ban firmware va WebServer duoc dung cho thiet bi, KHONG bao gom thu muc `mock/`.

## Co gi trong branch

- `src/task_webserver.cpp`: HTTP `GET /health`, `GET /api/sensors`, `GET /api/info`; WebSocket `/ws` tren cong `8080`.
- `src/task_handler.cpp`, `include/task_handler.h`: xu ly lenh dieu khien va phan hoi ACK dung client.
- `src/task_check_info.cpp`: xu ly luu cau hinh Wi-Fi/Core IoT vao LittleFS.
- `src/mainserver.cpp`: trang cau hinh Wi-Fi AP/STA.
- `data/`: giao dien WebServer duoc nap bang LittleFS.
- `test/web_ui_smoke.mjs`, `test/source_contract.test.mjs`: kiem thu tinh (khong can board).

## Lenh kiem tra

```powershell
node test/web_ui_smoke.mjs
node test/source_contract.test.mjs
& "$env:USERPROFILE\.platformio\penv\Scripts\platformio.exe" run -e yolo_uno
```

Khi co board, can upload firmware + LittleFS, kiem tra AP/STA, endpoint API, WebSocket, cam bien va luu cau hinh qua khoi dong lai.

## Gioi han va an toan

- `WEB_RELAY_GPIO` mac dinh `-1`: chua duoc phep dieu khien relay that. Chi cau hinh chan GPIO sau khi thanh vien phan cung xac nhan.
- Luu token Core IoT KHONG chung minh da ket noi MQTT/Core IoT thanh cong.
- Khong coi smoke tests va compile SUCCESS la kiem thu board that.
- WebServer chua co co che xac thuc day du cho cau hinh/dieu khien: chi demo tren LAN tin cay, khong public ra Internet.
- Khong commit Wi-Fi password/Token that vao Git.
- Branch nay duoc tao tu branch Wi-Fi AP/STA cua nhom, khong sua `main`.
