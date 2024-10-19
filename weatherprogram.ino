/*REFERENCIA LINKEK
https://randomnerdtutorials.com/esp8266-weather-forecaster/
https://github.com/olikraus/u8g2/wiki/u8g2reference
https://github.com/olikraus/u8g2/wiki
https://openweathermap.org/api
https://www.youtube.com/watch?v=GkbOr_RIUCg
https://randomnerdtutorials.com/esp8266-0-96-inch-oled-display-with-arduino-ide/
https://www.unitconverters.net/time/milliseconds-to-seconds.htm
https://arduinojson.org/v7/api/json/deserializejson/

font méretek:
u8g2_font_4x6_tf: 4x6 pixeles, 5 pixel magas
u8g2_font_5x7_tf: 5x7 pixeles, 6 pixel magas
u8g2_font_5x8_tf: 5x8 pixeles, 7 pixel magas
u8g2_font_6x10_tf: 6x10 pixeles, 9 pixel magas
*/
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const char* ssid = "Galaxy A54 5G EFC2";
const char* password = "2km8x9jv";

const char* host = "api.openweathermap.org";
const char* apiKey = "1a2a93b7f2e16793846429c345cab50c";
const char* cityId = "3054643";  // Budapest város ID-je

struct WeatherData {
  float temperature;
  int humidity;
  String description;
};

struct ForecastData {
  WeatherData hourly[40];  // 5 nap, 3 óránként az 40 adat, mert 24/3= 8 adat/nap és 8 adat *5 nap ==> 40 adat
  WeatherData daily[5];    // 5 nap napi előrejelzés
};

WeatherData currentWeather; // Jelenlegi időjárás
ForecastData forecast;  // Előrejelzés

unsigned long lastUpdateTime = 0; // utolsó adatfrissítés időpontja
const unsigned long updateInterval = 300000;  // 5 perc
unsigned long lastScreenChangeTime = 0; // utolsó képernyőváltás időpontja
const unsigned long screenChangeInterval = 5000;  // 5 másodperc
int currentScreen = 0; // aktuálisan megjelenített képernyő száma --> 0 az első képernyő
const int totalScreens = 7;  // 1 aktuális + 5 napi + 1 összesítő

// Egyszer fut le, amikor az eszköz be vagy kikapcsol
void setup() {
  Serial.begin(115200);  // inicializálja a soros kommunikációt 115200 baud sebességgel
  delay(10); // bizonyos hardverelemeknek legyen idejük inicializálódni

  Wire.begin(14, 12);  // SDA = 14, SCL = 12 || inicializálja az I2C (Inter-Integrated Circuit) kommunikációs protokollt
  Wire.setClock(400000);  // I2C kommunikáció órajelének sebességét 400 kHz-re

  u8g2.begin(); // inicializálja az U8g2 könyvtárat és a hozzá kapcsolódó kijelzőt
  u8g2.enableUTF8Print(); // Engedélyezi az UTF-8 karakterkódolás használatát
  u8g2.setFont(u8g2_font_unifont_t_latin); // latin nyelv beállítása

  u8g2.clearBuffer(); // Törli a kijelző pufferét, viszont ez nem törli azonnal a kijelzőt, csak a memóriában lévő képet tisztítja meg
  u8g2.setCursor(0, 15); // kurzor pozíciója a képernyőn
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.print("Varakozas a WiFire..."); // szöveg bekerül a pufferbe, még nem íródik ki
  u8g2.sendBuffer(); // Elküldi a pufferben lévő tartalmat a kijelzőre

  Serial.println("\nCsatlakozas a WiFi halozathoz..."); // Serial Monitorra kiíratás

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi csatlakoztatva");  // Serial Monitor kiíratás
  Serial.println("IP cim: " + WiFi.localIP().toString()); // Serial Monitor kiíratás

  u8g2.clearBuffer(); // // Törli a kijelző pufferét
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.setCursor(0, 15); // kurzor pozíciója a képernyőn
  u8g2.print("WiFi csatlakoztatva"); // szöveg bekerül a pufferbe
  u8g2.setCursor(0, 30); // kurzor pozíciója a képernyőn
  u8g2.print(WiFi.localIP().toString().c_str()); // Kiírja az IP címet a kijelzőre. 
  //A .c_str() metódus C-stílusú karakterlánccá alakítja az IP címet, amit az u8g2 könyvtár tud kezelni
  u8g2.sendBuffer(); // Elküldi a pufferben lévő tartalmat a kijelzőre

  updateWeatherData(); // Elindítja az időjárási adatok frissítését, ha WiFi kapcsolat létrejött
}

void loop() { // folyamatosan ismétlődik, amíg az eszköz be van kapcsolva
  unsigned long currentTime = millis(); // lekéri az aktuális időt milliszekundumokban az eszköz indulása óta

  if (currentTime - lastUpdateTime >= updateInterval) {  // ellenőrzi, hogy eltelt-e már az 5 perc az utolsó adatfrissítés óta
    updateWeatherData();
    lastUpdateTime = currentTime;
  }

  if (currentTime - lastScreenChangeTime >= screenChangeInterval) { // ellenőrzi, hogy eltelt-e a screenChangeInterval idő , azaz 5 másodperc az utolsó képernyőváltás óta
  /*
   currentScreen = 0, akkor: (0 + 1) % 7 = 1
   currentScreen = 1, akkor: (1 + 1) % 7 = 2
   currentScreen = 2, akkor: (2 + 1) % 7 = 3
   currentScreen = 3, akkor: (3 + 1) % 7 = 4
   currentScreen = 4, akkor: (4 + 1) % 7 = 5
   currentScreen = 5, akkor: (5 + 1) % 7 = 6
   currentScreen = 6, akkor: (6 + 1) % 7 = 0 (Itt visszaugrik nullára)
  */
    currentScreen = (currentScreen + 1) % totalScreens; 
    updateDisplay();
    lastScreenChangeTime = currentTime;
  }
}

void updateWeatherData() {
  if (WiFi.status() == WL_CONNECTED) {
    String weatherUrl = "http://" + String(host) + "/data/2.5/weather?id=" + String(cityId) + "&appid=" + String(apiKey) + "&units=metric&lang=hu";
    String weatherData = httpGETRequest(weatherUrl); // Végrehajtja a HTTP GET kérést a megadott URL-en
    if (weatherData != "") { // Ha sikerült adatot lekérni (nem üres string), akkor feldolgozza
      parseCurrentWeather(weatherData); // elemezze (parse-olja) a nyers időjárási adatokat, amelyeket az API-tól kapunk
      printCurrentWeather();  // Serial Monitorra írás
    }

    String forecastUrl = "http://" + String(host) + "/data/2.5/forecast?id=" + String(cityId) + "&appid=" + String(apiKey) + "&units=metric&lang=hu";
    String forecastData = httpGETRequest(forecastUrl); // Végrehajtja a HTTP GET kérést a megadott URL-en
    if (forecastData != "") { // Ha sikerült adatot lekérni (nem üres string), akkor feldolgozza
      parseForecast(forecastData); // elemezze (parse-olja) a nyers időjárási adatokat, amelyeket az API-tól kapunk
      printForecast();  // Serial Monitorra írás
    }
  }
}

String httpGETRequest(String url) { // visszatérési érték lesz a lekért adat vagy üres string hiba esetén
  WiFiClient client; // Létrehoz egy WiFiClient objektumot, amely a WiFi kapcsolatot kezeli
  HTTPClient http; // Létrehoz egy HTTPClient objektumot, amely a HTTP kéréseket kezeli
  
  if (http.begin(client, url)) { // Elindítja a HTTP kapcsolatot a megadott URL-lel és ellenőrzi, hogy sikerült-e inicializálni a kapcsolatot
    int httpResponseCode = http.GET(); // Végrehajtja a GET kérést és eltárolja a válasz kódját
    if (httpResponseCode > 0) { // Ellenőrzi, hogy kaptunk-e valamilyen válaszkódot a szervertől
      if (httpResponseCode == HTTP_CODE_OK) { // Ellenőrzi, hogy a válaszkód 200 (OK) volt-e
        return http.getString(); // Ha a válasz OK, visszaadja a szerver által küldött adatokat string formátumban
      }
    } else {
      Serial.printf("HTTP keres sikertelen, hibakod: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();  // Lezárja a HTTP kapcsolatot
  }
  return ""; // Bármilyen hiba történt, üres stringet ad vissza
}

void parseCurrentWeather(String json) {
  DynamicJsonDocument doc(1024); // Létrehoz egy dinamikus JSON dokumentumot 1024 bájt kapacitással
  deserializeJson(doc, json); // elemzi (parse-olja) a JSON stringet és feltölti a doc objektumot az adatokkal

  // kinyerik a releváns adatokat a JSON dokumentumból és eltárolják őket a currentWeather struktúrában
  currentWeather.temperature = doc["main"]["temp"];
  currentWeather.humidity = doc["main"]["humidity"];
  currentWeather.description = doc["weather"][0]["description"].as<String>(); //  explicit módon stringgé konvertálja az értéket
}

void parseForecast(String json) {
  DynamicJsonDocument doc(6144);  // Létrehoz egy dinamikus JSON dokumentumot 6144 bájt kapacitással
  deserializeJson(doc, json); // elemzi (parse-olja) a JSON stringet és feltölti a doc objektumot az adatokkal

  /*
  Az OpenWeatherMap API előrejelzési válasza általában egy "list" nevű tömböt tartalmaz,
  ahol minden elem egy adott időpontra vonatkozó előrejelzést reprezentál
  */
  JsonArray list = doc["list"]; // Kiválasztja a list tömböt a JSON dokumentumból
  //Óránkénti előrejelzések feldolgozása:
  for (int i = 0; i < 40 && i < list.size(); i++) {
    forecast.hourly[i].temperature = list[i]["main"]["temp"];
    forecast.hourly[i].humidity = list[i]["main"]["humidity"];
    forecast.hourly[i].description = list[i]["weather"][0]["description"].as<String>();
  }

  /*
  Ez a ciklus 5 napi előrejelzést generál.
  Minden napi előrejelzést az óránkénti előrejelzések közül választ ki, 8 óránként (i * 8).
  Ez feltételezi, hogy az óránkénti előrejelzések 3 óránként vannak (24 óra / 3 = 8 adat naponta).
  */
  for (int i = 0; i < 5; i++) {
    forecast.daily[i] = forecast.hourly[i * 8];
  }
}

void updateDisplay() {
  u8g2.clearBuffer();

  switch (currentScreen) {
    case 0:
      displayCurrentWeather();
      break;
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
      displayDailyForecast(currentScreen - 1); // index érték 0-4 között van
      break;
    case 6:
      displayWeeklyForecast();
      break;
  }

  u8g2.sendBuffer();
}

void displayCurrentWeather() {
  u8g2.setCursor(0, 15);
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.print("Homerseklet: ");
  u8g2.print(currentWeather.temperature);
  u8g2.print(" C");
  u8g2.setCursor(0, 30);
  u8g2.print("Paratartalom: ");
  u8g2.print(currentWeather.humidity);
  u8g2.print("%");
  u8g2.setCursor(0, 45);
  u8g2.print(currentWeather.description.c_str());
}

void displayDailyForecast(int day) {
  u8g2.setCursor(0, 15);
  u8g2.setFont(u8g2_font_5x8_tf);
  u8g2.print(day + 1);
  u8g2.print(". nap elorejelzes");
  u8g2.setCursor(0, 30);
  u8g2.print("Hom: ");
  u8g2.print(forecast.daily[day].temperature);
  u8g2.print(" C");
  u8g2.setCursor(0, 45);
  u8g2.print(forecast.daily[day].description.c_str());
  u8g2.setCursor(0, 60);
  u8g2.print("Paratartalom: ");
  u8g2.print(forecast.daily[day].humidity);

}

void displayWeeklyForecast() {
  for (int i = 0; i < 5; i++) {
    u8g2.setCursor(0, 13 * (i + 1));  // minden sor 13 pixellel lejjebb kerül az előzőhöz képest
    u8g2.setFont(u8g2_font_5x8_tf);
    u8g2.print(i + 1); // Kiírja a nap számát (1-től 5-ig)
    u8g2.print(". ");
    u8g2.print(forecast.daily[i].temperature);
    u8g2.print("C ");
    u8g2.print(forecast.daily[i].description.c_str());
  }
}


void printCurrentWeather() {
  Serial.println("\n--- Aktualis idojaras ---");
  Serial.print("Homerseklet: ");
  Serial.print(currentWeather.temperature);
  Serial.println(" C");
  Serial.print("Paratartalom: ");
  Serial.print(currentWeather.humidity);
  Serial.println("%");
  Serial.print("Leiras: ");
  Serial.println(currentWeather.description);
}

void printForecast() {
  Serial.println("\n--- 5 napos elorejelzes ---");
  for (int i = 0; i < 5; i++) {
    Serial.print(i + 1);
    Serial.println(". nap:");
    Serial.print("  Homerseklet: ");
    Serial.print(forecast.daily[i].temperature);
    Serial.println(" C");
    Serial.print("  Paratartalom: ");
    Serial.print(forecast.daily[i].humidity);
    Serial.println("%");
    Serial.print("  Leiras: ");
    Serial.println(forecast.daily[i].description);
  }

  Serial.println("\n--- 3 oras bontasu elorejelzes ---");
  for (int i = 0; i < 40; i++) {
    Serial.print(i * 3);
    Serial.println(". ora:");
    Serial.print("  Homerseklet: ");
    Serial.print(forecast.hourly[i].temperature);
    Serial.println(" C");
    Serial.print("  Paratartalom: ");
    Serial.print(forecast.hourly[i].humidity);
    Serial.println("%");
    Serial.print("  Leiras: ");
    Serial.println(forecast.hourly[i].description);
  }
}