#pragma once
#include <string>
#include <vector>
#include "imgui.h"
#include "TextEditor.h"

namespace Autocomplete {

extern bool g_autocompleteOpen;
extern bool g_autocompleteDismissed;
extern int g_autocompleteActiveIndex;
extern bool g_autocompleteSelectTriggered;
extern bool g_autocompleteCloseTriggered;
extern std::vector<std::string> g_autocompleteMatches;
extern const std::vector<std::string> combinedKeywordsList;

std::string GetCurrentWordPrefix(const TextEditor& editor);
void InsertSuggestion(TextEditor& editor, const std::string& suggestion);
void ProcessAutocomplete(TextEditor& editor);

} // namespace Autocomplete
