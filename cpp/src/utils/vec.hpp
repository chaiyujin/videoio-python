#pragma once
#include <iostream>

struct int2 {
    int32_t x = 0, y = 0;
};

inline int32_t prod(int2 const & _v) {
    return _v.x * _v.y;
}

inline std::ostream & operator<<(std::ostream & _out, int2 const & _v) {
    _out << "(" << _v.x << "," << _v.y << ")";
    return _out;
}
