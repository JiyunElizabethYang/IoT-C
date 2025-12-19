import asyncio
import winsdk.windows.devices.geolocation as wdg
from flask import Flask, jsonify

app = Flask(__name__)

current_lat = 37.5665  # Default value (Seoul City Hall)
current_lon = 126.9780
status_message = "Not initialized"

async def get_windows_location():
    global current_lat, current_lon, status_message
    
    print("\n--- [Location Diagnostics Start] ---")
    
    try:
        # 1. Request permission (Create Geolocator)
        locator = wdg.Geolocator()
        
        # 2. Check access status
        access_status = await wdg.Geolocator.request_access_async()
        
        print(f"👉 Access status code: {access_status}")
        # 0: Unspecified, 1: Allowed, 2: Denied
        
        if access_status == wdg.GeolocationAccessStatus.DENIED:
            print("❌ [Error] Location access is DENIED in Windows settings.")
            print("   -> Please enable: Settings > Privacy > Location > Allow desktop apps")
            status_message = "Permission denied"
            return

        print("🔍 Searching for GPS/Wi-Fi signals... (may take up to 10 seconds)")
        
        # 3. Get location (with timeout)
        # Give up if location is not acquired within 10 seconds
        pos = await asyncio.wait_for(
            locator.get_geoposition_async(),
            timeout=10.0
        )
        
        current_lat = pos.coordinate.point.position.latitude
        current_lon = pos.coordinate.point.position.longitude
        status_message = "Location acquired successfully"
        
        print(f"✅ [Success] Current location: {current_lat}, {current_lon}")

    except asyncio.TimeoutError:
        print("⏰ [Error] Timeout! (GPS/Wi-Fi signal not available, possibly indoors)")
        status_message = "Timeout (no signal)"
    except Exception as e:
        print(f"❌ [System Error] {e}")
        status_message = f"System error: {str(e)}"

@app.route('/location', methods=['GET'])
def get_location():
    print(f"[Request] Status: {status_message} -> Coordinates: {current_lat}, {current_lon}")
    return jsonify({
        "lat": current_lat,
        "lon": current_lon,
        "status": status_message,
        "source": "Windows Laptop"
    })

if __name__ == '__main__':
    # Run asynchronous event loop
    loop = asyncio.new_event_loop()
    asyncio.set_event_loop(loop)
    loop.run_until_complete(get_windows_location())
    
    print("\n🚀 Server started (http://0.0.0.0:5000/location)")
    app.run(host='0.0.0.0', port=5000)
