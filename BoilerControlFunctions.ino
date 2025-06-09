//***********************************************************************************************
// Функция обработчик сообщений Телеграм бота
//***********************************************************************************************
void newMsg(FB_msg& msg) {

  String msgID = msg.chatID;  // сохраняем chatID запроса, чтобы отправлять ответы только запросившему
  // разрешить обновление прошивки для Админа
  if (msg.OTA && (msg.chatID == ADMIN_ID)) bot.update();  // telegram update 
  
  if (msg.text == "/status" || msg.text == "Обзор") {
    String buf;
    (buf = "Т батареи " + String(tempBat) + "\n");
    (buf += "Т воздуха " + String(tempOut) + "\n");
    (buf += "Rh воздуха " + String(humidity) + "\n");
    (buf += "RSSI  " + String(rssi) + "\n");
    (buf += "Уставка Тбат " + String(data.get("tempBatBorder")) + "\n");
    (buf += "Уставка Твозд " + String(data.get("tempOutBorder")) + "\n");
    ((bool)data.get("alarmFlag")) ? (buf += "Тревоги ON\n") : (buf += "Тревоги OFF\n");    
    bot.sendMessage(buf, msgID);  // отправили сообщение
  }

  if (msg.text == "/alarm_stop" || msg.text == "Стоп тревог"){
    data.set("alarmFlag", 0);
    bot.sendMessage("Отслеживание тревог прекращено", chatId);  // отправили сообщение
  }

  if (msg.text == "/alarm_start" || msg.text == "Старт тревог"){
    data.set("alarmFlag", 1);
    bot.sendMessage("Начато отслеживание тревог", chatId);  // отправили сообщение
  }
}  // end void newMsg

//***********************************************************************************************
// Функция обновляет содержимое chatId
//***********************************************************************************************
void chatIdRefresh() {
  uint16_t counter = dataId.amount();  // количество пар
  chatId = "";
  for (uint16_t i = 1; i <= counter; i++) {
  // uint16_t k = i-1;  
  String text2 = dataId.get(i-1);     // индекс начинается с 0
  chatId += text2;
  if (i < counter) chatId += ",";   // добавляем разделительную запятую между отдельными telegram_id
  }
}  // end void chatIdRefresh()

//***********************************************************************************************
// Функция устанавливает WiFi соединения
//***********************************************************************************************
void initWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi ..");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(1000);
  }
  Serial.println(WiFi.localIP());
}  // end void initWiFi()

//***********************************************************************************************
// Функция выводит на экран значения температуры, влажности, RSSI
// и оставшееся время до импульса нагрева датчика 
//***********************************************************************************************
void showScreen() {
    oled.clear();                         // очищаем дисплей
    oled.setScale(2);                     // масштаб текста (1..4)
    oled.setCursor(0, 0);                 // курсор на начало 1 строки
    oled.print("Tбат ");                  // вывод Т
    oled.print(tempBat, 1);               // вывод температуры батареи
    oled.setCursor(0, 2);                 // курсор на начало 2 строки
    oled.print("Tout ");                  // вывод Т
    oled.print(tempOut, 1);               // вывод температуры воздуха
    oled.print(" ");                      // вывод " "
     
   #ifdef USE_SHT41                       // если используется датчик SHT41
    // counterDown это время, оставшееся до включения нагрева датчика SHT4x
    float counterDown = (heat4xPeriod - (millis() - heatTmr))/1000;  
    if (humidity > heat4xBorder) {       // если значение влажности больше heat4xBorder       
      oled.print(counterDown, 0);        // вывод значения времени до начала нагрева counterDown     
    }  // end IF  
   #endif
  
   #ifdef USE_SHT31                          // если используется датчик SHT31
    oled.print((heatFlag) ? " On" : " Off"); // вывод "On" если датчик греется
   #endif
    
    oled.setCursor(0, 4);                 // курсор на начало 3 строки
    oled.print("rH   ");                  // вывод H 
    oled.print(humidity, 1);              // вывод значения Humidity
    oled.setCursor(0, 6);                 // курсор на начало 4 строки
    oled.print("RSSI ");                  // вывод RSSI
    oled.print(rssi);                     // вывод значения RSSI.
    oled.update();                        // Вывод содержимого буфера на дисплей. Только при работе с буфером.
} // end showScreen

// функция отправляет данные на сервер NarodMon
// void sendToNarodMon() {
//   String buf;                           // Буфер для отправки
//   buf += F("#ESP32");
//   buf += WiFi.macAddress();
//   buf += F("\n");
//   buf.replace(":", "");                 // убираем из строки символы ":"
//   buf += F("#Temp1#");
//   buf += tempOut;
//   buf += F("#Подвал\n");                //NarodMon: вывод температуры подвала
//   buf += F("#RH1#");
//   buf+= humidity;
//   buf += F("#Подвал\n");                //NarodMon: вывод влажности подвала
//   buf += F("#DBM#");
//   buf += rssi;
//   buf += F("#Подвал\n");                //NarodMon: вывод силы сигнала Wi-Fi, dBm
//   buf += F("##\n");                     //NarodMon: закрываем пакет
//   client.connect("narodmon.ru", 8283);  //NarodMon: Подключаемся
//   client.print(buf.c_str());            // И отправляем данные в сеть
//   client.stop();                        // Разрываем соединение с сервером
// }

//***********************************************************************************************
// Функция отправляет данные на сервер open-monitoring.online
//***********************************************************************************************
void sendToOpenMon() {
  String buf;                                                  // Буфер для отправки
  buf.reserve(150);                                            // резервируем память с небольшим запасом
  buf += "http://open-monitoring.online/get?cid=3844&key=";    // формируем заголовок
  buf += OpenMonKey;                                           // добавляем пароль пользователя 
  buf += "&p1=";  
  buf += tempOut;                                              // добавляем температуру воздуха 
  buf += "&p2=";
  buf += humidity;                                             // добавляем влажность
  buf += "&p3=";
  buf += tempBat;                                              // добавляем температуру батареи 
  buf += "&p4=";
  buf += rssi;                                                 // вывод силы сигнала Wi-Fi, dBm
  http.begin(buf.c_str());                                     // отправляем сформированную строку
  http.GET();                                                  // Send HTTP GET request
  http.end();                                                  // Free resources
}
