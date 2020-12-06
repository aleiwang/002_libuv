-- xmake macro package -a x86_64 -o ../000_packages -p mingw
add_packagedirs("../000_packages")

set_project("002_libuv") 
set_version("1.0.0", {build = "%Y%m%d%H%M"})
set_xmakever("2.2.5")
set_warnings("all", "error")

set_languages("gnu99", "cxx11")

add_cxflags("-Wno-error=deprecated-declarations", "-fno-strict-aliasing", 
    "-Wno-error=unused-parameter", "-Wno-error=implicit-fallthrough",
    "-Wno-error=sign-compare", "-Wno-error=format")
add_cxflags("-O2", "-Wall", "-Wextra", "-fPIC")

add_defines("LUA_COMPAT_5_2=1")
set_strip("all")

target("uv-1.0.0") 
    set_kind("static")    
    add_headerfiles("libuv/*.h")
    add_files("libuv/*.c") 
    add_includedirs("libuv") 
    if is_plat("linux") then
        add_defines("_LINUX")
        add_files("libuv/unix/async.c",
        "libuv/unix/core.c",
        "libuv/unix/dl.c",
        "libuv/unix/fs.c",
        "libuv/unix/getaddrinfo.c",
        "libuv/unix/getnameinfo.c",
        "libuv/unix/linux-core.c",
        "libuv/unix/linux-inotify.c",
        "libuv/unix/linux-syscalls.c",
        "libuv/unix/loop.c",
        "libuv/unix/loop-watcher.c",
        "libuv/unix/pipe.c",
        "libuv/unix/poll.c",
        "libuv/unix/process.c",
        "libuv/unix/proctitle.c",
        "libuv/unix/signal.c",
        "libuv/unix/stream.c",
        "libuv/unix/tcp.c",
        "libuv/unix/thread.c",
        "libuv/unix/timer.c",
        "libuv/unix/tty.c",
        "libuv/unix/udp.c")
    elseif is_plat("mingw") then
        add_defines("MINGW") 
        add_files("libuv/win/*.c")
    end

target("uvdrv")  
    set_kind("shared")
    add_deps("uv-1.0.0")
    add_includedirs("libuv") 
    add_files("uvdrv/*.c", "uvdrv/*.cpp")
    add_packages("lualib-5.3.6", {links = "lualib-5.3.6"})
    add_links("uv-1.0.0")
    add_rules("binaryrule")
    if is_plat("linux") then
        add_links("dl", "pthread")
    elseif is_plat("mingw") then
        add_links("ws2_32", "psapi", "iphlpapi")
    end 

rule("binaryrule")
    set_extensions(".so", ".dll")  
    after_package(function (target)
    end)

target("postprocess")
    set_kind("phony")
    add_deps("uvdrv", "uv-1.0.0") 
    after_package(function (target)
        local srcdir  =  "$(projectdir)/lualib"
        local destdir1 = "../000_packages/uv-1.0.0.pkg/lualib"
        local destdir2 = "../000_packages/uv-1.0.0.pkg"
        if os.exists(destdir1) then os.rm(destdir1) end 

        print("package " .. srcdir .. " => " .. destdir2) 
        os.cp(srcdir, destdir2) 
    end)     
