newoption {
    trigger     = "with-docs",
    description = "Use doxygen to build the docs"
}
 
newoption {
    trigger = "lib-dir",
    description = "Specify extra directory to look for libraries in",
    value = "DIR"
}

newoption {
    trigger = "include-dir",
    description = "Specify extra directory to look for headers in",
    value = "DIR"
}

workspace "CryptoKernel"
    configurations {"Debug", "Release"}
    platforms {"Static", "Shared"}
    language "C++"
    cppdialect "C++14"
    includedirs {"/usr/include/lua5.3", "src/kernel",
                 _OPTIONS["include-dir"]}
    libdirs {_OPTIONS["lib-dir"]}
    symbols "On"

    filter {"configurations:Debug"}
        optimize "Off"

    filter {"configurations:Release"}
        optimize "Full"

cklibs = {"crypto", "lua5.3", "sfml-network", 
"sfml-system", "leveldb", "jsoncpp", "jsonrpccpp-server", 
"jsonrpccpp-client", "jsonrpccpp-common", "microhttpd"}


project "ck"
        
    files {"src/kernel/**.cpp", "src/kernel/**.h", "src/kernel/**.c"}
    
    configuration "with-docs"
        postbuildcommands{"doxygen %{wks.location}/doxyfile"}
    
    filter {"platforms:Static"}
        kind "StaticLib"

    filter {"platforms:Shared"}
        kind "SharedLib"

project "ckd"

    kind "ConsoleApp"
    files {"src/client/**.cpp", "src/client/**.h"}
    links {"ck"}
    links(cklibs)

    filter "system:linux"
        links {"pthread"}

project "test"

    kind "ConsoleApp"
    files {"tests/**.cpp", "tests/**.h"}
    links {"ck", "cppunit"}
    links(cklibs)
    postbuildcommands{"%{cfg.linktarget.abspath}"}
