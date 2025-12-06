//
// Created by adam on 12/6/25.
//

#ifndef BOOMERBOX_MEDIA_H
#define BOOMERBOX_MEDIA_H

#include <Arduino.h>

// Memory limits
// Each unloaded album uses ~100 bytes (3 Strings + metadata)
// With ~150KB available heap, 256 albums = ~25KB, leaving plenty for loaded songs
constexpr uint16_t MAX_ALBUMS = 256;
constexpr uint8_t MAX_SONGS_PER_ALBUM = 32;
constexpr uint8_t MAX_SCAN_DEPTH = 8;

struct Song {
    String title;
    String artist;
    String album;
    String filename;
    uint32_t duration = 0;
    uint8_t trackNumber = 0;
};

struct Album {
    String title;
    String artist;
    String path;
    Song* songs = nullptr;
    uint8_t song_count = 0;
    uint8_t expected_song_count = 0;
    bool loaded = false;  // True if songs have been loaded
    
    // Free songs array to reclaim memory
    void unload() {
        if (songs) {
            delete[] songs;
            songs = nullptr;
        }
        song_count = 0;
        loaded = false;
    }
};

inline const char* media_extensions[] = {"mp3", "wav", "ogg", "flac"};
constexpr uint8_t NUM_MEDIA_EXTENSIONS = 4;

// Check if a filename has a supported audio extension
inline bool isAudioFile(const char* filename) {
    String name = filename;
    int lastDot = name.lastIndexOf('.');
    if (lastDot < 0) return false;
    String ext = name.substring(lastDot + 1);
    ext.toLowerCase();
    
    for (uint8_t i = 0; i < NUM_MEDIA_EXTENSIONS; i++) {
        if (ext == media_extensions[i]) return true;
    }
    return false;
}

#endif //BOOMERBOX_MEDIA_H