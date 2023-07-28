#define WIFI_SSID         "Airfi"


// GPIO.
#define WATER_SENSOR_PIN    D7
#define WATER_STATUS_PIN    D1
#define PUMP_POWER_PIN      D4
#define PUMP_STATUS_PIN     D5
#define PUMP_SWITCH_PIN     D3
#define ERROR_STATUS_PIN    D2

// Time gap for update check and data sending.
#define UPDATE_DELAY        1000

// Time for error status display.
#define ERROR_DISPLAY_TIME  2000

// Trigger timers.
TimedTrigger dataUpdateTrigger(UPDATE_DELAY);
TimedTrigger errorDisplayTrigger(ERROR_DISPLAY_TIME);

// Switch.
ToggleSwitch pumpSwitch(PUMP_SWITCH_PIN);

// Data.
SumpStatus statusData = { 0 };

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
  pinMode(WATER_SENSOR_PIN, INPUT_PULLUP);
  pinMode(WATER_STATUS_PIN, OUTPUT);
  pinMode(PUMP_POWER_PIN, OUTPUT);
  pinMode(PUMP_STATUS_PIN, OUTPUT);
  pinMode(PUMP_SWITCH_PIN, INPUT_PULLUP);
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
  Serial.print("[Sump] Data send status: ");
  Serial.println((status == 0) ? "Success!" : "Failed!");
}

void dataReceiveCallback(uint8_t* mac, uint8_t* pData, uint8_t len)
{
  // Parse the data.
  ControlMsg* pCtrlMsg = (ControlMsg*) pData;
  Serial.print("[Sump] Data receieved: ");
  Serial.print("pump control: ");
  Serial.print(pCtrlMsg->pumpControl);
  Serial.print(" error status: ");
  Serial.println(pCtrlMsg->errorStatus);

  // Execute the request.
  controlPump(pCtrlMsg->pumpControl);

  // Update the status.
  if (pCtrlMsg->errorStatus)
  {
    showErrorStatus(true);
  }
}

void controlPump(bool pumpControl)
{
  // Ensure water level.
  int val = (pumpControl && statusData.waterAvailable) ? HIGH : LOW;

  // Control the pump and update the status.
  digitalWrite(PUMP_POWER_PIN, val);
  digitalWrite(PUMP_STATUS_PIN, val);
}

void updateStatus()
{
  // Water availability.
  statusData.waterAvailable = (digitalRead(WATER_SENSOR_PIN) == LOW);
  Serial.print("Water available: ");
  Serial.println((int)statusData.waterAvailable);
  digitalWrite(WATER_STATUS_PIN, statusData.waterAvailable ? HIGH : LOW);

  // Saftey precaution.
  if (!statusData.waterAvailable)
  {
    controlPump(0);
  }

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
