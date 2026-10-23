#include <SPI.h>
#include <Wire.h>
#include <RTClib.h>

// ================== 引脚定义 ==================
#define EPD_CS   20
#define EPD_DC   7
#define EPD_RST  5
#define EPD_BUSY 3
// 时钟模块 I2C 引脚
#define I2C_SDA  8
#define I2C_SCL  9

#define EPD_W 152
#define EPD_H 152
#define BUFFER_SIZE 2888 

uint8_t ImageBuffer[BUFFER_SIZE];
RTC_DS3231 rtc;

// ================== 屏幕底层驱动 ==================
void EPD_WR_DATA8(uint8_t data) {
  digitalWrite(EPD_DC, HIGH); digitalWrite(EPD_CS, LOW);
  SPI.transfer(data); digitalWrite(EPD_CS, HIGH);
}
void EPD_WR_REG(uint8_t cmd) {
  digitalWrite(EPD_DC, LOW); digitalWrite(EPD_CS, LOW);
  SPI.transfer(cmd); digitalWrite(EPD_CS, HIGH);
}
void EPD_READBUSY(void) {
  unsigned long start = millis();
  while (digitalRead(EPD_BUSY) == HIGH) {
    delay(10);
    if (millis() - start > 4000) break; 
  }
}

// ================== 屏幕初始化（官方手册版） ==================
void EPD_Init(void) {
  digitalWrite(EPD_RST, HIGH); delay(20);
  digitalWrite(EPD_RST, LOW);  delay(20);
  digitalWrite(EPD_RST, HIGH); delay(20);
  EPD_READBUSY();

  EPD_WR_REG(0x12); // SW Reset
  EPD_READBUSY();
  delay(10);

  EPD_WR_REG(0x01); EPD_WR_DATA8(0x97); EPD_WR_DATA8(0x00); EPD_WR_DATA8(0x00);
  EPD_WR_REG(0x11); EPD_WR_DATA8(0x03);
  EPD_WR_REG(0x44); EPD_WR_DATA8(0x00); EPD_WR_DATA8(0x12);
  EPD_WR_REG(0x45); EPD_WR_DATA8(0x00); EPD_WR_DATA8(0x00); EPD_WR_DATA8(0x97); EPD_WR_DATA8(0x00);
  EPD_WR_REG(0x3C); EPD_WR_DATA8(0x05);

  EPD_WR_REG(0x18); EPD_WR_DATA8(0x80);
  EPD_WR_REG(0x22); EPD_WR_DATA8(0xB1);
  EPD_WR_REG(0x20); EPD_READBUSY();
}

// ================== 将显存推送到屏幕 ==================
void EPD_Display(const uint8_t *image) {
  uint16_t i, j;
  EPD_WR_REG(0x4E); EPD_WR_DATA8(0x00);
  EPD_WR_REG(0x4F); EPD_WR_DATA8(0x00); EPD_WR_DATA8(0x00);
  EPD_WR_REG(0x24);
  for (j=0; j<EPD_H; j++) {
    for (i=0; i<19; i++) {
      EPD_WR_DATA8(image[i + j * 19]);
    }
  }
}

// ================== 绘图工具 ==================
void DrawPixel(int x, int y, bool black) {
  if (x < 0 || x >= EPD_W || y < 0 || y >= EPD_H) return;
  int byteIndex = (y * 19) + (x / 8);
  int bitIndex = 7 - (x % 8);
  if (black) ImageBuffer[byteIndex] &= ~(1 << bitIndex); 
  else ImageBuffer[byteIndex] |= (1 << bitIndex);  
}

// 手搓字库 (0-9, 冒号, 横杠)
void DrawChar(int x, int y, char c, int scale) {
  uint8_t bitmap[8] = {0};
  if (c == '0') { uint8_t b[8] = {0x3C,0x66,0x6E,0x76,0x66,0x66,0x3C,0x00}; memcpy(bitmap,b,8); }
  if (c == '1') { uint8_t b[8] = {0x18,0x38,0x18,0x18,0x18,0x18,0x7E,0x00}; memcpy(bitmap,b,8); }
  if (c == '2') { uint8_t b[8] = {0x3C,0x66,0x06,0x0C,0x18,0x30,0x7E,0x00}; memcpy(bitmap,b,8); }
  if (c == '3') { uint8_t b[8] = {0x3C,0x66,0x06,0x1C,0x06,0x66,0x3C,0x00}; memcpy(bitmap,b,8); }
  if (c == '4') { uint8_t b[8] = {0x0C,0x1C,0x3C,0x6C,0x7E,0x0C,0x0C,0x00}; memcpy(bitmap,b,8); }
  if (c == '5') { uint8_t b[8] = {0x7E,0x60,0x7C,0x06,0x06,0x66,0x3C,0x00}; memcpy(bitmap,b,8); }
  if (c == '6') { uint8_t b[8] = {0x3C,0x66,0x60,0x7C,0x66,0x66,0x3C,0x00}; memcpy(bitmap,b,8); }
  if (c == '7') { uint8_t b[8] = {0x7E,0x66,0x0C,0x18,0x18,0x18,0x18,0x00}; memcpy(bitmap,b,8); }
  if (c == '8') { uint8_t b[8] = {0x3C,0x66,0x66,0x3C,0x66,0x66,0x3C,0x00}; memcpy(bitmap,b,8); }
  if (c == '9') { uint8_t b[8] = {0x3C,0x66,0x66,0x3E,0x06,0x66,0x3C,0x00}; memcpy(bitmap,b,8); }
  if (c == ':') { uint8_t b[8] = {0x00,0x18,0x18,0x00,0x18,0x18,0x00,0x00}; memcpy(bitmap,b,8); }
  if (c == '-') { uint8_t b[8] = {0x00,0x00,0x00,0x7E,0x00,0x00,0x00,0x00}; memcpy(bitmap,b,8); }

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if (bitmap[row] & (0x80 >> col)) {
        for(int i=0; i<scale; i++){
          for(int j=0; j<scale; j++){
            DrawPixel(x + col * scale + i, y + row * scale + j, true);
          }
        }
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  
  pinMode(EPD_CS, OUTPUT); digitalWrite(EPD_CS, HIGH);
  pinMode(EPD_DC, OUTPUT);
  pinMode(EPD_RST, OUTPUT);
  pinMode(EPD_BUSY, INPUT);
  
  // 屏幕 SPI 启动
  SPI.begin(4, -1, 6, -1);
  SPI.beginTransaction(SPISettings(500000, MSBFIRST, SPI_MODE0));

  // 启动时钟模块
  Wire.begin(I2C_SDA, I2C_SCL);
  if (!rtc.begin()) {
    Serial.println("警告：找不到 DS3231！请检查 GPIO8 和 GPIO9 接线！");
  }
  
  // 如果你是第一次用这个时钟模块，或者发现时间不对，取消下面这行的注释，烧录一次校准时间，然后再注释掉重新烧录
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__))); 

  EPD_Init(); // 初始化屏幕
}

void loop() {
  DateTime now = rtc.now();
  
  // 安全限制，防止读取乱码
  int h = now.hour();
  int m = now.minute();
  if (h < 0 || h > 23) h = 12;
  if (m < 0 || m > 59) m = 34;

  char timeStr[6];
  // 将时间格式化为 HH:MM
  sprintf(timeStr, "%02d:%02d", h, m);

  // 清空显存为全白
  for(int i = 0; i < BUFFER_SIZE; i++) ImageBuffer[i] = 0xFF; 

  // =============== 排版设计 ===============
  // 画时间 (X=10, Y=50, 放大3倍)
  int startX = 10; 
  int startY = 50;
  int scale = 3;

  DrawChar(startX,      startY, timeStr[0], scale); // H1
  DrawChar(startX + 28, startY, timeStr[1], scale); // H2
  DrawChar(startX + 56, startY, timeStr[2], scale); // :
  DrawChar(startX + 70, startY, timeStr[3], scale); // M1
  DrawChar(startX + 98, startY, timeStr[4], scale); // M2

  // 画日期 (X=25, Y=115, 放大2倍)
  char dateStr[11];
  sprintf(dateStr, "%02d-%02d-%02d", now.year() % 100, now.month(), now.day());
  DrawChar(25,  115, dateStr[0], 2);
  DrawChar(43,  115, dateStr[1], 2);
  DrawChar(61,  115, dateStr[2], 2);
  DrawChar(79,  115, dateStr[3], 2);
  DrawChar(97,  115, dateStr[4], 2);
  DrawChar(115, 115, dateStr[5], 2);

  // 把画好的画面推送到屏幕
  EPD_Display(ImageBuffer);
  
  // 触发屏幕刷新
  EPD_WR_REG(0x22); EPD_WR_DATA8(0xF4); 
  EPD_WR_REG(0x20); 
  EPD_READBUSY(); 
  
  Serial.print("刷新完成，当前时间: ");
  Serial.println(timeStr);

  // 延迟 10 秒刷新一次，先测试稳定性（以后可以改成局部刷新或深度睡眠）
  delay(10000); 
}