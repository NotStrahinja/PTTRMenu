#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>

DWORD GetPIDByName(const char *exeName)
{
    HANDLE hSnapshot;
    PROCESSENTRY32 pe32;
    DWORD pid = 0;

    hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if(hSnapshot == INVALID_HANDLE_VALUE)
        return 0;

    pe32.dwSize = sizeof(PROCESSENTRY32);

    if(Process32First(hSnapshot, &pe32))
    {
        do {
            if(_stricmp(pe32.szExeFile, exeName) == 0)
            {
                pid = pe32.th32ProcessID;
                break;
            }
        } while(Process32Next(hSnapshot, &pe32));
    }

    CloseHandle(hSnapshot);
    return pid;
}

int main(int argc, char **argv)
{
    if(argc < 2)
    {
        printf("Usage: %s <exe_name> <cheat_dll>\n\n", argv[0]);
        return 1;
    }
    const char *name = argv[1];
    DWORD pid = GetPIDByName(name);
    char dllPath[MAX_PATH];

    GetFullPathNameA(argv[2], MAX_PATH, dllPath, NULL);

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    LPVOID pLibRemote = VirtualAllocEx(hProc, NULL, strlen(dllPath) + 1, MEM_COMMIT, PAGE_READWRITE);
    WriteProcessMemory(hProc, pLibRemote, dllPath, strlen(dllPath) + 1, NULL);
    HANDLE hThread = CreateRemoteThread(hProc, NULL, 0, (LPTHREAD_START_ROUTINE)LoadLibraryA, pLibRemote, 0, NULL);

    WaitForSingleObject(hThread, INFINITE);

    VirtualFreeEx(hProc, pLibRemote, 0, MEM_RELEASE);
    CloseHandle(hThread);
    CloseHandle(hProc);
    return 0;
}
