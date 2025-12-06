
#ifndef METADATA_PARSER_H
#define METADATA_PARSER_H

#include <Arduino.h>
#include <SD.h>

struct SongMetadata {
    String title;
    String artist;
    String album;
    uint32_t duration; // in seconds
    uint8_t trackNumber; // track number, 0 if not found
    uint8_t totalTracks; // total tracks on album, 0 if not found
};

// Parse WAV file metadata
// Returns true if file was successfully parsed
// Falls back to filename for title if metadata is missing
// Note: Does NOT close the file - caller is responsible
bool parseWavMetadata(File &file, SongMetadata &metadata);

// Parse MP3 file metadata (ID3v1 and ID3v2 tags)
// Returns true if file was successfully parsed
// Falls back to filename for title if metadata is missing
// Note: Does NOT close the file - caller is responsible
bool parseMp3Metadata(File &file, SongMetadata &metadata);

// Parse FLAC file metadata (Vorbis comments)
// Returns true if file was successfully parsed
// Falls back to filename for title if metadata is missing
// Note: Does NOT close the file - caller is responsible
bool parseFlacMetadata(File &file, SongMetadata &metadata);

// Parse OGG Vorbis file metadata (Vorbis comments)
// Returns true if file was successfully parsed
// Falls back to filename for title if metadata is missing
// Note: Does NOT close the file - caller is responsible
bool parseOggMetadata(File &file, SongMetadata &metadata);

// Generic metadata parser - auto-detects format based on file extension
// Supports: WAV, MP3, FLAC, OGG
// Returns true if file was successfully parsed
// Note: Does NOT close the file - caller is responsible
bool parseMetadata(File &file, SongMetadata &metadata);

String getFileExtension(const char* filepath);

#endif // METADATA_PARSER_H