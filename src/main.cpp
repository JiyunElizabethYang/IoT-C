#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>
#include <stdio.h>
#include <pgmspace.h>
#include <float.h>

#include "location.h"

// =============================
// OLED 설정
// =============================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// 메인 OLED (0x3C)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);
// 그래프 OLED (0x3D)
Adafruit_SSD1306 graphDisplay(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// =============================
// LED PIN 설정
// =============================
#define LED_RED     14
#define LED_YELLOW  12
#define LED_BLUE    27

// =============================
// 버튼 PIN (그래프 모드 전환)
// =============================
#define BTN_PIN 33
int graphMode = 0;  // 0=temp, 1=humid, 2=rain, 3=wind

// =============================
// 버튼 PIN (현재 위치)
// =============================
#define BUTTON_PIN 15
int lastButtonState = HIGH; // (INPUT_PULLUP이므로 기본이 HIGH)
int debouncedButtonState = HIGH;   // 디바운싱이 완료된 실제 버튼 상태
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; // 채터링 방지 (50ms)

// =============================
// 최근 12시간 그래프 데이터
// =============================
float tempHistory[12];
float humidHistory[12];
float rainHistory[12];
float windHistory[12];

// -----------------------------
// WiFi, API 설정
// -----------------------------
const char* ssid     = "Wokwi-GUEST";
const char* password = "";
const char* host    = "https://apihub.kma.go.kr";
const char* authKey = "wLWQLTOfRxC1kC0zn7cQ2g";

const long  gmtOffset_sec      = 9 * 3600;
const int   daylightOffset_sec = 0;
const char* ntpServer          = "pool.ntp.org";

// Local Server URL (필요시 IP 수정)
String serverUrl = "http://172.16.81.23:5000/location";

// =============================
// 기상청 격자 변환 상수 및 정의
// =============================
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define RE 6371.00877 // 지구 반경(km)
#define GRID 5.0      // 격자 간격(km)
#define SLAT1 30.0    // 투영 위도1(degree)
#define SLAT2 60.0    // 투영 위도2(degree)
#define OLON 126.0    // 기준점 경도(degree)
#define OLAT 38.0     // 기준점 위도(degree)
#define XO 43         // 기준점 X좌표(GRID)
#define YO 136        // 기준점 Y좌표(GRID)
#define DEGRAD (M_PI / 180.0)

// X, Y 좌표를 담을 구조체 정의
typedef struct {
    int x;
    int y;
} GridPoint;

// 좌표 (초기값: 서울)
int nx = 60;
int ny = 127;

// 현재 nx,ny에 대응하는 지역 이름 (OLED 표시용)
String currentLocationName = "";

float myLat;
float myLon;

// =============================
// 함수 프로토타입 선언
// =============================
bool extractWeather(const String&, float&, float&, float&, float&, float&);
void applyOutputs(float, float, float, float, float);
void getWeatherHistory12h();
void drawGraph();
GridPoint getLocation();
GridPoint changeToXY(double lat, double lon);
bool findXYByLocation(const char* inputName, int* outX, int* outY);
bool findLocationNameByXY(int gx, int gy, String &outName);
double getDistanceSquared(double lat1, double lon1, double lat2, double lon2);
const char* findNearestRegion(int inputX, int inputY, double currentLat, double currentLon);

// =============================
// Setup
// =============================
void setup() {
  Serial.begin(115200);
  delay(500);

  Wire.begin(21, 22);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)){
    Serial.println("Main OLED FAIL");
    while(1);
  }
  display.clearDisplay();
  display.display();

  if(!graphDisplay.begin(SSD1306_SWITCHCAPVCC, 0x3D)){
    Serial.println("Graph OLED FAIL");
    while(1);
  }
  graphDisplay.clearDisplay();
  graphDisplay.display();

  pinMode(LED_RED, OUTPUT);
  pinMode(LED_YELLOW, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  pinMode(BTN_PIN, INPUT_PULLUP);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  for (int i = 0; i < 12; i++){
    tempHistory[i]  = NAN;
    humidHistory[i] = NAN;
    rainHistory[i]  = NAN;
    windHistory[i]  = NAN;
  }

  Serial.println("=== ESP32 + KMA Weather (12h graph) ===");
  Serial.println("Enter grid nx ny (ex: 60 127) or Location Name.");

  WiFi.begin(ssid, password);
  Serial.print("Connecting WiFi");
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi connected!");

  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  Serial.println("Syncing time...");
  struct tm ti;
  while(!getLocalTime(&ti)){
    Serial.println("  retry...");
    delay(300);
  }
  Serial.println("Time synced.");
}

// =============================
// Loop
// =============================
void loop(){
  // 1. 현재 버튼 핀의 상태를 읽습니다.
  int reading = digitalRead(BUTTON_PIN);

  // 2. 핀 상태가 변했는지 확인 (노이즈 또는 누름 시작)
  if (reading != lastButtonState) {
    lastDebounceTime = millis(); // 타이머 리셋
  }

  // 3. 디바운싱 시간(50ms)이 지났다면, 실제 상태로 인정
  if ((millis() - lastDebounceTime) > debounceDelay) {
    
    // 기존에 알고 있던 상태와 현재 안정된 상태가 다르다면? (상태 변화 발생)
    if (reading != debouncedButtonState) {
      debouncedButtonState = reading; // 상태 업데이트

      // 4. 변화된 상태가 'LOW(눌림)'라면 기능 실행
      if (debouncedButtonState == LOW) {
        Serial.println("\n[Button Clicked] Requesting Location...");
        GridPoint point = getLocation(); 
        
        nx = point.x;
        ny = point.y;
        Serial.print("\n📌 New Grid -> ");
        Serial.print(nx);
        Serial.print(", ");
        Serial.println(ny);

        // GridPoint는 getLocation 내부에서 이미 findNearestRegion을 통해 
        // currentLocationName을 업데이트 했을 수도 있지만, 안전을 위해 확인
        if (currentLocationName == "") {
             String locName;
             if (findLocationNameByXY(nx, ny, locName)){
                currentLocationName = locName;
             }
        }
        
        getWeatherHistory12h();
      }
    }
  }

  // 다음 루프를 위해 현재 핀 값을 저장
  lastButtonState = reading;

  // ===== 시리얼 입력 처리 (좌표 or 지역명) =====
  if (Serial.available()){
    String line = Serial.readStringUntil('\n');
    line.trim();
    if(line.length() > 0){
      Serial.print("\n[입력] ");
      Serial.println(line);

      bool handled = false;

      // 1) "숫자 숫자" 패턴인지 먼저 검사
      int spaceIdx = line.indexOf(' ');
      if(spaceIdx > 0){
        String part1 = line.substring(0, spaceIdx);
        String part2 = line.substring(spaceIdx + 1);
        part2.trim();

        bool p1Numeric = true;
        bool p2Numeric = true;

        for (int i = 0; i < part1.length(); i++){
          if(!isDigit((unsigned char)part1[i])){
            p1Numeric = false;
            break;
          }
        }
        for (int i = 0; i < part2.length(); i++){
          if(!isDigit((unsigned char)part2[i])){
            p2Numeric = false;
            break;
          }
        }

        if(p1Numeric && p2Numeric){
          int newX = part1.toInt();
          int newY = part2.toInt();
          if (newX > 0 && newY > 0){
            nx = newX;
            ny = newY;

            // 좌표 → 지역명 찾아서 currentLocationName 갱신 (있으면)
            String locName;
            if (findLocationNameByXY(nx, ny, locName)){
              currentLocationName = locName;
            } else {
              currentLocationName = "";
            }

            Serial.print("📌 New Grid (XY) -> ");
            Serial.print(nx);
            Serial.print(", ");
            Serial.println(ny);
            getWeatherHistory12h();
            handled = true;
          }
        }
      }

      // 2) 숫자 좌표가 아니면 "지역 이름"으로 검색 (findXYByLocation 사용)
      if(!handled){
        int gx, gy;
        if (findXYByLocation(line.c_str(), &gx, &gy)) {
          nx = gx;
          ny = gy;
          currentLocationName = line;   // OLED에 그대로 표시
          Serial.print("📌 New Grid (지역명) -> ");
          Serial.print(nx);
          Serial.print(", ");
          Serial.println(ny);
          getWeatherHistory12h();
        } else {
          Serial.println("⚠ 지역 이름을 찾을 수 없습니다. (location.h 내용과 정확히 동일하게 입력)");
        }
      }
    }
  }

  // 버튼 - 그래프 모드 전환
  static int lastBtn = HIGH;
  int nowBtn = digitalRead(BTN_PIN);

  if(lastBtn == HIGH && nowBtn == LOW){
    graphMode = (graphMode + 1) % 4;
    Serial.print("Graph mode -> ");
    if      (graphMode == 0) Serial.println("Temperature");
    else if (graphMode == 1) Serial.println("Humidity");
    else if (graphMode == 2) Serial.println("Rain (log)");
    else                     Serial.println("Wind");
    drawGraph();
    delay(250);
  }

  lastBtn = nowBtn;
}

// =============================
// 12시간 데이터 가져오기
// =============================
void getWeatherHistory12h() {
  if (WiFi.status() != WL_CONNECTED) return;

  struct tm ti;
  getLocalTime(&ti);
  time_t now = mktime(&ti);

  now -= 7 * 60;  // 7분 전 기준 (기상청 업데이트 딜레이 고려)

  Serial.println("\n=== Fetch 12h history ===");

  // 1) 현재(보정된 now) 먼저 가져와서 LED+OLED 갱신
  {
    struct tm* bt = localtime(&now);

    char baseDate[9];
    sprintf(baseDate, "%04d%02d%02d",
            bt->tm_year + 1900,
            bt->tm_mon + 1,
            bt->tm_mday);

    // 7분 보정된 시각의 '시'만 사용해서 정각(HH00)으로 요청
    char baseTime[5];
    sprintf(baseTime, "%02d00", bt->tm_hour);

    String url = String(host) +
      "/api/typ02/openApi/VilageFcstInfoService_2.0/getUltraSrtNcst"
      "?authKey=" + authKey +
      "&dataType=JSON"
      "&numOfRows=60"
      "&pageNo=1"
      "&base_date=" + baseDate +
      "&base_time=" + baseTime +
      "&nx=" + nx +
      "&ny=" + ny;

    Serial.println("[Now] URL:");
    Serial.println(url);

    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;
    if (http.begin(client, url)) {
      int code = http.GET();
      Serial.print("  HTTP code: ");
      Serial.println(code);
      if (code == 200) {
        String js = http.getString();
        float T, H, RN, W, VEC;
        if (extractWeather(js, T, H, RN, W, VEC)) {
          Serial.print("  Now T=");  Serial.print(T);
          Serial.print("C, H=");     Serial.print(H);
          Serial.print("%, RN=");    Serial.print(RN);
          Serial.print("mm, W=");    Serial.print(W);
          Serial.print("m/s, VEC=");
          Serial.println(VEC);

          applyOutputs(T, H, RN, W, VEC);

          tempHistory[11]  = T;
          humidHistory[11] = H;
          rainHistory[11]  = RN;
          windHistory[11]  = W;
        }
      }
      http.end();
    }
  }

  // 2) 나머지 11시간 채우기 (과거 데이터)
  for (int i = 0; i < 11; i++) {
    // now(보정된 기준시간)에서 i시간 전
    time_t t = now - (11 - i) * 3600;
    struct tm* bt = localtime(&t);

    char baseDate[9];
    sprintf(baseDate, "%04d%02d%02d",
            bt->tm_year + 1900,
            bt->tm_mon + 1,
            bt->tm_mday);

    char baseTime[5];
    sprintf(baseTime, "%02d00", bt->tm_hour);

    String url = String(host) +
      "/api/typ02/openApi/VilageFcstInfoService_2.0/getUltraSrtNcst"
      "?authKey=" + authKey +
      "&dataType=JSON"
      "&numOfRows=60"
      "&pageNo=1"
      "&base_date=" + baseDate +
      "&base_time=" + baseTime +
      "&nx=" + nx +
      "&ny=" + ny;

    Serial.print("["); Serial.print(i); Serial.print("] ");
    // Serial.print(baseDate); Serial.print(" ");
    // Serial.print(baseTime); Serial.println(" URL:");
    // Serial.println(url);

    WiFiClientSecure client; client.setInsecure();
    HTTPClient http;

    if (http.begin(client, url)) {
      int code = http.GET();
      // Serial.print("  HTTP code: ");
      // Serial.println(code);
      if (code == 200) {
        String js = http.getString();
        float T, H, RN, W, VEC;
        if (extractWeather(js, T, H, RN, W, VEC)) {
          tempHistory[i]  = T;
          humidHistory[i] = H;
          rainHistory[i]  = RN;
          windHistory[i]  = W;

          Serial.print("  -> T=");  Serial.print(T);
          Serial.print("C, H=");     Serial.print(H);
          Serial.println("%");
        }
      }
      http.end();
    }
    // API 호출 간격
    delay(200);
  }

  // 그래프 갱신
  drawGraph();
}


// =============================
// JSON 파싱
// =============================
bool extractWeather(const String& json,
                    float &T1H, float &REH,
                    float &RN1, float &WSD, float &VEC){

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json);
  if(err){
    Serial.print("JSON error: ");
    Serial.println(err.c_str());
    return false;
  }

  JsonArray arr = doc["response"]["body"]["items"]["item"].as<JsonArray>();
  if(arr.isNull()) return false;

  T1H=REH=RN1=WSD=VEC=NAN;

  for(JsonObject o : arr){
    const char* cat = o["category"];
    float v = o["obsrValue"].as<float>();

    if(strcmp(cat,"T1H")==0) T1H=v;
    if(strcmp(cat,"REH")==0) REH=v;
    if(strcmp(cat,"RN1")==0) RN1=v;
    if(strcmp(cat,"WSD")==0) WSD=v;
    if(strcmp(cat,"VEC")==0) VEC=v;
  }
  return !isnan(T1H) && !isnan(REH);
}

// =============================
// LED + Main OLED
// =============================
void applyOutputs(float T, float H, float RN, float W, float VEC){
  // LED
  if(RN > 0){
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_YELLOW, LOW);
    Serial.println("LED: Rainy (Blue)");
  }
  else if(W >= 3.5){
    digitalWrite(LED_YELLOW, HIGH);
    digitalWrite(LED_RED, LOW);
    digitalWrite(LED_BLUE, LOW);
    Serial.println("LED: Windy (Yellow)");
  }
  else if(RN==0 && H<=50){
    digitalWrite(LED_RED, HIGH);
    digitalWrite(LED_YELLOW, LOW);
    digitalWrite(LED_BLUE, LOW);
    Serial.println("LED: Sunny (Red)");
  }
  else{
    digitalWrite(LED_RED,LOW);
    digitalWrite(LED_YELLOW,LOW);
    digitalWrite(LED_BLUE,LOW);
    Serial.println("LED: OFF");
  }

  // 메인 OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0,0);
  if(currentLocationName.length() > 0){
    // 너무 길면 잘라서 표시
    String line = currentLocationName;
    // 한글 등 멀티바이트 문자 처리 주의 필요하지만 단순 길이 체크로
    if(line.length() > 20){
      line = line.substring(0, 20) + "..";
    }
    display.println(line);
  } else {
    display.printf("Grid %d,%d\n", nx, ny);
  }

  display.printf("Temp : %.1f C\n", T);
  display.printf("Humid: %.0f %%\n", H);
  display.printf("Rain : %.1f mm\n", RN);
  display.printf("Wind : %.1f m/s\n", W);

  struct tm ti;
  getLocalTime(&ti);
  display.printf("Time : %02d:%02d\n", ti.tm_hour, ti.tm_min);

  display.display();

  Serial.println("=== Latest Weather Applied ===");
}

// =============================
// 그래프 표시
// =============================
void drawGraph(){
  graphDisplay.clearDisplay();
  graphDisplay.setTextSize(1);
  graphDisplay.setTextColor(SSD1306_WHITE);

  float* src;
  const char* title;

  switch(graphMode){
    case 0: src=tempHistory;  title="Temp (C)";       break;
    case 1: src=humidHistory; title="Humid (%)";      break;
    case 2: src=rainHistory;  title="Rain (mm,log)";  break;
    default: src=windHistory; title="Wind (m/s)";     break;
  }

  // 유효 확인
  bool ok=false;
  for(int i=0;i<12;i++){ if(!isnan(src[i])){ ok=true; break; } }
  if(!ok){
    graphDisplay.setCursor(0,20);
    graphDisplay.print("No data");
    graphDisplay.display();
    return;
  }

  // 원래 값 min/max
  float minO=9999,maxO=-9999;
  for(int i=0;i<12;i++){
    if(!isnan(src[i])){
      if(src[i] < minO) minO = src[i];
      if(src[i] > maxO) maxO = src[i];
    }
  }
  if(minO==maxO) maxO=minO+1;

  // y축용 맵값 (rain은 log scale)
  float ymap[12];
  for(int i=0;i<12;i++){
    if(isnan(src[i])){ ymap[i]=NAN; continue; }
    if(graphMode==2)
      ymap[i]=log10f(1+max(0.0f,src[i]));
    else
      ymap[i]=src[i];
  }

  float minV=9999,maxV=-9999;
  for(int i=0;i<12;i++){
    if(!isnan(ymap[i])){
      if(ymap[i] < minV) minV = ymap[i];
      if(ymap[i] > maxV) maxV = ymap[i];
    }
  }
  if(minV==maxV) maxV=minV+1;

  // 제목 + min/max
  graphDisplay.setCursor(0,0);
  graphDisplay.print(title);

  graphDisplay.setCursor(0,10);
  graphDisplay.printf("min %.1f max %.1f",minO,maxO);

  // 그래프 좌표
  int gTop=18, gBot=48;
  int gHeight = gBot - gTop;
  int gLeft=8, gRight=120;
  float stepX=(float)(gRight-gLeft)/11.0f;

  int lastX=-1,lastY=-1;

  for(int i=0;i<12;i++){
    if(isnan(ymap[i])) continue;

    int x = gLeft + round(stepX*i);
    int y = gBot - round((ymap[i]-minV)*gHeight/(maxV-minV));

    // 점
    graphDisplay.fillRect(x-1,y-1,3,3,SSD1306_WHITE);

    if(lastX>=0){
      graphDisplay.drawLine(lastX,lastY,x,y,SSD1306_WHITE);
    }

    lastX=x; lastY=y;
  }

  // 시간 축 (실제 시각 표시)
  struct tm ti;
  getLocalTime(&ti);
  int nowH=ti.tm_hour;

  int idxs[5]={0,3,6,9,11};

  for(int k=0;k<5;k++){
    int idx = idxs[k];
    int hour = (nowH - (11 - idx) + 24) % 24;

    int x = gLeft + round(stepX*idx);

    // 축 눈금
    graphDisplay.drawFastVLine(x,gBot+1,3,SSD1306_WHITE);

    char buf[4];
    sprintf(buf,"%02d",hour);

    int labelX = x-6;
    if(labelX < 0) labelX = 0;
    int labelY = gBot+4;   // y=52 근처

    graphDisplay.setCursor(labelX,labelY);
    graphDisplay.print(buf);
  }

  graphDisplay.display();
}

// =======================================================
// [중요] 좌표 가져오는 함수 (위치 찾기 핵심 로직)
// =======================================================
GridPoint getLocation() {
  GridPoint point = {0, 0};
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    http.begin(serverUrl);
    int httpCode = http.GET();
    
    if (httpCode > 0) {
      String payload = http.getString();
      
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, payload);

      if (!error) {
        float lat = doc["lat"];
        float lon = doc["lon"];
        
        // 1. 위경도를 기상청 격자(X, Y)로 변환
        point = changeToXY(lat, lon);
        
        // 2. location.h에서 최적 지역(동 이름) 검색 (New Algorithm)
        const char* regionName = findNearestRegion(point.x, point.y, lat, lon);
        
        if (regionName != NULL) {
            currentLocationName = String(regionName);
        } else {
            currentLocationName = "Unknown Loc";
        }

        // 3. 결과 시리얼 모니터 출력
        Serial.println("=====================================");
        Serial.println("       CURRENT LOCATION (IP BASED)   ");
        Serial.println("=====================================");
        Serial.print(" Lat (GPS) : "); Serial.println(lat, 6);
        Serial.print(" Lon (GPS) : "); Serial.println(lon, 6);
        Serial.print(" Grid X    : "); Serial.println(point.x);
        Serial.print(" Grid Y    : "); Serial.println(point.y);
        Serial.print(" Location  : "); Serial.println(currentLocationName);
        Serial.println("=====================================");
        
      } else {
        Serial.print("[Error] JSON Parsing failed: ");
        Serial.println(error.c_str());
      }
    } else {
      Serial.print("[Error] HTTP GET failed, code: ");
      Serial.println(httpCode);
    }
    http.end(); 
  } else {
    Serial.println("[Error] WiFi Disconnected");
  }
  return point;
}

// 위도, 경도를 입력받아 격자 X, Y를 반환하는 함수
GridPoint changeToXY(double lat, double lon) {
    GridPoint point;
    
    double re = RE / GRID;
    double slat1 = SLAT1 * DEGRAD;
    double slat2 = SLAT2 * DEGRAD;
    double olon = OLON * DEGRAD;
    double olat = OLAT * DEGRAD;

    double sn = tan(M_PI * 0.25 + slat2 * 0.5) / tan(M_PI * 0.25 + slat1 * 0.5);
    sn = log(cos(slat1) / cos(slat2)) / log(sn);
    
    double sf = tan(M_PI * 0.25 + slat1 * 0.5);
    sf = pow(sf, sn) * cos(slat1) / sn;
    
    double ro = tan(M_PI * 0.25 + olat * 0.5);
    ro = re * sf / pow(ro, sn);

    double ra = tan(M_PI * 0.25 + lat * DEGRAD * 0.5);
    ra = re * sf / pow(ra, sn);
    
    double theta = lon * DEGRAD - olon;
    if (theta > M_PI) theta -= 2.0 * M_PI;
    if (theta < -M_PI) theta += 2.0 * M_PI;
    theta *= sn;

    point.x = (int)floor(ra * sin(theta) + XO + 0.5);
    point.y = (int)floor(ro - ra * cos(theta) + YO + 0.5);

    return point;
}

// =======================================================
// [수정됨] 이름으로 좌표 찾기 (locationNameList 사용)
// =======================================================
bool findXYByLocation(const char* inputName, int* outX, int* outY) {
  LocationName loc; 

  for (int i = 0; i < locationCount; i++) {
    // [FIX] locationName -> locationNameList (새로운 헤더 파일 변수명)
    memcpy_P(&loc, &locationNameList[i], sizeof(LocationName));

    char nameBuffer[64];
    strcpy(nameBuffer, loc.name);

    if (strcmp(nameBuffer, inputName) == 0) {
      *outX = loc.gridX;
      *outY = loc.gridY;
      return true;
    }
  }
  return false; 
}

// =======================================================
// [수정됨] 좌표로 이름 찾기 (locationNameList 사용)
// =======================================================
bool findLocationNameByXY(int gx, int gy, String &outName) {
  LocationName loc;
  for (int i = 0; i < locationCount; i++) {
    // [FIX] locationName -> locationNameList (새로운 헤더 파일 변수명)
    memcpy_P(&loc, &locationNameList[i], sizeof(LocationName));
    
    if (loc.gridX == gx && loc.gridY == gy) {
      outName = String(loc.name);
      return true;
    }
  }
  return false;
}

// =======================================================
// [최적 지역 찾기] ESP32 전용 수정 (직접 접근 방식)
// =======================================================
double getDistanceSquared(double lat1, double lon1, double lat2, double lon2) {
    double dLat = lat1 - lat2;
    double dLon = lon1 - lon2;
    return (dLat * dLat) + (dLon * dLon);
}

const char* findNearestRegion(int inputX, int inputY, double currentLat, double currentLon) {
    const char* bestMatchName = NULL;
    double minDistanceSq = DBL_MAX;
    
    for (int i = 0; i < locationCount; i++) {
        // [수정] ESP32는 pgm_read_word 등을 쓰지 않고 배열처럼 직접 읽습니다.
        // 이렇게 해야 double(8바이트) 값을 정확하게 가져올 수 있습니다.
        int16_t dataX = locationList[i].gridX;
        int16_t dataY = locationList[i].gridY;

        // 1. 내 위치 기준 앞뒤 1칸(3x3 영역)에 있는 모든 데이터를 후보로 둡니다.
        if (abs(dataX - inputX) <= 1 && abs(dataY - inputY) <= 1) {
            
            // [수정] 구조체 멤버에 직접 접근
            double centerLat = locationList[i].lat; 
            double centerLon = locationList[i].lon;

            double distSq = getDistanceSquared(centerLat, centerLon, currentLat, currentLon);

            if (distSq < minDistanceSq) {
                minDistanceSq = distSq;
                bestMatchName = locationList[i].name;
            }
        }
    }
    return bestMatchName;
}