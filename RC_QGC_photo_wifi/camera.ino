/**
 * @file camera.ino
 * @brief 摄像头驱动模块(OV3660)
 * @author Oleg Kalachev <okalachev@gmail.com>
 * @date 2023
 * @version 1.0
 * 
 * 使用ESP32S3-EYE标准引脚配置驱动OV3660摄像头，支持JPEG图像捕获和Web实时预览。
 * 基于8位并行接口+SCCB协议通信。
 */

#include "esp_camera.h"
#include <WebServer.h>

extern WebServer server;

// ESP32S3-EYE 摄像头引脚定义
#define PWDN_GPIO_NUM      -1
#define RESET_GPIO_NUM     -1
#define XCLK_GPIO_NUM      15
#define SIOD_GPIO_NUM      4
#define SIOC_GPIO_NUM      5

#define Y9_GPIO_NUM        16
#define Y8_GPIO_NUM        17
#define Y7_GPIO_NUM        18
#define Y6_GPIO_NUM        12
#define Y5_GPIO_NUM        10
#define Y4_GPIO_NUM        8
#define Y3_GPIO_NUM        9
#define Y2_GPIO_NUM        11

#define VSYNC_GPIO_NUM     6
#define HREF_GPIO_NUM      7
#define PCLK_GPIO_NUM      13

// ====================== 摄像头参数 ======================
#define XCLK_FREQ_HZ         15000000
#define CAMERA_FRAMESIZE     FRAMESIZE_QQVGA
#define CAMERA_JPEG_QUALITY  10

// ====================== 全局状态 ======================

/** 最新JPEG图像缓冲区 */
static uint8_t *cameraJpgBuf = NULL;
/** 图像字节数 */
static size_t   cameraJpgLen = 0;
/** 摄像头缓冲区互斥锁 */
static SemaphoreHandle_t mutexCamera = NULL;
/** 摄像头就绪标志 */
bool cameraReady = false;

// ====================== 摄像头初始化 ======================

/**
 * @brief 初始化摄像头模块
 */
void setupCamera() {
  print("初始化摄像头 (OV3660)...\n");

  mutexCamera = xSemaphoreCreateMutex();
  configASSERT(mutexCamera != NULL);

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
  config.xclk_freq_hz = XCLK_FREQ_HZ;
  config.frame_size   = CAMERA_FRAMESIZE;
  config.pixel_format = PIXFORMAT_JPEG;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_DRAM;
  config.jpeg_quality = CAMERA_JPEG_QUALITY;
  config.fb_count     = 1;

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    print("摄像头初始化失败, 错误码: 0x%x\n", err);
    cameraReady = false;
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  if (s) {
    s->set_vflip(s, 1);
    s->set_bpc(s, 1);
    s->set_wpc(s, 1);
    s->set_lenc(s, 1);
    s->set_raw_gma(s, 1);
    s->set_sharpness(s, 1);
    s->set_denoise(s, 1);
    s->set_brightness(s, 0);
    s->set_contrast(s, 0);
    s->set_saturation(s, 0);
    s->set_whitebal(s, 1);
    s->set_gain_ctrl(s, 1);
    s->set_exposure_ctrl(s, 1);
  }

  cameraReady = true;
  print("摄像头初始化成功 (OV3660, QQVGA 160x120)\n");
}

/**
 * @brief 拍摄一帧图像
 * 
 * 将最新JPEG帧存入缓冲区，供Web端点使用。
 */
void cameraCapture() {
  if (!cameraReady) return;

  camera_fb_t *fb = esp_camera_fb_get();
  if (!fb) {
    return;
  }

  xSemaphoreTake(mutexCamera, portMAX_DELAY);

  if (cameraJpgBuf) {
    free(cameraJpgBuf);
    cameraJpgBuf = NULL;
  }

  cameraJpgLen = fb->len;
  cameraJpgBuf = (uint8_t *)malloc(cameraJpgLen);
  if (cameraJpgBuf) {
    memcpy(cameraJpgBuf, fb->buf, cameraJpgLen);
  } else {
    cameraJpgLen = 0;
  }

  xSemaphoreGive(mutexCamera);
  esp_camera_fb_return(fb);
}

/**
 * @brief Web端点: /capture
 * 
 * 返回最新的JPEG图像，前端img标签可直接引用此端点。
 */
void handleCameraCapture() {
  if (!cameraReady) {
    server.send(503, "text/plain", "Camera not ready");
    return;
  }

  cameraCapture();

  xSemaphoreTake(mutexCamera, pdMS_TO_TICKS(200));
  if (cameraJpgBuf && cameraJpgLen > 0) {
    server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    server.sendHeader("Pragma", "no-cache");
    server.sendHeader("Expires", "0");
    server.send_P(200, PSTR("image/jpeg"), (const char *)cameraJpgBuf, cameraJpgLen);
  } else {
    server.send(503, "text/plain", "No image available");
  }
  xSemaphoreGive(mutexCamera);
}