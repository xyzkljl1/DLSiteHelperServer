/*export module Util;
import std;
import std.compat;
import <cstdarg>;
import <cstdio>;
export void LogError(const char* _Format, ...)
{
	char buff[1024 * 24] = { 0 };
	va_list args;
	va_start(args, _Format);
	vsprintf_s(buff, sizeof(buff), _Format, args);
	va_end(args);
	fprintf(stderr, buff);
}

export void Log(const char* _Format, ...)
{
	char buff[1024 * 24] = { 0 };
	va_list args;
	va_start(args, _Format);
	vsprintf_s(buff, sizeof(buff), _Format, args);
	va_end(args);
	fprintf(stdout, buff);
}

export std::string Format(const char* Format, ...)
{
	char buff[1024 * 24] = { 0 };

	va_list args;
	va_start(args, Format);
	vsprintf_s(buff, sizeof(buff), Format, args);
	va_end(args);

	std::string str(buff);
	return str;
}*/