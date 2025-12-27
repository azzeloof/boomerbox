#include <lcd.h>

uint8_t up_arrow[] = {
    0b00100, 0b01110, 0b11111, 0b00100, 0b00100, 0b00100, 0b00000, 0b00000
};

uint8_t down_arrow[] = {
    0b00000, 0b00000, 0b00100, 0b00100, 0b00100, 0b11111, 0b01110, 0b00100
};


Lcd::Lcd(const uint8_t i2cAddr) : _lcd(i2cAddr), _buffer{}
{
}

bool Lcd::begin() {
    if (! _lcd.begin(COLS, ROWS)) {
        return false;
    }
    _lcd.createChar(CHAR_UP, up_arrow);
    _lcd.createChar(CHAR_DOWN, down_arrow);

    // Initialize the buffer
    for (auto & i : _buffer) {
        for (char & j : i) {
            j = ' ';
        }
    }

    return true;
}

void Lcd::set_backlight(const bool on) {
    _lcd.setBacklight(on ? 1 : 0);
}

void Lcd::clear_buffer() {
    for (auto & i : _buffer) {
        for (char & j : i) {
            j = ' ';
        }
    }
}

void Lcd::clear() {
    clear_buffer();
    _lcd.clear();
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
    for (uint8_t i=0; i<start; i++) {
        displayText = " " + displayText;
    }
    for (uint8_t i=start+length; i<COLS; i++) {
        displayText += " ";
    }
    for (uint8_t i=0; i<COLS; i++) {
        if (displayText[i] != _buffer[line][i]) {
            _lcd.setCursor(i, line);
            _lcd.write(displayText[i]);
            _buffer[line][i] = displayText[i];
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

void Lcd::display_progress(const uint32_t elapsed, const uint32_t duration, uint8_t index, uint8_t total, uint8_t line) {
    String time = "";

    // Format: "MM:SS / MM:SS"
    const uint8_t elapsedMin = elapsed / 60;
    const uint8_t elapsedSec = elapsed % 60;
    const uint8_t durationMin = duration / 60;
    const uint8_t durationSec = duration % 60;

    if (elapsedMin < 10) time += "0";
    time += String(elapsedMin) + ":";
    if (elapsedSec < 10) time += "0";
    time += String(elapsedSec) + "/";
    if (durationMin < 10) time += "0";
    time += String(durationMin) + ":";
    if (durationSec < 10) time += "0";
    time += String(durationSec);

    const String album_progress = "(" + String(index) + "/" + String(total) + ")";
    String text = time;
    for (uint8_t i=0; i<COLS-album_progress.length()-time.length(); i++) {
        text += ' ';
    }
    text += album_progress;
    
    display_line(text, line, false);
}

void Lcd::display_playing(const Song* song, const Album* album, const uint32_t elapsed) {
    if (!song) return;
    uint8_t n_songs = album->song_count;
    if (n_songs == 1 && album->title == song->album) {
        // If there is only one song, and it is titled the same as the album, only show the name once
        // This is mainly for classical pieces
        display_line(" ", 0);
    } else {
        display_line(song->title, 0);
    }
    display_line(song->album, 1);
    display_line(song->artist, 2);
    display_progress(elapsed, song->duration, song->trackNumber, n_songs, 3);
}

void Lcd::display_album_list(const Album* albums, const uint16_t albumCount, const uint16_t selectedIndex) {
    if (albumCount > 0 && albums != nullptr) {
        const Album* selected = &albums[selectedIndex];
        if (selectedIndex > 0) {
            display_line(String(static_cast<char>(CHAR_UP)), 0, false);
        } else {
            display_line("", 0, false);
        }
        display_line(selected->artist, 1);
        display_line(selected->title, 2);
        if (selectedIndex < albumCount - 1) {
            display_line(String(static_cast<char>(CHAR_DOWN)) + " (" + String(selectedIndex + 1) + "/" + String(albumCount) + ")", 3, false);
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
