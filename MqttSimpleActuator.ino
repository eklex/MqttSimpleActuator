#include "MqttSimpleActuator.h"

/**
 * Defines
 */
/* Enable probing power supply */
ADC_MODE(ADC_VCC);

/**
 * Constants
 */
/* Firmware version 
 * Note: Need to be updated to reflect release number */
const char* firmware_version = "mqtt-simple-actuator-rel0.9";
/* Deep sleep period */
const unsigned long sleep_period_sec = 60;
/* Actuator pin output */
const unsigned int actuator_pin = 5;
/* Delay before reconnecting to MQTT broker */
const unsigned long mqtt_reconnect_sec = 60;
/* Delay before checking OTA update */
const unsigned long ota_check_sec = 120;

/**
 * Global variables
 */
/* Reserved space for SPIFFS variables */
char wifi_ssid    [config_len]  = {0};
char wifi_key     [config_len]  = {0};
char wifi_hostname[config_len]  = {0};
char mqtt_broker  [config_len]  = {0};
char mqtt_topic   [topic_len]   = {0};
char mqtt_id      [topic_len*2] = {0};
char spiffs_ver   [spiffs_len]  = {0};

volatile bool actuator_trigger = false;
volatile bool actuator_status  = false;
static   long last_connection  = 0;
static   long last_ota_check   = 0;
static   int  fw_ota_status    = 0;
static   int  fs_ota_status    = 0;

void setup()
{
#ifdef DEBUG
  /**
   * Setup serial for debug
   */
  Serial.begin(115200);
#endif

  /**
   * Display firmware version
   */
  Debug("Firmware version: "); Debugln(firmware_version);
  
  /**
   * Setup pin mode
   */
  pinMode(actuator_pin, OUTPUT);
  digitalWrite(actuator_pin, false);
  
  /**
   * Retrieve WiFi and MQTT configurations
   * in file system
   */
  Debugln("\n---> Retrieve WiFi and MQTT configurations");
  if(retrieveConfig() != 0)
  {
    Debugln("Configurations failure!");
    delay(1000);
    ESP.reset();
  }
  
  /**
   * Connect to WiFi
   */
  Debugln("\n---> Connect to WiFi");
  if(wifiConnect() != 0)
  {
    Debugln("Connection failure!");
    delay(1000);
    ESP.reset();
  }

  /**
   * Initialize MQTT client
   */
  Debugln("\n---> Initialize MQTT client");
  mqttInit();

  /**
   * Update output according to previous status
   */
  if(actuator_trigger == true)
  {
    digitalWrite(actuator_pin, actuator_trigger);
  }
  
  /**
   * Process MQTT topics
   */
  Debugln("\n---> Process MQTT topics");
  mqttProcess(firmware_version);
  mqttProcess(actuator_status, ESP.getVcc());
  
#if 0
  /**
   * Enter in deep sleep mode to save power
   * Note: This is the last function call,
   *       on wake-up the uC is reset and
   *       restarts from the beginning.
   */
  Debugln("\n---> Go to Deep Sleep mode");
  ESP.deepSleep(sleep_period_sec * 1000000, RF_NO_CAL);
#endif

  Debugln("\n---> ESP setup routine completed.");
}

void loop()
{
  /**
   * Process MQTT messages
   */
  mqttLoop();
  
  /** 
   *  Check trigger agains status
   */
  if(actuator_trigger != actuator_status)
  {
    Debug("\n --> Actuator output changed to "); Debugln(actuator_trigger);
    digitalWrite(actuator_pin, actuator_trigger);
    actuator_status = actuator_trigger;
    mqttProcess(actuator_status, ESP.getVcc());
  }

  /**
   * Reconnect to MQTT broker and process data
   */
  if(millis() - last_connection > mqtt_reconnect_sec*1000)
  {
    last_connection = millis();
    mqttProcess(actuator_status, ESP.getVcc());
  }

  if((millis() - last_ota_check > ota_check_sec*1000) &&
     (actuator_status == false))
  {
    Debugln("\n---> Check firmware update");
    last_ota_check = millis();
    fs_ota_status = otaUpdate(mqtt_broker, spiffs_ver, FILESYSTEM);
    if(fs_ota_status == 1)
    {
      Debugln("\n---> File System OTA update done. Going to reset...");
      ESP.restart();
    }
    fw_ota_status = otaUpdate(mqtt_broker, firmware_version, FIRMWARE);
    if(fw_ota_status == 1)
    {
      Debugln("\n---> Firmware OTA update done. Going to reset...");
      ESP.restart();
    }
  }
}

