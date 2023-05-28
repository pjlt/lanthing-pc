#pragma once

#if defined(LT_WINDOWS)
#if defined(BUILDING_LTLIB)
#define LT_API _declspec(dllexport)
#else
#define LT_API _declspec(dllimport)
#endif
#else
#define LT_API
#endif