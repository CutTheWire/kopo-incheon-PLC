#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h> // WebSockets by Markus Sattler 라이브러리 필요

// ===== 사용자 설정 =====
const char* ap_ssid = "ESP_Relay_Control";
const char* ap_password = "12345678";

// D1 = GPIO5, D2 = GPIO4, D3 = GPIO0, D4 = GPIO2
const int relayPins[4] = {5, 4, 0, 2};
const bool LOGIC_REVERSE = true; // true = ACTIVE LOW (LOW 신호로 릴레이 ON)

// ===== 전역 변수 =====
bool relayState[4] = {false, false, false, false};
ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); // 81번 포트 사용

// 모든 웹소켓 클라이언트에게 현재 릴레이 상태 브로드캐스트
void broadcastRelayState() {
  String json = "{\"states\":[";
  for (int i = 0; i < 4; i++) {
    if (i > 0) json += ",";
    json += relayState[i] ? "true" : "false";
  }
  json += "]}";
  
  webSocket.broadcastTXT(json); // 연결된 모든 접속자에게 전송
}

// 웹소켓 이벤트 처리
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] 접속 해제\n", num);
      break;

    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] 새 접속자 연결됨 - IP: %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      
      // 새로 접속한 클라이언트에게 현재 상태 즉시 전달
      String json = "{\"states\":[";
      for (int i = 0; i < 4; i++) {
        if (i > 0) json += ",";
        json += relayState[i] ? "true" : "false";
      }
      json += "]}";
      webSocket.sendTXT(num, json);
      break;
    }

    case WStype_TEXT: {
      // 명령 수신 예: "TOGGLE:0:1" (채널 0, ON) 또는 "TOGGLE:2:0" (채널 2, OFF)
      String msg = String((char*)payload);
      if (msg.startsWith("TOGGLE:")) {
        int firstColon = msg.indexOf(':');
        int secondColon = msg.indexOf(':', firstColon + 1);
        
        int channel = msg.substring(firstColon + 1, secondColon).toInt();
        int state = msg.substring(secondColon + 1).toInt();

        if (channel >= 0 && channel < 4) {
          bool relayOn = (state == 1);
          int outputLevel = LOGIC_REVERSE ? (relayOn ? LOW : HIGH) : (relayOn ? HIGH : LOW);

          digitalWrite(relayPins[channel], outputLevel);
          relayState[channel] = relayOn;

          Serial.printf("명령 수신 [%u] -> 채널 %d: %s\n", num, channel + 1, relayOn ? "ON" : "OFF");

          // 상태가 변경되었으므로 모든 접속자에게 변경 사항 즉시 동기화
          broadcastRelayState();
        }
      }
      break;
    }
    default:
      break;
  }
}

// HTML 페이지 생성
String buildHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>4채널 실시간 릴레이 제어판</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f5f5f5; margin: 0; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; }
    h1 { color: #333; text-align: center; }
    .connection-status { text-align: center; font-size: 13px; margin-bottom: 15px; padding: 5px; border-radius: 4px; }
    .online { background-color: #e2f0d9; color: #385723; }
    .offline { background-color: #fce4d6; color: #c65911; }
    .relay-channel { margin: 15px 0; padding: 15px; border: 1px solid #eee; border-radius: 8px; background-color: #fafafa; }
    .channel-label { font-weight: bold; margin-bottom: 10px; display: flex; justify-content: space-between; align-items: center; }
    .status { padding: 4px 8px; border-radius: 4px; font-size: 14px; font-weight: bold; }
    .status.on { background-color: #d4edda; color: #155724; }
    .status.off { background-color: #f8d7da; color: #721c24; }
    .button { display: inline-block; width: 100%; padding: 12px; margin: 5px 0; font-size: 16px; border: none; border-radius: 6px; cursor: pointer; transition: all 0.2s ease; }
    .button.on { background-color: #28a745; color: white; }
    .button.off { background-color: #dc3545; color: white; }
    .button:active { transform: scale(0.98); }
    .info { margin-top: 20px; padding-top: 15px; border-top: 1px solid #eee; font-size: 14px; color: #666; text-align: center; }
  </style>
</head>
<body>
  <div class="container">
    <h1>4채널 실시간 릴레이 제어판</h1>
    <div id="ws-status" class="connection-status offline">웹소켓 연결 중...</div>
)rawliteral";

  for (int i = 0; i < 4; i++) {
    html += "<div class='relay-channel'>";
    html += "<div class='channel-label'>";
    html += "<span>채널 " + String(i + 1) + "</span>";
    html += "<span class='status " + String(relayState[i] ? "on" : "off") + "' id='status-" + String(i) + "'>" + String(relayState[i] ? "ON" : "OFF") + "</span>";
    html += "</div>";
    html += "<button class='button " + String(relayState[i] ? "off" : "on") + "' id='btn-" + String(i) + "' onclick='toggleRelay(" + String(i) + ", " + String(relayState[i] ? 0 : 1) + ")'>";
    html += (relayState[i] ? "끄기" : "켜기");
    html += "</button>";
    html += "</div>";
  }

  html += R"rawliteral(
    <div class="info">
      WiFi AP: ESP_Relay_Control<br>
      IP 주소: 192.168.4.1
    </div>
  </div>
  <script>
    let ws;

    function initWebSocket() {
      // 81번 포트로 웹소켓 연결
      ws = new WebSocket('ws://' + window.location.hostname + ':81/');

      ws.onopen = function() {
        const wsStatus = document.getElementById('ws-status');
        wsStatus.textContent = '실시간 동기화 연결됨';
        wsStatus.className = 'connection-status online';
      };

      ws.onmessage = function(event) {
        try {
          const data = JSON.parse(event.data);
          if (data && data.states) {
            data.states.forEach((state, index) => {
              const statusEl = document.getElementById('status-' + index);
              const btnEl = document.getElementById('btn-' + index);
              if (statusEl && btnEl) {
                statusEl.textContent = state ? 'ON' : 'OFF';
                statusEl.className = 'status ' + (state ? 'on' : 'off');
                btnEl.className = 'button ' + (state ? 'off' : 'on');
                btnEl.textContent = state ? '끄기' : '켜기';
                btnEl.setAttribute('onclick', 'toggleRelay(' + index + ', ' + (state ? 0 : 1) + ')');
              }
            });
          }
        } catch (e) {
          console.error("데이터 파싱 오류", e);
        }
      };

      ws.onclose = function() {
        const wsStatus = document.getElementById('ws-status');
        wsStatus.textContent = '연결 끊김 - 재연결 시도 중...';
        wsStatus.className = 'connection-status offline';
        // 끊겼을 때 2초 후 자동 재연결
        setTimeout(initWebSocket, 2000);
      };

      ws.onerror = function(err) {
        console.error('웹소켓 에러:', err);
        ws.close();
      };
    }

    function toggleRelay(channel, state) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        // 웹소켓으로 제어 명령 전송
        ws.send('TOGGLE:' + channel + ':' + state);
      } else {
        alert('서버와 연결되어 있지 않습니다.');
      }
    }

    window.onload = function() {
      initWebSocket();
    };
  </script>
</body>
</html>
)rawliteral";

  return html;
}

void handleRoot() {
  server.send(200, "text/html", buildHTML());
}

void handleNotFound() {
  server.send(404, "text/plain", "페이지를 찾을 수 없습니다");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP8266 4채널 릴레이 제어 시작 ===");

  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    int initialLevel = LOGIC_REVERSE ? HIGH : LOW;
    digitalWrite(relayPins[i], initialLevel);
    relayState[i] = false;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP 시작 - SSID: ");
  Serial.print(ap_ssid);
  Serial.print(", IP: ");
  Serial.println(apIP);

  // HTTP 웹서버 (포트 80)
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  // 웹소켓 서버 시작 (포트 81)
  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("HTTP 및 WebSocket 서버 준비 완료");
}

void loop() {
  server.handleClient();  // HTTP 요청 처리
  webSocket.loop();       // 웹소켓 이벤트 처리
}