// Paint example specifically for the TFTLCD Arduino shield.
// If using the breakout board, use the tftpaint.pde sketch instead!

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_TFTLCD.h> // Hardware-specific library
#include <TouchScreen.h>

#if defined(__SAM3X8E__)
    #undef __FlashStringHelper::F(string_literal)
    #define F(string_literal) string_literal
#endif

/*#ifndef USE_ADAFRUIT_SHIELD_PINOUT 
 #error "This sketch is intended for use with the TFT LCD Shield. Make sure that USE_ADAFRUIT_SHIELD_PINOUT is #defined in the Adafruit_TFTLCD.h library file."
#endif*/

// These are the pins for the shield!
#define YP A3  // must be an analog pin, use "An" notation!
#define XM A2  // must be an analog pin, use "An" notation!
#define YM 9   // can be a digital pin
#define XP 8   // can be a digital pin

#ifdef __SAM3X8E__
  #define TS_MINX 125
  #define TS_MINY 170
  #define TS_MAXX 880
  #define TS_MAXY 940
#else
  #define TS_MINX  150
  #define TS_MINY  120
  #define TS_MAXX  920
  #define TS_MAXY  940
#endif

// For better pressure precision, we need to know the resistance
// between X+ and X- Use any multimeter to read it
// For the one we're using, its 300 ohms across the X plate
TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

#define LCD_CS A3
#define LCD_CD A2
#define LCD_WR A1
#define LCD_RD A0
#define LCD_RESET A4

// Assign human-readable names to some common 16-bit color values:
#define BLACK   0x0000
#define BLUE    0x001F
#define RED     0xF800
#define GREEN   0x07E0
#define CYAN    0x07FF
#define MAGENTA 0xF81F
#define YELLOW  0xFFE0
#define WHITE   0xFFFF

#define BACKGROUND BLACK
#define TEXT WHITE

#define TIME_X 18
#define TIME_Y 88

#define CHAR_WIDTH 36

Adafruit_TFTLCD tft(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

long long START_TIME;
int OLD_T[4] {0};

// button press trackers
long long INC_TIME[3];
long long DEC_TIME[3];

bool SHOW_24H = true;

void setup(void) {
  Serial.begin(9600);

  tft.reset();

  uint16_t identifier = tft.readID();

  if(identifier == 0x9325) {
    Serial.println(F("Found ILI9325 LCD driver"));
  } else if(identifier == 0x9328) {
    Serial.println(F("Found ILI9328 LCD driver"));
  } else if(identifier == 0x7575) {
    Serial.println(F("Found HX8347G LCD driver"));
  } else {
    Serial.print(F("Unknown LCD driver chip: "));
    Serial.println(identifier, HEX);
    identifier = 0x9341;
    //return;
  }

  tft.begin(identifier);

  tft.fillScreen(BACKGROUND);
  tft.setRotation(1);
  tft.setTextColor(TEXT);
  tft.setTextSize(6);

  START_TIME = millis();
  
  pinMode(13, OUTPUT);
  
  Serial.println("Setup complete.");
}


// ensures the hours stay within 0-23
void fix_current_time(int* T) {
   while (T[0] >= 24) {
    START_TIME += (long long)24 * 60 * 60 * 1000;
    T[0] -= 24;
   }
}


// returns int[4] {hours, minutes, seconds, milliseconds}
int* get_current_time() {
  while (START_TIME > millis()) { // ensure that the start time is always smaller than current time
    START_TIME -= (long long)24 * 60 * 60 * 1000;
  }
  long long ms = millis() - START_TIME;
  
  long long s = ms / 1000;
  ms -= s * 1000;
  
  long long mins = s / 60;
  s -= mins * 60;
  
  long long h = mins / 60;
  mins -= h * 60;

  static int T[4];
  T[0] = h;
  T[1] = mins;
  T[2] = s;
  T[3] = ms;

  fix_current_time(T);
  
  return T;
}


// compares the hours, minutes and seconds (but not milliseconds)
bool is_different(int* T1, int* T2) {
  for (int i = 0; i < 3; i++) {
    if (T1[i] != T2[i]) return true;
  }
  return false;
}


// writes the current time to the screen, if it has changed
// also updates the global OLD_T variable with the new time
void write_time() {
  int* T = get_current_time();

  if (is_different(T, OLD_T)) {
    // prepare for the 12 hour time format
    bool is_pm = false;
    if (!SHOW_24H) {
      if (T[0] >= 12) {
        T[0] -= 12;
        is_pm = true;
      }
      T[0]++;
    }
    
    // clear existing time only where it needs to be changed
    tft.setTextColor(BACKGROUND);
    for (int i = 0; i < 3; i++) {
      if (OLD_T[i] != T[i]) {
        tft.setCursor(TIME_X + i * (3 * CHAR_WIDTH), TIME_Y);
        if (OLD_T[i] < 10) tft.print(0); // zero padding
        tft.print(OLD_T[i]);
      }
    }
    tft.setCursor(TIME_X, TIME_Y + CHAR_WIDTH);
    tft.fillRect(TIME_X, TIME_Y + CHAR_WIDTH, TIME_X + 2 * CHAR_WIDTH, TIME_Y, BACKGROUND);
    
    // print the new time
    tft.setCursor(TIME_X, TIME_Y);
    tft.setTextColor(TEXT);
    for (int i = 0; i < 3; i++) {
      if (T[i] < 10) tft.print(0); // zero padding
      tft.print(T[i]);
      if (i != 2) tft.print(":");
    }

    for (int i = 0; i < 4; i++) {
      OLD_T[i] = T[i];
    }
  }
}


#define MINPRESSURE 10
#define MAXPRESSURE 1000

void loop()
{
  digitalWrite(13, HIGH);
  TSPoint p = ts.getPoint();
  //p.x = 1023 - p.x;
  digitalWrite(13, LOW);

  // if sharing pins, you'll need to fix the directions of the touchscreen pins
  //pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  //pinMode(YM, OUTPUT);

  write_time();
 
  if (p.z > MINPRESSURE && p.z < MAXPRESSURE) {
    // scale from 0->1023 to tft.width
    int t = p.x;
    p.x = map(p.y, TS_MINX, TS_MAXX, tft.width(), 0);
    p.y = map(t, TS_MINY, TS_MAXY, tft.height(), 0);
    
    /*
    Serial.print("("); Serial.print(p.x);
    Serial.print(", "); Serial.print(p.y);
    Serial.println(")");
    */

    // determine whether hours, minutes or seconds are pressed
    int i = 0;
    if (p.x <= (TIME_X + CHAR_WIDTH * 2 + CHAR_WIDTH / 2)) { 
      i = 0; // hours
    } else if (p.x > (TIME_X + CHAR_WIDTH * 2 + CHAR_WIDTH / 2) && p.x <= (TIME_X + CHAR_WIDTH * 5 + CHAR_WIDTH / 2)) { 
      i = 1; // minutes
    } else if (p.x > (TIME_X + CHAR_WIDTH * 5 + CHAR_WIDTH / 2)) { 
      i = 2; // seconds
    }

    int do_change = 0;
    if (p.y <= 100) { // increase
      if ((millis() - INC_TIME[i]) > 200) {
        do_change = 1;
        INC_TIME[i] = millis();
      }
    } else if (p.y >= 140) { // decrease
      if ((millis() - DEC_TIME[i]) > 200) {
        do_change = -1;
        DEC_TIME[i] = millis();
      }
    }

    // calculate how much the start time needs to be changed
    long long change = 1000 * do_change;
    for (int j = 2; j > i; j--) {
      change *= 60;
    }
    START_TIME -= change;
  }
}

