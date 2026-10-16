# ESP8266 4채널 릴레이 제어 펌웨어

## 1. 문서 개요

이 문서는 `PLC_4ch_relay_control.ino`의 구조와 주요 코드 블록을 설명합니다. PDF 분석 자료인 **「ESP8266 4채널 릴레이 제어 시스템 상세 분석 문서」**의 설명을 참고하되, 실제 동작과 보안 특성은 스케치 코드에 맞춰 기술했습니다.

펌웨어는 ESP8266의 SoftAP에 접속한 브라우저에서 웹 제어판을 제공하고, HTTP 기본 인증과 WebSocket 연결을 이용해 4개 릴레이의 상태를 제어·동기화합니다.

## 2. 주요 기능 및 구성

| 영역 | 구현 |
|---|---|
| 네트워크 | ESP8266이 자체 Wi-Fi AP를 생성하는 `WIFI_AP` 모드 |
| 웹 서버 | HTTP 80번 포트, 루트 페이지와 기본 인증 처리 |
| 실시간 통신 | WebSocket 81번 포트 |
| 주소 검색 | mDNS 호스트 `esp-relay` |
| 릴레이 출력 | GPIO 5, 4, 0, 2를 사용하는 Active Low 기본 설정 |
| 브라우저 UI | 4개 채널의 푸시 버튼 및 래칭 토글 버튼 |

## 3. 라이브러리와 설정값

### 3.1 라이브러리 포함

```cpp
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <WebSocketsServer.h>
#include <ESP8266mDNS.h>
```

- `ESP8266WiFi.h`: SoftAP 네트워크 설정 및 IP 주소 확인
- `ESP8266WebServer.h`: HTTP 서버 및 인증 제공
- `WebSocketsServer.h`: 실시간 WebSocket 서버와 클라이언트 이벤트 처리
- `ESP8266mDNS.h`: 로컬 네트워크에서 호스트 이름으로 장치를 찾도록 함

Arduino IDE에서 ESP8266 보드 지원 패키지와 WebSockets 라이브러리를 설치해야 합니다.

### 3.2 사용자 설정 및 전역 상태

```cpp
const char* www_username = "admin";
const char* www_password = "team_B_261023_kopo!";

const char* ap_ssid = "ESP_Relay_Control";
const char* ap_password = "12345678";
const char* mdns_hostname = "esp-relay";

const int relayPins[4] = {5, 4, 0, 2};
const bool LOGIC_REVERSE = true;

bool relayState[4] = {false, false, false, false};
String authToken = "";
bool isClientAuthenticated[WEBSOCKETS_SERVER_CLIENT_MAX] = {false};

ESP8266WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81);
```

`relayPins`의 순서가 웹 UI의 채널 순서와 일치합니다. 배열 인덱스 `0..3`은 화면에 표시되는 채널 `1..4`에 각각 대응합니다.

| 화면 채널 | 배열 인덱스 | ESP8266 핀 | GPIO |
|---:|---:|---|---:|
| 1 | 0 | D1 | 5 |
| 2 | 1 | D2 | 4 |
| 3 | 2 | D3 | 0 |
| 4 | 3 | D4 | 2 |

`relayState`는 프로그램이 관리하는 ON/OFF 상태이며 실제 출력 레벨과 구분됩니다. `LOGIC_REVERSE == true`이면 릴레이 ON은 `LOW`, OFF는 `HIGH`로 출력됩니다.

## 4. WebSocket 인증 토큰과 상태 동기화

### 4.1 토큰 생성

```cpp
String generateAuthToken(const char* user, const char* pass) {
  String raw = String(user) + ":" + String(pass) + "_SECRET_SALT";
  uint32_t hash = 5381;
  for (size_t i = 0; i < raw.length(); i++) {
    hash = ((hash << 5) + hash) + raw[i];
  }
  return String(hash, HEX);
}
```

계정 문자열과 고정 문자열 `_SECRET_SALT`를 결합한 후 DJB2 방식의 32비트 해시를 계산합니다. 반환된 토큰은 브라우저 페이지에 삽입되고, WebSocket 연결 후 클라이언트가 `AUTH:<토큰>` 메시지로 보냅니다.

> **보안 주의:** DJB2는 암호학적 해시나 안전한 인증 프로토콜이 아닙니다. 토큰은 브라우저에 제공되며, 본 펌웨어는 HTTPS/WSS를 사용하지 않습니다. 따라서 이 토큰을 강한 보안 경계로 간주하면 안 됩니다. 신뢰할 수 있는 로컬 네트워크에서만 사용하고, 실제 배포 전에는 기본 자격 증명을 변경하며 네트워크 접근을 제한하십시오.

### 4.2 릴레이 상태 브로드캐스트

```cpp
void broadcastRelayState() {
  String json = "{\"type\":\"relay_state\",\"states\":[";
  for (int i = 0; i < 4; i++) {
    if (i > 0) json += ",";
    json += relayState[i] ? "true" : "false";
  }
  json += "]}";

  webSocket.broadcastTXT(json);
}
```

현재 상태를 다음 형태의 JSON 텍스트로 구성해 연결된 모든 WebSocket 클라이언트에 전달합니다.

```json
{"type":"relay_state","states":[false,true,false,false]}
```

새 클라이언트가 인증되었을 때에도 호출되므로, 브라우저는 페이지가 열린 뒤 현재 상태를 받아 UI를 동기화합니다.

## 5. WebSocket 이벤트와 릴레이 제어

### 5.1 연결 상태 및 인증

`webSocketEvent()`는 연결, 해제, 텍스트 메시지를 구분해 처리합니다.

```cpp
case WStype_CONNECTED: {
  IPAddress ip = webSocket.remoteIP(num);
  Serial.printf("[%u] 클라이언트 접속 - IP: %d.%d.%d.%d\n",
                num, ip[0], ip[1], ip[2], ip[3]);
  if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
    isClientAuthenticated[num] = false;
  }
  break;
}

case WStype_DISCONNECTED:
  Serial.printf("[%u] 웹소켓 연결 해제\n", num);
  if (num < WEBSOCKETS_SERVER_CLIENT_MAX) {
    isClientAuthenticated[num] = false;
  }
  break;
```

클라이언트가 접속할 때마다 인증 상태를 초기화합니다. 연결이 해제되면 해당 클라이언트 슬롯의 인증 상태도 초기화합니다.

텍스트 이벤트에서 `AUTH:` 접두사를 확인하고, 토큰이 일치하면 인증 상태를 참으로 바꾼 뒤 현재 릴레이 상태를 전송합니다. 토큰이 다르거나 인증 전에 제어 메시지를 보내면 연결을 끊습니다.

### 5.2 `TOGGLE` 명령과 GPIO 출력

```cpp
if (msg.startsWith("TOGGLE:")) {
  int firstColon = msg.indexOf(':');
  int secondColon = msg.indexOf(':', firstColon + 1);

  int channel = msg.substring(firstColon + 1, secondColon).toInt();
  int state = msg.substring(secondColon + 1).toInt();

  if (channel >= 0 && channel < 4) {
    bool relayOn = (state == 1);
    int outputLevel = LOGIC_REVERSE
        ? (relayOn ? LOW : HIGH)
        : (relayOn ? HIGH : LOW);

    digitalWrite(relayPins[channel], outputLevel);
    relayState[channel] = relayOn;
    broadcastRelayState();
  }
}
```

브라우저에서 보내는 명령은 `TOGGLE:<채널 인덱스>:<상태>` 형식입니다. 상태 `1`은 ON, 그 외 값은 OFF로 처리됩니다. 채널 범위가 `0..3`인지 확인한 뒤 GPIO를 설정하고, 메모리 상태를 갱신하며, 모든 클라이언트에 변경 상태를 전파합니다.

## 6. 웹 제어판 생성

### 6.1 HTML과 채널 UI 구성

`buildHTML()`은 HTML 문서의 정적 부분을 raw string literal로 작성하고, 반복문으로 채널별 제어 요소를 생성합니다.

```cpp
for (int i = 0; i < 4; i++) {
  html += "<div class='relay-channel'>";
  html += "<div class='channel-header'>";
  html += "<span class='channel-title'>채널 " + String(i + 1) + "</span>";
  html += "<span class='status-badge off' id='status-" + String(i) + "'>OFF</span>";
  html += "</div>";

  html += "<div class='btn-row'>";
  html += "<button class='btn btn-push' id='push-btn-" + String(i) +
          "'>푸시 (누를 때만 ON)</button>";
  html += "<button class='btn btn-toggle off' id='toggle-btn-" + String(i) +
          "' onclick='handleToggleClick(" + String(i) + ")'>스위치 켜기</button>";
  html += "</div></div>";
}
```

각 채널에는 두 종류의 입력이 있습니다.

- **푸시:** 누르고 있는 동안 ON 명령을 보내고, 손을 떼거나 포인터가 취소되면 OFF를 보냅니다.
- **스위치:** 현재 상태의 반대값을 보내 ON/OFF를 전환합니다.

### 6.2 브라우저 WebSocket 및 UI 동기화

페이지의 JavaScript는 현재 페이지 호스트의 81번 포트에 WebSocket으로 접속합니다.

```javascript
function initWebSocket() {
  ws = new WebSocket('ws://' + window.location.hostname + ':81/');

  ws.onopen = function() {
    ws.send('AUTH:' + WS_TOKEN);
    // 연결 상태 UI를 온라인으로 표시
  };

  ws.onmessage = function(event) {
    const data = JSON.parse(event.data);
    if (data.type === 'relay_state') {
      currentStates = data.states;
      renderUI();
    }
  };

  ws.onclose = function() {
    setTimeout(initWebSocket, 1000);
  };
}
```

연결이 열리면 인증 토큰을 보내고, 서버의 `relay_state` 메시지를 받아 상태 배지와 토글 버튼을 갱신합니다. 연결이 닫히면 1초 후 재접속을 시도합니다. `renderUI()`는 각 채널의 ON/OFF 텍스트와 CSS 클래스를 바꿉니다.

### 6.3 버튼 입력 전송

```javascript
function sendToggle(channel, state) {
  if (ws && ws.readyState === WebSocket.OPEN) {
    ws.send('TOGGLE:' + channel + ':' + state);
  }
}

function handleToggleClick(channel) {
  sendToggle(channel, currentStates[channel] ? 0 : 1);
}
```

토글 버튼은 브라우저가 마지막으로 수신한 `currentStates`를 기준으로 반대 상태를 요청합니다. 푸시 버튼은 포인터 이벤트에서 눌림/해제 시 각각 ON/OFF를 전송하고, `isPressed`로 중복 눌림 처리를 방지합니다.

## 7. HTTP 처리, 초기화, 메인 루프

### 7.1 페이지 인증 및 404 처리

```cpp
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
```

루트 경로에 접근하면 HTTP 기본 인증을 확인한 뒤 제어 페이지를 반환합니다. 존재하지 않는 경로도 먼저 인증을 요구합니다. 인증에 쓰이는 자격 증명은 펌웨어에 상수로 저장되어 있으므로 소스 코드 및 장치 접근을 보호해야 합니다.

### 7.2 `setup()` 초기화

```cpp
void setup() {
  Serial.begin(115200);
  authToken = generateAuthToken(www_username, www_password);

  for (int i = 0; i < 4; i++) {
    pinMode(relayPins[i], OUTPUT);
    digitalWrite(relayPins[i], LOGIC_REVERSE ? HIGH : LOW);
    relayState[i] = false;
  }

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  if (MDNS.begin(mdns_hostname)) {
    Serial.print("mDNS 호스트: http://");
    Serial.print(mdns_hostname);
    Serial.println(".local");
  }

  server.on("/", handleRoot);
  server.onNotFound(handleNotFound);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(webSocketEvent);
}
```

초기화 순서는 시리얼 로깅과 인증 토큰 생성, GPIO OFF 초기화, SoftAP 구동, mDNS 시작, HTTP/WebSocket 서버 등록 및 시작입니다. AP가 정상적으로 시작되면 기본 설정의 접속 주소는 `192.168.4.1`이며, 지원되는 클라이언트에서는 `http://esp-relay.local`을 사용할 수 있습니다.

### 7.3 `loop()` 이벤트 처리

```cpp
void loop() {
  server.handleClient();
  webSocket.loop();
  MDNS.update();
}
```

메인 루프에서 HTTP 요청, WebSocket 이벤트, mDNS 갱신을 계속 처리합니다. 별도의 장시간 `delay()` 없이 네트워크 이벤트를 자주 처리하는 구조입니다.

## 8. 빌드 및 사용 절차

1. Arduino IDE에서 ESP8266 보드 지원과 `arduinoWebSockets` 라이브러리를 설치합니다.
2. 보드 설정에서 실제 ESP8266 보드 모델과 포트를 선택합니다.
3. 소스의 관리자 계정, AP SSID/비밀번호, mDNS 이름 및 핀 매핑을 설치 환경에 맞게 변경합니다.
4. 펌웨어를 업로드하고 시리얼 모니터를 `115200 baud`로 열어 시작 로그를 확인합니다.
5. PC 또는 모바일에서 `ESP_Relay_Control` AP에 접속합니다.
6. 브라우저로 `http://192.168.4.1`에 접속하고 HTTP 기본 인증 계정으로 로그인합니다.
7. 제어 페이지에서 채널 푸시/토글 동작과 여러 브라우저 간 상태 동기화를 확인합니다.

## 9. 안전 및 유지보수 주의사항

- 현재 AP 비밀번호와 관리자 비밀번호는 소스 코드에 기본값으로 들어 있습니다. 실제 운용 전에 변경하고, 공개 저장소에 실제 운영 자격 증명을 올리지 마십시오.
- HTTP와 WebSocket 주소는 각각 `http://`, `ws://`를 사용합니다. 통신은 암호화되지 않으므로 신뢰할 수 없는 네트워크에 노출하지 마십시오.
- DJB2 해시 토큰은 암호학적으로 안전한 인증 수단이 아닙니다. 인터넷에 직접 노출하거나 보안 요구가 높은 환경에서 사용하지 마십시오.
- GPIO0과 GPIO2는 ESP8266 부팅 모드에 영향을 줄 수 있는 스트랩 핀입니다. 릴레이 모듈의 입력 회로나 외부 배선이 리셋/부팅 시 핀 레벨에 영향을 주지 않는지 실제 하드웨어에서 확인하십시오.
- 이 펌웨어는 `relayState`를 RAM에서 관리하며 재부팅 후 상태를 복원하지 않습니다. 부팅 시 설정된 출력은 모든 채널 OFF입니다.
- 릴레이 접점으로 AC 220V를 다루는 경우 감전·화재 위험이 있습니다. 정격에 맞는 릴레이/절연/퓨즈/배선과 적절한 외함을 사용하고, 전기 작업은 자격을 갖춘 사람이 수행해야 합니다.

## 10. 참고 자료

- 펌웨어: [`PLC_4ch_relay_control.ino`](../PLC_4ch_relay_control/PLC_4ch_relay_control.ino)
- 분석 자료: `ESP8266 4채널 릴레이 제어 시스템 상세 분석 문서.pdf` (사용자 제공 참고 문서)
- 발표용 시뮬레이터: [`relay-control-demo.html`](../relay-control-demo.html)
- 시뮬레이터 인증 서버: [`demo_server.py`](../demo_server.py)

### 발표용 시뮬레이터 실행

`Ardu` 폴더에서 `python demo_server.py`를 실행한 다음 브라우저에서 `http://127.0.0.1:8080/`에 접속합니다. 실제 펌웨어와 같은 HTTP Basic 인증 창이 표시됩니다. 기본 데모 계정은 `admin` / `team_B_261023_kopo!`입니다. 서버를 종료하려면 실행 중인 터미널에서 `Ctrl+C`를 누릅니다.

데모 서버는 로컬 컴퓨터(`127.0.0.1`)에서만 수신하며, 릴레이 상태는 브라우저 내부에서만 시뮬레이션합니다. 로그인 창은 HTTP 인증 대화상자이므로, 같은 브라우저에서 이미 인증에 성공했다면 창을 다시 띄우기 전에 브라우저를 닫거나 새 비공개 창을 사용해야 할 수 있습니다.
