// Датчик SHT41, SHT31  SDA - 21, SCL - 22
// Экран SSH1106 1,3''  SDA - 21, SCL - 22
// Датчик температуры DS18B20 - 16
// Esp32 с антенной
//***********************************************************************************************
// Ключи и пароли
//***********************************************************************************************
const char* ssid = "*****";         // wifi Login
const char* password = "*****";      // wifi Password
const char* mqttLogin = "*****";        // mqtt Login
const char* mqttPass = "*****";         // mqtt Password

const char* hubPrefix = "*****";  // GyverHub hubPrefix
const char* hubClientID = "*****";        // GyverHub Client ID
const char* OpenMonKey = "*****";         // Open Monitoring Key
const char* otaPass = "*****";          // OTA Password
#define BOT_TOKEN "*******"  // Telegram bot token
// 
#define CHAT_ID "****,****"                
#define OLEG_ID "****"                // кому разрешено обновлять прошивку в боте

//***********************************************************************************************
// Константы и дефайны
//***********************************************************************************************

//Необходимо выбрать, какой используется датчик температуры и влажности и оставить только одну строку. Другие строки должны быть закомментированы.
//#define USE_SHT41                           //использовать датчик Sensirion SHT41
#define USE_SHT31                           //использовать датчик Sensirion SHT31

#ifdef USE_SHT31                    // если используется датчик SHT31
#define heat3xTime 5*60*1000L       // время, на которое включается ежедневный нагрев датчика SHT3x
#include <SensirionI2cSht3x.h>      // библиотека датчиков температуры и влажности SHT3х
SensirionI2cSht3x sht3x;            // создание объекта датчика sht3x библиотеки SensirionI2cSht3x
#endif

#ifdef USE_SHT41                   // если используется датчик SHT41
#define heat4xBorder 75            // значение влажности, выше которого включается нагрев датчика SHT4x 
#include <SensirionI2cSht4x.h>     // библиотека датчиков температуры и влажности SHT4х
SensirionI2cSht4x sht4x;           // создание объекта датчика sht4x библиотеки SensirionI2cSht4x
#endif

#define dsPin 16                   // GPIO16, к которому подключен датчик DS18B20
#define sensorReadPeriod 1000      // период между опросами датчиков в мс
#define botAlarmPeriod 60*1000L    // период между посылками тревожных сообщений в Телеграм
#define openMonPeriod 5*60*1000L   // период между отправкой данных на сервер ОМ в мс
#define narodMonPeriod 10*60*1000L // период между отправкой данных на сервер NM в мс
#define checkWifiPeriod 30*1000L   // период проверки состояния WiFi соединения в мс
#define heat4xPeriod 120*1000L     // период включения нагрева SHT4x 
#define heatPeriod 24*60*60*1000L  // период безусловного включения нагрева любого датчика  (время МЕЖДУ включениями)
#define oledInvertPeriod 60*1000L  // период инверсии дисплея
#define WDT_TIMEOUT 30             // 30 секунд отсутствия отклика для перезагрузки через WDT

//***********************************************************************************************************************
// Библиотеки
//***********************************************************************************************************************
#include <esp_task_wdt.h>       // библиотека WatchDogTimer
#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <ArduinoOTA.h>         //бибилотека ОТА обновления по WiFi 
#include <ESPmDNS.h>            // нужно для работы бибилиотеки ArduinoOTA.h
#include <WiFiUdp.h>            // нужно для работы бибилиотеки ArduinoOTA.h
#include <GyverOLED.h>          //библиотека дисплея 
#include <GyverDS18.h>          // библиотека датчика температуры DS18B20
#include <Arduino.h>
#include <TimerMs.h>            // библиотека таймера
#include <GyverHub.h>           // GyverHub 
#include <FastBot.h>            // библиотека управления телеграм-ботом
#include <FileData.h>           // для сохранения переменных в памяти ESP32 вместо EEPROM
#include <LittleFS.h>           // для сохранения переменных в памяти ESP32 вместо EEPROM

struct Data {                  // структура для хранения настроек в памяти ESP32
  float humCorrection = 0;     // поправка влажности датчика 
  uint8_t tempBatBorder = 30;  // уставка измеренного значения температуры батареи
  uint8_t tempOutBorder = 15;  // уставка измеренного значения температуры воздуха
  bool alarmFlag = 1;          // будет ли бот реагировать на нарушение уставок  
};
Data myData;  // объявляем структуру myData с типом  Data
// создание объекта data библиотеки FileData для сохранения настроек на флеше ESP32
FileData data(&LittleFS, "/myData.dat", 136, &myData, sizeof(myData));

//******************************************************************************************************************
// Объекты библиотек
//******************************************************************************************************************
GyverOLED<SSH1106_128x64> oled;           // создание объекта экрана SSH1106 1,3''
GyverDS18Single dsTemp(dsPin);            // создаем объект датчика температуры DS18B20
HTTPClient http;                          // создаем объект http библиотеки HTTPClient
WiFiClient client;                        // создаем объект client библиотеки WiFiClient
TimerMs oledTmr(oledInvertPeriod, 1, 0);  // создаем объект oledTmr таймера TimerMs с периодом oledInvertPeriod
TimerMs heat4xTmr(heat4xPeriod, 1, 0);    // создаем объект heat4xTmr таймера TimerMs с периодом heat4xPeriod
TimerMs checkWifiTmr(checkWifiPeriod, 1, 0);   // создаем объект checkWifiTmr таймера TimerMs с периодом checkWifiPeriod
TimerMs sensorReadTmr(sensorReadPeriod, 1, 0); // создаем объект sensorReadTmr таймера TimerMs с периодом sensorReadPeriod
TimerMs botAlarmTmr(botAlarmPeriod, 1, 0);     // создаем объект botAlarmTmr таймера TimerMs с периодом botAlarmPeriod
GyverHub hub;                                  // создаем объект GyverHub
FastBot bot(BOT_TOKEN);                        // создаем объект FastBot

//*********************************************************************************************************************
// Переменные
//**********************************************************************************************************************
float tempBat;              // значение температуры батареи
float tempOut;              // значение температуры воздуха
float humidity = 50;        // значение влажности
float tempTempOut;          // первичное значение температуры с датчика до проверки на выброс
float tempHumidity;         // первичное значение влажности с датчика до проверки на выброс
int8_t rssi;                // переменная измеренного значения rssi, dB
uint32_t heatTmr = 0;       // переменная таймера нагрева датчика SHT31
uint32_t openMonTmr = 0;    // переменная таймера отправки сообщений на сервер open-monitoring.online
uint32_t narodMonTmr = 0;   // переменная таймера отсылки данных на сервер NarodMon
bool heatFlag = 0;          // флаг нагрева датчика
bool oledFlag = 0;          // флаг состояния инверсии дисплея
//*********************************************************************************************************
// Декларация функций
//*********************************************************************************************************
void initWiFi();
void newMsg(FB_msg& msg);
void showScreen();
void sendToOpenMon();

//*********************************************************************************************************
// билдер GyverHub
//*********************************************************************************************************
void build(gh::Builder& b) {     
  b.Title("Климат на балконе").fontSize(30).color(gh::Colors::Default); // добавим заголовок
  // вывод температуры и влажности воздуха
  if (b.beginRow()) {  
   b.Label_("TempOut", tempOut).label("Температура воздуха").color(gh::Colors::Red);
   b.Label_("Hum", humidity).label("Влажность").color(gh::Colors::Aqua);
   b.endRow();  
  }
  // вывод температуры батареи и RSSI
  if (b.beginRow()) {  
   b.Label_("TempBat", tempBat).label("Температура батареи").color(gh::Colors::Red);
   b.Label_("Rssi", rssi).label("RSSI").color(gh::Colors::Aqua);
   b.endRow();  
  }
  // добавляем спиннеры с уставкой температуры
  if (b.beginRow()) {
    if (b.Spinner(&myData.tempBatBorder).range(0, 50, 1).label("Уставка Т батареи").click()) data.update();
    if (b.Spinner(&myData.tempOutBorder).range(0, 25, 1).label("Уставка Т воздуха").click()) data.update();
    b.endRow();
  }
  // добавляем спиннер с поправкой влажности и флаг тревог
  if (b.beginRow()) {
    if (b.Spinner(&myData.humCorrection).range(-10, 10, 1).label("Поправка влажности").click()) data.update();
    if (b.Switch(&myData.alarmFlag).label("Отслеживание тревог").click()) data.update();
    b.endRow();
  } 
}  // end void build()

//**********************************************************************************************************************
// SETUP
//**********************************************************************************************************************
void setup() {

  pinMode(dsPin, INPUT);                      // назначаем dsPin, как вход  

  esp_task_wdt_init(WDT_TIMEOUT, true);       //enable panic so ESP32 restarts
  esp_task_wdt_add(NULL);                     //add current thread to WDT watch

  LittleFS.begin();                          // инициализация файловой системы на флеше для записи настроек
  FDstat_t stat = data.read();               // считываем данные настроек из флеша. При первом запуске во флеш пишутся данные из структуры
  
  Serial.begin(115200);
  Wire.begin();                             // SensirionI2cSht3x.h and SensirionI2cSht4x.h 

  #ifdef USE_SHT31                         // если используется датчик SHT31
    sht3x.begin(Wire, SHT31_I2C_ADDR_44);  // SensirionI2cSht3x.h 
    sht3x.disableHeater();                 // изначально выключаем нагрев датчика 
  #endif
  
  #ifdef USE_SHT41                         // если используется датчик SHT41
    sht4x.begin(Wire, SHT41_I2C_ADDR_44);  // SensirionI2cSht4x.h 
  #endif 

  dsTemp.setParasite(0);         // установка паразитного питания DS18B20
  dsTemp.setResolution(12);      // установка разрешения DS18B20
  dsTemp.requestTemp();          // первый запрос на измерение DS18B20

  oled.init();                   // инициализация дисплея
  oled.setContrast(10);          // яркость 0..255
  oled.textMode(BUF_REPLACE);    // вывод текста на экран с заменой символов
  oled.invertDisplay(oledFlag);  // вывод текста на экран с заменой символов
  oled.flipH(true);              // true/false - отзеркалить по горизонтали (для переворота экрана)
  oled.flipV(true);              // true/false - отзеркалить по вертикали (для переворота экрана)
  
  initWiFi();                   // установили соединение WiFi

 // библиотека ArduinoOTA организует все нужное для ОТА прошивки
  ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else  // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.setHostname("ESP32_Urozhaynaya");
  ArduinoOTA.setPassword(otaPass);
  ArduinoOTA.begin();

  hub.mqtt.config("m6.wqtt.ru", 17108, mqttLogin, mqttPass);  // подключаем платный защищенный MQTT сервис
  // hub.mqtt.config(F("test.mosquitto.org"), 1883);          // подключаем бесплатный незащищенный MQTT сервис
  hub.config(hubPrefix, F("Urozhaynaya"), F("f015"));
  hub.onBuild(build);
  hub.begin();
 
  bot.setChatID(CHAT_ID);      // задаем  CHAT_ID бота
  bot.setPeriod(5000);         // период опроса в мс (по умолч. 3500)
  bot.attach(newMsg);          // подключаем функцию-обработчик сообщений  
  bot.showMenu("Обзор \t Стоп тревог \t Старт тревог"); // показываем меню бота с сообщением

}  // end void Setup()

//***********************************************************************************************
// LOOP
//***********************************************************************************************
void loop() {

  esp_task_wdt_reset();  // сбрасываем Watch Dog Timer чтобы не прошла перезагрузка  
  
  ArduinoOTA.handle();  // Включаем поддержку ОТА

  data.tick();  // сохранение настроек во Флеш памяти по таймауту

  bot.tick();   // тикаем для работы телеграм бота

  hub.tick();                  // тикаем для работы конструктора интерфейса
  static gh::Timer tmr(2000);  // период 2 секунды  
  if (tmr) {                   // если прошел период
    hub.sendUpdate("TempOut"); // обновляем значение температуры
    hub.sendUpdate("TempBat"); // обновляем значение температуры
    hub.sendUpdate("Hum");     // обновляем значение влажности
    hub.sendUpdate("Rssi");    // обновляем значение RSSI
  }
  
  #ifdef USE_SHT31                         // если используется датчик SHT31     
  // с периодом heatPeriod включаем прогрев датчика SHT31 на время heat3xTime
  // начальные значения heatFlag = 0, heatTmr = 0
  if (millis() - heatTmr >= (heatFlag ? heat3xTime : heatPeriod)) {       
   heatTmr = millis();                     // сброс таймера на начало нагрева датчика
   heatFlag = !heatFlag;                   // переключаем флаг состояния нагрева датчика
   if (heatFlag) sht3x.enableHeater();     
   else sht3x.disableHeater();       
  } // end If
  #endif  

  #ifdef USE_SHT41                         // если используется датчик SHT41
  // подогреваем датчик SHT41 если Humidity > heat4xBorder с периодом heat4xPeriod на 1 секунду  
  // или если пришло время ежесуточного прогрева датчика SHT41
  if (((humidity > heat4xBorder) && heat4xTmr.tick()) || ((millis() - heatTmr) > heatPeriod)) { 
    heatTmr = millis();                                              // сброс таймера на начало нагрева датчика
    sht4x.activateHighestHeaterPowerLong(tempOut, tempHumidity); // SensirionI2cSht4x.h 
    humidity = tempHumidity + humCorrection;                         // SensirionI2cSht4x.h
    showScreen();                                                    // вывод показаний датчиков на экран
    delay(1000);                                                     // чтобы успеть увидеть цифры после нагрева    
  } // end If 
  #endif  

  if (oledTmr.tick()) {                               // если пришло время инвертировать дисплей
    oledFlag = !oledFlag;                             // инвертируем флаг состояния дисплея
    oled.invertDisplay(oledFlag);                     // инвертируем дисплей
  }

  // если пришло время опроса датчиков 
  if (sensorReadTmr.tick()){ 
    #ifdef USE_SHT31                         // если используется датчик SHT31
      sht3x.measureSingleShot(REPEATABILITY_HIGH, false, tempOut, humidity); // SensirionI2cSht3x.h 
    #endif  
    #ifdef USE_SHT41                         // если используется датчик SHT41
      sht4x.measureHighPrecision(tempOut, humidity);    // SensirionI2cSht4x.h    
    #endif  
    // Измерение датчиком DS18B20
    if (dsTemp.ready()) {            // если измерения готовы по таймеру
      if (dsTemp.readTemp()) {       // если чтение успешно
        tempBat = dsTemp.getTemp();  // записали текущую температуру
      } 
     dsTemp.requestTemp();            // запрос следующего измерения
    }

    rssi = WiFi.RSSI();
    humidity = humidity + myData.humCorrection;
    showScreen();                      // вывод показаний датчиков на экран
        
  }  // end if 

  // восстанавливаем соединение при случайной пропаже  
  if (checkWifiTmr.tick() && (WiFi.status() != WL_CONNECTED)) {
    Serial.println("Reconnecting to WiFi...");
    WiFi.disconnect();
    initWiFi();                          // установили соединение WiFi
  }

  // если температура батареи ниже уставки, высылаем сообщение в Телеграм
  if((tempBat < myData.tempBatBorder) && botAlarmTmr.tick() && myData.alarmFlag) {
    String buf;
    (buf = "Т батареи " + String(tempBat) + "\n");
    (buf += "ниже заданной " + String(myData.tempBatBorder) + " !\n");
    bot.sendMessage(buf, CHAT_ID);  // отправили сообщение по списку
  }

    // если температура воздуха ниже уставки, высылаем сообщение в Телеграм
  if((tempOut < myData.tempOutBorder) && botAlarmTmr.tick() && myData.alarmFlag) {
    String buf;
    (buf = "Т воздуха " + String(tempOut) + "\n");
    (buf += "ниже заданной " + String(myData.tempOutBorder) + " !\n");
    bot.sendMessage(buf, CHAT_ID);  // отправили сообщение по списку
  }
 
  // Если пришло время очередной отправки на open-monitoring.online и прошло заданное время с момента последнего нагрева датчика 
  if ((millis() - openMonTmr) >= openMonPeriod && (millis() - heatTmr) > (heat4xPeriod - 3000)) {
    openMonTmr = millis();               // сбрасываем таймер отправки данных  
    sendToOpenMon();                     // отправляем данные на open-monitoring.online
  }                                      // end if (sendtoOM)

  // Если пришло время очередной отправки на NarodMon и прошло заданное время с момента последнего нагрева датчика 
  // if (((millis() - narodMonTmr) >= narodMonPeriod) && ((millis() - heatTmr) >= (heat4xPeriod - 3000))) {      
  //   narodMonTmr = millis();               // сбрасываем таймер отправки данных
  //   sendToNarodMon();                     // отправляем данные на NarodMon
  // }                                       // end if (sendtonm)

}  // end Loop


