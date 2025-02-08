#include <GxEPD2_BW.h>
#include <WiFi.h>
#include <time.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>

/// Display Setup
GxEPD2_BW<GxEPD2_213_BN, GxEPD2_213_BN::HEIGHT> display(
    GxEPD2_213_BN(/*CS=*/ 5, /*DC=*/ 17, /*RST=*/ 16, /*BUSY=*/ 4)
);

/// Data Structures
struct Stock {
    String ticker;
    String name;
    float current_price;
    float open_price;
    float day_change;
    Stock(String t, String n, float c, float o, float d)
        : ticker(t), name(n), current_price(c), open_price(o), day_change(d) {}

    Stock(String t, String n)
        : ticker(t), name(n), current_price(0), open_price(0), day_change(0) {}
};

/// Const Memory
const char* ssid = "myssid";
const char* password = "******";
const String yahoofin = "https://query1.finance.yahoo.com/v8/finance/chart/" 

const Stock stocks[] = {
    Stock("VOO", "Vanguard S&P"),
    Stock("QQQ", "Invesco QQQ Trust"),
    Stock("PLTR", "Palantir"),
    Stock("LUNR", "Intuitive Machines"),
    Stock("KRKNF", "Kraken Robotics")
};
const int num_stocks = sizeof(stocks) / sizeof(stocks[0]);

const int max_attempts = 20;
const int stock_update_time_ms = 900000;        // 15 mins
const int hour_in_mins = 60;
const int market_open_in_mins = 6 * 60 + 30;    // 6:30am PST
const int market_close_in_mins = 13 * 60;       // 4pm PST

/// Function Prototypes
void updateDisplay(void);
void updateStockData(String symbol, int index);
void updateAllStocks(void);
bool isMarketOpen(void);
String getTimeStr(void);
String getDateStr(void);
void setup(void);

/// Global Mem
int currentStock = 0;

/// Stock API
void updateStockData(String symbol, int index) {
    if (WiFi.status() != WL_CONNECTED) return;

    HTTPClient http;
    String url =  + symbol;

    http.begin(url);
    int http_code = http.GET();

    if (http_code > 0) {
        String payload = http.getString();
        JsonDocument doc;
        deserializeJson(doc, payload);
        JsonObject result = doc["chart"]["result"][0];
        JsonObject meta = result["meta"];
        if (!meta.isNull()) {
            stocks[index].current_price = meta["regularMarketPrice"].as<float>();
            stocks[index].open_price = meta["previousClose"].as<float>();
            stocks[index].day_change = stocks[index].current_price - stocks[index].open_price;
        }
    }

    // Serial.print("symbol: ");
    // Serial.println(symbol);
    // Serial.print("price: ");
    // Serial.println(stocks[index].current_price);
    // Serial.print("change: ");
    // Serial.println(stocks[index].day_change);
    // Serial.print("open: ");
    // Serial.println(stocks[index].open_price);

    http.end();
    delay(1000);
}

void updateAllStocks() {
    for (int i = 0; i < num_stocks; i++)
        updateStockData(stocks[i].ticker, i);
}

/// Time Functions
String getTimeStr() {
    struct tm timeinfo;
    char time_str[6];

    if (!getLocalTime(&timeinfo)) return "Time Error";
    strftime(time_str, 6, "%H:%M", &timeinfo);
    return String(time_str);
}

String getDateStr() {
    struct tm timeinfo;
    char date_str[9];

    if (!getLocalTime(&timeinfo)) return "Date Error";
    strftime(date_str, 9, "%m/%d/%y", &timeinfo);
    return String(date_str);
}

bool isMarketOpen() {
    struct tm timeinfo;

    bool is_weekend = (timeinfo.tm_wday == 0 || timeinfo.tm_wday == 6); 
    if (!getLocalTime(&timeinfo) || is_weekend) return false;

    // Convert current time to minutes
    int current_minutes = timeinfo.tm_hour * hour_in_mins + timeinfo.tm_min;

    // Market hours: 6:30 AM to 1:00 PM PST
    return ((current_minutes >= market_open_in_mins) && (current_minutes <= market_close_in_mins));
}

/// Display Functions
void updateDisplay() {
    display.setFullWindow();
    display.firstPage();
    do {
        display.fillScreen(GxEPD_WHITE);
        display.setTextColor(GxEPD_BLACK);

        // Time (left)
        display.setFont(&FreeSans9pt7b);
        display.setCursor(10, 20);
        display.print(getTimeStr());

        // Market Status (right)
        display.setFont(&FreeSans9pt7b);
        display.setCursor(175, 20);
        if (isMarketOpen())
            display.print("OPEN");
        else
            display.print("CLOSED");

        // Ticker with up/down triangle
        display.setFont(&FreeSansBold12pt7b);
        display.setCursor(10, 60);
        display.print(stocks[currentStock].ticker);

        // Current Price
        display.setFont(&FreeSans9pt7b);
        display.setCursor(10, 85);
        display.print("$");
        display.print(stocks[currentStock].current_price, 2);
        display.print(" ");

        // Position for triangle
        int x = display.getCursorX();
        int y = display.getCursorY();

        if (stocks[currentStock].day_change > 0) {
            display.fillTriangle(
                x,      y,      // Bottom left point
                x+5,    y-10,   // Top point
                x+10,   y,      // Bottom right point
                GxEPD_BLACK
            );
        } else if (stocks[currentStock].day_change < 0) {
            display.fillTriangle(
                x,      y-10,   // Top left point
                x+5,    y,      // Bottom point
                x+10,   y-10,   // Top right point
                GxEPD_BLACK
            );
        }

        // Day Change (right side, aligned)
        display.setCursor(120, 60);
        display.print("Day:  ");
        char dayStr[10];
        sprintf(dayStr, "%s%.2f", (stocks[currentStock].day_change > 0) ? "+" : " ",stocks[currentStock].day_change);
        display.setCursor(178, 60);  // Fixed position for percentage
        display.print(dayStr);

        // Open Price (right side, aligned)
        display.setCursor(120, 80);
        display.print("Open:  ");
        char openStr[10];
        sprintf(openStr, "$%.2f", stocks[currentStock].open_price);
        display.setCursor(178, 80);  // Fixed position for percentage
        display.print(openStr);

        // Full Name (bottom left)
        display.setFont(&FreeSans9pt7b);
        display.setCursor(10, 120);
        display.print(stocks[currentStock].name);

        // Date (bottom right)
        display.setFont(&FreeSans9pt7b);
        display.setCursor(175, 120);
        display.print(getDateStr());

    } while (display.nextPage());
}

/// Arduino
void setup() {
    // Serial.begin(115200);
    // Serial.print("Setting Up Wifi....");
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) { delay(500); }
    // Serial.print("Setting Up Time....");
    configTime(-8 * 3600, 0, "pool.ntp.org");
    // Serial.print("Initializing Display....");
    display.init();
    display.setRotation(1);
    // Serial.print("Updating Stock Prices....\n");
    updateAllStocks();
    // Serial.print("Starting Stock Viewer....\n");
}

void loop() {
    static unsigned long lastUpdate = 0;
    unsigned long currentMillis = millis();

    // Update stock data every 15 minutes
    if (currentMillis - lastUpdate >= stock_update_time_ms) {
        updateAllStocks();
        lastUpdate = currentMillis;
    }
    currentStock = (currentStock + 1) % num_stocks;  // Cycle through stocks
    updateDisplay();
    delay(60000);  // Change stock every minute
}
