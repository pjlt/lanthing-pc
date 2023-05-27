#include <iostream>
#include <exception_handler.h>
#include "lt_minidump_generator.h"

static bool minidump_callback(const wchar_t* dump_path,
	const wchar_t* minidump_id,
	void* context,
	EXCEPTION_POINTERS* exinfo,
	MDRawAssertionInfo* assertion,
	bool succeeded)
{
	std::cout << "dump written" << std::endl;
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
		nullptr, /*context*/
		google_breakpad::ExceptionHandler::HANDLER_ALL
	};
	impl_ = handler;
}

LTMinidumpGenerator::~LTMinidumpGenerator()
{
	auto handler = reinterpret_cast<google_breakpad::ExceptionHandler*>(impl_);
	delete handler;
}