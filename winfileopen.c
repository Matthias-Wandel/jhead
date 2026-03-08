
#ifdef _WIN32
#include <io.h>
#include <windows.h>
#include <wchar.h>
//#include <wchar.h>
// Helper to convert UTF-8 string to Wide Char for Windows
FILE* fopen(const char* path, const char* mode) {
    wchar_t wPath[MAX_PATH];
    wchar_t wMode[10];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
    MultiByteToWideChar(CP_UTF8, 0, mode, -1, wMode, 10);
    return _wfopen(wPath, wMode);
}
#endif
