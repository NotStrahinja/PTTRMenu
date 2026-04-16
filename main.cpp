#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <unordered_set>
#include <algorithm>
#include "kiero.h"

#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include "imgui_internal.h"

#include "MinHook.h"

#include "globals.h"
#include "hooks.h"

Camera* mainCam;

std::unordered_set<Enemy*> enemies;

PTTRPlayer_Update_t oUpdate = nullptr;
PTTRPlayer* gPlayerInstance = nullptr;

Enemy_DoUpdate_t oEnemyDoUpdate = nullptr;

bool show_menu = true;

HWND gHwnd = nullptr;

Vector3 screen;

void SetMenuCursorState(bool menuOpen);

void __fastcall hook::Update(PTTRPlayer* self, void* methodInfo)
{
    if(!gPlayerInstance)
        gPlayerInstance = self;

    if(hook::HasWeapon(self, nullptr))
    {
        hook::weapon = hook::CurrentWeapon(self, nullptr);
    }
    else
    {
        hook::weapon = nullptr;
    }

    if(hook::weapon && global::unbreakable)
    {
        bool* unbreakablePtr = (bool*)((uintptr_t)hook::weapon + offset::UNBREAKABLE_FIELD_OFFSET);
        *unbreakablePtr = true;
    }

    if(global::godMode)
        hook::SetHealth(self, 9999.f, false, nullptr);

    if(global::infiniteSpecial)
        hook::SetSpecial(self, 9999.f, nullptr);

    if(global::enableCooldown)
        hook::SetKickCooldown(self, 1.0f, nullptr);

    oUpdate(self, methodInfo);

    SetMenuCursorState(show_menu);
}

void __fastcall hook::EnemyDoUpdate(Enemy* self, float deltaTime, void* method)
{
    enemies.insert(self);

    oEnemyDoUpdate(self, deltaTime, method);
}

HINSTANCE dll_handle;

bool show_canvas = false;
float speed = 0.5f;
bool is_ejecting = false;

void hook::DisableGameplayControls()
{
    if(show_menu)
        return;
    hook::oDisableGameplayControls();
}

void hook::EnableGameplayControls()
{
    if(!show_menu)
        return;
    hook::oEnableGameplayControls();
}

bool lastMenuState = false;

void SetMenuCursorState(bool menuOpen)
{
    if(gHwnd)
    {
        if(menuOpen)
        {
            RECT rect;
            GetClientRect(gHwnd, &rect);
            MapWindowPoints(gHwnd, nullptr, (POINT*)&rect, 2);
            ClipCursor(&rect);
        }
        else
        {
            ClipCursor(nullptr);
        }
    }

    if(menuOpen != lastMenuState)
    {
        if(menuOpen)
        {
            while(ShowCursor(true) < 0);
        }
        else
        {
            ShowCursor(false);
        }
        lastMenuState = menuOpen;
    }
}

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

void AddOutlinedText(ImDrawList* draw,
                     ImVec2 pos,
                     ImU32 text_color,
                     ImU32 outline_color,
                     const char* text,
                     float thickness = 1.0f)
{
    for(int dx = -1; dx <= 1; dx++)
    {
        for(int dy = -1; dy <= 1; dy++)
        {
            if(dx == 0 && dy == 0)
                continue;

            ImVec2 offset(pos.x + dx * thickness, pos.y + dy * thickness);

            draw->AddText(offset, outline_color, text);
        }
    }

    draw->AddText(pos, text_color, text);
}

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
            
            io.WantCaptureKeyboard = true;
            io.WantCaptureMouse = true;
            
            ImGui::StyleColorsDark();

            ImGui_ImplWin32_Init(gHwnd);
            ImGui_ImplDX11_Init(pDevice, pContext);

            init = true;
        }

        hook::SetHealth = (SetHealthFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::SET_HEALTH_RVA);
        hook::SetSpecial = (SetSpecialFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::SET_SPECIAL_RVA);
        hook::GetKickCooldown = (GetKickCooldownFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::GET_KICK_COOLDOWN_RVA);
        hook::SetKickCooldown = (SetKickCooldownFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::SET_KICK_COOLDOWN_RVA);
        hook::CurrentWeapon = (CurrentWeaponFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::CURRENT_WEAPON_RVA);
        hook::HasWeapon = (HasWeaponFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::HAS_WEAPON_RVA);
        hook::IsLocalPlayer = (IsLocalPlayerFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::IS_LOCAL_PLAYER_RVA);
        hook::Exists = (ExistsFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::EXISTS_RVA);
        hook::GetTransform = (GetTransformFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::GET_TRANSFORM_RVA);
        hook::WorldToScreenPoint = (WorldToScreenPointFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::WTSP_RVA);
        hook::GetMain = (GetMainFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::GET_MAIN_RVA);
        hook::GetPosition = (GetPositionFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::GET_POSITION_RVA);
        hook::GetHealthPct = (GetHealthPctFn)((uintptr_t)GetModuleHandleA("GameAssembly.dll") + offset::GET_HEALTH_PCT_RVA);
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
            // ImGuiStyle *style = &ImGui::GetStyle();
            // style->WindowPadding = ImVec2(15, 15);
            // style->WindowRounding = 5.0f;
            // style->FramePadding = ImVec2(5, 5);
            // style->FrameRounding = 4.0f;
            // style->ItemSpacing = ImVec2(12, 8);
            // style->ItemInnerSpacing = ImVec2(8, 6);
            // style->IndentSpacing = 25.0f;
            // style->ScrollbarSize = 15.0f;
            // style->ScrollbarRounding = 9.0f;
            // style->GrabMinSize = 5.0f;
            // style->GrabRounding = 3.0f;
            //
            // style->Colors[ImGuiCol_Text] = ImVec4(0.80f, 0.80f, 0.83f, 1.00f);
            // style->Colors[ImGuiCol_TextDisabled] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
            // style->Colors[ImGuiCol_WindowBg] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
            // style->Colors[ImGuiCol_PopupBg] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
            // style->Colors[ImGuiCol_Border] = ImVec4(0.80f, 0.80f, 0.83f, 0.88f);
            // style->Colors[ImGuiCol_BorderShadow] = ImVec4(0.92f, 0.91f, 0.88f, 0.00f);
            // style->Colors[ImGuiCol_FrameBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
            // style->Colors[ImGuiCol_FrameBgHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
            // style->Colors[ImGuiCol_FrameBgActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
            // style->Colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
            // style->Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(1.00f, 0.98f, 0.95f, 0.75f);
            // style->Colors[ImGuiCol_TitleBgActive] = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
            // style->Colors[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
            // style->Colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
            // style->Colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
            // style->Colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
            // style->Colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
            // style->Colors[ImGuiCol_CheckMark] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
            // style->Colors[ImGuiCol_SliderGrab] = ImVec4(0.80f, 0.80f, 0.83f, 0.31f);
            // style->Colors[ImGuiCol_SliderGrabActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
            // style->Colors[ImGuiCol_Button] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
            // style->Colors[ImGuiCol_ButtonHovered] = ImVec4(0.24f, 0.23f, 0.29f, 1.00f);
            // style->Colors[ImGuiCol_ButtonActive] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
            // style->Colors[ImGuiCol_Header] = ImVec4(0.10f, 0.09f, 0.12f, 1.00f);
            // style->Colors[ImGuiCol_HeaderHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
            // style->Colors[ImGuiCol_HeaderActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
            // style->Colors[ImGuiCol_ResizeGrip] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
            // style->Colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.56f, 0.56f, 0.58f, 1.00f);
            // style->Colors[ImGuiCol_ResizeGripActive] = ImVec4(0.06f, 0.05f, 0.07f, 1.00f);
            // style->Colors[ImGuiCol_PlotLines] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
            // style->Colors[ImGuiCol_PlotLinesHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
            // style->Colors[ImGuiCol_PlotHistogram] = ImVec4(0.40f, 0.39f, 0.38f, 0.63f);
            // style->Colors[ImGuiCol_PlotHistogramHovered] = ImVec4(0.25f, 1.00f, 0.00f, 1.00f);
            // style->Colors[ImGuiCol_TextSelectedBg] = ImVec4(0.25f, 1.00f, 0.00f, 0.43f);
            // style->Colors[ImGuiCol_ModalWindowDimBg] = ImVec4(1.00f, 0.98f, 0.95f, 0.73f);

            static int hitbox_type_i = 0;
            const char *types[] = {"Hitbox", "Hitbox + Health bar", "Hitbox (health color)"};
            static bool show_hp_pct = false;
            static bool show_enemy_count = false;

            if(show_menu)
            {
                io.WantCaptureKeyboard = true;
                io.WantCaptureMouse = true;
                ImGui::SetNextWindowPos(ImVec2(50, 50), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(300, 200), ImGuiCond_FirstUseEver);
                ImGui::Begin("PTTREasy", &show_menu);
                ImGui::BeginTabBar("tabs");
                if(ImGui::BeginTabItem("Cheats"))
                {
                    ImGui::Checkbox("Godmode", &global::godMode);
                    ImGui::Checkbox("Unlimited Special", &global::infiniteSpecial);
                    ImGui::Checkbox("Unlimited Kick", &global::enableCooldown);
                    ImGui::Checkbox("Unbreakable Weapon", &global::unbreakable);
                    ImGui::Checkbox("ESP", &global::esp);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Config"))
                {
                    ImGui::ColorEdit4("ESP Box Color", global::box_col);
                    ImGui::Combo("Hitbox Type", &hitbox_type_i, types, IM_ARRAYSIZE(types));
                    ImGui::Checkbox("Show HP%", &show_hp_pct);
                    ImGui::Checkbox("Show Enemy Count", &show_enemy_count);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Debug"))
                {
                    if(gPlayerInstance)
                    {
                        ImGui::Text("Status: Found localPlayer at %p", gPlayerInstance);
                    }
                    else
                    {
                        ImGui::Text("Status: Not in-game");
                        global::godMode = false;
                        global::infiniteSpecial = false;
                        global::enableCooldown = false;
                        global::esp = false;
                        global::unbreakable = false;
                    }
                    ImGui::Text("Camera.main: %p", mainCam);
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("About"))
                {
                    ImGui::Text("Made by Strahinja (c) 2026");
                    ImGui::Spacing();
                    ImGui::Text("Version: v2.3");
                    ImGui::EndTabItem();
                }
                if(ImGui::BeginTabItem("Help"))
                {
                    ImGui::Text("Press F3 to toggle menu");
                    ImGui::Text("Press F4 to unload menu");
                    ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
                ImGui::End();
            }
            else
            {
                io.WantCaptureKeyboard = false;
                io.WantCaptureMouse = false;
            }

            auto draw = ImGui::GetBackgroundDrawList();

            if(show_enemy_count)
            {
                char count_fmt[32];
                sprintf(count_fmt, "Enemies left: %llu", enemies.size());
                AddOutlinedText(draw, ImVec2(100, 100), IM_COL32(0, 255, 0, 255), IM_COL32(0, 0, 0, 255), count_fmt);
            }

            mainCam = hook::GetMain(nullptr);
            if(global::esp && mainCam)
            {
                for(auto enemy : enemies)
                {
                    float hp = hook::GetHealthPct(enemy, nullptr);
                    hp = std::clamp(hp, 0.0f, 1.0f);

                    Transform* transform = hook::GetTransform(enemy, nullptr);
                    if(!transform) continue;

                    Vector3 world = hook::GetPosition(transform, nullptr);

                    Vector3 feet = world;
                    Vector3 head = world;
                    head.y += 2.f;

                    Vector3 feetScreen = hook::WorldToScreenPoint(mainCam, feet, nullptr);
                    Vector3 headScreen = hook::WorldToScreenPoint(mainCam, head, nullptr);

                    feetScreen.y = 1080 - feetScreen.y;
                    headScreen.y = 1080 - headScreen.y;

                    if(feetScreen.z > 0 && headScreen.z > 0)
                    {
                        float height = feetScreen.y - headScreen.y;
                        float width = height * 0.5f;
                        float healthHeight = height * hp;

                        int r = (int)((1.0f - hp) * 255);
                        int g = (int)(hp * 255);
                        ImU32 healthColor = IM_COL32(r, g, 0, 255);
                        float barX = headScreen.x - width/2 - 6;

                        if(hitbox_type_i == HitboxType::HITBOX_HEALTH_BAR)
                        {
                            draw->AddRectFilled(
                                    ImVec2(barX, headScreen.y),
                                    ImVec2(barX + 4, feetScreen.y),
                                    IM_COL32(0,0,0,180)
                                    );
                            draw->AddRectFilled(
                                    ImVec2(barX, feetScreen.y - healthHeight),
                                    ImVec2(barX + 4, feetScreen.y),
                                    healthColor
                                    );
                            draw->AddRect(
                                    ImVec2(barX, headScreen.y),
                                    ImVec2(barX + 4, feetScreen.y),
                                    IM_COL32(0,0,0,255)
                                    );

                            ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                                        global::box_col[0],
                                        global::box_col[1],
                                        global::box_col[2],
                                        global::box_col[3]
                                        ));

                            draw->AddRect(
                                    ImVec2(headScreen.x - width/2, headScreen.y),
                                    ImVec2(headScreen.x + width/2, feetScreen.y),
                                    col
                                    );
                        }
                        if(hitbox_type_i == HitboxType::HITBOX_NORMAL)
                        {
                            ImU32 col = ImGui::ColorConvertFloat4ToU32(ImVec4(
                                        global::box_col[0],
                                        global::box_col[1],
                                        global::box_col[2],
                                        global::box_col[3]
                                        ));

                            draw->AddRect(
                                    ImVec2(headScreen.x - width/2, headScreen.y),
                                    ImVec2(headScreen.x + width/2, feetScreen.y),
                                    col
                                    );
                        }
                        if(hitbox_type_i == HitboxType::HITBOX_HEALTH_COLOR)
                        {
                            draw->AddRect(
                                    ImVec2(headScreen.x - width/2, headScreen.y),
                                    ImVec2(headScreen.x + width/2, feetScreen.y),
                                    healthColor
                                    );
                        }

                        if(show_hp_pct)
                        {
                            char hp_pct[32];
                            sprintf(hp_pct, "%.0f", hp * 100.0f);
                            AddOutlinedText(draw, ImVec2(headScreen.x - width/2 - 6 - (strlen(hp_pct) * 2), feetScreen.y - healthHeight), IM_COL32(255, 255, 255, 255), IM_COL32(0, 0, 0, 255), hp_pct);
                        }
                    }
                }
            }

            ImGui::EndFrame();
            ImGui::Render();
            
            pContext->OMSetRenderTargets(1, &mainRTV, nullptr);
            ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
            
            if(mainRTV) mainRTV->Release();
        }
    }

    enemies.clear();

    return oPresent(pSwapChain, SyncInterval, Flags);
}

DWORD WINAPI MainThread(LPVOID lpParameter)
{
    while(kiero::init(kiero::RenderType::D3D11) != kiero::Status::Success)
        Sleep(50);

    kiero::bind(8, (void**)&oPresent, (void*)detour_present);

    MH_Initialize();

    uintptr_t base = (uintptr_t)GetModuleHandleA("GameAssembly.dll");
    uintptr_t updateAddr = base + offset::UPDATE_RVA;

    MH_CreateHook((LPVOID)updateAddr, (void*)&hook::Update, reinterpret_cast<LPVOID*>(&oUpdate));
    MH_EnableHook((LPVOID)updateAddr);

    uintptr_t enemyUpdateAddr = base + 0x391710;

    MH_CreateHook(
            (LPVOID)enemyUpdateAddr,
            (void*)&hook::EnemyDoUpdate,
            reinterpret_cast<LPVOID*>(&oEnemyDoUpdate)
            );

    MH_EnableHook((LPVOID)enemyUpdateAddr);

    uintptr_t disableGCAddr = base + offset::DISABLE_CONTROLS_RVA;
    uintptr_t enableGCAddr = base + offset::ENABLE_CONTROLS_RVA;

    MH_CreateHook((LPVOID)disableGCAddr, (void*)&hook::DisableGameplayControls, reinterpret_cast<LPVOID*>(&hook::oDisableGameplayControls));
    MH_EnableHook((LPVOID)disableGCAddr);

    MH_CreateHook((LPVOID)enableGCAddr, (void*)&hook::EnableGameplayControls, reinterpret_cast<LPVOID*>(&hook::oEnableGameplayControls));
    MH_EnableHook((LPVOID)enableGCAddr);

    while(true)
    {
        Sleep(50);
        if(GetAsyncKeyState(VK_F3) & 0x01)
        {
            show_menu = !show_menu;

            if(show_menu)
                hook::oDisableGameplayControls();
            else hook::oEnableGameplayControls();
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
