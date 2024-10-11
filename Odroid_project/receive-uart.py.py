import serial
import requests
import json
import time

# ปรับเปลี่ยนพอร์ตสำหรับ PC
serial_port = "COM7"  # สำหรับ Windows ใช้ COM3 หรือเช็คจาก Device Manager
# สำหรับ Linux อาจใช้ '/dev/ttyUSB0' หรือ '/dev/ttyS0' แล้วแต่การเชื่อมต่อ
timeout_value = 1
baud_rate = 9600

# ใส่ Channel Access Token ตรงนี้ (จาก LINE Developer Console)
channel_access_token = 'dkraMDfqWDY1vJSNVsGlBuPmE+N5yqYI+Zzv7I8xK1+A9IfVLd0DJdSP5Ea5NRnzIPMjg8oof4Tdk6rNV9Byw7uT49FtP4GiBLksqfkdtv7+TQSrOkfiHxk0fProw1l5Z2PMTttntgAx/cIw2g5akwdB04t89/1O/w1cDnyilFU='
user_id = 'Uefd2f28e5bc2dcf94b265504336eaf10'

# Connect to Serial
ser = serial.Serial(serial_port, baud_rate, timeout=timeout_value)
time.sleep(3)  # Wait for the serial connection to initialize

# ฟังก์ชันส่งข้อความผ่าน LINE API ด้วย Push Message
def send_push_message(user_id, message):
    url = 'https://api.line.me/v2/bot/message/push'
    headers = {
        'Content-Type': 'application/json',
        'Authorization': f'Bearer {channel_access_token}'  # ใช้ Channel Access Token
    }
    data = {
        "to": user_id,
        "messages": [
            {
                "type": "text",
                "text": message
            }
        ]
    }
    
    response = requests.post(url, headers=headers, data=json.dumps(data))
    
    if response.status_code == 200:
        print("Message sent successfully!")
    else:
        print(f"Failed to send message: {response.status_code}")
        print(response.text)

# อ่านข้อมูลจาก Arduino
def read_from_arduino():
    while True:
        if ser.in_waiting > 0:  # Check if data is available to read
            response = ser.readline().decode('utf-8').strip()  # Read and decode the line
            if response:
                return response  # Return the response when valid data is received
        time.sleep(0.1)  # Small delay to prevent CPU overuse

try:
    while True:
        recv = read_from_arduino()  # Read data from Arduino
        print(f"Arduino: {recv}")

        if recv.startswith("Password:"):
            send_push_message(user_id, recv)  # ส่งข้อมูลไปยัง LINE webhook
except KeyboardInterrupt:
    print("Stop UART communication")

finally:
    ser.close()  # Close the serial connection when done
