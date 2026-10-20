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
   - Nhận diện các thế ngón tay: Bật đèn, Tắt đèn, Tăng/Giảm độ sáng 15% mỗi bước, Xoay tay đổi màu đèn.

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

## 🔌 Sơ đồ đấu nối phần cứng (Wokwi Simulation)

| Linh kiện | Chân linh kiện | Chân kết nối ESP32 | Ghi chú |
| :--- | :--- | :--- | :--- |
| **NeoPixel Ring (16 LED WS2812B)** | `VCC` | `VIN` (5V) | Nguồn dương |
| | `GND` | `GND` | Mass chung |
| | `DIN` | `GPIO 18` | Tín hiệu điều khiển dữ liệu |
| **Cảm biến quang trở LDR Module** | `VCC` | `3V3` | Nguồn 3.3V |
| | `GND` | `GND` | Mass chung |
| | `AO` | `GPIO 34` | Đọc tín hiệu Analog (ADC1_CH6) |
| **Nút bấm vật lý (Gesture Sim)** | Chân 1 | `GND` | Mass chung |
| | Chân 2 | `GPIO 12` | Kích hoạt điện trở kéo nội `INPUT_PULLUP` |

---

## 🖐️ Bảng ánh xạ Cử chỉ bàn tay (Hand Gestures)

| Cử chỉ | Thế ngón tay nhận diện | Hành vi hệ thống |
| :---: | :--- | :--- |
| ☝️ | Giơ 1 ngón trỏ | **Bật đèn** (Khôi phục mức sáng ghi nhớ) |
| ✌️ | Giơ 2 ngón tay (Chữ V) | **Tắt đèn** |
| 🤟 | Giơ 3 ngón tay | **Tăng độ sáng** thêm +15% |
| 🖖 | Giơ 4 ngón tay | **Giảm độ sáng** -15% (dưới 15% tự tắt) |
| 🖐️ | Xòe 5 ngón / Xoay tay | **Đổi màu đèn** xoay vòng qua 8 gam màu |

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
