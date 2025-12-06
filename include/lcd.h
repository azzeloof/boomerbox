#ifndef LCD_H
#define LCD_H

#include <Arduino.h>
#include <Adafruit_LiquidCrystal.h>
#include <media.h>


#define CHAR_UP 1
#define CHAR_DOWN 2

inline byte up_arrow[8] = {
    0b00100, 0b01110, 0b11111, 0b00100, 0b00100, 0b00100, 0b00000, 0b00000
  };

inline byte down_arrow[8] = {
    0b00000, 0b00000, 0b00100, 0b00100, 0b00100, 0b11111, 0b01110, 0b00100
  };

class Lcd {
public:
    static constexpr uint8_t COLS = 20;
    static constexpr uint8_t ROWS = 4;

    explicit Lcd(uint8_t i2cAddr);

    // Initialize the LCD. Returns true on success.
    bool begin();

    // Turn backlight on/off
    void set_backlight(bool on);

    // Clear the display
    void clear();

    // Display a single line of text, optionally centered
    void display_line(const String& text, uint8_t line, bool center = true);

    void display_character(char c, uint8_t line, uint8_t col);

    void clear_buffer();

    // Display the "now playing" screen
    void display_playing(const Song* song, uint32_t elapsed);

    // Display the album selection list
    void display_album_list(const Album* albums, uint16_t albumCount, uint16_t selectedIndex);

    // Display an initialization/splash screen
    void display_splash(const String& title, const String& subtitle);

    // Display an error message
    void display_error(const String& message);

private:
    Adafruit_LiquidCrystal _lcd;
    char _buffer[4][20];
    // Display a progress bar showing elapsed/duration
    void display_progress(uint32_t elapsed, uint32_t duration, uint8_t line);


};

#endif // LCD_H
