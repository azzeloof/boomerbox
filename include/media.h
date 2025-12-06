//
// Created by adam on 12/6/25.
//

#ifndef BOOMERBOX_MEDIA_H
#define BOOMERBOX_MEDIA_H

#include <Arduino.h>

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
    uint8_t song_count = 0; // How many songs are actually loaded
    uint8_t expected_song_count = 0; // How many songs there should be
};

inline String media_extensions[4] = {"mp3", "wav", "ogg", "flac"};

#endif //BOOMERBOX_MEDIA_H