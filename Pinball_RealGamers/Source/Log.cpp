#include "Globals.h"

#include <stdarg.h>
#include <stdio.h>

void log(const char file[], int line, const char* format, ...)
{
	static char tmp_string[4096];

	va_list ap;
	va_start(ap, format);
	vsnprintf(tmp_string, sizeof(tmp_string), format, ap);
	va_end(ap);

	// Console output. (Don't include <windows.h> here to use OutputDebugStringA:
	// it clashes with raylib names like Rectangle, DrawText or CloseWindow)
	printf("\n%s(%d) : %s", file, line, tmp_string);
	fflush(stdout);
}