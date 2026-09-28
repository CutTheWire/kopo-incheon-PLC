#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>

// ===== 사용자 설정 =====
const char* ap_ssid = "ESP_Relay_Control";
const char* ap_password = "12345678";

// D1 = GPIO5, D2 = GPIO4, D3 = GPIO0, D4 = GPIO2
const int relayPins[4] = {5, 4, 0, 2};
const bool LOGIC_REVERSE = true; // true = ACTIVE LOW (LOW 신호로 릴레이 ON)

// ===== 전역 변수 =====
bool relayState[4] = {false, false, false, false};

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// 모든 웹소켓 클라이언트에게 현재 릴레이 상태 브로드캐스트
void broadcastRelayState() {
  String json = "{\"states\":[";
  for (int i = 0; i < 4; i++) {
    if (i > 0) json += ",";
    json += relayState[i] ? "true" : "false";
  }
  json += "]}";

  webSocket.broadcastTXT(json);
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
      
      // 접속 시 최신 상태 브로드캐스트
      broadcastRelayState();
      break;
    }

    case WStype_TEXT: {
      String msg = String((char*)payload);
      
      // 릴레이 제어 명령 ("TOGGLE:채널:상태")
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

          Serial.printf("제어 [%u] -> 채널 %d: %s\n", num, channel + 1, relayOn ? "ON" : "OFF");
          broadcastRelayState();
        }
      }
      break;
    }
    default:
      break;
  }
}

String buildHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>4채널 릴레이 제어판</title>
  <style>
    * { -webkit-tap-highlight-color: transparent; user-select: none; box-sizing: border-box; }
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f5f5f5; margin: 0; padding: 15px; }
    .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; }
    h1 { color: #333; text-align: center; margin-top: 0; margin-bottom: 5px; }
    .connection-status { text-align: center; font-size: 13px; margin-bottom: 15px; padding: 6px; border-radius: 4px; }
    .online { background-color: #e2f0d9; color: #385723; }
    .offline { background-color: #fce4d6; color: #c65911; }
    
    .relay-channel { margin: 15px 0; padding: 15px; border: 1px solid #eee; border-radius: 8px; background-color: #fafafa; }
    .channel-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
    .channel-title { font-weight: bold; font-size: 16px; color: #222; }
    .status-badge { padding: 4px 10px; border-radius: 4px; font-size: 13px; font-weight: bold; }
    .status-badge.on { background-color: #d4edda; color: #155724; }
    .status-badge.off { background-color: #f8d7da; color: #721c24; }
    
    /* 2열 가로 배치 버튼 그룹 */
    .btn-row { display: flex; gap: 10px; }
    .btn { flex: 1; padding: 14px 5px; font-size: 15px; font-weight: bold; border: none; border-radius: 6px; cursor: pointer; transition: all 0.1s ease; text-align: center; touch-action: none; }
    
    /* 푸시 버튼 스타일 */
    .btn-push { background-color: #007bff; color: white; }
    .btn-push:active, .btn-push.active { background-color: #004494; transform: scale(0.97); }
    
    /* 토글 스위치 버튼 스타일 */
    .btn-toggle.off { background-color: #28a745; color: white; }
    .btn-toggle.on { background-color: #dc3545; color: white; }
    .btn-toggle:active { transform: scale(0.97); }

    .info { margin-top: 20px; padding-top: 15px; border-top: 1px solid #eee; font-size: 13px; color: #666; text-align: center; }
  </style>
</head>
<body>
  <div class="container">
    <h1>4채널 릴레이 제어판</h1>
    <div id="ws-status" class="connection-status offline">웹소켓 연결 중...</div>
)rawliteral";

  for (int i = 0; i < 4; i++) {
    html += "<div class='relay-channel'>";
    html += "<div class='channel-header'>";
    html += "<span class='channel-title'>채널 " + String(i + 1) + "</span>";
    html += "<span class='status-badge off' id='status-" + String(i) + "'>OFF</span>";
    html += "</div>";

    // 가로 2열 버튼 영역 (푸시버튼 | 스위치)
    html += "<div class='btn-row'>";
    html += "<button class='btn btn-push' id='push-btn-" + String(i) + "'>푸시 (누를 때만 ON)</button>";
    html += "<button class='btn btn-toggle off' id='toggle-btn-" + String(i) + "' onclick='handleToggleClick(" + String(i) + ")'>스위치 켜기</button>";
    html += "</div>";

    html += "</div>";
  }

  html += R"rawliteral(
    <div class="info">
      WiFi AP: ESP_Relay_Control | IP 주소: 192.168.4.1
    </div>
  </div>

  <script>
    let ws;
    let currentStates = [false, false, false, false];

    function initWebSocket() {
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
            currentStates = data.states;
            renderUI();
          }
        } catch (e) {
          console.error("데이터 파싱 오류", e);
        }
      };

      ws.onclose = function() {
        const wsStatus = document.getElementById('ws-status');
        wsStatus.textContent = '연결 끊김 - 재연결 시도 중...';
        wsStatus.className = 'connection-status offline';
        setTimeout(initWebSocket, 2000);
      };

      ws.onerror = function(err) {
        console.error('웹소켓 에러:', err);
        ws.close();
      };
    }

    // UI 상태 업데이트 (DOM 재생성 없이 기존 엘리먼트 속성만 변경하여 이벤트 유실 방지)
    function renderUI() {
      for (let i = 0; i < 4; i++) {
        const state = currentStates[i];

        // 1. 상태 바지 업데이트
        const statusBadge = document.getElementById('status-' + i);
        if (statusBadge) {
          statusBadge.textContent = state ? 'ON' : 'OFF';
          statusBadge.className = 'status-badge ' + (state ? 'on' : 'off');
        }

        // 2. 토글 스위치 버튼 업데이트
        const toggleBtn = document.getElementById('toggle-btn-' + i);
        if (toggleBtn) {
          toggleBtn.textContent = state ? '스위치 끄기' : '스위치 켜기';
          toggleBtn.className = 'btn btn-toggle ' + (state ? 'on' : 'off');
        }
      }
    }

    // 서버로 제어 명령 전송
    function sendToggle(channel, state) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('TOGGLE:' + channel + ':' + state);
      }
    }

    // 스위치 버튼 클릭 핸들러 (ON -> OFF / OFF -> ON 토글)
    function handleToggleClick(channel) {
      const currentState = currentStates[channel];
      sendToggle(channel, currentState ? 0 : 1);
    }

    // 푸시버튼 이벤트 바인딩 (Pointer Events 사용으로 떼기 누락 완전 방지)
    function bindPushEvents() {
      for (let i = 0; i < 4; i++) {
        const btn = document.getElementById('push-btn-' + i);
        if (!btn) continue;

        let isPressed = false;

        const handlePress = (e) => {
          e.preventDefault();
          if (!isPressed) {
            isPressed = true;
            try { btn.setPointerCapture(e.pointerId); } catch(err) {}
            btn.classList.add('active');
            sendToggle(i, 1);
          }
        };

        const handleRelease = (e) => {
          e.preventDefault();
          if (isPressed) {
            isPressed = false;
            try { btn.releasePointerCapture(e.pointerId); } catch(err) {}
            btn.classList.remove('active');
            sendToggle(i, 0);
          }
        };

        btn.onpointerdown = handlePress;
        btn.onpointerup = handleRelease;
        btn.onpointercancel = handleRelease; // 터치 취소, 바깥 이탈 등 모든 예외 상황 대응
      }
    }

    window.onload = function() {
      initWebSocket();
      bindPushEvents();
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

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("HTTP 및 WebSocket 서버 준비 완료");
}

void loop() {
  server.handleClient();
  webSocket.loop();
}