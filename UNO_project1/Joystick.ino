#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/sleep.h>
#include <util/delay.h>

#define F_CPU 16000000UL // frequency Clock 16 MHz
#define SLEEP_POWER_SAVE  3 //
#define AxisX 0
#define AxisY 1
#define BUAD_9600 103
#define BUAD_115200 8
#define PinSW 2

uint16_t readADC(uint8_t channel);
void setupADC();
void UART_init(unsigned int ubrr);
void UART_transmit(unsigned char data);
void UART_sendString(const char* str);

bool JoystickOn = true;
uint16_t ValueX, ValueY;
uint8_t SW;
char buffer[10]; // Buffer to hold the ASCII converted values

int main(void)
{
  DDRD &= (0 << PD2);
  PORTD |= (1 << PD2);  // ตั้งค่าบิต PD2 ให้เป็น 0 (active, LOW)
  setupADC();
  UART_init(BUAD_115200); // Initialize UART with calculated baud rate value
  setupInterrupt(); // Initialize the interrupt
  SLEEP_INITIALIZE(SLEEP_POWER_SAVE); // แก้ไขชื่อฟังก์ชันให้ถูกต้อง
  sei();  // เปิดการทำงานของ interrupt

  while (1)
  {
    if (JoystickOn)  // ตรวจสอบสถานะของ Joystick
    {
      // อ่านค่าจาก X และ Y axis ของ Joystick
      ValueX = readADC(AxisX);
      ValueY = readADC(AxisY);

      // ส่งค่าผ่าน UART
      itoa(ValueX, buffer, 10);  // แปลงค่าเป็น string
      UART_sendString("X:");
      UART_sendString(buffer);
      UART_sendString("\n");

      itoa(ValueY, buffer, 10);  // แปลงค่าเป็น string
      UART_sendString("Y:");
      UART_sendString(buffer);
      UART_sendString("\n");

      _delay_ms(150);
    }
    else  // ถ้า Joystick ถูกปิด
    {
      sleep_cpu();  // เข้าสู่ sleep mode
      SLEEP_DISABLE();  // ปิด sleep mode
    }
  }
}

uint16_t readADC(uint8_t channel)
{
  // เลือกช่อง ADC
  ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);

  // เริ่มการแปลงค่า
  ADCSRA |= (1 << ADSC);

  // รอจนกว่าการแปลงค่าจะเสร็จสิ้น
  while (ADCSRA & (1 << ADSC))
    ;

  return ADC;
}

void setupADC()
{
  // เปิดใช้งาน ADC และตั้งค่า prescaler ที่ 128
  ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
  ADMUX = (1 << REFS0);  // ตั้งค่าแรงดันอ้างอิงเป็น AVcc
}

void setupInterrupt()
{
  // ตั้งค่า INT0 ให้ trigger เมื่อเกิด falling edge
  EICRA |= (1 << ISC01); // ISC01 = 1, ISC00 = 0 -> Falling edge
  EICRA &= (0 << ISC00);
  EIMSK |= (1 << INT0);  // เปิดการทำงานของ INT0 interrupt
}

void UART_init(unsigned int ubrr)
{
  UBRR0H = (unsigned char)(ubrr >> 8);
  UBRR0L = (unsigned char)ubrr;
  UCSR0B = (1 << TXEN0);  // เปิดใช้งานการส่งข้อมูล
  UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);  // ตั้งค่า frame format: 8 data bits, 1 stop bit
  _delay_ms(200);
}

void UART_transmit(unsigned char data)
{
  while (!(UCSR0A & (1 << UDRE0)))  // รอจนกว่าบัฟเฟอร์จะว่าง
    ;
  UDR0 = data;  // ส่งข้อมูล
}

void UART_sendString(const char* str)
{
  while (*str)
  {
    UART_transmit(*str++);
  }
}

void SLEEP_DISABLE(void)
{
  SMCR &= 0xFE;  // ปิด sleep mode
}

void SLEEP_INITIALIZE(uint8_t mode)
{
  SMCR = (mode << 1) | 0x01;  // ตั้งค่า sleep mode
}

ISR(INT0_vect)
{
  _delay_ms(300);
  JoystickOn = !JoystickOn;  // เปลี่ยนสถานะ Joystick
  if (JoystickOn)
  {
    sprintf(buffer, "X:511\nY:511\nEN\n");
  }
  else
  {
    sprintf(buffer, "X:511\nY:511\nOFF\n");
  }
  UART_sendString(buffer);
}
