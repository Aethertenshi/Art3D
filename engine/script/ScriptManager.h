#pragma once
#include <string>
#include <memory>
#include <unordered_map>
#include "quickjs/quickjs.h"

class LogicScript;

namespace qjs {
    class Runtime;
    class Context;
    class Value;
}

class ScriptManager {
public:
    static void Initialize();
    static void Shutdown();
    static std::string Execute(const std::string& scriptName, const std::string& code);
    static bool RegisterScript(const std::string& scriptName, const std::string& code);
    static void UnregisterScript(const std::string& name);
    static void UnregisterAll();

private:
    static void RegisterBindings();
    static std::string Preprocess(const std::string& code);
    static std::string TransformClassBody(const std::string& className, const std::string& body);

    static std::unique_ptr<qjs::Runtime> s_Runtime;
    static std::unique_ptr<qjs::Context> s_Context;
    static bool s_Initialized;
    static std::unordered_map<std::string, JSValue> s_ScriptRegistry;
};
