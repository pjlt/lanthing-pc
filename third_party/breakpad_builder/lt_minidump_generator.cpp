#include <iostream>
#include "lt_minidump_generator.h"

#if defined(LT_WINDOWS)
#include <exception_handler.h>
#include <common/windows/http_upload.h>

#define STR_HELPER(str) #str
#define VERSTR(a, b, c) L"v" STR_HELPER(a) "." STR_HELPER(b) "." STR_HELPER(c)
#define TOSTR(str) STR_HELPER(str)

const wchar_t* ltGetProgramName();

static bool minidump_callback(const wchar_t* dump_path,
	const wchar_t* minidump_id,
	void* context,
	EXCEPTION_POINTERS* exinfo,
	MDRawAssertionInfo* assertion,
	bool succeeded)
{
	(void)dump_path;
	(void)minidump_id;
	(void)context;
	(void)exinfo;
	(void)assertion;
	(void)succeeded;
	std::cout << "dump written" << std::endl;
	if constexpr (LT_DUMP) {
		std::map<std::wstring, std::wstring> parameters;
		std::map<std::wstring, std::wstring> files;
		int timeout = 1000;
		int response_code = 0;
		std::wstring response_body;
		parameters[L"build"] = L"" __DATE__ " " __TIME__;
		parameters[L"program"] = ltGetProgramName();
		parameters[L"version"] = VERSTR(LT_VERSION_MAJOR, LT_VERSION_MINOR, LT_VERSION_PATCH);
		std::wstring fullpath;
		fullpath = fullpath + dump_path + L"/" + minidump_id + L".dmp";
		files[L"file"] = fullpath;
		google_breakpad::HTTPUpload::SendMultipartPostRequest(L"http://" TOSTR(LT_DUMP_URL), parameters, files, &timeout, &response_body, &response_code);
	}
	auto that = reinterpret_cast<LTMinidumpGenerator*>(context);
	that->invokeCallbacks();
	return false;
}


LTMinidumpGenerator::LTMinidumpGenerator(const std::string& path)
{
	std::wstring wpath;
	google_breakpad::WindowsStringUtils::safe_mbstowcs(path, &wpath);
	auto handler = new google_breakpad::ExceptionHandler{
		wpath,
		nullptr, /*filter*/
		minidump_callback,
		this, /*context*/
		google_breakpad::ExceptionHandler::HANDLER_ALL
	};
	impl_ = handler;
}

LTMinidumpGenerator::~LTMinidumpGenerator()
{
	auto handler = reinterpret_cast<google_breakpad::ExceptionHandler*>(impl_);
	delete handler;
}

void LTMinidumpGenerator::addCallback(const std::function<void()>& callback)
{
	callbacks_.push_back(callback);
}

void LTMinidumpGenerator::invokeCallbacks()
{
	for (auto& cb : callbacks_) {
		cb();
	}
}

#else

LTMinidumpGenerator::LTMinidumpGenerator(const std::string& path) { (void)path; }

LTMinidumpGenerator::~LTMinidumpGenerator() {}

void LTMinidumpGenerator::addCallback(const std::function<void()>& callback) { (void)callback; }

void LTMinidumpGenerator::invokeCallbacks() {}

#endif