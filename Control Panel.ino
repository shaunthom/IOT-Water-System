#define BLYNK_PRINT Serial



#include <BlynkSimpleEsp8266.h>
#include <LiquidCrystal_I2C.h>

// WiFi credentials.
#define WIFI_SSID       "Airfi"
#define WIFI_PASS       "wifimanoj"

// Building address.
#define BUILDING_ADDRESS  "Central Hospital, Redmond, WA 98052."

// Blynk authentication ID.
#define BLYNK_AUTH_ID   "hR5YDYJ_2WjCl2W7ynEqeO2ZLDYivTw0"


// GPIO.
#define CURRENT_SENSOR_PIN    A0
#define LOW_VOLT_SENSOR_PIN   D7
#define HIGH_VOLT_SENSOR_PIN  D6
#define PUMP_SWITCH_PIN       D3
#define MODE_SWITCH_PIN       D5

// Macro for ADC value to current value conversion.
#define ADC_VALUE_TO_CURRENT(val) ((2.5 - ((val) * (5.0 / 1023.0))) / 0.66)

// Time gap for measurement.
#define MEASURE_DELAY         50

// Current average and min/max.
#define CURRENT_AVG_COUNT     20
#define MIN_ALLOWED_CURRENT   1.0
#define MAX_ALLOWED_CURRENT   2.0

// Parameters processing interval.
#define PARAM_CHECK_DELAY     2000

// Time gap for status update.
#define STATUS_UPDATE_DELAY   1000

// Time gap for pump failure reset.
#define PUMP_DISABLED_MSG_TIME  2000

// Switches.
ToggleSwitch pumpSwitch(PUMP_SWITCH_PIN);
ToggleSwitch modeSwitch(MODE_SWITCH_PIN);

// LCD.
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Blynk terminal.
WidgetTerminal blynkTerminal(V0);

// Status data.
char voltageLevel = VALUE_LOW;
char currentLevel = VALUE_LOW;
char lastCurrentLevel = VALUE_LOW;
bool pumpDisabled = false;
bool manualMode = false;
bool overload = false;
bool noWaterNotified = false;

Sump* pActiveSump = &sump1;

////////////////////////////////////////////////////////////////////////
// Functions
////////////////////////////////////////////////////////////////////////

void setup()
{
  Serial.begin(115200);

  setupGPIO();
  setupLCD();
  setupBlynk();
  setupWiFi();
  setupESPNow();

  lcd.clear();
}

void loop()
{
  Blynk.run();

  // Check switch status.
  char updateNeeded = checkSwitchStatus();

  // Measure parameters.
  if (measureParamsTrigger.isTimeOut())
  {
    measureParams();
    measureParamsTrigger.reset();
  }

  // Check parameters.
  if (checkParamsTrigger.isTimeOut())
  {
    checkParams();
    checkParamsTrigger.reset();
  }

  // Check for pump disabled state.
  if (pumpDisabled)
  {
    if (pumpDisabledMsgTrigger.isTimeOut())
    {
      pumpDisabled = false;
    }
  }

  // Send the message if wait time is over.
  if (updateNeeded || dataUpdateTrigger.isTimeOut())
  {
    updateDisplay();
    dataUpdateTrigger.reset();
  }
}

void setupGPIO()
{
  pinMode(LOW_VOLT_SENSOR_PIN, INPUT_PULLUP);
  pinMode(HIGH_VOLT_SENSOR_PIN, INPUT_PULLUP);
  pinMode(PUMP_SWITCH_PIN, INPUT_PULLUP);
  pinMode(MODE_SWITCH_PIN, INPUT_PULLUP);
}

void setupLCD()
{
   lcd.begin();
  lcd.backlight();
  
  // Welcome display.
  lcd.setCursor(5, 0);
  lcd.print("WELCOME");
  lcd.setCursor(0, 1);
  lcd.print(" Control  Panel");
}

void setupBlynk()
{
  // Blynk init.
  Blynk.begin(BLYNK_AUTH_ID, WIFI_SSID, WIFI_PASS);
  Serial.println("Blynk init is complete!");
}

void setupWiFi()
{
  // Set WiFi mode as Station.
  Serial.println("WiFi init...");
  WiFi.mode(WIFI_AP_STA);

  // Mac address.
  Serial.print("Board MAC Address:  ");
  Serial.println(WiFi.macAddress());
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

  // Register devices.
  tank.registerRole();
  sump1.registerRole();
  sump2.registerRole();
}

void dataSendCallback(uint8_t *mac, uint8_t status)
{
  Serial.print("[CP] Data send status: ");
  Serial.println((status == 0) ? "Success!" : "Failed!");
}

void dataReceiveCallback(uint8_t* mac, uint8_t* pData, uint8_t len)
{
  // Mac address.
  char s[64];
  sprintf(s, "[CP] Data received from [%x:%x:%x:%x:%x:%x]",
          mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(s);

  // Process the data.
  if (tank.matchMac(mac))
  {
    tank.processData((TankStatus*) pData);
    // Check for pump ON request.
    if (tank.isPumpSwitchPressed())
    {
      Serial.println("Tank switch pressed!");
      if (pActiveSump->isPumpOn())
      {
        // Switch OFF the pump.
        switchOnPump(false);
      }
      else
      {
        // Try to switch ON the pump.
        if (!trySwitchOnPump(true))
        {
          tank.notifyError();
        }
      }
    }
  }
  else
  {
    Sump* pSump = getSump(mac);
    if (pSump != nullptr)
    {
      pSump->processData((SumpStatus*) pData);
      // Check for pump ON request.
      if (pSump->isPumpSwitchPressed())
      {
        Serial.println("Sump switch pressed!");
        if (pSump->isPumpOn())
        {
          // Switch OFF the pump which must be the active pump.
          switchOnPump(false);
          // Since the active pump is switched OFF from sump side,
          // use the other sump next time.
          switchToNextSump();
        }
        else
        {
          if (pActiveSump->isPumpOn())
          {
            // Switch OFF the active pump.
            switchOnPump(false);
          }
          pActiveSump = pSump;
          // Try to switch ON the pump, without sump switching.
          if (!trySwitchOnPump(false))
          {
            pSump->notifyError();
          }
        }
      }
    }
    else
    {
      Serial.println("Unknown device!");
    }
  }
}

Sump* getSump(uint8_t* mac)
{
  if (sump1.matchMac(mac))
  {
    return &sump1;
  }
  if (sump2.matchMac(mac))
  {
    return &sump2;
  }
  return nullptr;
}

bool checkSwitchStatus()
{
  bool updateNeeded = false;
  if (modeSwitch.isPressed())
  {
    manualMode = !manualMode;
    updateNeeded = true;
    if (!manualMode)
    {
      overload = false;
    }
  }

  if (pumpSwitch.isPressed())
  {
    // Switch to manual mode.
    manualMode = true;
    // Toggle the pump state.
    if (pActiveSump->isPumpOn())
    {
      switchOnPump(false);
    }
    else
    {
      if (!trySwitchOnPump(true))
      {
        // Cannot switch ON the pump.
        pumpDisabled = true;
        pumpDisabledMsgTrigger.reset();
      }
    }
    updateNeeded = true;
  }

  return updateNeeded;
}

bool trySwitchOnPump(bool enableSumpSwitching)
{
  bool success = false;
  if ((voltageLevel == VALUE_MED) &&
      (!tank.isBatteryDown()) &&
      (tank.getWaterLevel() != VALUE_HI))
  {
    if (!pActiveSump->isWaterAvailable() && enableSumpSwitching)
    {
      switchToNextSump();
    }
    if (pActiveSump->isWaterAvailable())
    {
      // Switch ON the pump.
      switchOnPump(true);
      success = true;
    }
  }
  return success;
}

void switchOnPump(bool pumpOn)
{
  if(pumpOn)
  {
    overload = false;
  }
  pActiveSump->switchOnPump(pumpOn);
  tank.notifyPumpState(pumpOn);
}

void switchToNextSump()
{
  pActiveSump = (pActiveSump == &sump1) ? &sump2 : &sump1;
}

int getActiveSumpIndex()
{
  return (pActiveSump == &sump1) ? 1 : 2;
}

void measureParams()
{
  // Check voltage.
  if (digitalRead(LOW_VOLT_SENSOR_PIN) == HIGH)
  {
    voltageLevel = VALUE_LOW;
  }
  else if (digitalRead(HIGH_VOLT_SENSOR_PIN) == HIGH)
  {
    voltageLevel = VALUE_HI;
  }
  else // Voltage is normal.
  {
    voltageLevel = VALUE_MED;
  }

  // Check current.
  float current = getCurrentAvg();
  //Serial.print("Avg. current: "); Serial.println(current);
  if (current < MIN_ALLOWED_CURRENT)
  {
    currentLevel = VALUE_LOW;
  }
  else if (current > MAX_ALLOWED_CURRENT)
  {
    currentLevel = VALUE_HI;
  }
  else // Current is normal.
  {
    currentLevel = VALUE_MED;
  }
}

float getCurrentAvg()
{
  static float vals[CURRENT_AVG_COUNT] = { 0 };
  static int circularIdx = 0;

  // Read the value.
  int adcVal = analogRead(CURRENT_SENSOR_PIN);
  float current = ADC_VALUE_TO_CURRENT(adcVal);
  vals[circularIdx] = current;

  // Find the average.
  float avg = 0;
  for (int idx = 0; idx < CURRENT_AVG_COUNT; idx++)
  {
    avg += vals[idx];
  }
  avg /= CURRENT_AVG_COUNT;

  // Update circular indexing.
  circularIdx = (circularIdx + 1) % CURRENT_AVG_COUNT;

  return avg;
}

void checkParams()
{
  // Check if all sumps are empty.
  if (!sump1.isWaterAvailable() &&
      !sump2.isWaterAvailable())
  {
    // Notify.
    if (!noWaterNotified)
    {
            noWaterNotified = true;
    }
  }
  else if (noWaterNotified)
  {
    
    noWaterNotified = false;
  }

  

  // Voltage/current and sump-water availability check.
  if (voltageLevel != VALUE_MED)
  {
    // Switch OFF the pump.
    if (pActiveSump->isPumpOn())
    {
      switchOnPump(false);
    }
  }
  else if ((currentLevel == VALUE_LOW) ||
           !pActiveSump->isWaterAvailable())
  {
    if (pActiveSump->isPumpOn())
    {
      // Possibily, no water in the current sump.
      switchOnPump(false);
      switchToNextSump();
      if (pActiveSump->isWaterAvailable())
      {
        switchOnPump(true);
      }
    }
  }
  else if (currentLevel == VALUE_HI)
  {
    // Overload detection.
    if (lastCurrentLevel == VALUE_HI)
    {
      overload = true;
      manualMode = true;
      // Switch OFF the pump.
      if (pActiveSump->isPumpOn())
      {
        switchOnPump(false);
        pActiveSump->notifyError();
        tank.notifyError();
      }
    }
  }
  else
  {
  }
  lastCurrentLevel = currentLevel;

  // Tank level check.
  if (tank.getWaterLevel() == VALUE_LOW)
  {
    // Switch ON the pump, if auto mode and pump is OFF.
    if (!manualMode && !pActiveSump->isPumpOn())
    {
      trySwitchOnPump(true);
    }
  }
  else if (tank.getWaterLevel() == VALUE_HI)
  {
    // Switch OFF the pump.
    if (pActiveSump->isPumpOn())
    {
      switchOnPump(false);
      switchToNextSump();
    }
  }
}

void notifyNoWater()
{
  char msg[128];
  sprintf(msg, "No water in %s", BUILDING_ADDRESS);
  Serial.println(msg);
  blynkTerminal.println(msg);
  blynkTerminal.flush();
  Blynk.notify(msg);
}

void notifyWaterAvailable()
{
  char msg[128];
  sprintf(msg, "Water available in %s", BUILDING_ADDRESS);
  Serial.println(msg);
  blynkTerminal.println(msg);
  blynkTerminal.flush();
  //Blynk.notify(msg);
}


void updateDisplay()
{
  // Electric params and water level.
  char* paramMsg[] = { "Low", "Nor", "Hi " };
  char* waterMsg[] = { "L", "M", "F" };
  char s[20];
  sprintf(s, "V:%s A:%s Wr:%s",
          paramMsg[voltageLevel],
          paramMsg[currentLevel],
          waterMsg[tank.getWaterLevel()]);
  lcd.setCursor(0, 0);
  lcd.print(s);

  // Pump/lid/battery Status.
  lcd.setCursor(0, 1);
  if (overload)
  {
    sprintf(s, "OvrLoad-%d", getActiveSumpIndex());
  }
  else if (tank.isBatteryDown())
  {
    sprintf(s, "Low Bat!  ");
  }
  else if (tank.isLidOpen())
  {
    sprintf(s, "Lid Open! ");
  }
  else
  {
    const char* pumpMsg = pumpDisabled ? "Err  "
                          : (pActiveSump->isPumpOn() ? "ON   " : "OFF  ");
    sprintf(s, "Pump:%s", pumpMsg);
  }
  lcd.print(s);

  // Control mode.
  lcd.setCursor(10, 1);
  lcd.print(manualMode ? " [MAN]" : "[AUTO]");
}
