#include "local_player.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <memory>

#include <esp_log.h>
#include <esp_audio_simple_dec.h>
#include <esp_aac_dec.h>
#include <esp_flac_dec.h>
#include <esp_mp3_dec.h>
#include <esp_pcm_dec.h>
#include <esp_audio_simple_dec_default.h>
#include <esp_ae_rate_cvt.h>

#include "audio_service.h"

#define TAG "LocalPlayer"

namespace {

constexpr size_t kReadChunk = 4096;
constexpr size_t kInitialOutBytes = 8192;
constexpr int kPushMs = 60;  // PCM handed to the playback queue per push

std::string Lowercase(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string Extension(const std::string& path) {
    auto dot = path.find_last_of('.');
    auto slash = path.find_last_of('/');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
        return "";
    }
    return Lowercase(path.substr(dot + 1));
}

esp_audio_simple_dec_type_t DecoderFor(const std::string& path) {
    const std::string ext = Extension(path);
    if (ext == "mp3") return ESP_AUDIO_SIMPLE_DEC_TYPE_MP3;
    if (ext == "aac") return ESP_AUDIO_SIMPLE_DEC_TYPE_AAC;
    if (ext == "m4a") return ESP_AUDIO_SIMPLE_DEC_TYPE_M4A;
    if (ext == "flac") return ESP_AUDIO_SIMPLE_DEC_TYPE_FLAC;
    if (ext == "wav") return ESP_AUDIO_SIMPLE_DEC_TYPE_WAV;
    return ESP_AUDIO_SIMPLE_DEC_TYPE_NONE;
}

// esp_audio_codec keeps global registries; register the built-in codecs once.
// The simple decoder only parses the file (MP3 frames, M4A boxes, WAV header)
// and hands the audio to a codec from the separate audio decoder registry, so
// both must be registered: without the codecs every file fails with "Decoder
// MP3 not registered".
bool EnsureDecodersRegistered() {
    static bool registered = false;
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    if (!registered) {
        // Only the codecs DecoderFor() can pick (WAV is PCM): registering all
        // of them would link every codec and cost about 460 KB of app space.
        // Another module may have registered one already; that is fine.
        for (auto reg : {esp_mp3_dec_register, esp_aac_dec_register, esp_flac_dec_register,
                         esp_pcm_dec_register}) {
            esp_audio_err_t err = reg();
            if (err != ESP_AUDIO_ERR_OK && err != ESP_AUDIO_ERR_ALREADY_EXIST) {
                ESP_LOGE(TAG, "Failed to register an audio codec: %d", err);
                return false;
            }
        }
        esp_audio_err_t err = esp_audio_simple_dec_register_default();
        if (err != ESP_AUDIO_ERR_OK && err != ESP_AUDIO_ERR_ALREADY_EXIST) {
            ESP_LOGE(TAG, "Failed to register decoders: %d", err);
            return false;
        }
        registered = true;
    }
    return true;
}

// Owns a simple decoder handle.
struct Decoder {
    esp_audio_simple_dec_handle_t handle = nullptr;
    ~Decoder() {
        if (handle != nullptr) esp_audio_simple_dec_close(handle);
    }
};

// Owns a rate converter handle.
struct Resampler {
    esp_ae_rate_cvt_handle_t handle = nullptr;
    ~Resampler() {
        if (handle != nullptr) esp_ae_rate_cvt_close(handle);
    }
};

struct FileCloser {
    void operator()(FILE* file) const {
        if (file != nullptr) fclose(file);
    }
};

}  // namespace

LocalMusicPlayer::LocalMusicPlayer(AudioService& audio_service, int output_sample_rate)
    : audio_service_(audio_service), output_sample_rate_(output_sample_rate) {
    xTaskCreatePinnedToCore(
        [](void* arg) { static_cast<LocalMusicPlayer*>(arg)->Run(); }, "music_player", 8192, this,
        3, &task_, 1);
}

LocalMusicPlayer::~LocalMusicPlayer() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        exit_ = true;
        interrupt_ = true;
        cv_.notify_all();
    }
    audio_service_.ResetDecoder();
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return task_ == nullptr; });
}

namespace {

// Adds `directory` and its subfolders that directly hold playable files.
void FindFolders(const std::string& directory, int depth, int max_depth,
                 std::vector<LocalMusicPlayer::Folder>& folders) {
    DIR* dir = opendir(directory.c_str());
    if (dir == nullptr) {
        return;
    }
    size_t tracks = 0;
    std::vector<std::string> subfolders;
    while (struct dirent* entry = readdir(dir)) {
        if (entry->d_name[0] == '.') {
            continue;
        }
        std::string path = directory + "/" + entry->d_name;
        if (entry->d_type == DT_DIR) {
            subfolders.push_back(std::move(path));
        } else if (DecoderFor(path) != ESP_AUDIO_SIMPLE_DEC_TYPE_NONE) {
            ++tracks;
        }
    }
    closedir(dir);
    if (tracks > 0) {
        folders.push_back({directory, tracks});
    }
    if (depth < max_depth) {
        for (const auto& path : subfolders) {
            FindFolders(path, depth + 1, max_depth, folders);
        }
    }
}

}  // namespace

std::vector<LocalMusicPlayer::Folder> LocalMusicPlayer::ListFolders(const std::string& root) const {
    std::vector<Folder> folders;
    FindFolders(root, 0, kMaxScanDepth, folders);
    std::sort(folders.begin(), folders.end(), [](const Folder& a, const Folder& b) {
        return Lowercase(a.path) < Lowercase(b.path);
    });
    return folders;
}

void LocalMusicPlayer::ScanDirectory(const std::string& directory, int depth, bool recursive) {
    DIR* dir = opendir(directory.c_str());
    if (dir == nullptr) {
        return;
    }
    while (struct dirent* entry = readdir(dir)) {
        if (entry->d_name[0] == '.') {
            continue;  // hidden files, "." and ".." (and macOS "._" junk)
        }
        std::string path = directory + "/" + entry->d_name;
        if (entry->d_type == DT_DIR) {
            if (recursive && depth < kMaxScanDepth) {
                ScanDirectory(path, depth + 1, recursive);
            }
        } else if (DecoderFor(path) != ESP_AUDIO_SIMPLE_DEC_TYPE_NONE) {
            paths_.push_back(std::move(path));
        }
        if (paths_.size() >= kMaxTracks) {
            break;
        }
    }
    closedir(dir);
}

size_t LocalMusicPlayer::Scan(const std::string& directory, bool recursive) {
    Stop();
    std::lock_guard<std::mutex> lock(mutex_);
    paths_.clear();
    current_ = -1;
    ScanDirectory(directory, 0, recursive);
    std::sort(paths_.begin(), paths_.end(),
              [](const std::string& a, const std::string& b) { return Lowercase(a) < Lowercase(b); });
    BumpLocked();
    ESP_LOGI(TAG, "Found %u tracks in %s", static_cast<unsigned>(paths_.size()), directory.c_str());
    return paths_.size();
}

size_t LocalMusicPlayer::TrackCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return paths_.size();
}

std::string LocalMusicPlayer::TrackName(size_t index) const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (index >= paths_.size()) {
        return "";
    }
    const std::string& path = paths_[index];
    auto slash = path.find_last_of('/');
    std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
    auto dot = name.find_last_of('.');
    return dot == std::string::npos ? name : name.substr(0, dot);
}

void LocalMusicPlayer::RequestLocked(int index) {
    requested_ = index;
    interrupt_ = true;
    paused_ = false;
    cv_.notify_all();
}

void LocalMusicPlayer::Play(size_t index) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (index >= paths_.size()) {
            return;
        }
        RequestLocked(static_cast<int>(index));
    }
    // Drop audio already queued for the old track and unblock its push.
    audio_service_.ResetDecoder();
}

void LocalMusicPlayer::TogglePause() {
    int start = -1;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (state_ == State::kPlaying) {
            paused_ = true;
            state_ = State::kPaused;
            BumpLocked();
        } else if (state_ == State::kPaused) {
            paused_ = false;
            state_ = State::kPlaying;
            BumpLocked();
            cv_.notify_all();
        } else if (!paths_.empty()) {
            start = current_ >= 0 ? current_ : 0;
        }
    }
    if (start >= 0) {
        Play(start);
    }
}

void LocalMusicPlayer::Next() {
    int index;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (paths_.empty()) return;
        index = (current_ + 1) % static_cast<int>(paths_.size());
    }
    Play(index);
}

void LocalMusicPlayer::Previous() {
    int index;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (paths_.empty()) return;
        index = current_ <= 0 ? static_cast<int>(paths_.size()) - 1 : current_ - 1;
    }
    Play(index);
}

void LocalMusicPlayer::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        requested_ = -1;
        interrupt_ = true;
        paused_ = false;
        if (state_ != State::kStopped) {
            state_ = State::kStopped;
            BumpLocked();
        }
        cv_.notify_all();
    }
    audio_service_.ResetDecoder();
}

LocalMusicPlayer::State LocalMusicPlayer::GetState() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return state_;
}

int LocalMusicPlayer::CurrentIndex() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return current_;
}

uint32_t LocalMusicPlayer::Revision() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return revision_;
}

bool LocalMusicPlayer::ShouldInterrupt() {
    std::lock_guard<std::mutex> lock(mutex_);
    return interrupt_ || exit_;
}

bool LocalMusicPlayer::WaitWhilePaused() {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [this]() { return !paused_ || interrupt_ || exit_; });
    return !interrupt_ && !exit_;
}

void LocalMusicPlayer::Run() {
    while (true) {
        std::string path;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return requested_ >= 0 || exit_; });
            if (exit_) break;
            current_ = requested_;
            requested_ = -1;
            interrupt_ = false;
            paused_ = false;
            state_ = State::kPlaying;
            path = paths_[current_];
            BumpLocked();
        }

        ESP_LOGI(TAG, "Playing %s", path.c_str());
        Result result = PlayFile(path);
        ESP_LOGI(TAG, "%s: %s", path.c_str(),
                 result == Result::kFinished ? "finished"
                 : result == Result::kFailed ? "failed"
                                             : "interrupted");

        std::lock_guard<std::mutex> lock(mutex_);
        if (exit_) break;
        if (requested_ >= 0 || interrupt_) {
            continue;  // Play/Next/Previous picked a new track, or Stop() ran
        }
        // Carry on through the list; a broken file is skipped, not fatal.
        if (current_ + 1 < static_cast<int>(paths_.size())) {
            requested_ = current_ + 1;
        } else {
            state_ = State::kStopped;
            BumpLocked();
        }
    }

    std::lock_guard<std::mutex> lock(mutex_);
    task_ = nullptr;
    cv_.notify_all();
    vTaskDelete(nullptr);
}

LocalMusicPlayer::Result LocalMusicPlayer::PlayFile(const std::string& path) {
    if (!EnsureDecodersRegistered()) {
        return Result::kFailed;
    }
    std::unique_ptr<FILE, FileCloser> file(fopen(path.c_str(), "rb"));
    if (!file) {
        ESP_LOGE(TAG, "Cannot open %s", path.c_str());
        return Result::kFailed;
    }

    esp_audio_simple_dec_cfg_t cfg = {};
    cfg.dec_type = DecoderFor(path);
    Decoder decoder;
    if (esp_audio_simple_dec_open(&cfg, &decoder.handle) != ESP_AUDIO_ERR_OK) {
        ESP_LOGE(TAG, "No decoder for %s", path.c_str());
        return Result::kFailed;
    }

    std::vector<uint8_t> in(kReadChunk);
    std::vector<uint8_t> out(kInitialOutBytes);
    Resampler resampler;
    bool format_ready = false;
    int channels = 1;
    const size_t push_samples = static_cast<size_t>(output_sample_rate_) * kPushMs / 1000;
    std::vector<int16_t> mono;
    std::vector<int16_t> pending;
    pending.reserve(push_samples * 2);

    // Mono at the source rate -> output rate, then into `pending`.
    auto emit = [&](const int16_t* samples, size_t count) -> bool {
        if (resampler.handle == nullptr) {
            pending.insert(pending.end(), samples, samples + count);
        } else {
            uint32_t max_out = 0;
            esp_ae_rate_cvt_get_max_out_sample_num(resampler.handle, count, &max_out);
            std::vector<int16_t> resampled(max_out);
            uint32_t actual = max_out;
            if (esp_ae_rate_cvt_process(resampler.handle, (esp_ae_sample_t)samples, count,
                                        (esp_ae_sample_t)resampled.data(), &actual) != ESP_AE_ERR_OK) {
                return false;
            }
            pending.insert(pending.end(), resampled.begin(), resampled.begin() + actual);
        }
        while (pending.size() >= push_samples) {
            if (!WaitWhilePaused()) return false;
            std::vector<int16_t> chunk(pending.begin(), pending.begin() + push_samples);
            pending.erase(pending.begin(), pending.begin() + push_samples);
            if (!audio_service_.PushPcmToPlaybackQueue(std::move(chunk))) return false;
        }
        return true;
    };

    bool eos = false;
    while (!eos) {
        if (ShouldInterrupt()) return Result::kInterrupted;
        size_t got = fread(in.data(), 1, in.size(), file.get());
        eos = got < in.size();

        esp_audio_simple_dec_raw_t raw = {};
        raw.buffer = in.data();
        raw.len = got;
        raw.eos = eos;
        while (raw.len > 0 || raw.eos) {
            esp_audio_simple_dec_out_t frame = {};
            frame.buffer = out.data();
            frame.len = out.size();
            esp_audio_err_t err = esp_audio_simple_dec_process(decoder.handle, &raw, &frame);
            if (err == ESP_AUDIO_ERR_BUFF_NOT_ENOUGH) {
                out.resize(frame.needed_size);
                continue;
            }
            if (err != ESP_AUDIO_ERR_OK) {
                ESP_LOGE(TAG, "Decode error %d in %s", err, path.c_str());
                return Result::kFailed;
            }
            raw.buffer += raw.consumed;
            raw.len -= raw.consumed;

            if (frame.decoded_size > 0) {
                if (!format_ready) {
                    esp_audio_simple_dec_info_t info = {};
                    esp_audio_simple_dec_get_info(decoder.handle, &info);
                    if (info.bits_per_sample != 16 || info.channel == 0) {
                        ESP_LOGE(TAG, "Unsupported format: %u bit, %u ch", info.bits_per_sample,
                                 info.channel);
                        return Result::kFailed;
                    }
                    channels = info.channel;
                    if (static_cast<int>(info.sample_rate) != output_sample_rate_) {
                        esp_ae_rate_cvt_cfg_t rc = {};
                        rc.src_rate = info.sample_rate;
                        rc.dest_rate = output_sample_rate_;
                        rc.channel = 1;
                        rc.bits_per_sample = 16;
                        rc.complexity = 2;
                        rc.perf_type = ESP_AE_RATE_CVT_PERF_TYPE_SPEED;
                        if (esp_ae_rate_cvt_open(&rc, &resampler.handle) != ESP_AE_ERR_OK) {
                            ESP_LOGE(TAG, "Cannot resample %lu Hz",
                                     static_cast<unsigned long>(info.sample_rate));
                            return Result::kFailed;
                        }
                    }
                    ESP_LOGI(TAG, "%lu Hz, %d ch", static_cast<unsigned long>(info.sample_rate),
                             channels);
                    format_ready = true;
                }

                // Downmix to mono by averaging the channels.
                const int16_t* pcm = reinterpret_cast<const int16_t*>(frame.buffer);
                size_t frames = frame.decoded_size / (sizeof(int16_t) * channels);
                mono.resize(frames);
                for (size_t i = 0; i < frames; ++i) {
                    int32_t sum = 0;
                    for (int c = 0; c < channels; ++c) sum += pcm[i * channels + c];
                    mono[i] = static_cast<int16_t>(sum / channels);
                }
                if (!emit(mono.data(), frames)) {
                    return ShouldInterrupt() ? Result::kInterrupted : Result::kFailed;
                }
            }
            if (frame.decoded_size == 0 && (raw.eos ? raw.len == 0 : raw.consumed == 0)) {
                break;  // flushed at the end, or needs more input
            }
        }
    }

    // Play the tail that did not fill a whole chunk.
    if (!pending.empty() && WaitWhilePaused()) {
        audio_service_.PushPcmToPlaybackQueue(std::move(pending));
    }
    return ShouldInterrupt() ? Result::kInterrupted : Result::kFinished;
}
