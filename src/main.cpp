#include <Arduino.h>

#include <DNSServer.h>
#if defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266Wifi.h>
#include <ESP8266HTTPClient.h>
#else
#include <WebServer.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <Update.h>
#endif

#include <WiFiManager.h>

#define USE_SERIAL Serial

#define CURRENT_VERSION VERSION
#define CLOUD_FUNCTION_URL "https://test.com/latest"

WiFiManager wifiManager;
WiFiClient client;
WebServer server(80);

/* 
 * Check if needs to update the device and returns the download url.
 */
String getDownloadUrl()
{
  HTTPClient http;
  String downloadUrl;
  USE_SERIAL.print("[HTTP] begin...\n");

  String url = CLOUD_FUNCTION_URL + String("?version=") + CURRENT_VERSION;
  http.begin(url);

  USE_SERIAL.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();

  // httpCode will be negative on error
  if (httpCode > 0)
  {
    // HTTP header has been send and Server response header has been handled
    USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {
      String payload = http.getString();
      USE_SERIAL.println(payload);
      downloadUrl = payload;
    }
  }
  else
  {
    USE_SERIAL.printf("[HTTP] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
  }

  http.end();

  return downloadUrl;
}

/* 
 * Download binary image and use Update library to update the device.
 */
bool downloadUpdate(String url)
{
  HTTPClient http;
  USE_SERIAL.print("[HTTP] Download begin...\n");

  http.begin(url);

  USE_SERIAL.print("[HTTP] GET...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  if (httpCode > 0)
  {
    // HTTP header has been send and Server response header has been handled
    USE_SERIAL.printf("[HTTP] GET... code: %d\n", httpCode);

    // file found at server
    if (httpCode == HTTP_CODE_OK)
    {

      int contentLength = http.getSize();
      USE_SERIAL.println("contentLength : " + String(contentLength));

      if (contentLength > 0)
      {
        bool canBegin = Update.begin(contentLength);
        if (canBegin)
        {
          WiFiClient stream = http.getStream();
          USE_SERIAL.println("Begin OTA. This may take 2 - 5 mins to complete. Things might be quite for a while.. Patience!");
          size_t written = Update.writeStream(stream);

          if (written == contentLength)
          {
            USE_SERIAL.println("Written : " + String(written) + " successfully");
          }
          else
          {
            USE_SERIAL.println("Written only : " + String(written) + "/" + String(contentLength) + ". Retry?");
          }

          if (Update.end())
          {
            USE_SERIAL.println("OTA done!");
            if (Update.isFinished())
            {
              USE_SERIAL.println("Update successfully completed. Rebooting.");
              ESP.restart();
            }
            else
            {
              USE_SERIAL.println("Update not finished? Something went wrong!");
            }
          }
          else
          {
            USE_SERIAL.println("Error Occurred. Error #: " + String(Update.getError()));
          }
        }
        else
        {
          USE_SERIAL.println("Not enough space to begin OTA");
          client.flush();
          return false;
        }
      }
      else
      {
        USE_SERIAL.println("There was no content in the response");
        client.flush();
        return false;
      }
    }
    else
    {
      return false;
    }
  }
  else
  {
    return false;
  }
}

/* 
 * Show current device version
 */
void handleRoot() {  
  server.send(200, "text/plain", "v" + String(CURRENT_VERSION));
}

void setup()
{
  // Setup Wifi Manager
  String version = String("<p>Current Version - v") + String(CURRENT_VERSION) + String("</p>");
  WiFiManagerParameter versionText(version.c_str());
  wifiManager.addParameter(&versionText);
  wifiManager.autoConnect();

 
  // Check if we need to download a new version
  String downloadUrl = getDownloadUrl();
  if (downloadUrl.length() > 0)
  {
    bool success = downloadUpdate(downloadUrl);
    if (!success)
    {
      USE_SERIAL.println("Error updating device");
    }
  }

  server.on("/", handleRoot);
  server.begin();
  USE_SERIAL.println("HTTP server started");

  USE_SERIAL.print("IP address: ");
  USE_SERIAL.println(WiFi.localIP());
}

void loop()
{
  // Just chill
  server.handleClient();
}