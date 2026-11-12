#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <WiFiManager.h>
#include <TFT_eSPI.h>
#include <time.h>

TFT_eSPI tft = TFT_eSPI();
ESP8266WebServer server(80);

// Настройки, сохраняемые в энергонезависимую память (LittleFS)
// Settings stored in non-volatile memory (LittleFS)
struct Config {
  char printer_ip[32] = "192.168.1.100";
  int rotation = 1;       // Поворот экрана / Screen rotation (0, 1, 2, 3)
  int arc_mode = 0;       // 0 = дуга (8..4 часа) / arc (60..300), 1 = полный круг / full circle (0..360)
  int update_ms = 1500;   // Интервал опроса (мс) / Polling interval (ms)
  int tz_offset = 3;      // Часовой пояс UTC / Timezone UTC offset
  int clock_idle = 1;     // 1 = часы при простое / clock when idle, 0 = всегда экран принтера / always printer UI
  int lang = 0;           // 0 = Русский (RU), 1 = English (EN)
} cfg;

unsigned long lastUpdate = 0;
unsigned long lastClockTick = 0;
unsigned long clockPreviewUntil = 0;

// Переменные телеметрии / Telemetry variables
float nozzleTemp = 0, nozzleTarget = 0;
float bedTemp = 0, bedTarget = 0;
float progress = 0;
float fanValue = 0;
float printDuration = 0;
String printState = "connecting";

int lastPct = -1;
int lastArcMode = -1;
bool isClockScreen = false;
bool colonState = true;

const char* daysOfWeek[8] = {"SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT", "SUN"};

// --- Работа с LittleFS / LittleFS configuration storage ---
void loadConfig() {
  if (LittleFS.begin() && LittleFS.exists("/config.json")) {
    File f = LittleFS.open("/config.json", "r");
    if (f) {
      JsonDocument doc;
      if (!deserializeJson(doc, f)) {
        strlcpy(cfg.printer_ip, doc["printer_ip"] | "192.168.1.100", sizeof(cfg.printer_ip));
        cfg.rotation = doc["rotation"] | 1;
        cfg.arc_mode = doc["arc_mode"] | 0;
        cfg.update_ms = doc["update_ms"] | 1500;
        cfg.tz_offset = doc["tz_offset"] | 3;
        cfg.clock_idle = doc["clock_idle"] | 1;
        cfg.lang = doc["lang"] | 0;
      }
      f.close();
    }
  }
}

void saveConfig() {
  if (LittleFS.begin()) {
    File f = LittleFS.open("/config.json", "w");
    if (f) {
      JsonDocument doc;
      doc["printer_ip"] = cfg.printer_ip;
      doc["rotation"] = cfg.rotation;
      doc["arc_mode"] = cfg.arc_mode;
      doc["update_ms"] = cfg.update_ms;
      doc["tz_offset"] = cfg.tz_offset;
      doc["clock_idle"] = cfg.clock_idle;
      doc["lang"] = cfg.lang;
      serializeJson(doc, f);
      f.close();
    }
  }
}

void applyTimezone() {
  configTime(cfg.tz_offset * 3600L, 0, "pool.ntp.org", "time.nist.gov", "ru.pool.ntp.org");
}

// --- Отрисовка интерфейса печати / Print UI drawing ---
void drawNozzleIcon(int cx, int y) {
  int x = cx - 5;
  tft.fillRect(x, y, 10, 5, TFT_LIGHTGREY);
  tft.fillTriangle(x, y + 5, x + 9, y + 5, x + 5, y + 11, TFT_LIGHTGREY);
  tft.fillRect(x - 3, y + 2, 3, 2, TFT_LIGHTGREY);
}

void drawBedIcon(int cx, int y) {
  int x = cx - 7;
  tft.drawFastHLine(x, y + 9, 14, TFT_LIGHTGREY);
  tft.drawFastHLine(x, y + 11, 14, TFT_LIGHTGREY);
  tft.drawLine(x + 3, y + 7, x + 3, y + 1, TFT_LIGHTGREY);
  tft.drawLine(x + 1, y + 3, x + 3, y + 1, TFT_LIGHTGREY);
  tft.drawLine(x + 5, y + 3, x + 3, y + 1, TFT_LIGHTGREY);
  tft.drawLine(x + 10, y + 7, x + 10, y + 1, TFT_LIGHTGREY);
  tft.drawLine(x + 8, y + 3, x + 10, y + 1, TFT_LIGHTGREY);
  tft.drawLine(x + 12, y + 3, x + 10, y + 1, TFT_LIGHTGREY);
}

void drawClockIcon(int x, int y) {
  tft.drawCircle(x, y, 7, TFT_WHITE);
  tft.drawLine(x, y, x, y - 4, TFT_WHITE);
  tft.drawLine(x, y, x + 3, y, TFT_WHITE);
}

void drawTempPair(int cx, int cy, float current, float target) {
  tft.setTextPadding(0);

  String curStr = String((int)round(current));
  String tgtStr = "/" + String((int)round(target));

  int curW = tft.textWidth(curStr, 2);
  int tgtW = tft.textWidth(tgtStr, 2);
  int totalW = curW + tgtW;
  int startX = cx - (totalW / 2);

  tft.fillRect(cx - 32, cy - 9, 64, 18, TFT_BLACK);

  tft.setTextDatum(ML_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(curStr, startX, cy, 2);

  tft.setTextColor(TFT_ORANGE, TFT_BLACK);
  tft.drawString(tgtStr, startX + curW, cy, 2);
}

void drawStaticLayout() {
  tft.setRotation(cfg.rotation);
  tft.fillScreen(TFT_BLACK);

  int arcStart = (cfg.arc_mode == 1) ? 0 : 60;
  int arcEnd   = (cfg.arc_mode == 1) ? 360 : 300;
  tft.drawSmoothArc(120, 120, 116, 106, arcStart, arcEnd, 0x2104, TFT_BLACK, true);

  drawNozzleIcon(72, 100);
  drawBedIcon(168, 100);
  drawClockIcon(88, 198);

  lastPct = -1;
  lastArcMode = cfg.arc_mode;
  isClockScreen = false;
}

String formatRemainingTime() {
  if (printState != "printing" || progress <= 0.01f || printDuration <= 5.0f) {
    return "--m";
  }
  float totalEstimated = printDuration / progress;
  long remSec = (long)(totalEstimated - printDuration);
  if (remSec < 0) remSec = 0;

  long mins = remSec / 60;
  if (mins < 60) {
    return String(mins) + "m";
  }
  long hrs = mins / 60;
  mins = mins % 60;
  return String(hrs) + "h " + String(mins) + "m";
}

// --- Отрисовка экрана настольных часов / Idle Clock UI ---
void drawClockUI(bool fullRedraw) {
  if (fullRedraw || !isClockScreen) {
    tft.setRotation(cfg.rotation);
    tft.fillScreen(TFT_BLACK);
    tft.drawCircle(120, 120, 116, 0x18E3);
    isClockScreen = true;
  }

  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  tft.setTextDatum(MC_DATUM);

  bool previewActive = (millis() < clockPreviewUntil);
  tft.setTextPadding(120);
  tft.setTextColor(previewActive ? TFT_YELLOW : TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString(previewActive ? "clock preview" : printState, 120, 48, 2);

  if (t->tm_year > 100) {
    char hourBuf[8];
    char minBuf[8];
    snprintf(hourBuf, sizeof(hourBuf), "%02d", (t->tm_hour & 0x1F));
    snprintf(minBuf, sizeof(minBuf), "%02d", (t->tm_min & 0x3F));

    tft.setTextPadding(64);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(hourBuf, 78, 110, 6);

    tft.setTextPadding(16);
    tft.setTextColor(colonState ? TFT_GREEN : TFT_BLACK, TFT_BLACK);
    tft.drawString(":", 120, 108, 6);

    tft.setTextPadding(64);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(minBuf, 162, 110, 6);

    char dateBuf[32];
    snprintf(dateBuf, sizeof(dateBuf), "%02d.%02d.%04d  %s",
             (t->tm_mday & 0x3F),
             ((t->tm_mon + 1) & 0x0F),
             ((t->tm_year + 1900) & 0x3FFF),
             daysOfWeek[t->tm_wday & 0x07]);
    tft.setTextPadding(150);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(dateBuf, 120, 154, 2);
  } else {
    tft.setTextPadding(160);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Syncing time...", 120, 112, 2);
  }

  tft.setTextPadding(130);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  String temps = "N: " + String((int)round(nozzleTemp)) + "C   B: " + String((int)round(bedTemp)) + "C";
  tft.drawString(temps, 120, 190, 2);

  tft.setTextPadding(0);
}

void updateUI() {
  bool previewActive = (millis() < clockPreviewUntil);
  bool isPrinterBusy = (printState == "printing" || printState == "paused" || nozzleTarget > 0.5f || bedTarget > 0.5f);

  if (previewActive || (cfg.clock_idle == 1 && !isPrinterBusy)) {
    drawClockUI(!isClockScreen);
    return;
  }

  if (isClockScreen) {
    drawStaticLayout();
  }

  tft.setTextDatum(MC_DATUM);

  // 1. Процент прогресса сверху / Progress percentage at the top (Font 4)
  int pct = (int)round(progress * 100.0f);
  tft.setTextPadding(84);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString(String(pct) + "%", 120, 42, 4);

  // 2. Статус печати / Print status (Font 2)
  uint16_t stColor = TFT_GREEN;
  if (printState == "paused") stColor = TFT_YELLOW;
  else if (printState == "error" || printState == "offline" || printState.startsWith("HTTP")) stColor = TFT_RED;
  else if (printState != "printing") stColor = TFT_LIGHTGREY;

  tft.setTextPadding(100);
  tft.setTextColor(stColor, TFT_BLACK);
  tft.drawString(printState, 120, 74, 2);

  // 3. Дуга прогресса / Progress arc
  if (pct != lastPct || cfg.arc_mode != lastArcMode) {
    int arcStart = (cfg.arc_mode == 1) ? 0 : 60;
    int arcEnd   = (cfg.arc_mode == 1) ? 360 : 300;
    tft.drawSmoothArc(120, 120, 116, 106, arcStart, arcEnd, 0x2104, TFT_BLACK, true);
    if (progress > 0.005f) {
      int endAngle = arcStart + (int)((arcEnd - arcStart) * progress);
      if (endAngle > arcEnd) endAngle = arcEnd;
      tft.drawSmoothArc(120, 120, 116, 106, arcStart, endAngle, TFT_GREEN, TFT_BLACK, true);
    }
    lastPct = pct;
    lastArcMode = cfg.arc_mode;
  }

  // 4. Температуры сопла и стола / Nozzle and bed temperatures (Font 2)
  drawTempPair(72, 124, nozzleTemp, nozzleTarget);
  drawTempPair(168, 124, bedTemp, bedTarget);

  // 5. Скорость вентилятора обдува модели / Part cooling fan speed (Font 2)
  float normalizedFan = (fanValue <= 1.0f) ? fanValue : (fanValue / 255.0f);
  int fanPct = constrain((int)round(normalizedFan * 100.0f), 0, 100);
  tft.setTextDatum(MC_DATUM);
  tft.setTextPadding(110);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("FAN: " + String(fanPct) + "%", 120, 158, 2);

  // 6. Оставшееся время / Remaining print time (Font 4)
  tft.setTextPadding(75);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString(formatRemainingTime(), 132, 198, 4);

  tft.setTextPadding(0);
}

// --- Опрос Moonraker API / Query Moonraker API ---
void fetchPrinterData() {
  if (WiFi.status() != WL_CONNECTED) {
    printState = "no wifi";
    return;
  }

  WiFiClient client;
  HTTPClient http;

  // =========================================================================
  // НАСТРОЙКА ВЕНТИЛЯТОРА ДЛЯ РАЗНЫХ ПРИНТЕРОВ KLIPPER / FAN CONFIG NOTE:
  // По умолчанию опрашивается `output_pin fan0` (используется в Creality K1/K2).
  // Если на вашем Klipper-принтере вентилятор обдува модели всегда показывает 0%,
  // замените `output_pin%20fan0` в URL ниже на `fan` (стандартная секция [fan] в Klipper):
  //
  // By default, `output_pin fan0` is queried (used on Creality K1/K2 series).
  // If your Klipper printer always shows FAN: 0%, replace `output_pin%20fan0`
  // in the URL below with `fan` (standard [fan] section in Klipper):
  // String url = "http://" + String(cfg.printer_ip) + ":7125/printer/objects/query?heater_bed&extruder&print_stats&virtual_sdcard&fan";
  // =========================================================================
  String url = "http://" + String(cfg.printer_ip) + ":7125/printer/objects/query?heater_bed&extruder&print_stats&virtual_sdcard&output_pin%20fan0";

  http.setTimeout(1500);
  if (http.begin(client, url)) {
    int httpCode = http.GET();
    if (httpCode == HTTP_CODE_OK) {
      String payload = http.getString();

      JsonDocument filter;
      filter["result"]["status"]["extruder"]["temperature"] = true;
      filter["result"]["status"]["extruder"]["target"] = true;
      filter["result"]["status"]["heater_bed"]["temperature"] = true;
      filter["result"]["status"]["heater_bed"]["target"] = true;
      filter["result"]["status"]["print_stats"]["state"] = true;
      filter["result"]["status"]["print_stats"]["print_duration"] = true;
      filter["result"]["status"]["virtual_sdcard"]["progress"] = true;

      // Если заменили объект в URL на `fan`, закомментируйте строку с `output_pin fan0`
      // и раскомментируйте строку с `["fan"]["speed"]`:
      // If you changed the URL object to `fan`, comment out `output_pin fan0`
      // and uncomment `["fan"]["speed"]`:
      filter["result"]["status"]["output_pin fan0"]["value"] = true;
      // filter["result"]["status"]["fan"]["speed"] = true;

      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload, DeserializationOption::Filter(filter));

      if (!error) {
        JsonObject status = doc["result"]["status"];
        nozzleTemp = status["extruder"]["temperature"] | 0.0f;
        nozzleTarget = status["extruder"]["target"] | 0.0f;
        bedTemp = status["heater_bed"]["temperature"] | 0.0f;
        bedTarget = status["heater_bed"]["target"] | 0.0f;
        printState = status["print_stats"]["state"] | "standby";
        printDuration = status["print_stats"]["print_duration"] | 0.0f;
        progress = status["virtual_sdcard"]["progress"] | 0.0f;

        // Чтение скорости вентилятора (также переключите на строку ниже при использовании `[fan]`):
        // Read fan speed (also switch to the line below if using standard `[fan]`):
        fanValue = status["output_pin fan0"]["value"] | 0.0f;
        // fanValue = status["fan"]["speed"] | 0.0f;
      } else {
        printState = "json err";
      }
    } else {
      printState = "HTTP " + String(httpCode);
    }
    http.end();
  } else {
    printState = "offline";
  }
}

// --- Веб-интерфейс настроек / Web Configuration UI (RU / EN) ---
void handleRoot() {
  bool en = (cfg.lang == 1);

  String html = "<!DOCTYPE html><html lang='" + String(en ? "en" : "ru") + "'><head><meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width,initial-scale=1'>";
  html += "<title>KlipperHelper Monitor</title><style>";
  html += "body{font-family:sans-serif;background:#121212;color:#eee;max-width:420px;margin:20px auto;padding:15px;}";
  html += "h2{color:#00e676;text-align:center;margin-bottom:14px;}label{display:block;margin-top:14px;color:#aaa;font-size:14px;}";
  html += "input,select{width:100%;padding:10px;margin-top:5px;border-radius:8px;border:1px solid #333;background:#1e1e1e;color:#fff;box-sizing:border-box;font-size:16px;}";
  html += "button{width:100%;padding:12px;margin-top:16px;border:none;border-radius:8px;background:#00e676;color:#000;font-weight:bold;font-size:16px;cursor:pointer;}";
  html += ".preview{background:#29b6f6;color:#000;margin-top:12px;}";
  html += ".danger{background:#ff5252;color:#fff;margin-top:12px;}";
  html += ".card{background:#1a1a1a;padding:12px;border-radius:8px;margin-bottom:15px;font-size:14px;line-height:1.5;}";
  html += "</style></head><body><h2>KlipperHelper</h2>";
  
  html += "<div class='card'><b>" + String(en ? "Status:" : "Статус:") + "</b> " + printState + "<br>";
  html += "<b>" + String(en ? "Nozzle:" : "Сопло:") + "</b> " + String((int)round(nozzleTemp)) + "/" + String((int)round(nozzleTarget)) + "°C | ";
  html += "<b>" + String(en ? "Bed:" : "Стол:") + "</b> " + String((int)round(bedTemp)) + "/" + String((int)round(bedTarget)) + "°C<br>";
  html += "<b>" + String(en ? "Progress:" : "Прогресс:") + "</b> " + String((int)round(progress * 100)) + "%</div>";

  html += "<form action='/preview_clock' method='POST'>";
  html += "<button type='submit' class='preview'>" + String(en ? "Test Clock on Screen (10 sec)" : "Тест часов на экране (10 сек)") + "</button></form>";

  html += "<form action='/save' method='POST'>";

  html += "<label>" + String(en ? "Interface Language:" : "Язык интерфейса:") + "</label><select name='lang'>";
  html += "<option value='0'" + String(cfg.lang == 0 ? " selected" : "") + ">Русский (RU)</option>";
  html += "<option value='1'" + String(cfg.lang == 1 ? " selected" : "") + ">English (EN)</option></select>";

  html += "<label>" + String(en ? "Klipper Printer IP Address:" : "IP-адрес принтера (Moonraker):") + "</label>";
  html += "<input type='text' name='ip' value='" + String(cfg.printer_ip) + "' required>";

  html += "<label>" + String(en ? "Idle Clock Mode:" : "Часы при простое принтера:") + "</label><select name='clk'>";
  html += "<option value='1'" + String(cfg.clock_idle == 1 ? " selected" : "") + ">" + String(en ? "On (show large clock)" : "Вкл (показывать крупные часы)") + "</option>";
  html += "<option value='0'" + String(cfg.clock_idle == 0 ? " selected" : "") + ">" + String(en ? "Off (always printer screen)" : "Выкл (всегда экран принтера)") + "</option></select>";

  html += "<label>" + String(en ? "Timezone (UTC):" : "Часовой пояс (UTC):") + "</label><select name='tz'>";
  for (int i = -12; i <= 14; i++) {
    String sign = (i >= 0) ? "+" : "";
    html += "<option value='" + String(i) + "'" + String(cfg.tz_offset == i ? " selected" : "") + ">UTC " + sign + String(i) + "</option>";
  }
  html += "</select>";

  html += "<label>" + String(en ? "Screen Rotation:" : "Ориентация экрана:") + "</label><select name='rot'>";
  html += "<option value='0'" + String(cfg.rotation == 0 ? " selected" : "") + ">" + String(en ? "0° (standard)" : "0° (стандарт)") + "</option>";
  html += "<option value='1'" + String(cfg.rotation == 1 ? " selected" : "") + ">" + String(en ? "90° (default)" : "90° (по умолчанию)") + "</option>";
  html += "<option value='2'" + String(cfg.rotation == 2 ? " selected" : "") + ">180°</option>";
  html += "<option value='3'" + String(cfg.rotation == 3 ? " selected" : "") + ">" + String(en ? "270° (inverted)" : "270° (перевернутая)") + "</option></select>";

  html += "<label>" + String(en ? "Progress Bar Style:" : "Шкала прогресса:") + "</label><select name='arc'>";
  html += "<option value='0'" + String(cfg.arc_mode == 0 ? " selected" : "") + ">" + String(en ? "Arc (8 to 4 o'clock)" : "Дуга (с 8 до 4 часов)") + "</option>";
  html += "<option value='1'" + String(cfg.arc_mode == 1 ? " selected" : "") + ">" + String(en ? "Full Circle (360°)" : "Полный круг (360°)") + "</option></select>";

  html += "<label>" + String(en ? "Polling Interval (ms):" : "Интервал опроса (мс):") + "</label>";
  html += "<input type='number' name='upd' min='500' max='10000' step='100' value='" + String(cfg.update_ms) + "'>";

  html += "<button type='submit'>" + String(en ? "Save & Apply" : "Сохранить и применить") + "</button></form>";
  html += "<form action='/reset_wifi' method='POST' onsubmit=\"return confirm('" + String(en ? "Reset Wi-Fi settings and reboot?" : "Сбросить настройки Wi-Fi и перезагрузить?") + "');\">";
  html += "<button type='submit' class='danger'>" + String(en ? "Reset Wi-Fi (AP Mode)" : "Сбросить Wi-Fi (режим точки доступа)") + "</button></form></body></html>";

  server.send(200, "text/html", html);
}

void handlePreviewClock() {
  clockPreviewUntil = millis() + 10000UL;
  drawClockUI(true);
  server.sendHeader("Location", "/");
  server.send(303);
}

void handleSave() {
  if (server.hasArg("lang")) cfg.lang = server.arg("lang").toInt();
  if (server.hasArg("ip")) strlcpy(cfg.printer_ip, server.arg("ip").c_str(), sizeof(cfg.printer_ip));
  if (server.hasArg("clk")) cfg.clock_idle = server.arg("clk").toInt();
  if (server.hasArg("tz")) {
    cfg.tz_offset = server.arg("tz").toInt();
    applyTimezone();
  }
  if (server.hasArg("rot")) cfg.rotation = server.arg("rot").toInt();
  if (server.hasArg("arc")) cfg.arc_mode = server.arg("arc").toInt();
  if (server.hasArg("upd")) cfg.update_ms = constrain(server.arg("upd").toInt(), 500, 10000);

  saveConfig();
  clockPreviewUntil = 0;
  isClockScreen = false;
  drawStaticLayout();
  updateUI();

  server.sendHeader("Location", "/");
  server.send(303);
}

void handleResetWiFi() {
  bool en = (cfg.lang == 1);
  String msg = en ? "<meta charset='UTF-8'><h3>Wi-Fi reset. Rebooting into KlipperHelper_Setup AP mode...</h3>"
                  : "<meta charset='UTF-8'><h3>Wi-Fi сброшен. Перезагрузка в режим точки доступа KlipperHelper_Setup...</h3>";
  server.send(200, "text/html", msg);
  delay(1000);
  WiFiManager wm;
  wm.resetSettings();
  ESP.restart();
}

void configModeCallback(WiFiManager* myWiFiManager) {
  tft.fillScreen(TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.drawString("WiFi Setup Mode", 120, 75, 4);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Connect to WiFi:", 120, 115, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("KlipperHelper_Setup", 120, 140, 2);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("IP: 192.168.4.1", 120, 170, 2);
}

void setup() {
  Serial.begin(115200);
  loadConfig();

  tft.init();
  tft.setRotation(cfg.rotation);
  tft.fillScreen(TFT_BLACK);

  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Starting WiFi...", 120, 120, 4);

  WiFiManager wm;
  wm.setAPCallback(configModeCallback);

  WiFiManagerParameter custom_printer_ip("printer_ip", "Klipper Printer IP", cfg.printer_ip, 32);
  wm.addParameter(&custom_printer_ip);

  if (!wm.autoConnect("KlipperHelper_Setup")) {
    ESP.restart();
  }

  if (String(custom_printer_ip.getValue()) != String(cfg.printer_ip)) {
    strlcpy(cfg.printer_ip, custom_printer_ip.getValue(), sizeof(cfg.printer_ip));
    saveConfig();
  }

  applyTimezone();

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("Connected!", 120, 90, 4);
  tft.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  tft.drawString("Web Panel IP:", 120, 125, 2);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString(WiFi.localIP().toString(), 120, 152, 4);
  delay(3000);

  server.on("/", handleRoot);
  server.on("/preview_clock", HTTP_POST, handlePreviewClock);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/reset_wifi", HTTP_POST, handleResetWiFi);
  server.begin();

  drawStaticLayout();
  fetchPrinterData();
  updateUI();
}

void loop() {
  server.handleClient();

  if (isClockScreen && millis() - lastClockTick >= 1000) {
    lastClockTick = millis();
    colonState = !colonState;
    drawClockUI(false);
  }

  if (millis() - lastUpdate >= (unsigned long)cfg.update_ms) {
    lastUpdate = millis();
    fetchPrinterData();
    updateUI();
  }
}