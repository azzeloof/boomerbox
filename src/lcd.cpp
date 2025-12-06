#include "lcd.h"

// Include the structs we need - these should ideally be in their own header
// For now, we'll define them here to match main.cpp

Lcd::Lcd(const uint8_t i2cAddr) : _lcd(i2cAddr), _buffer{}
{
}

bool Lcd::begin() {
    if (! _lcd.begin(COLS, ROWS)) {
        return false;
    }
    _lcd.createChar(0, upArrow);
    _lcd.createChar(1, downArrow);

    // Initialize the buffer
    for (uint8_t i = 0; i < ROWS; i++) {
        for (uint8_t j = 0; j < COLS; j++) {
            _buffer[i][j] = ' ';
        }
    }

    return true;
}

void Lcd::set_backlight(const bool on) {
    _lcd.setBacklight(on ? 1 : 0);
}

void Lcd::clear() {
    _lcd.clear();
    for (uint8_t i = 0; i < ROWS; i++) {
        for (uint8_t j = 0; j < COLS; j++) {
            _buffer[i][j] = ' ';
        }
    }
}

void Lcd::display_line(const String& text, const uint8_t line, const bool center) {
    if (line >= ROWS) return;
    String displayText = text;
    uint8_t length = displayText.length();

    // Truncate if too long
    if (length > COLS) {
        displayText = displayText.substring(0, COLS);
        length = COLS;
    }
    uint8_t start = 0;
    if (center) {
        start = (COLS - length) / 2;
    }
    for (uint8_t i = 0; i < length; i++) {
        if (text[i] != _buffer[line][i+start]) {
            _lcd.setCursor(i+start, line);
            _lcd.write(displayText[i]);
            _buffer[line][i+start] = displayText[i];
        }
    }
}

void Lcd::display_character(const char c, const uint8_t line, const uint8_t col) {
    if (col >= COLS || line >= ROWS) return;
    if (c != _buffer[line][col]) {
        _lcd.setCursor(col, line);
        _lcd.write(c);
        _buffer[line][col] = c;
    }
}

void Lcd::display_progress(const uint32_t elapsed, const uint32_t duration, uint8_t line) {
    _lcd.setCursor(0, line);

    // Format: "MM:SS / MM:SS"
    const uint8_t elapsedMin = elapsed / 60;
    const uint8_t elapsedSec = elapsed % 60;
    const uint8_t durationMin = duration / 60;
    const uint8_t durationSec = duration % 60;

    // Print elapsed time
    if (elapsedMin < 10) _lcd.print('0');
    _lcd.print(elapsedMin);
    _lcd.print(':');
    if (elapsedSec < 10) _lcd.print('0');
    _lcd.print(elapsedSec);

    _lcd.print(" / ");

    // Print duration
    if (durationMin < 10) _lcd.print('0');
    _lcd.print(durationMin);
    _lcd.print(':');
    if (durationSec < 10) _lcd.print('0');
    _lcd.print(durationSec);
}

void Lcd::display_playing(const Song* song, const uint32_t elapsed) {
    if (!song) return;

    display_line(song->title, 0);
    display_line(song->album, 1);
    display_line(song->artist, 2);
    display_progress(elapsed, song->duration, 3);
}

void Lcd::display_album_list(const Album* albums, const uint8_t albumCount, const uint8_t selectedIndex) {
    if (albumCount > 0 && albums != nullptr) {
        const Album* selected = &albums[selectedIndex];
        //_lcd.setCursor(0, 0);
        if (selectedIndex > 0) display_character(0x00, 3, 0);//_lcd.write(0);
        display_line(selected->artist, 1);
        display_line(selected->title, 2);
        //_lcd.setCursor(0, 3);
        if (selectedIndex < albumCount - 1)
        {
            display_line(" (" + String(selectedIndex + 1) + "/" + String(albumCount) + ")", 3, false);
            display_character(0x01, 3, 0);
            //display_character(*" ", 3, COLS - 1);
            //_lcd.print(" ");
        } else {
            display_line("(" + String(selectedIndex + 1) + "/" + String(albumCount) + ")", 3, false);
        }
    } else {
        display_line("No albums found!", 1);
        display_line("Check the SD Card.", 2);
    }
}

void Lcd::display_splash(const String& title, const String& subtitle) {
    display_line(title, 1);
    display_line(subtitle, 2);
}

void Lcd::display_error(const String& message) {
    display_line("ERROR", 1);
    display_line(message, 2);
}
