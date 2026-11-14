cmake_minimum_required(VERSION 3.16)
project(ChikenGun4K VERSION 2.5.6 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# 1. Многопоточный генератор 4K PBR текстур, кубической карты неба и 4K загрузочного арта
add_executable(gen_textures_4k tools/gen_textures_4k.cpp)
if(MSVC)
    target_compile_options(gen_textures_4k PRIVATE /O2 /W3 /EHsc /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX)
else()
    target_compile_options(gen_textures_4k PRIVATE -O3 -Wall -Wextra)
endif()
target_link_libraries(gen_textures_4k PRIVATE Threads::Threads)

# 2. Модульные тесты ядра (Lite-физика, баня, рулетка/слоты 777, 6 миниигр, PNG 4K)
enable_testing()
add_executable(test_core tests/test_core.cpp)
# Unit tests must keep assert() active even when a release preset defines NDEBUG.
if(MSVC)
    target_compile_options(test_core PRIVATE /O2 /W3 /EHsc /UNDEBUG /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX)
else()
    target_compile_options(test_core PRIVATE -O2 -Wall -Wextra -UNDEBUG)
endif()
add_test(NAME CorePhysicsAndAssets COMMAND test_core $<TARGET_FILE:gen_textures_4k>)
set_tests_properties(CorePhysicsAndAssets PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    ENVIRONMENT "CHIKENGUN_DATA_DIR=${CMAKE_CURRENT_SOURCE_DIR}/Data"
)

find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_test(NAME UrhoAppleCpuPatch
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_urho_apple_cpu_patch.py")
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Test all startup decisions without SDL, a display server or sound hardware.
    add_executable(test_sdl_policy tests/test_sdl_policy.cpp)
    target_include_directories(test_sdl_policy PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests/fake")
    target_compile_options(test_sdl_policy PRIVATE -O2 -Wall -Wextra -UNDEBUG)
    add_test(NAME SDLStartupPolicy COMMAND test_sdl_policy)

    find_program(BASH_EXECUTABLE bash)
    if(BASH_EXECUTABLE)
        add_test(NAME BuildScriptSafety
            COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_build_scripts.sh")
        set_tests_properties(BuildScriptSafety PROPERTIES TIMEOUT 60)

        # Docker packaging contract (Dockerfile, compose, entrypoint, docs).
        add_test(NAME DockerPackaging
            COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_docker_files.sh")
        set_tests_properties(DockerPackaging PROPERTIES TIMEOUT 60)

        find_program(PKG_CONFIG_FOR_LINK_TEST pkg-config)
        if(PKG_CONFIG_FOR_LINK_TEST)
            add_test(NAME CMakeSDLLinkage
                COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_cmake_sdl_linkage.sh"
                        "${CMAKE_COMMAND}" "${CMAKE_CXX_COMPILER}" "${PKG_CONFIG_FOR_LINK_TEST}" "${CMAKE_AR}")
            set_tests_properties(CMakeSDLLinkage PROPERTIES TIMEOUT 90)
        endif()
    endif()
endif()

# 3. Цель генерации PBR текстур (1024 быстро по умолчанию, 4096 для 4K ULTRA)
set(TEX_SIZE "1024" CACHE STRING "Разрешение процедурных PBR текстур (1024..4096)")
add_custom_target(textures_4k
    COMMAND gen_textures_4k "${CMAKE_BINARY_DIR}/Data/Textures" "${TEX_SIZE}"
    DEPENDS gen_textures_4k
    COMMENT "Генерация PBR текстур ${TEX_SIZE}x${TEX_SIZE} + Skybox4K + LoadingBg4K..."
)

# 4. Сборка нативного игрового клиента MyShooter (Urho3D авто-поиск, pkg-config или FetchContent)
option(BUILD_GAME "Собирать игровой клиент MyShooter (требует SDL2 и OpenGL)" ON)
option(AUTO_FETCH_URHO3D "Скачать и собрать Urho3D автоматически через FetchContent, если не найден локально" ON)
set(URHO3D_GIT_TAG "1.9.0" CACHE STRING "Тег/ветка Urho3D для автосборки")
set(URHO3D_HOME "" CACHE PATH "Путь к установленному Urho3D SDK")

if(BUILD_GAME)
    set(URHO_HINTS
        "${URHO3D_HOME}"
        "$ENV{URHO3D_HOME}"
        "${CMAKE_CURRENT_SOURCE_DIR}/.cache/urho3d-sdk"
        "$ENV{HOME}/urho3d/build"
        "$ENV{HOME}/urho3d-build/out"
        "$ENV{HOME}/urho3d-build"
        "$ENV{HOME}/urho3d_build/out"
        "$ENV{HOME}/urho3d_build"
        "$ENV{HOME}/urho3d-local"
        "/opt/homebrew"
        "/usr/local"
        "/usr"
    )

    set(_urho_lib_suffixes lib lib/Urho3D lib64 lib64/Urho3D build/lib build/lib/Urho3D)
    if(CMAKE_LIBRARY_ARCHITECTURE)
        list(APPEND _urho_lib_suffixes "lib/${CMAKE_LIBRARY_ARCHITECTURE}" "lib/${CMAKE_LIBRARY_ARCHITECTURE}/Urho3D")
    endif()

    # A user-selected SDK should win over CMAKE_PREFIX_PATH and cached SDKs.
    # Respect explicit URHO3D_INCLUDE_DIR / URHO3D_LIBRARY cache overrides.
    set(_explicit_urho_home "${URHO3D_HOME}")
    if(NOT _explicit_urho_home AND DEFINED ENV{URHO3D_HOME})
        set(_explicit_urho_home "$ENV{URHO3D_HOME}")
    endif()
    if(_explicit_urho_home)
        find_path(URHO3D_INCLUDE_DIR
            NAMES Urho3D/Engine/Application.h
            HINTS "${_explicit_urho_home}"
            PATH_SUFFIXES include build/include include/Urho3D
            NO_DEFAULT_PATH
        )
        find_library(URHO3D_LIBRARY
            NAMES Urho3D Urho3D_d
            HINTS "${_explicit_urho_home}"
            PATH_SUFFIXES ${_urho_lib_suffixes}
            NO_DEFAULT_PATH
        )
        if(NOT URHO3D_INCLUDE_DIR OR NOT URHO3D_LIBRARY)
            message(FATAL_ERROR "URHO3D_HOME does not contain both Urho3D headers and a library: ${_explicit_urho_home}. Fix the SDK path or specify URHO3D_INCLUDE_DIR and URHO3D_LIBRARY explicitly.")
        endif()
    endif()

    find_path(URHO3D_INCLUDE_DIR
        NAMES Urho3D/Engine/Application.h
        HINTS ${URHO_HINTS}
        PATH_SUFFIXES include build/include include/Urho3D
    )

    find_library(URHO3D_LIBRARY
        NAMES Urho3D Urho3D_d
        HINTS ${URHO_HINTS}
        PATH_SUFFIXES ${_urho_lib_suffixes}
    )

    find_package(PkgConfig QUIET)
    set(URHO3D_PC_FOUND FALSE)
    if(PkgConfig_FOUND)
        set(_extra_pc_dirs "")
        if(URHO3D_HOME)
            list(APPEND _extra_pc_dirs "${URHO3D_HOME}/lib/pkgconfig" "${URHO3D_HOME}/lib64/pkgconfig")
        endif()
        if(DEFINED ENV{URHO3D_HOME} AND NOT "$ENV{URHO3D_HOME}" STREQUAL "")
            list(APPEND _extra_pc_dirs "$ENV{URHO3D_HOME}/lib/pkgconfig" "$ENV{URHO3D_HOME}/lib64/pkgconfig")
        endif()
        list(APPEND _extra_pc_dirs
            "${CMAKE_CURRENT_SOURCE_DIR}/.cache/urho3d-sdk/lib/pkgconfig"
            "${CMAKE_CURRENT_SOURCE_DIR}/.cache/urho3d-sdk/lib64/pkgconfig"
            "$ENV{HOME}/urho3d-build/out/lib/pkgconfig"
            "$ENV{HOME}/urho3d-build/lib/pkgconfig"
            "$ENV{HOME}/urho3d-local/lib/pkgconfig"
            "$ENV{HOME}/urho3d-local/lib64/pkgconfig"
            "$ENV{HOME}/.nix-profile/lib/pkgconfig"
            "/run/current-system/sw/lib/pkgconfig"
        )
        # Preserve priority: an explicitly selected SDK must precede the
        # repository cache, not be reversed by repeated prepends.
        set(_existing_pc_dirs "")
        foreach(_pcd IN LISTS _extra_pc_dirs)
            if(EXISTS "${_pcd}")
                list(APPEND _existing_pc_dirs "${_pcd}")
            endif()
        endforeach()
        if(_existing_pc_dirs)
            list(JOIN _existing_pc_dirs ":" _extra_pc_path)
            set(ENV{PKG_CONFIG_PATH} "${_extra_pc_path}:$ENV{PKG_CONFIG_PATH}")
        endif()
        pkg_check_modules(URHO3D_PC QUIET Urho3D)
        if(URHO3D_PC_FOUND AND NOT URHO3D_INCLUDE_DIR)
            find_path(URHO3D_INCLUDE_DIR NAMES Urho3D/Engine/Application.h HINTS ${URHO3D_PC_INCLUDE_DIRS})
        endif()
        if(URHO3D_PC_FOUND AND URHO3D_LIBRARY AND URHO3D_PC_LIBDIR)
            get_filename_component(_selected_lib_dir "${URHO3D_LIBRARY}" DIRECTORY)
            get_filename_component(_selected_lib_dir "${_selected_lib_dir}" REALPATH)
            get_filename_component(_pc_lib_dir "${URHO3D_PC_LIBDIR}" REALPATH)
            if(NOT _selected_lib_dir STREQUAL _pc_lib_dir)
                message(STATUS "Ignoring Urho3D.pc from ${_pc_lib_dir}: selected library is in ${_selected_lib_dir}")
                set(URHO3D_PC_FOUND FALSE)
            endif()
        endif()
    endif()

    set(_URHO_READY OFF)
    set(_URHO_IS_FETCHED OFF)

    if(URHO3D_INCLUDE_DIR AND URHO3D_LIBRARY)
        message(STATUS "Найден локальный Urho3D: include=${URHO3D_INCLUDE_DIR}, lib=${URHO3D_LIBRARY}")
        set(_URHO_READY ON)
    elseif(AUTO_FETCH_URHO3D)
        message(STATUS "Локальный Urho3D не найден — загружаю Urho3D ${URHO3D_GIT_TAG} через FetchContent...")
        include(FetchContent)
        if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
            set(CMAKE_POLICY_VERSION_MINIMUM "3.5" CACHE STRING "" FORCE)
        endif()
        set(LIB_SUFFIX "" CACHE STRING "" FORCE)
        set(URHO3D_USE_LIB_DEB ON CACHE BOOL "" FORCE)
        set(URHO3D_LIB_TYPE STATIC CACHE STRING "" FORCE)
        set(URHO3D_C++11 ON CACHE BOOL "" FORCE)
        set(URHO3D_PCH OFF CACHE BOOL "" FORCE)
        set(URHO3D_SAMPLES OFF CACHE BOOL "" FORCE)
        set(URHO3D_TOOLS OFF CACHE BOOL "" FORCE)
        set(URHO3D_LUA OFF CACHE BOOL "" FORCE)
        set(URHO3D_LUAJIT OFF CACHE BOOL "" FORCE)
        set(URHO3D_ANGELSCRIPT OFF CACHE BOOL "" FORCE)
        set(URHO3D_NETWORK OFF CACHE BOOL "" FORCE)
        set(URHO3D_DATABASE_ODBC OFF CACHE BOOL "" FORCE)
        set(URHO3D_DATABASE_SQLITE OFF CACHE BOOL "" FORCE)
        set(URHO3D_URHO2D OFF CACHE BOOL "" FORCE)
        set(URHO3D_WEBP OFF CACHE BOOL "" FORCE)
        set(URHO3D_TESTING OFF CACHE BOOL "" FORCE)
        set(URHO3D_DOCS OFF CACHE BOOL "" FORCE)
        if(WIN32)
            # Наши кастомные пост-эффекты (Cinema4K.glsl, SSAO4K.glsl) написаны на GLSL
            set(URHO3D_OPENGL ON CACHE BOOL "" FORCE)
            set(URHO3D_D3D11 OFF CACHE BOOL "" FORCE)
            set(URHO3D_WIN32_CONSOLE OFF CACHE BOOL "" FORCE)
        endif()

        set(_SAVED_C_FLAGS "${CMAKE_C_FLAGS}")
        set(_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu11 -Wno-error -Wno-implicit-function-declaration -Wno-incompatible-pointer-types")
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include cstdint -Wno-error")
        endif()

        FetchContent_Declare(
            urho3d_fc
            GIT_REPOSITORY https://github.com/urho3d/Urho3D.git
            GIT_TAG "${URHO3D_GIT_TAG}"
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(urho3d_fc)

        set(CMAKE_C_FLAGS "${_SAVED_C_FLAGS}")
        set(CMAKE_CXX_FLAGS "${_SAVED_CXX_FLAGS}")
        set(_URHO_READY ON)
        set(_URHO_IS_FETCHED ON)
    else()
        message(WARNING "Urho3D не найден и AUTO_FETCH_URHO3D=OFF — цель MyShooter пропущена.")
    endif()

    if(_URHO_READY)
        find_package(OpenGL REQUIRED)
        if(UNIX AND NOT APPLE)
            find_package(X11 QUIET)
        endif()
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 QUIET sdl2)
            if(UNIX AND NOT APPLE)
                pkg_check_modules(ALSA QUIET alsa)
                pkg_check_modules(PULSE QUIET libpulse libpulse-simple)
                pkg_check_modules(UDEV QUIET libudev)
                pkg_check_modules(XRANDR QUIET xrandr)
                pkg_check_modules(XI QUIET xi)
                pkg_check_modules(XEXT QUIET xext)
                pkg_check_modules(XCURSOR QUIET xcursor)
                pkg_check_modules(XINERAMA QUIET xinerama)
                pkg_check_modules(XXF86VM QUIET xxf86vm)
                pkg_check_modules(XRENDER QUIET xrender)
                pkg_check_modules(XFIXES QUIET xfixes)
                pkg_check_modules(XSCRNSAVER QUIET xscrnsaver)
                pkg_check_modules(XCB QUIET xcb)
                pkg_check_modules(DBUS QUIET dbus-1)
                pkg_check_modules(XKBCOMMON QUIET xkbcommon)
                pkg_check_modules(WAYLAND QUIET wayland-client wayland-egl wayland-cursor)
            endif()
        endif()

        # На Windows URHO3D_DEFINE_APPLICATION_MAIN создаёт WinMain (если не задан URHO3D_WIN32_CONSOLE)
        if(WIN32)
            add_executable(MyShooter WIN32 src/MyApp.cpp)
            target_compile_definitions(MyShooter PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
        else()
            add_executable(MyShooter src/MyApp.cpp)
        endif()

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            if(BASH_EXECUTABLE)
                set(_sdl_test_urho_libdir "")
                if(NOT _URHO_IS_FETCHED AND URHO3D_LIBRARY)
                    get_filename_component(_sdl_test_urho_libdir "${URHO3D_LIBRARY}" DIRECTORY)
                endif()
                add_test(NAME SDLStartupDiagnostics
                    COMMAND "${BASH_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_sdl_startup.sh"
                            $<TARGET_FILE:MyShooter> "${_sdl_test_urho_libdir}")
                set_tests_properties(SDLStartupDiagnostics PROPERTIES TIMEOUT 30)
            endif()
        endif()

        if(MSVC)
            target_compile_options(MyShooter PRIVATE /O2 /W3 /EHsc /bigobj /MP /D_CRT_SECURE_NO_WARNINGS)
        else()
            target_compile_options(MyShooter PRIVATE -O2 -Wall -Wextra)
            if(MINGW)
                target_compile_options(MyShooter PRIVATE -Wa,-mbig-obj)
            endif()
        endif()

        if(APPLE)
            set_target_properties(MyShooter PROPERTIES
                INSTALL_RPATH "@executable_path;@executable_path/lib"
                BUILD_WITH_INSTALL_RPATH TRUE
            )
        elseif(UNIX)
            set_target_properties(MyShooter PROPERTIES
                INSTALL_RPATH "$ORIGIN;$ORIGIN/lib"
                BUILD_WITH_INSTALL_RPATH TRUE
            )
        endif()

        if(_URHO_IS_FETCHED)
            target_link_libraries(MyShooter PRIVATE Urho3D)
            target_include_directories(MyShooter SYSTEM PRIVATE
                "${urho3d_fc_BINARY_DIR}/include"
                "${urho3d_fc_BINARY_DIR}/include/Urho3D/ThirdParty"
                "${urho3d_fc_BINARY_DIR}/Source/ThirdParty/SDL/include/generated"
                "${urho3d_fc_BINARY_DIR}/include/Urho3D/ThirdParty/SDL"
                "${urho3d_fc_SOURCE_DIR}/Source/ThirdParty/Bullet/src"
            )
            if(TARGET Urho3D)
                get_target_property(_urho_type Urho3D TYPE)
                if(_urho_type STREQUAL "STATIC_LIBRARY")
                    target_compile_definitions(MyShooter PRIVATE URHO3D_STATIC_DEFINE)
                endif()
            endif()
        else()
            target_include_directories(MyShooter SYSTEM PRIVATE
                "${URHO3D_INCLUDE_DIR}"
                "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty"
                "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/Bullet"
                "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/Bullet/src"
            )
            if(EXISTS "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/SDL/SDL_joystick.h"
               AND NOT EXISTS "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/SDL/SDL_config.h")
                if(EXISTS "${URHO3D_INCLUDE_DIR}/../Source/ThirdParty/SDL/include/generated/SDL_config.h")
                    target_include_directories(MyShooter SYSTEM PRIVATE
                        "${URHO3D_INCLUDE_DIR}/../Source/ThirdParty/SDL/include/generated"
                        "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/SDL"
                    )
                elseif(EXISTS "${URHO3D_INCLUDE_DIR}/../Source/ThirdParty/SDL/include/SDL_config.h")
                    target_include_directories(MyShooter SYSTEM PRIVATE
                        "${URHO3D_INCLUDE_DIR}/../Source/ThirdParty/SDL/include"
                        "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/SDL"
                    )
                elseif(SDL2_INCLUDE_DIRS)
                    target_include_directories(MyShooter SYSTEM PRIVATE
                        ${SDL2_INCLUDE_DIRS}
                        "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/SDL"
                    )
                endif()
            endif()
            target_link_libraries(MyShooter PRIVATE "${URHO3D_LIBRARY}")
            if(URHO3D_LIBRARY MATCHES "\\.a$" OR URHO3D_LIBRARY MATCHES "Urho3D(_d)?\\.lib$")
                target_compile_definitions(MyShooter PRIVATE URHO3D_STATIC_DEFINE)
            endif()
            if(URHO3D_PC_FOUND)
                set(_urho_pc_defs ${URHO3D_PC_CFLAGS_OTHER})
                list(FILTER _urho_pc_defs INCLUDE REGEX "^-D")
                if(_urho_pc_defs)
                    target_compile_options(MyShooter PRIVATE ${_urho_pc_defs})
                endif()
                target_link_libraries(MyShooter PRIVATE ${URHO3D_PC_LDFLAGS})
            endif()
        endif()

        set(_NEED_SYS_SDL2 FALSE)
        if(NOT _URHO_IS_FETCHED AND SDL2_LIBRARIES)
            if(NOT EXISTS "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/SDL/SDL_joystick.h")
                set(_NEED_SYS_SDL2 TRUE)
            elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
                # Official SDKs embed SDL, but some ship the same headers while
                # linking SDL separately. Inspect the actual library so that
                # preflight uses Urho3D's instance rather than a second SDL.
                set(_urho_symbols_known FALSE)
                set(_urho_exports_sdl FALSE)
                if(CMAKE_NM)
                    if(URHO3D_LIBRARY MATCHES "\\.a$")
                        set(_nm_args -g --defined-only)
                    else()
                        set(_nm_args -D --defined-only)
                    endif()
                    execute_process(
                        COMMAND "${CMAKE_NM}" ${_nm_args} "${URHO3D_LIBRARY}"
                        RESULT_VARIABLE _nm_status OUTPUT_VARIABLE _urho_symbols ERROR_QUIET
                    )
                    if(_nm_status EQUAL 0)
                        set(_urho_symbols_known TRUE)
                        if(_urho_symbols MATCHES "[ \t][TW] _?SDL_Init(@[^ \r\n]*)?([\r\n]|$)")
                            set(_urho_exports_sdl TRUE)
                        endif()
                    endif()
                    unset(_urho_symbols)
                endif()
                if((_urho_symbols_known AND NOT _urho_exports_sdl) OR
                   (NOT _urho_symbols_known AND NOT URHO3D_LIBRARY MATCHES "\\.a$"))
                    set(_NEED_SYS_SDL2 TRUE)
                endif()
            endif()
        endif()
        if(_NEED_SYS_SDL2 AND SDL2_INCLUDE_DIRS)
            target_include_directories(MyShooter SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
        endif()

        target_link_libraries(MyShooter PRIVATE
            OpenGL::GL
            Threads::Threads
        )
        if(_NEED_SYS_SDL2)
            if(SDL2_LIBRARY_DIRS)
                target_link_directories(MyShooter PRIVATE ${SDL2_LIBRARY_DIRS})
            endif()
            target_link_libraries(MyShooter PRIVATE ${SDL2_LIBRARIES})
        endif()

        if(WIN32)
            target_link_libraries(MyShooter PRIVATE
                opengl32 glu32 user32 gdi32 winmm imm32 ole32 oleaut32
                version uuid setupapi shell32 advapi32 ws2_32 iphlpapi dbghelp
            )
        elseif(APPLE)
            target_link_libraries(MyShooter PRIVATE
                "-framework OpenGL"
                "-framework Cocoa"
                "-framework IOKit"
                "-framework CoreAudio"
                "-framework AudioUnit"
                "-framework AudioToolbox"
                "-framework CoreVideo"
                "-framework QuartzCore"
                "-framework CoreFoundation"
                "-framework SystemConfiguration"
                "-framework CoreBluetooth"
                "-framework CoreServices"
                "-framework ForceFeedback"
                "-framework Carbon"
                "-framework Metal"
                "-framework GameController"
                iconv
            )
        else()
            target_link_libraries(MyShooter PRIVATE
                ${CMAKE_DL_LIBS}
                m
            )
            if(TARGET OpenGL::GLU)
                target_link_libraries(MyShooter PRIVATE OpenGL::GLU)
            endif()
            if(X11_FOUND)
                target_link_libraries(MyShooter PRIVATE ${X11_LIBRARIES})
            endif()
            foreach(_extra_lib
                ALSA_LIBRARIES PULSE_LIBRARIES UDEV_LIBRARIES
                XRANDR_LIBRARIES XI_LIBRARIES XEXT_LIBRARIES
                XCURSOR_LIBRARIES XINERAMA_LIBRARIES XXF86VM_LIBRARIES
                XRENDER_LIBRARIES XFIXES_LIBRARIES XSCRNSAVER_LIBRARIES
                XCB_LIBRARIES DBUS_LIBRARIES XKBCOMMON_LIBRARIES WAYLAND_LIBRARIES
            )
                if(${_extra_lib})
                    target_link_libraries(MyShooter PRIVATE ${${_extra_lib}})
                endif()
            endforeach()
        endif()

        # Синхронизация Data/CoreData/settings.ini рядом с готовым бинарником (в т.ч. Debug/Release на Windows/macOS)
        add_custom_command(TARGET MyShooter POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/Data" "$<TARGET_FILE_DIR:MyShooter>/Data"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/CoreData" "$<TARGET_FILE_DIR:MyShooter>/CoreData"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/bin/settings.ini" "$<TARGET_FILE_DIR:MyShooter>/settings.ini"
            COMMENT "Копирование Data/, CoreData/ и settings.ini в директорию запуска..."
        )

        # Установка и CPack для портативного релиза
        install(TARGETS MyShooter RUNTIME DESTINATION .)
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/Data" DESTINATION .)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/CoreData")
            install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/CoreData" DESTINATION .)
        endif()
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/bin/settings.ini")
            install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/bin/settings.ini" DESTINATION .)
        endif()
    endif()
endif()

set(CPACK_PACKAGE_NAME "CHIKENGUN")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_GENERATOR "TGZ;ZIP")
include(CPack)
