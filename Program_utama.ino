#define TINY_GSM_MODEM_SIM900

#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6RyvsyY1I"
#define BLYNK_TEMPLATE_NAME "Capstone Project RFID"
#define BLYNK_AUTH_TOKEN "bz5aWu9h04Xh3oIZBGUPWbLhO4Ls5eW1"

#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>
#include <SPI.h>
#include <MFRC522.h>

// Konfigurasi modem
char apn[] = "3data";
char user[] = "";
char pass[] = "";

// Pin dan Serial
#define GPS_RX 16
#define GPS_TX 17
#define SIM900_RX_PIN 27
#define SIM900_TX_PIN 26
#define MFRC_SS 21 // SDA
#define MFRC_RST 22
#define LED_PIN 2
#define RELAY_PIN 13

// Deepsleep
#define WAKEUP_PIN 14
#define WAKEUP_LEVEL 1

HardwareSerial sim900(1);
TinyGsm modem(sim900);
TinyGPSPlus gps;
BlynkTimer timer;

int pinV1State = 0;
bool kondisiKemalingan = false;

// GPS Shared Variable
float currentLat = 0.0;
float currentLng = 0.0;
bool gpsAvailable = false;

// MFRC dan Tag
MFRC522 mfrc522(MFRC_SS, MFRC_RST);
String cardUID = "";
String Pemilik_KTP_1 = "0580bfe798d100";
String Pemilik_KTP_2 = "05810bf0dc9100";
String pemilikTagCustom = "c3ea2dff";
String pemilikTagBiasa = "7070be58";
unsigned long timeout = 10000;       
unsigned long startTime;

// Task Handle
TaskHandle_t TaskBlynkHandle = NULL;
TaskHandle_t TaskTelegramHandle = NULL;
TaskHandle_t TaskRfidHandle = NULL;

void goToDeepSleep() {
  esp_sleep_enable_ext0_wakeup(GPIO_NUM_14, WAKEUP_LEVEL);
  Serial.println("Masuk ke deep sleep...");
  Serial.flush();
  esp_deep_sleep_start();
}

void print_wakeup_reason() {
  esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();
  Serial.print("Penyebab Wakeup: ");
  switch (wakeup_reason) {
    case ESP_SLEEP_WAKEUP_EXT0: Serial.println("Sinyal eksternal dari GPIO14 HIGH"); break;
    default: Serial.printf("Bukan dari deep sleep: %d\n", wakeup_reason); break;
  }
}

// Fungsi bantu membaca data GPS
void readGPSData() {
  while (Serial2.available() > 0) {
    if (gps.encode(Serial2.read())) {
      if (gps.location.isValid()) {
        currentLat = gps.location.lat();
        currentLng = gps.location.lng();
        gpsAvailable = true;
      } else {
        gpsAvailable = false;
      }
    }
  }
}

void myTimerEvent() {
  Blynk.virtualWrite(V0, String(currentLat, 6));
  Blynk.virtualWrite(V2, String(currentLng, 6));
}

BLYNK_WRITE(V1) {
  pinV1State = param.asInt();
}

void checkPinV1State() {
  if (pinV1State) {
    digitalWrite(LED_PIN, HIGH);
    digitalWrite(RELAY_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, LOW);
    digitalWrite(RELAY_PIN, HIGH);
  }
}

bool sendCommandWaitResponse(const String& command, const String& expectedResponse, unsigned long timeout = 5000) {
  sim900.println(command);
  unsigned long startTime = millis();
  String response = "";

  while (millis() - startTime < timeout) {
    while (sim900.available()) {
      char c = sim900.read();
      response += c;
    }

    if (response.indexOf(expectedResponse) != -1) {
      return true;
    }
  }

  Serial.println("Response timeout or failed: " + response);
  return false;
}

void sendCommand(const String& command, unsigned long delayTime = 250) {
  sim900.println(command);
  delay(delayTime);
}

// ------------------ TASK 1: Blynk ------------------
void TaskBlynk(void* parameter) {
  while (1) {
    Blynk.run();
    timer.run();
    vTaskDelay(pdMS_TO_TICKS(1000));  // minimal delay untuk task
  }
}

// ------------------ TASK 2: Telegram via CallMeBot ------------------
void TaskTelegram(void* parameter) {
  Serial.println("Memulai CallMeBot");
  while (1) {
    Serial.println("CallMeBot : True");
    if (gpsAvailable) {
      Serial.println("Memulai AT Command");
      sendCommand("AT+SAPBR=3,1,\"Contype\",\"GPRS\"");
      sendCommand("AT+CSTT=\"3data\"");
      sendCommand("AT+SAPBR=1,1");
      sendCommand("AT+SAPBR=2,1");
      sendCommand("AT+HTTPINIT");
      sendCommand("AT+HTTPPARA=\"CID\",\"1\"");

      String url = "AT+HTTPPARA=\"URL\",\"http://api.callmebot.com/text.php?user=@rahmatamalul&text=Location+:+https%3A%2F%2Fwww.google.com%2Fmaps%2Fsearch%2F%3Fapi%3D1%26query%3D" 
                   + String(currentLat, 6) + "%2C" + String(currentLng, 6) + "\"";
                   
      sendCommand(url);
      sendCommand("AT+HTTPACTION=0");
      sendCommand("AT+HTTPREAD");
      sendCommand("AT+HTTPTERM");
      Serial.println("Pesan Telegram Dikirim");
    }
    if (kondisiKemalingan) {
      vTaskDelay(pdMS_TO_TICKS(10000)); 
    }
    else {
      vTaskDelay(pdMS_TO_TICKS(200000));
    }
  }
}

void TaskRfid(void* parameter) {
  while (true) {
    cardUID = "";
    if (mfrc522.PICC_IsNewCardPresent() && mfrc522.PICC_ReadCardSerial()) {
      for (byte i = 0; i < mfrc522.uid.size; i++) {
        if (mfrc522.uid.uidByte[i] < 0x10) cardUID += "0";
        cardUID += String(mfrc522.uid.uidByte[i], HEX);
      }
      Serial.print("🔍 UID terbaca: ");
      Serial.println(cardUID);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
  }
}

void stateNonKemalingan() {
  // Inisialisasi Blynk
  timer.setInterval(5000L, checkPinV1State);
  timer.setInterval(300000L, myTimerEvent);

  // Task GPS di loop utama
  xTaskCreatePinnedToCore(TaskTelegram, "TelegramTask", 4096, NULL, 1, &TaskTelegramHandle, 1);
  xTaskCreatePinnedToCore(TaskBlynk, "BlynkTask", 4096, NULL, 1, &TaskBlynkHandle, 1);
}

void stateKemalingan() {
  kondisiKemalingan = true;
  // Inisialisasi Blynk
  timer.setInterval(5000L, checkPinV1State);
  timer.setInterval(30000L, myTimerEvent);

  // Task GPS di loop utama
  xTaskCreatePinnedToCore(TaskTelegram, "TelegramTask", 4096, NULL, 1, &TaskTelegramHandle, 1);
  xTaskCreatePinnedToCore(TaskBlynk, "BlynkTask", 4096, NULL, 1, &TaskBlynkHandle, 1);
}

// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);

  pinMode(WAKEUP_PIN, INPUT);
  print_wakeup_reason();

  // Jika pin masih LOW saat boot (tidak ditekan), tidur lagi
  if (digitalRead(WAKEUP_PIN) == LOW) {
    Serial.println("GPIO14 LOW - tidak ada trigger, masuk deep sleep.");
    goToDeepSleep();
  }

  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_EXT0) {
    Serial.println("Bangun oleh GPIO14 HIGH. Memulai program utama");

    SPI.begin();
    mfrc522.PCD_Init();
    byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
    if (version == 0x00 || version == 0xFF) {
      Serial.println("❌ Modul RFID tidak terdeteksi.");
      while (1);
    }
    Serial.println("✅ Siap membaca kartu.");

    // Serial komunikasi
    sim900.begin(9600, SERIAL_8N1, SIM900_RX_PIN, SIM900_TX_PIN);
    Serial2.begin(9600, SERIAL_8N1, GPS_RX, GPS_TX);
    // Inisialisasi modem
    if (!modem.restart()) {
      Serial.println("Modem gagal restart!");
      while (true);
    }
    if (!modem.waitForNetwork()) {
      Serial.println("Tidak ada jaringan!");
      while (true);
    }
    if (!modem.gprsConnect(apn, user, pass)) {
      Serial.println("Gagal GPRS!");
      while (true);
    }

    Blynk.begin(BLYNK_AUTH_TOKEN, modem, apn, user, pass);

    xTaskCreatePinnedToCore(TaskRfid, "RfidTask", 4096, NULL, 1, &TaskRfidHandle, 1);
    while (cardUID == "") {
      vTaskDelay(pdMS_TO_TICKS(500));
      Serial.println(".");
    }
    if (cardUID == Pemilik_KTP_1 || cardUID == Pemilik_KTP_2 ) {
      Serial.println("Ini adalah KTP");
      Serial.println("Memulai program Non Kemalingan");
      stateNonKemalingan();
    }
    else if (cardUID == pemilikTagCustom || cardUID == pemilikTagBiasa) {
      Serial.println("Ini adalah Tag Custom atau Biasa");
      Serial.println("Memulai program Kemalingan");
      stateKemalingan();
    }
    else {
      Serial.println("Kartu tidak dikenali");
      Serial.println("Memulai program Kemalingan");
      stateKemalingan();
    }
  } 
  else {
    Serial.println("Boot awal atau bukan wakeup EXT0. Tidur kembali.");
    goToDeepSleep();
  }

}

void loop() {
  if (digitalRead(WAKEUP_PIN) == LOW) {
    Serial.println("GPIO14 kembali LOW - masuk deep sleep.");
    goToDeepSleep();
  }

  readGPSData();  // baca GPS di loop utama (low priority)
  vTaskDelay(pdMS_TO_TICKS(500));
}
