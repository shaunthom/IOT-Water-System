#include "common.h"


#define WIFI_SSID         "Airfi"

// GPIO.
#define BAT_CHECK_PIN     A0
#define BAT_WARN_PIN      D1
#define WATER_LEVEL1_PIN  D6
#define WATER_LEVEL2_PIN  D7
#define LID_SENSOR_PIN    D4
#define PUMP_SWITCH_PIN   D3
#define PUMP_STATUS_PIN   D5
#define ERROR_STATUS_PIN  D2



// Time gap for update check and data sending.
#define UPDATE_DELAY      1000

// Time for error status display.
#define ERROR_DISPLAY_TIME  2000

// Trigger timers.
TimedTrigger dataUpdateTrigger(UPDATE_DELAY);
TimedTrigger errorDisplayTrigger(ERROR_DISPLAY_TIME);

// Switch.
ToggleSwitch pumpSwitch(PUMP_SWITCH_PIN);

// Data.
TankStatus statusData = { 0 };

////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);

  setupGPIO();
  setupWiFi();
  setupESPNow();
}

void loop()
{
  // Check switch status.
  checkSwitchStatus();

  // Error staus display.
  if (errorDisplayTrigger.isTimeOut())
  {
    showErrorStatus(false);
  }

  // Send the message if wait time is over.
  if (dataUpdateTrigger.isTimeOut())
  {
    updateStatus();
    dataUpdateTrigger.reset();
  }
}



void setupGPIO()
{
  pinMode(BAT_WARN_PIN, OUTPUT);
  pinMode(WATER_LEVEL1_PIN, INPUT_PULLUP);
  pinMode(WATER_LEVEL2_PIN, INPUT_PULLUP);
  pinMode(LID_SENSOR_PIN, INPUT_PULLUP);
  pinMode(PUMP_SWITCH_PIN, INPUT_PULLUP);
  pinMode(PUMP_STATUS_PIN, OUTPUT);
  pinMode(ERROR_STATUS_PIN, OUTPUT);
}

void setupWiFi()
{
  // Set WiFi mode as Station.
  Serial.println("WiFi init...");
  WiFi.mode(WIFI_STA);

  // Mac address.
  Serial.print("Board MAC Address:  ");
  Serial.println(WiFi.macAddress());

  // Set the same WiFi channel for our SSID.
  int32_t channel = getWiFiChannel();

  Serial.print("WiFi channel detected: ");
  Serial.println(channel);
  Serial.println("Before channel change...");
  WiFi.printDiag(Serial);

  wifi_promiscuous_enable(1);
  wifi_set_channel(channel);
  wifi_promiscuous_enable(0);

  Serial.println("After channel change...");
  WiFi.printDiag(Serial);
}

void setupESPNow()
{
  // ESP-NOW init.
  if (esp_now_init() != 0)
  {
    Serial.println("ESP-NOW init failed!");
    return;
  }
  esp_now_set_self_role(ESP_NOW_ROLE_COMBO);
  esp_now_register_send_cb(dataSendCallback);
  esp_now_register_recv_cb(dataReceiveCallback);

  // Register receiver.
  esp_now_add_peer(recvAddr, ESP_NOW_ROLE_COMBO, 1, NULL, 0);
}

void dataSendCallback(uint8_t *mac, uint8_t status)
{
  Serial.print("[Tank] Data send status: ");
  Serial.println((status == 0) ? "Success!" : "Failed!");
}

void dataReceiveCallback(uint8_t* mac, uint8_t* pData, uint8_t len)
{
  // Parse the data.
  ControlMsg* pCtrlMsg = (ControlMsg*) pData;
  Serial.print("[Tank] Data receieved: ");
  Serial.print("pump control: ");
  Serial.print(pCtrlMsg->pumpControl);
  Serial.print(" error status: ");
  Serial.println(pCtrlMsg->errorStatus);

  // Update the status.
  digitalWrite(PUMP_STATUS_PIN, pCtrlMsg->pumpControl ? HIGH : LOW);

  // Update the status.
  if (pCtrlMsg->errorStatus)
  {
    showErrorStatus(true);
  }
}

bool isBatLow()
{
  float volt = analogRead(BAT_CHECK_PIN) * MAX_BAT_VOLT / 1023;
  return (volt < MIN_BAT_VOLT);
}

void updateStatus()
{
  // Battery status.
  statusData.batWarn = isBatLow();
  Serial.print("Battery status: ");
  Serial.print((int)statusData.batWarn);
  digitalWrite(BAT_WARN_PIN, statusData.batWarn ? HIGH : LOW);

  // Water level.
  int waterLevel1 = digitalRead(WATER_LEVEL1_PIN);
  int waterLevel2 = digitalRead(WATER_LEVEL2_PIN);
  if (waterLevel1 && waterLevel2)
  {
    statusData.waterLevel = VALUE_LOW;
  }
  else if (waterLevel1 != waterLevel2)
  {
    statusData.waterLevel = VALUE_MED;
  }
  else
  {
    statusData.waterLevel = VALUE_HI;
  }
  Serial.print(" Water level: ");
  Serial.print((int)statusData.waterLevel);

  // Lid status.
  statusData.lidOpen = (digitalRead(LID_SENSOR_PIN) == LOW);
  Serial.print(" Lid open: ");
  Serial.println((int)statusData.lidOpen);

  // Send the data.
  esp_now_send(recvAddr, (uint8_t *) &statusData, sizeof(statusData));
}

void showErrorStatus(bool show)
{
  if (show)
  {
    digitalWrite(ERROR_STATUS_PIN, HIGH);
    errorDisplayTrigger.reset();
  }
  else
  {
    digitalWrite(ERROR_STATUS_PIN, LOW);
  }
}

void checkSwitchStatus()
{
  if (pumpSwitch.isPressed())
  {
    // Request pump ON.
    statusData.pumpSwitchPressed = true;
    Serial.println("Pump switch is pressed!");
    // Update control panel immediately.
    esp_now_send(recvAddr, (uint8_t *) &statusData, sizeof(statusData));
    // Reset manual switch status.
    statusData.pumpSwitchPressed = false;
  }
}
