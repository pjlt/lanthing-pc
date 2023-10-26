#pragma once
#include <string>
#include <vector>
#include <functional>

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

	void addCallback(const std::function<void()>& callback);
	void invokeCallbacks();

private:
	void* impl_ = nullptr;
	std::vector<std::function<void()>> callbacks_;
};