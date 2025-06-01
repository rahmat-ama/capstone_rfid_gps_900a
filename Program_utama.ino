#define TINY_GSM_MODEM_SIM900

#define BLYNK_PRINT Serial
#define BLYNK_TEMPLATE_ID "TMPL6RyvsyY1I"
#define BLYNK_TEMPLATE_NAME "Temperatur"
#define BLYNK_AUTH_TOKEN "bz5aWu9h04Xh3oIZBGUPWbLhO4Ls5eW1"

#include <TinyGsmClient.h>
#include <BlynkSimpleTinyGSM.h>
#include <TinyGPS++.h>
#include <HardwareSerial.h>

// Konfigurasi modem
char apn[] = "3data";
char user[] = "";
char pass[] = "";

// Pin dan Serial
#define GPS_RX 16
#define GPS_TX 17
#define SIM900_RX_PIN 27
#define SIM900_TX_PIN 26
#define LED_PIN 2

HardwareSerial sim900(1);
TinyGsm modem(sim900);
TinyGPSPlus gps;
BlynkTimer timer;

int pinV1State = 0;

// GPS Shared Variable
float currentLat = 0.0;
float currentLng = 0.0;
bool gpsAvailable = false;

// Task Handle
TaskHandle_t TaskBlynkHandle = NULL;
TaskHandle_t TaskTelegramHandle = NULL;

// Fungsi bantu
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
  } else {
    digitalWrite(LED_PIN, LOW);
  }
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
    vTaskDelay(pdMS_TO_TICKS(10000));  // Kirim setiap 10 detik
  }
}

// ------------------ SETUP ------------------
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

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

  xTaskCreatePinnedToCore(TaskTelegram, "TelegramTask", 4096, NULL, 1, &TaskTelegramHandle, 1);

  // Inisialisasi Blynk
  Blynk.begin(BLYNK_AUTH_TOKEN, modem, apn, user, pass);
  timer.setInterval(2000L, checkPinV1State);
  timer.setInterval(5000L, myTimerEvent);

  // Task GPS di loop utama
  xTaskCreatePinnedToCore(TaskBlynk, "BlynkTask", 4096, NULL, 1, &TaskBlynkHandle, 1);
}

void loop() {
  readGPSData();  // baca GPS di loop utama (low priority)
  vTaskDelay(pdMS_TO_TICKS(500));
}
