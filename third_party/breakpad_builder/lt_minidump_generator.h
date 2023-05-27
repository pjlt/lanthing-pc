#pragma once
#include <string>

#if defined(LT_WINDOWS)
#if defined(BUILDING_MINIDUMP)
#define MD_API _declspec(dllexport)
#else
#define MD_API _declspec(dllimport)
#endif
#else
#define MD_API
#endif

class MD_API LTMinidumpGenerator
{
public:
	LTMinidumpGenerator(const std::string& path);
	LTMinidumpGenerator(const LTMinidumpGenerator&) = delete;
	LTMinidumpGenerator& operator=(const LTMinidumpGenerator&) = delete;
	~LTMinidumpGenerator();

private:
	void* impl_ = nullptr;
};