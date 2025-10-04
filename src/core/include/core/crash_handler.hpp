#pragma once

#include <string>

class CrashHandler {
public:
    // directory where .dmp + .stack.txt will be written (e.g. "./crash")
    static void install(const std::wstring& dumpDir = L".");
};
