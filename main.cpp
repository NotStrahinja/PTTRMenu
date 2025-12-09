#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include "kiero.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"

#include "MinHook.h"

#define SET_HEALTH_RVA 0x72B2C0 // SetHealth() function
#define UPDATE_RVA 0x71F8B0
#define SET_SPECIAL_RVA 0x72BC50
#define GET_KICK_COOLDOWN_RVA 0x719050
#define SET_KICK_COOLDOWN_RVA 0x7190C0
#define DISABLE_CONTROLS_RVA 0x4717F0
#define ENABLE_CONTROLS_RVA 0x471770
#define CURSOR_VISIBLE_SET_RVA 0x1256F00
#define CURSOR_LOCK_SET_RVA 0x1256FA0

struct PTTRPlayer {};

typedef void(__fastcall* PTTRPlayer_Update_t)(void* self, void* methodInfo);
PTTRPlayer_Update_t oUpdate = nullptr;
PTTRPlayer* gPlayerInstance = nullptr;

bool godMode = false;
bool infiniteSpecial = false;
bool enableCooldown = false;

typedef void(*SetHealthFn)(PTTRPlayer* __this, float hp, bool lowering, void* method);
SetHealthFn SetHealth = nullptr;

typedef void(*SetSpecialFn)(PTTRPlayer* __this, float setSpecialTo, void* method);
SetSpecialFn SetSpecial = nullptr;

typedef float (*GetKickCooldownFn)(PTTRPlayer* self, const void* method);
typedef void (*SetKickCooldownFn)(PTTRPlayer* self, float value, const void* method);
GetKickCooldownFn GetKickCooldown = nullptr;
SetKickCooldownFn SetKickCooldown = nullptr;

typedef void (*DisableGameplayControlsFn)();
DisableGameplayControlsFn oDisableGameplayControls = nullptr;
typedef void (*EnableGameplayControlsFn)();
EnableGameplayControlsFn oEnableGameplayControls = nullptr;

typedef void(*tSetVisible)(bool value, void* method);
tSetVisible oSetVisible;
typedef void(*tSetLockState)(int state, void* method);
tSetLockState oSetLockState;

bool show_menu = true;

void __fastcall hkUpdate(PTTRPlayer* self, void* methodInfo)
{
    if(!gPlayerInstance)
        gPlayerInstance = self;

    if(godMode)
        SetHealth(self, 9999.f, false, nullptr);

    if(infiniteSpecial)
        SetSpecial(self, 9999.f, nullptr);

    if(enableCooldown)
        SetKickCooldown(self, 1.0f, nullptr);

    oUpdate(self, methodInfo);
}

#include <d3d11.h>

HINSTANCE dll_handle;

bool show_canvas = false;
float speed = 0.5f;
bool is_ejecting = false;

void hkDisableGameplayControls()
{
    if(show_menu)
        return;
    oDisableGameplayControls();
}

void hkEnableGameplayControls()
{
    if(!show_menu)
        return;
    oEnableGameplayControls();
}

typedef HRESULT(__stdcall* Present)(IDXGISwapChain*, UINT, UINT);
Present oPresent = nullptr;

WNDPROC oWndProc;
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT __stdcall WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if(show_menu)
    {
        if(ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return 1;
        if(uMsg >= WM_KEYFIRST && uMsg <= WM_KEYLAST)
            return 1;
        if(uMsg >= WM_MOUSEFIRST && uMsg <= WM_MOUSELAST)
            return 1;
    }
    return CallWindowProcA(oWndProc, hWnd, uMsg, wParam, lParam);
}

HWND gHwnd = nullptr;

HRESULT __stdcall detour_present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
    if(is_ejecting)
    {
        return oPresent ? oPresent(pSwapChain, SyncInterval, Flags) : 0;
    }

    static bool init = false;
    static ID3D11Device* pDevice = nullptr;
    static ID3D11DeviceContext* pContext = nullptr;

    HMODULE base = GetModuleHandleA("GameAssembly.dll");
    
    if(!init)
    {
        if(SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)&pDevice)))
        {
            pDevice->GetImmediateContext(&pContext);

            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            gHwnd = sd.OutputWindow;

            oWndProc = (WNDPROC)SetWindowLongPtr(gHwnd, GWLP_WNDPROC, (LONG_PTR)WndProc);

            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
            
            io.WantCaptureKeyboard = true;
            io.WantCaptureMouse = true;
            
            ImGui::StyleColorsDark();
            
            ImGui_ImplWin32_Init(gHwnd);
            ImGui_ImplDX11_Init(pDevice, pContext);

            init = true;
        }

        SetHealth = (SetHealthFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + SET_HEALTH_RVA);
        SetSpecial = (SetSpecialFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + SET_SPECIAL_RVA);
        GetKickCooldown = (GetKickCooldownFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + GET_KICK_COOLDOWN_RVA);
        SetKickCooldown = (SetKickCooldownFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + SET_KICK_COOLDOWN_RVA);
    }

    if(init)
    {
        ID3D11Texture2D* pBackBuffer = nullptr;
        ID3D11RenderTargetView* mainRTV = nullptr;
        
        if(SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuffer)))
        {
            pDevice->CreateRenderTargetView(pBackBuffer, nullptr, &mainRTV);
            pBackBuffer->Release();

            ImGui_ImplDX11_NewFrame();
            ImGui_ImplWin32_NewFrame();
            ImGui::NewFrame();

            ImGuiIO& io = ImGui::GetIO();
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

            if(show_menu)
            {
                io.WantCaptureKeyboard = true;
                io.WantCaptureMouse = true;
                ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
                ImGui::Begin("Cheat Menu", nullptr, ImGuiWindowFlags_None);
                ImGui::Text("Made by Strahinja");
                ImGui::Separator();
                ImGui::Checkbox("Godmode", &godMode);
                ImGui::Checkbox("Unlimited Special", &infiniteSpecial);
                ImGui::Checkbox("Unlimited Kick", &enableCooldown);
                ImGui::Separator();
                ImGui::Text("Found localPlayer at %p", gPlayerInstance);
                ImGui::Text("Press F3 to toggle menu");
                ImGui::Text("Press F4 to exit menu");
                ImGui::End();
            }
            else
            {
                io.WantCaptureKeyboard = false;
                io.WantCaptureMouse = false;
            }

            ImGui::EndFrame();
            ImGui::Render();
            
            pContext->OMSetRenderTargets(1, &mainRTV, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            
            if(mainRTV) mainRTV->Release();
        }
    }

    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpParameter)
{
    while(kiero::init(kiero::RenderType::D3D11) != kiero::Status::Success)
        Sleep(50);

    kiero::bind(8, (void**)&oPresent, (void*)detour_present);

    MH_Initialize();

    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    uintptr_t updateAddr = base + UPDATE_RVA;

    MH_CreateHook((LPVOID)updateAddr, (void*)&hkUpdate, reinterpret_cast<LPVOID*>(&oUpdate));
    MH_EnableHook((LPVOID)updateAddr);

    uintptr_t disableGCAddr = base + DISABLE_CONTROLS_RVA;
    uintptr_t enableGCAddr = base + ENABLE_CONTROLS_RVA;
    uintptr_t setVisibleAddr = base + CURSOR_VISIBLE_SET_RVA;
    uintptr_t setLockAddr = base + CURSOR_LOCK_SET_RVA;

    MH_CreateHook((LPVOID)disableGCAddr, (void*)&hkDisableGameplayControls, reinterpret_cast<LPVOID*>(&oDisableGameplayControls));
    MH_EnableHook((LPVOID)disableGCAddr);

    MH_CreateHook((LPVOID)enableGCAddr, (void*)&hkEnableGameplayControls, reinterpret_cast<LPVOID*>(&oEnableGameplayControls));
    MH_EnableHook((LPVOID)enableGCAddr);

    while(true)
    {
        Sleep(50);
        if(GetAsyncKeyState(VK_F3) & 0x01)
        {
            show_menu = !show_menu;

            if(show_menu)
                oDisableGameplayControls();
            else oEnableGameplayControls();
        }
        if(GetAsyncKeyState(VK_F4) & 0x01) break;
    }

    is_ejecting = true;
    show_menu = false;

    kiero::unbind(8);

    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if(oWndProc && gHwnd)
    {
        SetWindowLongPtrA(gHwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(oWndProc));
        oWndProc = nullptr;
        gHwnd = nullptr;
    }

    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    
    FreeLibraryAndExitThread(dll_handle, 0);
    return 0;
}

BOOL APIENTRY DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{
    if(dwReason == DLL_PROCESS_ATTACH)
    {
        DisableThreadLibraryCalls(hModule);
        dll_handle = hModule;
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
    }
    return TRUE;
}
