// ============================================================================
// WEB_PAGES_MISC.CPP - Допоміжні сторінки
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Модуль: Допомога, відладка
// ============================================================================

#include <WebServer.h>
#include <WiFi.h>
#include "web_common.h"
#include "global_declarations.h"

extern WebServer server;
extern SystemConfig config;
// Forward declarations
extern bool checkAuth();
extern String getUkraineMarquee();

// ============================================================================
// СТОРІНКА ДОПОМОГИ
// ============================================================================

void handleHelpPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    String html = getHtmlHead("📖 Довідка");
    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("📖 ДОВІДКА - КЛІМАТ-КОНТРОЛЬ");

    // Додаткові стилі
    html += "<style>";
    html += ".section { background: #f9f9f9; padding: 20px; border-radius: 8px; margin: 20px 0; border-left: 4px solid #9c27b0; }";
    html += ".tip { background: #e8f5e9; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #4caf50; }";
    html += ".warning { background: #fff3cd; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #ffc107; }";
    html += ".danger { background: #ffebee; padding: 15px; border-radius: 5px; margin: 15px 0; border-left: 4px solid #f44336; }";
    html += ".code { background: #263238; color: #aed581; padding: 10px; border-radius: 5px; font-family: monospace; margin: 10px 0; }";
    html += ".faq-item { margin: 20px 0; }";
    html += ".faq-q { font-weight: 600; color: #9c27b0; font-size: 1.1em; margin-bottom: 8px; }";
    html += ".faq-a { color: #555; line-height: 1.6; margin-left: 20px; }";
    html += "code { background: #f5f5f5; padding: 2px 6px; border-radius: 3px; font-family: monospace; color: #e91e63; }";
    html += "h2 { color: #9c27b0; margin-top: 30px; }";
    html += "ul, ol { line-height: 1.8; }";
    html += "</style>";

    // FAQ
    html += "<h2>❓ Найчастіші питання (FAQ)</h2>";

    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>📱 Не працює з телефону Android?</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Проблема:</strong> Android не підтримує mDNS (адреси типу <code>klimat.local</code>)<br>";
    html += "<strong>Рішення:</strong><br>";
    html += "1️⃣ Використовуйте IP-адресу: <code>" + WiFi.localIP().toString() + "</code><br>";
    html += "2️⃣ Відскануйте QR-код на головній сторінці<br>";
    html += "3️⃣ Збережіть IP в закладки";
    html += "</div></div>";

    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>💻 Не працює з іншої мережі WiFi?</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Проблема:</strong> Ваш пристрій і ESP32 в різних мережах<br>";
    html += "<strong>Рішення:</strong><br>";
    html += "1️⃣ Підключіть обидва до однієї WiFi мережі<br>";
    html += "2️⃣ Або налаштуйте доступ через інтернет (Port Forwarding)<br>";
    html += "<div class='tip'>💡 Поточна мережа ESP32: <strong>" + htmlEscape(WiFi.SSID()) + "</strong></div>";
    html += "</div></div>";

    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>🔐 Як захистити від сторонніх?</div>";
    html += "<div class='faq-a'>";
    html += "1️⃣ Перейдіть: Налаштування → WiFi<br>";
    html += "2️⃣ Увімкніть автентифікацію<br>";
    html += "3️⃣ Встановіть складний пароль<br>";
    html += "<div class='warning'>⚠️ Обов'язково для інтернет-доступу!</div>";
    html += "</div></div>";

    html += "<div class='faq-item'>";
    html += "<div class='faq-q'>🌐 IP-адреса постійно змінюється</div>";
    html += "<div class='faq-a'>";
    html += "<strong>Рішення:</strong> Налаштуйте статичну IP<br>";
    html += "1️⃣ Налаштування → WiFi<br>";
    html += "2️⃣ Встановіть галочку \"Використовувати статичну IP\"<br>";
    html += "3️⃣ Введіть вільну IP з вашої мережі (наприклад 192.168.1.100)<br>";
    html += "4️⃣ Шлюз = адреса роутера (зазвичай 192.168.1.1)";
    html += "</div></div>";

    // Доступ до системи
    html += "<h2>🔗 Доступ до системи</h2>";
    html += "<div class='section'>";
    html += "<h3>З ноутбука (Windows/Mac/Linux):</h3>";
    html += "<div class='code'>http://klimat.local</div>";
    html += "<p>✅ Працює автоматично</p>";

    html += "<h3>З телефону iOS (iPhone/iPad):</h3>";
    html += "<div class='code'>http://klimat.local</div>";
    html += "<p>✅ Працює автоматично</p>";

    html += "<h3>З телефону Android:</h3>";
    html += "<div class='code'>http://" + WiFi.localIP().toString() + "</div>";
    html += "<p>⚠️ Використовуйте IP-адресу</p>";

    html += "<div class='tip'>";
    html += "<strong>💡 Корисно:</strong><br>";
    html += "• Додайте сторінку в закладки<br>";
    html += "• На Android: Chrome → Меню → Додати на головний екран<br>";
    html += "• Відскануйте QR-код на головній сторінці";
    html += "</div>";
    html += "</div>";

    // Безпека
    html += "<h2>🔐 Безпека</h2>";
    html += "<div class='section'>";
    html += "<div class='danger'>";
    html += "<strong>⚠️ ВАЖЛИВО:</strong> Обов'язково увімкніть автентифікацію якщо налаштовуєте доступ через інтернет!";
    html += "</div>";
    html += "<p><strong>Як увімкнути:</strong></p>";
    html += "<ol>";
    html += "<li>Налаштування → WiFi</li>";
    html += "<li>☑️ Увімкнути автентифікацію</li>";
    html += "<li>Встановіть логін (за замовчуванням: admin)</li>";
    html += "<li>Встановіть СКЛАДНИЙ пароль (не 12345!)</li>";
    html += "</ol>";
    html += "<div class='tip'>";
    html += "<strong>💡 Вимоги до пароля:</strong><br>";
    html += "• Мінімум 8 символів<br>";
    html += "• Використовуйте букви, цифри, спецсимволи";
    html += "</div>";
    html += "</div>";

    // Доступ через інтернет
    html += "<h2>🌐 Доступ через інтернет</h2>";
    html += "<div class='section'>";
    html += "<ol>";
    html += "<li>✅ Увімкніть автентифікацію</li>";
    html += "<li>🔧 Налаштуйте Port Forwarding на роутері</li>";
    html += "<li>🌍 Дізнайтесь зовнішню IP на <a href='https://myip.com.ua' target='_blank'>myip.com.ua</a></li>";
    html += "<li>🚀 Підключайтесь: http://[ваша_IP]:8080</li>";
    html += "</ol>";
    html += "<p><strong>Port Forwarding:</strong></p>";
    html += "<div class='code'>";
    html += "Внутрішня IP: " + WiFi.localIP().toString() + "<br>";
    html += "Внутрішній порт: 80<br>";
    html += "Зовнішній порт: 8080";
    html += "</div>";
    html += "</div>";

    // Корисні поради
    html += "<h2>💡 Корисні поради</h2>";
    html += "<div class='section'>";
    html += "<ul>";
    html += "<li>📱 <strong>QR-код:</strong> На головній сторінці є QR-код для швидкого доступу</li>";
    html += "<li>📊 <strong>Історія:</strong> Система зберігає останні 24 години даних</li>";
    html += "<li>🤖 <strong>Авто-режим:</strong> Система сама підтримує температуру та вологість</li>";
    html += "<li>🔄 <strong>Оновлення:</strong> Дані оновлюються автоматично кожні 5 секунд</li>";
    html += "</ul>";
    html += "</div>";

    // Системна інформація
    html += "<h2>📞 Системна інформація</h2>";
    html += "<div class='section'>";
    html += "<ul>";
    html += "<li>Мережа: <strong>" + htmlEscape(WiFi.SSID()) + "</strong></li>";
    html += "<li>IP-адреса: <strong>" + WiFi.localIP().toString() + "</strong></li>";
    html += "<li>mDNS: <strong>klimat.local</strong></li>";
    html += "<li>Сигнал: <strong>" + String(WiFi.RSSI()) + " dBm</strong></li>";
    html += "<li>Версія: <strong>" VERSION "</strong></li>";
    html += "<li>Дата збірки: <strong>" BUILD_DATE " " BUILD_TIME "</strong></li>";
    html += "</ul>";
    html += "</div>";

    html += "</div>"; // container

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

// ============================================================================
// СТОРІНКА ВІДЛАДКИ
// ============================================================================

void handleDebugPage() {
    if (!checkAuth()) return;
    if (WiFi.status() != WL_CONNECTED) return;

    String html = getHtmlHead("🔧 Відладка");
    html += "<div class='container'>";

    // Навігація зверху
    html += getNavHeader("🔧 ВІДЛАДКОВА ІНФОРМАЦІЯ");

    // Системна інформація
    html += "<div class='card'>";
    html += "<h3 style='margin-top: 0;'>💾 Пам'ять</h3>";
    html += "<div style='display: grid; gap: 10px;'>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Вільна пам'ять:</strong>";
    html += "<span style='color: #4CAF50; font-weight: 600;'>" + String(ESP.getFreeHeap() / 1024) + " KB</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Всього пам'яті:</strong>";
    html += "<span>" + String(ESP.getHeapSize() / 1024) + " KB</span>";
    html += "</div>";

    float heapUsed = 100.0 - (ESP.getFreeHeap() * 100.0 / ESP.getHeapSize());
    String heapColor = heapUsed > 80 ? "#f44336" : (heapUsed > 60 ? "#ff9800" : "#4CAF50");
    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Використано:</strong>";
    html += "<span style='color: " + heapColor + "; font-weight: 600;'>" + String(heapUsed, 1) + "%</span>";
    html += "</div>";

    html += "</div>";
    html += "</div>";

    // CPU та система
    html += "<div class='card'>";
    html += "<h3 style='margin-top: 0;'>🖥️ Система</h3>";
    html += "<div style='display: grid; gap: 10px;'>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Задач FreeRTOS:</strong>";
    html += "<span>" + String(uxTaskGetNumberOfTasks()) + "</span>";
    html += "</div>";

    unsigned long uptimeSec = millis() / 1000;
    unsigned long days = uptimeSec / 86400;
    unsigned long hours = (uptimeSec % 86400) / 3600;
    unsigned long minutes = (uptimeSec % 3600) / 60;
    String uptimeStr = "";
    if (days > 0) uptimeStr += String(days) + "д ";
    uptimeStr += String(hours) + "г " + String(minutes) + "хв";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Час роботи:</strong>";
    html += "<span style='color: #2196F3; font-weight: 600;'>" + uptimeStr + "</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Температура чіпа:</strong>";
    html += "<span>" + String(temperatureRead(), 1) + "°C</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Частота CPU:</strong>";
    html += "<span>" + String(getCpuFrequencyMhz()) + " MHz</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Версія SDK:</strong>";
    html += "<span>" + String(ESP.getSdkVersion()) + "</span>";
    html += "</div>";

    html += "</div>";
    html += "</div>";

    // WiFi інформація
    html += "<div class='card'>";
    html += "<h3 style='margin-top: 0;'>📶 WiFi</h3>";
    html += "<div style='display: grid; gap: 10px;'>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>SSID:</strong>";
    html += "<span>" + htmlEscape(WiFi.SSID()) + "</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>IP:</strong>";
    html += "<span>" + WiFi.localIP().toString() + "</span>";
    html += "</div>";

    int rssi = WiFi.RSSI();
    String rssiColor = rssi > -50 ? "#4CAF50" : (rssi > -70 ? "#ff9800" : "#f44336");
    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>Сигнал:</strong>";
    html += "<span style='color: " + rssiColor + "; font-weight: 600;'>" + String(rssi) + " dBm</span>";
    html += "</div>";

    html += "<div style='display: flex; justify-content: space-between; padding: 10px; background: #f5f5f5; border-radius: 5px;'>";
    html += "<strong>MAC:</strong>";
    html += "<span style='font-family: monospace;'>" + WiFi.macAddress() + "</span>";
    html += "</div>";

    html += "</div>";
    html += "</div>";

    html += "</div>"; // container

    html += getNavFooter();
    html += getUkraineMarquee();
    html += getHtmlFooter();

    server.send(200, "text/html", html);
}

