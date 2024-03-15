#pragma once
#include <string>
#include <vector>
#include <functional>

class LTMinidumpGenerator
{
public:
	LTMinidumpGenerator(const std::string& path, const std::string& program_name);
	LTMinidumpGenerator(const std::wstring& path, const std::wstring& program_name);
	LTMinidumpGenerator(const LTMinidumpGenerator&) = delete;
	LTMinidumpGenerator& operator=(const LTMinidumpGenerator&) = delete;
	~LTMinidumpGenerator();

	void addCallback(const std::function<void()>& callback);
	void invokeCallbacks();
	std::string programName() const;
	std::wstring programWName() const;

private:
	void* impl_ = nullptr;
	std::string program_name_;
	std::wstring program_wname_;
	std::vector<std::function<void()>> callbacks_;
};