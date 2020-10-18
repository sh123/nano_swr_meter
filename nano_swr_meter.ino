#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <timer.h>
#include <math.h>

#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 32 // OLED display height, in pixels

int fwdPin = A7;
int rflPin = A6;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire);

struct correction_table_t {
  int v;
  double pwr;
} correctionTable[] = {
  {   2,  0.01 },
  { 100,   0.5 },
  { 117,   1.0 },
  { 143,   2.0 },
  { 158,   3.0 },
  { 184,   4.0 },
  { 196,   5.0 },
  { 285,  10.0 },
  { 336,  15.0 },
  { 382,  20.0 },
  { 427,  25.0 },
  { 477,  30.0 },
  { 569,  40.0 },
  { 665,  50.0 },
  { 727,  60.0 },
  { 793,  70.0 },
  { 853,  80.0 },
  { 912,  90.0 },
  { 973, 100.0 },
  {1024, 110.0 },
  { -1,    0.0 }
};

const int screenUpdateMs = 500;

const int numReadings = 10;
int readIndex = 0;

int fwdTotal = 0;
int fwdAverage = 0;
int rflTotal = 0;
int rflAverage = 0;

int fwdReadings[numReadings];
int rflReadings[numReadings];

double gamma;
double vswr;

auto timer = timer_create_default();

void setup() {
  Serial.begin(9600);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) 
  {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  
  display.clearDisplay();
  display.setTextSize(2);      // Normal 1:1 pixel scale
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(8, 8);     // Start at top-left corner
  display.print(F("SWR meter"));
  display.display();
  delay(1000);
  
  display.clearDisplay();
  display.display();

  for (int thisReading = 0; thisReading < numReadings; thisReading++) {
    fwdReadings[thisReading] = 0;
    rflReadings[thisReading] = 0;
  }

  timer.every(screenUpdateMs, updateScreen);
}

double toWatts(int val)
{
  int v, v_prev;
  double pwr, pwr_prev;

  v = v_prev = 0;
  pwr = pwr_prev = 0.0;
  
  for (int i = 0; ;i++) {
    v_prev = v;
    pwr_prev = pwr;
    
    v = correctionTable[i].v;
    pwr = correctionTable[i].pwr;

    if (v == -1) break;
    if (v == 0) continue;

    if (val > v_prev && val <= v) {
        double k = (double)(pwr - pwr_prev) / (double)(v - v_prev);
        return k * (val - v_prev) + pwr_prev;
    }
  }
  return 0.0;
}

void loop() 
{
  int fwdVal = analogRead(fwdPin);
  int rflVal = analogRead(rflPin);

  fwdTotal = fwdTotal - fwdReadings[readIndex];
  fwdReadings[readIndex] = fwdVal;
  fwdTotal = fwdTotal + fwdReadings[readIndex];

  rflTotal = rflTotal - rflReadings[readIndex];
  rflReadings[readIndex] = rflVal;
  rflTotal = rflTotal + rflReadings[readIndex];
  
  readIndex = readIndex + 1;
  
  if (readIndex >= numReadings) {
    readIndex = 0;
  }
  
  fwdAverage = fwdTotal / numReadings;
  rflAverage = rflTotal / numReadings;
  
  gamma = (double)rflVal / (double)fwdVal;
  vswr = (1 + abs(gamma)) / (1 - abs(gamma));

  timer.tick();
  delay(50);
}

bool updateScreen() 
{
  double fwdWatts = toWatts(fwdAverage);
  double rflWatts = toWatts(rflAverage);
    
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 8);

  display.print(F("FWD: "));
  if (fwdWatts < 1.0) {
    display.print((int)(fwdWatts*1000));
    display.print(F("mW"));
  } else {
    display.print(fwdWatts);
    display.print(F("W"));
  }
  display.print(F("/"));
  //display.print(10 * log10(1000 * fwdWatts));
  //display.print(F("dBm/"));
  display.println(fwdAverage);

  display.print(F("RFL: "));
  if (rflWatts < 1.0) {
    display.print((int)(rflWatts*1000));
    display.print(F("mW"));
  }
  else {
    display.print(rflWatts);
    display.print(F("W"));
  }
  display.print(F("/"));
  //display.print(10 * log10(1000 * rflWatts));
  //display.print(F("dBm/"));
  display.println(rflAverage);

  display.print(F("SWR: "));
  display.print(vswr);
  display.print(F("/"));
  display.print(gamma * 100.0);
  display.print(F("%"));

  for (int i = 0; i <= 10; i++) {
    display.drawPixel(i * (display.width()-1) / 10, 0, SSD1306_WHITE);
    display.drawPixel(i * (display.width()-1) / 10, 2, SSD1306_WHITE);
    display.drawPixel(i * (display.width()-1) / 10, 4, SSD1306_WHITE);
  }
  for (int i = 0; i <= 4; i++) {
    display.drawPixel(i * (display.width()-1) / 4, 6, SSD1306_WHITE);
  }
  
  display.drawLine(0, 0, fwdWatts * (display.width()-1) / 1.0, 0, SSD1306_WHITE);
  display.drawLine(0, 2, fwdWatts * (display.width()-1) / 10.0, 2, SSD1306_WHITE);
  display.drawLine(0, 4, fwdWatts * (display.width()-1) / 100.0, 4, SSD1306_WHITE);
  display.drawLine(0, 6, (vswr - 1) * (display.width()-1) / 5.0, 6, SSD1306_WHITE);
  
  display.display();

  return true;
}
