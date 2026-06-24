#pragma once
#include <string>
#include <iostream>
#include "miniaudio.h"

class AudioClip {
public:
    AudioClip() {}
    ~AudioClip() { Unload(); }

    bool Load(ma_engine* engine, const std::string& filepath) {
        if (!engine) return false;
        Unload();

        m_Engine = engine;

        ma_result result = ma_sound_init_from_file(m_Engine, filepath.c_str(),
            MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, NULL, &m_Sound);
        if (result != MA_SUCCESS) {
            std::cerr << "AudioClip: failed to load \"" << filepath << "\" (" << result << ")" << std::endl;
            m_Engine = nullptr;
            return false;
        }

        ma_uint64 frames;
        if (ma_sound_get_length_in_pcm_frames(&m_Sound, &frames) == MA_SUCCESS) {
            m_Length = (float)frames / (float)ma_engine_get_sample_rate(m_Engine);
        }

        m_FilePath = filepath;
        m_Loaded = true;
        return true;
    }

    void Unload() {
        if (!m_Loaded) return;
        ma_sound_uninit(&m_Sound);
        m_Engine = nullptr;
        m_Loaded = false;
        m_Length = 0.0f;
        m_FilePath.clear();
    }

    void Play() {
        if (!m_Loaded) return;
        if (ma_sound_at_end(&m_Sound)) {
            ma_sound_seek_to_pcm_frame(&m_Sound, 0);
        }
        ma_sound_start(&m_Sound);
    }

    void Pause() {
        if (!m_Loaded) return;
        ma_sound_stop(&m_Sound);
    }

    void Stop() {
        if (!m_Loaded) return;
        ma_sound_stop(&m_Sound);
        ma_sound_seek_to_pcm_frame(&m_Sound, 0);
    }

    bool IsPlaying() const {
        return m_Loaded && ma_sound_is_playing(&m_Sound);
    }

    bool IsFinished() const {
        return m_Loaded && !ma_sound_is_playing(&m_Sound) && ma_sound_at_end(&m_Sound);
    }

    float GetLength() const {
        return m_Length;
    }

    float GetPosition() const {
        if (!m_Loaded) return 0.0f;
        ma_uint64 frames;
        if (ma_sound_get_cursor_in_pcm_frames(&m_Sound, &frames) == MA_SUCCESS) {
            return (float)frames / (float)ma_engine_get_sample_rate(m_Engine);
        }
        return 0.0f;
    }

    void SetPosition(float seconds) {
        if (!m_Loaded) return;
        ma_uint64 frames = (ma_uint64)(seconds * ma_engine_get_sample_rate(m_Engine));
        ma_sound_seek_to_pcm_frame(&m_Sound, frames);
    }

    void SetVolume(float volume) {
        if (!m_Loaded) return;
        ma_sound_set_volume(&m_Sound, volume);
    }

    float GetVolume() const {
        if (!m_Loaded) return 0.0f;
        return ma_sound_get_volume(&m_Sound);
    }

    void SetPitch(float pitch) {
        if (!m_Loaded) return;
        ma_sound_set_pitch(&m_Sound, pitch);
    }

    float GetPitch() const {
        if (!m_Loaded) return 1.0f;
        return ma_sound_get_pitch(&m_Sound);
    }

    void SetLooping(bool loop) {
        if (!m_Loaded) return;
        ma_sound_set_looping(&m_Sound, loop);
    }

    bool IsLooping() const {
        if (!m_Loaded) return false;
        return ma_sound_is_looping(&m_Sound);
    }

    const std::string& GetFilePath() const { return m_FilePath; }
    bool IsLoaded() const { return m_Loaded; }

private:
    ma_engine* m_Engine = nullptr;
    ma_sound m_Sound;
    bool m_Loaded = false;
    float m_Length = 0.0f;
    std::string m_FilePath;
};
