/*
-----------------------------
  02.20.2018
  BryanRibas@gmail.com @R1B4Z01D
  http://www.R1B4Z01D.com
  With out @voroshkov's work on Chorus-RF-Laptimer this never could have been done. I will split out his code to include the MIT notice. 
  https://github.com/voroshkov/Chorus-RF-Laptimer
  -----------------------------
*/
#include <FastLED.h>

//----Pin config
#define DATA_PIN 9
#define RSSI_ADC A2
#define SPI_DATA_PIN 10
#define SPI_SS_PIN 11
#define SPI_CLOCK_PIN 12

// Config, vars and stats
#define NUM_LEDS 75
#define RSSI_DELAY 50
#define LED_UPDATE_DELAY 10
#define RSSI_TRIGGER 230

int channels[]={0,2,4,6};
int bands[]={4,4,4,4};
int rssis[]={0,0,0,0};
int stats[]={0,0,0,0};
CRGB::HTMLColorCode pilotColor[] = {CRGB::Red,CRGB::Purple,CRGB::Blue,CRGB::Green};
int captured = 0;
int brightness = 10;
uint8_t gHue = 0;
CRGB leds[NUM_LEDS];
CRGB::HTMLColorCode lastColor = CRGB::White;

void setLEDColors(void){
  ////set every led color
  for(int i=0; i<= (NUM_LEDS/2);i++){
    leds[i] = lastColor;    
    leds[NUM_LEDS-i] = lastColor;
    delay(LED_UPDATE_DELAY);
    FastLED.show();
  }//end change led colors
}
// --------------------------------------------------------
const uint16_t channelTable[] PROGMEM = {
    // Channel 1 - 8
    0x281D, 0x288F, 0x2902, 0x2914, 0x2987, 0x2999, 0x2A0C, 0x2A1E, // Raceband 0
    0x2A05, 0x299B, 0x2991, 0x2987, 0x291D, 0x2913, 0x2909, 0x289F, // Band A 1
    0x2903, 0x290C, 0x2916, 0x291F, 0x2989, 0x2992, 0x299C, 0x2A05, // Band B 2
    0x2895, 0x288B, 0x2881, 0x2817, 0x2A0F, 0x2A19, 0x2A83, 0x2A8D, // Band E 3
    0x2906, 0x2910, 0x291A, 0x2984, 0x298E, 0x2998, 0x2A02, 0x2A0C, // Band F 4
    0x2609, 0x261C, 0x268E, 0x2701, 0x2713, 0x2786, 0x2798, 0x280B  // Band D 5
};

void SERIAL_SENDBIT1() {
    digitalWrite(SPI_CLOCK_PIN, LOW);
    delayMicroseconds(1);

    digitalWrite(SPI_DATA_PIN, HIGH);
    delayMicroseconds(1);
    digitalWrite(SPI_CLOCK_PIN, HIGH);
    delayMicroseconds(1);

    digitalWrite(SPI_CLOCK_PIN, LOW);
    delayMicroseconds(1);
}

void SERIAL_SENDBIT0() {
    digitalWrite(SPI_CLOCK_PIN, LOW);
    delayMicroseconds(1);

    digitalWrite(SPI_DATA_PIN, LOW);
    delayMicroseconds(1);
    digitalWrite(SPI_CLOCK_PIN, HIGH);
    delayMicroseconds(1);

    digitalWrite(SPI_CLOCK_PIN, LOW);
    delayMicroseconds(1);
}

void SERIAL_ENABLE_LOW() {
    delayMicroseconds(1);
    digitalWrite(SPI_SS_PIN, LOW);
    delayMicroseconds(1);
}

void SERIAL_ENABLE_HIGH() {
    delayMicroseconds(1);
    digitalWrite(SPI_SS_PIN, HIGH);
    delayMicroseconds(1);
}

void setChannelModule(uint8_t channel, uint8_t band) {
    uint8_t i;
    uint16_t channelData;

    channelData = pgm_read_word_near(channelTable + channel + (8 * band));

    // bit bang out 25 bits of data
    // Order: A0-3, !R/W, D0-D19
    // A0=0, A1=0, A2=0, A3=1, RW=0, D0-19=0
    SERIAL_ENABLE_HIGH();
    delayMicroseconds(1);
    SERIAL_ENABLE_LOW();

    SERIAL_SENDBIT0();
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT1();

    SERIAL_SENDBIT0();

    // remaining zeros
    for (i = 20; i > 0; i--) {
        SERIAL_SENDBIT0();
    }

    // Clock the data in
    SERIAL_ENABLE_HIGH();
    delayMicroseconds(1);
    SERIAL_ENABLE_LOW();

    // Second is the channel data from the lookup table
    // 20 bytes of register data are sent, but the MSB 4 bits are zeros
    // register address = 0x1, write, data0-15=channelData data15-19=0x0
    SERIAL_ENABLE_HIGH();
    SERIAL_ENABLE_LOW();

    // Register 0x1
    SERIAL_SENDBIT1();
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT0();
    SERIAL_SENDBIT0();

    // Write to register
    SERIAL_SENDBIT1();

    // D0-D15
    //   note: loop runs backwards as more efficent on AVR
    for (i = 16; i > 0; i--) {
        // Is bit high or low?
        if (channelData & 0x1) {
            SERIAL_SENDBIT1();
        }
        else {
            SERIAL_SENDBIT0();
        }
        // Shift bits along to check the next one
        channelData >>= 1;
    }

    // Remaining D16-D19
    for (i = 4; i > 0; i--) {
        SERIAL_SENDBIT0();
    }

    // Finished clocking data in
    SERIAL_ENABLE_HIGH();
    delayMicroseconds(1);

    digitalWrite(SPI_SS_PIN, LOW);
    digitalWrite(SPI_CLOCK_PIN, LOW);
    digitalWrite(SPI_DATA_PIN, LOW);
}

void updateRSSI(void){
  for(int i=0;i<4;i++){
     setChannelModule(channels[i],bands[i]);    
     delay(RSSI_DELAY);
     rssis[i] = analogRead(RSSI_ADC);
  }
}

void printData(void){
  for(int i =0;i<4;i++){
    Serial.print(rssis[i]);
    Serial.print(" ");
    Serial.print(stats[i]);
    Serial.print(" ");
  }
    Serial.println("");
}

void setup(void) {
    FastLED.addLeds<WS2811, DATA_PIN, GRB>(leds, NUM_LEDS);
    Serial.begin(9600);
    pinMode(SPI_SS_PIN, OUTPUT);
    pinMode(SPI_CLOCK_PIN, OUTPUT);
    pinMode(SPI_DATA_PIN, OUTPUT);
    //setLEDColors();
    FastLED.setBrightness(brightness);
    fill_rainbow( leds, NUM_LEDS, 0, 7);
    FastLED.show(); 
    delay(1000);
}

void loop(void) {
  updateRSSI();
  int maxRSSI = max(max(rssis[0], rssis[1]),max(rssis[2],rssis[3]));
  if (maxRSSI >= RSSI_TRIGGER){
    for(int i=0;i<4;i++){
       if(maxRSSI==rssis[i]){
        if(lastColor!=pilotColor[i]){
            stats[i]++;
            lastColor=pilotColor[i];
            setLEDColors();
            captured = 1;
        }
       }
    }
  }//end if any RSSI > RSSI_TRIGGER

  printData();  
  if(!captured){ 
    fill_rainbow( leds, NUM_LEDS, gHue, 5);
    FastLED.show();
    gHue++; // slowly cycle the "base color" through the rainbow  
  }
  
}//end loop

