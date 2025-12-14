//
// Created by adam on 12/5/25.
//

#ifndef BOOMERBOX_PINDEFS_H
#define BOOMERBOX_PINDEFS_H

#define VS1053_RESET -1 // Not used

// Feather ESP8266
#if defined(ESP8266)
  #define VS1053_CS      16     // VS1053 chip select pin (output)
  #define VS1053_DCS     15     // VS1053 Data/command select pin (output)
  #define CARDCS          2     // Card chip select pin
  #define VS1053_DREQ     0     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP32-C6
#elif defined(ARDUINO_ADAFRUIT_FEATHER_ESP32C6)
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS      8     // VS1053 Data/command select pin (output)
  #define CARDCS          5     // Card chip select pin
  #define VS1053_DREQ     7     // VS1053 Data request, ideally an Interrupt pin

// Feather ESP32
#elif defined(ESP32) && !defined(ARDUINO_ADAFRUIT_FEATHER_ESP32S2)
  #define VS1053_CS      32     // VS1053 chip select pin (output)
  #define VS1053_DCS     33     // VS1053 Data/command select pin (output)
  #define CARDCS         14     // Card chip select pin
  #define VS1053_DREQ    15     // VS1053 Data request, ideally an Interrupt pin

// Feather Teensy3
#elif defined(TEENSYDUINO)
  #define VS1053_CS       3     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          8     // Card chip select pin
  #define VS1053_DREQ     4     // VS1053 Data request, ideally an Interrupt pin

// WICED feather
#elif defined(ARDUINO_STM32_FEATHER)
  #define VS1053_CS       PC7     // VS1053 chip select pin (output)
  #define VS1053_DCS      PB4     // VS1053 Data/command select pin (output)
  #define CARDCS          PC5     // Card chip select pin
  #define VS1053_DREQ     PA15    // VS1053 Data request, ideally an Interrupt pin

#elif defined(ARDUINO_NRF52832_FEATHER )
  #define VS1053_CS       30     // VS1053 chip select pin (output)
  #define VS1053_DCS      11     // VS1053 Data/command select pin (output)
  #define CARDCS          27     // Card chip select pin
  #define VS1053_DREQ     31     // VS1053 Data request, ideally an Interrupt pin

// Feather RP2040
#elif defined(ARDUINO_ADAFRUIT_FEATHER_RP2040)
  #define VS1053_CS       8     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          7     // Card chip select pin
  #define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin

// Feather M4, M0, 328, ESP32S2, nRF52840 or 32u4
#else
  #define VS1053_CS       6     // VS1053 chip select pin (output)
  #define VS1053_DCS     10     // VS1053 Data/command select pin (output)
  #define CARDCS          5     // Card chip select pin
  // DREQ should be an Int pin *if possible* (not possible on 32u4)
  #define VS1053_DREQ     9     // VS1053 Data request, ideally an Interrupt pin

#endif

#define LCD_I2C_ADDR 0x20

#define SS_I2C_ADDR 0x3A

#define BTN_PLAY 18
#define BTN_STOP 19
#define BTN_UP 20
#define BTN_DOWN 2
#define LED_PLAY 12
#define LED_STOP 13
#define LED_UP 0
#define LED_DOWN 1

#define VOL_KNOB 14

#endif //BOOMERBOX_PINDEFS_H