/*
  RVCS - Wheelchair Rear View Camera System
  Target: AI Thinker-compatible ESP32-CAM with OV2640
  Programmer: ESP32-CAM-MB USB-C (CH340)

  After flashing, connect your phone to:
    Wi-Fi: RVCS-Camera
    Password: RVCS-View-2026
  Then browse to http://192.168.4.1

  This is a local, password-protected Wi-Fi access point. No internet required.
  Change the Wi-Fi password before deploying.
*/

#include <Arduino.h>
#include "esp_camera.h"
#include <WiFi.h>
#include "esp_http_server.h"

// AI Thinker-compatible ESP32-CAM / OV2640 pin map
#define PWDN_GPIO_NUM      32
#define RESET_GPIO_NUM     -1
#define XCLK_GPIO_NUM       0
#define SIOD_GPIO_NUM      26
#define SIOC_GPIO_NUM      27
#define Y9_GPIO_NUM        35
#define Y8_GPIO_NUM        34
#define Y7_GPIO_NUM        39
#define Y6_GPIO_NUM        36
#define Y5_GPIO_NUM        21
#define Y4_GPIO_NUM        19
#define Y3_GPIO_NUM        18
#define Y2_GPIO_NUM         5
#define VSYNC_GPIO_NUM     25
#define HREF_GPIO_NUM      23
#define PCLK_GPIO_NUM      22

static const char *AP_SSID = "RVCS-Camera";
static const char *AP_PASSWORD = "teampeatz"; // Change this before real use; minimum 8 characters.

static httpd_handle_t webServer = NULL;

// The phone shows the live MJPEG stream and provides a local mirror control.
static const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta name="theme-color" content="#09131a">
  <meta name="color-scheme" content="dark">
  <title>RVCS | Rear View Camera</title>
  <link id="site-icon" rel="icon" type="image/png" href="data:image/png;base64,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">
  <style>
    :root {
      color-scheme: dark;
      font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
      background: #09131a;
      color: #f2f7f8;
      font-synthesis: none;
      --surface: #111f28;
      --surface-raised: #172833;
      --line: rgba(192, 221, 229, 0.12);
      --muted: #91a6af;
      --accent: #6de0bd;
    }

    * { box-sizing: border-box; }

    body {
      min-width: 320px;
      min-height: 100vh;
      min-height: 100dvh;
      margin: 0;
      padding: clamp(16px, 4vw, 48px);
      background:
        radial-gradient(ellipse at 50% -30%, rgba(35, 108, 111, 0.2), transparent 58%),
        #09131a;
    }

    .app {
      width: min(100%, 1080px);
      margin: 0 auto;
      display: grid;
      align-content: center;
      gap: clamp(16px, 2.4vw, 26px);
    }

    .topbar {
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 20px;
    }

    .brand {
      display: flex;
      align-items: center;
      gap: 14px;
      min-width: 0;
    }

    .brand-mark {
      width: 48px;
      height: 48px;
      flex: 0 0 auto;
      display: block;
      padding: 3px;
      border: 1px solid rgba(255, 255, 255, 0.12);
      border-radius: 12px;
      background: #2c2c2c;
      object-fit: contain;
    }

    .eyebrow {
      margin: 0 0 3px;
      color: var(--accent);
      font-size: 0.7rem;
      font-weight: 800;
      letter-spacing: 0.17em;
      white-space: nowrap;
    }

    h1 {
      overflow: hidden;
      margin: 0;
      font-size: clamp(1.2rem, 3vw, 1.65rem);
      font-weight: 680;
      letter-spacing: -0.035em;
      text-overflow: ellipsis;
      white-space: nowrap;
    }

    .status {
      display: inline-flex;
      align-items: center;
      gap: 9px;
      min-height: 40px;
      padding: 8px 13px;
      border: 1px solid var(--line);
      border-radius: 999px;
      background: rgba(17, 31, 40, 0.82);
      color: #dce8eb;
      font-size: 0.84rem;
      font-weight: 700;
      white-space: nowrap;
    }

    .status::before {
      width: 8px;
      height: 8px;
      border-radius: 50%;
      background: #f4bc62;
      box-shadow: 0 0 12px rgba(244, 188, 98, 0.65);
      content: "";
    }

    .status[data-state="live"]::before {
      background: var(--accent);
      box-shadow: 0 0 12px rgba(109, 224, 189, 0.75);
    }

    .status[data-state="reconnecting"]::before {
      background: #ff7777;
      box-shadow: 0 0 12px rgba(255, 119, 119, 0.65);
    }

    .camera-card {
      overflow: hidden;
      padding: clamp(10px, 1.5vw, 16px);
      border: 1px solid var(--line);
      border-radius: clamp(18px, 2vw, 26px);
      background: linear-gradient(145deg, rgba(21, 39, 49, 0.98), rgba(13, 25, 33, 0.98));
      box-shadow: 0 28px 80px rgba(0, 0, 0, 0.28), inset 0 1px rgba(255, 255, 255, 0.035);
    }

    .camera-toolbar {
      min-height: 40px;
      display: flex;
      align-items: center;
      justify-content: space-between;
      gap: 12px;
      padding: 0 5px 11px;
    }

    .feed-label, .feed-mode {
      display: inline-flex;
      align-items: center;
      gap: 8px;
      color: #bfd0d5;
      font-size: 0.73rem;
      font-weight: 750;
      letter-spacing: 0.09em;
      text-transform: uppercase;
    }

    .feed-label svg { width: 16px; height: 16px; color: var(--accent); }
    .feed-mode { color: var(--muted); font-weight: 650; letter-spacing: 0.04em; }

    .screen {
      position: relative;
      overflow: hidden;
      aspect-ratio: 16 / 9;
      min-height: 240px;
      display: grid;
      place-items: center;
      isolation: isolate;
      border: 1px solid rgba(184, 218, 227, 0.13);
      border-radius: clamp(12px, 1.5vw, 18px);
      background:
        linear-gradient(rgba(67, 102, 111, 0.09) 1px, transparent 1px),
        linear-gradient(90deg, rgba(67, 102, 111, 0.09) 1px, transparent 1px),
        radial-gradient(ellipse at center, #13232b, #071016 78%);
      background-size: 36px 36px, 36px 36px, auto;
    }

    .screen::after {
      position: absolute;
      z-index: 1;
      inset: 0;
      border: 1px solid rgba(255, 255, 255, 0.025);
      border-radius: inherit;
      content: "";
      pointer-events: none;
    }

    .screen:fullscreen {
      width: 100%;
      height: 100%;
      min-height: 0;
      aspect-ratio: auto;
      border: 0;
      border-radius: 0;
    }

    .screen img {
      position: relative;
      z-index: 1;
      width: 100%;
      height: 100%;
      object-fit: contain;
      opacity: 0;
      transition: opacity 220ms ease, transform 180ms ease;
    }

    .screen[data-state="live"] img { opacity: 1; }
    .screen img.mirrored { transform: scaleX(-1); }

    .empty-state {
      position: absolute;
      inset: 0;
      display: grid;
      align-content: center;
      justify-items: center;
      gap: 10px;
      padding: 24px;
      color: #c4d2d6;
      text-align: center;
    }

    .screen[data-state="live"] .empty-state { display: none; }

    .camera-glyph {
      width: 58px;
      height: 58px;
      display: grid;
      place-items: center;
      margin-bottom: 4px;
      border: 1px solid rgba(109, 224, 189, 0.22);
      border-radius: 20px;
      background: rgba(109, 224, 189, 0.07);
      color: var(--accent);
    }

    .camera-glyph svg { width: 28px; height: 28px; }
    .empty-title { margin: 0; font-size: 1rem; font-weight: 700; }
    .empty-copy { max-width: 340px; margin: 0; color: var(--muted); font-size: 0.88rem; line-height: 1.5; }

    .controls {
      display: grid;
      grid-template-columns: repeat(3, minmax(0, 1fr));
      gap: 12px;
      margin-top: 12px;
    }

    button {
      min-width: 0;
      min-height: 58px;
      display: inline-flex;
      align-items: center;
      justify-content: center;
      gap: 10px;
      padding: 12px 16px;
      border: 1px solid var(--line);
      border-radius: 14px;
      background: linear-gradient(180deg, #1a2c36, #15252e);
      color: #edf5f6;
      font: inherit;
      font-size: 0.92rem;
      font-weight: 700;
      cursor: pointer;
      touch-action: manipulation;
      transition: border-color 140ms ease, background 140ms ease, transform 140ms ease;
    }

    button svg { width: 19px; height: 19px; flex: 0 0 auto; }
    button:hover { border-color: rgba(164, 211, 218, 0.32); background: #203742; }
    button:active { transform: scale(0.985); }
    button[aria-pressed="true"] { border-color: rgba(109, 224, 189, 0.6); background: rgba(109, 224, 189, 0.1); color: #a7f1d8; }
    button:focus-visible { outline: 3px solid #79c7ff; outline-offset: 3px; }

    .reconnect-button {
      border-color: transparent;
      background: var(--accent);
      color: #09221d;
    }

    .reconnect-button:hover { border-color: transparent; background: #8cebcf; }

    .notice {
      margin: 0;
      color: #ffd18b;
      text-align: center;
      font-size: 0.88rem;
    }

    .hint {
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 8px;
      margin: -4px 0 0;
      color: var(--muted);
      text-align: center;
      font-size: 0.82rem;
    }

    .hint svg { width: 16px; height: 16px; flex: 0 0 auto; color: var(--accent); }
    .hint-copy { min-width: 0; }

    @media (max-width: 600px) {
      body { padding: 18px 14px max(16px, env(safe-area-inset-bottom)); }
      .app { align-content: start; gap: 16px; }
      .topbar { gap: 10px; }
      .brand { gap: 10px; }
      .brand-mark { width: 42px; height: 42px; border-radius: 11px; }
      .eyebrow { font-size: 0.56rem; letter-spacing: 0.08em; }
      h1 { font-size: clamp(1.05rem, 5vw, 1.3rem); }
      .status { min-height: 35px; gap: 7px; padding: 6px 9px; font-size: 0.75rem; }
      .camera-card { padding: 8px; border-radius: 17px; }
      .camera-toolbar { min-height: 34px; padding: 0 3px 8px; }
      .feed-label, .feed-mode { gap: 6px; font-size: 0.64rem; }
      .screen { aspect-ratio: 4 / 3; min-height: 0; border-radius: 12px; }
      .controls { gap: 8px; margin-top: 9px; }
      button { min-height: 60px; flex-direction: column; gap: 5px; padding: 8px 4px; border-radius: 12px; font-size: 0.75rem; }
      button svg { width: 19px; height: 19px; }
      .reconnect-button { box-shadow: 0 5px 18px rgba(109, 224, 189, 0.15); }
      .camera-glyph { width: 50px; height: 50px; border-radius: 17px; }
      .empty-title { font-size: 0.92rem; }
      .empty-copy { max-width: 270px; font-size: 0.8rem; }
      .hint { align-items: flex-start; font-size: 0.76rem; line-height: 1.45; }
    }

    @media (max-width: 360px) {
      .status { gap: 6px; padding-inline: 8px; font-size: 0.7rem; }
      .brand { gap: 8px; }
      .brand-mark { width: 38px; height: 38px; }
      .feed-mode { font-size: 0.58rem; }
    }

    @media (prefers-reduced-motion: reduce) {
      *, *::before, *::after { scroll-behavior: auto !important; transition-duration: 0.01ms !important; }
    }
  </style>
</head>
<body>
  <main class="app">
    <header class="topbar">
      <div class="brand">
        <img id="brand-logo" class="brand-mark" width="48" height="48" alt="uOttawa">
        <div>
          <p class="eyebrow">RVCS</p>
          <h1>Rear View</h1>
        </div>
      </div>
      <div id="connection-status" class="status" data-state="connecting" role="status" aria-live="polite">
        Connecting...
      </div>
    </header>

    <section class="camera-card" aria-label="Rear view camera">
      <div class="camera-toolbar">
        <span class="feed-label">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
            <rect x="3" y="5" width="18" height="14" rx="3"/>
            <circle cx="12" cy="12" r="3"/>
          </svg>
          Live camera
        </span>
        <span class="feed-mode">ESP32-CAM · WI-FI</span>
      </div>

      <div id="camera-screen" class="screen" data-state="connecting" role="img" aria-label="Live rear view camera feed">
        <div class="empty-state" aria-hidden="true">
          <div class="camera-glyph">
            <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round">
              <rect x="3" y="6" width="18" height="13" rx="3"/>
              <circle cx="12" cy="12.5" r="3"/>
              <path d="m7 6 1-2h8l1 2"/>
            </svg>
          </div>
          <p id="empty-title" class="empty-title">Connecting to your camera</p>
          <p id="empty-copy" class="empty-copy">Your live view will appear here as soon as the camera is ready.</p>
        </div>
        <img id="camera-feed" alt="">
      </div>

      <div class="controls" aria-label="Camera controls">
        <button id="mirror-button" type="button" aria-pressed="false">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
            <path d="M12 3v18M6 7l-3 5 3 5M18 7l3 5-3 5"/>
            <path d="M8 8h2M8 12h2M8 16h2M14 8h2M14 12h2M14 16h2"/>
          </svg>
          <span>Mirror off</span>
        </button>
        <button id="fullscreen-button" type="button">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
            <path d="M8 3H5a2 2 0 0 0-2 2v3M16 3h3a2 2 0 0 1 2 2v3M21 16v3a2 2 0 0 1-2 2h-3M3 16v3a2 2 0 0 0 2 2h3"/>
          </svg>
          <span>Full screen</span>
        </button>
        <button id="reconnect-button" class="reconnect-button" type="button">
          <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
            <path d="M20 7v5h-5M4 17v-5h5"/>
            <path d="M5.5 9A7 7 0 0 1 17 6l3 6M4 12l3 6a7 7 0 0 0 11.5-3"/>
          </svg>
          <span>Reconnect</span>
        </button>
      </div>
    </section>

    <p id="camera-notice" class="notice" role="status" aria-live="polite" hidden></p>
    <p class="hint">
      <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
        <path d="M5 10a11 11 0 0 1 14 0M8 13a6.5 6.5 0 0 1 8 0M11 16a2 2 0 0 1 2 0"/>
        <path d="M12 20h.01"/>
      </svg>
      <span class="hint-copy">Connect to the <strong>RVCS-Camera</strong> Wi-Fi network to start viewing.</span>
    </p>
  </main>

  <script>
    (() => {
      // Change this constant if the camera's AP address or stream path changes.
      const STREAM_URL = "http://192.168.4.1/stream";
      const INITIAL_RETRY_DELAY_MS = 1000;
      const MAX_RETRY_DELAY_MS = 15000;

      const screen = document.getElementById("camera-screen");
      const image = document.getElementById("camera-feed");
      const brandLogo = document.getElementById("brand-logo");
      brandLogo.src = document.getElementById("site-icon").href;
      const status = document.getElementById("connection-status");
      const emptyTitle = document.getElementById("empty-title");
      const emptyCopy = document.getElementById("empty-copy");
      const notice = document.getElementById("camera-notice");
      const mirrorButton = document.getElementById("mirror-button");
      const fullscreenButton = document.getElementById("fullscreen-button");
      const reconnectButton = document.getElementById("reconnect-button");

      let retryCount = 0;
      let retryTimer = null;
      let requestId = 0;

      function setStatus(state, message) {
        status.dataset.state = state;
        screen.dataset.state = state;
        status.textContent = message;
        emptyTitle.textContent = state === "reconnecting"
          ? "Reconnecting to your camera"
          : "Connecting to your camera";
        emptyCopy.textContent = state === "reconnecting"
          ? "Trying again automatically. Check that you're connected to the camera's Wi-Fi."
          : "Your live view will appear here as soon as the camera is ready.";
      }

      function connect() {
        window.clearTimeout(retryTimer);
        retryTimer = null;
        requestId += 1;
        const thisRequest = requestId;
        setStatus("connecting", retryCount === 0 ? "Connecting..." : "Reconnecting...");
        notice.hidden = true;

        image.onload = () => {
          if (thisRequest !== requestId) return;
          retryCount = 0;
          setStatus("live", "Live");
        };

        image.onerror = () => {
          if (thisRequest !== requestId) return;
          setStatus("reconnecting", "Reconnecting...");
          scheduleReconnect();
        };

        image.src = `${STREAM_URL}?t=${Date.now()}`;
      }

      function scheduleReconnect() {
        if (retryTimer !== null) return;
        const delay = Math.min(
          INITIAL_RETRY_DELAY_MS * (2 ** retryCount),
          MAX_RETRY_DELAY_MS
        );
        retryCount += 1;
        retryTimer = window.setTimeout(connect, delay);
      }

      mirrorButton.addEventListener("click", () => {
        const isMirrored = mirrorButton.getAttribute("aria-pressed") !== "true";
        mirrorButton.setAttribute("aria-pressed", String(isMirrored));
        mirrorButton.querySelector("span").textContent = `Mirror ${isMirrored ? "on" : "off"}`;
        image.classList.toggle("mirrored", isMirrored);
      });

      reconnectButton.addEventListener("click", () => {
        retryCount = 0;
        connect();
      });

      fullscreenButton.addEventListener("click", async () => {
        notice.hidden = true;
        try {
          if (document.fullscreenElement) {
            await document.exitFullscreen();
          } else {
            await screen.requestFullscreen();
          }
        } catch (error) {
          notice.textContent = `Full screen unavailable: ${error.message}`;
          notice.hidden = false;
        }
      });

      document.addEventListener("fullscreenchange", () => {
        fullscreenButton.querySelector("span").textContent = document.fullscreenElement
          ? "Exit full screen"
          : "Full screen";
      });

      connect();
    })();
  </script>
</body>
</html>

)HTML";

static esp_err_t page_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html; charset=utf-8");
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");
  return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t stream_handler(httpd_req_t *req) {
  esp_err_t res = httpd_resp_set_type(req, "multipart/x-mixed-replace;boundary=frame");
  if (res != ESP_OK) return res;
  httpd_resp_set_hdr(req, "Cache-Control", "no-store");

  while (true) {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera frame capture failed");
      return ESP_FAIL;
    }

    char hdr[96];
    int len = snprintf(hdr, sizeof(hdr),
      "--frame\r\nContent-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n",
      (unsigned int)fb->len);

    res = httpd_resp_send_chunk(req, hdr, len);
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)fb->buf, fb->len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, "\r\n", 2);
    }
    esp_camera_fb_return(fb);

    if (res != ESP_OK) break; // Phone disconnected or network went away
    vTaskDelay(1); // Yield briefly; no intentional 20 ms inter-frame wait
  }
  return res;
}

static void startWebServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;
  config.stack_size = 8192;
  config.max_uri_handlers = 4;
  config.lru_purge_enable = true;

  if (httpd_start(&webServer, &config) != ESP_OK) {
    Serial.println("Failed to start web server");
    return;
  }

  httpd_uri_t page = {};
  page.uri = "/";
  page.method = HTTP_GET;
  page.handler = page_handler;
  httpd_register_uri_handler(webServer, &page);

  httpd_uri_t stream = {};
  stream.uri = "/stream";
  stream.method = HTTP_GET;
  stream.handler = stream_handler;
  httpd_register_uri_handler(webServer, &stream);
}

void setup() {
  Serial.begin(115200);
  Serial.println("\nRVCS camera starting...");

  camera_config_t config = {};
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size = FRAMESIZE_QVGA;     // 320 x 240: transmit smaller frames for lower latency
  config.jpeg_quality = 20;              // 0 is best quality, 63 is lowest; faster transfer
  config.fb_count = 2;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.grab_mode = CAMERA_GRAB_LATEST;

  if (!psramFound()) {
    Serial.println("No PSRAM: switching to lower-memory settings");
    config.frame_size = FRAMESIZE_QVGA;  // 320 x 240
    config.jpeg_quality = 20;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera initialization failed: 0x%x\n", err);
    Serial.println("Check ribbon cable, 5V power, and selected ESP32-CAM board.");
    while (true) delay(1000);
  }

  WiFi.mode(WIFI_AP);
  WiFi.setSleep(false);
  WiFi.softAPConfig(IPAddress(192,168,4,1), IPAddress(192,168,4,1), IPAddress(255,255,255,0));
  if (!WiFi.softAP(AP_SSID, AP_PASSWORD)) {
    Serial.println("Failed to create Wi-Fi access point");
    while (true) delay(1000);
  }

  startWebServer();
  Serial.print("Wi-Fi network: "); Serial.println(AP_SSID);
  Serial.print("Open on phone: http://"); Serial.println(WiFi.softAPIP());
}

void loop() {
  delay(1000);
}
