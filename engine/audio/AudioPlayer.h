#pragma once
#include "AudioClip.h"
#include "imgui.h"
#include <string>
#include <cmath>
#include <cstdio>

class AudioPlayer {
public:
    AudioPlayer() : m_Open(false) {}

    void SetEngine(ma_engine* engine) {
        m_Engine = engine;
    }

    bool LoadFile(const std::string& path) {
        if (!m_Engine) return false;
        m_Open = true;
        return m_Clip.Load(m_Engine, path);
    }

    bool IsLoaded() const {
        return m_Clip.IsLoaded();
    }

    void Draw() {
        ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_FirstUseEver);

        if (!ImGui::Begin("Audio Player")) {
            ImGui::End();
            return;
        }

        if (!m_Clip.IsLoaded()) {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "No audio file loaded.");
            ImGui::Text("Double-click an audio file in the Asset Browser to play.");
            ImGui::End();
            return;
        }

        // File info
        std::string filename = m_Clip.GetFilePath();
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) filename = filename.substr(pos + 1);

        ImGui::Text("%s", filename.c_str());
        ImGui::Separator();

        // Playback controls
        DrawPlaybackControls();

        ImGui::Separator();

        // Progress slider
        DrawProgressSlider();

        ImGui::Separator();

        // Volume
        DrawVolume();

        ImGui::Separator();

        // Effects
        DrawEffects();

        if (ImGui::Button("Close", ImVec2(ImGui::GetContentRegionAvail().x, 0))) {
            m_Open = false;
        }

        ImGui::End();
    }

    AudioClip& GetClip() { return m_Clip; }

    bool IsOpen() const { return m_Open; }
    void ToggleOpen() { m_Open = !m_Open; }

private:
    void DrawPlaybackControls() {
        // Play / Pause button
        if (m_Clip.IsPlaying()) {
            if (ImGui::Button("Pause", ImVec2(80, 30))) {
                m_Clip.Pause();
            }
        } else {
            if (ImGui::Button("Play", ImVec2(80, 30))) {
                m_Clip.Play();
            }
        }

        ImGui::SameLine();

        // Stop button
        if (ImGui::Button("Stop", ImVec2(80, 30))) {
            m_Clip.Stop();
        }

        ImGui::SameLine();

        // Loop toggle
        bool looping = m_Clip.IsLooping();
        if (ImGui::Checkbox("Loop", &looping)) {
            m_Clip.SetLooping(looping);
        }

        ImGui::SameLine();

        // Restart button
        if (ImGui::Button("Restart", ImVec2(80, 30))) {
            m_Clip.SetPosition(0.0f);
            m_Clip.Play();
        }
    }

void DrawProgressSlider() {
    float pos = m_Clip.GetPosition();
    float len = m_Clip.GetLength();
    if (len <= 0.0f) len = 1.0f;

    char currentStr[16], totalStr[16];
    FormatTime(currentStr, sizeof(currentStr), pos);
    FormatTime(totalStr, sizeof(totalStr), len);

    // 1. Capture layout metrics before drawing anything on this line
    float totalWidth = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    
    // A safe max-width boundary for "99:99.99" so spacing never moves
    static const float maxLabelWidth = ImGui::CalcTextSize("99:99.99").x;
    
    // Calculate precise slider width to fill the exact remaining space
    float sliderWidth = totalWidth - (maxLabelWidth * 2.0f) - (spacing * 2.0f);
    if (sliderWidth < 40.0f) sliderWidth = 40.0f; // Safety fallback

    float startX = ImGui::GetCursorPosX();

    // 2. Right-align current time within its fixed slot to look incredibly clean
    float currentTextWidth = ImGui::CalcTextSize(currentStr).x;
    ImGui::SetCursorPosX(startX + (maxLabelWidth - currentTextWidth));
    ImGui::TextUnformatted(currentStr);

    // 3. Anchor the slider to an explicit, unchanging starting point
    ImGui::SameLine();
    ImGui::SetCursorPosX(startX + maxLabelWidth + spacing);
    ImGui::PushItemWidth(sliderWidth);
    
    float progress = pos / len;
    if (ImGui::SliderFloat("##progress", &progress, 0.0f, 1.0f, "")) {
        m_Clip.SetPosition(progress * len);
    }
    
    ImGui::PopItemWidth();

    // 4. Anchor the total duration text to its explicit endpoint
    ImGui::SameLine();
    ImGui::SetCursorPosX(startX + maxLabelWidth + spacing + sliderWidth + spacing);
    ImGui::TextUnformatted(totalStr);
}

    void DrawVolume() {
        float vol = m_Clip.GetVolume();
        if (ImGui::SliderFloat("Volume", &vol, 0.0f, 1.0f, "%.2f")) {
            m_Clip.SetVolume(vol);
        }
    }

    void DrawEffects() {
        if (ImGui::CollapsingHeader("Effects", ImGuiTreeNodeFlags_DefaultOpen)) {
            // Speed control (pitch)
            float speed = m_Clip.GetPitch();
            if (ImGui::SliderFloat("Speed", &speed, 0.25f, 4.0f, "%.2fx")) {
                m_Clip.SetPitch(speed);
            }

            ImGui::TextDisabled("Lowpass Filter  (coming soon)");
            ImGui::TextDisabled("Reverb  (coming soon)");
            ImGui::TextDisabled("Echo / Delay  (coming soon)");
        }
    }

    static void FormatTime(char* buf, size_t size, float seconds) {
        if (seconds < 0) seconds = 0;
        int mins = (int)(seconds / 60);
        int secs = (int)seconds % 60;
        int ms = (int)((seconds - (int)seconds) * 100);
        snprintf(buf, size, "%d:%02d.%02d", mins, secs, ms);
    }

    ma_engine* m_Engine = nullptr;
    AudioClip m_Clip;
    bool m_Open = true;
};
