// 在这个文件(dll)中,作用是DisablePasswordVerificationOfRrrx,只要注入
// 就算起作用 所以也不写太多的逻辑了
#include <Windows.h>
#include <TlHelp32.h>
#include<DbgHelp.h>
#pragma comment(lib,"Dbghelp.lib")

#define EXIT_CODE 1111
#pragma comment(lib,"Kernel32.lib")

typedef
void (* OriginalShdTraceType)(unsigned long, unsigned long, wchar_t const* function_name, wchar_t const*, unsigned long, wchar_t const*, ...);
OriginalShdTraceType OriginalShdTrace = nullptr;

void MyShdTrace
(
    unsigned long, unsigned long, wchar_t const* function_name, wchar_t const*, unsigned long, wchar_t const*, ...
);

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    if (ul_reason_for_call != DLL_PROCESS_ATTACH)
        return TRUE;

    //只在此实行挂钩
    HMODULE  hShdPub =
        GetModuleHandle(L"ShdPub.dll");
    if (hShdPub == nullptr)
        ExitProcess(EXIT_CODE);

    //对这个值 在此初始化
    void* addrOfShdServ = nullptr;
    HANDLE hSnapShot =
        CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());
    if (hSnapShot == INVALID_HANDLE_VALUE)
        ExitProcess(EXIT_CODE);
    MODULEENTRY32 me{ sizeof(MODULEENTRY32)};
    if (!Module32First(hSnapShot, &me))
        ExitProcess(EXIT_CODE);
    do
    {
        if (wcscmp(L"shdserv.exe", me.szModule) == 0)
            addrOfShdServ = me.modBaseAddr;

    } while (Module32Next(hSnapShot, &me) && GetLastError() != ERROR_NO_MORE_FILES);

    if (addrOfShdServ == nullptr)
        ExitProcess(EXIT_CODE);
    CloseHandle(hSnapShot);

    void* FuncAddrOfShdTrace =
        GetProcAddress(hShdPub,"?ShdTrace@@YAXKKPEB_W0K0ZZ");
    if (FuncAddrOfShdTrace == nullptr)
        ExitProcess(EXIT_CODE);

    PIMAGE_SECTION_HEADER Section_Header_Table = nullptr;
    ULONG Size_Section_Header_Table = 0;
    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = nullptr;

    pImportDesc = static_cast<PIMAGE_IMPORT_DESCRIPTOR>(ImageDirectoryEntryToDataEx(addrOfShdServ, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT,
        &Size_Section_Header_Table, &Section_Header_Table));
    if (pImportDesc == nullptr)
        ExitProcess(EXIT_CODE);

    PIMAGE_THUNK_DATA pIat = 
    reinterpret_cast<PIMAGE_THUNK_DATA>((ULONGLONG)addrOfShdServ+ pImportDesc->FirstThunk );
    for ( ; ; pIat++)     //这里应该用具体的数来限制 但是我不想写了 反正iat一般在最后 访问过界就会直接崩了呗
    {
        if (pIat->u1.Function == (ULONGLONG)FuncAddrOfShdTrace)
            break;
    }
    
    if (pIat->u1.Function == (ULONGLONG)nullptr)
        ExitProcess(EXIT_CODE);
    OriginalShdTrace = (OriginalShdTraceType)pIat->u1.Function;

    DWORD oldProtect = 0;
    if(!VirtualProtect(&pIat->u1.Function, sizeof(pIat->u1.Function), PAGE_EXECUTE_READWRITE, &oldProtect))
        ExitProcess(EXIT_CODE);
    pIat->u1.Function = (ULONGLONG)MyShdTrace;

    return TRUE;
}

//直接挂这个函数好了
void MyShdTrace
(
    unsigned long,
    unsigned long,
    wchar_t const* function_name,
    wchar_t const*,
    unsigned long,
    wchar_t const*, ...
)
{
     
    if (wcscmp(function_name, L"CShdUser::LogonUserW") && wcscmp(function_name, L"CShdUser::HasUserRight"))
        return;
    //开始施法

    DWORD64 CaptruedFreams[3]{ 0 };
    RtlCaptureStackBackTrace(0, 3, (PVOID*)CaptruedFreams, nullptr);

    DWORD oldProtect = 0;
    if (!VirtualProtect((LPVOID)CaptruedFreams[1], 5, PAGE_EXECUTE_READWRITE, &oldProtect))
        ExitProcess(EXIT_CODE);

    auto currentRip = (BYTE*)CaptruedFreams[1];
    for (; *currentRip != 0xB8; currentRip++)
        ;
    currentRip++;

    if (!VirtualProtect((LPVOID)currentRip, 4, PAGE_EXECUTE_READWRITE, &oldProtect))
        ExitProcess(EXIT_CODE);
    *(DWORD*)currentRip = 0;

    return;
}