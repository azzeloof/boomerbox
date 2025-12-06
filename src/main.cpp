#include <Arduino.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <metadata_parser.h>
#include <lcd.h>
#include <Adafruit_seesaw.h>
#include <pindefs.h>
#include <media.h>

#define DEBUG 1 // only enable for usb tethered operation

// Every song should have title, artist, album, and track number metadata for proper cataloging
// A directory can only contain either other directories, or songs. A directory containing songs
// is considered an album.

enum class State {
    INITIALIZING,
    IDLE,
    PLAYING,
    PAUSED,
    STOPPED,
    ERROR
};

struct ButtonStates {
    bool back = false;
    bool play = false;
    bool stop = false;
    bool next = false;
    bool up = false;
    bool down = false;
};

Adafruit_VS1053_FilePlayer musicPlayer =
    Adafruit_VS1053_FilePlayer(
        VS1053_RESET,
        VS1053_CS,
        VS1053_DCS,
        VS1053_DREQ,
        CARDCS
        );

Lcd lcd(LCD_I2C_ADDR);

Adafruit_seesaw ss0;
Adafruit_seesaw ss1;

State player_state = State::IDLE;
boolean sd_card_present = false;
ButtonStates button_states = ButtonStates();
//Album* albums = nullptr;
Album albums[1024];
uint8_t n_albums = 0;
uint16_t n_songs = 0;
uint8_t album_list_index = 0;
Album* current_album = nullptr;
Song* current_song = nullptr;
uint32_t elapsed = 0;

void poll_buttons() {
    button_states.back = !ss0.digitalRead(BTN_BACK);
    button_states.play = !ss0.digitalRead(BTN_PLAY);
    button_states.stop = !ss0.digitalRead(BTN_STOP);
    button_states.next = !ss0.digitalRead(BTN_NEXT);
    button_states.up = !ss1.digitalRead(BTN_UP);
    button_states.down = !ss1.digitalRead(BTN_DOWN);
}

Song file_to_song(File file) {
    Song song;
    song.filename = file.name();
    Serial.println(file.name());
    if (SongMetadata metadata; parseMetadata(file, metadata)) {
        song.title = metadata.title;
        song.artist = metadata.artist;
        song.album = metadata.album;
        song.duration = metadata.duration;
    }
    return song;
}

void scan_dir(File dir) {
    if (File file = dir.openNextFile(); file.isDirectory()) {
        // This is not an album, keep scanning deeper
        scan_dir(file);
        file.close();
    } else {
        // The first file is not another directory. This is an album.
        // Count the songs in th
        String extension = getFileExtension(file.name());
        extension.toLowerCase();
        bool is_media = false;
        for (int i=0; i<sizeof(media_extensions); i++) {
            if (extension == media_extensions[i]) {
                is_media = true;
            }
        }
        if (is_media) {
            Serial.println(file.name());
            Song song = file_to_song(file);
            for (int j=0; j<n_albums; j++) {
                if (albums[j].artist == song.artist && albums[j].title == song.album) {
                    // The album exists, add the song to it.
                    continue;
                } else {
                    // The album does not exist. Create it and add the song.
                }
            }
        }
    }
}

void scan_songs() {
    File base_dir = SD.open("/");
    scan_dir(base_dir);
    base_dir.close();

    // loop through to enumerate albums
    /*
    uint8_t album_count = 0;
    while (File entry = base_dir.openNextFile()) {
        if (entry.isDirectory()) {
            album_count++;
            entry.close();
        }
    }
    albums = new Album[album_count];
    n_albums = album_count;

    // loop back through to catalog albums and songs
    base_dir.rewindDirectory();
    uint8_t album_index = 0;
    while (File entry = base_dir.openNextFile()) {
        if (entry.isDirectory()) {
            Serial.println(entry.name());
            uint8_t song_count = 0;

            // First pass: count songs
            while (File song = entry.openNextFile()) {
                if (!song.isDirectory()) {
                    song_count++;
                }
                song.close();
            }

            // Allocate memory for songs
            if (song_count > 0) {
                albums[album_index].songs = new Song[song_count];
                albums[album_index].song_count = song_count;

                // Second pass: populate songs
                entry.rewindDirectory();
                uint8_t song_index = 0;
                while (File song = entry.openNextFile()) {
                    if (!song.isDirectory()) {
                        albums[album_index].songs[song_index] = file_to_song(song);
                        song_index++;
                    }
                    song.close();
                }

                // Set album metadata from the first song
                albums[album_index].title = albums[album_index].songs[0].album;
                albums[album_index].artist = albums[album_index].songs[0].artist;
            }

            n_songs += song_count;
            albums[album_index].path = entry.name();
            album_index++;
        }
        entry.close();
    }
    base_dir.close();
    */
}


void play_album(Album* album) {
    elapsed = 0;
    current_album = album;
    current_song = album->songs; // TODO: what happens if the album is empty?
    String const path = String(album->path + "/" + current_song->filename);
    musicPlayer.startPlayingFile(path.c_str());
}

void pause() {

}

void resume() {

}

void stop() {

}

void setup() {
    player_state = State::INITIALIZING;
    Serial.begin(115200);

    #if DEBUG
        while (!Serial) { delay(1); } // halt until serial port is opened
    #endif

    delay(500);

    Serial.println("Initializing LCD...");
    if (! lcd.begin()) {

    }
    lcd.set_backlight(true);
    Serial.println("LCD initialized successfully!");
    lcd.display_line("BoomerBox", 1);
    lcd.display_line("Initializing", 2);

    Serial.println("Initializing VS1053...");
    if (! musicPlayer.begin()) {
        Serial.println("Failed to initialize VS1053!");
        player_state = State::ERROR;
        return;
    }
    Serial.println("VS1053 initialized successfully!");

    Serial.println("Initializing SD card...");
    if (! SD.begin(CARDCS)) {
        Serial.println("Failed to initialize SD card!");
        sd_card_present = false;
    } else {
        sd_card_present = true;
        Serial.println("SD card initialized successfully!");
    }

    Serial.println("Initializing Buttons...");
    if (! ss0.begin(SS0_I2C_ADDR)) {
        Serial.println("Failed to initialize Seesaw 0!");
        player_state = State::ERROR;
        return;
    }
    if (! ss1.begin(SS1_I2C_ADDR)) {
        Serial.println("Failed to initialize Seesaw 1!");
        player_state = State::ERROR;
        return;
    }
    ss0.pinMode(BTN_BACK, INPUT_PULLUP);
    ss0.pinMode(BTN_PLAY, INPUT_PULLUP);
    ss0.pinMode(BTN_STOP, INPUT_PULLUP);
    ss0.pinMode(BTN_NEXT, INPUT_PULLUP);
    ss1.pinMode(BTN_UP, INPUT_PULLUP);
    ss1.pinMode(BTN_DOWN, INPUT_PULLUP);
    Serial.println("Buttons initialized successfully!");

    if (sd_card_present) {
        Serial.println("Scanning for songs...");
        scan_songs();
        Serial.println("Scan complete!");
        Serial.println("Found " + String(n_songs) + " songs in " + String(n_albums) + " albums.");
    }

    Serial.println("Ready to play!");
    player_state = State::IDLE;
    lcd.display_splash("BoomerBox", "Ready to play!");
    delay(2000);
}

void loop() {
    //handleLcdUpdate();
    switch (player_state) {
        case State::INITIALIZING:
            // This should never happen
            Serial.println("Player is in the initializing state, but it shouldn't be!");
            Serial.println("Moving to IDLE state.");
            player_state = State::IDLE;
            delay(1000);
            break;
        case State::IDLE:
            // No music is playing, display the album list
            poll_buttons();
            if (button_states.up) {
                if (album_list_index > 0) {
                    album_list_index--;
                }
            } else if (button_states.down) {
                if (album_list_index < n_albums - 1) {
                    album_list_index++;
                }
            } else if (button_states.play) { // TODO: consider re-ordering
                play_album(albums + album_list_index);
                player_state = State::PLAYING;
            }
            lcd.display_album_list(albums, n_albums, album_list_index);
            //display_list();
            break;
        case State::PLAYING:
            // A song is playing, display the name, artist, etc.
            poll_buttons();
            lcd.display_playing(current_song, elapsed);
            //display_playing(current_song);
            break;
        case State::PAUSED:
            // A song is playing but paused.
            // flash the PLAY button
            poll_buttons();
            //display_playing(current_song);
            break;
        case State::STOPPED:
            // we may not need this state, because pressing STOP in PLAYING can just go right to IDLE
            break;
        case State::ERROR:
            // Unrecoverable error, exit
            Serial.println("Player is in an error state!");
            delay(1000);
            break;
        default:
            // This should never happen
            Serial.println("Player is in an unknown state: " + String(static_cast<int>(player_state)) + "");
            delay(1000);
            break;
    }
}