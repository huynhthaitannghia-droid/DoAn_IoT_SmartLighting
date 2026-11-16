# HỆ THỐNG ĐIỀU KHIỂN ĐÈN THÔNG MINH QUA GIỌNG NÓI VÀ CỬ CHỈ (AIoT SMART LIGHTING)

> **Đồ án môn học:** Internet of Things (IoT)  
> **Trường:** Đại học Công Thương TP. Hồ Chí Minh (HUIT) - Khoa Công nghệ Thông tin  
> **Giảng viên hướng dẫn:** ThS. Đinh Huy Hoàng  
> **Nhóm thực hiện:** Nhóm 15  
> - Huỳnh Thái Tấn Nghĩa (Nhóm trưởng) - MSSV: 2001230556  
> - Bùi Quốc Toàn - MSSV: 2001230823  
> - Bùi Quang Tiến - MSSV: 2001230807  

---

## 🌟 Giới thiệu tổng quan

Hệ thống **AIoT Smart Lighting** là giải pháp chiếu sáng thông minh không chạm (Touchless / Hands-free) kết hợp giữa Trí tuệ nhân tạo (AI) và Internet vạn vật (IoT). Hệ thống cho phép điều khiển linh hoạt qua cử chỉ bàn tay (Computer Vision AI), giọng nói tiếng Việt tự nhiên (Web Speech), giao diện Web Dashboard thời gian thực và tự động hóa theo cảm biến ánh sáng môi trường.

Đặc biệt, hệ thống tích hợp **Mô hình phân cấp ưu tiên 4 tầng (Conflict Resolution Algorithm)** và **Thuật toán Hysteresis chống rung quang học** giúp điều phối lệnh điều khiển mượt mà, giải quyết triệt để xung đột tài nguyên.

---

## 🚀 Các tính năng nổi bật

1. **Nhận diện cử chỉ AI qua Camera (Google MediaPipe Hands):**
   - Trích xuất 21 điểm mốc không gian (3D Landmarks) của bàn tay theo thời gian thực (30-60 FPS).
   - Nhận diện các thế ngón tay chuẩn xác: Xòe tay (Bật), Nắm tay (Tắt), 1 ngón trỏ (Đổi màu), 2 ngón (Tăng sáng 15%), 3 ngón (Giảm sáng 15%).

2. **Điều khiển giọng nói tiếng Việt tự nhiên (Web Speech API):**
   - Xử lý ngôn ngữ tự nhiên tiếng Việt (`vi-VN`) trực tiếp trên Web Dashboard: *"Bật đèn"*, *"Tắt đèn"*, *"Tăng sáng"*, *"Giảm sáng"*, *"Ánh sáng ấm"*, *"Màu đỏ"*, v.v.

3. **Giao diện Web Dashboard thời gian thực (Real-time):**
   - Kết nối hai chiều qua giao thức **MQTT over WebSockets** (`broker.hivemq.com`).
   - Bảng màu RGB HTML5 tương tác với hơn **16.7 triệu màu**.
   - Thanh trượt độ sáng (Dimming UX) thông minh với khả năng ghi nhớ mức sáng khi tắt đèn.

4. **Tự động hóa cảm biến ánh sáng LDR với bộ lọc Hysteresis:**
   - Đọc giá trị Analog quang trở LDR (12-bit ADC).
   - Vùng đệm trễ 800 đơn vị ADC ngăn chặn hiện tượng đèn chập chờn khi ánh sáng môi trường dao động ở ngưỡng chuyển tiếp.

5. **Thuật toán điều phối & xử lý xung đột 4 tầng (Conflict Resolution):**
   - **Tầng 1 (Ưu tiên cao nhất):** Lệnh Khóa khẩn cấp từ Ứng dụng (App Emergency Lock).
   - **Tầng 2:** Lệnh Giọng nói từ người dùng (Voice Command).
   - **Tầng 3:** Lệnh Cử chỉ bàn tay AI / Nút bấm phần cứng (Gesture Command).
   - **Tầng 4:** Tự động hóa cảm biến môi trường (LDR Sensor Automation).
   *Cơ chế `USER_OVERRIDE_HOLD_MS = 15000` tự động khóa tạm thời cảm biến LDR trong 15 giây khi người dùng vừa chủ động điều khiển.*

---

## 🛠️ Cấu trúc thư mục dự án

```text
.
├── src/
│   └── main.cpp             # Firmware chính cho ESP32 (C++ PlatformIO)
├── dashboard.html           # Web Dashboard AIoT (MediaPipe AI + Web Speech + MQTT)
├── diagram.json             # Sơ đồ mạch mô phỏng linh kiện trên Wokwi
├── wokwi.toml               # Cấu hình biên dịch và mô phỏng Wokwi
├── platformio.ini           # Cấu hình thư viện và board ESP32 (PlatformIO)
├── .gitignore               # Bộ lọc tệp git chuẩn
└── README.md                # Tài liệu hướng dẫn dự án
```

---

## Khác biệt với cấu hình tham chiếu của đề kiểm tra

| Hạng mục | Đề kiểm tra | Bản mô phỏng hiện tại |
|---|---|---|
| Cử chỉ | Cảm biến APDS-9960 qua I²C | Camera và MediaPipe trên dashboard. Nút GPIO 12 tạo lệnh cử chỉ cục bộ để kiểm tra firmware. |
| Giọng nói | Micro INMP441 qua I²S | Web Speech API trên trình duyệt. Firmware có nhánh nhận mã lệnh UART dành cho Elechouse V3. |
| Tải đèn | Relay/Triac điều khiển đèn 220 V | Vòng LED NeoPixel trên GPIO 18. |
| Dịch vụ mạng | Kiến trúc Cloud theo đề | Broker HiveMQ công cộng. Backend riêng, xác thực và cơ sở dữ liệu lịch sử là phần mở rộng. |

Ở câu 2A, phương án lọc nhiễu quang từ đèn huỳnh quang áp dụng cho APDS-9960 theo giả thiết của đề. Bản Wokwi dùng nút GPIO 12 để minh họa lọc dội và loại xung ngắn trong firmware. Kiểm thử nhiễu quang cần thực hiện trên APDS-9960 thật.

## 🔌 Sơ đồ đấu nối phần cứng (Wokwi Simulation)

| Linh kiện | Chân linh kiện | Chân kết nối ESP32 | Ghi chú |
| :--- | :--- | :--- | :--- |
| **NeoPixel Ring (16 LED WS2812B)** | `VCC` | `VIN` (5V) | Nguồn dương |
| | `GND` | `GND` | Mass chung |
| | `DIN` | `GPIO 18` | Tín hiệu điều khiển dữ liệu |
| **Cảm biến quang trở LDR Module** | `VCC` | `3V3` | Nguồn 3.3V |
| | `GND` | `GND` | Mass chung |
| | `AO` | `GPIO 34` | Đọc tín hiệu Analog (ADC1_CH6) |
| **Nút bấm vật lý (Mô phỏng cử chỉ)** | Chân 1 | `GND` | Mass chung |
| | Chân 2 | `GPIO 12` | Kích hoạt điện trở kéo nội `INPUT_PULLUP` |

---

## 🖐️ Bảng điều khiển bằng Cử chỉ bàn tay (Camera AI MediaPipe)

| Cử chỉ | Thế ngón tay nhận diện | Lệnh MQTT | Hành vi hệ thống |
| :---: | :--- | :---: | :--- |
| ✋ | **Xòe bàn tay** (Mở từ 4 đến 5 ngón) | `ON` | **Bật đèn** (Khôi phục mức sáng ghi nhớ) |
| ✊ | **Nắm bàn tay** (0 ngón mở) | `OFF` | **Tắt đèn** hoàn toàn |
| ☝️ | **1 ngón trỏ** dựng thẳng | `ROTATE` | **Đổi màu tiếp theo** (Xoay vòng 8 gam màu) |
| ✌️ | **2 ngón tay** (Ngón trỏ + Ngón giữa) | `SWIPE_RIGHT` | **Tăng độ sáng** (+15% mỗi bước) |
| 🤟 | **3 ngón tay** (Ngón trỏ + Giữa + Áp út) | `SWIPE_LEFT` | **Giảm độ sáng** (-15% mỗi bước, dưới 15% tự động tắt) |

---

## 🎙️ Bảng lệnh Điều khiển Giọng nói tiếng Việt (Web Speech API)

| Lệnh giọng nói (Mẫu câu nhận diện) | Hành vi hệ thống |
| :--- | :--- |
| *"Bật đèn"*, *"Mở đèn"*, *"Sáng lên"* | **Bật đèn** (Khôi phục mức sáng ghi nhớ) |
| *"Tắt đèn"*, *"Tắt đi"*, *"Đi ngủ"* | **Tắt đèn** hoàn toàn |
| *"Sáng hơn"*, *"Tăng sáng"*, *"Tăng độ sáng"* | **Tăng độ sáng** thêm +15% |
| *"Tối hơn"*, *"Giảm sáng"*, *"Giảm độ sáng"* | **Giảm độ sáng** -15% (dưới 15% tự tắt) |
| *"Đổi màu"*, *"Chuyển màu"* | **Xoay vòng đổi màu** qua 8 gam màu |
| *"Màu đỏ"*, *"Màu xanh lá"*, *"Màu xanh dương"*, *"Ánh sáng ấm"*... | **Chuyển trực tiếp sang màu tương ứng** |

---

## 💻 Hướng dẫn chạy dự án

### 1. Mô phỏng ESP32 trên Wokwi
- Cài đặt extension **Wokwi Simulator** trong VS Code hoặc mở trực tiếp trên [Wokwi.com](https://wokwi.com).
- Biên dịch mã nguồn qua PlatformIO:
  ```bash
  pio run
  ```
- Nhấn phím `F1` trong VS Code $\rightarrow$ chọn **Wokwi: Start Simulator**.

### 2. Mở Web Dashboard AIoT
- Mở file `dashboard.html` trực tiếp bằng trình duyệt Google Chrome hoặc Microsoft Edge (khuyến nghị dùng Live Server).
- Cấp quyền truy cập **Camera** (để chạy MediaPipe Hands AI) và **Microphone** (để nhận diện giọng nói tiếng Việt).
- Web Dashboard sẽ tự động kết nối vào MQTT Broker `broker.hivemq.com` và đồng bộ tức thời với ESP32!

---

### 3. Demo broker MQTT nội bộ (câu 2B)

Trong thư mục dự án, chạy `npm.cmd ci` một lần để cài thư viện qua Internet. Sau đó chạy `npm.cmd run broker` và giữ terminal này mở. Broker lắng nghe MQTT TCP trên `127.0.0.1:1883`, còn dashboard và MQTT WebSocket dùng cổng `8080`.

1. Mở dashboard tại `http://127.0.0.1:8080/` và đăng nhập bằng mật khẩu in khi khởi động broker. Trang này tải thư viện MQTT từ máy chạy broker.
2. Build firmware, khởi động Wokwi trong VS Code và gõ `MQTT LOCAL` trong Serial Monitor. ESP32 dùng `host.wokwi.internal:1883` qua Wokwi IoT Gateway. Nếu trước đó đã gõ `MQTT OFF`, gõ thêm `MQTT ON`.
3. Chờ log `THANH CONG!` sau dòng kết nối đến `host.wokwi.internal`. Bấm **BẬT ĐÈN** và **TẮT ĐÈN** trên dashboard, quan sát vòng LED trong Wokwi.
4. Sau khi bản demo hoạt động, có thể thử ngắt đường Internet ở router nhưng giữ Wi-Fi/LAN và broker đang chạy. Chế độ khởi động Wokwi hoàn toàn offline yêu cầu gói Pro. [Wokwi Offline](https://docs.wokwi.com/vscode/offline-mode)

Dashboard và broker TCP của bản demo chỉ nghe trên loopback. Wokwi kết nối qua gateway trên cùng máy.

Các nút web và điều khiển độ sáng hoạt động với broker nội bộ. Camera hiện tải MediaPipe từ CDN khi bật lần đầu và nhận diện giọng nói vẫn tùy dịch vụ của trình duyệt. Để trở về luồng HiveMQ, gõ `MQTT PUBLIC` và mở lại trang `dashboard.html` qua Live Server.

### 4. Xem lịch sử bật/tắt đèn (câu 2C)

Chạy broker nội bộ bằng Node.js 24 và giữ ESP32 ở chế độ `MQTT LOCAL`. Mở dashboard nội bộ, bấm **BẬT ĐÈN** và **TẮT ĐÈN**, rồi xem mục **Lịch sử bật/tắt đèn** bên dưới nhật ký hoạt động. Danh sách tự cập nhật mỗi 3 giây và hiện tối đa 20 bản ghi mới nhất.

Backend lưu trạng thái ban đầu và các lần chuyển `ON`/`OFF` nhận được từ topic trạng thái vào `data/smartlight-history.sqlite`. Thời gian hiển thị là lúc backend nhận bản tin. File được giữ khi khởi động lại broker. Những thay đổi diễn ra lúc ESP32 ngắt broker có thể không được ghi vì firmware chỉ đồng bộ trạng thái mới nhất khi kết nối lại.

### 5. Đo độ trễ phản hồi ESP32 (câu 2C)

Build và nạp firmware mới, chuyển sang `MQTT LOCAL` rồi mở dashboard nội bộ. Bấm **BẬT ĐÈN** hoặc **TẮT ĐÈN** để đo một lần, hoặc chọn **Đo 20 lần bật/tắt** để chạy các lệnh luân phiên. Dashboard hiển thị số lần được xác nhận, số mẫu dưới 200 ms và trung bình của các mẫu đã nhận. Nếu mất kết nối hoặc một lệnh quá 2,5 giây chưa có xác nhận, lượt đo kết thúc và hiển thị lý do.

Dashboard gắn `commandId` cho từng lệnh. ESP32 phản hồi mã lệnh và trạng thái trên `nhom15/smartlight/ack` sau khi cập nhật LED. Dashboard dùng `performance.now()` để đo từ lúc gửi đến lúc nhận đúng xác nhận. Chỉ số này là độ trễ khứ hồi qua MQTT, bao gồm thời gian xử lý ở ESP32.

### 6. Đăng nhập dashboard nội bộ (câu 2C)

Chạy `npm.cmd ci` để cài thư viện JWT, rồi khởi động lại bằng `npm.cmd run broker`. Mật khẩu ngẫu nhiên xuất hiện trong terminal cho mỗi lần khởi động, trừ khi đặt biến môi trường `SMARTLIGHT_PASSWORD` trước khi chạy. Không đưa mật khẩu vào ảnh demo hoặc mã nguồn.

Sau khi đăng nhập, trình duyệt nhận JWT trong cookie HttpOnly, hết hạn sau 30 phút. Backend kiểm tra phiên khi mở dashboard, đọc lịch sử và bắt đầu kết nối MQTT WebSocket. Đăng xuất hủy phiên và đóng kết nối WebSocket của phiên đó. Dashboard được gửi lệnh điều khiển và đọc trạng thái/xác nhận. ESP32 được nhận lệnh và gửi trạng thái/xác nhận qua cổng TCP loopback. Đây là xác thực cục bộ bằng JWT; nhà cung cấp OAuth2, TLS 1.3 và xác thực riêng cho thiết bị là các phần triển khai tiếp theo.

## 📡 MQTT Topics

- **Topic Điều khiển:** `nhom15/smartlight/control`
- **Topic Trạng thái:** `nhom15/smartlight/status`

*Payload JSON ví dụ:*
```json
{
  "state": "ON",
  "brightness": 70,
  "r": 255,
  "g": 180,
  "b": 50,
  "priority": 2,
  "source": "GIỌNG_NÓI",
  "emergency": false
}
```

---

## 📜 Bản quyền & Giấy phép
Đồ án thuộc bản quyền học thuật của **Nhóm 15 - Khoa Công nghệ Thông tin - Trường Đại học Công Thương TP.HCM (HUIT)**.
