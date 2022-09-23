#pragma once
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>
#include <spdlog/fmt/ostr.h>
#include <spdlog/fmt/chrono.h>

#if defined(BACKWARD_ENABLED)
#include <backward.hpp>
#endif

namespace spdlog {

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

}
