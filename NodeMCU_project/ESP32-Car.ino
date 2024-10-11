#include <WiFi.h>
#include <ESPmDNS.h>
#include <Wire.h>
#include <MPU6050.h>
#include <ESP32Servo.h>

#define IN1 15  // Motor 1 direction
#define IN2 2
#define ENA 17  // PWM control for motor 1 speed

#define IN3 4   // Motor 2 direction
#define IN4 16
#define ENB 18  // PWM control for motor 2 speed
#define myDegToRad 0.017453292519943295769236907684886
#define myRadToDeg 57.295779513082320876798154814105
#define servoPin 14
#define trigPin 13  // ขาที่ต่อกับ Trig ของ Ultrasonic
#define echoPin 12 // ขาที่ต่อกับ Echo ของ Ultrasonic


const char* ssid = "Pumbaa-WiFi";
const char* password = "pumbaa@coe";

//const char* password = "S0935847242";

int indexOfColon;     //: separate data from socket
int xAxis;            //value XAxis from joystick 
int yAxis;            //value Yaxis from joystick
int motorSpeedA = 0;  //SpeedMotor A Left Wheel
int motorSpeedB = 0;  //SpeedMotor B Right Wheel
double currentLat = 0;  // ตัวอย่างตำแหน่งปัจจุบัน (ละติจูด)
double currentLon = 0; // ตัวอย่างตำแหน่งปัจจุบัน (ลองจิจูด)
double destLat = 0;  // พิกัดเป้าหมาย (ละติจูด)
double destLon = 0; // พิกัดเป้าหมาย (ลองจิจูด)
double bearing = 0;  // ทิศทางปัจจุบันของหุ่นยนต์
double targetBearing = 0;  // ทิศทางที่ต้องไปให้ถึง (คำนวณจาก GPS)
float yawAngle = 0;  // มุม Yaw ที่ได้จาก Gyroscope
float previousYaw = 0;  // เก็บค่าก่อนหน้าเพื่อการเปรียบเทียบ
int16_t gx, gy, gz;
float gxOffset = 0, gyOffset = 0, gzOffset = 0;  // ตัวแปรเก็บค่า offset
bool joyOn = false;
bool ReciveGPS = false;
bool Automode = false;
long duration;
int distance;
unsigned long previousMillis = 0;  // สำหรับจับเวลาช่วงเวลาในการคำนวณ
const long interval = 100;  // 100 ms ระหว่างการอ่านข้อมูลใหม่

MPU6050 gyro;
Servo ultrasonicServo;
WiFiServer server(80);  // Create server on port 80
WiFiClient client1,client2,client3;  // Store clients

void handleClientTask(void* param);
void reconnectWiFiTask(void* param);
void motorBackward(int speedA =255,int speedB =255);
void motorForward(int speedA =255,int speedB =255);
void turnLeft(int speedA =255,int speedB =255);
void turnRight(int speedA =255,int speedB =255);
void stopMotor();
double calculateBearing(double lat1, double lon1, double lat2, double lon2);

void setup() {
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  Wire.begin();
  gyro.initialize();

  Serial.println("Gyroscope Initialized");
  
  Serial.begin(115200);

  // Connect to Wi-Fi
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("Connecting to WiFi...");
  }

  Serial.println("WiFi connected!");

  // Start the server
  if (!MDNS.begin("esp32-server")) {
    Serial.println("Error starting mDNS");
    return;
  }

  Serial.println("mDNS responder started");

  // Start the server
  server.begin();
  Serial.println("Server started");

  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  Serial.println("Calibrating gyroscope...");
  calibrateGyro();  // เรียกใช้ฟังก์ชันหาค่า Offset

  // ตั้งขาของ Ultrasonic
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // ตั้งค่าขาเซอร์โว
  ultrasonicServo.attach(servoPin);
  ultrasonicServo.write(90);  // เริ่มต้นที่ตำแหน่งตรงกลาง

  // Start task to handle reconnecting Wi-Fi
  xTaskCreate(reconnectWiFiTask, "WiFiReconnect", 2048, NULL, 1, NULL);

  // Create tasks for handling clients
  xTaskCreate(handleClientTask, "Client1 Task", 4096, &client1, 1, NULL);
  xTaskCreate(handleClientTask, "Client2 Task", 4096, &client2, 1, NULL);
  xTaskCreate(handleClientTask, "Client3 Task", 4096, &client3, 1, NULL);
}

void loop() {
  // Main loop does nothing since tasks are running
}

void handleClientTask(void* param) {
  WiFiClient* client = (WiFiClient*)param;

  while (true) {
    // Check for new client connection
    if (!*client || !client->connected()) {
      *client = server.available();
      if (*client) {
        Serial.println("Client connected!");
      }
    }
    // Handle communication if client is connected
    if (*client && client->connected()) {
      if(Automode){
        unsigned long currentMillis = millis();
        if (currentMillis - previousMillis >= interval) {
          previousMillis = currentMillis;

          // อ่านค่า yaw จาก Gyroscope
          int16_t gx, gy, gz;
          gyro.getRotation(&gx, &gy, &gz);
          gx -= gxOffset;
          gy -= gyOffset;
          gz -= gzOffset;
          
          // คำนวณ yawAngle จากค่า gz (สำหรับช่วง ±250°/s)
          yawAngle = gz / 131.0;

          // อัปเดต bearing ปัจจุบันตาม yawAngle ที่ได้
          bearing += (yawAngle * (interval / 1000.0));  // อัปเดต bearing ตาม yaw ในช่วงเวลา 100ms
          
          if (bearing >= 360.0) bearing -= 360.0;
          if (bearing < 0.0) bearing += 360.0;

          // แสดงค่า yaw และ bearing
          Serial.print("Yaw Angle: ");
          Serial.print(yawAngle);
          Serial.print(" | Current Bearing: ");
          Serial.println(bearing);

          // เปรียบเทียบกับ Target Bearing เพื่อปรับทิศทาง
          double bearingError = targetBearing - bearing;

          if (bearingError > 5) {
            Serial.println("Turning Right to reach target bearing...");
            Serial.println(bearingError);
            turnRight(80,0);
          } else if (bearingError < -5) {
            Serial.println("Turning Left to reach target bearing...");
            Serial.println(bearingError);
            turnLeft(0,80);
          } else {
            Serial.println("Moving Forward towards target bearing...");
            Serial.println(bearingError);
            motorForward(150,150);
          }
        }
        int frontDistance = measureDistance();
        Serial.print("Front Distance: ");
        Serial.println(frontDistance);

        if (frontDistance < 30) {  // ถ้าพบสิ่งกีดขวางด้านหน้า
          Serial.println("Obstacle detected! Checking sides...");

          // หมุนไปทางซ้ายเพื่อเช็คสิ่งกีดขวางด้านซ้าย
          ultrasonicServo.write(0);  
          delay(500);  // รอให้เซอร์โวหมุนไปทางซ้าย
          int leftDistance = measureDistance();
          Serial.print("Left Distance: ");
          Serial.println(leftDistance);

          if (leftDistance < 30) {  // ถ้ามีสิ่งกีดขวางด้านซ้าย
            // หมุนไปทางขวาเพื่อเช็คสิ่งกีดขวางด้านขวา
            ultrasonicServo.write(180);
            delay(500);  // รอให้เซอร์โวหมุนไปทางขวา
            int rightDistance = measureDistance();
            Serial.print("Right Distance: ");
            Serial.println(rightDistance);

            if (rightDistance < 30) {  // ถ้าพบสิ่งกีดขวางด้านขวา
              Serial.println("Obstacles on both sides! Reversing...");

              // ถอยหลังเล็กน้อย
              motorBackward();
            } else {
              Serial.println("Right side is clear! Turning right...");
              // หมุนไปทางขวา
              turnRight();
            }
          } else {
            Serial.println("Left side is clear! Turning left...");
            // หมุนไปทางซ้าย
            turnLeft();
          }
        } else {
          Serial.println("No obstacle ahead. Moving forward...");
          // ไม่มีสิ่งกีดขวาง ดำเนินการไปข้างหน้า
          motorForward();
        }

        // คืนเซอร์โวกลับไปตรงกลาง
        ultrasonicServo.write(90);
      }
      if (client->available()) {
        String data = client->readStringUntil('\n');
        Serial.println("Received: " + data);
        //------------------------------------------------------------
        //------------------------Odroid------------------------------
        //------------------------------------------------------------
        if(data.startsWith("GPS:")){
          // Extract current latitude
          Serial.println("Success to check");
          indexOfColon = data.indexOf("currentlat=");
          currentLat = data.substring(indexOfColon + 11, data.indexOf(",", indexOfColon)).toDouble(); // 11 characters after "currentlat="

          // Extract current longitude
          indexOfColon = data.indexOf("currentlon=");
          currentLon = data.substring(indexOfColon + 11, data.indexOf(",", indexOfColon)).toDouble(); // 11 characters after "currentlon="

          // Extract destination latitude
          indexOfColon = data.indexOf("destlat=");
          destLat = data.substring(indexOfColon + 8, data.indexOf(",", indexOfColon)).toDouble(); // 8 characters after "destlat="

          // Extract destination longitude
          indexOfColon = data.indexOf("destlon=");
          destLon = data.substring(indexOfColon + 8).toDouble(); // 8 characters after "destlon="
          Serial.print("curlat = ");
          Serial.print(currentLat);
          Serial.print("curlon = ");
          Serial.print(currentLon);
          Serial.print("destlat = ");
          Serial.print(destLat);
          Serial.print("destlon = ");
          Serial.println(destLon);
          ReciveGPS = true;
        }
        //------------------------------------------------------------
        //------------------------Joy Stick---------------------------
        //------------------------------------------------------------
        if (data == "EN") {
          joyOn = true;
          Serial.println("Switched to ESP8266 only mode.");
        } else if (data == "OFF") {
          joyOn = false;
          Serial.println("Switched to multi-client mode.");
        }
        if (data == "Auto"){
          Serial.println("Auto mode on");
          Automode = true;
        }
        if (data == "Manual"){
          Serial.println("Manual Mode");
          Automode = false;
        }

        if (joyOn) {
          Serial.println("Handling data from ESP8266.");
          if (data.startsWith("X:")) {
            indexOfColon = data.indexOf(':');
            xAxis = data.substring(indexOfColon + 1).toInt();
          } else if (data.startsWith("Y:")) {
            indexOfColon = data.indexOf(':');
            yAxis = data.substring(indexOfColon + 1).toInt();
          }

          // Motor control based on joystick input
          if (yAxis < 470) {
            Serial.println("Backward");
            digitalWrite(IN1, HIGH);
            digitalWrite(IN2, LOW);
            digitalWrite(IN3, HIGH);
            digitalWrite(IN4, LOW);
            motorSpeedA = map(yAxis, 470, 0, 0, 255);
            motorSpeedB = map(yAxis, 470, 0, 0, 255);
          } else if (yAxis > 550) {
            Serial.println("Forward");
            digitalWrite(IN1, LOW);
            digitalWrite(IN2, HIGH);
            digitalWrite(IN3, LOW);
            digitalWrite(IN4, HIGH);
            motorSpeedA = map(yAxis, 550, 1023, 0, 255);
            motorSpeedB = map(yAxis, 550, 1023, 0, 255);
          } else {
            Serial.println("Stop");
            motorSpeedA = 0;
            motorSpeedB = 0;
          }

          // Turning logic
          if (xAxis < 470) {
            Serial.println("RIGHT");
            int xMapped = map(xAxis, 470, 0, 0, 255);
            motorSpeedA += xMapped;
            motorSpeedB -= xMapped;
            if (motorSpeedA < 0) motorSpeedA = 0;
            if (motorSpeedB > 255) motorSpeedB = 255;
          }
          if (xAxis > 550) {
            Serial.println("LEFT");
            int xMapped = map(xAxis, 550, 1023, 0, 255);
            motorSpeedA -= xMapped;
            motorSpeedB += xMapped;
            if (motorSpeedA > 255) motorSpeedA = 255;
            if (motorSpeedB < 0) motorSpeedB = 0;
          }

          // Apply speed control
          analogWrite(ENA, motorSpeedA);
          analogWrite(ENB, motorSpeedB);
        //------------------------------------------------------------
        //------------------------Keyboard----------------------------
        //------------------------------------------------------------
        }else{
          Serial.println("Handling data from any client.");
          if (data == "w") {
            Serial.println("W Key pressed!");
            //client->print("Forward");
            motorForward();
          } else if (data == "s") {
            Serial.println("S Key pressed!");
            //client->print("Backward");
            motorBackward();
          } else if (data == "a") {
            Serial.println("A Key pressed!");
            //client->print("TurnLeft");
            turnLeft();
          } else if (data == "d") {
            Serial.println("D Key pressed!");
            //client->print("TurnRight");
            turnRight();
          } else if(data == "p"){
            Serial.println("P Key pressed!");
            //client.print("Stop")
            stopMotor();
          }
        }
      }

      // Disconnect if client is no longer connected
      if (!client->connected()) {
        Serial.println("Client disconnected.");
        client->stop();
      }
    }

    vTaskDelay(10 / portTICK_PERIOD_MS);  // Avoid task hogging CPU
  }
}

void reconnectWiFiTask(void* param) {
  while (true) {
    if (WiFi.status() != WL_CONNECTED) {
      //Serial.println("WiFi lost connection. Reconnecting...");
      WiFi.begin(ssid, password);
      while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        //Serial.println("Reconnecting to WiFi...");
      }
      //Serial.println("WiFi reconnected!");
    }

    vTaskDelay(5000 / portTICK_PERIOD_MS);  // Recheck Wi-Fi status every 5 seconds
  }
}

double calculateBearing(double lat1, double lon1, double lat2, double lon2) {
  lat1 *= DEG_TO_RAD;
  lon1 *= DEG_TO_RAD;
  lat2 *= DEG_TO_RAD;
  lon2 *= DEG_TO_RAD;
  
  double deltaLon = lon2 - lon1;
  double y = sin(deltaLon) * cos(lat2);
  double x = cos(lat1) * sin(lat2) - sin(lat1) * cos(lat2) * cos(deltaLon);
  
  double bearing = atan2(y, x);  // Bearing in radians
  bearing = bearing * RAD_TO_DEG;  // Convert bearing to degrees
  
  if (bearing < 0) {
    bearing += 360;
  }
  
  return bearing;
}

void calibrateGyro() {
  long sumX = 0, sumY = 0, sumZ = 0;
  const int numReadings = 1000;  // จำนวนครั้งที่อ่านค่าเพื่อหาค่าเฉลี่ย

  for (int i = 0; i < numReadings; i++) {
    gyro.getRotation(&gx, &gy, &gz);
    sumX += gx;
    sumY += gy;
    sumZ += gz;
    delay(5);  // หน่วงเวลาเล็กน้อยเพื่อให้การอ่านค่ามีช่วงเวลาต่อเนื่อง
  }

  gxOffset = sumX / numReadings;
  gyOffset = sumY / numReadings;
  gzOffset = sumZ / numReadings;

  Serial.print("Gyro X Offset: "); Serial.println(gxOffset);
  Serial.print("Gyro Y Offset: "); Serial.println(gyOffset);
  Serial.print("Gyro Z Offset: "); Serial.println(gzOffset);
}

int measureDistance() {
  // ส่งสัญญาณออกจาก Trig
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  // รับสัญญาณจาก Echo และคำนวณระยะทาง
  duration = pulseIn(echoPin, HIGH);
  distance = duration * 0.034 / 2;  // ระยะทางเป็น cm
  return distance;
}

void motorBackward(int speedA,int speedB) {
  digitalWrite(IN1, HIGH);   // Set IN1 high
  digitalWrite(IN2, LOW);    // Set IN2 low
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  analogWrite(ENA, speedA);   // Set motor speed (PWM control)
  analogWrite(ENB, speedB);
}

// Function to run the motor backward at a given speed (0-1023)
void motorForward(int speedA,int speedB) {
  digitalWrite(IN1, LOW);    // Set IN1 low
  digitalWrite(IN2, HIGH);   // Set IN2 high
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  analogWrite(ENA, speedA);   // Set motor speed (PWM control)
  analogWrite(ENB, speedB);
}
// Function to turn left
void turnLeft(int speedA,int speedB) {
  // Turn off right motor and run left motor forward
  digitalWrite(IN1, LOW);    // Right motor off
  digitalWrite(IN2, LOW);    // Right motor off
  digitalWrite(IN3, LOW);   // Left motor forward
  digitalWrite(IN4, HIGH);    // Left motor forward
  analogWrite(ENA, speedA);   // Set motor speed for turning
  analogWrite(ENB, speedB); 
}

// Function to turn right
void turnRight(int speedA,int speedB) {
  // Turn off left motor and run right motor forward
  digitalWrite(IN1, LOW);   // Right motor forward
  digitalWrite(IN2, HIGH);    // Right motor forward
  digitalWrite(IN3, LOW);    // Left motor off
  digitalWrite(IN4, LOW);    // Left motor off
  analogWrite(ENA, speedA);   // Set motor speed for turning
  analogWrite(ENB, speedB); 
}

// Function to stop the motor
void stopMotor() {
  digitalWrite(IN1, LOW);    // Set IN1 low
  digitalWrite(IN2, LOW);    // Set IN2 low
  digitalWrite(IN3, LOW);    // Set IN3 low
  digitalWrite(IN4, LOW);    // Set IN4 low
  analogWrite(ENA,0);
  analogWrite(ENB,0);
}