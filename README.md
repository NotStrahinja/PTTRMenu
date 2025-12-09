# What is it?
It's a simple mod menu for Paint The Town Red.

# How does it work?
It works by getting the player instance, hooking the Update() function, and hooking other values like health, kick cooldown, etc.
The DX11 hook was implemented with Kiero, and the menu was made with ImGui.

# How did you make this?
I made it using IL2CPP to decompile the GameAssembly.dll from the game's root folder, along with dnSpy to get all of the RVAs of the functions and fields.

# Can it work in online?
Yes.

# Features
- [x] Godmode
- [x] Infinite Ability
- [x] Infinite Kick
- [ ] Unlimited Ammo
- [ ] Unlimited Weapon Durability

# How to compile
Just run the build.bat file provided. It will build both the menu DLL and the injector.

# How to use
You need an injector and the main menu DLL file. You can either use a third party DLL injector, or my own.
1. Run the game
2. Run: `inject.exe PaintTheTownRed.exe main.dll`
3. Done
Now you can use F3 to toggle the menu, and F4 to eject it (aka unload the DLL).
