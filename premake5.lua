workspace "CryptoKernel"
    configurations {"Debug", "Release"}
    platforms {"Static", "Shared"}
    language "C++"
    cppdialect "C++14"
    includedirs {"/usr/include/lua5.3", "src/kernel"}

    symbols "On"

    filter {"configurations:Debug"}
        optimize "Off"

    filter {"configurations:Release"}
        optimize "Full"

cklibs = {"crypto", "lua5.3", "sfml-network", 
"sfml-system", "leveldb", "jsoncpp", "jsonrpccpp-server", 
"jsonrpccpp-client", "jsonrpccpp-common", "microhttpd"}

newoption {
    trigger     = "with-docs",
    description = "Use doxygen to build the docs"
 }
 

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
