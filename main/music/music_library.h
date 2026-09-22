#ifndef MUSIC_LIBRARY_H_
#define MUSIC_LIBRARY_H_

#include <cstddef>
#include <string>

/**
 * A small catalogue of Vietnamese children's songs the assistant can play.
 *
 * The device streams audio with the same pipeline the server uses for notify
 * messages, which is Opus inside an Ogg container. MP3 will NOT play.
 *
 * Where the audio comes from
 * --------------------------
 * Each track carries a file name rather than a full URL. The playable URL is
 * <base_url>/<file>, where base_url is stored in NVS (namespace "music", key
 * "base_url") and points at a folder of .opus files you host yourself. Set it
 * once with the self.music.set_library tool, e.g.
 *     https://example.com/nhac-thieu-nhi
 * A track whose file field is already an absolute http(s) URL is used as-is.
 *
 * No audio ships with the firmware, so supply recordings you have the right to
 * use. Most of these songs are traditional, but individual recordings are not.
 */
struct MusicTrack {
    const char* title;    // Display title, with Vietnamese diacritics.
    const char* aliases;  // Extra search terms, ';' separated. May be empty.
    const char* file;     // File name under base_url, or an absolute URL.
};

namespace music {

// Lowercases, strips Vietnamese diacritics and collapses punctuation so that
// "Chú Ếch Con", "chu ech con" and "CHU-ECH-CON" all compare equal.
std::string Normalize(const std::string& text);

// Number of tracks in the built-in catalogue.
size_t Count();

// Track by index; index must be < Count().
const MusicTrack& At(size_t index);

// Best match for a free-text query, or nullptr when nothing matches.
// Matching is accent-insensitive and accepts partial titles and aliases.
const MusicTrack* Find(const std::string& query);

// Playable URL for a track, or an empty string when no library base URL is set.
std::string ResolveUrl(const MusicTrack& track);

// Library base URL stored in NVS ("" when unset).
std::string GetBaseUrl();
void SetBaseUrl(const std::string& url);

// JSON array of the catalogue, for the self.music.list tool.
std::string ListJson();

}  // namespace music

#endif  // MUSIC_LIBRARY_H_
