#pragma once

#ifdef _MSC_VER

#include "../Util/WinHeader.h"
#include <Dbghelp.h>

void makeMinidump(EXCEPTION_POINTERS* e)
{
    auto hDbgHelp = LoadLibraryA("dbghelp");

    if(hDbgHelp == nullptr)
        return;

    auto pMiniDumpWriteDump = (decltype(&MiniDumpWriteDump))GetProcAddress(hDbgHelp, "MiniDumpWriteDump");

    if(pMiniDumpWriteDump == nullptr)
        return;

    char name[MAX_PATH];
    {
        char* nameEnd = name + GetModuleFileNameA(GetModuleHandleA(0), name, MAX_PATH);

        SYSTEMTIME t;
        GetSystemTime(&t);
        wsprintfA(nameEnd - strlen(".exe"), "_%4d%02d%02d_%02d%02d%02d.dmp",
            t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
    }

    HANDLE hFile = CreateFileA(name, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);

    if(hFile == INVALID_HANDLE_VALUE)
        return;

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo;
    exceptionInfo.ThreadId = GetCurrentThreadId();
    exceptionInfo.ExceptionPointers = e;
    exceptionInfo.ClientPointers = FALSE;

    pMiniDumpWriteDump(GetCurrentProcess(),
                       GetCurrentProcessId(),
                       hFile,
                       MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpScanMemory),
                       e ? &exceptionInfo : nullptr,
                       nullptr,
                       nullptr);

    CloseHandle(hFile);

    return;
}

LONG CALLBACK unhandledExceptionHandler(EXCEPTION_POINTERS* e)
{
    makeMinidump(e);

    return EXCEPTION_CONTINUE_SEARCH;
}

#endif