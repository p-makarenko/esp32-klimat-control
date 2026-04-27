// ============================================================================
// WEB_COMMON.H - Спільні стилі та шаблони для веб-інтерфейсу
// ============================================================================
// Рефакторинг за методом "Скептичного Архітектора"
// Критика Крок 2: Дублювання CSS - вирішено централізацією стилів
// ============================================================================

#ifndef WEB_COMMON_H
#define WEB_COMMON_H

#include <Arduino.h>

// ============================================================================
// СПІЛЬНІ CSS СТИЛІ
// ============================================================================

// Базові стилі для всіх сторінок
inline String getCommonCSS() {
    String css = "";
    css += "@import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;500;600;700&display=swap');";
    css += "* { box-sizing: border-box; }";
    css += "body { font-family: 'Inter', -apple-system, BlinkMacSystemFont, 'Segoe UI', sans-serif; margin: 20px; background: #f0f0f0; }";
    css += ".container { max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 10px; box-shadow: 0 0 10px rgba(0,0,0,0.1); }";
    css += ".header { text-align: center; margin-bottom: 30px; }";

    // Кнопки
    css += ".btn { background: #2196F3; color: white; padding: 10px 15px; border: none; border-radius: 5px; cursor: pointer; margin: 5px; transition: background 0.2s; font-weight: 500; font-size: 14px; }";
    css += ".btn:hover { background: #1976D2; }";
    css += ".btn:active { transform: scale(0.98); }";
    css += ".btn-danger { background: #f44336; }";
    css += ".btn-danger:hover { background: #d32f2f; }";
    css += ".btn-success { background: #4CAF50; }";
    css += ".btn-success:hover { background: #45a049; }";

    // Навігація
    css += ".nav { display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 10px; margin: 30px 0; }";
    css += ".nav-btn { background: #4CAF50; color: white; padding: 15px; text-align: center; text-decoration: none; border-radius: 5px; display: block; transition: background 0.3s; font-weight: 500; }";
    css += ".nav-btn:hover { background: #45a049; transform: translateY(-2px); }";

    // Картки
    css += ".card { background: #f9f9f9; padding: 20px; border-radius: 8px; border-left: 4px solid #4CAF50; margin-bottom: 15px; }";
    css += ".status-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(280px, 1fr)); gap: 20px; margin: 30px 0; }";

    // Форми
    css += "input, select, textarea { padding: 10px; border: 1px solid #ccc; border-radius: 4px; font-size: 14px; }";
    css += "input:focus, select:focus, textarea:focus { outline: none; border-color: #2196F3; box-shadow: 0 0 0 2px rgba(33,150,243,0.2); }";

    // Quick buttons (командна строка)
    css += ".command-section { background: #f5f5f5; padding: 20px; border-radius: 8px; margin: 20px 0; }";
    css += ".quick-buttons { display: flex; flex-wrap: wrap; gap: 8px; margin-top: 10px; }";
    css += ".quick-btn { background: #e0e0e0; padding: 8px 12px; border-radius: 4px; cursor: pointer; border: none; transition: all 0.2s; font-size: 13px; }";
    css += ".quick-btn:hover { background: #bdbdbd; transform: translateY(-1px); }";
    css += ".quick-btn:active { transform: scale(0.98); }";

    // Command output
    css += "#commandOutput { background: white; padding: 10px; border-radius: 5px; font-family: 'SF Mono', 'Monaco', 'Consolas', monospace; min-height: 50px; white-space: pre-wrap; overflow-y: auto; max-height: 200px; border: 1px solid #ddd; }";

    // Power indicators
    css += ".power-indicators { display: flex; justify-content: space-between; margin: 20px 0; background: #e8f5e9; padding: 15px; border-radius: 8px; }";
    css += ".power-item { text-align: center; flex: 1; padding: 10px; }";
    css += ".power-label { font-size: 0.9em; color: #666; margin-bottom: 5px; }";
    css += ".power-value { font-size: 1.8em; font-weight: 600; color: #2c3e50; }";

    // Status values
    css += ".status-value { font-size: 1.2em; font-weight: 600; }";
    css += ".temp-status { color: #e74c3c; }";
    css += ".hum-status { color: #3498db; }";
    css += ".sys-status { color: #2c3e50; }";

    // Tooltips
    css += ".tooltip { position: relative; display: inline-block; cursor: help; }";
    css += ".tooltip .tooltiptext { visibility: hidden; width: 250px; background-color: #555; color: #fff; text-align: left; border-radius: 6px; padding: 10px; position: absolute; z-index: 1; bottom: 125%; left: 50%; margin-left: -125px; opacity: 0; transition: opacity 0.3s; font-size: 0.85em; line-height: 1.4; }";
    css += ".tooltip .tooltiptext::after { content: ''; position: absolute; top: 100%; left: 50%; margin-left: -5px; border-width: 5px; border-style: solid; border-color: #555 transparent transparent transparent; }";
    css += ".tooltip:hover .tooltiptext { visibility: visible; opacity: 1; }";

    // Charts
    css += ".chart-container { position: relative; height: 300px; margin: 30px 0; }";

    return css;
}

// ============================================================================
// МОБІЛЬНІ СТИЛІ
// Критика Крок 2: Мобільні стилі відсутні - вирішено
// ============================================================================

inline String getMobileCSS() {
    String css = "";

    // Tablet (768px)
    css += "@media (max-width: 768px) {";
    css += "  body { margin: 10px; }";
    css += "  .container { padding: 15px; }";
    css += "  h1 { font-size: 1.5em; }";
    css += "  .nav { grid-template-columns: repeat(2, 1fr); gap: 8px; }";
    css += "  .nav-btn { padding: 12px 10px; font-size: 13px; }";
    css += "  .status-grid { grid-template-columns: 1fr 1fr; gap: 10px; }";
    css += "  .card { padding: 15px; }";
    css += "  .power-indicators { flex-wrap: wrap; padding: 10px; }";
    css += "  .power-item { flex: 1 1 33%; min-width: 80px; padding: 5px; }";
    css += "  .power-value { font-size: 1.4em; }";
    css += "  .command-section { padding: 15px; }";
    css += "  .quick-buttons { gap: 5px; }";
    css += "  .quick-btn { padding: 8px 10px; font-size: 12px; flex: 1 1 calc(50% - 5px); min-width: 0; text-align: center; }";
    css += "  input, select { font-size: 16px; padding: 12px; }"; // Запобігає zoom на iOS
    css += "  .btn { padding: 12px 16px; font-size: 14px; }";
    css += "  .chart-container { height: 250px; margin: 15px 0; }";
    css += "  #commandOutput { max-height: 150px; font-size: 12px; }";
    css += "}";

    // Mobile (480px)
    css += "@media (max-width: 480px) {";
    css += "  body { margin: 5px; }";
    css += "  .container { padding: 10px; border-radius: 5px; }";
    css += "  h1 { font-size: 1.3em; }";
    css += "  .nav { grid-template-columns: 1fr 1fr; gap: 5px; }";
    css += "  .nav-btn { padding: 10px 8px; font-size: 12px; }";
    css += "  .status-grid { grid-template-columns: 1fr; }";
    css += "  .quick-btn { font-size: 11px; padding: 6px 8px; flex: 1 1 calc(33.33% - 4px); }";
    css += "  .power-indicators { flex-direction: column; }";
    css += "  .power-item { flex: 1 1 100%; border-bottom: 1px solid #c8e6c9; padding: 8px; }";
    css += "  .power-item:last-child { border-bottom: none; }";
    css += "  .chart-container { height: 200px; }";
    css += "  .btn { width: 100%; margin: 5px 0; }";
    css += "}";

    return css;
}

// ============================================================================
// НАВІГАЦІЙНІ ЕЛЕМЕНТИ
// Критика Крок 2: Навігація непослідовна - вирішено
// ============================================================================

// Заголовок сторінки з кнопкою "Назад" зверху
inline String getNavHeader(const String& title, const String& backUrl = "/") {
    String html = "";
    html += "<div style='display: flex; align-items: center; gap: 15px; margin-bottom: 20px; flex-wrap: wrap;'>";
    html += "<a href='" + backUrl + "' style='display: inline-flex; align-items: center; gap: 5px; padding: 10px 15px; background: #607D8B; color: white; text-decoration: none; border-radius: 5px; font-weight: 500; transition: background 0.2s;' onmouseover=\"this.style.background='#455A64'\" onmouseout=\"this.style.background='#607D8B'\">";
    html += "<span style='font-size: 1.2em;'>&larr;</span> На головну";
    html += "</a>";
    html += "<h1 style='margin: 0; flex: 1;'>" + title + "</h1>";
    html += "</div>";
    return html;
}

// Footer з кнопкою навігації внизу
inline String getNavFooter(const String& backUrl = "/", const String& backText = "На головну") {
    String html = "";
    html += "<div style='margin-top: 30px; padding-top: 20px; border-top: 1px solid #eee; text-align: center;'>";
    html += "<a href='" + backUrl + "' class='btn btn-success' style='display: inline-block; padding: 12px 30px; font-size: 16px;'>";
    html += "&larr; " + backText;
    html += "</a>";
    html += "</div>";
    return html;
}

// Повна навігаційна панель (для головної сторінки)
inline String getMainNavigation() {
    String html = "";
    html += "<div class='nav'>";
    html += "<a href='/control' class='nav-btn'>🎛️ ПАНЕЛЬ КЕРУВАННЯ</a>";
    html += "<a href='/settings' class='nav-btn'>⚙️ ПАНЕЛЬ НАЛАШТУВАНЬ</a>";
    html += "<a href='/energy' class='nav-btn' style='background: #f59e0b;'>⚡ ЕНЕРГОКОНТРОЛЕР</a>";
    html += "<a href='/ota' class='nav-btn' style='background: #00bcd4;'>🔄 OTA ОНОВЛЕННЯ</a>";
    html += "<a href='/help' class='nav-btn' style='background: #9c27b0;'>📖 ДОВІДКА</a>";
    html += "<a href='/history' class='nav-btn' style='background: #e91e63;'>📈 ГРАФІКИ</a>";
    html += "<a href='/debug' class='nav-btn'>🔧 ВІДЛАДКА</a>";
    html += "<a href='/light' class='nav-btn' style='background:#f59e0b;'>💡 ОСВІТЛЕННЯ</a>";
    html += "</div>";
    return html;
}

// ============================================================================
// HTML ШАБЛОНИ
// ============================================================================

// Початок HTML документа
inline String getHtmlHead(const String& title, bool includeChartJS = false) {
    String html = "<!DOCTYPE html><html lang='uk'><head>";
    html += "<meta charset='UTF-8'>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1.0, maximum-scale=5.0, user-scalable=yes'>";
    html += "<title>" + title + "</title>";
    html += "<style>";
    html += getCommonCSS();
    html += getMobileCSS();
    html += "</style>";

    if (includeChartJS) {
        html += "<script src='https://cdn.jsdelivr.net/npm/chart.js@4.4.0'></script>";
        // Hammer.js потрібен для pinch/pan на touch-екранах
        html += "<script src='https://cdn.jsdelivr.net/npm/hammerjs@2.0.8'></script>";
        html += "<script src='https://cdn.jsdelivr.net/npm/chartjs-plugin-zoom@2.0.1'></script>";
    }

    html += "</head><body>";
    return html;
}

// Кінець HTML документа
inline String getHtmlFooter() {
    return "</body></html>";
}

// ============================================================================
// CHART.JS КОНФІГУРАЦІЯ ДЛЯ TOUCH
// Критика Крок 2: Touch events для графіків - вирішено
// ============================================================================

inline String getChartZoomConfig() {
    String config = "";
    config += "plugins: {";
    config += "  zoom: {";
    config += "    pan: {";
    config += "      enabled: true,";
    config += "      mode: 'x',";
    config += "      threshold: 5,";  // Менший поріг для touch
    config += "      modifierKey: null";  // Працює без Ctrl на touch
    config += "    },";
    config += "    zoom: {";
    config += "      wheel: { enabled: true, speed: 0.1 },";
    config += "      pinch: { enabled: true },";  // Pinch zoom для мобільних
    config += "      drag: {";
    config += "        enabled: true,";
    config += "        backgroundColor: 'rgba(66, 133, 244, 0.2)',";
    config += "        borderColor: 'rgba(66, 133, 244, 0.8)',";
    config += "        borderWidth: 1,";
    config += "        threshold: 10";
    config += "      },";
    config += "      mode: 'x'";
    config += "    }";
    config += "  }";
    config += "}";
    return config;
}

// ============================================================================
// ДОПОМІЖНІ ФУНКЦІЇ
// ============================================================================

// HTML escape (захист від XSS)
inline String htmlEscape(const String& str) {
    String escaped = "";
    escaped.reserve(str.length() * 1.2);

    for (size_t i = 0; i < str.length(); i++) {
        char c = str.charAt(i);
        switch (c) {
            case '<':  escaped += "&lt;";   break;
            case '>':  escaped += "&gt;";   break;
            case '&':  escaped += "&amp;";  break;
            case '"':  escaped += "&quot;"; break;
            case '\'': escaped += "&#x27;"; break;
            default:   escaped += c;        break;
        }
    }
    return escaped;
}

#endif // WEB_COMMON_H
