// minimal music visualizer: SDL2 audio -> Hann-windowed FFT -> log-spaced bars, waveform, radial ring.
//   visualizer                 default capture device (mic / line in)
//   visualizer --wav song.wav  plays the file and shows it
//   visualizer --tone          silent synthetic sweep, no audio device needed

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <SDL.h>
#undef main // smg examples link no SDL2main; SDL.h would rename ours

#include "GuiBase.hh"
#include "imgui.h"

namespace {

constexpr int FftSize = 2048;
constexpr int BarCount = 64;
constexpr int SampleRate = 48000;
constexpr float Pi = 3.14159265358979f;

// fixed-size ring the audio thread writes and the UI reads (under SDL_LockAudioDevice)
struct Ring {
    std::vector<float> data = std::vector<float>(FftSize * 2, 0.0f);
    std::size_t head = 0;
    void push(const float* s, int n) {
        for(int i = 0; i < n; ++i) {
            data[head] = s[i];
            head = (head + 1) % data.size();
        }
    }
    // newest `n` samples, oldest first
    void latest(std::vector<float>& out, int n) const {
        out.resize(static_cast<std::size_t>(n));
        for(int i = 0; i < n; ++i) out[i] = data[(head + data.size() - n + i) % data.size()];
    }
};

struct Playback {
    std::vector<float> samples; // mono, SampleRate
    std::size_t pos = 0;
};

struct AudioState {
    Ring ring;
    Playback playback;
};

void SDLCALL captureCallback(void* user, Uint8* stream, int len) {
    static_cast<AudioState*>(user)->ring.push(reinterpret_cast<const float*>(stream), len / static_cast<int>(sizeof(float)));
}

void SDLCALL playbackCallback(void* user, Uint8* stream, int len) {
    AudioState* a = static_cast<AudioState*>(user);
    float* out = reinterpret_cast<float*>(stream);
    const int n = len / static_cast<int>(sizeof(float));
    for(int i = 0; i < n; ++i) {
        out[i] = a->playback.pos < a->playback.samples.size() ? a->playback.samples[a->playback.pos++] : 0.0f;
        if(a->playback.pos >= a->playback.samples.size()) a->playback.pos = 0; // loop
    }
    a->ring.push(out, n);
}

// any WAV SDL reads, converted to mono float at SampleRate
bool loadWav(const char* path, std::vector<float>& out) {
    SDL_AudioSpec spec;
    Uint8* buf = nullptr;
    Uint32 len = 0;
    if(!SDL_LoadWAV(path, &spec, &buf, &len)) return false;
    SDL_AudioStream* stream = SDL_NewAudioStream(spec.format, spec.channels, spec.freq, AUDIO_F32, 1, SampleRate);
    SDL_AudioStreamPut(stream, buf, static_cast<int>(len));
    SDL_AudioStreamFlush(stream);
    out.resize(static_cast<std::size_t>(SDL_AudioStreamAvailable(stream)) / sizeof(float));
    SDL_AudioStreamGet(stream, out.data(), static_cast<int>(out.size() * sizeof(float)));
    SDL_FreeAudioStream(stream);
    SDL_FreeWAV(buf);
    return !out.empty();
}

// in-place iterative radix-2 FFT
void fft(std::vector<std::complex<float>>& a) {
    const std::size_t n = a.size();
    for(std::size_t i = 1, j = 0; i < n; ++i) {
        std::size_t bit = n >> 1;
        for(; j & bit; bit >>= 1) j ^= bit;
        j ^= bit;
        if(i < j) std::swap(a[i], a[j]);
    }
    for(std::size_t len = 2; len <= n; len <<= 1) {
        const std::complex<float> w = std::polar(1.0f, -2.0f * Pi / static_cast<float>(len));
        for(std::size_t i = 0; i < n; i += len) {
            std::complex<float> wk = 1.0f;
            for(std::size_t k = 0; k < len / 2; ++k) {
                const std::complex<float> u = a[i + k];
                const std::complex<float> v = a[i + k + len / 2] * wk;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                wk *= w;
            }
        }
    }
}

ImU32 barColor(float t, float level) {
    // hue sweeps across the spectrum, brightness follows the level
    float r, g, b;
    ImGui::ColorConvertHSVtoRGB(0.62f - 0.62f * t, 0.75f, 0.45f + 0.55f * level, r, g, b);
    return ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));
}

} // namespace

int main(int argc, char** argv) {
    std::string wav;
    bool tone = false;
    for(int i = 1; i < argc; ++i) {
        if(std::strcmp(argv[i], "--wav") == 0 && i + 1 < argc) wav = argv[++i];
        else if(std::strcmp(argv[i], "--tone") == 0) tone = true;
    }

    smg::GuiConfig config;
    config.title = "smg visualizer";
    config.size = { 1100, 640 };
    smg::GuiBase gui({ argc, argv }, config);

    AudioState audio;
    SDL_AudioDeviceID device = 0;
    std::string source = "synthetic sweep";
    if(!tone) {
        SDL_InitSubSystem(SDL_INIT_AUDIO);
        SDL_AudioSpec want = {};
        want.freq = SampleRate;
        want.format = AUDIO_F32;
        want.channels = 1;
        want.samples = 512;
        want.userdata = &audio;
        if(!wav.empty()) {
            if(loadWav(wav.c_str(), audio.playback.samples)) {
                want.callback = playbackCallback;
                device = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
                source = wav;
            } else {
                std::fprintf(stderr, "cannot read %s: %s\n", wav.c_str(), SDL_GetError());
            }
        } else {
            want.callback = captureCallback;
            device = SDL_OpenAudioDevice(nullptr, 1, &want, nullptr, 0);
            source = "default input device";
        }
        if(device) SDL_PauseAudioDevice(device, 0);
        else {
            std::fprintf(stderr, "no audio device (%s); using the synthetic sweep\n", SDL_GetError());
            tone = true;
            source = "synthetic sweep";
        }
    }

    std::vector<float> window(FftSize);
    for(int i = 0; i < FftSize; ++i) window[i] = 0.5f - 0.5f * std::cos(2.0f * Pi * static_cast<float>(i) / (FftSize - 1));
    std::vector<float> samples;
    std::vector<std::complex<float>> spectrum(FftSize);
    std::vector<float> bars(BarCount, 0.0f), peaks(BarCount, 0.0f);
    float gain = 1.0f;
    float bass = 0.0f;
    float peakHz = 0.0f;
    double tonePhase = 0.0;
    double toneTime = 0.0;
    int mode = 0; // 0 bars, 1 radial

    gui.add_callback([&]() {
        const float dt = std::max(gui.dt(), 1.0f / 1000.0f);

        if(tone) {
            // a sweep 60 Hz -> 8 kHz every 6 s plus a pulsing kick, generated at the frame rate
            const int n = static_cast<int>(dt * SampleRate);
            std::vector<float> chunk(static_cast<std::size_t>(n));
            for(int i = 0; i < n; ++i) {
                toneTime += 1.0 / SampleRate;
                const double f = 60.0 * std::pow(8000.0 / 60.0, std::fmod(toneTime, 6.0) / 6.0);
                tonePhase += 2.0 * 3.14159265358979 * f / SampleRate;
                const double kick = std::exp(-std::fmod(toneTime, 0.5) * 12.0) * std::sin(2.0 * 3.14159265358979 * 55.0 * toneTime);
                chunk[i] = static_cast<float>(0.4 * std::sin(tonePhase) + 0.5 * kick);
            }
            audio.ring.push(chunk.data(), n);
            audio.ring.latest(samples, FftSize);
        } else {
            SDL_LockAudioDevice(device);
            audio.ring.latest(samples, FftSize);
            SDL_UnlockAudioDevice(device);
        }

        for(int i = 0; i < FftSize; ++i) spectrum[i] = samples[i] * window[i];
        fft(spectrum);

        // log-spaced bands 40 Hz .. 16 kHz, in dB, mapped to 0..1
        const float binHz = static_cast<float>(SampleRate) / FftSize;
        float best = 0.0f;
        for(int k = 1; k < FftSize / 2; ++k)
            if(std::abs(spectrum[k]) > best) {
                best = std::abs(spectrum[k]);
                peakHz = k * binHz;
            }
        for(int b = 0; b < BarCount; ++b) {
            const float lo = 40.0f * std::pow(400.0f, static_cast<float>(b) / BarCount);
            const float hi = 40.0f * std::pow(400.0f, static_cast<float>(b + 1) / BarCount);
            const int k0 = std::max(1, static_cast<int>(lo / binHz));
            const int k1 = std::max(k0 + 1, static_cast<int>(hi / binHz));
            float m = 0.0f;
            for(int k = k0; k < k1 && k < FftSize / 2; ++k) m = std::max(m, std::abs(spectrum[k]));
            const float db = 20.0f * std::log10(m * gain / (FftSize * 0.25f) + 1e-6f);
            const float v = std::clamp((db + 70.0f) / 70.0f, 0.0f, 1.0f);
            // fast attack, slow release, falling peak caps
            bars[b] = v > bars[b] ? v : bars[b] - (bars[b] - v) * std::min(1.0f, dt * 8.0f);
            peaks[b] = std::max(peaks[b] - dt * 0.4f, bars[b]);
        }
        const float lowNow = (bars[0] + bars[1] + bars[2] + bars[3]) * 0.25f;
        bass += (lowNow - bass) * std::min(1.0f, dt * 10.0f);

        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::Begin("##viz", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings
                | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoScrollbar);
        ImGui::Text("%s   peak %5.0f Hz", source.c_str(), peakHz);
        ImGui::SameLine();
        ImGui::RadioButton("bars", &mode, 0);
        ImGui::SameLine();
        ImGui::RadioButton("radial", &mode, 1);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        ImGui::SliderFloat("gain", &gain, 0.1f, 20.0f, "%.1f", ImGuiSliderFlags_Logarithmic);

        ImDrawList* draw = ImGui::GetWindowDrawList();
        const ImVec2 origin = ImGui::GetCursorScreenPos();
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float waveH = avail.y * 0.22f;
        const ImVec2 area(avail.x, avail.y - waveH - 8.0f);
        draw->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(8, 8, 14, 255), 8.0f);

        if(mode == 0) {
            const float slot = area.x / BarCount;
            for(int b = 0; b < BarCount; ++b) {
                const float x0 = origin.x + b * slot + slot * 0.15f;
                const float x1 = origin.x + (b + 1) * slot - slot * 0.15f;
                const float base = origin.y + area.y;
                const float t = static_cast<float>(b) / (BarCount - 1);
                draw->AddRectFilled(ImVec2(x0, base - bars[b] * area.y), ImVec2(x1, base), barColor(t, bars[b]), 3.0f);
                const float py = base - peaks[b] * area.y;
                draw->AddRectFilled(ImVec2(x0, py - 3.0f), ImVec2(x1, py), IM_COL32(255, 255, 255, 200), 1.0f);
            }
        } else {
            const ImVec2 c(origin.x + area.x * 0.5f, origin.y + area.y * 0.5f);
            const float r0 = std::min(area.x, area.y) * (0.18f + 0.08f * bass);
            const float len = std::min(area.x, area.y) * 0.3f;
            draw->AddCircleFilled(c, r0 * 0.9f, barColor(0.0f, bass), 64);
            for(int b = 0; b < BarCount * 2; ++b) {
                // mirrored so the ring is symmetric
                const int i = b < BarCount ? b : 2 * BarCount - 1 - b;
                const float a = 2.0f * Pi * b / (BarCount * 2) - Pi * 0.5f;
                const ImVec2 d(std::cos(a), std::sin(a));
                const float l = r0 + bars[i] * len;
                draw->AddLine(ImVec2(c.x + d.x * r0, c.y + d.y * r0), ImVec2(c.x + d.x * l, c.y + d.y * l),
                    barColor(static_cast<float>(i) / (BarCount - 1), bars[i]), 4.0f);
            }
        }

        // waveform strip
        const float wy = origin.y + avail.y - waveH * 0.5f;
        const int points = 512;
        std::vector<ImVec2> line(points);
        for(int i = 0; i < points; ++i) {
            const float s = samples[FftSize - points + i] * gain;
            line[i] = ImVec2(origin.x + avail.x * i / (points - 1), wy - std::clamp(s, -1.0f, 1.0f) * waveH * 0.45f);
        }
        draw->AddPolyline(line.data(), points, IM_COL32(120, 220, 255, 220), ImDrawFlags_None, 1.5f);

        ImGui::End();
        return 0;
    });

    while(gui.mainLoopIteration()) {}
    if(device) SDL_CloseAudioDevice(device);
    std::printf("visualizer: last peak %.0f Hz\n", peakHz);
    return 0;
}
