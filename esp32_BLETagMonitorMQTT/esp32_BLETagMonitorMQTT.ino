#include <esp_system.h>
#include <rom/rtc.h>
#include <EEPROM.h>

#include <WiFi.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <PubSubClient.h>

struct BLEADD
{
  uint8_t a[6];
};


// configuration
/**
#define WLAN_SSID  <WLAN ssid>
#define WLAN_PSWD  <WLAN password>
#define MQTT_HOST  <MQTT host address>
#define MQTT_PORT  <MQTT port number>
#define MQTT_TOPIC <MQTT topic name>

#define MONITOR_NAME "m01"
#define SCAN_TIME      10
#define SLEEP_TIME_MIN 10
#define SLEEP_TIME_MAX 30

#define EEPROM_SIZE 512
#define MAX_TAGS 16
**/
#include "config.h"

#define DEBUG


int nTags;
BLEADD tagIds[MAX_TAGS];

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

bool doScanBLETag();
bool doMqttPublish();

void softReset();
void deepSleep();
String convAddress(BLEAddress address);


void setup()
{
  delay(500);

#ifdef DEBUG
  // デバッグ用にシリアルを開く
  Serial.begin(115200);
  delay(500);
  Serial.println("[start setup()]");
#endif

  // WiFiをOFF
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);

  // BLEタグをスキャン
  if (!doScanBLETag())
  {
    softReset();
  }

  // スキャン結果を送信
  if (!doMqttPublish())
  {
    softReset();
  }

  // deep sleep
  deepSleep();
}


void loop()
{
}


bool doScanBLETag()
{
#ifdef DEBUG
  Serial.println("[start doScanBLETag()]");
  delay(100);
#endif

  // BLEスキャン
  //esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
  BLEDevice::init("");
  BLEScan* pBLEScan = BLEDevice::getScan();
  pBLEScan->setActiveScan(false);
#ifdef DEBUG
  Serial.println("BLE Scanning...");
  delay(100);
#endif
  BLEScanResults scanResults = pBLEScan->start(SCAN_TIME);
  nTags = scanResults.getCount();
#ifdef DEBUG
  Serial.print("BLE Scan done! (");
  Serial.print(nTags);
  Serial.println(" devices found)");
  delay(100);
#endif
  if (nTags > MAX_TAGS)
  {
    nTags = MAX_TAGS;
  }

  // スキャン結果をグローバル変数に格納
  if (nTags > 0)
  {
    for (int i=0; i<nTags; i++)
    {
      unsigned char* addr = *scanResults.getDevice(i).getAddress().getNative();
      for (int j=0; j<6; j++)
      {
        tagIds[i].a[j] = addr[j];
      }
    }
  }

  // BLE切断
  BLEDevice::deinit(true);
  delay(100);

  return true;
}


bool doMqttPublish()
{
#ifdef DEBUG
  Serial.println("[start doSendResult()]");
  delay(100);
#endif

  // グローバル変数からスキャン結果を読取
  if (nTags > MAX_TAGS)
  {
    nTags = MAX_TAGS;
  }
  else if (nTags < 0)
  {
    nTags = 0;
  }
  String data = "{\"monitor_name\":\"" + String(MONITOR_NAME) + "\",\"tag_detected\":[";
  BLEADD address;
  if (nTags > 0)
  {
    for (uint8_t i=0; i<nTags; i++)
    {
      data += "\"" + convAddress(tagIds[i]) + "\",";
    }
    data = data.substring(0, data.length()-1);
  }
  data += "]}";
#ifdef DEBUG
  Serial.print("data = ");
  Serial.println(data);
  delay(100);
#endif

  // WiFi接続
  WiFi.mode(WIFI_STA);
  //WiFi.setAutoConnect(false);
  //WiFi.setAutoReconnect(false);
#ifdef DEBUG
  Serial.print("Try to connect to SSID: ");
  Serial.println(WLAN_SSID);
#endif
  int counter = 0;
  WiFi.begin(WLAN_SSID, WLAN_PSWD);
  while (WiFi.status() != WL_CONNECTED) {
#ifdef DEBUG
    Serial.print(".");
#endif
    delay(1000);
    counter++;
    if (counter > 10)
    {
#ifdef DEBUG
      Serial.println("\n(wifiConnect) failed!");
      delay(100);
#endif
      return false;
    }
  }
#ifdef DEBUG
  Serial.print(" Connected to ");
  Serial.print(WLAN_SSID);
  Serial.print(" (IP = ");
  Serial.print(WiFi.localIP());
  Serial.println(")");
#endif

  // MQTT接続
#ifdef DEBUG
  Serial.print("Try to connect to MQTT broker: ");
  Serial.print(MQTT_HOST);
  Serial.print(":");
  Serial.println(MQTT_PORT);
#endif
  counter = 0;
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  while ( !mqttClient.connected() ) {
#ifdef DEBUG
    Serial.print(".");
#endif
    String clientId = String(MONITOR_NAME);
    if ( mqttClient.connect(clientId.c_str()) ) {
#ifdef DEBUG
      Serial.print(" Connected by ");
      Serial.println(clientId);
#endif
      break;
    }
    counter++;
    if (counter > 10)
    {
#ifdef DEBUG
      Serial.println("\n(mqttConnect) failed!");
      delay(100);
#endif
      return false;
    }
  }

  // MQTTブローカーへpublish
  if ( mqttClient.publish(MQTT_TOPIC, data.c_str()) )
  {
#ifdef DEBUG
    Serial.println("MQTT publish succeeded!");
#endif
  }
  else
  {
#ifdef DEBUG
    Serial.println("MQTT publish failed!");
#endif
  }
  delay(100);

  // MQTT切断
  if (mqttClient.connected())
  {
    mqttClient.disconnect();
  }
  delay(100);

  // HTTP切断
  if (wifiClient.connected())
  {
    wifiClient.stop();
  }
  delay(100);

  // WiFi切断
  WiFi.disconnect(true);
  delay(100);

  return true;
}


// software reset
void softReset()
{
#ifdef DEBUG
  Serial.println("Going to software reset now...");
  delay(100);
#endif
  //esp_restart();
  ESP.restart();
}


// deep sleep
void deepSleep()
{
  randomSeed(micros());
  long sleepTime = random(SLEEP_TIME_MIN, SLEEP_TIME_MAX);
#ifdef DEBUG
  Serial.print("Going to deep sleep (");
  Serial.print(sleepTime);
  Serial.print("sec) now...");
  delay(100);
#endif
  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_SLOW_MEM, ESP_PD_OPTION_OFF);
  //esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_FAST_MEM, ESP_PD_OPTION_OFF);
  //esp_sleep_pd_config(ESP_PD_DOMAIN_MAX, ESP_PD_OPTION_OFF);
  esp_sleep_enable_timer_wakeup(sleepTime * 1000 * 1000);
  esp_deep_sleep_start();
}


// BLEタグMACアドレスを書式変換
String convAddress(BLEADD address)
{
  String out = "";
  for (int i=0; i<6; i++)
  {
    String tmp = String(address.a[i], HEX);
    if(tmp.length() == 1) tmp = "0" + tmp;
    out += tmp + ":";
  }
  out = out.substring(0, out.length()-1);
  out.toUpperCase();
  return out;
}

