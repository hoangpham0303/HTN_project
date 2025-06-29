#define BLYNK_TEMPLATE_NAME "xedieukhien"
#define BLYNK_AUTH_TOKEN "6F_cCx_qbaElYHLulsWLi78_QHHyIg8d"
#define BLYNK_TEMPLATE_ID "TMPL6XO2Oyr_Y"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include "esp_camera.h"
#include <WebServer.h>

// Chọn đúng model camera
#define CAMERA_MODEL_AI_THINKER
#include "camera_pins.h"  // Có sẵn trong ví dụ của ESP32 Camera WebServer

// Thông tin WiFi
char ssid[] = "Ip 16promax";
char pass[] = "11111111";

// Đèn flash ESP32-CAM nối chân GPIO 4
#define LED_PIN 4

WebServer server(80);

const char HEADER[] = "HTTP/1.1 200 OK\r\n" \
                      "Access-Control-Allow-Origin: *\r\n" \
                      "Content-Type: multipart/x-mixed-replace; boundary=123456789000000000000987654321\r\n";
const char BOUNDARY[] = "\r\n--123456789000000000000987654321\r\n";
const char CTNTTYPE[] = "Content-Type: image/jpeg\r\nContent-Length: ";
const int hdrLen = strlen(HEADER);
const int bdrLen = strlen(BOUNDARY);
const int cntLen = strlen(CTNTTYPE);

// Hàm nhận tín hiệu từ app Blynk
BLYNK_WRITE(V4) {
  int state = param.asInt(); // Lấy trạng thái nút
  digitalWrite(LED_PIN, state);
}

void handle_jpg_stream() {
  WiFiClient client = server.client();
  camera_fb_t * fb = NULL;

  client.write(HEADER, hdrLen);
  client.write(BOUNDARY, bdrLen);

  while (true) {
    if (!client.connected()) break;

    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      continue;
    }

    client.write(CTNTTYPE, cntLen);
    char buf[32];
    sprintf(buf, "%d\r\n\r\n", fb->len);
    client.write(buf, strlen(buf));
    client.write((char *)fb->buf, fb->len);
    client.write(BOUNDARY, bdrLen);

    esp_camera_fb_return(fb);
  }
}

void handle_jpg() {
  WiFiClient client = server.client();
  if (!client.connected()) return;

  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    server.send(500, "text/plain", "Camera capture failed");
    return;
  }

  client.println("HTTP/1.1 200 OK");
  client.println("Content-disposition: inline; filename=capture.jpg");
  client.println("Content-type: image/jpeg");
  client.println();
  client.write((char *)fb->buf, fb->len);
  esp_camera_fb_return(fb);
}

void handleNotFound() {
  String message = "Server is running!\n\n";
  message += "URI: " + server.uri();
  server.send(200, "text/plain", message);
}

void startCameraServer() {
  server.on("/mjpeg/1", HTTP_GET, handle_jpg_stream);
  server.on("/jpg", HTTP_GET, handle_jpg);
  server.onNotFound(handleNotFound);
  server.begin();
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = Y2_GPIO_NUM;
  config.pin_d1       = Y3_GPIO_NUM;
  config.pin_d2       = Y4_GPIO_NUM;
  config.pin_d3       = Y5_GPIO_NUM;
  config.pin_d4       = Y6_GPIO_NUM;
  config.pin_d5       = Y7_GPIO_NUM;
  config.pin_d6       = Y8_GPIO_NUM;
  config.pin_d7       = Y9_GPIO_NUM;
  config.pin_xclk     = XCLK_GPIO_NUM;
  config.pin_pclk     = PCLK_GPIO_NUM;
  config.pin_vsync    = VSYNC_GPIO_NUM;
  config.pin_href     = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn     = PWDN_GPIO_NUM;
  config.pin_reset    = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QVGA;
  config.jpeg_quality = 12;
  config.fb_count     = 2;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println(WiFi.localIP());
  Serial.print("Stream Link: http://");
  Serial.print(WiFi.localIP());
  Serial.println("/mjpeg/1");

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  startCameraServer();
}

void loop() {
  Blynk.run();
  server.handleClient();
}