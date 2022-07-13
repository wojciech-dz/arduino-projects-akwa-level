#include <Wire.h>
#include <DS3231.h>
#include <OneWire.h>
#include <DS18B20.h>

/*
02       TEMP
03  PWM  
04       HEAT
05  PWM  COLD
06  PWM  RB
07
08
09  PWM  DAY
10  PWM  NIGHT
11  PWM
12
13

*/

// TODO
// dodać obsługę manualną - dwa klawisze lub rotary knob - ręczne ustawianie poziomu każdego rodzaju światła
// dodać wyświetlacz ?

DS3231 clock;
RTCDateTime dt;
RTCDateTime sdt;

typedef enum LEDType_t {
  Day = 0,
  Night,
  RB,
  Cold,
  COUNT
} LEDType;

typedef struct LED_t {
  int target_lvl;
  int level;
  int pin;
} LED;

typedef struct Interval_t {
  LEDType lt;
  int begin;
  int end;
  int level;      //nowa pozycja - mozliwosc sterowania jasnoscia zestawu
} Interval;

const int MIN_LEVEL = 0;
const int MAX_LEVEL = 255;
const int DAY_BEGIN = 0;
const int DAY_END = 2400;
const int LED_DAY = 9;       // pwm pin 0
const int LED_RB = 6;        // pwm pin 1
const int LED_NIGHT = 10;    // pwm pin 2
const int LED_COLD = 5;      // pwm pin 3
const int ONEWIRE_PIN = 2;   // termosonda
const int HEAT = 4;          // digital pin 4

//  int work_mode = 0;       // do ewentualnego trybu recznego - uzycie knoba
int teraz;
int kiedys;
int yyyy;
int mm;
int dd;
int akt_level;

byte address[8] = {0x28, 0xB9, 0x77, 0x45, 0x92, 0x13, 0x2, 0xB0};

OneWire onewire(ONEWIRE_PIN);
DS18B20 sensors(&onewire);

LED leds[COUNT] = {
  { akt_level, akt_level, LED_DAY },
  { akt_level, akt_level, LED_NIGHT },
  { akt_level, akt_level, LED_RB },
  { akt_level, akt_level, LED_COLD }
};

//const int INTERVALS_COUNT = 19;

//Interval intervals[INTERVALS_COUNT] = {
//  { Night,  730,  830,  50 },
//  { Cold,   730,  800,  30 },
//  { Cold,   800,  830,  50 },
//  { Day,    815,  900,  30 },
//  { Cold,   831, 1300, 255 },
//  { Night,  831,  915, 250 },
//  { Day,    901, 1300, 255 },
//  { RB,     915, 1245, 255 },
//  { Cold,  1500, 2000, 255 },
//  { RB,    1515, 1930, 255 },
//  { Day,   1515, 2000, 255 },
//  { Night, 1930, 2200, 150 },
//  { Day,   2001, 2100,  50 },
//  { Cold,  2001, 2100,  40 },
//  { RB,    2100, 2130,  10 },
//  { Day,   2101, 2130,  20 },
//  { Cold,  2101, 2200,  10 },
//  { Cold,  2201, 2230,   5 },
//  { Night, 2201, 2230,   8 },
//};

// const int INTERVALS_COUNT = 13;
// Interval intervals[INTERVALS_COUNT] = {
Interval intervals[INTERVALS_COUNT] = {
  { Night,  550,  700,  20 },
  { Cold,   600,  700,  20 },
  { Day,    615,  700,  20 },
  { Cold,  1400, 2000, 255 },
  { RB,    1515, 1930, 255 },
  { Day,   1515, 2000, 255 },
  { Night, 1930, 2100, 100 },
  { Day,   2001, 2100,  30 },
  { Cold,  2001, 2100,  30 },
  { Day,   2101, 2130,  15 },
  { Cold,  2101, 2200,   5 },
  { Cold,  2201, 2230,   3 },
  { Night, 2130, 2230,   5 },
};

void reset_lvl(LEDType lt);
void change_lvl(LEDType lt);
boolean isWithinInterval(int hourmin, int index);
boolean isWithinInterval(int hourmin, int begin, int end);
void analogWrite(LEDType lt);

void setup() {
  pinMode(LED_DAY, OUTPUT);
  pinMode(LED_RB, OUTPUT);
  pinMode(LED_NIGHT, OUTPUT);
  pinMode(LED_COLD, OUTPUT);
  pinMode(HEAT, OUTPUT);
  //  pinMode(TEMP, INPUT);
  
  
  while (!Serial);
  Serial.begin(9600);     //inicjalizacja monitora szeregowego
  clock.begin();            // Inicjalizacja DS3231
  
  //czytanie temperatury
  sensors.begin();
  sensors.request(address);

  // następna linia do ustawienia zegarka, normalnie wyremowana
  // clock.setDateTime(__DATE__, __TIME__);

  for (int i = 0; i < COUNT; ++i) {
    for (int ii = 0; ii < 100; ++ii) {
      analogWrite(leds[i].pin, ii);
      delay(20);
    }
    for (int ii = 100; ii > 0; --ii) {
      analogWrite(leds[i].pin, ii);
      delay(20);
    }
  }

  
}

void loop() {
  const int H2O_TEMP = 23;    // temperatura wody
  const double EPSILON = 0.3; // dopuszczalne wahania +/- x.x 'C
  const double HYSTERESIS_UP = H2O_TEMP + EPSILON;
  const double HYSTERESIS_DOWN = H2O_TEMP - EPSILON;

  dt = clock.getDateTime();
  teraz = 100 * dt.hour + dt.minute;
  // if (teraz == 200) {        // korekta nedznego, chinskiego wzorca czasu
  //   clock.setDateTime(dt.year, dt.month, dt.day, dt.hour, 03, 00);
  // }
  Serial.println();
  Serial.println(clock.dateFormat("d-m-Y H:i:s", dt));
  Serial.println(teraz);

//   obsluga sondy temperatury
  if (sensors.available()) {

    float temperature = sensors.readTemperature(address);
    
    if (temperature < HYSTERESIS_DOWN) {
      digitalWrite(HEAT, LOW); //włączam
      Serial.println("PIN 4 ON - grzalka wlaczona, LOW");
    }
    if (temperature > HYSTERESIS_UP) {
      digitalWrite(HEAT, HIGH);  //wyłączam
      Serial.println("PIN 4 OFF - grzalka wylaczona, HIGH");
    }
    Serial.print(temperature);
    Serial.println(F(" 'C"));
    sensors.request(address);
  }
  

  for (int i = 0; i < COUNT; ++i) {
    reset_lvl((LEDType)i);
  }

  for (int i = 0; i < INTERVALS_COUNT; ++i) {
    if (isWithinInterval(teraz, i) == true) {
      leds[intervals[i].lt].target_lvl = intervals[i].level;
      Serial.print("ustawiam:  ");
      Serial.print(intervals[i].lt);
      Serial.print("  -->  ");
      Serial.println(leds[intervals[i].lt].target_lvl);
    }
  }

  for (int i = 0; i < COUNT; ++i) {
    change_lvl((LEDType)i);
  }

  analogWrite(Day);
  analogWrite(Night);
  analogWrite(RB);
  analogWrite(Cold);

  delay(1000);
}

void reset_lvl(LEDType lt) {
  leds[lt].target_lvl = 0;
}

void change_lvl(LEDType lt) {
  if (leds[lt].level > leds[lt].target_lvl) {
    --leds[lt].level;
  }
  if (leds[lt].level < leds[lt].target_lvl) {
    ++leds[lt].level;
  }
}

boolean isWithinInterval(int hourmin, int index) {
  return isWithinInterval(hourmin, intervals[index].begin, intervals[index].end);
}

boolean isWithinInterval(int hourmin, int begin, int end) {
  if (begin > end) {
    return (begin <= hourmin && hourmin < DAY_END) || (DAY_BEGIN <= hourmin && hourmin < end);
  }
  else {
    return begin <= hourmin && hourmin < end;
  }
}

void analogWrite(LEDType lt) {
  analogWrite(leds[lt].pin, leds[lt].level);
  Serial.print(leds[lt].level);
  Serial.print("  --->  ");
  Serial.println(leds[lt].target_lvl);
}
