// only support
//  windows - msvc
//  linux - gcc
#if defined(LT_WINDOWS)
#define WARNING_DISABLE(xx) __pragma(warning(disable : xx))
#define WARNING_ENABLE(xx) __pragma(warning(default : xx))
#elif defined(LT_LINUX)
#define WARNING_DISABLE(xx)
#define WARNING_ENABLE(xx)
#else
#define WARNING_DISABLE(xx)
#define WARNING_ENABLE(xx)
#endif