#include <LiquidCrystal_I2C.h>
#include <Keypad_I2C.h>
#include <Keypad.h>
#include <Wire.h>
#include <SPI.h>
#include <Servo.h>
#include <MFRC522.h>
#include <EEPROM.h>  // นำเข้าไลบรารี EEPROM มาตรฐาน

#define I2CADDR 0x20
#define RELAY_PIN 8 // RELAY ELECTRIC LOCK ACTIVE LOW
#define SS_PIN 10
#define RST_PIN 9
#define SERVO_PIN 3
#define Adminpass "22714"
#define TIMER_1SEC 15624 // TIMER COUNTER 1 SECOND INTERRUPT
const byte ROWS = 4;
const byte COLS = 4;

//--------------Declare Function--------------
void open_box();
void lock_box();
void generatepassword();
void send_password(String correct_password);
bool check_pin_is_number(char key);
void delete_lastpin();
void confirm_password();
void enter_password(char key);
void warning_nonumber();
void rfid_scan();
void delete_lastpin();
void add_card_eeprom();
//--------------Intitial Library--------------
//---------------LCD Intitial-----------------
LiquidCrystal_I2C lcd(0x27, 16, 2); // Set the I2C address of your LCD
//---------------RFID Intitial----------------
MFRC522 rfid(SS_PIN, RST_PIN);
//---------------Servo Intitial---------------
Servo servo;
//--------------Keypad Intitial---------------
char hexaKeys[ROWS][COLS] = {
  {'1', '2', '3', 'A'},
  {'4', '5', '6', 'B'},
  {'7', '8', '9', 'C'},
  {'*', '0', '#', 'D'}
};
byte rowPins[ROWS] = {7, 6, 5, 4}; 
byte colPins[COLS] = {3, 2, 1, 0};

Keypad_I2C keypad(makeKeymap(hexaKeys), rowPins, colPins, ROWS, COLS, I2CADDR);
//----------------initial Varible------------------------
bool item = false; //item is check item in box
bool GPS = false;
bool isNumber = false;
bool open_zonecontrol = false;
bool lock = true;
bool add_card = false;
bool delete_card = false;
bool in_use = false;
String password = "";      // Variable to store the entered password
String correct_password;  // Variable to store the generated password

void setup() {
  Wire.begin();
  keypad.begin();
  servo.attach(SERVO_PIN);
  SPI.begin();
  rfid.PCD_Init();
  lcd.init();
  lcd.backlight();
  lcd.home();
  Serial.begin(9600);
  randomSeed(analogRead(0)); // Seed the random generator with an analog pin
  pinMode(RELAY_PIN,OUTPUT);
  digitalWrite(RELAY_PIN,HIGH); // lock door
  servo.write(55);
  init_display();
  //display_all_uids_on_lcd(3);  // แสดง UID สำหรับบัตร 10 ใบ
}

void loop() {
  char key = keypad.getKey();
  if(!item && key){
    if(key == '#' && !in_use){
      Serial.println("#");
      open_box();
      delay(3000);
    }
    else if(key == 'A' && !in_use){
      in_use = true;
      Serial.println("A");
      password = "";
      item = true;
      lock_box();
      generatepassword();
      send_password(correct_password);// to odroid uart
    }
    else if(key == 'B' && !in_use){
      in_use = !in_use;
      open_zonecontrol = !open_zonecontrol;
      if(open_zonecontrol){
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("TAP CARD");
        lcd.setCursor(2,1);
        lcd.print("TO OPEN GATE");
      }else{
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("OFF MODE");
        lcd.setCursor(4,1);
        lcd.print("TAP CARD");
        delay(1000);
        init_display();
      }
    }
    else if(key == 'D' && !in_use){
      in_use = !in_use;
      add_card = !add_card;
      if(add_card){
        lcd.clear();
        lcd.setCursor(5,0);
        lcd.print("MODE");
        lcd.setCursor(2,1);
        lcd.print("INSERT CARD");
        delay(1500);
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Enter Password:");
        password = "";
      }else{
        lcd.clear();
        lcd.setCursor(4,0);
        lcd.print("OFF MODE");
        lcd.setCursor(2,1);
        lcd.print("INSERT CARD");
        delay(1000);
        init_display();
      }
    }
    else if(key == '*' && !in_use){
      delete_card = true;
      in_use = true;
      password = "";
      char key = '\0';
      lcd.clear();
      lcd.setCursor(5,0);
      lcd.print("MODE");
      lcd.setCursor(2,1);
      lcd.print("DELETE CARD");
      delay(1500);
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Enter Password:");
    }
  }else if(item && key){
    isNumber = check_pin_is_number(key);
    if (key == '*'){
      Serial.println("*");
      delete_lastpin();
    }
    else if(key == '#'){
      Serial.println('#');
      confirm_password();
    }
    else if(isNumber){
      Serial.println("password");
      enter_password(key);
    }else{
      Serial.println(key);
      warning_nonumber();
    }
  }
  if(open_zonecontrol){
    rfid_scan();
  }else if(add_card){
    bool isNumber = check_pin_is_number(key);
    if (key == '*'){
      Serial.println("*");
      delete_lastpin();
    }
    else if(key == 'C'){
      Serial.println('C');
      bool correct = checkpassword_admin();
      if(correct){
        char key = '\0';  // ตัวแปรสำหรับเก็บค่าปุ่มที่กด
        // รอจนกว่าจะมีการกดปุ่ม
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print("PLEASE CHOOSE ID");
        lcd.setCursor(4,0);
        lcd.print("ID:");
        while (!key) {
          key = keypad.getKey();  // ตรวจสอบการกดปุ่ม และเก็บค่าปุ่มที่กด
        }
        lcd.print(key);
        delay(500);
        add_card_confirm(key);
      }
    }
    else if(isNumber){
      Serial.println(password);
      enter_password(key);
    }
  }else if(delete_card){
    bool isNumber = check_pin_is_number(key);
    if(key == '*'){
      Serial.println("*");
      delete_lastpin();
    }else if(key == 'C'){
      Serial.println('C');
      bool correct = checkpassword_admin();
      if(correct){
        char key = '\0';  // ตัวแปรสำหรับเก็บค่าปุ่มที่กด
        // รอจนกว่าจะมีการกดปุ่ม
        lcd.clear();
        lcd.setCursor(0,1);
        lcd.print("PLEASE CHOOSE ID");
        lcd.setCursor(4,0);
        lcd.print("ID:");
        while (!key) {
          key = keypad.getKey();  // ตรวจสอบการกดปุ่ม และเก็บค่าปุ่มที่กด
          bool isNumber = check_pin_is_number(key);
          if(!isNumber)
            key = '\0';
        }
        lcd.print(key);
        delay(500);
        int index = key - '0' ;
        delete_uid_from_eeprom(index);
        lcd.clear();
        lcd.setCursor(0,0);
        lcd.print("Card Deleted");
        delay(1500);
        init_display();  // กลับไปหน้าจอหลัก
        delete_card = false;
        in_use = false;
      }
    }else if(isNumber){
      Serial.println(password);
      enter_password(key);
    }
  }
}

//function work if item = false
void open_box(){
  digitalWrite(RELAY_PIN,LOW); //unlock
  lcd.clear();
  lcd.setCursor(2,0);
  lcd.print("Door Unlocked");
  lcd.setCursor(5,1);
  delay(2000);
  lcd.clear();
  lcd.setCursor(4,0);
  lcd.print("Welcome!");
  lcd.setCursor(2,1);
  lcd.print("BPS Delivery");
  digitalWrite(RELAY_PIN,HIGH); // lock
}

void lock_box(){
  digitalWrite(RELAY_PIN,HIGH);
  printLCD("INSERT ITEM","LOCK BOX");
}

void generatepassword(){
  correct_password = "";
  for (int i = 0; i < 6; i++) {
    correct_password += String(random(0, 10)); // สร้างตัวเลขสุ่มระหว่าง 0-9
  }
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("New Password:");
  lcd.setCursor(0, 1);
 
  // แสดงรหัสผ่านที่สร้างขึ้นบน Serial Monitor
  Serial.println(correct_password); 
  
  delay(2000); // แสดงรหัสผ่านสักครู่หนึ่ง
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
}

void send_password(String correct_password) {
  Serial.print("Password:");
  Serial.println(correct_password);
}

void confirm_password(){
  if(password == correct_password){
    printLCD("Correct Password",password);
    open_box();
    item = false;
    in_use = false;
  }else{
    password = "";
    printLCD("WRONG PASSWORD","TRY AGAIN");
    delay(2000);
    printLCD("EnterPassword:",password);
  }
}

void enter_password(char key){
  password += key;
  printLCD("Enter Password:",password);
}

void warning_nonumber(){
  printLCD("Please Enter","Number 0-9");
  delay(500);
    // แสดงผลบนหน้าจอ LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
  lcd.setCursor(0, 1);
  lcd.print(password);  // แสดงรหัสผ่านปัจจุบันที่เหลือ
}

void delete_lastpin(){
  if (password.length() > 0) {
  // ลบตัวอักษรตัวสุดท้าย
  password.remove(password.length() - 1);
  
  // แสดงผลบนหน้าจอ LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Enter Password:");
  lcd.setCursor(0, 1);
  lcd.print(password);  // แสดงรหัสผ่านปัจจุบันที่เหลือ
  }
}

bool check_pin_is_number(char key){
  char number[10] = {'1','2','3','4','5','6','7','8','9','0'};
  for (int i = 0; i < sizeof(number); i++) {
    if (key == number[i]) {
      return true;
    }
  }
  return false;
}

void printLCD(String line1, String line2) {
  lcd.clear();              // ล้างจอ LCD ก่อน
  lcd.setCursor(0, 0);      // ตั้งตำแหน่งเคอร์เซอร์ไปที่บรรทัดแรก
  lcd.print(line1);         // แสดงข้อความบรรทัดแรก
  lcd.setCursor(0, 1);      // ตั้งตำแหน่งเคอร์เซอร์ไปที่บรรทัดที่สอง
  lcd.print(line2);         // แสดงข้อความบรรทัดที่สอง
}

void save_password_to_eeprom(String password) {
  for (int i = 0; i < password.length(); i++) {
    EEPROM.update(i, password[i]);  // ใช้ EEPROM.update() แทน EEPROM.write()
  }
  EEPROM.update(password.length(), '\0');  // เขียนตัวสิ้นสุด string
}

String read_password_from_eeprom() {
  String password = "";
  char ch;
  int i = 0;
  
  while ((ch = EEPROM.read(i)) != '\0') {
    password += ch;
    i++;
  }
  return password;
}

void read_uids_from_eeprom(String uid_array[], int num_cards) {
  for (int card_index = 0; card_index < num_cards; card_index++) {
    int address = card_index * 16;  // ตำแหน่งเริ่มต้นของแต่ละบัตร
    String uid = "";
    char ch;
    int i = 0;
    
    // อ่านค่า UID ทีละไบต์จนกว่าจะเจอตัวสิ้นสุด '\0' หรือค่า 0xFF
    while ((ch = EEPROM.read(address + i)) != '\0' && ch != 0xFF && i < 16) {
      uid += ch;
      i++;
    }
    
    if (uid.length() > 0) {
      uid_array[card_index] = uid;  // เก็บ UID ที่อ่านได้ลงใน array
    } else {
      uid_array[card_index] = "Empty";  // ถ้าไม่มี UID ให้แสดงว่า "Empty"
    }
  }
}

void write_uid_to_eeprom(int card_index, String uid) {
  int address = card_index * 16;  // ตำแหน่งเริ่มต้นของบัตร
  
  // เขียน UID ลงใน EEPROM ทีละตัวอักษร
  for (int i = 0; i < uid.length(); i++) {
    EEPROM.write(address + i, uid[i]);
  }
  
  // เขียนตัวสิ้นสุด '\0' เพื่อบอกว่าจบแล้ว
  EEPROM.write(address + uid.length(), '\0');
}

bool checkpassword_admin(){
  Serial.print("Checking");
  if(password == Adminpass){  // ตรวจสอบรหัสผ่าน Admin ก่อน
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Correct Password");
    delay(500);
    return true;
  }else{
    password = "";
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("Wrong Password");
    lcd.setCursor(2,1);
    lcd.print("Try Again");
    delay(500);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Enter Password:");
    lcd.setCursor(0,1);
    lcd.print(password);
    return false;
  }
}

void add_card_confirm(char key){
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("TAP CARD PLEASE");
    lcd.setCursor(1,1);
    lcd.print("Waiting...");
    delay(1500);
    if ( !rfid.PICC_IsNewCardPresent()) {
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("No card found!");
      delay(1500);
      init_display();  // กลับไปหน้าจอหลัก
      add_card = false;
      in_use = false;
      return;
    }
    
    if ( !rfid.PICC_ReadCardSerial()) {
      lcd.clear();
      lcd.setCursor(2, 0);
      lcd.print("Error reading card!");
      delay(1500);
      init_display();  // กลับไปหน้าจอหลัก
      add_card = false;
      in_use = false;
      return;
    }
    Serial.print("Succes!");
    
    lcd.clear();
    lcd.setCursor(3,0);
    // อ่าน UID ของบัตร RFID
    String ID = "";
    for (byte i = 0; i < rfid.uid.size; i++) {
      lcd.print(".");
      ID.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
      ID.concat(String(rfid.uid.uidByte[i], HEX));
      delay(300);
    }
    ID.toUpperCase();
    //ASCII 0 = 48 ถ้าลบ0ก็แปลว่าจะได้ค่าหลังจาก48เป็นเลขทั้งหมด
    int index = key - '0';
    // เขียน UID ลงใน EEPROM
    write_uid_to_eeprom(index,ID);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Card Registered");
    delay(1500);
    lcd.clear();
    init_display();
    add_card = false;
    in_use = false;
}

void rfid_scan(){
  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print("TAP CARD PLEASE");
  lcd.setCursor(1,1);
  lcd.print("Waiting...");
  delay(1500);
  if ( !rfid.PICC_IsNewCardPresent()) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("No card found!");
    delay(1500);
    init_display();  // กลับไปหน้าจอหลัก
    open_zonecontrol = false;
    in_use = false;
    return;
  }
  if ( !rfid.PICC_ReadCardSerial()) {
    lcd.clear();
    lcd.setCursor(2, 0);
    lcd.print("Error reading card!");
    delay(1500);
    init_display();  // กลับไปหน้าจอหลัก
    open_zonecontrol = false;
    in_use = false;
    return;
  }
  // อ่าน UID ของบัตรที่สแกน
  String scannedID = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    lcd.print(".");
    scannedID.concat(String(rfid.uid.uidByte[i] < 0x10 ? " 0" : " "));
    scannedID.concat(String(rfid.uid.uidByte[i], HEX));
    delay(300);
  }
  scannedID.toUpperCase();

  // สร้าง array สำหรับเก็บ UID ของบัตรที่บันทึกไว้
  String uid_array[10];
  read_uids_from_eeprom(uid_array, 10);

  // ตรวจสอบว่าบัตรที่สแกนตรงกับบัตรใดใน array หรือไม่
  bool found = false;
  for (int i = 0; i < 10; i++) {
    if (scannedID == uid_array[i]) {
      found = true;
      break;
    }
  }

  // แสดงผลลัพธ์
  if (found && !lock) {
    servo.write(55);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door is locked");
    delay(1500);
    init_display();
    lock = true;
    open_zonecontrol = false;
    in_use = false;
  } else if(found && lock){
    servo.write(0);
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Door is open");
    delay(1500);
    init_display();
    lock = false;
    open_zonecontrol = false;
    in_use = false;
  }else {
    lcd.clear();
    lcd.setCursor(3, 0);
    lcd.print("NO CARD!");
    lcd.setCursor(2,1);
    lcd.print("IN EEPROM");
    delay(1500);
    init_display();
    open_zonecontrol = false;
    in_use = false;
  }
}

void display_all_uids_on_lcd(int num_cards) {
  lcd.clear();
  lcd.setCursor(0, 0);  
  lcd.print("UIDs in EEPROM:");
  delay(2000);  // แสดงข้อความชั่วคราวก่อนแสดง UID

  for (int card_index = 0; card_index < num_cards; card_index++) {
    int address = card_index * 16;  // ตำแหน่งเริ่มต้นของแต่ละบัตร
    String uid = "";
    char ch;
    int i = 0;
    
    // อ่านค่า UID ทีละไบต์จนกว่าจะเจอตัวสิ้นสุด '\0' หรือค่า 0xFF
    while ((ch = EEPROM.read(address + i)) != 0 && ch != 0xFF && i < 16) {
      uid += ch;
      i++;
    }
    
    lcd.clear();
    lcd.setCursor(0, 0);  
    lcd.print("Card ");
    lcd.print(card_index + 1);

    if (uid.length() > 0) {
      lcd.setCursor(0, 1);
      lcd.print(uid);  // แสดง UID ที่เก็บไว้
    } else {
      lcd.setCursor(0, 1);
      lcd.print("Empty");  // ถ้าไม่มี UID เก็บอยู่ แสดงว่า Empty
    }
    
    delay(1500);  // แสดงข้อมูลแต่ละบัตร 3 วินาที
  }

  lcd.clear();
  lcd.setCursor(0, 0);  
  lcd.print("End of UID list");
  delay(1500);  // แสดงข้อความสุดท้ายชั่วคราว
  init_display();  // กลับไปหน้าจอเริ่มต้น
}

void display_all_uids_from_eeprom(int num_cards) {
  Serial.println("Displaying all UIDs stored in EEPROM:");
  
  for (int card_index = 0; card_index < num_cards; card_index++) {
    int address = card_index * 16;  // ตำแหน่งเริ่มต้นของแต่ละบัตร
    String uid = "";
    char ch;
    int i = 0;
    
    // อ่านค่า UID ทีละไบต์จนกว่าจะเจอตัวสิ้นสุด '\0' หรือค่า 0xFF
    while ((ch = EEPROM.read(address + i)) != 0 && ch != 0xFF && i < 16) {
      uid += ch;
      i++;
    }
    
    if (uid.length() > 0) {
      Serial.print("Card ");
      Serial.print(card_index + 1);
      Serial.print(": ");
      Serial.println(uid);
    } else {
      Serial.print("Card ");
      Serial.print(card_index + 1);
      Serial.println(": Empty");
    }
  }
  Serial.println("-------------------------------");
}

void delete_uid_from_eeprom(int card_index) {
  int address = card_index * 14;  // เริ่มที่ตำแหน่งของบัตรที่ต้องการลบ
  
  // ลบข้อมูลใน EEPROM โดยเขียนค่า 0xFF ลงไปในทุกตำแหน่งที่ UID ของบัตรนั้นถูกเก็บไว้
  for (int i = 0; i < 14; i++) {
    EEPROM_Erase_only(address + i);  // เขียนค่า 0xFF เพื่อลบข้อมูล
  }
  Serial.print("UID at index ");
  Serial.print(card_index);
  Serial.println("deleted from EEPROM.");
}

unsigned char EEPROM_read1byte(uint16_t addr)  //ชื่อฟังก์ชันและพารามิเตอร์รับเข้า
{       
  while (EECR & (1<<EEPE)) ;    //วนซ้ำจนกว่าบิต EEPE จะเปลี่ยนเป็นตรรกะต่ำ
  EEAR = addr;        //คัดลอกพารามิเตอร์รับเข้าสู่เรจิสเตอร์ EEAR
  EECR |= (1<<EERE);      //สั่งให้บิต EERE เป็นตรรกะสูงเพื่อเริ่มการอ่าน
  return EEDR;        //นำค่าที่ปรากฏใน EEDR ไปใช้งาน
}

void EEPROM_Erase_only(uint16_t addr)
{  
  while(EECR & (1<<EEPE)) ; //วนซ้ำจนกว่าบิต EEPE จะเปลี่ยนเป็นตรรกะต่ำ
  EECR = 0b01 << EEPM0;   //ลบอย่างเดียว 
  EEAR = addr;      //คัดลอกพารามิเตอร์รับเข้า addr สู่เรจิสเตอร์ EEAR
  EEDR = 0xFF;      //คัดลอกพารามิเตอร์รับเข้า data สู่เรจิสเตอร์ EEDR
  char backupSREG;    //ตัวแปรสำหรับเก็บค่าสถานะของเรจิสเตอร์ตัวบ่งชี้
  backupSREG = SREG;    //คัดลอกค่าในเรจิสเตอร์ตัวบ่งชี้ใส่ในตัวแปรที่ตั้งไว้
  cli();        //ปิดทางการขัดจังหวะส่วนกลางของตัวประมวลผล
  EECR |= (1<<EEMPE);   //สั่งให้บิต EEMPE ใน EECR เป็นตรรกะสูง
  EECR |= (1<<EEPE);    //สั่งให้บิต EEPE ใน EECR เป็นตรรกะสูง
  SREG = backupSREG;    //คืนค่าเรจิสเตอร์ตัวบ่งชี้กลับสู่สถานะเดิม
}

void display_all_data_in_EEPROM()
{
  Serial.println("Data in the EEPROM");
  int i;
  uint8_t c;
  for (i=0;i<1024;i++)
  {
    c = EEPROM_read1byte(i);
    if( (i>0) && (i%16==0) )
      Serial.println("");
    Serial.print(c);
    Serial.print(" ");
  }
  Serial.println("\n\r--------------------------");  
}

void init_display(){
  lcd.clear();
  lcd.setCursor(3,0);
  lcd.print("Welcome!");
  lcd.setCursor(2,1);
  lcd.print("BPS Delivery");
}