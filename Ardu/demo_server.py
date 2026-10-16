"""mDNS(esp-relay.local) 및 ESP8266 원본 UI/웹소켓/인증을 모사하는 파이썬 시뮬레이터 서버"""

from base64 import b64decode
from hmac import compare_digest
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import asyncio
import json
import socket
import threading
import websockets
from zeroconf import ServiceInfo, Zeroconf

# ===== 사용자 설정 (ESP8266 원본과 동일) =====
WWW_USERNAME = "admin"
WWW_PASSWORD = "team_B_261023_kopo!"
MDNS_HOSTNAME = "esp-relay"

HTTP_PORT = 80
WS_PORT = 81

# ===== 전역 상태 변수 =====
relay_states = [False, False, False, False]
connected_websockets = set()


# 보안 토큰 생성 함수 (ESP8266 원본 알고리즘 재현)
def generate_auth_token(user: str, pass_: str) -> str:
    raw = f"{user}:{pass_}_SECRET_SALT"
    hash_val = 5381
    for char in raw:
        hash_val = ((hash_val << 5) + hash_val) + ord(char)
    return format(hash_val & 0xFFFFFFFF, "x")


AUTH_TOKEN = generate_auth_token(WWW_USERNAME, WWW_PASSWORD)


# ===== ESP8266 원본 HTML 템플릿 =====
def build_html() -> str:
    return f"""<!DOCTYPE html>
<html lang="ko">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>4채널 릴레이 제어판</title>
  <style>
    * {{ -webkit-tap-highlight-color: transparent; user-select: none; box-sizing: border-box; }}
    body {{ font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background-color: #f4f6f9; margin: 0; padding: 0; display: flex; justify-content: center; align-items: center; min-height: 100vh; }}
    .container {{ background: white; border-radius: 12px; box-shadow: 0 4px 15px rgba(0,0,0,0.08); padding: 25px; width: 100%; max-width: 480px; margin: 15px; }}
    h1 {{ color: #2c3e50; text-align: center; margin-top: 0; margin-bottom: 8px; font-size: 22px; }}
    .connection-status {{ text-align: center; font-size: 13px; margin-bottom: 20px; padding: 8px; border-radius: 6px; font-weight: 500; }}
    .online {{ background-color: #e8f5e9; color: #2e7d32; }}
    .offline {{ background-color: #ffebee; color: #c62828; }}

    .relay-channel {{ margin-bottom: 15px; padding: 15px; border: 1px solid #e0e0e0; border-radius: 8px; background-color: #fafafa; }}
    .channel-header {{ display: flex; justify-content: space-between; align-items: center; margin-bottom: 12px; }}
    .channel-title {{ font-weight: bold; font-size: 16px; color: #333; }}
    .status-badge {{ padding: 4px 12px; border-radius: 4px; font-size: 12px; font-weight: bold; }}
    .status-badge.on {{ background-color: #d4edda; color: #155724; }}
    .status-badge.off {{ background-color: #f8d7da; color: #721c24; }}
    
    .btn-row {{ display: flex; gap: 10px; }}
    .btn {{ flex: 1; padding: 14px 5px; font-size: 14px; font-weight: bold; border: none; border-radius: 6px; cursor: pointer; transition: all 0.1s ease; text-align: center; touch-action: none; }}
    
    .btn-push {{ background-color: #007bff; color: white; }}
    .btn-push:active, .btn-push.active {{ background-color: #004494; transform: scale(0.97); }}
    
    .btn-toggle.off {{ background-color: #28a745; color: white; }}
    .btn-toggle.on {{ background-color: #dc3545; color: white; }}
    .btn-toggle:active {{ transform: scale(0.97); }}

    .info {{ margin-top: 20px; padding-top: 15px; border-top: 1px solid #eee; font-size: 12px; color: #7f8c8d; text-align: center; line-height: 1.5; }}
  </style>
</head>
<body>
  <div class="container">
    <h1>4채널 릴레이 제어판</h1>
    <div id="ws-status" class="connection-status offline">웹소켓 연결 중...</div>
    {"".join(f'''<div class='relay-channel'>
      <div class='channel-header'>
        <span class='channel-title'>채널 {i + 1}</span>
        <span class='status-badge off' id='status-{i}'>OFF</span>
      </div>
      <div class='btn-row'>
        <button class='btn btn-push' id='push-btn-{i}'>푸시 (누를 때만 ON)</button>
        <button class='btn btn-toggle off' id='toggle-btn-{i}' onclick='handleToggleClick({i})'>스위치 켜기</button>
      </div></div>''' for i in range(4))}
    <div class="info">
      시뮬레이션 모드 (포트: {HTTP_PORT})<br>
      mDNS 주소: http://{MDNS_HOSTNAME}.local:{HTTP_PORT}
    </div>
  </div>

  <script>
    const WS_TOKEN = "{AUTH_TOKEN}";
    let ws;
    let currentStates = [false, false, false, false];

    function initWebSocket() {{
      ws = new WebSocket('ws://' + window.location.hostname + ':{WS_PORT}/');

      ws.onopen = function() {{
        ws.send('AUTH:' + WS_TOKEN);
        const wsStatus = document.getElementById('ws-status');
        wsStatus.textContent = '실시간 제어 연결됨';
        wsStatus.className = 'connection-status online';
      }};

      ws.onmessage = function(event) {{
        try {{
          const data = JSON.parse(event.data);
          if (data.type === 'relay_state') {{
            currentStates = data.states;
            renderUI();
          }}
        }} catch (e) {{
          console.error("데이터 파싱 오류", e);
        }}
      }};

      ws.onclose = function() {{
        const wsStatus = document.getElementById('ws-status');
        wsStatus.textContent = '연결 끊김 - 재연결 시도 중...';
        wsStatus.className = 'connection-status offline';
        setTimeout(initWebSocket, 1000);
      }};

      ws.onerror = function(err) {{
        ws.close();
      }};
    }}

    function renderUI() {{
      for (let i = 0; i < 4; i++) {{
        const state = currentStates[i];
        const statusBadge = document.getElementById('status-' + i);
        if (statusBadge) {{
          statusBadge.textContent = state ? 'ON' : 'OFF';
          statusBadge.className = 'status-badge ' + (state ? 'on' : 'off');
        }}
        const toggleBtn = document.getElementById('toggle-btn-' + i);
        if (toggleBtn) {{
          toggleBtn.textContent = state ? '스위치 끄기' : '스위치 켜기';
          toggleBtn.className = 'btn btn-toggle ' + (state ? 'on' : 'off');
        }}
      }}
    }}

    function sendToggle(channel, state) {{
      if (ws && ws.readyState === WebSocket.OPEN) {{
        ws.send('TOGGLE:' + channel + ':' + state);
      }}
    }}

    function handleToggleClick(channel) {{
      sendToggle(channel, currentStates[channel] ? 0 : 1);
    }}

    function bindPushEvents() {{
      for (let i = 0; i < 4; i++) {{
        const btn = document.getElementById('push-btn-' + i);
        if (!btn) continue;
        let isPressed = false;

        const handlePress = (e) => {{
          e.preventDefault();
          if (!isPressed) {{
            isPressed = true;
            try {{ btn.setPointerCapture(e.pointerId); }} catch(err) {{}}
            btn.classList.add('active');
            sendToggle(i, 1);
          }}
        }};

        const handleRelease = (e) => {{
          e.preventDefault();
          if (isPressed) {{
            isPressed = false;
            try {{ btn.releasePointerCapture(e.pointerId); }} catch(err) {{}}
            btn.classList.remove('active');
            sendToggle(i, 0);
          }}
        }};

        btn.onpointerdown = handlePress;
        btn.onpointerup = handleRelease;
        btn.onpointercancel = handleRelease;
      }}
    }}

    window.onload = function() {{
      initWebSocket();
      bindPushEvents();
    }};
  </script>
</body>
</html>
"""


# ===== HTTP Basic 인증 및 라우팅 핸들러 =====
class SimulatorHTTPHandler(BaseHTTPRequestHandler):

    def do_GET(self):
        if self.path != "/":
            self.send_error(404, "Not Found")
            return

        auth_header = self.headers.get("Authorization")
        if auth_header:
            try:
                scheme, encoded = auth_header.split(" ", 1)
                if scheme.lower() == "basic":
                    decoded = b64decode(encoded.strip()).decode("utf-8")
                    user, pwd = decoded.split(":", 1)
                    if compare_digest(user, WWW_USERNAME) and compare_digest(
                        pwd, WWW_PASSWORD
                    ):
                        content = build_html().encode("utf-8")
                        self.send_response(200)
                        self.send_header(
                            "Content-Type", "text/html; charset=utf-8"
                        )
                        self.send_header("Connection", "close")
                        self.end_headers()
                        self.wfile.write(content)
                        return
            except Exception:
                pass

        self.send_response(401)
        self.send_header(
            "WWW-Authenticate", 'Basic realm="ESP8266 Relay Control System"'
        )
        self.send_header("Connection", "close")
        self.send_header("Content-Length", "0")
        self.end_headers()

    def log_message(self, format, *args):
        print(f"[HTTP] {self.address_string()} - {format % args}")


# ===== 웹소켓 서버 핸들러 =====
async def broadcast_state():
    if not connected_websockets:
        return
    state_json = json.dumps(
        {"type": "relay_state", "states": relay_states}, separators=(",", ":")
    )
    await asyncio.gather(
        *[client.send(state_json) for client in connected_websockets]
    )


async def websocket_handler(websocket):
    authenticated = False
    connected_websockets.add(websocket)
    try:
        async for message in websocket:
            if message.startswith("AUTH:"):
                client_token = message[5:]
                if client_token == AUTH_TOKEN:
                    authenticated = True
                    print("[WS] 토큰 인증 성공 (클라이언트 접속)")
                    await websocket.send(
                        json.dumps(
                            {
                                "type": "relay_state",
                                "states": relay_states,
                            },
                            separators=(",", ":"),
                        )
                    )
                else:
                    print("[WS] 토큰 인증 실패 - 연결 종료")
                    await websocket.close()
                    break
                continue

            if not authenticated:
                await websocket.close()
                break

            if message.startswith("TOGGLE:"):
                parts = message.split(":")
                if len(parts) == 3:
                    try:
                        channel = int(parts[1])
                        state = int(parts[2])
                        if 0 <= channel < 4:
                            relay_states[channel] = state == 1
                            print(
                                f"[릴레이 제어] 채널 {channel + 1} -> {'ON' if relay_states[channel] else 'OFF'}"
                            )
                            await broadcast_state()
                    except ValueError:
                        pass
    except websockets.exceptions.ConnectionClosed:
        pass
    finally:
        connected_websockets.remove(websocket)


# ===== mDNS 서비스 등록 (esp-relay.local) =====
def register_mdns():
    try:
        # 로컬 IP 획득
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        local_ip = s.getsockname()[0]
        s.close()
    except Exception:
        local_ip = "127.0.0.1"

    zeroconf = Zeroconf()
    info = ServiceInfo(
        "_http._tcp.local.",
        f"{MDNS_HOSTNAME}._http._tcp.local.",
        addresses=[socket.inet_aton(local_ip)],
        port=HTTP_PORT,
        properties={},
        server=f"{MDNS_HOSTNAME}.local.",
    )
    zeroconf.register_service(info)
    print(f"[mDNS 시작] http://{MDNS_HOSTNAME}.local:{HTTP_PORT} (IP: {local_ip})")
    return zeroconf


# ===== 백그라운드 서버 실행 관리 =====
def run_http_server():
    server = ThreadingHTTPServer(("0.0.0.0", HTTP_PORT), SimulatorHTTPHandler)
    server.serve_forever()


def run_websocket_server():
    async def main():
        async with websockets.serve(websocket_handler, "0.0.0.0", WS_PORT):
            await asyncio.Future()

    asyncio.run(main())


if __name__ == "__main__":
    # mDNS 등록
    zconf = register_mdns()

    # 서버 스레드 구동
    threading.Thread(target=run_http_server, daemon=True).start()
    threading.Thread(target=run_websocket_server, daemon=True).start()

    print(f"=== 시뮬레이터 서버 가동 완료 ===")
    print(f"접속 주소: http://{MDNS_HOSTNAME}.local:{HTTP_PORT}")
    try:
        threading.Event().wait()
    except KeyboardInterrupt:
        zconf.close()
        print("\n서버가 종료되었습니다.")