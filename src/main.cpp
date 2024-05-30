#include "Adafruit_Sensor.h"
#include <DHT.h>
#include <TM1637Display.h>
#include <FastLED.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// Порты
#define DIR 5
#define CLK 4
#define FOTO A0
#define DHTPIN D5
#define LED_PIN_1 D7

// Параметры ленты
#define NUM_LEDS_1 53
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

// Объекты для работы с устройствами
DHT dht(DHTPIN, DHT11);
TM1637Display display(CLK, DIR);
CRGB leds[NUM_LEDS_1];

// Интервалы обновления данных
unsigned long previousMillis = 0;
unsigned long displayPreviousMillis = 0;
const long interval = 2000;
const long displayInterval = 1000;

bool showTemperature = true;

float humidity;
float temperature;
int brightness;
int bri;

String color = "#ff0000";  // Цвет по умолчанию
String mode = "static";   // Режим по умолчанию
bool autoBrightness = false;

// Параметры Wi-Fi
const char* ssid = "Lump";
const char* password = "123456789";
const IPAddress staticIP(192, 168, 4, 1);
ESP8266WebServer server(80);

enum MODE {
  TEMP_SET,
  RGB_SET,
  RADUGA_MODE
} mood;

enum DATA {
  HUM,
  TEMP,
  BRIGHT
};

void UpdateTMP();
void UpdateFoto();
float UpdateData(DATA data);

void handleRoot() {
  String html = "<html><head><meta charset=\"UTF-8\">";
  html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
  html += "<title>Sensor Data</title></head><body>";
  html += "<h1>Sensor Data</h1>";
  html += "<p>Temperature: <span id='temperature'></span> &#8451;</p>";
  html += "<p>Humidity: <span id='humidity'></span> %</p>";
  html += "<p>Brightness: <span id='brightness'></span></p>";
  html += "<h2>LED Control</h2>";
  html += "<label for='color'>Select Color:</label>";
  html += "<input type='color' id='color' name='color' value='#ff0000'><br>";
  html += "<label for='mode'>Mode:</label>";
  html += "<select id='mode' name='mode'>";
  html += "<option value='static'>Static Color</option>";
  html += "<option value='rainbow'>Rainbow</option>";
  html += "<option value='rgb'>RGB</option>";
  html += "</select><br>";
  html += "<label for='brightness'>Brightness:</label>";
  html += "<input type='range' id='brightnessSlider' name='brightness' min='0' max='255'><br>";
  html += "<label for='autoBrightness'>Auto Brightness:</label>";
  html += "<input type='checkbox' id='autoBrightness' name='autoBrightness'><br>";
  html += "<script>";
  html += "function fetchData() {";
  html += "  fetch('/data').then(response => response.json()).then(data => {";
  html += "    document.getElementById('temperature').textContent = data.temperature;";
  html += "    document.getElementById('humidity').textContent = data.humidity;";
  html += "    document.getElementById('brightness').textContent = data.brightness;";
  html += "  });";
  html += "}";
  html += "setInterval(fetchData, 2000);";  // Обновление каждые 2 секунды
  html += "fetchData();";  // Начальное получение данных при загрузке страницы
  html += "document.getElementById('color').addEventListener('change', function() {";
  html += "  fetch('/setColor?color=' + encodeURIComponent(this.value));";
  html += "});";
  html += "document.getElementById('mode').addEventListener('change', function() {";
  html += "  fetch('/setMode?mode=' + encodeURIComponent(this.value));";
  html += "});";
  html += "document.getElementById('brightnessSlider').addEventListener('input', function() {";
  html += "  fetch('/setBrightness?brightness=' + this.value);";
  html += "});";
  html += "document.getElementById('autoBrightness').addEventListener('change', function() {";
  html += "  fetch('/setAutoBrightness?auto=' + (this.checked ? '1' : '0'));";
  html += "});";
  html += "</script>";
  html += "</body></html>";
  
  server.send(200, "text/html", html);
}



void handleData() {
  String json = "{";
  json += "\"temperature\":" + String(temperature) + ",";
  json += "\"humidity\":" + String(humidity) + ",";
  json += "\"brightness\":" + String(brightness);
  json += "}";
  server.send(200, "application/json", json);
}

void handleSetColor() {
  if (server.hasArg("color")) {
    color = server.arg("color");
    Serial.println("Color set to: " + color);
  }
  server.send(200, "text/plain", "OK");
}

void handleSetMode() {
  if (server.hasArg("mode")) {
    mode = server.arg("mode");
    Serial.println("Mode set to: " + mode);
  }
  server.send(200, "text/plain", "OK");
}

void handleSetBrightness() {
  if (server.hasArg("brightness")) {
    bri = server.arg("brightness").toInt();
    Serial.println("Brightness set to: " + String(brightness));
  }
  server.send(200, "text/plain", "OK");
}

void handleSetAutoBrightness() {
  if (server.hasArg("auto")) {
    autoBrightness = server.arg("auto") == "1";
    Serial.println("Auto Brightness set to: " + String(autoBrightness));
  }
  server.send(200, "text/plain", "OK");
}


void setup() {
  Serial.begin(9600);
  
  // Инициализация датчика DHT11
  dht.begin();
  
  // Инициализация дисплея TM1637
  display.setBrightness(0x0f);  // Максимальная яркость

  // Инициализация ленты WS2812B
  FastLED.addLeds<LED_TYPE, LED_PIN_1, COLOR_ORDER>(leds, NUM_LEDS_1);
  

  // Настройка точки доступа Wi-Fi
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(staticIP, staticIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(ssid, password);

  Serial.print("Точка доступа запущена. IP: ");
  Serial.println(WiFi.softAPIP());

  // Настройка обработчиков веб-сервера
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/setColor", handleSetColor);
  server.on("/setMode", handleSetMode);
  server.on("/setBrightness", handleSetBrightness);
  server.on("/setAutoBrightness", handleSetAutoBrightness);
  server.begin();
  Serial.println("Веб-сервер запущен");
}

void loop() {

  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= 10) {
    previousMillis = currentMillis;
    // Обновление данных с сенсоров
    UpdateTMP();
    UpdateFoto();
  }
  
  server.handleClient();


  

  
}

void UpdateTMP() {
  unsigned long currentMillis = millis();
  humidity = dht.readHumidity();
  temperature = dht.readTemperature();

  // Проверка на ошибки чтения
  if (isnan(humidity) || isnan(temperature)) {
    Serial.println("Ошибка чтения с датчика DHT11!");
    return;
  } 

    // Обновление данных на дисплее TM1637
  if (currentMillis - displayPreviousMillis >= displayInterval) {
    displayPreviousMillis = currentMillis;
    display.clear();  // Очистка дисплея перед отображением новых данных

    if (showTemperature) {
      display.showNumberDec((int)temperature);
    } else {
      display.showNumberDec((int)humidity);
    }

    showTemperature = !showTemperature;  // Переключение между показом температуры и влажности
  }
}

void UpdateFoto() {
  brightness = map(analogRead(FOTO), 0, 1023, 0, 100);
  

  if (autoBrightness) {
    bri = map(brightness, 0, 100, 255, 0);
  }
  

  unsigned long currentMillis = millis();

  if (mode == "static") {
    mood = MODE::TEMP_SET;
  } else if (mode == "rgb") {
    mood = MODE::RGB_SET;
  } else if (mode == "rainbow") {
    mood = MODE::RADUGA_MODE;
  }

  switch (mood)
  {
  case MODE::TEMP_SET: {
    for (int i = 0; i < NUM_LEDS_1; i++) {
      // Преобразование температуры в цвет (пример, можно изменить по вкусу)
      byte red = map(temperature, 15, 30, 0, 255);
      byte green = 0;
      byte blue = map(temperature, 15, 30, 255, 0);
      
      leds[i] = CRGB(red, green, blue);

      // Преобразование уровня освещенности в яркость
      
      leds[i].nscale8_video(bri);
    }

    // Обновление состояния светодиодов
    FastLED.show();
    break;
  }

  case MODE::RGB_SET: {
    CRGB selectedColor = CRGB::Black;
    long number = strtol(&color[1], NULL, 16);  // Преобразование HEX в long
    selectedColor = CRGB((number >> 16) & 0xFF, (number >> 8) & 0xFF, number & 0xFF);
    
    for (int i = 0; i < NUM_LEDS_1; i++) {
      leds[i] = selectedColor;
    }
    FastLED.setBrightness(bri);
    FastLED.show();
    break;
  }

  case MODE::RADUGA_MODE: {
    fill_rainbow(leds, NUM_LEDS_1, currentMillis / 10, 7);
    FastLED.setBrightness(bri);
    FastLED.show();
    break;
  }
  default:
    break;
  }
  
}

float UpdateData(DATA data) {
  switch (data)
  {
  case DATA::HUM:
    UpdateTMP();
    return humidity;
  
  case DATA::TEMP:
    UpdateTMP();
    return temperature;
  
  case DATA::BRIGHT:
    UpdateFoto();
    return brightness;
  default:
  return 0;
  }
  return 0;
}
