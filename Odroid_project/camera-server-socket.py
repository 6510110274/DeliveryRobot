import socket
import cv2
import pickle
import struct

# ตั้งค่า TCP socket สำหรับฝั่งไคลเอนต์
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
host_ip = '192.168.2.140'  # IP ของ Odroid C4
port = 8888

client_socket.connect((host_ip, port))

data = b""
payload_size = struct.calcsize("Q")  # ขนาดของ header

while True:
    # รอรับข้อมูลจากเซิร์ฟเวอร์จนได้ขนาด payload_size
    while len(data) < payload_size:
        packet = client_socket.recv(4*1024)  # รับข้อมูลจาก socket
        if not packet:
            break
        data += packet
    
    # แยกขนาดข้อมูลของเฟรม
    packed_msg_size = data[:payload_size]
    data = data[payload_size:]
    msg_size = struct.unpack("Q", packed_msg_size)[0]
    
    # รับข้อมูลเฟรมตามขนาดที่ได้
    while len(data) < msg_size:
        data += client_socket.recv(4*1024)
    
    # ดึงข้อมูลของเฟรมภาพจาก data
    frame_data = data[:msg_size]
    data = data[msg_size:]
    
    # Deserialize และแปลงข้อมูลกลับเป็นเฟรมด้วย pickle และ cv2.imdecode
    frame = pickle.loads(frame_data)
    frame = cv2.imdecode(frame, cv2.IMREAD_COLOR)
    
    # แสดงเฟรมภาพ
    cv2.imshow("Receiving", frame)
    
    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

client_socket.close()
