#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <DHT.h>
#include <ThingSpeak.h>
#include <elapsedMillis.h>
#include <LiquidCrystal_I2C.h>

#define DHTPIN 4
#define DHTTYPE DHT21
#define HYGRO 34

#define RELAY_1 26   // FAN
#define RELAY_2 25   // POMPA


#define WIFI_SSID "UGMURO-INE"
#define WIFI_PASSWORD "Gepuk15000"

// ======================
// ThingSpeak
// ======================
unsigned long channelNum = 3399921;
const char* WriteAPIKey = "1VI22AD11A0Y925L";

// ======================
// Interval
// ======================
unsigned long thingSpeakInterval = 15000;
unsigned long sensorInterval     = 5000;
unsigned long displayInterval    = 2000;

elapsedMillis thingSpeakMillis;
elapsedMillis sensorMillis;
elapsedMillis displayMillis;

// ======================
// Threshold Otomatis
// ======================
const float TEMP_THRESHOLD = 36.0;
const float HUM_THRESHOLD  = 75.0;

const int SOIL_ON  = 30;
const int SOIL_OFF = 40;

// ======================
// Variabel
// ======================
float h = 0;
float c = 0;

int soil = 0;
int soilPercentage = 0;

bool fanStatus = false;
bool pumpStatus = false;

// mode manual dari web
bool manualFan  = false;
bool manualPump = false;

DHT dht(DHTPIN, DHTTYPE);

WiFiClient client;
WebServer server(80);

LiquidCrystal_I2C lcd(0x27, 20, 4);

// ======================
// WEB FILE
// ======================

void handleRoot()
{
  File file = LittleFS.open("/index.html", "r");

  if(!file)
  {
    server.send(404,"text/plain","index.html not found");
    return;
  }

  server.streamFile(file,"text/html");
  file.close();
}

void handleCSS()
{
  File file = LittleFS.open("/style.css","r");

  if(!file)
  {
    server.send(404,"text/plain","style.css not found");
    return;
  }

  server.streamFile(file,"text/css");
  file.close();
}

void handleJS()
{
  File file = LittleFS.open("/script.js","r");

  if(!file)
  {
    server.send(404,"text/plain","script.js not found");
    return;
  }

  server.streamFile(file,"application/javascript");
  file.close();
}

// ======================
// JSON DATA
// ======================

void handleData()
{
  String json = "{";

  json += "\"temperature\":" + String(c,1) + ",";
  json += "\"humidity\":" + String(h,1) + ",";
  json += "\"soil\":" + String(soilPercentage) + ",";
  json += "\"fan\":" + String(fanStatus ? 1 : 0) + ",";
  json += "\"pump\":" + String(pumpStatus ? 1 : 0);

  json += "}";

  server.send(200,"application/json",json);
}

// ======================
// RELAY CONTROL
// ======================

void handleRelay()
{
  if(!server.hasArg("device") ||
     !server.hasArg("state"))
  {
    server.send(400,"text/plain","Bad Request");
    return;
  }
  String device = server.arg("device");
  int state = server.arg("state").toInt();

  if(device == "fan")
  {
    manualFan = true;
    fanStatus = state;

    digitalWrite(RELAY_1,
                 fanStatus ? HIGH : LOW);
  }

  if(device == "pump")
  {
    manualPump = true;
    pumpStatus = state;

    digitalWrite(RELAY_2,
                 pumpStatus ? HIGH : LOW);
  }

  server.send(200,"text/plain","OK");
}
void handleMode()
{
  if(!server.hasArg("device") ||
     !server.hasArg("mode"))
  {
    server.send(400,"text/plain","Bad Request");
    return;
  }

  String device = server.arg("device");
  String mode   = server.arg("mode");

  if(device == "fan")
  {
    manualFan = (mode == "manual");
  }

  if(device == "pump")
  {
    manualPump = (mode == "manual");
  }

  server.send(200,"text/plain","OK");
}

// ======================
// WIFI
// ======================

void connectWiFi()
{
  Serial.print("Connecting WiFi");

  WiFi.begin(WIFI_SSID,
             WIFI_PASSWORD);

  while(WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi Connected");

  Serial.print("IP Address : ");
  Serial.println(WiFi.localIP());
}

// ======================
// SETUP
// ======================

void setup()
{
  Serial.begin(115200);

  connectWiFi();

  if(!LittleFS.begin(true))
  {
    Serial.println("LittleFS Error");
    return;
  }

  Serial.println("LittleFS Mounted");

  server.on("/", handleRoot);
  server.on("/style.css", handleCSS);
  server.on("/script.js", handleJS);
  server.on("/data", handleData);
  server.on("/relay", handleRelay);
  server.on("/mode", handleMode);


  server.begin();

  Serial.println("Web Server Started");
  Serial.print("Open : http://");
  Serial.println(WiFi.localIP());

  pinMode(DHTPIN, INPUT_PULLUP);   // aktifkan pull-up internal ESP32 untuk DHT21
  dht.begin();

  pinMode(HYGRO, INPUT);

  pinMode(RELAY_1, OUTPUT);
  pinMode(RELAY_2, OUTPUT);

  digitalWrite(RELAY_1, LOW);
  digitalWrite(RELAY_2, LOW);

  ThingSpeak.begin(client);

  lcd.init();
  lcd.backlight();

  lcd.setCursor(0,0);
  lcd.print(" IoT SMART FARM ");

  lcd.setCursor(0,1);
  lcd.print("Temp :");

  lcd.setCursor(0,2);
  lcd.print("Hum  :");

  lcd.setCursor(0,3);
  lcd.print("Soil :");
}

// ======================
// LOOP
// ======================

void loop()
{
  server.handleClient();

  // ======================
  // SENSOR
  // ======================
  if(sensorMillis >= sensorInterval)
  {
    float newH = dht.readHumidity();
    float newC = dht.readTemperature();

    if(!isnan(newH) && !isnan(newC))
    {
      h = newH;
      c = newC;
    }
    else
    {
      Serial.println("DHT read gagal, pakai nilai sebelumnya");
    }

    soil = analogRead(HYGRO);

    soilPercentage =
      map(soil,0,4095,100,0);

    soilPercentage =
      constrain(
        soilPercentage,
        0,
        100);

    Serial.println();

    Serial.print("Temp : ");
    Serial.println(c);

    Serial.print("Hum  : ");
    Serial.println(h);

    Serial.print("Soil : ");
    Serial.println(soilPercentage);

    sensorMillis = 0;
  }

  // ======================
  // AUTO FAN
  // ======================
  if(!manualFan)
  {
    if(c > TEMP_THRESHOLD ||
       h > HUM_THRESHOLD)
    {
      fanStatus = true;
    }
    else
    {
      fanStatus = false;
    }

    digitalWrite(RELAY_1,
                 fanStatus);
  }

  // ======================
  // AUTO PUMP
  // ======================
  if(!manualPump)
  {
    if(soilPercentage < SOIL_ON)
    {
      pumpStatus = true;
    }
    else if(soilPercentage > SOIL_OFF)
    {
      pumpStatus = false;
    }

    digitalWrite(RELAY_2,
                 pumpStatus);
  }

  // ======================
  // LCD
  // ======================
  if(displayMillis >= displayInterval)
  {
    lcd.setCursor(7,1);
    lcd.print("      ");
    lcd.setCursor(7,1);
    lcd.print(c,1);

    lcd.setCursor(7,2);
    lcd.print("      ");
    lcd.setCursor(7,2);
    lcd.print(h,1);

    lcd.setCursor(7,3);
    lcd.print("      ");
    lcd.setCursor(7,3);
    lcd.print(soilPercentage);

    lcd.setCursor(13,1);
    lcd.print("F:");
    lcd.print(fanStatus ? "ON " : "OFF");

    lcd.setCursor(13,2);
    lcd.print("P:");
    lcd.print(pumpStatus ? "ON " : "OFF");

    displayMillis = 0;
  }

  // ======================
  // THINGSPEAK
  // ======================
  if(thingSpeakMillis >= thingSpeakInterval)
  {
    ThingSpeak.setField(1, c);
    ThingSpeak.setField(2, h);
    ThingSpeak.setField(3, soilPercentage);
    ThingSpeak.setField(4, fanStatus);
    ThingSpeak.setField(5, pumpStatus);

    int statusCode =
      ThingSpeak.writeFields(
        channelNum,
        WriteAPIKey);

    if(statusCode == 200)
    {
      Serial.println(
      "Data berhasil dikirim");
    }
    else
    {
      Serial.print(
      "ThingSpeak Error : ");

      Serial.println(statusCode);
    }

    thingSpeakMillis = 0;
  }
}
