cmake_minimum_required(VERSION 3.16)
project(ChikenGun4K VERSION 2.5.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# 1. Многопоточный генератор 4K PBR текстур, кубической карты неба и 4K загрузочного арта
add_executable(gen_textures_4k tools/gen_textures_4k.cpp)
target_compile_options(gen_textures_4k PRIVATE -O3 -Wall -Wextra)
target_link_libraries(gen_textures_4k PRIVATE Threads::Threads)

# 2. Модульные тесты ядра (Lite-физика, баня, рулетка/слоты 777, 6 миниигр, PNG 4K)
enable_testing()
add_executable(test_core tests/test_core.cpp)
# Unit tests must keep assert() active even when a release preset defines NDEBUG.
target_compile_options(test_core PRIVATE -O2 -Wall -Wextra -UNDEBUG)
add_test(NAME CorePhysicsAndAssets COMMAND test_core $<TARGET_FILE:gen_textures_4k>)

# 3. Цель генерации PBR текстур (1024 быстро по умолчанию, 4096 для 4K ULTRA)
set(TEX_SIZE "1024" CACHE STRING "Разрешение процедурных PBR текстур (1024..4096)")
add_custom_target(textures_4k
    COMMAND gen_textures_4k "${CMAKE_BINARY_DIR}/Data/Textures" "${TEX_SIZE}"
    DEPENDS gen_textures_4k
    COMMENT "Генерация PBR текстур ${TEX_SIZE}x${TEX_SIZE} + Skybox4K + LoadingBg4K..."
)

# 4. Сборка нативного игрового клиента MyShooter (Urho3D авто-поиск или FetchContent)
option(BUILD_GAME "Собирать игровой клиент MyShooter (требует SDL2, OpenGL, X11)" ON)
option(AUTO_FETCH_URHO3D "Скачать и собрать Urho3D автоматически через FetchContent, если не найден локально" ON)
set(URHO3D_GIT_TAG "1.9.0" CACHE STRING "Тег/ветка Urho3D для автосборки")

if(BUILD_GAME)
    set(URHO_HINTS
        "${CMAKE_CURRENT_SOURCE_DIR}/.cache/urho3d-sdk"
        "$ENV{URHO3D_HOME}"
        "$ENV{HOME}/urho3d/build"
        "$ENV{HOME}/urho3d_build"
        "$ENV{HOME}/urho3d-local"
        "/usr/local"
        "/usr"
    )

    find_path(URHO3D_INCLUDE_DIR
        NAMES Urho3D/Engine/Application.h
        HINTS ${URHO_HINTS}
        PATH_SUFFIXES include build/include
    )

    find_library(URHO3D_LIBRARY
        NAMES Urho3D
        HINTS ${URHO_HINTS}
        PATH_SUFFIXES lib lib/Urho3D build/lib
    )

    set(_URHO_READY OFF)
    set(_URHO_IS_FETCHED OFF)

    if(URHO3D_INCLUDE_DIR AND URHO3D_LIBRARY)
        message(STATUS "Найден локальный Urho3D: include=${URHO3D_INCLUDE_DIR}, lib=${URHO3D_LIBRARY}")
        set(_URHO_READY ON)
    elseif(AUTO_FETCH_URHO3D)
        message(STATUS "Локальный Urho3D не найден — загружаю Urho3D ${URHO3D_GIT_TAG} через FetchContent...")
        include(FetchContent)
        set(URHO3D_LIB_TYPE STATIC CACHE STRING "" FORCE)
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
        FetchContent_Declare(
            urho3d_fc
            GIT_REPOSITORY https://github.com/urho3d/Urho3D.git
            GIT_TAG "${URHO3D_GIT_TAG}"
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(urho3d_fc)
        set(_URHO_READY ON)
        set(_URHO_IS_FETCHED ON)
    else()
        message(WARNING "Urho3D не найден и AUTO_FETCH_URHO3D=OFF — цель MyShooter пропущена.")
    endif()

    if(_URHO_READY)
        find_package(OpenGL REQUIRED)
        find_package(X11 QUIET)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 QUIET sdl2)
        endif()

        add_executable(MyShooter src/MyApp.cpp)
        target_compile_options(MyShooter PRIVATE -O2 -Wall -Wextra)
        set_target_properties(MyShooter PROPERTIES
            INSTALL_RPATH "$ORIGIN;$ORIGIN/lib"
            BUILD_WITH_INSTALL_RPATH TRUE
        )

        if(_URHO_IS_FETCHED)
            target_link_libraries(MyShooter PRIVATE Urho3D)
            target_include_directories(MyShooter SYSTEM PRIVATE
                "${urho3d_fc_BINARY_DIR}/include"
                "${urho3d_fc_BINARY_DIR}/include/Urho3D/ThirdParty"
                "${urho3d_fc_SOURCE_DIR}/Source/ThirdParty/Bullet/src"
            )
        else()
            target_include_directories(MyShooter SYSTEM PRIVATE
                "${URHO3D_INCLUDE_DIR}"
                "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty"
                "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/Bullet"
                "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/Bullet/src"
            )
            target_link_libraries(MyShooter PRIVATE "${URHO3D_LIBRARY}")
        endif()

        if(SDL2_INCLUDE_DIRS)
            target_include_directories(MyShooter SYSTEM PRIVATE ${SDL2_INCLUDE_DIRS})
        endif()

        target_link_libraries(MyShooter PRIVATE
            OpenGL::GL
            Threads::Threads
            ${SDL2_LIBRARIES}
            ${CMAKE_DL_LIBS}
            m
        )
        if(X11_FOUND)
            target_link_libraries(MyShooter PRIVATE ${X11_LIBRARIES})
        endif()

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
