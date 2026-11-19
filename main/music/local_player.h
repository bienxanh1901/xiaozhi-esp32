#pragma once

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class AudioService;

/**
 * Plays audio files from local storage (an SD card) through AudioService.
 *
 * Files are decoded with esp_audio_codec's simple decoder (MP3, AAC/M4A, FLAC,
 * WAV), downmixed to mono, resampled to the codec output rate and pushed into
 * the playback queue. Decoding runs in the player's own task; the control
 * methods are safe to call from any task and return immediately.
 *
 * The chatbot must not use the speaker at the same time: callers suspend chat
 * (Application::SetChatSuspended) while the player is in use.
 */
class LocalMusicPlayer {
public:
    enum class State { kStopped, kPlaying, kPaused };

    LocalMusicPlayer(AudioService& audio_service, int output_sample_rate);
    ~LocalMusicPlayer();
    LocalMusicPlayer(const LocalMusicPlayer&) = delete;
    LocalMusicPlayer& operator=(const LocalMusicPlayer&) = delete;

    // Finds playable files under `directory` (a few levels deep), sorted by
    // path. Stops playback. Returns the number of tracks found.
    size_t Scan(const std::string& directory);

    size_t TrackCount() const;
    // Display name of a track: its file name without the extension.
    std::string TrackName(size_t index) const;

    void Play(size_t index);
    void TogglePause();
    void Next();
    void Previous();
    void Stop();

    State GetState() const;
    // Index of the current track, or -1 when nothing is selected.
    int CurrentIndex() const;
    // Bumped on every state or track change, so a UI can poll cheaply.
    uint32_t Revision() const;

private:
    enum class Result { kFinished, kInterrupted, kFailed };

    static constexpr size_t kMaxTracks = 500;
    static constexpr int kMaxScanDepth = 3;

    AudioService& audio_service_;
    const int output_sample_rate_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::string> paths_;
    State state_ = State::kStopped;
    int current_ = -1;
    int requested_ = -1;        // Track to start next, set by the control methods
    bool interrupt_ = false;    // Abandon the track being decoded
    bool paused_ = false;
    bool exit_ = false;
    uint32_t revision_ = 0;
    TaskHandle_t task_ = nullptr;

    void ScanDirectory(const std::string& directory, int depth);
    void RequestLocked(int index);
    void BumpLocked() { ++revision_; }
    void Run();
    Result PlayFile(const std::string& path);
    bool WaitWhilePaused();
    bool ShouldInterrupt();
};
