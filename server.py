import asyncio
import winsdk.windows.devices.geolocation as wdg
from flask import Flask, jsonify

app = Flask(__name__)

current_lat = 37.5665 # 기본값 (서울 시청)
current_lon = 126.9780
status_message = "초기화 안됨"

async def get_windows_location():
    global current_lat, current_lon, status_message
    
    print("\n--- [위치 진단 시작] ---")
    
    try:
        # 1. 권한 요청 (Geolocator 생성)
        locator = wdg.Geolocator()
        
        # 2. 권한 상태 확인
        access_status = await wdg.Geolocator.request_access_async()
        
        print(f"👉 권한 상태 코드: {access_status}")
        # 0: Unspecified, 1: Allowed, 2: Denied
        
        if access_status == wdg.GeolocationAccessStatus.DENIED:
            print("❌ [오류] 윈도우 설정에서 위치 접근이 '거부'되었습니다.")
            print("   -> 설정 > 개인정보 > 위치 > '데스크톱 앱 허용'을 켜주세요.")
            status_message = "권한 거부됨"
            return

        print("🔍 위성/Wi-Fi 신호 검색 중... (최대 10초 소요)")
        
        # 3. 위치 가져오기 (타임아웃 설정 추가)
        # 10초 동안 못 찾으면 포기
        pos = await asyncio.wait_for(locator.get_geoposition_async(), timeout=10.0)
        
        current_lat = pos.coordinate.point.position.latitude
        current_lon = pos.coordinate.point.position.longitude
        status_message = "위치 확보 성공"
        
        print(f"✅ [성공] 현재 위치: {current_lat}, {current_lon}")

    except asyncio.TimeoutError:
        print("⏰ [오류] 시간 초과! (실내라서 GPS/Wi-Fi 신호를 못 잡았습니다)")
        status_message = "시간 초과 (신호 없음)"
    except Exception as e:
        print(f"❌ [시스템 오류] {e}")
        status_message = f"시스템 오류: {str(e)}"

@app.route('/location', methods=['GET'])
def get_location():
    print(f"[요청] 상태: {status_message} -> 좌표: {current_lat}, {current_lon}")
    return jsonify({
        "lat": current_lat,
        "lon": current_lon,
        "status": status_message,
        "source": "Windows Laptop"
    })

if __name__ == '__main__':
    # 비동기 루프 실행
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(get_windows_location())
    
    print("\n🚀 서버 시작 (http://0.0.0.0:5000/location)")
    app.run(host='0.0.0.0', port=5000)