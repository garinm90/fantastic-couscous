#include <Arduino.h>
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

const char *ssid = "DENNYS";
const char *password = "LEDlights1";

const char *PARAM_MESSAGE = "message";
extern const char index_html[];

char buffer[128];

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len);

void initWebSocket();

void notFound(AsyncWebServerRequest *request)
{
  request->send(404, "text/plain", "Not found");
}

void setup()
{
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.printf("WiFi Failed!\n");
    return;
  }

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  initWebSocket();

  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.onNotFound(notFound);

  server.begin();
}
void loop()
{
  if (Serial.available() > 0)
  {
    int bytes_available = Serial.available();
    Serial.readBytes(buffer, bytes_available);
    ws.textAll(buffer, bytes_available);
    memset(buffer, 0, sizeof buffer);
  }
  if (Serial2.available() > 0)
  {
    int bytes_available = Serial2.available();
    Serial2.readBytes(buffer, bytes_available);
    Serial.print(buffer);
    memset(buffer, 0, sizeof buffer);
  }
}

void initWebSocket()
{
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

void handleWebSocketMessage(void *arg, uint8_t *data, size_t len)
{
  AwsFrameInfo *info = (AwsFrameInfo *)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT)
  {
    if (strncmp((char *)data, "BD", 2) == 0)
    {
      Serial.println((char *)data);
      char *data_buffer = (char *)&data[3];
      String baud_rate = data_buffer;
      int new_baud_rate = baud_rate.toInt();
      Serial2.flush();
      Serial2.begin(new_baud_rate);
      // Flush out garbage collected while changing baudrate
      while (Serial2.available())
        Serial2.read();
      ws.printfAll("Baud rate set to %i", new_baud_rate);
    }
    else
    {
      Serial2.print((char *)data);
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len)
{
  switch (type)
  {
  case WS_EVT_CONNECT:
    Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    break;
  case WS_EVT_DISCONNECT:
    Serial.printf("WebSocket client #%u disconnected\n", client->id());
    break;
  case WS_EVT_DATA:
    handleWebSocketMessage(arg, data, len);
    break;
  case WS_EVT_PONG:
  case WS_EVT_ERROR:
    break;
  }
}

const char index_html[] PROGMEM = R"rawliteral(
 <!DOCTYPE html>
<html lang="en">
  <head>
    <meta charset="UTF-8" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Serial Monitor</title>
  </head>
  <body>
    <h4 style="text-align: center">Monitor</h4>
    <h5 style="text-align: center">Enter "BD Rate" no line ending to set the baud rate.</h5>

    <textarea
      style="display: block; margin-left: auto; margin-right: auto"
      id="monitor"
      name="textarea"
      rows="20"
      cols="70"
    ></textarea>
    <div style="display: flex; justify-content: center; margin-top: 1rem">
      <input
        id="message-value"
        style="margin-right: 5px"
        type="text"
        size="55"
      />
      <button id="message-button">Submit</button>
      <select id="line-selector" name="choice" style="margin-left: 5px">
        <option value="NL">New Line</option>
        <option value="CR">Carriage Return</option>
        <option value="NL+CR" selected>NL + CR</option>
        <option value="None">No Line Ending</option>
      </select>
    </div>
  </body>

  <script>
    const gateway = `ws://${window.location.hostname}/ws`;
    let websocket;
    let monitor = document.getElementById("monitor");
    const messageButton = document.getElementById("message-button");
    messageButton.addEventListener("click", sendMessage);
    window.addEventListener("load", onLoad);
    function initWebSocket() {
      console.log("Trying to open a WebSocket connection...");
      websocket = new WebSocket(gateway);
      websocket.onopen = onOpen;
      websocket.onclose = onClose;
      websocket.onmessage = onMessage; // <-- add this line
    }
    function onOpen(event) {
      console.log("Connection opened");
    }
    function onClose(event) {
      console.log("Connection closed");
      setTimeout(initWebSocket, 2000);
    }
    function onMessage(event) {
      // Concatenate any messages into the text field.
      console.log(event.data);
      monitor.value += event.data;
        monitor.scrollTop = monitor.scrollHeight;

    }
    function onLoad(event) {
      initWebSocket();
    }
    function sendMessage(event) {
      const selector = document.getElementById("line-selector").options
        .selectedIndex;
      // Remove any linebreaks the browser might add cause javascript. Then add our line endings for the monitor.
      let messageValue = document.getElementById("message-value").value;
      messageValue = messageValue.replace(/(\r\n|\n|\r)/gm, " ");
      switch (selector) {
        case 0:
          messageValue += String.fromCharCode(10);
          break;
        case 1:
          messageValue += String.fromCharCode(13);
          break;
        case 2:
          messageValue += String.fromCharCode(10, 13);
          break;
        default:
          break;
      }
      websocket.send(messageValue);
      monitor.value += "Sent: " + messageValue;
        monitor.scrollTop = monitor.scrollHeight;

    }

  </script>
</html>

)rawliteral";