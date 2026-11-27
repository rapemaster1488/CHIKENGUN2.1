#!/usr/bin/env bash
# Восстановление заголовков Urho3D 1.9.0 для tools/syntax_check.sh.
#
# ЗАЧЕМ. syntax_check.sh нужен URHO3D_HOME с заголовками движка. В песочнице
# их нет, а полная сборка занимает десятки минут. Этот скрипт делает sparse-
# клон только с Source/Urho3D + Source/ThirdParty (~50 МБ, ~20 секунд) и
# докладывает три заглушки, без которых заголовки 1.9.0 не самодостаточны:
#
#   1. Source/Urho3D/Urho3D.h — в апстриме генерируется CMake
#      (generate_export_header). Пустой файл достаточно: проверяются типы.
#   2. Resource/XMLElement.h — не подключает pugixml, хотя использует
#      pugi::xpath_query в unique_ptr. GCC 12 требует полный тип.
#   3. Core/StringHashRegister.h — unique_ptr<Mutex> без include Core/Mutex.h.
#   4. Base/PrimitiveTypes.h — AllocatorFree/AllocatorUninitialize объявлены
#      в Container/Allocator.h, который не попадает в цепочку HashBase.h.
#
# Использование:
#   ./tools/restore_urho_syntax.sh            # в /tmp/urho_ci
#   URHO_CI_ROOT=~/urho ./tools/restore_urho_syntax.sh
#   URHO3D_HOME=/tmp/urho_ci/1.9.0 ./tools/syntax_check.sh
set -euo pipefail

R="${URHO_CI_ROOT:-/tmp/urho_ci}"
mkdir -p "$R"
cd "$R"

if [ ! -d 1.9.0/Source/Urho3D ]; then
    git clone --depth 1 --branch 1.9.0 --filter=blob:none --sparse \
        https://github.com/urho3d/Urho3D.git 1.9.0 >/dev/null 2>&1
fi

cd 1.9.0
git sparse-checkout set Source/Urho3D Source/ThirdParty >/dev/null 2>&1
# SDL_config.h в апстриме только .cmake — нужна хотя бы минимальная версия.
git sparse-checkout add Source/ThirdParty/SDL >/dev/null 2>&1 || true

mkdir -p include Source/Urho3D/ThirdParty
ln -sfn "$PWD/Source/Urho3D" include/Urho3D
ln -sfn "$PWD/Source/ThirdParty/SDL/include" Source/Urho3D/ThirdParty/SDL

printf '#pragma once\n#ifndef URHO3D_API\n#define URHO3D_API\n#endif\n' \
    > Source/Urho3D/Urho3D.h

if [ ! -f Source/ThirdParty/SDL/include/SDL_config.h ]; then
    cat > Source/ThirdParty/SDL/include/SDL_config.h <<'CFG'
/* Минимальный SDL_config.h для проверки синтаксиса (в сборке генерит CMake). */
#pragma once
#define HAVE_STDINT_H 1
#define HAVE_STDIO_H 1
#define HAVE_STRING_H 1
#define HAVE_MALLOC 1
#define HAVE_MEMORY_H 1
#define HAVE_STDLIB_H 1
CFG
fi

if [ -f Source/ThirdParty/PugiXml/src/pugixml.hpp ]; then
    ln -sfn "$PWD/Source/ThirdParty/PugiXml/src/pugixml.hpp" include/pugixml.hpp
    ln -sfn "$PWD/Source/ThirdParty/PugiXml/src/pugiconfig.hpp" include/pugiconfig.hpp
fi

python3 - <<'PY'
# Патчим ЛОКАЛЬНУЮ КОПИЮ движка (не репозиторий игры): заголовки 1.9.0
# не самодостаточны для GCC 12.
import io

def patch(path, anchor, insert, marker):
    try:
        s = io.open(path, encoding='utf-8', errors='ignore').read()
    except IOError:
        return
    if marker in s:
        return
    io.open(path, 'w', encoding='utf-8').write(s.replace(anchor, anchor + insert, 1))

patch('Source/Urho3D/Resource/XMLElement.h', '#include <memory>',
      '\n#include <pugixml.hpp>', 'pugixml.hpp')

patch('Source/Urho3D/Core/StringHashRegister.h', '#include "../Math/StringHash.h"',
      '\n#include "../Core/Mutex.h"', 'Core/Mutex.h')

patch('Source/Urho3D/Base/PrimitiveTypes.h', 'namespace Urho3D\n{',
      '\nstruct AllocatorBlock;\n'
      'void AllocatorFree(AllocatorBlock*, void*);\n'
      'void AllocatorUninitialize(AllocatorBlock*);\n', 'AllocatorFree')
PY

echo "URHO3D_HOME=$R/1.9.0"
echo "Теперь: URHO3D_HOME=$R/1.9.0 ./tools/syntax_check.sh"
