#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>

// ===== 사용자 설정 =====
const char* ap_ssid = "ESP_Relay_Control";
const char* ap_password = "12345678";

// D1 = GPIO5, D2 = GPIO4, D3 = GPIO0, D4 = GPIO2
const int relayPins[4] = {5, 4, 0, 2};
const bool LOGIC_REVERSE = true; // true = ACTIVE LOW (LOW 신호로 릴레이 ON)

// ===== 전역 변수 =====
bool relayState[4] = {false, false, false, false};
ESP8266WebServer server(80);

// HTML 페이지 생성
String buildHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="ko">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>4채널 릴레이 제어판</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f5f5f5; margin: 0; padding: 20px; }
    .container { max-width: 600px; margin: 0 auto; background: white; border-radius: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.1); padding: 20px; }
    h1 { color: #333; text-align: center; }
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
    <h1>4채널 릴레이 제어판</h1>
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
      IP 주소: 192.168.4.1<br>
    </div>
  </div>
  <script>
    let pollingInterval = null;
    
    function toggleRelay(channel, state) {
      // 캐시 방지를 위해 타임스탬프(t) 추가
      fetch('/relay?channel=' + channel + '&state=' + state + '&t=' + new Date().getTime())
        .then(response => {
          if (response.ok) {
            // 상태 업데이트는 polling을 통해 처리
          } else {
            alert('제어 명령 전송 실패');
          }
        })
        .catch(err => {
          console.error(err);
          alert('통신 오류가 발생했습니다.');
        });
    }
    
    function updateStatus() {
      fetch('/status?t=' + new Date().getTime())
        .then(response => response.json())
        .then(data => {
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
        })
        .catch(err => {
          console.error('상태 업데이트 오류:', err);
        });
    }
    
    // 페이지 로드 시 polling 시작
    window.onload = function() {
      updateStatus(); // 초기 상태 즉시 업데이트
      pollingInterval = setInterval(updateStatus, 1000); // 1초마다 업데이트
    };
    
    // 페이지 언로드 시 polling 중지
    window.onunload = function() {
      if (pollingInterval) {
        clearInterval(pollingInterval);
      }
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

void handleRelay() {
  if (server.hasArg("channel") && server.hasArg("state")) {
    int channel = server.arg("channel").toInt();
    int state = server.arg("state").toInt();

    if (channel >= 0 && channel < 4) {
      bool relayOn = (state == 1);
      int outputLevel = LOGIC_REVERSE ? (relayOn ? LOW : HIGH) : (relayOn ? HIGH : LOW);

      digitalWrite(relayPins[channel], outputLevel);
      relayState[channel] = relayOn;

      Serial.printf("채널 %d: %s (핀: %d, 출력: %d)\n",
                    channel + 1, relayOn ? "ON" : "OFF", relayPins[channel], outputLevel);
    }
  }
  // AJAX 요청에 대해 200 OK 응답 반환
  server.send(200, "text/plain", "OK");
}

// 상태 정보 제공 엔드포인트
void handleStatus() {
  String json = "{";
  json += "\"states\":[";
  for (int i = 0; i < 4; i++) {
    if (i > 0) json += ",";
    json += relayState[i] ? "true" : "false";
  }
  json += "]}";
  server.send(200, "application/json", json);
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
  server.on("/relay", HTTP_GET, handleRelay);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP 서버 시작됨");
}

void loop() {
  server.handleClient();
}