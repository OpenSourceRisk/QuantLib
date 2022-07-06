# Platform-specific flags and settings
option(MSVC_LINK_DYNAMIC_RUNTIME "Link against dynamic runtime" ON)
option(MSVC_PARALLELBUILD "Use flag /MP" ON)

if(MSVC)
    # See cmake policy CMP00091
    # One of "MultiThreaded", "MultiThreadedDebug", "MultiThreadedDLL", "MultiThreadedDebugDLL"
    set(BUILD_SHARED_LIBS OFF)

    # build static libs always
    set(CMAKE_MSVC_RUNTIME_LIBRARY
        "MultiThreaded$<$<CONFIG:Debug>:Debug>$<$<BOOL:${MSVC_LINK_DYNAMIC_RUNTIME}>:DLL>")

    # Export all symbols so MSVC can populate the .lib and .dll
    if(BUILD_SHARED_LIBS)
        # # Temp: disable DLL builds on MSVC
        message(FATAL_ERROR
            <CONFIG>_OUTPUT_NAME "Shared library (DLL) builds for QuantLib on MSVC are not supported")
        set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
    endif()

    add_definitions(/MP)

    add_compile_definitions(NOMINMAX)

    # /wd4267
    # Suppress warnings: assignment of 64-bit value to 32-bit QuantLib::Integer (x64)

    # /wd26812
    # Suppress warnings: "Prefer enum class over enum" (Enum.3)
    add_compile_options(/wd4267 /wd26812)
endif()
