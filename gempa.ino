// Hasil modifikasi dari percakapan di group tg:Arduino club indonesia

#include <HTTPClient.h>
#include <UniversalTelegramBot.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// Inisialisasi constant
constexpr char *WIFISSID = "Wokwi-GUEST";
constexpr char *WIFIPASS = "";
constexpr char *BOTTOKEN = ""; // Token Bot Anda
constexpr char *CHAT_ID = "";                                       // ID Chat Anda
constexpr char *BMKG_URL = "https://data.bmkg.go.id/DataMKG/TEWS/autogempa.json";
constexpr float GEMPA_THRESHOLD = 4.0; // Ambang batas magnitudo untuk notifikasi
constexpr int RELAYPIN = 6;                // Pin untuk relay

WiFiClientSecure *tg_client;
WiFiClientSecure *https_client;
UniversalTelegramBot *bot;

String lastGempaTime = "";
String lastGempaLocation = "";

void setup()
{
  Serial.begin(115200);
  configTime(60 * 60 * 7, 0, "pool.ntp.org"); // Mendapatkan waktu UTC melalui NTP

  tg_client = new WiFiClientSecure{};
  https_client = new WiFiClientSecure{};

  tg_client->setCACert(TELEGRAM_CERTIFICATE_ROOT);

  bot = new UniversalTelegramBot(BOTTOKEN, *tg_client);

  Serial.print("Connecting to WiFi...\n\rLoading");
  WiFi.begin(WIFISSID, WIFIPASS, 6);

  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\n\rConnected to WiFi");

  pinMode(RELAYPIN, OUTPUT);    // Siapkan pin relay
  digitalWrite(RELAYPIN, HIGH); // Pastikan relay mati di awal

  const auto status = bot->sendMessage(CHAT_ID, "Bot started up");
  Serial.printf("status bot: %d\n\r", status);
}

void loop()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    https_client->setInsecure(); // Mengabaikan sertifikat SSL (hanya untuk testing, tidak disarankan untuk produksi)

    HTTPClient http;
    http.begin(*https_client, BMKG_URL); // Gunakan WiFiClientSecure sebagai parameter
    int httpCode = http.GET();

    if (httpCode > 0)
    {
      const String payload = http.getString();

      // Cetak respons API untuk memastikan data yang diterima
      Serial.println("Payload dari API:");
      Serial.println(payload);

      JsonDocument doc{};
      auto error = deserializeJson(doc, payload);

      if (error)
      {
        Serial.print("DeserializationError: ");
        Serial.println(error.c_str());
        return;
      }

      // Ambil data gempa terkini dari JSON
      const String tanggal = doc["Infogempa"]["gempa"]["Tanggal"].as<String>();
      const String jam = doc["Infogempa"]["gempa"]["Jam"].as<String>();
      const String magnitude = doc["Infogempa"]["gempa"]["Magnitude"].as<String>();
      const String lokasi = doc["Infogempa"]["gempa"]["Wilayah"].as<String>();
      const String kedalaman = doc["Infogempa"]["gempa"]["Kedalaman"].as<String>();
      const String potensi = doc["Infogempa"]["gempa"]["Potensi"].as<String>();

      // Cek apakah gempa ini baru
      const String waktuGempa = tanggal + " " + jam;
      if (waktuGempa != lastGempaTime || lokasi != lastGempaLocation)
      {
        lastGempaTime = waktuGempa;
        lastGempaLocation = lokasi;

        char message[350]{};
        sprintf(message,
                "=== Informasi Gempa Baru ===\n\r"
                "Waktu: %s\n\r"
                "Magnitudo: %s\n\r"
                "Lokasi: %s\n\r"
                "Kedalaman: %s\n\r"
                "Potensi: %s\n\r",
                waktuGempa.c_str(), magnitude.c_str(), lokasi.c_str(), kedalaman.c_str(), potensi.c_str());
        Serial.print(message);
        // Kirim notifikasi ke Telegram
        const auto status = bot->sendMessage(CHAT_ID, message);
        Serial.printf("status pesan bot: %d\n\r", status);

        //  Cek apakah magnitudo gempa melebihi ambang batas
        if (magnitude.toFloat() >= GEMPA_THRESHOLD)
        {
          Serial.println("** Magnitudo tinggi! Mengaktifkan relay...");
          digitalWrite(RELAYPIN, LOW);  // Nyalakan relay sebagai notifikasi
          delay(1000);                  // Biarkan relay menyala selama 10 detik
          digitalWrite(RELAYPIN, HIGH); // Matikan relay
        }
      }
      else
      {
        Serial.println("Tidak ada gempa baru.");
      }
    }
    else
    {
      Serial.print("Error on HTTP request: ");
      Serial.println(httpCode);
    }
    http.end();
  }
  delay(5000); // Tunggu selama 1 menit sebelum request berikutnya
}
