#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>

// ===== 사용자 설정 =====
// 1. 웹페이지 접속 인증 계정 설정
const char* www_username = "admin";
const char* www_password = "team_B_261023_kopo!";

// 2. 자체 AP 설정
const char* ap_ssid = "ESP_Relay_Control";
const char* ap_password = "12345678";

// 3. mDNS 도메인 이름 설정 (http://esp-relay.local)
const char* mdns_hostname = "esp-relay";

// 핀 설정: D1 = GPIO5, D2 = GPIO4, D3 = GPIO0, D4 = GPIO2
const int relayPins[4] = {5, 4, 0, 2};
const bool LOGIC_REVERSE = true; // true = ACTIVE LOW

// ===== 전역 변수 =====
bool relayState[4] = {false, false, false, false};

// 웹소켓 검증용 보안 토큰
String authToken = "";

// 클라이언트별 웹소켓 인증 완료 여부 배열
bool isClientAuthenticated[WEBSOCKETS_SERVER_CLIENT_MAX] = {false};

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);

// 보안 토큰 생성 함수
String generateAuthToken(const char* user, const char* pass) {
  String raw = String(user) + ":" + String(pass) + "_SECRET_SALT";
  uint32_t hash = 5381;
  for (size_t i = 0; i < raw.length(); i++) {
    hash = ((hash << 5) + hash) + raw[i];
  }
  return String(hash, HEX);
}

// 모든 웹소켓 클라이언트에게 현재 릴레이 상태 브로드캐스트
void broadcastRelayState() {
  String json = "{\"type\":\"relay_state\",\"states\":[";
  for (int i = 0; i < 4; i++) {
    if (i > 0) json += ",";
    json += relayState[i] ? "true" : "false";
  }
  json += "]}";

  webSocket.broadcastTXT(json);
}

// 웹소켓 이벤트 핸들러
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t length) {
  switch (type) {
    case WStype_DISCONNECTED:
      Serial.printf("[%u] 웹소켓 연결 해제\n", num);
      if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
        isClientAuthenticated[num] = false;
      }
      break;

    case WStype_CONNECTED: {
      IPAddress ip = webSocket.remoteIP(num);
      Serial.printf("[%u] 클라이언트 접속 - IP: %d.%d.%d.%d\n", num, ip[0], ip[1], ip[2], ip[3]);
      if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
        isClientAuthenticated[num] = false;
      }
      break;
    }

    case WStype_TEXT: {
      String msg = String((char*)payload);
      
      // 0. 핸드셰이크 토큰 인증
      if (msg.startsWith("AUTH:")) {
        String clientToken = msg.substring(5);
        if (clientToken == authToken) {
          if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
            isClientAuthenticated[num] = true;
          }
          Serial.printf("[%u] 토큰 인증 성공\n", num);
          broadcastRelayState(); // 초기 상태 전달
        } else {
          Serial.printf("[%u] 토큰 인증 실패 - 접속 차단\n", num);
          webSocket.disconnect(num);
        }
        return;
      }

      // 미인증 클라이언트 요청 차단
      if (num < WEBSOCKETS_SERVER_CLIENT_MAX && !isClientAuthenticated[num]) {
        webSocket.disconnect(num);
        return;
      }

      // 1. 릴레이 제어 명령
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

          broadcastRelayState();
        }
      }
      break;
    }
    default:
      break;
  }
}

// HTML 웹페이지 생성 함수
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
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f4f6f9; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; }
    .container { background: white; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.08); padding: 25px; width: 100%; max-width: 480px; margin: 15px; }
    h1 { color: #2c3e50; text-align: center; margin-top: 0; margin-bottom: 8px; font-size: 22px; }
    .connection-status { text-align: center; font-size: 13px; margin-bottom: 20px; padding: 8px; border-radius: 6px; font-weight: 500; }
    .online { background-color: #e8f5e9; color: #2e7d32; }
    .offline { background-color: #ffebee; color: #c62828; }

    .relay-channel { margin-bottom: 15px; padding: 15px; border: 1px solid #e0e0e0; border-radius: 8px; background-color: #fafafa; }
    .channel-header { display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }
    .channel-title { font-weight: bold; font-size: 16px; color: #333; }
    .status-badge { padding: 4px 12px; border-radius: 4px; font-size: 12px; font-weight: bold; }
    .status-badge.on { background-color: #d4edda; color: #155724; }
    .status-badge.off { background-color: #f8d7da; color: #721c24; }
    
    .btn-row { display: flex; gap: 10px; }
    .btn { flex: 1; padding: 14px 5px; font-size: 14px; font-weight: bold; border: none; border-radius: 6px; cursor: pointer; transition: all 0.1s ease; text-align: center; touch-action: none; }
    
    .btn-push { background-color: #007bff; color: white; }
    .btn-push:active, .btn-push.active { background-color: #004494; transform: scale(0.97); }
    
    .btn-toggle.off { background-color: #28a745; color: white; }
    .btn-toggle.on { background-color: #dc3545; color: white; }
    .btn-toggle:active { transform: scale(0.97); }

    .info { margin-top: 20px; padding-top: 15px; border-top: 1px solid #eee; font-size: 12px; color: #7f8c8d; text-align: center; line-height: 1.5; }
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

    html += "<div class='btn-row'>";
    html += "<button class='btn btn-push' id='push-btn-" + String(i) + "'>푸시 (누를 때만 ON)</button>";
    html += "<button class='btn btn-toggle off' id='toggle-btn-" + String(i) + "' onclick='handleToggleClick(" + String(i) + ")'>스위치 켜기</button>";
    html += "</div></div>";
  }

  html += R"rawliteral(
    <div class="info">
      접속 IP: 192.168.4.1<br>
      mDNS 주소: http://)rawliteral" + String(mdns_hostname) + R"rawliteral(.local
    </div>
  </div>

  <script>
    const WS_TOKEN = ")rawliteral" + authToken + R"rawliteral(";
    let ws;
    let currentStates = [false, false, false, false];

    function initWebSocket() {
      ws = new WebSocket('ws://' + window.location.hostname + ':81/');

      ws.onopen = function() {
        ws.send('AUTH:' + WS_TOKEN);
        const wsStatus = document.getElementById('ws-status');
        wsStatus.textContent = '실시간 제어 연결됨';
        wsStatus.className = 'connection-status online';
      };

      ws.onmessage = function(event) {
        try {
          const data = JSON.parse(event.data);
          if (data.type === 'relay_state') {
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
        setTimeout(initWebSocket, 1000);
      };

      ws.onerror = function(err) {
        ws.close();
      };
    }

    function renderUI() {
      for (let i = 0; i < 4; i++) {
        const state = currentStates[i];
        const statusBadge = document.getElementById('status-' + i);
        if (statusBadge) {
          statusBadge.textContent = state ? 'ON' : 'OFF';
          statusBadge.className = 'status-badge ' + (state ? 'on' : 'off');
        }
        const toggleBtn = document.getElementById('toggle-btn-' + i);
        if (toggleBtn) {
          toggleBtn.textContent = state ? '스위치 끄기' : '스위치 켜기';
          toggleBtn.className = 'btn btn-toggle ' + (state ? 'on' : 'off');
        }
      }
    }

    function sendToggle(channel, state) {
      if (ws && ws.readyState === WebSocket.OPEN) {
        ws.send('TOGGLE:' + channel + ':' + state);
      }
    }

    function handleToggleClick(channel) {
      sendToggle(channel, currentStates[channel] ? 0 : 1);
    }

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
        btn.onpointercancel = handleRelease;
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

// HTTP 기본 인증 핸들러
void handleRoot() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  server.send(200, "text/html", buildHTML());
}

void handleNotFound() {
  if (!server.authenticate(www_username, www_password)) {
    return server.requestAuthentication();
  }
  server.send(404, "text/plain", "Not Found");
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n=== ESP8266 4채널 릴레이 시스템 시작 ===");

  // 보안 토큰 생성
  authToken = generateAuthToken(www_username, www_password);

  // 1. GPIO 초기화
  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOGIC_REVERSE ? HIGH : LOW);
    relayState[i] = false;
  }

  // 2. 단독 AP(SoftAP) 전용 모드 설정
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  Serial.print("SoftAP IP: ");
  Serial.println(WiFi.softAPIP());

  // 3. mDNS 시작
  if (MDNS.begin(mdns_hostname)) {
    Serial.print("mDNS 호스트: http://");
    Serial.print(mdns_hostname);
    Serial.println(".local");
  }

  // 4. HTTP 및 웹소켓 서버 등록
  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);

  Serial.println("웹 서버 및 웹소켓 가동 완료");
}

void loop() {
  // 메인 태스크 처리 (최고의 반응 속도 유지)
  server.handleClient();
  webSocket.loop();
  MDNS.update();
}