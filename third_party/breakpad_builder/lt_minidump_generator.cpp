#include <iostream>
#include "lt_minidump_generator.h"

#if defined(LT_WINDOWS)
#include <exception_handler.h>

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