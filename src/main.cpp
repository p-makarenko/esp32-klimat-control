#include <Arduino.h>

#include "config.h"
#include "system_core.h"
#include "sensor_manager.h"
#include "actuator_manager.h"
#include "web_interface.h"
#include "data_storage.h"
#include "advanced_climate_logic.h"
#include "utility_functions.h"
#include "global_declarations.h"
#include <Arduino.h>

// ГЛОБАЛЬНІ ЗМІННІ
SystemConfig config;
SensorData sensorData;
HeatingState heatingState;
VentilationState ventState;
HumidifierState humidifierState;

extern int historyIndex;
extern bool historyInitialized;
extern HistoryData history[HISTORY_BUFFER_SIZE];

extern bool compactMode;

// НАЛАШТУВАННЯ СИСТЕМИ
void setup() {
  Serial.begin(115200);
  delay(500);  // Даємо час Serial ініціалізуватись
  
  Serial.println("\n\n==========================================");
  Serial.println("  Клімат-контроль системи вентиляції");
  Serial.println("  " VERSION);
  Serial.println("  " VERSION_COMMENT);
  Serial.print("  Зібрано: ");
  Serial.print(BUILD_DATE);
  Serial.print(" ");
  Serial.println(BUILD_TIME);
  Serial.printf("  Рядків коду: %d\n", TOTAL_CODE_LINES);
  Serial.printf("  Розмір прошивки: %d KB\n", FIRMWARE_SIZE_KB);
  Serial.println("==========================================");
  
  initMutexes();
  loadConfiguration();
  
  Serial.println("\n=== ІНІЦІАЛІЗАЦІЯ WI-FI ===");
  initWiFi();
  
  Serial.println("\n=== ІНІЦІАЛІЗАЦІЯ ЧАСУ ===");
  initTime();
  
  Serial.println("\n=== ІНІЦІАЛІЗАЦІЯ ДАТЧИКІВ ===");
  if (!initTemperatureSensors()) {
    Serial.println("⚠️  Помилка: Проблема з датчиками температури!");
  } else {
    Serial.println("✅  Датчики температури готові");
  }
  
  if (!initBME280()) {
    Serial.println("⚠️  Помилка: Проблема з датчиком BME280!");
  } else {
    Serial.println("✅  Датчик BME280 готовий");
  }
  
  Serial.println("\n=== ІНІЦІАЛІЗАЦІЯ ІСПОЛНЮВАЧІВ ===");
  initGPIO();
  initPWM();
  initServo();
  Serial.println("✅  Виконавчі пристрої готові");
  
  Serial.println("\n=== ІНІЦІАЛІЗАЦІЯ ЛОГІКИ ===");
  initAdvancedLogic();
  
  Serial.println("\n=== СТВОРЕННЯ ЗАВДАНЬ ===");
  createTasks();
  
  Serial.println("\n=== ІНІЦІАЛІЗАЦІЯ СИСТЕМИ ===");
  heatingState.active = false;
  heatingState.pumpPower = 0;
  heatingState.fanPower = 0;
  heatingState.extractorPower = 0;
  heatingState.forceMode = false;
  heatingState.emergencyMode = false;
  heatingState.manualMode = false;
  heatingState.adaptive_heating_active = false;
  
  humidifierState.active = false;
  humidifierState.startTime = 0;
  humidifierState.lastCycle = 0;
  humidifierState.cyclesToday = 0;
  
  if (config.pumpMinPercent == 0) config.pumpMinPercent = PUMP_MIN_DEFAULT;
  if (config.pumpMaxPercent == 0) config.pumpMaxPercent = PUMP_MAX_DEFAULT;
  if (config.fanMaxPercent == 0) config.fanMaxPercent = FAN_MAX_DEFAULT;
  if (config.extractorMinPercent == 0) config.extractorMinPercent = EXTRACTOR_MIN_DEFAULT;
  if (config.extractorMaxPercent == 0) config.extractorMaxPercent = EXTRACTOR_MAX_DEFAULT;
  
  config.extractorTimer.cycleStart = 0;
  config.extractorTimer.state = false;
  config.extractorTimer.lastChange = 0;
  
  Serial.println("\n▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄");
  Serial.println("║  КЛІМАТ-КОНТРОЛЬ ГОТОВИЙ ДО РОБОТИ  ║");
  Serial.println("▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀");
  Serial.println("Використовуйте:");
  Serial.println("  'm' - Меню керування");
  Serial.println("  's' - Статус системи");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("  Веб-інтерфейс:");
    Serial.println("    http://klimat.local (рекомендовано)");
    Serial.println("    http://" + WiFi.localIP().toString());
  } else {
    Serial.println("  Веб-інтерфейс: http://192.168.4.1 (режим точки доступу)");
  }
  Serial.println("▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄\n");
  
  setHeatingPower(0, 0, 0);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  
  Serial.println("✅  Початковий стан встановлений");
  Serial.println("✅  Всі системи ініціалізовані");
}

// ГОЛОВНИЙ ЦИКЛ
void loop() {
  static unsigned long lastWiFiCheck = 0;
  const unsigned long wifiCheckInterval = 30000; // Перевірка кожні 30 секунд
  
  // Автоматичне перепідключення WiFi
  unsigned long now = millis();
  if (now - lastWiFiCheck >= wifiCheckInterval) {
    lastWiFiCheck = now;
    
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("");
      Serial.println("========================================");
      Serial.println("   WiFi втрачено - перепідключення");
      Serial.println("========================================");
      
      // Читаємо збережені мережі з Preferences
      preferences.begin("wifi", true);
      int networkCount = preferences.getInt("netCount", 0);
      
      // Масив для збережених мереж
      struct SavedNetwork {
        String ssid;
        String password;
      };
      SavedNetwork savedNetworks[5];
      
      // Якщо немає збережених мереж, використовуємо KNOWN_NETWORKS
      if (networkCount == 0) {
        Serial.println("Збережених мереж немає, використовую стандартні");
        networkCount = KNOWN_NETWORKS_COUNT;
        for (int i = 0; i < networkCount && i < 5; i++) {
          savedNetworks[i].ssid = KNOWN_NETWORKS[i].ssid;
          savedNetworks[i].password = KNOWN_NETWORKS[i].password;
        }
      } else {
        Serial.printf("Знайдено збережених мереж: %d\n", networkCount);
        for (int i = 0; i < networkCount && i < 5; i++) {
          savedNetworks[i].ssid = preferences.getString(("ssid" + String(i)).c_str(), "");
          savedNetworks[i].password = preferences.getString(("pass" + String(i)).c_str(), "");
          Serial.printf("  %d: %s\n", i+1, savedNetworks[i].ssid.c_str());
        }
      }
      preferences.end();
      
      // Відключаємося від поточної мережі перед скануванням
      WiFi.disconnect();
      delay(100);
      
      // Скануємо доступні мережі
      Serial.println("Сканування мереж...");
      int n = WiFi.scanNetworks();
      
      if (n < 0) {
        Serial.printf("Помилка сканування: %d\n", n);
        Serial.println("Спроба повторного сканування через 10 сек...");
        delay(10000);
        lastWiFiCheck = millis();
        return;
      }
      
      Serial.printf("Знайдено мереж: %d\n", n);
      
      if (n > 0) {
        // Показуємо всі знайдені мережі
        for (int i = 0; i < n; i++) {
          Serial.printf("  %d: %s (%d dBm)\n", i+1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
        }
        
        // Шукаємо найкращу зі збережених мереж
        bool reconnected = false;
        int bestRSSI = -999;
        String bestSSID = "";
        String bestPassword = "";
        
        for (int i = 0; i < networkCount && i < 5; i++) {
          if (savedNetworks[i].ssid.length() == 0) continue;
          
          for (int j = 0; j < n; j++) {
            if (WiFi.SSID(j) == savedNetworks[i].ssid) {
              Serial.printf("Знайдено збережену мережу: %s (%d dBm)\n", savedNetworks[i].ssid.c_str(), WiFi.RSSI(j));
              if (WiFi.RSSI(j) > bestRSSI) {
                bestRSSI = WiFi.RSSI(j);
                bestSSID = savedNetworks[i].ssid;
                bestPassword = savedNetworks[i].password;
              }
            }
          }
        }
        
        if (bestSSID.length() > 0) {
          Serial.printf("Connecting to: %s (signal: %d dBm)\n", bestSSID.c_str(), bestRSSI);
          
          WiFi.disconnect();
          delay(100);
          WiFi.begin(bestSSID.c_str(), bestPassword.c_str());
          
          int attempts = 0;
          while (WiFi.status() != WL_CONNECTED && attempts < 20) {
            delay(500);
            Serial.print(".");
            attempts++;
          }
          Serial.println("");
          
          if (WiFi.status() == WL_CONNECTED) {
            Serial.println("========================================");
            Serial.println("   WiFi підключено успішно!");
            Serial.println("========================================");
            Serial.printf("SSID: %s\n", bestSSID.c_str());
            Serial.printf("IP:   %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
            Serial.println("========================================");
            reconnected = true;
          } else {
            Serial.println("Не вдалось підключитись до: " + bestSSID);
          }
        } else {
          Serial.println("Жодна збережена мережа не знайдена!");
        }
        
        if (!reconnected) {
          Serial.println("Перепідключення не вдалось");
        }
      } else {
        Serial.println("Жодної мережі не знайдено");
      }
      Serial.println("========================================");
    }
  }
  
  processAdvancedSerialCommand();
  autoPrintStatus();
  addToHistory();
  checkEmergencyTimeout();
  delay(100);
}

// ДОДАТКОВІ ФУНКЦІЇ
void printSystemInfo() {
  Serial.println("\n▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄");
  Serial.println("║  ІНФОРМАЦІЯ ПРО СИСТЕМУ  ║");
  Serial.println("▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀");
  Serial.printf("  IP адреса: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("  Вільна пам'ять: %lu KB\n", ESP.getFreeHeap() / 1024);
  Serial.printf("  Час роботи: %lu сек\n", millis() / 1000);
  Serial.printf("  Записів в історії: %d\n", historyIndex);
  Serial.printf("  Режим Serial: %s\n", compactMode ? "Компактний" : "Розширений");
  Serial.printf("  Насос (A): %d%% (min:%d%%, max:%d%%)\n", 
                (heatingState.pumpPower * 100) / 255, 
                config.pumpMinPercent, config.pumpMaxPercent);
  Serial.printf("  Вентилятор (B): %d%% (min:%d%%, max:%d%%)\n", 
                (heatingState.fanPower * 100) / 255,
                config.fanMinPercent, config.fanMaxPercent);
  Serial.printf("  Витяжка (C): %d%% (min:%d%%, max:%d%%)\n", 
                (heatingState.extractorPower * 100) / 255,
                config.extractorMinPercent, config.extractorMaxPercent);
  Serial.println("▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄\n");
}

void emergencyStop() {
  Serial.println("\n⚠️⚠️⚠️  АВАРІЙНЕ ВИМКНЕННЯ СИСТЕМИ  ⚠️⚠️⚠️");
  
  setHeatingPower(0, 0, 0);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  moveServoSmooth(SERVO_CLOSED_ANGLE);
  
  heatingState.emergencyMode = true;
  heatingState.manualMode = false;
  heatingState.forceMode = false;
  
  Serial.println("✅  Всі системи відключені");
  Serial.println("⚠️  Система в аварійному режимі");
}

void runSystemTest() {
  Serial.println("\n▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄");
  Serial.println("║  ПОВНИЙ ТЕСТ СИСТЕМИ  ║");
  Serial.println("▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀");
  
  Serial.println("1. Тест насоса...");
  setPumpPercent(50);
  delay(2000);
  setPumpPercent(0);
  Serial.println("   ✅  Насос: ОК");
  
  Serial.println("2. Тест вентилятора...");
  setFanPercent(50);
  delay(2000);
  setFanPercent(0);
  Serial.println("   ✅  Вентилятор: ОК");
  
  Serial.println("3. Тест витяжки...");
  setExtractorPercent(50);
  delay(2000);
  setExtractorPercent(0);
  Serial.println("   ✅  Витяжка: ОК");
  
  Serial.println("4. Тест вентиляції...");
  testVentilation();
  Serial.println("   ✅  Вентиляція: ОК");
  
  Serial.println("5. Тест зволожувача...");
  digitalWrite(HUMIDIFIER_PIN, HIGH);
  delay(1000);
  digitalWrite(HUMIDIFIER_PIN, LOW);
  Serial.println("   ✅  Зволожувач: ОК");
  
  Serial.println("6. Перевірка датчиків...");
  if (sensorData.carrierValid && sensorData.roomValid && sensorData.bmeValid) {
    Serial.printf("   ✅  Датчики: T=%.1f°C, H=%.1f%%, P=%.1fhPa\n",
                 sensorData.tempRoom, sensorData.humidity, sensorData.pressure);
  } else {
    Serial.println("   ⚠️  Проблема з датчиками!");
  }
  
  Serial.println("7. Перевірка Wi-Fi...");
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("   ✅  Wi-Fi: %s (%d dBm)\n", WiFi.SSID().c_str(), WiFi.RSSI());
  } else {
    Serial.println("   ⚠️  Wi-Fi не підключений!");
  }
  
  Serial.println("8. Перевірка часу...");
  String timeStr = getTimeString();
  if (timeStr != "Помилка часу") {
    Serial.println("   ✅  Час: " + timeStr);
  } else {
    Serial.println("   ⚠️  Проблема з часом!");
  }
  
  Serial.println("\n▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄");
  Serial.println("║  ТЕСТ СИСТЕМИ УСПІШНО ЗАВЕРШЕНО!  ║");
  Serial.println("▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀\n");
  
  setHeatingPower(0, 0, 0);
  digitalWrite(HUMIDIFIER_PIN, LOW);
}

void debugCommands() {
  Serial.println("\n▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄");
  Serial.println("║  КОМАНДИ НАЛАГОДЖЕННЯ  ║");
  Serial.println("▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀▀");
  Serial.println("  test     - Повний тест системи");
  Serial.println("  info     - Інформація про систему");
  Serial.println("  stop     - Екстрене відключення");
  Serial.println("  start    - Запуск автоматичного режиму");
  Serial.println("  reset    - Скидання налаштувань до заводських");
  Serial.println("▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄▄\n");
}

void resetToFactoryDefaults() {
  Serial.println("\n⚠️⚠️⚠️  СКИДАННЯ НАЛАШТУВАНЬ ДО ЗАВОДСЬКИХ  ⚠️⚠️⚠️");
  Serial.println("Всі налаштування будуть втрачені!");
  Serial.println("Для відновлення перезавантажте систему протягом 5 секунд...");
  
  for (int i = 5; i > 0; i--) {
    Serial.printf("Залишилось %d секунд...\n", i);
    delay(1000);
  }
  
  preferences.begin("climate", false);
  preferences.clear();
  preferences.end();
  
  Serial.println("✅  Налаштування скинуті до заводських");
  Serial.println("🔄  Перезавантаження системи...");
  delay(2000);
  ESP.restart();
}

void handleSerialInput() {
  if (Serial.available()) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input == "test") {
      runSystemTest();
    }
    else if (input == "info") {
      printSystemInfo();
    }
    else if (input == "stop") {
      emergencyStop();
    }
    else if (input == "start") {
      heatingState.manualMode = false;
      heatingState.emergencyMode = false;
      Serial.println("✅  Автоматичний режим активовано");
    }
    else if (input == "reset") {
      resetToFactoryDefaults();
    }
    else if (input == "debug") {
      debugCommands();
    }
    else {
      processAdvancedSerialCommand();
    }
  }
}