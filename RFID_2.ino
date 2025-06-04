#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 21   // SDA
#define RST_PIN 22  // RST

MFRC522 mfrc522(SS_PIN, RST_PIN);
String cardUID = ""; // Variable to store card UID as string
String Pemilik_KTP = "0580bfe798d100";
String pemilikTagCustom = "c3ea2dff";
String pemilikTagBiasa = "7070be58";

void setup() {
  Serial.begin(115200);
  SPI.begin();
  mfrc522.PCD_Init();
  
  // Cek koneksi MFRC522 dengan membaca versi firmware
  byte version = mfrc522.PCD_ReadRegister(mfrc522.VersionReg);
  Serial.print("Versi firmware MFRC522: 0x");
  Serial.println(version, HEX);

  if (version == 0x00 || version == 0xFF) {
    Serial.println("❌ Gagal mendeteksi modul MFRC522. Cek wiring dan power!");
    while (true); // Stop
  } else {
    Serial.println("✅ Modul MFRC522 terdeteksi dan siap digunakan.");
  }

  Serial.println("Scan kartu RFID");
}

void loop() {
  if (!mfrc522.PICC_IsNewCardPresent()) {
    delay(500);
    return;
  }

  if (!mfrc522.PICC_ReadCardSerial()) {
    Serial.println("Kartu terdeteksi tapi tidak bisa dibaca.");
    delay(500);
    return;
  }

  // Reset cardUID string
  cardUID = "";
  
  // Convert UID to string
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    if (mfrc522.uid.uidByte[i] < 0x10) {
      cardUID += "0";
    }
    cardUID += String(mfrc522.uid.uidByte[i], HEX);
  }

  Serial.print("UID Kartu: ");
  Serial.println(cardUID);

  if (cardUID == Pemilik_KTP) {
    Serial.println("Ini adalah KTP");
  }
  else if (cardUID == pemilikTagCustom) {
    Serial.println("Ini adalah Tag Custom");
  }
  MFRC522::PICC_Type piccType = mfrc522.PICC_GetType(mfrc522.uid.sak);
  Serial.print("PICC Type: ");
  Serial.println(mfrc522.PICC_GetTypeName(piccType));

  mfrc522.PICC_HaltA();
  delay(2000);
}