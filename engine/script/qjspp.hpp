#pragma once

#include "quickjs/quickjs.h"
#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include <type_traits>
#include <stdexcept>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>

namespace qjs {

class Context;

class exception : public std::exception {
    std::string msg_;
public:
    exception(std::string msg) : msg_(std::move(msg)) {}
    const char* what() const noexcept override { return msg_.c_str(); }
};

class Value {
    friend class Context;
    JSContext* m_ctx = nullptr;
    JSValue m_val = JS_UNDEFINED;
    Value(JSContext* ctx, JSValue val, bool allow_exception = false) : m_ctx(ctx), m_val(val) {
        if (!allow_exception && JS_IsException(m_val)) throw fetch_exception(ctx);
    }
    static exception fetch_exception(JSContext* ctx) {
        JSValue exc = JS_GetException(ctx);
        const char* s = JS_ToCString(ctx, exc);
        std::string msg = s ? s : "js exception";
        if (s) JS_FreeCString(ctx, s);
        JS_FreeValue(ctx, exc);
        return exception(msg);
    }
public:
    Value() = default;
    Value(Value&& o) noexcept : m_ctx(o.m_ctx), m_val(o.m_val) { o.m_ctx = nullptr; o.m_val = JS_UNDEFINED; }
    Value(const Value& o) : m_ctx(o.m_ctx) { m_val = JS_DupValue(m_ctx, o.m_val); }
    Value& operator=(Value o) noexcept { std::swap(m_ctx, o.m_ctx); std::swap(m_val, o.m_val); return *this; }
    ~Value() { if (m_ctx) JS_FreeValue(m_ctx, m_val); }

    template<typename T> T as() const;
    JSValue release() { auto v = m_val; m_ctx = nullptr; m_val = JS_UNDEFINED; return v; }
    JSValue raw() const { return m_val; }
    bool isException() const { return JS_IsException(m_val); }
};

namespace detail {

struct store {
    std::function<JSValue(JSContext*, int, JSValueConst*)> func;
};

inline std::vector<store>& registry() {
    static std::vector<store> r;
    return r;
}

inline JSValue tramp(JSContext* ctx, JSValueConst, int argc, JSValueConst* argv, int magic) {
    try { return registry()[magic].func(ctx, argc, argv); }
    catch (...) { return JS_EXCEPTION; }
}

template<typename T> struct fr;
template<> struct fr<double> { static double go(JSContext* ctx, JSValueConst v) { double r; if (JS_ToFloat64(ctx, &r, v)) throw exception("expected number"); return r; } };
template<> struct fr<int32_t> { static int32_t go(JSContext* ctx, JSValueConst v) { int32_t r; if (JS_ToInt32(ctx, &r, v)) throw exception("expected integer"); return r; } };
template<> struct fr<bool> { static bool go(JSContext* ctx, JSValueConst v) { return JS_ToBool(ctx, v) > 0; } };
template<> struct fr<std::string> { static std::string go(JSContext* ctx, JSValueConst v) { const char* s = JS_ToCString(ctx, v); if (!s) throw exception("expected string"); std::string r(s); JS_FreeCString(ctx, s); return r; } };

template<typename T> struct t0;
template<> struct t0<double> { static JSValue go(JSContext* ctx, double v) { return JS_NewFloat64(ctx, v); } };
template<> struct t0<int32_t> { static JSValue go(JSContext* ctx, int32_t v) { return JS_NewInt32(ctx, v); } };
template<> struct t0<bool> { static JSValue go(JSContext* ctx, bool v) { return JS_NewBool(ctx, v); } };
template<> struct t0<std::string> { static JSValue go(JSContext* ctx, const std::string& v) { return JS_NewStringLen(ctx, v.data(), v.size()); } };
template<> struct t0<const char*> { static JSValue go(JSContext* ctx, const char* v) { return JS_NewString(ctx, v); } };
template<> struct t0<JSValue> { static JSValue go(JSContext* ctx, JSValue v) { return v; } };

template<typename R, typename... Args, size_t... I>
JSValue call_seq(std::function<R(Args...)>& f, JSContext* ctx, JSValueConst* argv, std::index_sequence<I...>) {
    if constexpr (std::is_same_v<R, void>) {
        f(fr<std::decay_t<Args>>::go(ctx, argv[I])...);
        return JS_UNDEFINED;
    } else {
        return t0<R>::go(ctx, f(fr<std::decay_t<Args>>::go(ctx, argv[I])...));
    }
}

template<typename R, typename... Args>
struct binder {
    static JSValue call(JSContext* ctx, std::function<R(Args...)>& f, int argc, JSValueConst* argv) {
        if ((size_t)argc < sizeof...(Args))
            return JS_ThrowTypeError(ctx, "expected %zu args", sizeof...(Args));
        try { return call_seq(f, ctx, argv, std::index_sequence_for<Args...>{}); }
        catch (const exception&) { return JS_EXCEPTION; }
    }
};

template<typename... Args>
void wrap_args(JSContext* ctx, JSValue* argv, Args&&... args) {
    size_t i = 0;
    ((argv[i++] = t0<std::decay_t<Args>>::go(ctx, std::forward<Args>(args))), ...);
}

inline std::string readFile(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error(std::string("cannot open ") + path);
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace detail

class Runtime {
public:
    JSRuntime* rt;
    Runtime() { rt = JS_NewRuntime(); if (!rt) throw std::runtime_error("cannot create runtime"); }
    ~Runtime() { JS_FreeRuntime(rt); }
    Runtime(const Runtime&) = delete;
    Runtime& operator=(const Runtime&) = delete;
};

template<> inline std::string Value::as<std::string>() const { return detail::fr<std::string>::go(m_ctx, m_val); }
template<> inline double Value::as<double>() const { return detail::fr<double>::go(m_ctx, m_val); }
template<> inline int32_t Value::as<int32_t>() const { return detail::fr<int32_t>::go(m_ctx, m_val); }
template<> inline bool Value::as<bool>() const { return detail::fr<bool>::go(m_ctx, m_val); }

class Context {
    JSContext* m_ctx = nullptr;
public:
    Context(JSRuntime* rt) {
        m_ctx = JS_NewContext(rt);
        if (!m_ctx) throw std::runtime_error("cannot create context");
        JS_SetContextOpaque(m_ctx, this);
    }
    ~Context() { if (m_ctx) JS_FreeContext(m_ctx); }
    Context(const Context&) = delete;

    JSContext* ctx() const { return m_ctx; }

    Value eval(std::string_view code, const char* filename = "<eval>", int flags = JS_EVAL_TYPE_GLOBAL) {
        return Value(m_ctx, JS_Eval(m_ctx, code.data(), code.size(), filename, flags));
    }

    Value evalFile(const char* path) {
        std::string code = detail::readFile(path);
        return Value(m_ctx, JS_Eval(m_ctx, code.data(), code.size(), path, JS_EVAL_TYPE_GLOBAL));
    }

    template<typename... Args>
    Value call(const char* name, Args&&... args) {
        JSValue global = JS_GetGlobalObject(m_ctx);
        JSValue fn = JS_GetPropertyStr(m_ctx, global, name);
        JS_FreeValue(m_ctx, global);
        if (JS_IsException(fn)) { JS_FreeValue(m_ctx, fn); throw exception("function not found: " + std::string(name)); }
        if (!JS_IsFunction(m_ctx, fn)) { JS_FreeValue(m_ctx, fn); throw exception("not a function: " + std::string(name)); }
        JSValue argv[sizeof...(Args)];
        detail::wrap_args(m_ctx, argv, std::forward<Args>(args)...);
        JSValue result = JS_Call(m_ctx, fn, JS_UNDEFINED, (int)sizeof...(Args), argv);
        for (auto& v : argv) JS_FreeValue(m_ctx, v);
        JS_FreeValue(m_ctx, fn);
        return Value(m_ctx, result, true);
    }

    template<typename R, typename... Args>
    void registerFunction(const char* name, std::function<R(Args...)> f) {
        auto& reg = detail::registry();
        int id = (int)reg.size();
        reg.push_back({[f = std::move(f)](JSContext* ctx, int argc, JSValueConst* argv) mutable -> JSValue {
            return detail::binder<R, Args...>::call(ctx, f, argc, argv);
        }});
        JSValue global = JS_GetGlobalObject(m_ctx);
        JSValue jsv = JS_NewCFunctionMagic(m_ctx, detail::tramp, name, (int)sizeof...(Args), JS_CFUNC_generic_magic, id);
        JS_SetPropertyStr(m_ctx, global, name, jsv);
        JS_FreeValue(m_ctx, global);
    }
};

} // namespace qjs
