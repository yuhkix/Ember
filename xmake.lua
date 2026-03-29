set_project("Prophecy")
set_version("1.0.0")
set_languages("c++26", "c23") -- C++26

add_rules("mode.debug", "mode.release")

if is_plat("windows") then
    add_cxflags("/utf-8")
    add_defines("_CRT_SECURE_NO_WARNINGS", "UNICODE", "_UNICODE")
end

target("Prophecy")
    set_kind("binary")
    set_targetdir("$(builddir)/bin")

    add_files("src/*.cpp")

    -- ImGui core
    add_files(
        "vendor/imgui/imgui.cpp",
        "vendor/imgui/imgui_draw.cpp",
        "vendor/imgui/imgui_tables.cpp",
        "vendor/imgui/imgui_widgets.cpp",
        "vendor/imgui/imgui_demo.cpp",
        "vendor/imgui/backends/imgui_impl_dx11.cpp",
        "vendor/imgui/backends/imgui_impl_win32.cpp"
    )
    add_includedirs("vendor/imgui", "vendor/imgui/backends")

    -- LZMA SDK
    add_files(
        "vendor/lzma/C/Alloc.c",
        "vendor/lzma/C/LzmaDec.c",
        "vendor/lzma/C/LzmaEnc.c",
        "vendor/lzma/C/LzFind.c",
        "vendor/lzma/C/LzFindMt.c",
        "vendor/lzma/C/Threads.c",
        "vendor/lzma/C/LzmaLib.c",
        "vendor/lzma/C/CpuArch.c"
    )
    add_includedirs("vendor/lzma/C")
    add_defines("_7ZIP_ST")

    -- Crunch (header-only)
    add_includedirs("vendor/crunch")

    -- Native File Dialog Extended
    add_files("vendor/nfd/src/nfd_win.cpp")
    add_includedirs("vendor/nfd/src/include")

    -- System libraries
    add_syslinks("d3d11", "d3dcompiler", "dxgi", "ole32", "shell32", "user32", "gdi32")

    -- No console window
    if is_plat("windows") then
        add_ldflags("/SUBSYSTEM:WINDOWS", "/ENTRY:mainCRTStartup", {force = true})
    end
