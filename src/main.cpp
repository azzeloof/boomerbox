#include <Arduino.h>
#include <Adafruit_VS1053.h>
#include <SD.h>
#include <metadata_parser.h>
#include <lcd.h>
#include <Adafruit_seesaw.h>
#include <pindefs.h>
#include <media.h>

#define DEBUG 1 // only enable for usb tethered operation

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

// Album storage - only metadata, songs loaded on demand
Album albums[MAX_ALBUMS];
uint16_t n_albums = 0;
uint16_t album_list_index = 0;
Album* current_album = nullptr;
Song* current_song = nullptr;
uint8_t current_song_index = 0;
uint32_t elapsed = 0;

void poll_buttons() {
    button_states.back = !ss0.digitalRead(BTN_BACK);
    button_states.play = !ss0.digitalRead(BTN_PLAY);
    button_states.stop = !ss0.digitalRead(BTN_STOP);
    button_states.next = !ss0.digitalRead(BTN_NEXT);
    button_states.up = !ss1.digitalRead(BTN_UP);
    button_states.down = !ss1.digitalRead(BTN_DOWN);
}

// ============================================================================
// LAZY LOADING IMPLEMENTATION
// ============================================================================

void unloadAlbumsExcept(const int16_t keepIndex) {
    for (uint16_t i = 0; i < n_albums; i++) {
        if (i != keepIndex && albums[i].loaded) {
            Serial.print("Unloading album: ");
            Serial.println(albums[i].title);
            albums[i].unload();
        }
    }
}

// Count audio files in a directory (without loading metadata)
uint8_t countAudioFiles(File& dir) {
    uint8_t count = 0;
    dir.rewindDirectory();
    while (File entry = dir.openNextFile()) {
        if (!entry.isDirectory() && isAudioFile(entry.name())) {
            count++;
            if (count >= MAX_SONGS_PER_ALBUM) {
                entry.close();
                break;
            }
        }
        entry.close();
    }
    return count;
}

// Load full song details for an album
bool loadAlbumSongs(Album* album) {
    if (!album || album->loaded) return album != nullptr;

    // First, unload other albums to free memory
    int16_t albumIndex = -1;
    for (uint16_t i = 0; i < n_albums; i++) {
        if (&albums[i] == album) {
            albumIndex = i;
            break;
        }
    }
    unloadAlbumsExcept(albumIndex);

    Serial.print("Loading songs for: ");
    Serial.println(album->title);

    File dir = SD.open(album->path);
    if (!dir) {
        Serial.print("Failed to open: ");
        Serial.println(album->path);
        return false;
    }

    // Count files first
    const uint8_t fileCount = countAudioFiles(dir);
    if (fileCount == 0) {
        dir.close();
        return false;
    }

    // Allocate the songs array
    uint8_t const allocCount = min(fileCount, MAX_SONGS_PER_ALBUM);
    album->songs = new (std::nothrow) Song[allocCount];
    if (!album->songs) {
        Serial.println("Failed to allocate songs!");
        dir.close();
        return false;
    }

    // Load each song
    dir.rewindDirectory();
    uint8_t songIndex = 0;
    bool hasValidTrackNumbers = false;
    uint8_t tracksWithNumbers = 0;

    while (File entry = dir.openNextFile()) {
        if (songIndex >= allocCount) {
            entry.close();
            break;
        }

        if (!entry.isDirectory() && isAudioFile(entry.name())) {
            Song& song = album->songs[songIndex];
            song.filename = entry.name();

            if (SongMetadata metadata; parseMetadata(entry, metadata)) {
                song.title = metadata.title;
                song.artist = metadata.artist;
                song.album = metadata.album;
                song.duration = metadata.duration;
                song.trackNumber = metadata.trackNumber;
                
                if (metadata.trackNumber > 0) {
                    tracksWithNumbers++;
                    // Consider track numbers valid if they're reasonable
                    // (not impossibly high for a normal album)
                    if (metadata.trackNumber <= 99) {
                        hasValidTrackNumbers = true;
                    }
                }
            } else {
                // Fallback to filename
                song.title = entry.name();
                song.artist = album->artist;
                song.album = album->title;
                song.trackNumber = 0;
                song.duration = 0;
            }

            songIndex++;
        }
        entry.close();
    }

    dir.close();
    album->song_count = songIndex;
    album->loaded = true;

    // Decide whether to sort based on track numbers
    // Sort if: we have valid track numbers AND at least half the songs have them

    if (bool shouldSort = hasValidTrackNumbers && (tracksWithNumbers >= (songIndex + 1) / 2)) {
        // Log any suspicious metadata before sorting
        for (uint8_t i = 0; i < album->song_count; i++) {
            if (Song& song = album->songs[i]; song.trackNumber > album->song_count && song.trackNumber > 0) {
                Serial.print("Warning: ");
                Serial.print(song.title);
                Serial.print(" has track number ");
                Serial.print(song.trackNumber);
                Serial.print(" but album only has ");
                Serial.print(album->song_count);
                Serial.println(" songs");
            }
        }
        
        // Stable insertion sort: maintains relative order of equal elements
        // This will sort songs by track number, placing songs with trackNumber == 0 at the end
        for (uint8_t i = 1; i < album->song_count; i++) {
            const Song temp = album->songs[i];
            int8_t j = i - 1;
            
            while (j >= 0) {
                const uint8_t prevTrack = album->songs[j].trackNumber;
                const uint8_t currTrack = temp.trackNumber;
                
                // If current has a track number (non-zero)
                if (currTrack > 0) {
                    // Move previous if: previous has no track (0) OR previous track > current track
                    if (prevTrack == 0 || prevTrack > currTrack) {
                        album->songs[j + 1] = album->songs[j];
                        j--;
                    } else {
                        break;
                    }
                } else {
                    // Current has no track number (0), keep it at the end in load order
                    break;
                }
            }
            album->songs[j + 1] = temp;
        }
        
        Serial.print("Sorted ");
        Serial.print(tracksWithNumbers);
        Serial.print("/");
        Serial.print(album->song_count);
        Serial.println(" songs by track number");
    } else {
        if (tracksWithNumbers > 0) {
            Serial.print("Skipping sort: only ");
            Serial.print(tracksWithNumbers);
            Serial.print("/");
            Serial.print(album->song_count);
            Serial.println(" songs have track numbers");
        } else {
            Serial.println("No track numbers found, keeping load order");
        }
    }

    // Final song order for debugging
    Serial.println("Play order:");
    for (uint8_t i = 0; i < album->song_count; i++) {
        Serial.print("  ");
        Serial.print(i + 1);
        Serial.print(". ");
        if (album->songs[i].trackNumber > 0) {
            Serial.print("[Track ");
            Serial.print(album->songs[i].trackNumber);
            Serial.print("] ");
        }
        Serial.println(album->songs[i].title);
    }

    Serial.print("Loaded ");
    Serial.print(album->song_count);
    Serial.println(" songs");

    return true;
}

// Register an album from a directory (only reads first song for metadata)
bool registerAlbumFromDir(File& dir, const String& path) {
    if (n_albums >= MAX_ALBUMS) {
        Serial.println("Max albums reached!");
        return false;
    }

    // Look for the first audio file to get album metadata
    File firstAudio;
    bool found = false;

    dir.rewindDirectory();
    while (File entry = dir.openNextFile()) {
        if (!entry.isDirectory() && isAudioFile(entry.name())) {
            firstAudio = entry;
            found = true;
            break;
        }
        entry.close();
    }

    if (!found) {
        return false;
    }

    // Parse metadata from first file
    SongMetadata metadata;
    const bool hasMetadata = parseMetadata(firstAudio, metadata);
    firstAudio.close();

    // Create album entry
    Album& album = albums[n_albums];
    album.path = path;
    album.songs = nullptr;
    album.song_count = 0;
    album.loaded = false;

    if (hasMetadata) {
        album.title = metadata.album.length() > 0 ? metadata.album : path;
        album.artist = metadata.artist.length() > 0 ? metadata.artist : "Unknown Artist";
        album.expected_song_count = metadata.totalTracks;
    } else {
        // Fallback to directory name
        int lastSlash = path.lastIndexOf('/');
        album.title = lastSlash >= 0 ? path.substring(lastSlash + 1) : path;
        album.artist = "Unknown Artist";
        album.expected_song_count = 0;
    }

    n_albums++;

    Serial.print("Found album: ");
    Serial.print(album.artist);
    Serial.print(" - ");
    Serial.println(album.title);

    return true;
}

// Recursively scan directories for albums
void scan_dir(File& dir, const String& path, uint8_t depth) {
    if (depth > MAX_SCAN_DEPTH || n_albums >= MAX_ALBUMS) {
        return;
    }

    bool hasAudioFiles = false;
    bool hasSubdirs = false;

    // First pass: check what this directory contains
    while (File entry = dir.openNextFile()) {
        if (entry.isDirectory()) {
            hasSubdirs = true;
        } else if (isAudioFile(entry.name())) {
            hasAudioFiles = true;
        }
        entry.close();

        // Early exit if we know it's an album
        if (hasAudioFiles) break;
    }

    if (hasAudioFiles) {
        // This directory is an album - register it
        registerAlbumFromDir(dir, path);
    } else if (hasSubdirs) {
        // Recurse into subdirectories
        dir.rewindDirectory();
        while (File entry = dir.openNextFile()) {
            if (entry.isDirectory()) {
                String subPath = path.length() > 0 
                    ? path + "/" + entry.name() 
                    : String("/") + entry.name();
                scan_dir(entry, subPath, depth + 1);
            }
            entry.close();

            if (n_albums >= MAX_ALBUMS) break;
        }
    }
}

void scan_songs() {
    // Clear existing albums
    for (uint16_t i = 0; i < n_albums; i++) {
        albums[i].unload();
        albums[i].title = "";
        albums[i].artist = "";
        albums[i].path = "";
    }
    n_albums = 0;

    File root = SD.open("/");
    if (!root) {
        Serial.println("Failed to open root directory!");
        return;
    }

    scan_dir(root, "", 0);
    root.close();

    Serial.print("Scan complete: found ");
    Serial.print(n_albums);
    Serial.println(" albums");
}

// ============================================================================
// PLAYBACK
// ============================================================================

void play_next_song();
void play_prev_song();

void play_album(Album* album) {
    Serial.println("play_album()");
    if (!album) return;

    // Load songs if not already loaded
    if (!album->loaded) {
        lcd.clear();
        lcd.display_splash("Loading...", album->title);

        if (!loadAlbumSongs(album)) {
            lcd.display_error("Load failed!");
            delay(2000);
            return;
        }
    }

    if (album->song_count == 0) {
        lcd.display_error("No songs!");
        delay(2000);
        return;
    }

    elapsed = 0;
    current_album = album;
    current_song_index = 0;
    current_song = &album->songs[0];

    const String filePath = album->path + "/" + current_song->filename;
    Serial.print("Playing: ");
    Serial.println(filePath);

    if (!musicPlayer.startPlayingFile(filePath.c_str())) {
        Serial.println("Failed to start playback!");
        lcd.display_error("Playback failed!");
        delay(2000);
        current_song = nullptr;
        return;
    }
    
    // Give the player time to start
    delay(1000);
}

void play_next_song() {
    Serial.println("play_next_song()");
    if (!current_album || !current_album->loaded) return;

    if (current_song_index < current_album->song_count - 1) {
        current_song_index++;
        current_song = &current_album->songs[current_song_index];
        elapsed = 0;

        const String filePath = current_album->path + "/" + current_song->filename;
        Serial.print("Playing next: ");
        Serial.println(filePath);
        
        if (!musicPlayer.playFullFile(filePath.c_str())) { // interrupts wouldn't work. Maybe 2040 problem
            Serial.println("Failed to start playback!");
            // Try next song
            play_next_song();
            return;
        }
        
        // Give the player time to start
        delay(50);
    } else {
        // End of album
        Serial.println("End of album");
        current_song = nullptr;
        player_state = State::IDLE;
    }
}

void play_prev_song() {
    Serial.println("play_prev_song()");
    if (!current_album || !current_album->loaded) return;

    if (current_song_index > 0) {
        current_song_index--;
        current_song = &current_album->songs[current_song_index];
        elapsed = 0;

        const String filePath = current_album->path + "/" + current_song->filename;
        Serial.print("Playing prev: ");
        Serial.println(filePath);
        
        if (!musicPlayer.startPlayingFile(filePath.c_str())) {
            Serial.println("Failed to start playback!");
        }
        
        // Give the player time to start
        delay(50);
    }
}

void pause() {
    Serial.println("pause()");
    musicPlayer.pausePlaying(true);
}

void resume() {
    Serial.println("resume()");
    musicPlayer.pausePlaying(false);
}

void stop() {
    Serial.println("stop()");
    musicPlayer.stopPlaying();
    current_song = nullptr;
    current_album = nullptr;
}

// ============================================================================
// SETUP & LOOP
// ============================================================================

void setup() {
    player_state = State::INITIALIZING;
    Serial.begin(115200);

    #if DEBUG
        while (!Serial) { delay(1); }
    #endif

    delay(500);

    Serial.println("Initializing LCD...");
    if (!lcd.begin()) {
        Serial.println("LCD init failed!");
    }
    lcd.set_backlight(true);
    Serial.println("LCD initialized successfully!");
    lcd.display_splash("BoomerBox", "Initializing...");

    Serial.println("Initializing VS1053...");
    if (!musicPlayer.begin()) {
        Serial.println("Failed to initialize VS1053!");
        player_state = State::ERROR;
        return;
    }
    musicPlayer.useInterrupt(VS1053_FILEPLAYER_TIMER0_INT);
    Serial.println("VS1053 initialized successfully!");

    Serial.println("Initializing SD card...");
    if (!SD.begin(CARDCS)) {
        Serial.println("Failed to initialize SD card!");
        sd_card_present = false;
    } else {
        sd_card_present = true;
        Serial.println("SD card initialized successfully!");
    }

    Serial.println("Initializing Buttons...");
    if (!ss0.begin(SS0_I2C_ADDR)) {
        Serial.println("Failed to initialize Seesaw 0!");
        player_state = State::ERROR;
        return;
    }
    if (!ss1.begin(SS1_I2C_ADDR)) {
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
        lcd.display_splash("BoomerBox", "Scanning...");
        Serial.println("Scanning for albums...");
        scan_songs();
    }

    Serial.println("Ready to play!");
    player_state = State::IDLE;
    lcd.display_splash("BoomerBox", "Ready!");
    delay(1000);
}

void loop() {
    switch (player_state) {
        case State::INITIALIZING:
            Serial.println("Player is in the initializing state, but it shouldn't be!");
            Serial.println("Moving to IDLE state.");
            player_state = State::IDLE;
            delay(1000);
            break;

        case State::IDLE:
            poll_buttons();
            if (button_states.up) {
                if (album_list_index > 0) {
                    album_list_index--;
                }
                delay(150);
            } else if (button_states.down) {
                if (album_list_index < n_albums - 1) {
                    album_list_index++;
                }
                delay(150);
            } else if (button_states.play && n_albums > 0) {
                play_album(&albums[album_list_index]);
                if (current_song) {
                    player_state = State::PLAYING;
                }
            }
            lcd.display_album_list(albums, n_albums, album_list_index);
            break;

        case State::PLAYING:
            poll_buttons();

            // Check if song finished - use stopped() for more reliable check
            // Also add a small debounce to avoid false positives right after starting
            if (musicPlayer.stopped()) {
                play_next_song();
            }

            if (button_states.stop) {
                stop();
                player_state = State::IDLE;
            } else if (button_states.play) {
                pause();
                player_state = State::PAUSED;
                delay(200);
            } else if (button_states.next) {
                musicPlayer.stopPlaying();
                play_next_song();
                delay(200);
            } else if (button_states.back) {
                musicPlayer.stopPlaying();
                play_prev_song();
                delay(200);
            }

            if (current_song) {
                lcd.display_playing(current_song, elapsed);
            }
            break;

        case State::PAUSED:
            poll_buttons();
            if (button_states.play) {
                resume();
                player_state = State::PLAYING;
                delay(200);
            } else if (button_states.stop) {
                stop();
                player_state = State::IDLE;
            }
            if (current_song) {
                lcd.display_playing(current_song, elapsed);
            }
            break;

        case State::STOPPED:
            player_state = State::IDLE;
            break;

        case State::ERROR:
            Serial.println("Player is in an error state!");
            lcd.display_error("System Error");
            delay(1000);
            break;

        default:
            Serial.println("Player is in an unknown state!");
            player_state = State::IDLE;
            delay(1000);
            break;
    }
}