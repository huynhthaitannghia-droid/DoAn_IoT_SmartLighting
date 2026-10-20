#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <Adafruit_NeoPixel.h>
#include <ArduinoJson.h>

// ==============================================================================
// ĐỒ ÁN MÔN HỌC: INTERNET OF THINGS (IoT)
// ĐỀ TÀI: HỆ THỐNG ĐIỀU KHIỂN ĐÈN THÔNG MINH QUA GIỌNG NÓI VÀ CỬ CHỈ
// Nhóm 15: Huỳnh Thái Tấn Nghĩa | Bùi Quốc Toàn | Bùi Quang Tiến
// GVHD: ThS. Đinh Huy Hoàng - Khoa CNTT, ĐH Công Thương TP.HCM
// ==============================================================================

// ------------------------------------------------------------------------------
// 1. ĐỊNH NGHĨA CHÂN PHẦN CỨNG & THÔNG SỐ (THEO CHƯƠNG 2 & 3 BÁO CÁO)
// ------------------------------------------------------------------------------
#define LED_PIN          18       // Chân DIN điều khiển vòng LED WS2812B
#define NUM_LEDS         16       // 16 bóng LED NeoPixel RGB
#define LDR_PIN          34       // Chân ADC1 đọc cảm biến quang trở LDR
#define GESTURE_PIN      12       // Chân nút bấm mô phỏng cử chỉ vật lý (Tap / Rotate)
#define VOICE_RX2_PIN    16       // Chân RX2 nhận UART từ module giọng nói Elechouse V3
#define VOICE_TX2_PIN    17       // Chân TX2 truyền UART sang module giọng nói

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Cấu hình Wi-Fi & MQTT Broker
const char* ssid        = "Wokwi-GUEST";
const char* password    = "";
const char* mqtt_server = "broker.hivemq.com";
const int   mqtt_port   = 1883;

const char* topic_control = "nhom15/smartlight/control";
const char* topic_status  = "nhom15/smartlight/status";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

// ------------------------------------------------------------------------------
// 2. MÔ HÌNH PHÂN CẤP ƯU TIÊN & XỬ LÝ XUNG ĐỘT (THEO CHƯƠNG 4.3 BÁO CÁO)
// ------------------------------------------------------------------------------
// Thứ tự ưu tiên (giá trị số càng nhỏ thì quyền ưu tiên càng cao):
// Mức 1: Lệnh khẩn cấp từ App di động (Emergency Lock / App override)
// Mức 2: Lệnh giọng nói của người dùng (Voice Command)
// Mức 3: Lệnh cử chỉ tay (Gesture Sensor APDS-9960 / Physical Tap)
// Mức 4: Tự động hóa cảm biến môi trường (LDR Sensor / Automation)
enum PriorityLevel {
    PRIORITY_APP_EMERGENCY = 1,
    PRIORITY_VOICE         = 2,
    PRIORITY_GESTURE       = 3,
    PRIORITY_AUTO          = 4
};

// Trạng thái hiện tại của đèn (Theo Chương 4.4 Báo cáo)
bool    lightState        = true;   // ON / OFF
uint8_t brightnessPercent = 70;     // Độ sáng phần trăm: 0 - 100% (cố định bước 15%)
uint8_t brightness        = 179;    // Dimming phần cứng: 0 - 255 (round(70 * 255 / 100))
uint8_t colorR            = 255;    // Màu RGB
uint8_t colorG            = 180;
uint8_t colorB            = 50;

// Trạng thái hệ thống & Xử lý xung đột
bool    emergencyLock  = false;   // Cờ khóa khẩn cấp từ App (Ưu tiên 1)
bool    autoLdrEnabled = true;    // Bật/Tắt chế độ tự động theo ánh sáng
int     currentPriority = PRIORITY_AUTO;
String  lastSource     = "KHỞI ĐỘNG";
unsigned long lastManualActionTime = 0;
const unsigned long USER_OVERRIDE_HOLD_MS = 15000; // Người dùng điều khiển thì tạm khóa Auto 15 giây

// Bảng màu xoay vòng cho cử chỉ "Xoay tay đổi màu" (Bảng 4.1 Báo cáo)
const uint8_t PALETTE[][3] = {
    {255, 180, 50},   // Vàng ấm
    {255, 0, 0},     // Đỏ
    {0, 255, 0},     // Xanh lá
    {0, 150, 255},   // Xanh dương
    {255, 220, 0},   // Vàng tươi
    {180, 0, 255},   // Tím
    {0, 255, 220},   // Xanh ngọc (Cyan)
    {255, 255, 255}  // Trắng sáng
};
const int PALETTE_SIZE = sizeof(PALETTE) / sizeof(PALETTE[0]);
int currentPaletteIndex = 0;

// Timer đọc LDR không dùng delay (Non-blocking)
unsigned long lastLdrCheckTime = 0;
const unsigned long LDR_CHECK_INTERVAL = 1000;
int lastLdrState = -1; // 0: Đủ sáng, 1: Tối

// Biến chống dội nút bấm cử chỉ Wokwi
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 200;

// ------------------------------------------------------------------------------
// 3. CÁC HÀM XỬ LÝ PHẦN CỨNG & LED WS2812B
// ------------------------------------------------------------------------------
void updateLEDs() {
    if (!lightState) {
        strip.clear();
    } else {
        uint8_t r = (uint16_t)(colorR * brightness) / 255;
        uint8_t g = (uint16_t)(colorG * brightness) / 255;
        uint8_t b = (uint16_t)(colorB * brightness) / 255;
        uint32_t color = strip.Color(r, g, b);

        for (int i = 0; i < NUM_LEDS; i++) {
            strip.setPixelColor(i, color);
        }
    }
    strip.show();
}

void rotateColor() {
    currentPaletteIndex = (currentPaletteIndex + 1) % PALETTE_SIZE;
    colorR = PALETTE[currentPaletteIndex][0];
    colorG = PALETTE[currentPaletteIndex][1];
    colorB = PALETTE[currentPaletteIndex][2];
    lightState = true;
    Serial.printf("[CỬ CHỈ XOAY TAY]: Chuyển sang màu palette #%d: RGB(%d, %d, %d)\n", 
                  currentPaletteIndex, colorR, colorG, colorB);
}

// Báo cáo trạng thái đèn lên MQTT Broker
void publishStatus(const char* conflictMsg = "") {
    if (!mqttClient.connected()) return;

    JsonDocument doc;
    doc["state"]       = lightState ? "ON" : "OFF";
    doc["brightness"]  = brightnessPercent;
    doc["r"]           = colorR;
    doc["g"]           = colorG;
    doc["b"]           = colorB;
    doc["emergency"]   = emergencyLock;
    doc["auto_ldr"]    = autoLdrEnabled;
    doc["priority"]    = currentPriority;
    doc["source"]      = lastSource;
    if (strlen(conflictMsg) > 0) {
        doc["conflict"] = conflictMsg;
    }

    char buffer[320];
    serializeJson(doc, buffer);
    mqttClient.publish(topic_status, buffer);
}

// ------------------------------------------------------------------------------
// 4. THUẬT TOÁN ĐIỀU PHỐI VÀ XỬ LÝ XUNG ĐỘT LỆNH (CONFLICT RESOLUTION)
// ------------------------------------------------------------------------------
bool requestExecution(PriorityLevel priority, const char* sourceName) {
    // Trường hợp 1: Đang có lệnh Khóa Khẩn Cấp từ App (Ưu tiên 1)
    if (emergencyLock && priority > PRIORITY_APP_EMERGENCY) {
        Serial.printf("\n[XUNG ĐỘT BỊ CHẶN] Lệnh từ [%s] (Mức %d) bị TỪ CHỐI vì hệ thống đang BỊ KHÓA KHẨN CẤP bởi App!\n", 
                      sourceName, priority);
        publishStatus("BỊ CHẶN: ĐANG KHÓA KHẨN CẤP");
        return false;
    }

    // Trường hợp 2: Cảm biến tự động (Ưu tiên 4) muốn can thiệp trong lúc người dùng vừa chủ động điều khiển
    if (priority == PRIORITY_AUTO && (millis() - lastManualActionTime < USER_OVERRIDE_HOLD_MS)) {
        // Tạm hoãn tự động để nhường quyền chủ động của con người
        return false;
    }

    // Lệnh được chấp thuận
    currentPriority = priority;
    lastSource = sourceName;
    if (priority <= PRIORITY_GESTURE) {
        lastManualActionTime = millis();
    }
    Serial.printf("\n[CHẤP THUẬN LỆNH] Nguồn: [%s] | Mức ưu tiên: %d\n", sourceName, priority);
    return true;
}

// Hàm thực thi các lệnh điều khiển
void executeCommand(String cmd, PriorityLevel priority, const char* source) {
    if (!requestExecution(priority, source)) return;

    cmd.trim();
    String upper = cmd;
    upper.toUpperCase();

    // 1. Phân tích lệnh JSON từ App / Dashboard
    if (cmd.startsWith("{") && cmd.endsWith("}")) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, cmd);
        if (!error) {
            // Xử lý cờ khóa khẩn cấp (Ưu tiên 1)
            if (doc["emergency"].is<bool>()) {
                emergencyLock = doc["emergency"].as<bool>();
                Serial.printf("[HỆ THỐNG]: Trạng thái Khóa Khẩn Cấp = %s\n", emergencyLock ? "BẬT" : "TẮT");
            }
            if (doc["auto_ldr"].is<bool>()) {
                autoLdrEnabled = doc["auto_ldr"].as<bool>();
                Serial.printf("[HỆ THỐNG]: Tự động cảm biến LDR = %s\n", autoLdrEnabled ? "BẬT" : "TẮT");
            }
            if (doc["state"].is<String>()) {
                String st = doc["state"].as<String>();
                st.toUpperCase();
                lightState = (st == "ON" || st == "1");
                if (lightState && brightnessPercent == 0) {
                    brightnessPercent = 70;
                    brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
                }
            }
            if (doc["brightness"].is<int>()) {
                int b = doc["brightness"].as<int>();
                brightnessPercent = (uint8_t)constrain(b, 0, 100);
                brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
                if (brightnessPercent > 0) lightState = true;
                else lightState = false;
            }
            if (doc["r"].is<int>() && doc["g"].is<int>() && doc["b"].is<int>()) {
                colorR = doc["r"].as<uint8_t>();
                colorG = doc["g"].as<uint8_t>();
                colorB = doc["b"].as<uint8_t>();
                lightState = true;
            }
            if (doc["gesture"].is<String>()) {
                String g = doc["gesture"].as<String>();
                g.toUpperCase();
                if (g == "UP" || g == "ON") {
                    lightState = true;
                    if (brightnessPercent == 0) {
                        brightnessPercent = 70;
                        brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
                    }
                }
                else if (g == "DOWN" || g == "OFF") {
                    lightState = false;
                }
                else if (g == "RIGHT" || g == "SWIPE_RIGHT") {
                    if (!lightState || brightnessPercent == 0) {
                        lightState = true;
                        brightnessPercent = 15;
                    } else {
                        brightnessPercent = (uint8_t)constrain(brightnessPercent + 15, 0, 100);
                    }
                    brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
                }
                else if (g == "LEFT" || g == "SWIPE_LEFT") {
                    if (brightnessPercent > 15) {
                        brightnessPercent -= 15;
                        brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
                    } else {
                        // Khi đang ở <= 15% (hoặc 10%) mà tiếp tục giảm -> Tắt đèn luôn!
                        brightnessPercent = 0;
                        brightness = 0;
                        lightState = false;
                    }
                }
                else if (g == "ROTATE") rotateColor();
            }
            updateLEDs();
            publishStatus();
            return;
        }
    }

    // 2. Phân tích lệnh dạng chuỗi văn bản (Giọng nói hoặc Cử chỉ)
    if (upper == "ON" || upper == "BAT_DEN") {
        lightState = true;
        if (brightnessPercent == 0) {
            brightnessPercent = 70;
            brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
        }
    } else if (upper == "OFF" || upper == "TAT_DEN") {
        lightState = false;
    } else if (upper == "TOGGLE") {
        lightState = !lightState;
        if (lightState && brightnessPercent == 0) {
            brightnessPercent = 70;
            brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
        }
    } else if (upper == "SWIPE_UP") {
        lightState = true;
        if (brightnessPercent == 0) {
            brightnessPercent = 70;
            brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
        }
    } else if (upper == "SWIPE_DOWN") {
        lightState = false;
    } else if (upper == "SWIPE_RIGHT" || upper == "TANG_SANG") {
        if (!lightState || brightnessPercent == 0) {
            lightState = true;
            brightnessPercent = 15;
        } else {
            brightnessPercent = (uint8_t)constrain(brightnessPercent + 15, 0, 100);
        }
        brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
    } else if (upper == "SWIPE_LEFT" || upper == "GIAM_SANG") {
        if (brightnessPercent > 15) {
            brightnessPercent -= 15;
            brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
        } else {
            brightnessPercent = 0;
            brightness = 0;
            lightState = false;
        }
    } else if (upper == "ROTATE" || upper == "XOAY_TAY") {
        rotateColor();
    } else if (upper == "EMERGENCY_ON") {
        emergencyLock = true;
        lightState = false; // Tắt khẩn cấp an toàn
    } else if (upper == "EMERGENCY_OFF") {
        emergencyLock = false;
    } else if (upper == "RED") {
        colorR = 255; colorG = 0; colorB = 0; lightState = true;
    } else if (upper == "GREEN") {
        colorR = 0; colorG = 255; colorB = 0; lightState = true;
    } else if (upper == "BLUE") {
        colorR = 0; colorG = 150; colorB = 255; lightState = true;
    } else if (upper == "YELLOW") {
        colorR = 255; colorG = 220; colorB = 0; lightState = true;
    } else if (upper == "WARM") {
        colorR = 255; colorG = 180; colorB = 50; lightState = true;
    } else if (upper == "WHITE") {
        colorR = 255; colorG = 255; colorB = 255; lightState = true;
    } else if (upper.startsWith("BRIGHT:")) {
        int b = upper.substring(7).toInt();
        brightnessPercent = (uint8_t)constrain(b, 0, 100);
        brightness = (uint8_t)round((float)brightnessPercent * 255.0f / 100.0f);
        if (brightnessPercent > 0) lightState = true;
    }

    updateLEDs();
    publishStatus();
}

// ------------------------------------------------------------------------------
// 5. CẢM BIẾN TỰ ĐỘNG LDR (QUANG TRỞ - THEO MỤC 4.2 BÁO CÁO)
// ------------------------------------------------------------------------------
void handleLdrSensor() {
    if (!autoLdrEnabled) return;
    if (millis() - lastLdrCheckTime < LDR_CHECK_INTERVAL) return;
    lastLdrCheckTime = millis();

    // ESP32 ADC: 0 (rất sáng) đến 4095 (rất tối) trên module LDR
    int rawValue = analogRead(LDR_PIN);

    // Sử dụng thuật toán trễ (Hysteresis) để tránh bật/tắt chập chờn ở ngưỡng biên
    // Tối: rawValue > 2500 -> Bật đèn
    // Sáng: rawValue < 1500 -> Tắt đèn
    if (rawValue > 2500 && lastLdrState != 1) {
        if (requestExecution(PRIORITY_AUTO, "CẢM BIẾN ÁNH SÁNG LDR (TRỜI TỐI)")) {
            Serial.printf("[TỰ ĐỘNG HÓA]: Phát hiện trời tối (ADC: %d) -> Tự động BẬT đèn!\n", rawValue);
            lightState = true;
            lastLdrState = 1;
            updateLEDs();
            publishStatus();
        }
    } else if (rawValue < 1500 && lastLdrState != 0) {
        if (requestExecution(PRIORITY_AUTO, "CẢM BIẾN ÁNH SÁNG LDR (TRỜI SÁNG)")) {
            Serial.printf("[TỰ ĐỘNG HÓA]: Phát hiện trời sáng (ADC: %d) -> Tự động TẮT đèn tiết kiệm điện!\n", rawValue);
            lightState = false;
            lastLdrState = 0;
            updateLEDs();
            publishStatus();
        }
    }
}

// Callback khi nhận tin từ MQTT
void mqttCallback(char* topic, byte* payload, unsigned int length) {
    String message = "";
    for (unsigned int i = 0; i < length; i++) {
        message += (char)payload[i];
    }

    // Phân loại nguồn dựa vào nội dung hoặc mặc định
    PriorityLevel prio = PRIORITY_APP_EMERGENCY;
    String source = "APP_DI_DONG";

    if (message.indexOf("\"source\":\"VOICE\"") >= 0 || message.startsWith("VOICE:")) {
        prio = PRIORITY_VOICE;
        source = "GIỌNG_NÓI";
    } else if (message.indexOf("CAMERA_AI") >= 0) {
        prio = PRIORITY_GESTURE;
        source = "AI_CAMERA_CỬ_CHỈ";
    } else if (message.indexOf("GESTURE") >= 0 || message.startsWith("SWIPE_") || message == "ROTATE") {
        prio = PRIORITY_GESTURE;
        source = "CỬ_CHỈ";
    } else if (message.indexOf("\"emergency\":true") >= 0) {
        prio = PRIORITY_APP_EMERGENCY;
        source = "KHÓA_KHẨN_CẤP_APP";
    }

    executeCommand(message, prio, source.c_str());
}

// Quản lý kết nối MQTT
void reconnectMQTT() {
    while (!mqttClient.connected()) {
        Serial.print("Dang ket noi MQTT Broker (broker.hivemq.com)... ");
        String clientId = "ESP32_SmartLight_Nhom15_" + String(random(0xffff), HEX);
        if (mqttClient.connect(clientId.c_str())) {
            Serial.println("THANH CONG!");
            mqttClient.subscribe(topic_control);
            Serial.printf("Da lang nghe topic: %s\n", topic_control);
            publishStatus();
        } else {
            Serial.printf("That bai (rc=%d). Thu lai sau 3s...\n", mqttClient.state());
            delay(3000);
        }
    }
}

// ------------------------------------------------------------------------------
// 6. SETUP VÀ LOOP CHÍNH CỦA ESP32
// ------------------------------------------------------------------------------
void setup() {
    Serial.begin(115200);
    // Khởi tạo cổng UART phụ Serial2 (sẵn sàng cho module Elechouse V3 ở chân 16 RX, 17 TX)
    Serial2.begin(9600, SERIAL_8N1, VOICE_RX2_PIN, VOICE_TX2_PIN);

    delay(300);
    Serial.println("\n==================================================================");
    Serial.println("  HỆ THỐNG ĐÈN THÔNG MINH QUA GIỌNG NÓI & CỬ CHỈ - NHÓM 15       ");
    Serial.println("  Cấu hình: ESP32 + WS2812B + LDR + Voice (UART) + Gesture (I2C) ");
    Serial.println("==================================================================");

    // Cấu hình chân
    pinMode(GESTURE_PIN, INPUT_PULLUP);
    pinMode(LDR_PIN, INPUT);

    // Khởi động LED
    strip.begin();
    strip.show();
    updateLEDs();

    // Kết nối Wi-Fi Wokwi
    Serial.printf("Ket noi Wi-Fi [%s]...\n", ssid);
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(250);
        Serial.print(".");
    }
    Serial.println("\nWi-Fi DA KET NOI! IP: " + WiFi.localIP().toString());

    // Cấu hình MQTT
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);

    Serial.println("\n[SẴN SÀNG THỬ NGHIỆM]:");
    Serial.println(" 1. Web Dashboard: Bật/Tắt, Giọng nói tiếng Việt, Cử chỉ.");
    Serial.println(" 2. Wokwi LDR: Kéo thanh trượt ánh sáng để xem đèn tự động bật khi trời tối.");
    Serial.println(" 3. Wokwi Nút bấm GPIO 12: Nhấn để mô phỏng cử chỉ xoay tay đổi màu.");
    Serial.println("==================================================================\n");
}

void loop() {
    // 1. Quản lý MQTT
    if (!mqttClient.connected()) {
        reconnectMQTT();
    }
    mqttClient.loop();

    // 2. Cảm biến ánh sáng tự động LDR (Ưu tiên 4)
    handleLdrSensor();

    // 3. Đọc nút bấm cử chỉ vật lý trên Wokwi (GPIO 12) -> Xoay tay đổi màu
    int reading = digitalRead(GESTURE_PIN);
    if (reading != lastButtonState) {
        lastDebounceTime = millis();
    }
    if ((millis() - lastDebounceTime) > debounceDelay) {
        if (reading == LOW && lastButtonState == HIGH) {
            executeCommand("ROTATE", PRIORITY_GESTURE, "NÚT_CỬ_CHỈ_VẬT_LÝ");
        }
    }
    lastButtonState = reading;

    // 4. Lắng nghe UART phụ Serial2 (Module Elechouse V3)
    if (Serial2.available()) {
        byte voiceByte = Serial2.read();
        Serial.printf("[UART ELECHOUSE V3]: Nhan ma lenh 0x%02X\n", voiceByte);
        if (voiceByte == 0x11) executeCommand("ON", PRIORITY_VOICE, "ELECHOUSE_V3_UART");
        else if (voiceByte == 0x12) executeCommand("OFF", PRIORITY_VOICE, "ELECHOUSE_V3_UART");
        else if (voiceByte == 0x13) executeCommand("SWIPE_RIGHT", PRIORITY_VOICE, "ELECHOUSE_V3_UART");
        else if (voiceByte == 0x14) executeCommand("SWIPE_LEFT", PRIORITY_VOICE, "ELECHOUSE_V3_UART");
        else if (voiceByte == 0x15) executeCommand("ROTATE", PRIORITY_VOICE, "ELECHOUSE_V3_UART");
    }

    // 5. Lắng nghe lệnh gõ từ bàn phím Serial Monitor (Dự phòng khẩn cấp)
    if (Serial.available()) {
        String input = Serial.readStringUntil('\n');
        input.trim();
        if (input.length() > 0) {
            executeCommand(input, PRIORITY_VOICE, "SERIAL_KEYBOARD");
        }
    }
}
