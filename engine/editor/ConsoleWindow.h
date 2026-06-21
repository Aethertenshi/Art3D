#pragma once
#include <SDL3/SDL.h>
#include "imgui.h"
#include <vector>
#include <string>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <cstdarg>
#include <cstdio>

extern ImFont* g_MonoFont;

class ConsoleWindow
{
public:
    struct LogEntry {
        SDL_LogPriority priority;
        std::string message;
        std::string timestamp;
    };

private:
    static std::vector<LogEntry> s_Logs;
    static std::mutex s_Mutex;
    static bool s_AutoScroll;
    static bool s_ScrollToBottom;
    static char s_FilterBuffer[256];
    static bool s_ShowInfo;
    static bool s_ShowWarn;
    static bool s_ShowError;
    static bool s_ShowDebug;
    static SDL_LogOutputFunction s_OriginalLogCallback;
    static void* s_OriginalLogUserdata;

    static void SDLCALL LogCallback(void* userdata, int category, SDL_LogPriority priority, const char* message) {
        AddLog(priority, "%s", message);
        if (s_OriginalLogCallback) {
            s_OriginalLogCallback(s_OriginalLogUserdata, category, priority, message);
        }
    }

public:
    static void AddLog(SDL_LogPriority priority, const char* fmt, ...) {
        char buf[2048];
        va_list args;
        va_start(args, fmt);
        vsnprintf(buf, sizeof(buf), fmt, args);
        va_end(args);

        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);
        std::tm tm_now;
#if defined(_WIN32)
        localtime_s(&tm_now, &time_t_now);
#else
        localtime_r(&time_t_now, &tm_now);
#endif

        std::ostringstream oss;
        oss << std::put_time(&tm_now, "%H:%M:%S");

        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Logs.push_back({ priority, std::string(buf), oss.str() });
        if (s_Logs.size() > 2000) {
            s_Logs.erase(s_Logs.begin());
        }
        if (s_AutoScroll) {
            s_ScrollToBottom = true;
        }
    }

    static void Clear() {
        std::lock_guard<std::mutex> lock(s_Mutex);
        s_Logs.clear();
    }

    void Initialize() {
        SDL_GetLogOutputFunction(&s_OriginalLogCallback, &s_OriginalLogUserdata);
        SDL_SetLogOutputFunction(LogCallback, nullptr);
        AddLog(SDL_LOG_PRIORITY_INFO, "Console system initialized. Intercepting SDL output.");
    }

    void Draw() {
        if (ImGui::Begin("Console")) {
            if (ImGui::Button("Clear")) { Clear(); }
            ImGui::SameLine();
            if (ImGui::Button("Copy Logs")) {
                std::lock_guard<std::mutex> lock(s_Mutex);
                std::string all_logs;
                for (const auto& log : s_Logs) {
                    all_logs += "[" + log.timestamp + "] " + log.message + "\n";
                }
                ImGui::SetClipboardText(all_logs.c_str());
            }
            ImGui::SameLine();
            ImGui::Checkbox("Auto-Scroll", &s_AutoScroll);
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::Checkbox("Info", &s_ShowInfo);
            ImGui::SameLine();
            ImGui::Checkbox("Warn", &s_ShowWarn);
            ImGui::SameLine();
            ImGui::Checkbox("Error", &s_ShowError);
            ImGui::SameLine();
            ImGui::Checkbox("Debug/Trace", &s_ShowDebug);
            ImGui::SameLine();
            ImGui::TextDisabled("|");
            ImGui::SameLine();
            ImGui::Text("Filter:");
            ImGui::SameLine();
            ImGui::InputText("##FilterInput", s_FilterBuffer, sizeof(s_FilterBuffer));

            ImGui::Separator();

            const float footer_height_to_reserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
            if (ImGui::BeginChild("ConsoleScrollingRegion", ImVec2(0, -footer_height_to_reserve), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
                std::lock_guard<std::mutex> lock(s_Mutex);
                if (g_MonoFont) ImGui::PushFont(g_MonoFont, ImGui::GetFontSize());

                const ImVec4 timeColor  = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                const ImVec4 infoColor  = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
                const ImVec4 warnColor  = ImVec4(1.0f, 0.75f, 0.3f, 1.0f);
                const ImVec4 errorColor = ImVec4(1.0f, 0.4f, 0.4f, 1.0f);
                const ImVec4 debugColor = ImVec4(0.45f, 0.7f, 1.0f, 1.0f);

                for (const auto& log : s_Logs) {
                    if (log.priority == SDL_LOG_PRIORITY_INFO && !s_ShowInfo) continue;
                    if (log.priority == SDL_LOG_PRIORITY_WARN && !s_ShowWarn) continue;
                    if (log.priority == SDL_LOG_PRIORITY_ERROR && !s_ShowError) continue;
                    if (log.priority == SDL_LOG_PRIORITY_CRITICAL && !s_ShowError) continue;
                    if ((log.priority == SDL_LOG_PRIORITY_DEBUG || log.priority == SDL_LOG_PRIORITY_TRACE || log.priority == SDL_LOG_PRIORITY_VERBOSE) && !s_ShowDebug) continue;
                    if (s_FilterBuffer[0] != '\0' && log.message.find(s_FilterBuffer) == std::string::npos) continue;

                    ImVec4 color = infoColor;
                    const char* priorityLabel = "[INFO]";
                    if (log.priority == SDL_LOG_PRIORITY_WARN) { color = warnColor; priorityLabel = "[WARN]"; }
                    else if (log.priority == SDL_LOG_PRIORITY_ERROR || log.priority == SDL_LOG_PRIORITY_CRITICAL) { color = errorColor; priorityLabel = "[ERROR]"; }
                    else if (log.priority == SDL_LOG_PRIORITY_DEBUG || log.priority == SDL_LOG_PRIORITY_TRACE || log.priority == SDL_LOG_PRIORITY_VERBOSE) { color = debugColor; priorityLabel = "[DEBUG]"; }

                    ImGui::TextColored(timeColor, "[%s]", log.timestamp.c_str());
                    ImGui::SameLine();
                    ImGui::TextColored(color, "%s %s", priorityLabel, log.message.c_str());
                }

                if (g_MonoFont) ImGui::PopFont();

                if (s_ScrollToBottom || (s_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())) {
                    ImGui::SetScrollHereY(1.0f);
                    s_ScrollToBottom = false;
                }
            }
            ImGui::EndChild();

            ImGui::Separator();
            static char s_InputBuf[256] = "";
            bool reclaim_focus = false;
            ImGui::Text("cmd>");
            ImGui::SameLine();
            ImGuiInputTextFlags input_flags = ImGuiInputTextFlags_EnterReturnsTrue;
            if (ImGui::InputText("##ConsoleInputInput", s_InputBuf, IM_COUNTOF(s_InputBuf), input_flags)) {
                char* s = s_InputBuf;
                while (*s == ' ') s++;
                if (*s) {
                    AddLog(SDL_LOG_PRIORITY_INFO, "Executing command: %s", s);
                    strcpy_s(s_InputBuf, "");
                    reclaim_focus = true;
                }
            }
            ImGui::SetItemDefaultFocus();
            if (reclaim_focus) {
                ImGui::SetKeyboardFocusHere(-1);
            }
        }
        ImGui::End();
    }
};

inline std::vector<ConsoleWindow::LogEntry> ConsoleWindow::s_Logs;
inline std::mutex ConsoleWindow::s_Mutex;
inline bool ConsoleWindow::s_AutoScroll = true;
inline bool ConsoleWindow::s_ScrollToBottom = false;
inline char ConsoleWindow::s_FilterBuffer[256] = "";
inline bool ConsoleWindow::s_ShowInfo = true;
inline bool ConsoleWindow::s_ShowWarn = true;
inline bool ConsoleWindow::s_ShowError = true;
inline bool ConsoleWindow::s_ShowDebug = true;
inline SDL_LogOutputFunction ConsoleWindow::s_OriginalLogCallback = nullptr;
inline void* ConsoleWindow::s_OriginalLogUserdata = nullptr;
