#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>

// ================= CONFIGURAÇÕES DE WI-FI E BROKER MQTT =================
const char* SSID        = "FIAP_IOT2";
const char* PASSWORD    = "F!@p25.IOT";
const char* BROKER_MQTT = "3.135.234.101";  
const int   BROKER_PORT = 1883;

// ================= TÓPICOS MQTT (FIWARE ULTRALIGHT 2.0 / MYMQTT) =================
const char* TOPICO_SUBSCRIBE    = "/TEF/wineCellar001/cmd";
const char* TOPICO_PUBLISH_ATTR = "/TEF/wineCellar001/attrs";
const char* TOPICO_PUBLISH_TEMP = "/TEF/wineCellar001/attrs/t";
const char* TOPICO_PUBLISH_HUM  = "/TEF/wineCellar001/attrs/h";
const char* TOPICO_PUBLISH_LUM  = "/TEF/wineCellar001/attrs/l";
const char* ID_MQTT             = "fiware_wineCellar001"; // Se der conflito no servidor, altere para 002, 003, etc.

// ================= DEFINIÇÃO DOS PINOS DO HARDWARE =================
#define PIN_DHT 4     // GPIO 4 (D4) -> Barramento DATA do DHT11
#define DHTTYPE DHT11 // Modelo do sensor: DHT11
#define PIN_LDR 34    // GPIO 34 (D34 / ADC1_CH6) -> Sensor LDR
#define PIN_LED 2     // GPIO 2 (D2) -> Atuador LED1

DHT dht(PIN_DHT, DHTTYPE);
WiFiClient espClient;
PubSubClient MQTT(espClient);

unsigned long ultimoEnvio = 0;
const unsigned long INTERVALO_ENVIO = 5000; // Intervalo entre leituras (5 segundos)

void initWiFi() {
  delay(10);
  Serial.println("====== [Wi-Fi] Inicializando Conexao ======");
  Serial.print("Conectando-se a rede: ");
  Serial.println(SSID);

  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\n[Wi-Fi] Conectado com sucesso!");
  Serial.print("[Wi-Fi] Endereco IP: ");
  Serial.println(WiFi.localIP());
}

// CORRIGIDO: char* topic
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) {
    msg += (char)payload[i];
  }

  Serial.print("[MQTT] Comando recebido em [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(msg);

  // Acionamento do LED1 via MyMQTT ("on" / "off") ou comando NGSIv2 do FIWARE
  if (msg.equals("wineCellar001@on|") || msg.equalsIgnoreCase("on") || msg.equals("1")) {
    digitalWrite(PIN_LED, HIGH);
    MQTT.publish(TOPICO_PUBLISH_ATTR, "s|on");
    Serial.println("[ATUADOR] LED1 -> LIGADO (HIGH)");
  } else if (msg.equals("wineCellar001@off|") || msg.equalsIgnoreCase("off") || msg.equals("0")) {
    digitalWrite(PIN_LED, LOW);
    MQTT.publish(TOPICO_PUBLISH_ATTR, "s|off");
    Serial.println("[ATUADOR] LED1 -> DESLIGADO (LOW)");
  }
}

void initMQTT() {
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setCallback(mqttCallback);
}

void reconnectMQTT() {
  while (!MQTT.connected()) {
    Serial.print("[MQTT] Conectando ao Broker (");
    Serial.print(BROKER_MQTT);
    Serial.println(")...");

    if (MQTT.connect(ID_MQTT)) {
      Serial.println("[MQTT] Conectado com sucesso!");
      MQTT.subscribe(TOPICO_SUBSCRIBE);
      Serial.print("[MQTT] Inscrito no topico: ");
      Serial.println(TOPICO_SUBSCRIBE);
    } else {
      Serial.print("[MQTT] Falha na conexao. Codigo: ");
      Serial.print(MQTT.state());
      Serial.println(" | Tentando novamente em 2s...");
      delay(2000);
    }
  }
}

void verificaConexoes() {
  if (WiFi.status() != WL_CONNECTED) {
    initWiFi();
  }
  if (!MQTT.connected()) {
    reconnectMQTT();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  dht.begin();
  initWiFi();
  initMQTT();
}

void loop() {
  verificaConexoes();
  MQTT.loop();

  unsigned long agora = millis();
  if (agora - ultimoEnvio >= INTERVALO_ENVIO) {
    ultimoEnvio = agora;

    // 1. Captura de Temperatura (°C) e Umidade (%) pelo DHT11
    float temperatura = dht.readTemperature();
    float umidade     = dht.readHumidity();

    // 2. Captura de Luminosidade (0 a 100%) pelo LDR
    int leituraADC   = analogRead(PIN_LDR);
    int luminosidade = map(leituraADC, 0, 4095, 0, 100);

    if (isnan(temperatura) || isnan(umidade)) {
      Serial.println("[ERRO] Falha ao ler o sensor DHT11! Verifique os cabos.");
      return;
    }

    // Formatacao dos payloads
    char strTemp[10], strHum[10], strLum[10], payloadUltraLight[64];
    dtostrf(temperatura, 4, 1, strTemp);
    dtostrf(umidade, 4, 1, strHum);
    snprintf(strLum, sizeof(strLum), "%d", luminosidade);
    snprintf(payloadUltraLight, sizeof(payloadUltraLight), "t|%s|h|%s|l|%s", strTemp, strHum, strLum);

    // Publicacao nos topicos individuais (MyMQTT) e no topico unificado (FIWARE IoT Agent)
    MQTT.publish(TOPICO_PUBLISH_TEMP, strTemp);
    MQTT.publish(TOPICO_PUBLISH_HUM,  strHum);
    MQTT.publish(TOPICO_PUBLISH_LUM,  strLum);
    MQTT.publish(TOPICO_PUBLISH_ATTR, payloadUltraLight);

    // Log no Serial Monitor
    Serial.println("--------------------------------------------------");
    Serial.print("🌡️ Temperatura : "); Serial.print(strTemp); Serial.println(" °C");
    Serial.print("💧 Umidade     : "); Serial.print(strHum);  Serial.println(" %");
    Serial.print("💡 Luminosidade: "); Serial.print(strLum);  Serial.println(" %");
    Serial.print("📦 Payload UL2 : "); Serial.println(payloadUltraLight);
  }
}
 