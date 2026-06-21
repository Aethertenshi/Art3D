#include "Autocomplete.h"
#include "TextEditor.h"
#include <cctype>

namespace Autocomplete {

bool g_autocompleteOpen = false;
bool g_autocompleteDismissed = false;
int g_autocompleteActiveIndex = 0;
bool g_autocompleteSelectTriggered = false;
bool g_autocompleteCloseTriggered = false;
std::vector<std::string> g_autocompleteMatches;

const std::vector<std::string> combinedKeywordsList = {
    "arguments", "as", "async", "await", "break", "case", "catch",
    "class", "const", "constructor", "continue", "debugger", "default",
    "delete", "do", "else", "enum", "error", "export", "extends",
    "false", "finally", "for", "foreign", "from", "function", "get",
    "getters", "if", "import", "in", "instanceof", "is", "let",
    "new", "null", "of", "print", "return", "set", "setters",
    "static", "super", "switch", "target", "this", "throw", "true",
    "try", "typeof", "undefined", "var", "void", "warn", "while",
    "with", "yield"
};

std::string GetCurrentWordPrefix(const TextEditor& editor) {
    TextEditor::Coordinates cursor = editor.GetCursorPosition();
    TextEditor::Coordinates lineStart(cursor.mLine, 0);
    std::string lineToCursor = editor.GetText(lineStart, cursor);

    int len = (int)lineToCursor.length();
    int i = len - 1;
    while (i >= 0) {
        char c = lineToCursor[i];
        if (std::isalnum((unsigned char)c) || c == '_') i--;
        else break;
    }
    return lineToCursor.substr(i + 1);
}

void InsertSuggestion(TextEditor& editor, const std::string& suggestion) {
    TextEditor::Coordinates cursor = editor.GetCursorPosition();
    std::string prefix = GetCurrentWordPrefix(editor);

    int cursorCharIndex = editor.GetCharacterIndex(cursor);
    int startCharIndex = cursorCharIndex - (int)prefix.length();
    int startColumn = editor.GetCharacterColumn(cursor.mLine, startCharIndex);

    TextEditor::Coordinates start(cursor.mLine, startColumn);
    editor.DeleteRange(start, cursor);
    TextEditor::Coordinates newCursor = start;
    editor.InsertTextAt(newCursor, suggestion.c_str());
    editor.SetCursorPosition(newCursor);
}

void ProcessAutocomplete(TextEditor& editor) {
    std::string currentWord = GetCurrentWordPrefix(editor);
    bool showAutocomplete = false;

    if (editor.IsTextChanged()) {
        g_autocompleteDismissed = false;
    }

    if (!currentWord.empty() && std::isalpha((unsigned char)currentWord[0]) && !g_autocompleteDismissed) {
        g_autocompleteMatches.clear();
        for (const auto& keyword : combinedKeywordsList) {
            if (keyword.size() >= currentWord.size() && keyword.compare(0, currentWord.size(), currentWord) == 0) {
                g_autocompleteMatches.push_back(keyword);
            }
        }
        if (!g_autocompleteMatches.empty()) {
            showAutocomplete = true;
        }
    }

    if (showAutocomplete) {
        g_autocompleteOpen = true;

        if (g_autocompleteActiveIndex < 0) {
            g_autocompleteActiveIndex = (int)g_autocompleteMatches.size() - 1;
        }
        if (g_autocompleteActiveIndex >= (int)g_autocompleteMatches.size()) {
            g_autocompleteActiveIndex = 0;
        }

        if (g_autocompleteSelectTriggered) {
            InsertSuggestion(editor, g_autocompleteMatches[g_autocompleteActiveIndex]);
            g_autocompleteSelectTriggered = false;
            g_autocompleteOpen = false;
            showAutocomplete = false;
        } else if (g_autocompleteCloseTriggered) {
            g_autocompleteCloseTriggered = false;
            g_autocompleteOpen = false;
            showAutocomplete = false;
            g_autocompleteDismissed = true;
        }

        if (showAutocomplete) {
            ImVec2 cursorScreenPos = editor.GetCursorScreenPos();
            float lineHeight = ImGui::GetTextLineHeightWithSpacing();
            ImGui::SetNextWindowPos(ImVec2(cursorScreenPos.x, cursorScreenPos.y + lineHeight));
            ImGui::SetNextWindowSizeConstraints(ImVec2(150, 50), ImVec2(300, 200));

            ImGuiWindowFlags popupFlags = ImGuiWindowFlags_NoTitleBar |
                                         ImGuiWindowFlags_NoResize |
                                         ImGuiWindowFlags_NoMove |
                                         ImGuiWindowFlags_HorizontalScrollbar |
                                         ImGuiWindowFlags_AlwaysAutoResize |
                                         ImGuiWindowFlags_NoSavedSettings |
                                         ImGuiWindowFlags_Tooltip;

            if (ImGui::Begin("##autocompletePopup", nullptr, popupFlags)) {
                for (int j = 0; j < (int)g_autocompleteMatches.size(); j++) {
                    bool isSelected = (j == g_autocompleteActiveIndex);
                    if (isSelected) {
                        ImGui::PushStyleColor(ImGuiCol_Header, ImGui::GetStyle().Colors[ImGuiCol_HeaderActive]);
                    }
                    if (ImGui::Selectable(g_autocompleteMatches[j].c_str(), isSelected)) {
                        InsertSuggestion(editor, g_autocompleteMatches[j]);
                        g_autocompleteOpen = false;
                    }
                    if (isSelected) {
                        ImGui::PopStyleColor();
                        if (ImGui::IsWindowAppearing()) {
                            ImGui::SetScrollHereY();
                        }
                    }
                }
            }
            ImGui::End();
        }
    } else {
        g_autocompleteOpen = false;
    }
}

} // namespace Autocomplete
