#pragma once
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/color.h>
#include <fmt/chrono.h>
#include <fmt/ranges.h>
#include <fmt/os.h>

#if defined(BACKWARD_ENABLED)
#include <backward.hpp>
#endif

namespace snow::log {

#if defined(BACKWARD_ENABLED)
inline void print_stack_trace() {
    backward::StackTrace st; st.load_here(32);
    backward::Printer p;
    st.skip_n_firsts(4);  // omit stack frames of backward
    p.object = true;
    p.color_mode = backward::ColorMode::always;
    p.address = true;
    p.print(st, stderr);
}
#else
inline void print_stack_trace() {}
#endif

#if defined(NDEBUG)
// only log in debug
template<typename ...Args> inline void debug(const char *fmt, const Args &... args) {}
#else
// only log in debug
template<typename ...Args> inline void debug(const char *fmt, const Args &... args) {
    fmt::print(fg(fmt::color::orange), "(?) ");
    fmt::print(fg(fmt::color::orange), fmt, args...); fmt::print("\n");
}
#endif

template<typename ...Args> inline void info(const char *fmt, const Args &... args) {
    fmt::print(fg(fmt::rgb(0x00FFDB)), "(+) ");
    fmt::print(fg(fmt::rgb(0x00FFDB)), fmt, args...); fmt::print("\n");
}

template<typename ...Args> inline void warn(const char *fmt, const Args &... args) {
    fmt::print(fg(fmt::color::yellow), "(!) ");
    fmt::print(fg(fmt::color::yellow), fmt, args...); fmt::print("\n");
}

template<typename ...Args> inline void error(const char *fmt, const Args &... args) {
    fmt::print(fg(fmt::rgb(0xFF75B5)), "(E) ");
    fmt::print(fg(fmt::rgb(0xFF75B5)), fmt, args...); fmt::print("\n");
}

template<typename ...Args> inline void critical(const char *fmt, const Args &... args) {
    print_stack_trace();
    fmt::print(fmt::emphasis::bold | fg(fmt::rgb(0xFF2C6D)), "(F) ");
    fmt::print(fmt::emphasis::bold | fg(fmt::rgb(0xFF2C6D)), fmt, args...); fmt::print("\n");
    exit(1);
}

template<typename... Args> inline void check(bool flag) {
    if (flag) return;
    print_stack_trace();
    fmt::print(fmt::emphasis::bold | fg(fmt::rgb(0xFF75B5)), "(A) assertion failed!");
    exit(1);
}
template<typename ...Args> inline void check(bool flag, const char *fmt, const Args &... args) {
    if (flag) return;
    print_stack_trace();
    fmt::print(fmt::emphasis::bold | fg(fmt::rgb(0xFF75B5)), "(A) ");
    fmt::print(fmt::emphasis::bold | fg(fmt::rgb(0xFF75B5)), fmt, args...);
    fmt::print("\n");
    exit(1);
}

#if defined(NDEBUG)
template<typename... Args> inline void debug_check(bool flag) {}
template<typename ...Args> inline void debug_check(bool flag, const char *fmt, const Args &... args) {}
#else
template<typename... Args> inline void debug_check(bool flag) { check(flag); }
template<typename ...Args> inline void debug_check(bool flag, const char *fmt, const Args &... args) { check(flag, fmt, args...); }
#endif

}
