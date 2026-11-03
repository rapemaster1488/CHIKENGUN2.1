cmake_minimum_required(VERSION 3.16)
project(ChikenGun4K VERSION 2.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

# 1. Многопоточный генератор 4K PBR текстур, кубической карты неба и 4K загрузочного арта
add_executable(gen_textures_4k tools/gen_textures_4k.cpp)
target_compile_options(gen_textures_4k PRIVATE -O3 -Wall -Wextra)
target_link_libraries(gen_textures_4k PRIVATE Threads::Threads)

# 2. Модульные тесты C++ ядра (LitePhysics, CyberSystems 2.4, рейкасты, настройки XML, PNG 4K)
enable_testing()
add_executable(test_core tests/test_core.cpp)
target_compile_options(test_core PRIVATE -O2 -Wall -Wextra)
add_test(NAME CorePhysicsAndAssets COMMAND test_core $<TARGET_FILE:gen_textures_4k>)

# 3. Цель генерации 4K текстур (по умолчанию 4096x4096)
set(TEX_SIZE "4096" CACHE STRING "Разрешение процедурных PBR текстур")
add_custom_target(textures_4k
    COMMAND gen_textures_4k "${CMAKE_BINARY_DIR}/Data/Textures" "${TEX_SIZE}"
    DEPENDS gen_textures_4k
    COMMENT "Генерация PBR текстур ${TEX_SIZE}x${TEX_SIZE} + Skybox4K + LoadingBg4K..."
)

# 4. Поиск Urho3D и сборка нативного C++ клиента MyShooter (отключается через -DBUILD_GAME=OFF в быстрых CI-проверках)
option(BUILD_GAME "Собирать нативный C++ игровой клиент MyShooter (требует Urho3D, SDL2, OpenGL)" ON)

if(BUILD_GAME)
    set(URHO_HINTS
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
        PATH_SUFFIXES lib build/lib
    )

    if(URHO3D_INCLUDE_DIR AND URHO3D_LIBRARY)
        message(STATUS "Найден Urho3D: include=${URHO3D_INCLUDE_DIR}, lib=${URHO3D_LIBRARY}")
        find_package(OpenGL REQUIRED)
        find_package(X11 QUIET)
        find_package(PkgConfig QUIET)
        if(PkgConfig_FOUND)
            pkg_check_modules(SDL2 QUIET sdl2)
        endif()

        add_executable(MyShooter src/MyApp.cpp)
        target_compile_options(MyShooter PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -Wno-sign-compare)
        set_target_properties(MyShooter PROPERTIES
            BUILD_RPATH "$ORIGIN/lib"
            INSTALL_RPATH "$ORIGIN/lib"
        )
        target_include_directories(MyShooter PRIVATE
            "${URHO3D_INCLUDE_DIR}"
            "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty"
            "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/Bullet"
            "${URHO3D_INCLUDE_DIR}/Urho3D/ThirdParty/Bullet/src"
            ${SDL2_INCLUDE_DIRS}
        )
        target_link_libraries(MyShooter PRIVATE
            "${URHO3D_LIBRARY}"
            OpenGL::GL
            Threads::Threads
            ${SDL2_LIBRARIES}
            ${CMAKE_DL_LIBS}
            m
        )
        if(X11_FOUND)
            target_link_libraries(MyShooter PRIVATE ${X11_LIBRARIES})
        endif()

        install(TARGETS MyShooter RUNTIME DESTINATION .)
    else()
        message(WARNING "Urho3D не найден — цель MyShooter пропущена (собраны gen_textures_4k и test_core). Укажите -DURHO3D_INCLUDE_DIR=... -DURHO3D_LIBRARY=...")
    endif()
endif()

# 5. Установка ресурсов и упаковка скачиваемого архива игры (CPack TGZ / ZIP)
install(TARGETS gen_textures_4k RUNTIME DESTINATION tools)
install(DIRECTORY CoreData DESTINATION .)
install(DIRECTORY Data DESTINATION .
    PATTERN "Data/Data" EXCLUDE
    PATTERN "Data/Urho2D" EXCLUDE
    PATTERN "Data/LuaScripts" EXCLUDE
    PATTERN "Data/Scripts" EXCLUDE
)
install(FILES bin/settings.ini DESTINATION .)
install(PROGRAMS run.sh DESTINATION .)

set(CPACK_PACKAGE_NAME "CHIKENGUN")
set(CPACK_PACKAGE_VENDOR "ChikenGun Team")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "CHIKENGUN-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_GENERATOR "TGZ;ZIP")
include(CPack)
