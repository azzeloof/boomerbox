#include "metadata_parser.h"

// Extract filename without path and extension for fallback title
static String getFilenameWithoutExtension(const char* filepath) {
    const String path = filepath;
    int lastSlash = path.lastIndexOf('/');
    int lastDot = path.lastIndexOf('.');

    if (lastSlash < 0) lastSlash = -1;
    if (lastDot < 0 || lastDot < lastSlash) lastDot = path.length();

    return path.substring(lastSlash + 1, lastDot);
}

// Get file extension (lowercase)
String getFileExtension(const char* filepath) {
    const String path = filepath;
    const int lastDot = path.lastIndexOf('.');
    if (lastDot < 0) return "";
    String ext = path.substring(lastDot + 1);
    ext.toLowerCase();
    return ext;
}

// Read a sync safe integer (used in ID3v2)
static uint32_t readSyncSafeInt(File &file) {
    uint8_t bytes[4];
    file.read(bytes, 4);
    return (static_cast<uint32_t>(bytes[0]) << 21) | (static_cast<uint32_t>(bytes[1]) << 14) |
           (static_cast<uint32_t>(bytes[2]) << 7) | bytes[3];
}

// Read a big-endian 32-bit integer
static uint32_t readBigEndian32(File &file) {
    uint8_t bytes[4];
    file.read(bytes, 4);
    return (static_cast<uint32_t>(bytes[0]) << 24) | (static_cast<uint32_t>(bytes[1]) << 16) |
           (static_cast<uint32_t>(bytes[2]) << 8) | bytes[3];
}

// Read a little-endian 32-bit integer
static uint32_t readLittleEndian32(File &file) {
    uint32_t value;
    file.read(&value, 4);
    return value;
}

// Read a big-endian 24-bit integer
static uint32_t readBigEndian24(File &file) {
    uint8_t bytes[3];
    file.read(bytes, 3);
    return (static_cast<uint32_t>(bytes[0]) << 16) | (static_cast<uint32_t>(bytes[1]) << 8) | bytes[2];
}

static void parseTrackNumber(const char* str, uint8_t &trackNumber, uint8_t &totalTracks) {
    trackNumber = atoi(str);
    const char* slash = strchr(str, '/');
    if (slash) {
        totalTracks = atoi(slash + 1);
    }
}

// Maximum iterations for parsing loops to prevent hangs
static constexpr uint32_t MAX_PARSE_ITERATIONS = 500;

bool parseWavMetadata(File &file, SongMetadata &metadata) {
    if (!file) return false;

    file.seek(0);

    // Initialize with defaults
    metadata.title = "";
    metadata.artist = "";
    metadata.album = "";
    metadata.duration = 0;
    metadata.trackNumber = 0;
    metadata.totalTracks = 0;

    // Check for RIFF header
    char header[4];
    if (file.read(header, 4) != 4 || strncmp(header, "RIFF", 4) != 0) {
        return false;
    }

    uint32_t fileSize;
    file.read(&fileSize, 4);

    // Check for WAVE format
    if (file.read(header, 4) != 4 || strncmp(header, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint32_t dataSize = 0;

    char chunkId[5] = {0};
    uint32_t chunkSize;
    char buffer[64];

    // Parse chunks
    uint32_t iterations = 0;
    while (file.available() && iterations++ < MAX_PARSE_ITERATIONS) {
        if (file.read(chunkId, 4) != 4) break;
        if (file.read(&chunkSize, 4) != 4) break;

        const uint32_t chunkStart = file.position();

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            // Format chunk - needed for duration calculation
            file.seek(chunkStart + 2); // Skip audio format
            file.read(&channels, 2);
            file.read(&sampleRate, 4);
            file.seek(chunkStart + 14);
            file.read(&bitsPerSample, 2);

        } else if (strncmp(chunkId, "data", 4) == 0) {
            // Data chunk - needed for duration calculation
            dataSize = chunkSize;

        } else if (strncmp(chunkId, "LIST", 4) == 0) {
            // LIST chunk may contain INFO metadata
            if (file.read(header, 4) != 4) break;

            if (strncmp(header, "INFO", 4) == 0) {
                const uint32_t listEnd = chunkStart + chunkSize;
                uint32_t infoIterations = 0;

                while (file.position() < listEnd && infoIterations++ < MAX_PARSE_ITERATIONS) {
                    char infoId[5] = {0};
                    uint32_t infoSize;

                    if (file.read(infoId, 4) != 4) break;
                    if (file.read(&infoSize, 4) != 4) break;

                    const uint32_t readSize = min(infoSize, static_cast<uint32_t>(63));
                    file.read(buffer, readSize);
                    buffer[readSize] = '\0';

                    if (strcmp(infoId, "INAM") == 0) {
                        metadata.title = buffer;
                    } else if (strcmp(infoId, "IART") == 0) {
                        metadata.artist = buffer;
                    } else if (strcmp(infoId, "IPRD") == 0) {
                        metadata.album = buffer;
                    } else if (strcmp(infoId, "ITRK") == 0) {
                        parseTrackNumber(buffer, metadata.trackNumber, metadata.totalTracks);
                    }

                    // Move to the next info chunk (account for padding)
                    file.seek(file.position() - readSize + ((infoSize + 1) & ~1));
                }
            }
        }

        // Move to the next chunk (chunks are word-aligned)
        file.seek(chunkStart + ((chunkSize + 1) & ~1));
    }

    // Calculate duration from audio data
    if (sampleRate > 0 && channels > 0 && bitsPerSample > 0) {
        const uint32_t bytesPerSample = (bitsPerSample / 8) * channels;
        metadata.duration = dataSize / (sampleRate * bytesPerSample);
    }

    // Fall back to filename if no title found
    if (metadata.title.length() == 0) {
        metadata.title = getFilenameWithoutExtension(file.name());
    }

    return true;
}

bool parseMp3Metadata(File &file, SongMetadata &metadata) {
    if (!file) return false;

    file.seek(0);

    // Initialize with defaults
    metadata.title = "";
    metadata.artist = "";
    metadata.album = "";
    metadata.duration = 0;
    metadata.trackNumber = 0;
    metadata.totalTracks = 0;

    char buffer[128];

    // Try to read the ID3v2 tag first (at the beginning of the file)
    char header[10];
    if (file.read(header, 10) == 10 && strncmp(header, "ID3", 3) == 0) {
        // ID3v2 tag found
        const uint8_t majorVersion = header[3];

        // Calculate tag size (sync safe integer)
        const uint32_t tagSize = (static_cast<uint32_t>(header[6] & 0x7F) << 21) |
                          (static_cast<uint32_t>(header[7] & 0x7F) << 14) |
                          (static_cast<uint32_t>(header[8] & 0x7F) << 7) |
                          (header[9] & 0x7F);

        const uint32_t tagEnd = 10 + tagSize;

        // Parse ID3v2 frames
        uint32_t iterations = 0;
        while (file.position() < tagEnd && file.available() && iterations++ < MAX_PARSE_ITERATIONS) {
            char frameId[5] = {0};
            if (file.read(frameId, 4) != 4) break;

            // Check for padding (null bytes)
            if (frameId[0] == 0) break;

            uint32_t frameSize;
            if (majorVersion >= 4) {
                // ID3v2.4 uses sync safe integers for frame size
                frameSize = readSyncSafeInt(file);
            } else {
                // ID3v2.3 and earlier use regular integers
                frameSize = readBigEndian32(file);
            }

            // Skip frame flags
            file.seek(file.position() + 2);

            if (frameSize > 0 && frameSize < 256) {
                // Read frame content
                uint8_t encoding = 0;
                file.read(&encoding, 1);

                const uint32_t textSize = min(frameSize - 1, static_cast<uint32_t>(126));
                file.read(buffer, textSize);
                buffer[textSize] = '\0';

                // Handle different encodings (simplified - assumes ASCII/UTF-8)
                if (strcmp(frameId, "TIT2") == 0) {
                    metadata.title = buffer;
                } else if (strcmp(frameId, "TPE1") == 0) {
                    metadata.artist = buffer;
                } else if (strcmp(frameId, "TALB") == 0) {
                    metadata.album = buffer;
                } else if (strcmp(frameId, "TRCK") == 0) {
                    // Track number may be "N" or "N/M" format
                    parseTrackNumber(buffer, metadata.trackNumber, metadata.totalTracks);
                }

                // Skip remaining bytes if the frame was larger
                if (frameSize > textSize + 1) {
                    file.seek(file.position() + (frameSize - textSize - 1));
                }
            } else {
                // Skip large or empty frames
                file.seek(file.position() + frameSize);
            }
        }
    }

    // If no ID3v2 metadata found, try ID3v1 at the end of the file
    if (metadata.title.length() == 0 && metadata.artist.length() == 0) {
        // ID3v1 tag is 128 bytes at the end of the file
        if (file.size() > 128) {
            file.seek(file.size() - 128);

            char tag[4] = {0};
            if (file.read(tag, 3) == 3 && strcmp(tag, "TAG") == 0) {
                // ID3v1 tag found
                char title[31] = {0};
                char artist[31] = {0};
                char album[31] = {0};

                file.read(title, 30);
                file.read(artist, 30);
                file.read(album, 30);

                // Trim trailing spaces
                for (int i = 29; i >= 0 && title[i] == ' '; i--) title[i] = '\0';
                for (int i = 29; i >= 0 && artist[i] == ' '; i--) artist[i] = '\0';
                for (int i = 29; i >= 0 && album[i] == ' '; i--) album[i] = '\0';

                if (strlen(title) > 0) metadata.title = title;
                if (strlen(artist) > 0) metadata.artist = artist;
                if (strlen(album) > 0) metadata.album = album;
            }
            // ID3v1.1: Track number is stored at byte 126 if byte 125 is zero
            // Note: ID3v1 does not support total tracks
            file.seek(file.size() - 3);
            uint8_t zeroByte, trackNum;
            file.read(&zeroByte, 1);
            file.read(&trackNum, 1);
            if (zeroByte == 0 && trackNum > 0) {
                metadata.trackNumber = trackNum;
            }
        }
    }

    // Estimate duration by finding the first valid MP3 frame
    file.seek(0);

    // Skip ID3v2 tag if present
    if (file.read(header, 3) == 3 && strncmp(header, "ID3", 3) == 0) {
        file.seek(6);
        const uint32_t tagSize = readSyncSafeInt(file);
        file.seek(10 + tagSize);
    } else {
        file.seek(0);
    }

    // Search for MP3 frame sync
    uint32_t searchIterations = 0;
    while (file.available() && searchIterations++ < 8192) {
        uint8_t syncByte;
        file.read(&syncByte, 1);
        if (syncByte == 0xFF) {
            uint8_t frameByte;
            file.read(&frameByte, 1);
            if ((frameByte & 0xE0) == 0xE0) {
                // Found frame sync, parse header
                uint8_t headerBytes[2];
                file.read(headerBytes, 2);

                // Extract bitrate index
                const uint8_t bitrateIndex = (headerBytes[0] >> 4) & 0x0F;

                // Bitrate table for MPEG1 Layer 3
                static const uint16_t bitrates[] = {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 0};

                if (bitrateIndex > 0 && bitrateIndex < 15) {
                    const uint32_t bitrate = bitrates[bitrateIndex] * 1000;
                    uint32_t audioSize = file.size();

                    // Subtract ID3v1 tag size if present
                    if (audioSize > 128) {
                        file.seek(file.size() - 128);
                        char tag[4] = {0};
                        file.read(tag, 3);
                        if (strcmp(tag, "TAG") == 0) {
                            audioSize -= 128;
                        }
                    }

                    if (bitrate > 0) {
                        metadata.duration = (audioSize * 8) / bitrate;
                    }
                }
                break;
            }
        }
    }

    // Fall back to filename if no title found
    if (metadata.title.length() == 0) {
        metadata.title = getFilenameWithoutExtension(file.name());
    }

    return true;
}

// Parse Vorbis comment block (OGG)
static void parseVorbisComments(File &file, uint32_t blockLength, SongMetadata &metadata) {
    const uint32_t startPos = file.position();
    const uint32_t endPos = startPos + blockLength;
    
    // Read vendor string length
    const uint32_t vendorLength = readLittleEndian32(file);

    // Bounds check before seeking
    if (vendorLength > blockLength || file.position() + vendorLength > endPos) {
        return;
    }

    // Skip vendor string
    file.seek(file.position() + vendorLength);

    // Read the number of comments
    uint32_t numComments = readLittleEndian32(file);

    // Limit to a reasonable number
    if (numComments > MAX_PARSE_ITERATIONS) {
        numComments = MAX_PARSE_ITERATIONS;
    }

    char buffer[128];

    for (uint32_t i = 0; i < numComments && file.available() && file.position() < endPos; i++) {
        const uint32_t commentLength = readLittleEndian32(file);

        // Bounds check
        if (file.position() + commentLength > endPos) {
            break;
        }

        if (commentLength > 0 && commentLength < sizeof(buffer) - 1) {
            file.read(buffer, commentLength);
            buffer[commentLength] = '\0';

            // Parse key=value format
            char* equals = strchr(buffer, '=');
            if (equals) {
                *equals = '\0';
                char* key = buffer;
                const char* value = equals + 1;

                // Convert key to uppercase for comparison
                for (char* p = key; *p; p++) {
                    if (*p >= 'a' && *p <= 'z') *p -= 32;
                }

                if (strcmp(key, "TITLE") == 0) {
                    metadata.title = value;
                } else if (strcmp(key, "ARTIST") == 0) {
                    metadata.artist = value;
                } else if (strcmp(key, "ALBUM") == 0) {
                    metadata.album = value;
                } else if (strcmp(key, "TRACKNUMBER") == 0) {
                    // May be "N" or "N/M" format
                    parseTrackNumber(value, metadata.trackNumber, metadata.totalTracks);
                } else if (strcmp(key, "TOTALTRACKS") == 0 || strcmp(key, "TRACKTOTAL") == 0) {
                    // Some files use separate field for total
                    metadata.totalTracks = atoi(value);
                }
            }
        } else {
            // Skip large comments
            file.seek(file.position() + commentLength);
        }
    }
}

bool parseOggMetadata(File &file, SongMetadata &metadata) {
    if (!file) return false;

    file.seek(0);

    // Initialize with defaults
    metadata.title = "";
    metadata.artist = "";
    metadata.album = "";
    metadata.duration = 0;
    metadata.trackNumber = 0;
    metadata.totalTracks = 0;

    // Check for "OggS" magic number
    char magic[4];
    if (file.read(magic, 4) != 4 || strncmp(magic, "OggS", 4) != 0) {
        return false;
    }

    uint32_t sampleRate = 0;
    uint64_t lastGranulePos = 0;

    // Reset to beginning
    file.seek(0);

    // Parse OGG pages looking for Vorbis headers
    int pageCount = 0;
    while (file.available() && pageCount < 10) {
        // Read the page header
        char pageSync[4];
        if (file.read(pageSync, 4) != 4 || strncmp(pageSync, "OggS", 4) != 0) {
            break;
        }

        // Skip version
        file.seek(file.position() + 1);

        // Skip header type
        file.seek(file.position() + 1);

        // Granule position (8 bytes, little-endian)
        uint8_t granuleBytes[8];
        file.read(granuleBytes, 8);
        uint64_t granulePos = 0;
        for (int i = 7; i >= 0; i--) {
            granulePos = (granulePos << 8) | granuleBytes[i];
        }
        if (granulePos != 0xFFFFFFFFFFFFFFFFULL) {
            lastGranulePos = granulePos;
        }

        // Skip bitstream serial number (4 bytes) and page sequence number (4 bytes)
        file.seek(file.position() + 8);

        // Skip CRC checksum (4 bytes)
        file.seek(file.position() + 4);

        // Page segments
        uint8_t pageSegments;
        file.read(&pageSegments, 1);

        // Read the segment table
        uint32_t pageDataSize = 0;
        for (uint8_t i = 0; i < pageSegments; i++) {
            uint8_t segSize;
            file.read(&segSize, 1);
            pageDataSize += segSize;
        }

        const uint32_t pageDataStart = file.position();

        // Check for Vorbis identification header
        if (pageCount == 0) {
            uint8_t packetType;
            file.read(&packetType, 1);

            char vorbis[6];
            file.read(vorbis, 6);

            if (packetType == 1 && strncmp(vorbis, "vorbis", 6) == 0) {
                // Vorbis identification header
                file.seek(file.position() + 4); // Skip version

                // Skip channels
                file.seek(file.position() + 1);

                sampleRate = readLittleEndian32(file);
            }
        } else if (pageCount == 1) {
            // Second page usually contains comment header
            uint8_t packetType;
            file.read(&packetType, 1);

            char vorbis[6];
            file.read(vorbis, 6);

            if (packetType == 3 && strncmp(vorbis, "vorbis", 6) == 0) {
                // Vorbis comment header
                parseVorbisComments(file, pageDataSize - 7, metadata);
            }
        }

        // Move to the next page
        file.seek(pageDataStart + pageDataSize);
        pageCount++;
    }

    // To get accurate duration, find the last OGG page
    // Seek near the end of the file and look for the last "OggS"
    uint32_t searchStart = 0;
    if (file.size() > 65536) {
        searchStart = file.size() - 65536;
    }
    file.seek(searchStart);

    // Search for the last OggS page
    uint32_t searchIterations = 0;
    while (file.available() && searchIterations++ < 65536) {
        char c;
        file.read(&c, 1);
        if (c == 'O') {
            char ggs[3];
            if (file.read(ggs, 3) == 3 && strncmp(ggs, "ggS", 3) == 0) {
                // Found a page, read granule position
                file.seek(file.position() + 2); // Skip version and header type

                uint8_t granuleBytes[8];
                file.read(granuleBytes, 8);
                uint64_t granulePos = 0;
                for (int i = 7; i >= 0; i--) {
                    granulePos = (granulePos << 8) | granuleBytes[i];
                }
                if (granulePos != 0xFFFFFFFFFFFFFFFFULL && granulePos > lastGranulePos) {
                    lastGranulePos = granulePos;
                }
            }
        }
    }

    // Calculate duration
    if (sampleRate > 0 && lastGranulePos > 0) {
        metadata.duration = lastGranulePos / sampleRate;
    }

    // Fall back to filename if no title found
    if (metadata.title.length() == 0) {
        metadata.title = getFilenameWithoutExtension(file.name());
    }

    return true;
}

bool parseMetadata(File &file, SongMetadata &metadata) {
    if (!file) return false;

    const String ext = getFileExtension(file.name());

    if (ext == "wav") {
        return parseWavMetadata(file, metadata);
    } else if (ext == "mp3") {
        return parseMp3Metadata(file, metadata);
    } else if (ext == "ogg") {
        return parseOggMetadata(file, metadata);
    }

    // Unknown format - try to at least set a title from filename
    metadata.title = getFilenameWithoutExtension(file.name());
    metadata.artist = "";
    metadata.album = "";
    metadata.duration = 0;
    metadata.trackNumber = 0;
    metadata.totalTracks = 0;

    return false;
}