#ifndef DEBUG_MACROS_H
#define DEBUG_MACROS_H


//#define GRID_DEBUG

template<class X, class A>
inline void Assert(A assertion) {
    if (!assertion) throw X();
}
#endif // DEBUG_MACROS_H
