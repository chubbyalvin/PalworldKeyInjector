-- Put this file, palworld_keyinjector.lua, and PalworldKeyInjector.dll in the
-- UE4SS mod's Scripts directory.
local PalworldKeyInjector = require("palworld_keyinjector")
local injector, loadError = PalworldKeyInjector.new()

if injector == nil then
    print("[KeyInjectorExample] DLL load failed: " .. tostring(loadError) .. "\n")
    return
end

RegisterKeyBind(Key.F8, function()
    local ok, detail = injector:inject("CTRL+SHIFT+F10")
    if ok then
        print("[KeyInjectorExample] Called " .. tostring(detail) .. "\n")
    else
        print("[KeyInjectorExample] Rejected: " .. tostring(detail) .. "\n")
    end
end)

print("[KeyInjectorExample] Ready; press F8 to call CTRL+SHIFT+F10\n")
