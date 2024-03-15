#pragma once
#include <string>
#include <vector>
#include <functional>

class LTMinidumpGenerator
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