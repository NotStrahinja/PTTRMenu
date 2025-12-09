g++ -o main.dll main.cpp kiero.cpp imgui.cpp imgui_demo.cpp imgui_draw.cpp imgui_widgets.cpp imgui_tables.cpp imgui_impl_dx11.cpp imgui_impl_win32.cpp -lMinHook -luser32 -lgdi32 -ldwmapi -ld3d11 -ld3dcompiler -shared -fpermissive -s
gcc -o inject inject.c -s
