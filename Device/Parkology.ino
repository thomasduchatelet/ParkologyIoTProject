// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 
// To get started please visit https://microsoft.github.io/azure-iot-developer-kit/docs/projects/connect-iot-hub?utm_source=ArduinoExtension&utm_medium=ReleaseNote&utm_campaign=VSCode
#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "DevKitMQTTClient.h"

#include "config.h"
#include "utility.h"
#include "SystemTickCounter.h"

static bool hasWifi = false;
int messageCount = 1;
int sentMessageCount = 0;
static bool messageSending = true;

int lastEntranceState = LOW;
int lastExitState = LOW;
int entranceState;
int exitState;
const int entranceSwitch = PB_4;
const int exitSwitch = PB_3;

unsigned long lastDebounceTimeEnter = 0;  // the last time the output pin was toggled$
unsigned long lastDebounceTimeExit = 0;
unsigned long debounceDelay = 50;    // the debounce time; increase if the output flickers
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities
static void InitWifi()
{
  Screen.print(2, "Connecting...");
  
  if (WiFi.begin() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    Screen.print(1, ip.get_address());
    hasWifi = true;
    Screen.print(2, "Running... \r\n");
  }
  else
  {
    hasWifi = false;
    Screen.print(1, "No Wi-Fi\r\n ");
  }
}

static void SendConfirmationCallback(IOTHUB_CLIENT_CONFIRMATION_RESULT result)
{
  if (result == IOTHUB_CLIENT_CONFIRMATION_OK)
  {
    blinkSendConfirmation();
    sentMessageCount++;
  }

  Screen.print(1, "> IoT Hub");
  char line1[20];
  sprintf(line1, "Count: %d/%d",sentMessageCount, messageCount); 
  Screen.print(2, line1);
  }

static void MessageCallback(const char* payLoad, int size)
{
  blinkLED();
  Screen.print(1, payLoad, true);
}

static void DeviceTwinCallback(DEVICE_TWIN_UPDATE_STATE updateState, const unsigned char *payLoad, int size)
{
  char *temp = (char *)malloc(size + 1);
  if (temp == NULL)
  {
    return;
  }
  memcpy(temp, payLoad, size);
  temp[size] = '\0';
  free(temp);
}

static int  DeviceMethodCallback(const char *methodName, const unsigned char *payload, int size, unsigned char **response, int *response_size)
{
  LogInfo("Try to invoke method %s", methodName);
  const char *responseMessage = "\"Successfully invoke device method\"";
  int result = 200;

  if (strcmp(methodName, "start") == 0)
  {
    LogInfo("Start sending data");
    messageSending = true;
  }
  else if (strcmp(methodName, "stop") == 0)
  {
    LogInfo("Stop sending data");
    messageSending = false;
  }
  else
  {
    LogInfo("No method %s found", methodName);
    responseMessage = "\"No method found\"";
    result = 404;
  }

  *response_size = strlen(responseMessage) + 1;
  *response = (unsigned char *)strdup(responseMessage);

  return result;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino sketch
void setup()
{
  Screen.init();
  Screen.print(0, "Parkology IoT");
  Screen.print(2, "Initializing...");
  
  Screen.print(3, " > Serial");
  Serial.begin(115200);

  // Initialize the WiFi module
  Screen.print(3, " > WiFi");
  hasWifi = false;
  InitWifi();
  if (!hasWifi)
  {
    return;
  }

  LogTrace("HappyPathSetup", NULL);
  
  pinMode(entranceSwitch, INPUT);
  pinMode(exitSwitch, INPUT);
  lastEntranceState = digitalRead(entranceSwitch);
  lastExitState = digitalRead(exitSwitch);

  Screen.print(3, " > IoT Hub");
  DevKitMQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "Parkology-iot");
  DevKitMQTTClient_Init(true);

  DevKitMQTTClient_SetSendConfirmationCallback(SendConfirmationCallback);
  DevKitMQTTClient_SetMessageCallback(MessageCallback);
  DevKitMQTTClient_SetDeviceTwinCallback(DeviceTwinCallback);
  DevKitMQTTClient_SetDeviceMethodCallback(DeviceMethodCallback);
}

void loop()
{
  if (hasWifi)
  {
    if (messageSending)
    {
      Screen.print(3, " > Sending");
      
      entranceState = digitalRead(entranceSwitch);
      exitState = digitalRead(exitSwitch);
        
      if (entranceState == HIGH && lastEntranceState == LOW) {
        sendMessage(1);
        Serial.println("enter sent");
      }
      if (exitState == HIGH && lastExitState == LOW) {
        sendMessage(-1);
        Serial.println("exit sent");
          
      }
      lastEntranceState = entranceState;
      lastExitState = exitState;
    }
    else
    {
      DevKitMQTTClient_Check();
    }
  }
}

void sendMessage(int value){
  char messagePayload[MESSAGE_MAX_LEN];
  readMessage(messageCount, messagePayload, value);
  EVENT_INSTANCE* message = DevKitMQTTClient_Event_Generate(messagePayload, MESSAGE);
  DevKitMQTTClient_SendEventInstance(message);
}
