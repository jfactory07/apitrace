/**************************************************************************
 *
 * Copyright 2011-2012 Jose Fonseca
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 **************************************************************************/


/*
 * Code for the DLL that will be injected in the target process.
 *
 * The injected DLL will manipulate the import tables to hook the
 * modules/functions of interest.
 *
 * See also:
 * - http://www.codeproject.com/KB/system/api_spying_hack.aspx
 * - http://www.codeproject.com/KB/threads/APIHooking.aspx
 * - http://msdn.microsoft.com/en-us/magazine/cc301808.aspx
 */


#include <assert.h>
#include <stdio.h>
#include <stdarg.h>

#include <set>

#include <windows.h>
#include <tlhelp32.h>

#include "inject.h"


#define VERBOSITY 0
#define NOOP 0


static CRITICAL_SECTION Mutex = {(PCRITICAL_SECTION_DEBUG)-1, -1, 0, 0, 0, 0};



static HMODULE g_hThisModule = NULL;


static void
debugPrintf(const char *format, ...)
{
    char buf[512];

    va_list ap;
    va_start(ap, format);
    _vsnprintf(buf, sizeof buf, format, ap);
    va_end(ap);

    OutputDebugStringA(buf);
}


static HMODULE WINAPI
MyLoadLibraryA(LPCSTR lpLibFileName);

static HMODULE WINAPI
MyLoadLibraryW(LPCWSTR lpLibFileName);

static HMODULE WINAPI
MyLoadLibraryExA(LPCSTR lpFileName, HANDLE hFile, DWORD dwFlags);

static HMODULE WINAPI
MyLoadLibraryExW(LPCWSTR lpFileName, HANDLE hFile, DWORD dwFlags);

static FARPROC WINAPI
MyGetProcAddress(HMODULE hModule, LPCSTR lpProcName);


static void
MyCreateProcessCommon(BOOL bRet,
                      DWORD dwCreationFlags,
                      LPPROCESS_INFORMATION lpProcessInformation)
{
    if (!bRet) {
        return;
    }

    char szDllPath[MAX_PATH];
    GetModuleFileNameA(g_hThisModule, szDllPath, sizeof szDllPath);

    const char *szError = NULL;
    if (!injectDll(lpProcessInformation->hProcess, szDllPath, &szError)) {
        debugPrintf("warning: failed to inject child process (%s)\n", szError);
    }

    if (!(dwCreationFlags & CREATE_SUSPENDED)) {
        ResumeThread(lpProcessInformation->hThread);
    }
}

static BOOL WINAPI
MyCreateProcessA(LPCSTR lpApplicationName,
                 LPSTR lpCommandLine,
                 LPSECURITY_ATTRIBUTES lpProcessAttributes,
                 LPSECURITY_ATTRIBUTES lpThreadAttributes,
                 BOOL bInheritHandles,
                 DWORD dwCreationFlags,
                 LPVOID lpEnvironment,
                 LPCSTR lpCurrentDirectory,
                 LPSTARTUPINFOA lpStartupInfo,
                 LPPROCESS_INFORMATION lpProcessInformation)
{
    if (VERBOSITY >= 2) {
        debugPrintf("%s(\"%s\", \"%s\", ...)\n",
                    __FUNCTION__,
                    lpApplicationName,
                    lpCommandLine);
    }

    BOOL bRet;
    bRet = CreateProcessA(lpApplicationName,
                          lpCommandLine,
                          lpProcessAttributes,
                          lpThreadAttributes,
                          bInheritHandles,
                          dwCreationFlags | CREATE_SUSPENDED,
                          lpEnvironment,
                          lpCurrentDirectory,
                          lpStartupInfo,
                          lpProcessInformation);

    MyCreateProcessCommon(bRet, dwCreationFlags, lpProcessInformation);

    return bRet;
}

static BOOL WINAPI
MyCreateProcessW(LPCWSTR lpApplicationName,
                 LPWSTR lpCommandLine,
                 LPSECURITY_ATTRIBUTES lpProcessAttributes,
                 LPSECURITY_ATTRIBUTES lpThreadAttributes,
                 BOOL bInheritHandles,
                 DWORD dwCreationFlags,
                 LPVOID lpEnvironment,
                 LPCWSTR lpCurrentDirectory,
                 LPSTARTUPINFOW lpStartupInfo,
                 LPPROCESS_INFORMATION lpProcessInformation)
{
    if (VERBOSITY >= 2) {
        debugPrintf("%s(\"%S\", \"%S\", ...)\n",
                    __FUNCTION__,
                    lpApplicationName,
                    lpCommandLine);
    }

    BOOL bRet;
    bRet = CreateProcessW(lpApplicationName,
                          lpCommandLine,
                          lpProcessAttributes,
                          lpThreadAttributes,
                          bInheritHandles,
                          dwCreationFlags | CREATE_SUSPENDED,
                          lpEnvironment,
                          lpCurrentDirectory,
                          lpStartupInfo,
                          lpProcessInformation);

    MyCreateProcessCommon(bRet, dwCreationFlags, lpProcessInformation);

    return bRet;
}

static BOOL WINAPI
MyCreateProcessAsUserA(HANDLE hToken,
                       LPCSTR lpApplicationName,
                       LPSTR lpCommandLine,
                       LPSECURITY_ATTRIBUTES lpProcessAttributes,
                       LPSECURITY_ATTRIBUTES lpThreadAttributes,
                       BOOL bInheritHandles,
                       DWORD dwCreationFlags,
                       LPVOID lpEnvironment,
                       LPCSTR lpCurrentDirectory,
                       LPSTARTUPINFOA lpStartupInfo,
                       LPPROCESS_INFORMATION lpProcessInformation)
{
    if (VERBOSITY >= 2) {
        debugPrintf("%s(\"%S\", \"%S\", ...)\n",
                    __FUNCTION__,
                    lpApplicationName,
                    lpCommandLine);
    }

    BOOL bRet;
    bRet = CreateProcessAsUserA(hToken,
                               lpApplicationName,
                               lpCommandLine,
                               lpProcessAttributes,
                               lpThreadAttributes,
                               bInheritHandles,
                               dwCreationFlags,
                               lpEnvironment,
                               lpCurrentDirectory,
                               lpStartupInfo,
                               lpProcessInformation);

    MyCreateProcessCommon(bRet, dwCreationFlags, lpProcessInformation);

    return bRet;
}

static BOOL WINAPI
MyCreateProcessAsUserW(HANDLE hToken,
                       LPCWSTR lpApplicationName,
                       LPWSTR lpCommandLine,
                       LPSECURITY_ATTRIBUTES lpProcessAttributes,
                       LPSECURITY_ATTRIBUTES lpThreadAttributes,
                       BOOL bInheritHandles,
                       DWORD dwCreationFlags,
                       LPVOID lpEnvironment,
                       LPCWSTR lpCurrentDirectory,
                       LPSTARTUPINFOW lpStartupInfo,
                       LPPROCESS_INFORMATION lpProcessInformation)
{
    if (VERBOSITY >= 2) {
        debugPrintf("%s(\"%S\", \"%S\", ...)\n",
                    __FUNCTION__,
                    lpApplicationName,
                    lpCommandLine);
    }

    BOOL bRet;
    bRet = CreateProcessAsUserW(hToken,
                               lpApplicationName,
                               lpCommandLine,
                               lpProcessAttributes,
                               lpThreadAttributes,
                               bInheritHandles,
                               dwCreationFlags,
                               lpEnvironment,
                               lpCurrentDirectory,
                               lpStartupInfo,
                               lpProcessInformation);

    MyCreateProcessCommon(bRet, dwCreationFlags, lpProcessInformation);

    return bRet;
}


static const char *
getImportDescriptionName(HMODULE hModule, const PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor) {
    const char* szName = (const char*)((PBYTE)hModule + pImportDescriptor->Name);
    return szName;
}


static PIMAGE_IMPORT_DESCRIPTOR
getImportDescriptor(HMODULE hModule,
                    const char *szModule,
                    const char *pszDllName)
{
    MEMORY_BASIC_INFORMATION MemoryInfo;
    if (VirtualQuery(hModule, &MemoryInfo, sizeof MemoryInfo) != sizeof MemoryInfo) {
        debugPrintf("%s: %s: VirtualQuery failed\n", __FUNCTION__, szModule);
        return NULL;
    }
    if (MemoryInfo.Protect & (PAGE_NOACCESS | PAGE_EXECUTE)) {
        debugPrintf("%s: %s: no read access (Protect = 0x%08x)\n", __FUNCTION__, szModule, MemoryInfo.Protect);
        return NULL;
    }

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)hModule;
    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((PBYTE)hModule + pDosHeader->e_lfanew);

    PIMAGE_OPTIONAL_HEADER pOptionalHeader = &pNtHeaders->OptionalHeader;

    UINT_PTR ImportAddress = pOptionalHeader->DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;

    if (!ImportAddress) {
        return NULL;
    }

    PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor = (PIMAGE_IMPORT_DESCRIPTOR)((PBYTE)hModule + ImportAddress);

    while (pImportDescriptor->FirstThunk) {
        const char* szName = getImportDescriptionName(hModule, pImportDescriptor);
        if (stricmp(pszDllName, szName) == 0) {
            return pImportDescriptor;
        }
        ++pImportDescriptor;
    }

    return NULL;
}


static BOOL
replaceAddress(LPVOID *lpOldAddress, LPVOID lpNewAddress)
{
    DWORD flOldProtect;

    if (*lpOldAddress == lpNewAddress) {
        return TRUE;
    }

    EnterCriticalSection(&Mutex);

    if (!(VirtualProtect(lpOldAddress, sizeof *lpOldAddress, PAGE_READWRITE, &flOldProtect))) {
        LeaveCriticalSection(&Mutex);
        return FALSE;
    }

    *lpOldAddress = lpNewAddress;

    if (!(VirtualProtect(lpOldAddress, sizeof *lpOldAddress, flOldProtect, &flOldProtect))) {
        LeaveCriticalSection(&Mutex);
        return FALSE;
    }

    LeaveCriticalSection(&Mutex);
    return TRUE;
}


static LPVOID *
getOldFunctionAddress(HMODULE hModule,
                    PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor,
                    const char* pszFunctionName)
{
    PIMAGE_THUNK_DATA pOriginalFirstThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + pImportDescriptor->OriginalFirstThunk);
    PIMAGE_THUNK_DATA pFirstThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + pImportDescriptor->FirstThunk);

    if (VERBOSITY >= 4) {
        debugPrintf("  %s\n", __FUNCTION__);
    }

    while (pOriginalFirstThunk->u1.Function) {
        PIMAGE_IMPORT_BY_NAME pImport = (PIMAGE_IMPORT_BY_NAME)((PBYTE)hModule + pOriginalFirstThunk->u1.AddressOfData);
        const char* szName = (const char* )pImport->Name;
        if (VERBOSITY >= 4) {
            debugPrintf("    %s\n", szName);
        }
        if (strcmp(pszFunctionName, szName) == 0) {
            if (VERBOSITY >= 4) {
                debugPrintf("  %s succeeded\n", __FUNCTION__);
            }
            return (LPVOID *)(&pFirstThunk->u1.Function);
        }
        ++pOriginalFirstThunk;
        ++pFirstThunk;
    }

    if (VERBOSITY >= 4) {
        debugPrintf("  %s failed\n", __FUNCTION__);
    }

    return NULL;
}


static void
replaceModule(HMODULE hModule,
              const char *szModule,
              PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor,
              HMODULE hNewModule)
{
    PIMAGE_THUNK_DATA pOriginalFirstThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + pImportDescriptor->OriginalFirstThunk);
    PIMAGE_THUNK_DATA pFirstThunk = (PIMAGE_THUNK_DATA)((PBYTE)hModule + pImportDescriptor->FirstThunk);

    while (pOriginalFirstThunk->u1.Function) {
        PIMAGE_IMPORT_BY_NAME pImport = (PIMAGE_IMPORT_BY_NAME)((PBYTE)hModule + pOriginalFirstThunk->u1.AddressOfData);
        const char* szFunctionName = (const char* )pImport->Name;
        if (VERBOSITY > 0) {
            debugPrintf("      hooking %s->%s!%s\n", szModule,
                    getImportDescriptionName(hModule, pImportDescriptor),
                    szFunctionName);
        }

        PROC pNewProc = GetProcAddress(hNewModule, szFunctionName);
        if (!pNewProc) {
            debugPrintf("warning: no replacement for %s\n", szFunctionName);
        } else {
            LPVOID *lpOldAddress = (LPVOID *)(&pFirstThunk->u1.Function);
            replaceAddress(lpOldAddress, (LPVOID)pNewProc);
            if (pSharedMem) {
                pSharedMem->bReplaced = TRUE;
            }
        }

        ++pOriginalFirstThunk;
        ++pFirstThunk;
    }
}


static BOOL
hookFunction(HMODULE hModule,
             const char *szModule,
             const char *pszDllName,
             const char *pszFunctionName,
             LPVOID lpNewAddress)
{
    PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor = getImportDescriptor(hModule, szModule, pszDllName);
    if (pImportDescriptor == NULL) {
        return FALSE;
    }
    LPVOID* lpOldFunctionAddress = getOldFunctionAddress(hModule, pImportDescriptor, pszFunctionName);
    if (lpOldFunctionAddress == NULL) {
        return FALSE;
    }

    if (*lpOldFunctionAddress == lpNewAddress) {
        return TRUE;
    }

    if (VERBOSITY >= 3) {
        debugPrintf("      hooking %s->%s!%s\n", szModule, pszDllName, pszFunctionName);
    }

    return replaceAddress(lpOldFunctionAddress, lpNewAddress);
}


static BOOL
replaceImport(HMODULE hModule,
              const char *szModule,
              const char *pszDllName,
              HMODULE hNewModule)
{
    if (NOOP) {
        return TRUE;
    }

    PIMAGE_IMPORT_DESCRIPTOR pImportDescriptor = getImportDescriptor(hModule, szModule, pszDllName);
    if (pImportDescriptor == NULL) {
        return TRUE;
    }

    replaceModule(hModule, szModule, pImportDescriptor, hNewModule);

    return TRUE;
}


struct Replacement {
    const char *szMatchModule;
    HMODULE hReplaceModule;
};

static unsigned numReplacements = 0;
static Replacement replacements[32];


static void
hookLibraryLoaderFunctions(HMODULE hModule,
                           const char *szModule,
                           const char *pszDllName)
{
    hookFunction(hModule, szModule, pszDllName, "LoadLibraryA", (LPVOID)MyLoadLibraryA);
    hookFunction(hModule, szModule, pszDllName, "LoadLibraryW", (LPVOID)MyLoadLibraryW);
    hookFunction(hModule, szModule, pszDllName, "LoadLibraryExA", (LPVOID)MyLoadLibraryExA);
    hookFunction(hModule, szModule, pszDllName, "LoadLibraryExW", (LPVOID)MyLoadLibraryExW);
    hookFunction(hModule, szModule, pszDllName, "GetProcAddress", (LPVOID)MyGetProcAddress);
}

static void
hookProcessThreadsFunctions(HMODULE hModule,
                            const char *szModule,
                            const char *pszDllName)
{
    hookFunction(hModule, szModule, pszDllName, "CreateProcessA", (LPVOID)MyCreateProcessA);
    hookFunction(hModule, szModule, pszDllName, "CreateProcessW", (LPVOID)MyCreateProcessW);
    hookFunction(hModule, szModule, pszDllName, "CreateProcessAsUserA", (LPVOID)MyCreateProcessAsUserA);
    hookFunction(hModule, szModule, pszDllName, "CreateProcessAsUserW", (LPVOID)MyCreateProcessAsUserW);
}


/* Set of previously hooked modules */
static std::set<HMODULE>
g_hHookedModules;


static void
hookModule(HMODULE hModule,
           const char *szModule)
{
    /* Hook modules only once */
    std::pair< std::set<HMODULE>::iterator, bool > ret;
    EnterCriticalSection(&Mutex);
    ret = g_hHookedModules.insert(hModule);
    LeaveCriticalSection(&Mutex);
    if (!ret.second) {
        return;
    }

    /* Never hook this module */
    if (hModule == g_hThisModule) {
        return;
    }

    /* Don't hook our replacement modules (based on HMODULE) */
    for (unsigned i = 0; i < numReplacements; ++i) {
        if (hModule == replacements[i].hReplaceModule) {
            return;
        }
    }

    const char *szBaseName = getBaseName(szModule);

    /* Don't hook our replacement modules (based on MODULE name) */
    for (unsigned i = 0; i < numReplacements; ++i) {
        if (stricmp(szBaseName, replacements[i].szMatchModule) == 0) {
            return;
        }
    }

    /* Leave these modules alone */
    if (stricmp(szBaseName, "kernel32.dll") == 0 ||
        stricmp(szBaseName, "ConEmuHk.dll") == 0) {
        return;
    }

    /*
     * Hook kernel32.dll functions, and its respective Windows API Set.
     *
     * http://msdn.microsoft.com/en-us/library/dn505783.aspx (Windows 8.1)
     * http://msdn.microsoft.com/en-us/library/hh802935.aspx (Windows 8)
     */

    hookLibraryLoaderFunctions(hModule, szModule, "kernel32.dll");
    hookLibraryLoaderFunctions(hModule, szModule, "api-ms-win-core-libraryloader-l1-1-0.dll");
    hookLibraryLoaderFunctions(hModule, szModule, "api-ms-win-core-libraryloader-l1-1-1.dll");
    hookLibraryLoaderFunctions(hModule, szModule, "api-ms-win-core-libraryloader-l1-2-0.dll");

    hookProcessThreadsFunctions(hModule, szModule, "kernel32.dll");
    hookProcessThreadsFunctions(hModule, szModule, "api-ms-win-core-processthreads-l1-1-0.dll");
    hookProcessThreadsFunctions(hModule, szModule, "api-ms-win-core-processthreads-l1-1-1.dll");
    hookProcessThreadsFunctions(hModule, szModule, "api-ms-win-core-processthreads-l1-1-2.dll");

    for (unsigned i = 0; i < numReplacements; ++i) {
        replaceImport(hModule, szModule, replacements[i].szMatchModule, replacements[i].hReplaceModule);
        replaceImport(hModule, szModule, replacements[i].szMatchModule, replacements[i].hReplaceModule);
        replaceImport(hModule, szModule, replacements[i].szMatchModule, replacements[i].hReplaceModule);
    }
}

static void
hookAllModules(void)
{
    HANDLE hModuleSnap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, GetCurrentProcessId());
    if (hModuleSnap == INVALID_HANDLE_VALUE) {
        return;
    }

    MODULEENTRY32 me32;
    me32.dwSize = sizeof me32;

    if (VERBOSITY > 0) {
        static bool first = true;
        if (first) {
            if (Module32First(hModuleSnap, &me32)) {
                debugPrintf("  modules:\n");
                do  {
                    debugPrintf("     %s\n", me32.szExePath);
                } while (Module32Next(hModuleSnap, &me32));
            }
            first = false;
        }
    }

    if (Module32First(hModuleSnap, &me32)) {
        do  {
            hookModule(me32.hModule, me32.szExePath);
        } while (Module32Next(hModuleSnap, &me32));
    }

    CloseHandle(hModuleSnap);
}




static HMODULE WINAPI
MyLoadLibrary(LPCSTR lpLibFileName, HANDLE hFile = NULL, DWORD dwFlags = 0)
{
    // To Send the information to the server informing that,
    // LoadLibrary is invoked.
    HMODULE hModule = LoadLibraryExA(lpLibFileName, hFile, dwFlags);

    // Hook all new modules (and not just this one, to pick up any dependencies)
    hookAllModules();

    return hModule;
}

static HMODULE WINAPI
MyLoadLibraryA(LPCSTR lpLibFileName)
{
    if (VERBOSITY >= 2) {
        debugPrintf("%s(\"%s\")\n", __FUNCTION__, lpLibFileName);
    }

    if (VERBOSITY > 0) {
        const char *szBaseName = getBaseName(lpLibFileName);
        for (unsigned i = 0; i < numReplacements; ++i) {
            if (stricmp(szBaseName, replacements[i].szMatchModule) == 0) {
                debugPrintf("%s(\"%s\")\n", __FUNCTION__, lpLibFileName);
#ifdef __GNUC__
                void *caller = __builtin_return_address (0);

                HMODULE hModule = 0;
                BOOL bRet = GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,
                                         (LPCTSTR)caller,
                                         &hModule);
                assert(bRet);
                char szCaller[256];
                DWORD dwRet = GetModuleFileNameA(hModule, szCaller, sizeof szCaller);
                assert(dwRet);
                debugPrintf("  called from %s\n", szCaller);
#endif
                break;
            }
        }
    }

    return MyLoadLibrary(lpLibFileName);
}

static HMODULE WINAPI
MyLoadLibraryW(LPCWSTR lpLibFileName)
{
    if (VERBOSITY >= 2) {
        debugPrintf("%s(L\"%S\")\n", __FUNCTION__, lpLibFileName);
    }

    char szFileName[256];
    wcstombs(szFileName, lpLibFileName, sizeof szFileName);

    return MyLoadLibrary(szFileName);
}

static HMODULE WINAPI
MyLoadLibraryExA(LPCSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    if (VERBOSITY >= 2) {
        debugPrintf("%s(\"%s\")\n", __FUNCTION__, lpLibFileName);
    }
    return MyLoadLibrary(lpLibFileName, hFile, dwFlags);
}

static HMODULE WINAPI
MyLoadLibraryExW(LPCWSTR lpLibFileName, HANDLE hFile, DWORD dwFlags)
{
    if (VERBOSITY >= 2) {
        debugPrintf("%s(L\"%S\")\n", __FUNCTION__, lpLibFileName);
    }

    char szFileName[256];
    wcstombs(szFileName, lpLibFileName, sizeof szFileName);

    return MyLoadLibrary(szFileName, hFile, dwFlags);
}

static FARPROC WINAPI
MyGetProcAddress(HMODULE hModule, LPCSTR lpProcName) {

    if (VERBOSITY >= 99) {
        /* XXX this can cause segmentation faults */
        debugPrintf("%s(\"%s\")\n", __FUNCTION__, lpProcName);
    }

    assert(hModule != g_hThisModule);
    for (unsigned i = 0; i < numReplacements; ++i) {
        if (hModule == replacements[i].hReplaceModule) {
            if (pSharedMem) {
                pSharedMem->bReplaced = TRUE;
            }
            return GetProcAddress(hModule, lpProcName);
        }
    }

    if (!NOOP) {
        char szModule[256];
        DWORD dwRet = GetModuleFileNameA(hModule, szModule, sizeof szModule);
        assert(dwRet);
        const char *szBaseName = getBaseName(szModule);

        for (unsigned i = 0; i < numReplacements; ++i) {

            if (stricmp(szBaseName, replacements[i].szMatchModule) == 0) {
                if (VERBOSITY > 0) {
                    debugPrintf("  %s(\"%s\", \"%s\")\n", __FUNCTION__, szModule, lpProcName);
                }
                FARPROC pProcAddress = GetProcAddress(replacements[i].hReplaceModule, lpProcName);
                if (pProcAddress) {
                    if (VERBOSITY >= 2) {
                        debugPrintf("      replacing %s!%s\n", szBaseName, lpProcName);
                    }
                    if (pSharedMem) {
                        pSharedMem->bReplaced = TRUE;
                    }
                    return pProcAddress;
                } else {
                    if (VERBOSITY > 0) {
                        debugPrintf("      ignoring %s!%s\n", szBaseName, lpProcName);
                    }
                    break;
                }
            }
        }
    }

    return GetProcAddress(hModule, lpProcName);
}


EXTERN_C BOOL WINAPI
DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpReserved)
{
    const char *szNewDllName = NULL;
    HMODULE hNewModule = NULL;
    const char *szNewDllBaseName;

    switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
        if (VERBOSITY > 0) {
            debugPrintf("DLL_PROCESS_ATTACH\n");
        }

        g_hThisModule = hinstDLL;

        {
            char szProcess[MAX_PATH];
            GetModuleFileNameA(NULL, szProcess, sizeof szProcess);
            if (VERBOSITY > 0) {
                debugPrintf("  attached to %s\n", szProcess);
            }
        }

        /*
         * Calling LoadLibrary inside DllMain is strongly discouraged.  But it
         * works quite well, provided that the loaded DLL does not require or do
         * anything special in its DllMain, which seems to be the general case.
         *
         * See also:
         * - http://stackoverflow.com/questions/4370812/calling-loadlibrary-from-dllmain
         * - http://msdn.microsoft.com/en-us/library/ms682583
         */

        if (!USE_SHARED_MEM) {
            szNewDllName = getenv("INJECT_DLL");
            if (!szNewDllName) {
                debugPrintf("warning: INJECT_DLL not set\n");
                return FALSE;
            }
        } else {
            static char szSharedMemCopy[MAX_PATH];
            GetSharedMem(szSharedMemCopy, sizeof szSharedMemCopy);
            szNewDllName = szSharedMemCopy;
        }

        if (VERBOSITY > 0) {
            debugPrintf("  injecting %s\n", szNewDllName);
        }

        hNewModule = LoadLibraryA(szNewDllName);
        if (!hNewModule) {
            debugPrintf("warning: failed to load %s\n", szNewDllName);
            return FALSE;
        }

        szNewDllBaseName = getBaseName(szNewDllName);
        if (stricmp(szNewDllBaseName, "dxgitrace.dll") == 0) {
            replacements[numReplacements].szMatchModule = "dxgi.dll";
            replacements[numReplacements].hReplaceModule = hNewModule;
            ++numReplacements;

            replacements[numReplacements].szMatchModule = "d3d10.dll";
            replacements[numReplacements].hReplaceModule = hNewModule;
            ++numReplacements;

            replacements[numReplacements].szMatchModule = "d3d10_1.dll";
            replacements[numReplacements].hReplaceModule = hNewModule;
            ++numReplacements;

            replacements[numReplacements].szMatchModule = "d3d11.dll";
            replacements[numReplacements].hReplaceModule = hNewModule;
            ++numReplacements;
        } else {
            replacements[numReplacements].szMatchModule = szNewDllBaseName;
            replacements[numReplacements].hReplaceModule = hNewModule;
            ++numReplacements;
        }

        hookAllModules();
        break;

    case DLL_THREAD_ATTACH:
        break;

    case DLL_THREAD_DETACH:
        break;

    case DLL_PROCESS_DETACH:
        if (VERBOSITY > 0) {
            debugPrintf("DLL_PROCESS_DETACH\n");
        }
        break;
    }
    return TRUE;
}
