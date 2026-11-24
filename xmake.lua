includes("lib/commonlibsse-ng")


set_project("CreatureMotion")
set_version("1.0.0")
set_license("GPL-3.0")
set_languages("c++23")
set_warnings("allextra")

add_rules("mode.debug", "mode.releasedbg")
add_rules("plugin.vsxmake.autoupdate")

target("CreatureMotion")
    add_rules("commonlibsse-ng.plugin", {
        name = "CreatureMotion",
        author = "Rain",
        description = "Flight for floating creatures and burrowing for digging ones"
    })

    add_installfiles("dist/CreatureMotion.ini", { prefixdir = "SKSE/Plugins" })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    add_includedirs("extern/SKSEMenuFramework")
    set_pcxxheader("src/pch.h")
