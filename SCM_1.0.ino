#include "AppPref.h"

const char ssid[] = "ganz2";
const char pass[] = "Password";

RTC_DS3231 rtc;
DateTime lastDateTime;
char days[7][12] = { "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday" };

TaskHandle_t PumpTaskHandle;
QueueHandle_t PumpQueue;
SemaphoreHandle_t mySemaphore;

void pumpControlTask(void *parameter) {
  Serial.print("running on core: ");
  Serial.println(xTaskGetAffinity(NULL));
  int duration[2];

  while (true) {
    if (xQueueReceive(PumpQueue, &duration, pdMS_TO_TICKS(1000)) == pdTRUE) {
      int Min = duration[0];
      int Sec = duration[1];
      TickType_t lastWakeTime = xTaskGetTickCount();

      modePaused = true;
      pumpRunning = true;
      while (Min > 0 || Sec > 0) {
        digitalWrite(pump, LOW);
        vTaskDelayUntil(&lastWakeTime, pdMS_TO_TICKS(1000));

        Sec--;
        if (Sec < 0) {
          Sec = 59;
          Min--;
        }
      }
      digitalWrite(pump, HIGH);
      pumpRunning = false;
      modePaused = false;
      Serial.println("Pompa Mati");
    }
  }
}

// void sendDataTask(void *parameter) {
//   for (;;) {
//     dataDHT::DHT_Values dhtValues = dataDHT::getValues();

//     DateTime now = rtc.now();
//     char waktu[50];
//     sprintf(waktu, "/%04d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

//     Serial.println("Attempting to take semaphore in sendDataTask");
//     if (xSemaphoreTake(mySemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
//     Serial.println("Semaphore taken by sendDataTask");
//     dtostrf(dhtValues.suhu, 6, 2, temp_str);
//     dtostrf(dhtValues.kelembaban, 6, 2, humi_str);
//     StaticJsonDocument<200> jsonDocument;
//     jsonDocument["suhu"] = temp_str;
//     jsonDocument["kelembaban"] = humi_str;
//     jsonDocument["waktu"] = waktu;
//     char jsonBuffer[200];
//     serializeJson(jsonDocument, jsonBuffer);
//     client.publish(Sensor_Topic, jsonBuffer);
//      Serial.println("Data sent by sendDataTask");

//       vTaskDelay(pdMS_TO_TICKS(1000));

//       xSemaphoreGive(mySemaphore);
//       Serial.println("Semaphore given by sendDataTask");
//     } else {
//       Serial.println("Send data timeout");
//     }

//     vTaskDelay(pdMS_TO_TICKS(5000));
//   } 
// }

// void historyDataTask(void *parameter) {
//   for (;;) {
//     dataDHT::DHT_Values dhtValues = dataDHT::getValues();
//     DateTime now = rtc.now();
//     char waktu[50];
//     sprintf(waktu, "/%04d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

//     if (xSemaphoreTake(mySemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
//       if (dhtValues.suhu != 0 && dhtValues.kelembaban != 0) {
//         dtostrf(dhtValues.suhu, 6, 2, temp_str);
//         dtostrf(dhtValues.kelembaban, 6, 2, humi_str);
//         DynamicJsonDocument jsonDocument(200);
//         jsonDocument["suhu"] = temp_str;
//         jsonDocument["kelembaban"] = humi_str;
//         jsonDocument["mode"] = StatusModeH;
//         jsonDocument["waktu"] = waktu;
//         jsonDocument["fan"] = statusFan ? "ON" : "OFF";
//         jsonDocument["pump"] = statusPump ? "ON" : "OFF";
//         String output;
//         serializeJson(jsonDocument, output);
//         client.publish(History, output.c_str());
//       }

//       vTaskDelay(pdMS_TO_TICKS(1000));

//       xSemaphoreGive(mySemaphore);
//     } else {
//       Serial.println("History data timeout");
//     }

//     vTaskDelay(pdMS_TO_TICKS(10000));
//   }
// }

void dataTask(void *parameter) {
  for (;;) {
    dataDHT::DHT_Values dhtValues = dataDHT::getValues();
    DateTime now = rtc.now();
    char waktu[50];
    sprintf(waktu, "/%04d/%02d/%02d %02d:%02d:%02d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());

    if (xSemaphoreTake(mySemaphore, pdMS_TO_TICKS(1000)) == pdTRUE) {
      if (dhtValues.suhu != 0 && dhtValues.kelembaban != 0) {
        char temp_str[10];
        char humi_str[10];
        dtostrf(dhtValues.suhu, 6, 2, temp_str);
        dtostrf(dhtValues.kelembaban, 6, 2, humi_str);

        // Send data
        StaticJsonDocument<200> sendDataJson;
        sendDataJson["suhu"] = temp_str;
        sendDataJson["kelembaban"] = humi_str;
        sendDataJson["waktu"] = waktu;
        char sendDataBuffer[200];
        serializeJson(sendDataJson, sendDataBuffer);
        client.publish(Sensor_Topic, sendDataBuffer);

        // History data
        DynamicJsonDocument historyDataJson(200);
        historyDataJson["suhu"] = temp_str;
        historyDataJson["kelembaban"] = humi_str;
        historyDataJson["mode"] = StatusModeH;
        historyDataJson["waktu"] = waktu;
        historyDataJson["fan"] = statusFan ? "ON" : "OFF";
        historyDataJson["pump"] = statusPump ? "ON" : "OFF";
        String historyOutput;
        serializeJson(historyDataJson, historyOutput);
        client.publish(History, historyOutput.c_str());

        vTaskDelay(pdMS_TO_TICKS(1000));

        xSemaphoreGive(mySemaphore);
      } else {
        Serial.println("Invalid sensor data");
      }
    } else {
      Serial.println("Data send timeout");
    }

    vTaskDelay(pdMS_TO_TICKS(5000)); // Delay 5 seconds
  }
}

void messageReceived(char *topic, byte *payload, unsigned int length) {
  DynamicJsonDocument jsonDocument(1024);
  DeserializationError error = deserializeJson(jsonDocument, payload, length);

  if (strcmp(topic, setPointW) == 0) {
    setPoints.MinS = jsonDocument["MinS"].as<int>();
    setPoints.MidS = jsonDocument["MidS"].as<int>();
    setPoints.MinK = jsonDocument["MinK"].as<int>();
    setPoints.MidK = jsonDocument["MidK"].as<int>();
    EEPROM.put(Address.addresMinS, setPoints.MinS);
    EEPROM.put(Address.addresMidS, setPoints.MidS);
    EEPROM.put(Address.addresMinK, setPoints.MinK);
    EEPROM.put(Address.addresMidK, setPoints.MidK);
    EEPROM.commit();
    Serial.println("Data Berhasil di simpan");
  } else if (strcmp(topic, setPointSW) == 0) {
    if (jsonDocument.containsKey("jam1") && jsonDocument.containsKey("menit1")) {
      setSchedule1.AH1 = jsonDocument["jam1"].as<int>();
      setSchedule1.AM1 = jsonDocument["menit1"].as<int>();
      EEPROM.put(Address.addresAH1, setSchedule1.AH1);
      EEPROM.put(Address.addresAM1, setSchedule1.AM1);
      EEPROM.commit();
      Serial.println("Data Jadwal 1 Berhasil disimpan");
    } else if (jsonDocument.containsKey("jam2") && jsonDocument.containsKey("menit2")) {
      setSchedule1.AH2 = jsonDocument["jam2"].as<int>();
      setSchedule1.AM2 = jsonDocument["menit2"].as<int>();
      EEPROM.put(Address.addresAH2, setSchedule1.AH2);
      EEPROM.put(Address.addresAM2, setSchedule1.AM2);
      EEPROM.commit();
      Serial.println("Data Jadwal 2 Berhasil disimpan");
    } else if (jsonDocument.containsKey("jam3") && jsonDocument.containsKey("menit3")) {
      setSchedule1.AH3 = jsonDocument["jam3"].as<int>();
      setSchedule1.AM3 = jsonDocument["menit3"].as<int>();
      EEPROM.put(Address.addresAH3, setSchedule1.AH3);
      EEPROM.put(Address.addresAM3, setSchedule1.AM3);
      EEPROM.commit();
      Serial.println("Data Jadwal 3 Berhasil disimpan");
    }
  } else if (strcmp(topic, setPointMW) == 0) {
    Mode.statusModeW = jsonDocument["mode"].as<int>();
    EEPROM.put(Address.addresMode, Mode.statusModeW);
    EEPROM.commit();
    // Serial.println("Data written to EEPROM: " + Mode.statusModeW);
    // Serial.println("Mode Kontrol :");
    // Serial.print(Mode.statusModeW);
  } else if (strcmp(topic, ScheduleMode) == 0) {
    Mode.statusScheduleM = jsonDocument["Smode"].as<String>();
    EEPROM.put(Address.addresSmode, Mode.statusScheduleM);
    EEPROM.commit();
    lcd.clear();
    lcd.setCursor(0, 1);
    lcd.print("Schedule: ");
    lcd.setCursor(0, 2);
    lcd.print(Mode.statusScheduleM);
    delay(1000);
    lcd.clear();
    state = 0;
    return menuUtama();
  } else if (strcmp(topic, setTimeW) == 0) {
    setTimer.Min = jsonDocument["Menit"].as<int>();
    setTimer.Sec = jsonDocument["Detik"].as<int>();
    EEPROM.put(Address.addresMin, setTimer.Min);
    EEPROM.put(Address.addresSec, setTimer.Sec);
    EEPROM.commit();
    Serial.println("Berhasil Setting Timer");
  } else if (strcmp(topic, ControllFan) == 0) {
    Fan = jsonDocument["fan"].as<int>();
    if (Fan == 0) {
      digitalWrite(fan, HIGH);
      statusFan = false;
    } else {
      digitalWrite(fan, LOW);
      statusFan = true;
    }
  } else if (strcmp(topic, ControllPump) == 0) {
    Pump = jsonDocument["pump"].as<int>();
    if (Pump == 0) {
      digitalWrite(pump, HIGH);
      statusPump = false;
    } else {
      digitalWrite(pump, LOW);
      statusPump = true;
    }
  }
}

void connect() {
  if (WiFi.status() != WL_CONNECTED) {
    lcd.clear();
    lcd.setCursor(6, 1);
    lcd.print("Device");
    lcd.setCursor(3, 2);
    lcd.print("Disconnected");
    while (WiFi.status() != WL_CONNECTED) {
      WiFi.begin(ssid, pass);
      delay(500);
    }
    lcd.clear();
    lcd.setCursor(6, 1);
    lcd.print("Device");
    lcd.setCursor(4, 2);
    lcd.print("Connected");
    isWiFiConnected = true;
    connectionStatusMillis = millis();
  }

  while (!client.connected()) {
    String client_id = "SCM";
    client_id += String(WiFi.macAddress());
    Serial.printf("The client %s connects to the public MQTT broker\n", client_id.c_str());
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println("Terhubung ke MQTT Broker");
      client.subscribe(subscribe);
      lcd.clear();
      lcd.setCursor(6, 1);
      lcd.print("Device");
      lcd.setCursor(4, 2);
      lcd.print("Connected");
      lcd.clear();
      connectionStatusMillis = millis();
    }
  }
}

void setup() {
  Serial.begin(115200);
  lcd.init();
  lcd.backlight();
  EEPROM.begin(512);
  FastLED.addLeds<WS2812, LED_PIN, GRB>(leds, NUM_LEDS);
  pinMode(bUP, INPUT_PULLUP);
  pinMode(bOK, INPUT_PULLUP);
  pinMode(bDN, INPUT_PULLUP);
  // pinMode(modeMan, INPUT_PULLUP);
  // pinMode(modeAuto, INPUT_PULLUP);
  pinMode(buttonFan, INPUT_PULLUP);
  pinMode(buttonPump, INPUT_PULLUP);
  pinMode(fan, OUTPUT);
  pinMode(pump, OUTPUT);
  digitalWrite(fan, HIGH);
  digitalWrite(pump, HIGH);
  dataDHT::setup();

  WiFi.begin(ssid, pass);
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(messageReceived);

  connect();

  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    while (1) delay(10);
  }
  if (rtc.lostPower()) {
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // timer.setInterval(5000, sendData);
  // timer.setInterval(10000, history);
  PumpQueue = xQueueCreate(10, sizeof(int[2]));
  mySemaphore = xSemaphoreCreateMutex();
  if (PumpQueue == NULL || mySemaphore == NULL) {
    Serial.println("Failed to create queue or semaphore");
    while (1)
      ;
  }

  // Create tasks
     xTaskCreatePinnedToCore(pumpControlTask, "Pump Control Task", 10000, NULL, 1, &PumpTaskHandle, 1);  // Pin to core 1
  // xTaskCreatePinnedToCore(sendDataTask, "Send Data Task", 10000, NULL, 1, NULL, 0);                   // Pin to core 0
  // xTaskCreatePinnedToCore(historyDataTask, "History Task", 10000, NULL, 2, NULL, 1);                  // Pin to core 1
     xTaskCreatePinnedToCore(dataTask,"Data Task", 10000, NULL, 1 , NULL, 0);

  client.subscribe(subscribe);
  // client.subscribe(setPointW);
  // client.subscribe(setTime);
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    isWiFiConnected = false;
    connect();
  }
  client.loop();
  unsigned long currentMillis = millis();
  if (isWiFiConnected && (currentMillis - connectionStatusMillis >= 2000)) {
    menuUtama();
    mode(statusFan, statusPump, statusMode, StatusModeH);
    scheduleMode();
  }

  // Serial.print("Free heap: ");
  // Serial.println(esp_get_free_heap_size());
}

void menuUtama() {
  dataDHT::DHT_Values dhtValues = dataDHT::getValues();
  DateTime now = rtc.now();
  String times;
  char time[50];
  sprintf(time, "%02d:%02d %02d/%02d/%04d", now.hour(), now.minute(), now.day(), now.month(), now.year());
  times = time;
  switch (state) {
    case 0:
      lcd.setCursor(0, 0);
      lcd.print("Time:");
      lcd.print(times);
      lcd.setCursor(0, 1);
      lcd.print("Temp: ");
      lcd.print(dhtValues.suhu);
      lcd.print(" C");
      lcd.setCursor(0, 2);
      lcd.print("Humi: ");
      lcd.print(dhtValues.kelembaban);
      lcd.print(" %");
      lcd.setCursor(2, 3);
      lcd.print(statusMode);

      if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 1;
      }
      delay(100);
      break;

    case 1:
      lcd.setCursor(0, 0);
      lcd.print("->Set SetPoint ");
      lcd.setCursor(0, 1);
      lcd.print("  Set Schedule ");
      lcd.setCursor(0, 2);
      lcd.print("  Set Timer ");

      if (digitalRead(bDN) == tekan) {
        delay(tahan);
        while (digitalRead(bDN) == tekan) {}
        lcd.clear();
        state = 2;
      } else if (digitalRead(bUP) == tekan) {
        delay(tahan);
        while (digitalRead(bUP) == tekan) {}
        lcd.clear();
        state = 3;
      } else if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 4;
      }
      break;

    case 2:
      lcd.setCursor(0, 0);
      lcd.print("  Set SetPoint ");
      lcd.setCursor(0, 1);
      lcd.print("->Set Schedule ");
      lcd.setCursor(0, 2);
      lcd.print("  Set Timer ");

      if (digitalRead(bDN) == tekan) {
        delay(tahan);
        while (digitalRead(bDN) == tekan) {}
        lcd.clear();
        state = 3;
      } else if (digitalRead(bUP) == tekan) {
        delay(tahan);
        while (digitalRead(bUP) == tekan) {}
        lcd.clear();
        state = 1;
      } else if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 6;
      }
      break;

    case 3:
      lcd.setCursor(0, 0);
      lcd.print("  Set SetPoint ");
      lcd.setCursor(0, 1);
      lcd.print("  Set Schedule ");
      lcd.setCursor(0, 2);
      lcd.print("->Set Timer ");

      if (digitalRead(bDN) == tekan) {
        delay(tahan);
        while (digitalRead(bDN) == tekan) {}
        lcd.clear();
        state = 1;
      } else if (digitalRead(bUP) == tekan) {
        delay(tahan);
        while (digitalRead(bUP) == tekan) {}
        lcd.clear();
        state = 2;
      }

      if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 0;
        Time();
      }
      break;

    case 4:
      lcd.setCursor(0, 0);
      lcd.print("->Set Suhu ");
      lcd.setCursor(0, 1);
      lcd.print("  Set Kelembaban ");

      if (digitalRead(bDN) == tekan) {
        delay(tahan);
        while (digitalRead(bDN) == tekan) {}
        lcd.clear();
        state = 5;
      } else if (digitalRead(bUP) == tekan) {
        delay(tahan);
        while (digitalRead(bUP) == tekan) {}
        lcd.clear();
        state = 4;
      }

      if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 0;
        setNilaiS();
      }
      break;

    case 5:
      lcd.setCursor(0, 0);
      lcd.print("  Set Suhu ");
      lcd.setCursor(0, 1);
      lcd.print("->Set Kelembaban ");

      if (digitalRead(bUP) == tekan) {
        delay(tahan);
        while (digitalRead(bUP) == tekan) {}
        lcd.clear();
        state = 5;
      } else if (digitalRead(bDN) == tekan) {
        delay(tahan);
        while (digitalRead(bDN) == tekan) {}
        lcd.clear();
        state = 4;
      }

      if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 0;
        setNilaiK();
      }
      break;

    case 6:
      lcd.setCursor(0, 0);
      lcd.print("->Set Jadwal 1 ");
      lcd.setCursor(0, 1);
      lcd.print("  Set Jadwal 2 ");
      lcd.setCursor(0, 2);
      lcd.print("  Set Jadwal 3 ");

      if (digitalRead(bUP) == tekan) {
        delay(tahan);
        while (digitalRead(bUP) == tekan) {}
        lcd.clear();
        state = 8;
      } else if (digitalRead(bDN) == tekan) {
        delay(tahan);
        while (digitalRead(bDN) == tekan) {}
        lcd.clear();
        state = 7;
      }

      if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 0;
        setJadwal1();
      }
      break;

    case 7:
      lcd.setCursor(0, 0);
      lcd.print("  Set Jadwal 1 ");
      lcd.setCursor(0, 1);
      lcd.print("->Set Jadwal 2 ");
      lcd.setCursor(0, 2);
      lcd.print("  Set Jadwal 3 ");

      if (digitalRead(bUP) == tekan) {
        delay(tahan);
        while (digitalRead(bUP) == tekan) {}
        lcd.clear();
        state = 6;
      } else if (digitalRead(bDN) == tekan) {
        delay(tahan);
        while (digitalRead(bDN) == tekan) {}
        lcd.clear();
        state = 8;
      }

      if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 0;
        setJadwal2();
      }
      break;

    case 8:
      lcd.setCursor(0, 0);
      lcd.print("  Set Jadwal 1 ");
      lcd.setCursor(0, 1);
      lcd.print("  Set Jadwal 2 ");
      lcd.setCursor(0, 2);
      lcd.print("->Set Jadwal 3 ");

      if (digitalRead(bUP) == tekan) {
        delay(tahan);
        while (digitalRead(bUP) == tekan) {}
        lcd.clear();
        state = 7;
      } else if (digitalRead(bDN) == tekan) {
        delay(tahan);
        while (digitalRead(bDN) == tekan) {}
        lcd.clear();
        state = 6;
      }

      if (digitalRead(bOK) == tekan) {
        delay(tahan);
        while (digitalRead(bOK) == tekan) {}
        lcd.clear();
        state = 0;
        setJadwal3();
      }
      break;
  }
}

void mode(bool &statusFan, bool &statusPump, String &statusMode, String &StatusModeH) {
  if (modePaused) {
    return;
  }

  dataDHT::DHT_Values dhtValues = dataDHT::getValues();
  Mode.statusModeW = EEPROM.read(Address.addresMode);
  int suhuM = EEPROM.read(Address.addresMinS);
  int suhuR = EEPROM.read(Address.addresMidS);
  int kelM = EEPROM.read(Address.addresMinK);
  int kelR = EEPROM.read(Address.addresMidK);

  int dataMode = Mode.statusModeW;
  if (dataMode == 1) {
    leds[0] = CRGB(0, 0, 255);
    FastLED.show();
    statusMode = "Manual Mode Aktif";
    StatusModeH = "Manual";

    if (digitalRead(buttonFan) == 0) {
      digitalWrite(fan, !digitalRead(fan));
      while (digitalRead(buttonFan) == 0)
        ;
      if (digitalRead(fan) == LOW) {
        statusFan = true;
      } else {
        statusFan = false;
      }
    }

    if (digitalRead(buttonPump) == 0) {
      digitalWrite(pump, !digitalRead(pump));
      while (digitalRead(buttonPump) == 0)
        ;
      if (digitalRead(pump) == LOW) {
        statusPump = true;
      } else {
        statusPump = false;
      }
    }
  } else if (dataMode == 2) {
    leds[0] = CRGB::Purple;
    FastLED.show();
    statusMode = "Auto Mode Aktif  ";
    StatusModeH = "Auto";

    if (dhtValues.suhu >= suhuM) {
      digitalWrite(fan, LOW);
      statusFan = true;
    } else if (dhtValues.suhu <= suhuR) {
      digitalWrite(fan, HIGH);
      statusFan = false;
    } else {
      digitalWrite(fan, HIGH);
      statusFan = false;
    }

    if (dhtValues.kelembaban <= kelM) {
      digitalWrite(pump, LOW);
      statusPump = true;
    } else if (dhtValues.kelembaban >= kelR) {
      digitalWrite(pump, HIGH);
      statusPump = false;
    } else {
      digitalWrite(pump, HIGH);  // Matikan pump
      statusPump = false;
    }

  } else if (dataMode == 3) {
    leds[0] = CRGB(255, 128, 0);
    FastLED.show();
    statusMode = "Hybrid Mode Aktif";
    StatusModeH = "Hybrid";

    buttonStateFan = digitalRead(buttonFan);
    buttonStatePump = digitalRead(buttonPump);

    if (buttonStateFan == LOW && previousButtonStateFan == HIGH) {
      delay(50);
      if (digitalRead(buttonFan) == LOW) {
        manualFanControl = !manualFanControl;
        delay(50);
      }
    }
    previousButtonStateFan = buttonStateFan;

    if (manualFanControl) {
      if (digitalRead(buttonFan) == 0) {
        digitalWrite(fan, !digitalRead(fan));
        while (digitalRead(buttonFan) == 0)
          ;
      }
    } else {
      if (dhtValues.suhu >= suhuM) {
        digitalWrite(fan, LOW);
        statusFan = true;
      } else if (dhtValues.suhu <= suhuR) {
        digitalWrite(fan, HIGH);
        statusFan = false;
      } else {
        digitalWrite(fan, HIGH);
        statusFan = false;
      }
    }

    if (buttonStatePump == LOW && previousButtonStatePump == HIGH) {
      delay(50);
      if (digitalRead(buttonPump) == LOW) {
        manualPumpControl = !manualPumpControl;
        delay(50);
      }
    }
    previousButtonStatePump = buttonStatePump;
    if (manualPumpControl) {
      if (digitalRead(buttonPump) == 0) {
        digitalWrite(pump, !digitalRead(pump));
        while (digitalRead(buttonPump) == 0)
          ;
      }
    } else {
      if (dhtValues.kelembaban <= kelM) {
        digitalWrite(pump, LOW);
        statusPump = true;
      } else if (dhtValues.kelembaban >= kelR) {
        digitalWrite(pump, HIGH);
        statusPump = false;
      } else {
        digitalWrite(pump, HIGH);  // Matikan pump
        statusPump = false;
      }
    }
  }
}

void scheduleMode() {
  DateTime now = rtc.now();

  byte dataBytes[Length_data];
  int i = 0;
  while (i < Length_data) {
    dataBytes[i] = EEPROM.read(Address.addresSmode + i);
    if (dataBytes[i] == '\0') {
      break;
    }
    i++;
  }

  String dataScheduleMode = "";

  for (int j = 0; j < i; j++) {
    dataScheduleMode += (char)dataBytes[j];
  }

  setTimer.Min = EEPROM.read(Address.addresMin);
  setTimer.Sec = EEPROM.read(Address.addresSec);

  int Jam1 = EEPROM.read(Address.addresAH1);
  int Min1 = EEPROM.read(Address.addresAM1);
  int Jam2 = EEPROM.read(Address.addresAH2);
  int Min2 = EEPROM.read(Address.addresAM2);
  int Jam3 = EEPROM.read(Address.addresAH3);
  int Min3 = EEPROM.read(Address.addresAM3);

  int Min = setTimer.Min;
  int Sec = setTimer.Sec;

  static bool pumpStarted = false;

  if (dataScheduleMode == "modeSchedule1") {
    if ((now.hour() == setSchedule1.AH1 && now.minute() == setSchedule1.AM1) || (now.hour() == setSchedule1.AH2 && now.minute() == setSchedule1.AM2) || (now.hour() == setSchedule1.AH3 && now.minute() == setSchedule1.AM3)) {
      if (!pumpStarted) {
        pumpStarted = true;
        int duration[2] = { Min, Sec };
        xQueueSend(PumpQueue, &duration, portMAX_DELAY);
      }
    } else {
      pumpStarted = false;
    }
  } else if (dataScheduleMode == "modeSchedule2") {
    if ((now.hour() == setSchedule1.AH1 && now.minute() == setSchedule1.AM1) || (now.hour() == setSchedule1.AH2 && now.minute() == setSchedule1.AM2)) {
      if (!pumpStarted) {
        pumpStarted = true;
        int duration[2] = { Min, Sec };
        xQueueSend(PumpQueue, &duration, portMAX_DELAY);
      }
    } else {
      pumpStarted = false;
    }
  }
}

void setNilaiS() {
  StaticJsonDocument<200> docMinS;
  StaticJsonDocument<200> docMidS;
  char jsonMinS[20];
  char jsonMidS[20];
  while (true) {
    switch (state) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print("->Set Minimal ");
        lcd.setCursor(0, 1);
        lcd.print("  Set Rata-Rata ");
        // lcd.setCursor(0, 2);
        // lcd.print("  Set Maximal ");
        if (digitalRead(bDN) == tekan) {
          delay(tahan);
          while (digitalRead(bDN) == tekan) {}
          lcd.clear();
          state = 1;
        } else if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          state = 2;
        }
        break;

      case 1:
        lcd.setCursor(0, 0);
        lcd.print("  Set Minimal ");
        lcd.setCursor(0, 1);
        lcd.print("->Set Rata-Rata ");
        // lcd.setCursor(0, 2);
        // lcd.print("  Set Maximal ");
        if (digitalRead(bUP) == tekan) {
          delay(tahan);
          while (digitalRead(bUP) == tekan) {}
          lcd.clear();
          state = 0;
        } else if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          state = 3;
        }
        break;

        // case 2:
        //   lcd.setCursor(0, 0);
        //   lcd.print("  Set Minimal ");
        //   lcd.setCursor(0, 1);
        //   lcd.print("  Set Rata-Rata ");
        //   lcd.setCursor(0, 2);
        //   lcd.print("->Set Maximal ");
        //   if (digitalRead(bUP) == tekan) {
        //     delay(tahan);
        //     while (digitalRead(bUP) == tekan) {}
        //     lcd.clear();
        //     state = 1;
        //   } else if (digitalRead(bOK) == tekan) {
        //     delay(tahan);
        //     while (digitalRead(bOK) == tekan) {}
        //     lcd.clear();
        //     state = 5;
        //   }
        // break;

      case 2:
        setPoints.MinS = EEPROM.read(Address.addresMinS);
        docMinS["MinS"] = setPoints.MinS;
        serializeJson(docMinS, jsonMinS);
        lcd.setCursor(0, 0);
        lcd.print("Set Suhu Min: ");
        lcd.print(setPoints.MinS);
        lcd.setCursor(0, 1);
        lcd.print("                  ");
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          lcd.setCursor(8, 1);
          lcd.print("Saved!");
          delay(1000);
          client.publish(setPoint, jsonMinS);
          lcd.clear();
          state = 0;
        } else if (digitalRead(bUP) == tekan) {
          setPoints.MinS = (setPoints.MinS + 1) % 61;  // Tambah MinS dengan batasan 0-60
          EEPROM.put(Address.addresMinS, setPoints.MinS);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setPoints.MinS = (setPoints.MinS + 59) % 61;  // Kurangi MinS dengan batasan 0-60
          EEPROM.put(Address.addresMinS, setPoints.MinS);
          EEPROM.commit();
        }
        break;

      case 3:
        setPoints.MidS = EEPROM.read(Address.addresMidS);
        docMidS["MidS"] = setPoints.MidS;
        serializeJson(docMidS, jsonMidS);
        lcd.setCursor(0, 0);
        lcd.print("Set Suhu Rata: ");
        lcd.print(setPoints.MidS);
        lcd.setCursor(0, 1);
        lcd.print("                  ");

        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          lcd.setCursor(8, 1);
          lcd.print("Saved!");
          delay(1000);
          client.publish(setPoint, jsonMidS);
          lcd.clear();
          state = 0;
          return menuUtama();
        } else if (digitalRead(bUP) == tekan) {
          setPoints.MidS = (setPoints.MidS + 1) % 61;  // Tambah MinS dengan batasan 0-60
          EEPROM.put(Address.addresMidS, setPoints.MidS);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setPoints.MidS = (setPoints.MidS + 59) % 61;  // Kurangi MinS dengan batasan 0-60
          EEPROM.put(Address.addresMidS, setPoints.MidS);
          EEPROM.commit();
        }
        break;

        // case 5:
        //   setPoints.MaxS = EEPROM.read(Address.addresMaxS);
        //   lcd.setCursor(0, 0);
        //   lcd.print("Set Suhu Max: ");
        //   lcd.print(setPoints.MaxS);
        //   lcd.setCursor(0, 1);
        //   lcd.print("                  ");

        //   if (digitalRead(bUP) == tekan) {
        //   setPoints.MaxS = (setPoints.MaxS + 1) % 61;
        //     EEPROM.put(Address.addresMaxS, setPoints.MaxS);
        //     EEPROM.commit();
        //   } else if (digitalRead(bDN) == tekan) {
        //   setPoints.MaxS = (setPoints.MaxS + 59) % 61;
        //     EEPROM.put(Address.addresMaxS, setPoints.MaxS);
        //     EEPROM.commit();
        //   } else if (digitalRead(bOK) == tekan) {
        //     delay(tahan);
        //     while (digitalRead(bOK) == tekan) {}
        //     lcd.clear();
        //     lcd.setCursor(8, 1);
        //     lcd.print("Saved!");
        //     delay(1000);
        //     lcd.clear();
        //     state = 0;
        //     return menuUtama();
        //   }
        // break;
    }
  }
}

void setNilaiK() {
  StaticJsonDocument<200> docMinK;
  StaticJsonDocument<200> docMidK;
  char jsonMinK[20];
  char jsonMidK[20];
  while (true) {
    switch (state) {
      case 0:
        lcd.setCursor(0, 0);
        lcd.print("->Set Minimal ");
        lcd.setCursor(0, 1);
        lcd.print("  Set Rata-Rata ");
        // lcd.setCursor(0, 2);
        // lcd.print("  Set Maximal ");
        if (digitalRead(bDN) == tekan) {
          delay(tahan);
          while (digitalRead(bDN) == tekan) {}
          lcd.clear();
          state = 1;
        } else if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          state = 2;
        }
        break;

      case 1:
        lcd.setCursor(0, 0);
        lcd.print("  Set Minimal ");
        lcd.setCursor(0, 1);
        lcd.print("->Set Rata-Rata ");
        // lcd.setCursor(0, 2);
        // lcd.print("  Set Maximal ");
        if (digitalRead(bUP) == tekan) {
          delay(tahan);
          while (digitalRead(bUP) == tekan) {}
          lcd.clear();
          state = 0;
        } else if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          state = 3;
        }
        break;

        // case 2:
        //   lcd.setCursor(0, 0);
        //   lcd.print("  Set Minimal ");
        //   lcd.setCursor(0, 1);
        //   lcd.print("  Set Rata-Rata ");
        //   lcd.setCursor(0, 2);
        //   lcd.print("->Set Maximal ");
        //   if (digitalRead(bUP) == tekan) {
        //     delay(tahan);
        //     while (digitalRead(bUP) == tekan) {}
        //     lcd.clear();
        //     state = 1;
        //   } else if (digitalRead(bOK) == tekan) {
        //     delay(tahan);
        //     while (digitalRead(bOK) == tekan) {}
        //     lcd.clear();
        //     state = 5;
        //   }
        // break;

      case 2:
        setPoints.MinK = EEPROM.read(Address.addresMinK);
        docMinK["MinK"] = setPoints.MinK;
        serializeJson(docMinK, jsonMinK);
        lcd.setCursor(0, 0);
        lcd.print("Set Kel Min: ");
        lcd.print(setPoints.MinK);
        lcd.setCursor(0, 1);
        lcd.print("                  ");
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          lcd.setCursor(8, 1);
          lcd.print("Saved!");
          delay(1000);
          client.publish(setPoint, jsonMinK);
          lcd.clear();
          state = 0;
        } else if (digitalRead(bUP) == tekan) {
          setPoints.MinK = (setPoints.MinK + 1) % 101;  // Tambah MinK dengan batasan 0-60
          EEPROM.put(Address.addresMinK, setPoints.MinK);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setPoints.MinK = (setPoints.MinK + 100) % 101;  // Kurangi MinK dengan batasan 0-60
          EEPROM.put(Address.addresMinK, setPoints.MinK);
          EEPROM.commit();
        }
        break;

      case 3:
        setPoints.MidK = EEPROM.read(Address.addresMidK);
        docMidK["MidK"] = setPoints.MidK;
        serializeJson(docMidK, jsonMidK);
        lcd.setCursor(0, 0);
        lcd.print("Set Kel Rata: ");
        lcd.print(setPoints.MidK);
        lcd.setCursor(0, 1);
        lcd.print("                  ");

        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          lcd.setCursor(8, 1);
          lcd.print("Saved!");
          delay(1000);
          client.publish(setPoint, jsonMidK);
          lcd.clear();
          state = 0;
          return menuUtama();
        } else if (digitalRead(bUP) == tekan) {
          setPoints.MidK = (setPoints.MidK + 1) % 101;  // Tambah MinS dengan batasan 0-60
          EEPROM.put(Address.addresMidK, setPoints.MidK);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setPoints.MidK = (setPoints.MidK + 100) % 101;  // Kurangi MinS dengan batasan 0-60
          EEPROM.put(Address.addresMidK, setPoints.MidK);
          EEPROM.commit();
        }
        break;

        // case 5:
        //   setPoints.MaxK = EEPROM.read(Address.addresMaxK);
        //   lcd.setCursor(0, 0);
        //   lcd.print("Set Kel Max: ");
        //   lcd.print(setPoints.MaxK);
        //   lcd.setCursor(0, 1);
        //   lcd.print("                  ");

        //   if (digitalRead(bUP) == tekan) {
        //   setPoints.MaxK = (setPoints.MaxK + 1) % 101;
        //     EEPROM.put(Address.addresMaxK, setPoints.MaxK);
        //     EEPROM.commit();
        //   } else if (digitalRead(bDN) == tekan) {
        //   setPoints.MaxK = (setPoints.MaxK + 100) % 101;
        //     EEPROM.put(Address.addresMaxK, setPoints.MaxK);
        //     EEPROM.commit();
        //   } else if (digitalRead(bOK) == tekan) {
        //     delay(tahan);
        //     while (digitalRead(bOK) == tekan) {}
        //     lcd.clear();
        //     lcd.setCursor(8, 1);
        //     lcd.print("Saved!");
        //     delay(1000);
        //     lcd.clear();
        //     state = 0;
        //     return menuUtama();
        //   }
        // break;
    }
  }
}

void setJadwal1() {
  String Waktu;
  StaticJsonDocument<200> docJam1;
  StaticJsonDocument<200> docMenit1;
  char jsonJam1[20];
  char jsonMenit1[20];
  while (true) {
    switch (state) {
      case 0:
        setSchedule1.AH1 = EEPROM.read(Address.addresAH1);
        setSchedule1.AM1 = EEPROM.read(Address.addresAM1);
        docJam1["Jam1"] = setSchedule1.AH1;
        serializeJson(docJam1, jsonJam1);
        Waktu = "->" + String(setSchedule1.AH1) + " : " + String(setSchedule1.AM1) + "  ";
        lcd.setCursor(0, 0);
        lcd.print(" Setting Jadwal 1 ");
        lcd.setCursor(5, 1);
        lcd.print(Waktu);
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          client.publish(schedule1, jsonJam1);
          state = 1;
        } else if (digitalRead(bUP) == tekan) {
          setSchedule1.AH1 = (setSchedule1.AH1 + 1) % 24;
          EEPROM.put(Address.addresAH1, setSchedule1.AH1);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setSchedule1.AH1 = (setSchedule1.AH1 + 23) % 24;
          EEPROM.put(Address.addresAH1, setSchedule1.AH1);
          EEPROM.commit();
        }
        break;

      case 1:
        setSchedule1.AH1 = EEPROM.read(Address.addresAH1);
        setSchedule1.AM1 = EEPROM.read(Address.addresAM1);
        docMenit1["Menit1"] = setSchedule1.AM1;
        serializeJson(docMenit1, jsonMenit1);
        Waktu = "  " + String(setSchedule1.AH1) + " :->" + String(setSchedule1.AM1) + "  ";
        lcd.setCursor(0, 0);
        lcd.print(" Setting Jadwal 1 ");
        lcd.setCursor(5, 1);
        lcd.print(Waktu);
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          lcd.setCursor(8, 1);
          lcd.print("Saved!");
          delay(1000);
          client.publish(schedule1, jsonMenit1);
          lcd.clear();
          state = 0;
          return menuUtama();
        } else if (digitalRead(bUP) == tekan) {
          setSchedule1.AM1 = (setSchedule1.AM1 + 1) % 60;
          EEPROM.put(Address.addresAM1, setSchedule1.AM1);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setSchedule1.AM1 = (setSchedule1.AM1 + 59) % 60;
          EEPROM.put(Address.addresAM1, setSchedule1.AM1);
          EEPROM.commit();
        }
        break;
    }
  }
}

void setJadwal2() {
  String Waktu;
  StaticJsonDocument<200> docJam2;
  StaticJsonDocument<200> docMenit2;
  char jsonJam2[20];
  char jsonMenit2[20];
  while (true) {
    switch (state) {
      case 0:
        setSchedule1.AH2 = EEPROM.read(Address.addresAH2);
        setSchedule1.AM2 = EEPROM.read(Address.addresAM2);
        docJam2["Jam2"] = setSchedule1.AH2;
        serializeJson(docJam2, jsonJam2);
        Waktu = "->" + String(setSchedule1.AH2) + " : " + String(setSchedule1.AM2) + "  ";
        lcd.setCursor(0, 0);
        lcd.print(" Setting Jadwal 2 ");
        lcd.setCursor(5, 1);
        lcd.print(Waktu);
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          client.publish(schedule2, jsonJam2);
          state = 1;
        } else if (digitalRead(bUP) == tekan) {
          setSchedule1.AH2 = (setSchedule1.AH2 + 1) % 24;
          EEPROM.put(Address.addresAH2, setSchedule1.AH2);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setSchedule1.AH2 = (setSchedule1.AH2 + 23) % 24;
          EEPROM.put(Address.addresAH2, setSchedule1.AH2);
          EEPROM.commit();
        }
        break;

      case 1:
        setSchedule1.AH2 = EEPROM.read(Address.addresAH2);
        setSchedule1.AM2 = EEPROM.read(Address.addresAM2);
        docMenit2["Menit2"] = setSchedule1.AM2;
        serializeJson(docMenit2, jsonMenit2);
        Waktu = "  " + String(setSchedule1.AH2) + " :->" + String(setSchedule1.AM2) + "  ";
        lcd.setCursor(0, 0);
        lcd.print(" Setting Jadwal 2 ");
        lcd.setCursor(5, 1);
        lcd.print(Waktu);
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          lcd.setCursor(8, 1);
          lcd.print("Saved!");
          delay(1000);
          client.publish(schedule2, jsonMenit2);
          lcd.clear();
          state = 0;
          return menuUtama();
        } else if (digitalRead(bUP) == tekan) {
          setSchedule1.AM2 = (setSchedule1.AM2 + 1) % 60;
          EEPROM.put(Address.addresAM2, setSchedule1.AM2);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setSchedule1.AM2 = (setSchedule1.AM2 + 59) % 60;
          EEPROM.put(Address.addresAM2, setSchedule1.AM2);
          EEPROM.commit();
        }
        break;
    }
  }
}

void setJadwal3() {
  StaticJsonDocument<200> docJam3;
  StaticJsonDocument<200> docMenit3;
  char jsonJam3[20];
  char jsonMenit3[20];
  String Waktu;
  while (true) {
    switch (state) {
      case 0:
        setSchedule1.AH3 = EEPROM.read(Address.addresAH3);
        setSchedule1.AM3 = EEPROM.read(Address.addresAM3);
        docJam3["Jam3"] = setSchedule1.AH3;
        serializeJson(docJam3, jsonJam3);
        Waktu = "->" + String(setSchedule1.AH3) + " : " + String(setSchedule1.AM3) + "  ";
        lcd.setCursor(0, 0);
        lcd.print(" Setting Jadwal 3 ");
        lcd.setCursor(5, 1);
        lcd.print(Waktu);
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          client.publish(schedule3, jsonJam3);
          state = 1;
        } else if (digitalRead(bUP) == tekan) {
          setSchedule1.AH3 = (setSchedule1.AH3 + 1) % 24;
          EEPROM.put(Address.addresAH3, setSchedule1.AH3);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setSchedule1.AH3 = (setSchedule1.AH3 + 23) % 24;
          EEPROM.put(Address.addresAH3, setSchedule1.AH3);
          EEPROM.commit();
        }
        break;

      case 1:
        setSchedule1.AH3 = EEPROM.read(Address.addresAH3);
        setSchedule1.AM3 = EEPROM.read(Address.addresAM3);
        docMenit3["Menit3"] = setSchedule1.AM3;
        serializeJson(docMenit3, jsonMenit3);
        Waktu = "  " + String(setSchedule1.AH3) + " :->" + String(setSchedule1.AM3) + "  ";
        lcd.setCursor(0, 0);
        lcd.print(" Setting Jadwal 3 ");
        lcd.setCursor(5, 1);
        lcd.print(Waktu);
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          lcd.setCursor(8, 1);
          lcd.print("Saved!");
          delay(1000);
          lcd.clear();
          client.publish(schedule3, jsonMenit3);
          state = 0;
          return menuUtama();
        } else if (digitalRead(bUP) == tekan) {
          setSchedule1.AM3 = (setSchedule1.AM3 + 1) % 60;
          EEPROM.put(Address.addresAM3, setSchedule1.AM3);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setSchedule1.AM3 = (setSchedule1.AM3 + 59) % 60;
          EEPROM.put(Address.addresAM3, setSchedule1.AM3);
          EEPROM.commit();
        }
        break;
    }
  }
}

void Time() {
  String Timers;
  StaticJsonDocument<200> docMin;
  StaticJsonDocument<200> docSec;
  char jsonMin[20];
  char jsonSec[20];
  while (true) {
    switch (state) {
      case 0:
        setTimer.Min = EEPROM.read(Address.addresMin);
        setTimer.Sec = EEPROM.read(Address.addresSec);
        docSec["Detik"] = setTimer.Sec;
        serializeJson(docSec, jsonSec);
        Timers = "  " + String(setTimer.Min) + " :->" + String(setTimer.Sec) + "  ";
        lcd.setCursor(0, 0);
        lcd.print(" Setting Timer ");
        lcd.setCursor(5, 1);
        lcd.print(Timers);
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          client.publish(setTime, jsonSec);
          state = 1;
        } else if (digitalRead(bUP) == tekan) {
          setTimer.Sec = (setTimer.Sec + 1) % 60;
          EEPROM.put(Address.addresSec, setTimer.Sec);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setTimer.Sec = (setTimer.Sec + 59) % 60;
          EEPROM.put(Address.addresSec, setTimer.Sec);
          EEPROM.commit();
        }
        break;

      case 1:
        setTimer.Min = EEPROM.read(Address.addresMin);
        setTimer.Sec = EEPROM.read(Address.addresSec);
        docMin["Menit"] = setTimer.Min;
        serializeJson(docMin, jsonMin);
        Timers = "->" + String(setTimer.Min) + " : " + String(setTimer.Sec) + "  ";
        lcd.setCursor(0, 0);
        lcd.print(" Setting Timer ");
        lcd.setCursor(5, 1);
        lcd.print(Timers);
        if (digitalRead(bOK) == tekan) {
          delay(tahan);
          while (digitalRead(bOK) == tekan) {}
          lcd.clear();
          lcd.setCursor(8, 1);
          lcd.print("Saved!");
          delay(1000);
          client.publish(setTime, jsonMin);
          lcd.clear();
          state = 0;
          return menuUtama();
        } else if (digitalRead(bUP) == tekan) {
          setTimer.Min = (setTimer.Min + 1) % 60;
          EEPROM.put(Address.addresMin, setTimer.Min);
          EEPROM.commit();
        } else if (digitalRead(bDN) == tekan) {
          setTimer.Min = (setTimer.Min + 59) % 60;
          EEPROM.put(Address.addresMin, setTimer.Min);
          EEPROM.commit();
        }
        break;
    }
  }
}