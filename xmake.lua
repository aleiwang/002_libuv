-- xmake macro package -a x86_64 -o ../000_packages -p mingw

set_project("002_libuv") 
set_version("1.0.0", {build = "%Y%m%d%H%M"})
set_xmakever("2.2.5")
set_warnings("all", "error")

--set_languages("c99", "cxx11")

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