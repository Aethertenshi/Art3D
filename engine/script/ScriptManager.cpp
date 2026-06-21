#include "ScriptManager.h"
#include <SDL3/SDL.h>
#include "qjspp.hpp"
#include "../scene/LogicScript.h"
#include "../editor/ConsoleWindow.h"
#include <sstream>
#include <algorithm>
#include <regex>

std::unique_ptr<qjs::Runtime> ScriptManager::s_Runtime;
std::unique_ptr<qjs::Context> ScriptManager::s_Context;
bool ScriptManager::s_Initialized = false;
std::unordered_map<std::string, JSValue> ScriptManager::s_ScriptRegistry;

void ScriptManager::Initialize() {
    if (s_Initialized) return;
    try {
        s_Runtime = std::make_unique<qjs::Runtime>();
        s_Context = std::make_unique<qjs::Context>(s_Runtime->rt);
        RegisterBindings();
        s_Initialized = true;
        ConsoleWindow::AddLog(SDL_LOG_PRIORITY_INFO, "QuickJS runtime initialized successfully.");
    } catch (const std::exception& e) {
        ConsoleWindow::AddLog(SDL_LOG_PRIORITY_ERROR, "Failed to initialize QuickJS: %s", e.what());
    }
}

void ScriptManager::Shutdown() {
    UnregisterAll();
    s_Context.reset();
    s_Runtime.reset();
    s_Initialized = false;
    ConsoleWindow::AddLog(SDL_LOG_PRIORITY_INFO, "QuickJS runtime shutdown.");
}

void ScriptManager::UnregisterScript(const std::string& name) {
    auto it = s_ScriptRegistry.find(name);
    if (it != s_ScriptRegistry.end()) {
        JS_FreeValue(s_Context->ctx(), it->second);
        s_ScriptRegistry.erase(it);
        ConsoleWindow::AddLog(SDL_LOG_PRIORITY_INFO, "[Export] Unregistered '%s'", name.c_str());
    }
}

void ScriptManager::UnregisterAll() {
    if (s_Context) {
        JSContext* ctx = s_Context->ctx();
        for (auto& [name, val] : s_ScriptRegistry) {
            JS_FreeValue(ctx, val);
        }
    }
    s_ScriptRegistry.clear();
}

std::string ScriptManager::TransformClassBody(const std::string& className, const std::string& body) {
    std::string result;
    std::istringstream stream(body);
    std::string line;
    bool inFunction = false;
    std::string funcName;
    std::string funcContent;
    int braceDepth = 0;

    while (std::getline(stream, line)) {
        if (!inFunction) {
            std::string trimmed = line;
            size_t firstNonSpace = trimmed.find_first_not_of(" \t\r");
            if (firstNonSpace == std::string::npos) {
                result += line + "\n";
                continue;
            }
            trimmed = trimmed.substr(firstNonSpace);

            if (trimmed.size() > 4 && (trimmed.substr(0, 4) == "let " || trimmed.substr(0, 4) == "var ")) {
                size_t eqPos = trimmed.find('=');
                size_t semiPos = trimmed.find(';');
                std::string prefix = line.substr(0, firstNonSpace);
                size_t nameStart = 4;
                while (nameStart < trimmed.size() && (trimmed[nameStart] == ' ' || trimmed[nameStart] == '\t')) nameStart++;
                if (eqPos != std::string::npos) {
                    size_t nameEnd = eqPos;
                    while (nameEnd > nameStart && (trimmed[nameEnd - 1] == ' ' || trimmed[nameEnd - 1] == '\t')) nameEnd--;
                    std::string propName = trimmed.substr(nameStart, nameEnd - nameStart);
                    result += prefix + className + "." + propName + " =" + trimmed.substr(eqPos + 1) + "\n";
                } else if (semiPos != std::string::npos) {
                    size_t nameEnd = semiPos;
                    while (nameEnd > nameStart && (trimmed[nameEnd - 1] == ' ' || trimmed[nameEnd - 1] == '\t')) nameEnd--;
                    std::string propName = trimmed.substr(nameStart, nameEnd - nameStart);
                    result += prefix + className + "." + propName + " = undefined;\n";
                } else {
                    result += line + "\n";
                }
                continue;
            }

            if (trimmed.size() > 8 && trimmed.substr(0, 8) == "function") {
                inFunction = true;
                size_t parenPos = trimmed.find('(', 8);
                if (parenPos != std::string::npos) {
                    size_t nameStart = 8;
                    while (nameStart < trimmed.size() && (trimmed[nameStart] == ' ' || trimmed[nameStart] == '\t')) nameStart++;
                    size_t nameEnd = parenPos;
                    while (nameEnd > nameStart && (trimmed[nameEnd - 1] == ' ' || trimmed[nameEnd - 1] == '\t')) nameEnd--;
                    funcName = trimmed.substr(nameStart, nameEnd - nameStart);
                }
                std::string prefix = line.substr(0, firstNonSpace);
                size_t funcBodyStart = trimmed.find('(');
                funcContent = prefix + className + "." + funcName + " = function" + trimmed.substr(funcBodyStart);
                braceDepth = 0;
                for (char c : funcContent) {
                    if (c == '{') braceDepth++;
                    else if (c == '}') braceDepth--;
                }
                if (braceDepth <= 0) {
                    result += funcContent + "\n";
                    inFunction = false;
                }
                continue;
            }
            result += line + "\n";
        } else {
            funcContent += "\n" + line;
            for (char c : line) {
                if (c == '{') braceDepth++;
                else if (c == '}') braceDepth--;
            }
            if (braceDepth <= 0) {
                result += funcContent + "\n";
                inFunction = false;
            }
        }
    }
    if (inFunction) {
        result += funcContent + "\n";
    }
    return result;
}

std::string ScriptManager::Preprocess(const std::string& code) {
    std::string result;
    std::vector<std::string> classNames;
    size_t pos = 0;

    while (pos < code.size()) {
        size_t declPos = code.find("export class ", pos);
        if (declPos == std::string::npos) {
            result += code.substr(pos);
            break;
        }
        result += code.substr(pos, declPos - pos);
        size_t nameStart = declPos + 13;
        while (nameStart < code.size() && code[nameStart] == ' ') nameStart++;
        size_t nameEnd = nameStart;
        while (nameEnd < code.size() && (isalnum((unsigned char)code[nameEnd]) || code[nameEnd] == '_')) nameEnd++;
        std::string className = code.substr(nameStart, nameEnd - nameStart);
        classNames.push_back(className);
        size_t bracePos = code.find('{', nameEnd);
        if (bracePos == std::string::npos) { result += code.substr(declPos); break; }
        int depth = 1;
        size_t closePos = bracePos + 1;
        while (closePos < code.size() && depth > 0) {
            if (code[closePos] == '{') depth++;
            else if (code[closePos] == '}') depth--;
            closePos++;
        }
        std::string body = code.substr(bracePos + 1, closePos - bracePos - 2);
        result += "var " + className + " = {};\n";
        result += TransformClassBody(className, body);
        pos = closePos;
    }

    if (!classNames.empty()) {
        result += "\nvar __art3d_exports__ = {};\n";
        for (const auto& name : classNames) {
            result += "__art3d_exports__[\"" + name + "\"] = " + name + ";\n";
        }
        result += "return __art3d_exports__;\n";
    }
    return result;
}

bool ScriptManager::RegisterScript(const std::string& scriptName, const std::string& code) {
    if (!s_Initialized) return false;
    try {
        std::string processed = Preprocess(code);
        bool hasExports = (processed.find("__art3d_exports__") != std::string::npos);
        if (!hasExports) return false;
        std::string wrapped = "(function(){\n" + processed + "\n})();";
        qjs::Value result = s_Context->eval(wrapped, scriptName.c_str());
        JSContext* ctx = s_Context->ctx();
        JSValue exportsObj = result.raw();
        if (JS_IsUndefined(exportsObj) || JS_IsException(exportsObj)) return false;
        JSPropertyEnum* tab = nullptr;
        uint32_t len = 0;
        int ret = JS_GetOwnPropertyNames(ctx, &tab, &len, exportsObj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);
        if (ret != 0 || !tab) return false;
        for (uint32_t i = 0; i < len; i++) {
            JSValue key = JS_AtomToString(ctx, tab[i].atom);
            const char* keyStr = JS_ToCString(ctx, key);
            if (keyStr) {
                JSValue val = JS_GetProperty(ctx, exportsObj, tab[i].atom);
                auto old = s_ScriptRegistry.find(keyStr);
                if (old != s_ScriptRegistry.end()) { JS_FreeValue(ctx, old->second); old->second = JS_DupValue(ctx, val); }
                else { s_ScriptRegistry[keyStr] = JS_DupValue(ctx, val); }
                JS_FreeValue(ctx, val);
                JS_FreeCString(ctx, keyStr);
            }
            JS_FreeValue(ctx, key);
        }
        js_free(ctx, tab);
        return true;
    } catch (const std::exception&) { return false; }
}

std::string ScriptManager::Execute(const std::string& scriptName, const std::string& code) {
    if (!s_Initialized) return "[error] runtime not initialized";
    UnregisterScript(scriptName);
    try {
        std::string processed = Preprocess(code);
        bool hasExports = (processed.find("__art3d_exports__") != std::string::npos);
        std::string wrapped = "(function(){\n" + processed + "\n})();";
        qjs::Value result = s_Context->eval(wrapped, scriptName.c_str());
        if (hasExports) {
            JSContext* ctx = s_Context->ctx();
            JSValue exportsObj = result.raw();
            if (!JS_IsUndefined(exportsObj) && !JS_IsException(exportsObj)) {
                JSPropertyEnum* tab = nullptr;
                uint32_t len = 0;
                int ret = JS_GetOwnPropertyNames(ctx, &tab, &len, exportsObj, JS_GPN_STRING_MASK | JS_GPN_ENUM_ONLY);
                if (ret == 0 && tab) {
                    for (uint32_t i = 0; i < len; i++) {
                        JSValue key = JS_AtomToString(ctx, tab[i].atom);
                        const char* keyStr = JS_ToCString(ctx, key);
                        if (keyStr) {
                            JSValue val = JS_GetProperty(ctx, exportsObj, tab[i].atom);
                            auto old = s_ScriptRegistry.find(keyStr);
                            if (old != s_ScriptRegistry.end()) { JS_FreeValue(ctx, old->second); old->second = JS_DupValue(ctx, val); }
                            else { s_ScriptRegistry[keyStr] = JS_DupValue(ctx, val); }
                            JS_FreeValue(ctx, val);
                            JS_FreeCString(ctx, keyStr);
                        }
                        JS_FreeValue(ctx, key);
                    }
                    js_free(ctx, tab);
                }
            }
        }
        JSContext* ctx = s_Context->ctx();
        JSValue raw = result.raw();
        JSValue strVal = JS_ToString(ctx, raw);
        std::string output;
        if (!JS_IsException(strVal)) {
            const char* str = JS_ToCString(ctx, strVal);
            output = str ? str : "[no output]";
            if (str) JS_FreeCString(ctx, str);
        } else { output = "[no output]"; JS_FreeValue(ctx, strVal); }
        if (!output.empty() && output != "undefined") {
            ConsoleWindow::AddLog(SDL_LOG_PRIORITY_INFO, "[%s] %s", scriptName.c_str(), output.c_str());
        }
        return output;
    } catch (const qjs::exception& e) {
        ConsoleWindow::AddLog(SDL_LOG_PRIORITY_ERROR, "[%s Error] %s", scriptName.c_str(), e.what());
        return "[error] " + std::string(e.what());
    } catch (const std::exception& e) {
        ConsoleWindow::AddLog(SDL_LOG_PRIORITY_ERROR, "[%s Error] %s", scriptName.c_str(), e.what());
        return "[error] " + std::string(e.what());
    }
}

void ScriptManager::RegisterBindings() {
    s_Context->registerFunction("__print_native", std::function<void(const std::string&)>(
        [](const std::string& msg) { ConsoleWindow::AddLog(SDL_LOG_PRIORITY_INFO, msg.c_str()); }
    ));
    s_Context->registerFunction("__warn_native", std::function<void(const std::string&)>(
        [](const std::string& msg) { ConsoleWindow::AddLog(SDL_LOG_PRIORITY_WARN, msg.c_str()); }
    ));
    s_Context->registerFunction("__error_native", std::function<void(const std::string&)>(
        [](const std::string& msg) { ConsoleWindow::AddLog(SDL_LOG_PRIORITY_ERROR, msg.c_str()); }
    ));
    s_Context->registerFunction("__native_findScript", std::function<JSValue(const std::string&)>(
        [](const std::string& name) -> JSValue {
            if (!s_Initialized || !s_Context) return JS_UNDEFINED;
            JSContext* ctx = s_Context->ctx();
            auto it = s_ScriptRegistry.find(name);
            if (it != s_ScriptRegistry.end()) return JS_DupValue(ctx, it->second);
            return JS_UNDEFINED;
        }
    ));
    s_Context->registerFunction("__native_listScripts", std::function<JSValue()>(
        []() -> JSValue {
            if (!s_Initialized || !s_Context) return JS_UNDEFINED;
            JSContext* ctx = s_Context->ctx();
            JSValue arr = JS_NewArray(ctx);
            uint32_t i = 0;
            for (const auto& [name, val] : s_ScriptRegistry) {
                JSValue str = JS_NewString(ctx, name.c_str());
                JS_SetPropertyUint32(ctx, arr, i++, str);
                JS_FreeValue(ctx, str);
            }
            return arr;
        }
    ));

    std::string setup = R"(
var print = function(msg) {
    var line = '?';
    try {
        var m = (new Error().stack || '').match(/<script>:(\d+):\d+\)/g);
        if (m) {
            var first = m[0];
            var l = first.match(/<script>:(\d+)/);
            if (l) line = parseInt(l[1], 10) - 1;
        }
    } catch(e) {}
    __print_native('[Script :' + line + '] ' + msg);
};
var warn = function(msg) {
    var line = '?';
    try {
        var m = (new Error().stack || '').match(/<script>:(\d+):\d+\)/g);
        if (m) {
            var first = m[0];
            var l = first.match(/<script>:(\d+)/);
            if (l) line = parseInt(l[1], 10) - 1;
        }
    } catch(e) {}
    __warn_native('[Script :' + line + '] ' + msg);
};
var error = function(msg) {
    var line = '?';
    try {
        var m = (new Error().stack || '').match(/<script>:(\d+):\d+\)/g);
        if (m) {
            var first = m[0];
            var l = first.match(/<script>:(\d+)/);
            if (l) line = parseInt(l[1], 10) - 1;
        }
    } catch(e) {}
    __error_native('[Script :' + line + '] ' + msg);
};
var scripts = {
    find: function(name) { return __native_findScript(name); },
    list: function() { return __native_listScripts(); }
};
)";
    s_Context->eval(setup, "<setup>");
}
