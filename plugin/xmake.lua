-- ScrapStacks F4SE plugin.
--
-- One DLL for every Fallout 4 runtime F4SE supports: hook sites are discovered at
-- startup (src/logic/Discovery.h), via the Dear-Modding-FO4 CommonLibF4 fork.
--
--   xmake f -m releasedbg --deploy_dir="<MO2>/mods/Scrap Stacks"
--   xmake build ScrapStacks
--   xmake run tests

set_xmakever("3.0.0")

set_project("ScrapStacks")
set_version("0.1.0")
set_license("GPL-3.0")

set_languages("c++23")
set_encodings("utf-8")
set_runtimes("MT")

add_rules("mode.debug", "mode.releasedbg")

includes("extern/CommonLibF4")

add_requires("doctest")

option("addrlib_dir")
    set_default("")
    set_showmenu(true)
    set_description("Folder with Address Library version-*.bin files for the offline exe tests (default: ../re/addrlib)")
option_end()

option("deploy_dir")
    set_default("")
    set_showmenu(true)
    set_description("MO2 mod folder to copy the built DLL + INI into (empty = skip)")
option_end()

target("ScrapStacks")
    set_kind("shared")
    set_arch("x64")

    add_deps("commonlibf4")

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

    add_defines("_UNICODE", "COMMONLIB_RUNTIMECOUNT=3")

    add_cxxflags(
        "/sdl",
        "/W4",
        "/GS",
        "/guard:cf",
        "/utf-8",
        "/Zi",
        "/permissive-",
        "/Zc:preprocessor",
        "/Zc:__cplusplus",
        "/Zc:throwingNew",
        "/Zc:inline",
        "/wd4200",
        "/wd4100",
        { force = true, tools = { "cl" } }
    )
    add_ldflags(
        "/DYNAMICBASE",
        "/NXCOMPAT",
        "/HIGHENTROPYVA",
        "/guard:cf",
        { force = true, tools = { "link" } }
    )

    -- Drop the built DLL straight into its MO2 mod folder so it can be toggled
    -- there like any other mod, leaving the game directory untouched.
    after_build(function (target)
        local deploy_dir = get_config("deploy_dir")
        if not deploy_dir or deploy_dir == "" then
            return
        end
        local plugins_dir = path.join(deploy_dir, "F4SE", "Plugins")
        os.mkdir(plugins_dir)
        os.cp(target:targetfile(), plugins_dir)
        local pdb = path.join(target:targetdir(), target:name() .. ".pdb")
        if os.isfile(pdb) then
            os.cp(pdb, plugins_dir)
        end
        -- Never overwrite a deployed ini: it holds the user's settings.
        local deployed_ini = path.join(plugins_dir, "ScrapStacks.ini")
        if os.isfile("ScrapStacks.ini") and not os.isfile(deployed_ini) then
            os.cp("ScrapStacks.ini", deployed_ini)
            cprint("${bright green}deploy: ${clear}installed a fresh ScrapStacks.ini")
        elseif os.isfile(deployed_ini) then
            cprint("${yellow}deploy: ${clear}kept your existing ScrapStacks.ini")
        end
        cprint("${bright green}deploy: ${clear}copied to %s", plugins_dir)
    end)

-- Unit tests for the game-independent logic in src/logic. These headers must not
-- include anything from CommonLibF4, so the tests build and run without the game.
target("tests")
    set_kind("binary")
    set_arch("x64")
    set_default(false)

    add_packages("doctest")
    add_files("tests/**.cpp")
    add_includedirs("src")
    add_cxxflags("/utf-8", { force = true, tools = { "cl" } })

    -- The exe tests run discovery against every unpacked Fallout4.exe in re/.
    on_load(function (target)
        local function quoted(p) return '"' .. p:gsub("\\", "/") .. '"' end
        target:add("defines", "SCRAPSTACKS_RE_DIR=" .. quoted(path.join(os.projectdir(), "..", "re")))
        local addrlib = get_config("addrlib_dir")
        if not addrlib or addrlib == "" then
            addrlib = path.join(os.projectdir(), "..", "re", "addrlib")
        end
        target:add("defines", "SCRAPSTACKS_ADDRLIB_DIR=" .. quoted(addrlib))
    end)
