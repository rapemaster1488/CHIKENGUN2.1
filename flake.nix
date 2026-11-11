{
  description = "CHIKENGUN 2.5 — ПЕРО СУДЬБЫ (Native C++17 / Urho3D 4K FPS Shooter)";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    urho3d-src = {
      url = "github:urho3d/Urho3D/1.9.0";
      flake = false;
    };
  };

  outputs = { self, nixpkgs, urho3d-src }:
    let
      supportedSystems = [
        "x86_64-linux"
        "aarch64-linux"
        "aarch64-darwin"
        "x86_64-darwin"
      ];
      forAllSystems = f:
        nixpkgs.lib.genAttrs supportedSystems (system:
          let
            pkgs = import nixpkgs { inherit system; };
          in
          f system pkgs);
    in
    {
      packages = forAllSystems (system: pkgs:
        let
          inherit (pkgs) lib stdenv;

          # Поддержка как современных (top-level libX11), так и старых (xorg.libX11) версий nixpkgs
          x11Libs = lib.optionals stdenv.isLinux [
            (pkgs.libX11 or pkgs.xorg.libX11)
            (pkgs.libXext or pkgs.xorg.libXext)
            (pkgs.libXrandr or pkgs.xorg.libXrandr)
            (pkgs.libXi or pkgs.xorg.libXi)
            (pkgs.libXcursor or pkgs.xorg.libXcursor)
            (pkgs.libXinerama or pkgs.xorg.libXinerama)
            (pkgs.libXxf86vm or pkgs.xorg.libXxf86vm)
          ];

          linuxGlLibs = lib.optionals stdenv.isLinux [
            (pkgs.libGL or pkgs.mesa)
            (pkgs.libGLU or pkgs.glu)
          ];

          linuxAudioAndSysLibs = lib.optionals stdenv.isLinux [
            pkgs.alsa-lib
            pkgs.libpulseaudio
            (pkgs.udev or pkgs.systemdLibs or pkgs.systemd)
            pkgs.dbus
            pkgs.libxkbcommon
          ];

          sdlPkg = pkgs.SDL2 or pkgs.sdl2-compat;

          darwinLibs = lib.optionals stdenv.isDarwin (
            [ sdlPkg pkgs.libiconv ]
            ++ (if pkgs ? apple-sdk then [ pkgs.apple-sdk ] else with pkgs.darwin.apple_sdk.frameworks; [
              Cocoa
              IOKit
              CoreAudio
              AudioUnit
              AudioToolbox
              CoreVideo
              QuartzCore
              ForceFeedback
              Carbon
              OpenGL
              Metal
              GameController
            ])
          );

          commonRuntimeLibs =
            [ sdlPkg ]
            ++ linuxGlLibs
            ++ x11Libs
            ++ linuxAudioAndSysLibs;

          # 1. Минимальный статический Urho3D 1.9.0 SDK (собирается оффлайн из flake input)
          urho3d = stdenv.mkDerivation {
            pname = "urho3d-minimal";
            version = "1.9.0";
            src = urho3d-src;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.pkg-config
              pkgs.git
              pkgs.python3
            ];

            buildInputs =
              (if stdenv.isLinux then commonRuntimeLibs else darwinLibs);

            # Старый код ThirdParty в Urho3D 1.9.0 может триггерить -Wformat-security в Nix hardening
            hardeningDisable = [ "format" ];

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DCMAKE_POLICY_VERSION_MINIMUM=3.5"
              "-DLIB_SUFFIX="
              "-DURHO3D_LIB_TYPE=STATIC"
              "-DURHO3D_C++11=1"
              "-DURHO3D_PCH=0"
              "-DURHO3D_SAMPLES=0"
              "-DURHO3D_TOOLS=0"
              "-DURHO3D_LUA=0"
              "-DURHO3D_LUAJIT=0"
              "-DURHO3D_ANGELSCRIPT=0"
              "-DURHO3D_NETWORK=0"
              "-DURHO3D_DATABASE_ODBC=0"
              "-DURHO3D_DATABASE_SQLITE=0"
              "-DURHO3D_URHO2D=0"
              "-DURHO3D_WEBP=0"
              "-DURHO3D_TESTING=0"
              "-DURHO3D_DOCS=0"
            ];

            # Флаги с пробелами передаём через cmakeFlagsArray, чтобы stdenv не разорвал их по словам
            preConfigure = ''
              cmakeFlagsArray+=(
                "-DCMAKE_C_FLAGS=-Wno-error -Wno-implicit-function-declaration -Wno-incompatible-pointer-types"
                "-DCMAKE_CXX_FLAGS=-include cstdint -Wno-error"
              )
            '';

            meta = with lib; {
              description = "Minimal static Urho3D 1.9.0 SDK for CHIKENGUN 2.5";
              homepage = "https://urho3d.io";
              license = licenses.mit;
              platforms = supportedSystems;
            };
          };

          # 2. Игровой клиент CHIKENGUN 2.5 + генератор 4K PBR текстур + модульные тесты
          chikengun = stdenv.mkDerivation {
            pname = "chikengun";
            version = "2.5.6";
            src = self;

            nativeBuildInputs = [
              pkgs.cmake
              pkgs.pkg-config
              pkgs.python3
              pkgs.makeWrapper
            ];

            buildInputs =
              [ urho3d ]
              ++ (if stdenv.isLinux then commonRuntimeLibs else darwinLibs);

            cmakeFlags = [
              "-DCMAKE_BUILD_TYPE=Release"
              "-DAUTO_FETCH_URHO3D=OFF"
              "-DBUILD_GAME=ON"
              "-DURHO3D_HOME=${urho3d}"
              "-DTEX_SIZE=1024"
            ];

            postBuild = ''
              if [ ! -f ../Data/Sounds/BanyaSteam.wav ] || [ ! -f ../Data/Sounds/Shot_Railgun.wav ]; then
                (cd .. && python3 tools/gen_sounds.py)
              fi
              ./gen_textures_4k ./generated_textures 1024
            '';

            doCheck = true;
            checkPhase = ''
              runHook preCheck
              ctest --output-on-failure
              runHook postCheck
            '';

            installPhase = ''
              runHook preInstall
              mkdir -p "$out/libexec/chikengun" "$out/bin" "$out/share/chikengun/Data/Textures" "$out/share/chikengun/CoreData"

              cp MyShooter "$out/libexec/chikengun/MyShooter"
              cp gen_textures_4k "$out/bin/chikengun-gen-textures"
              cp test_core "$out/bin/chikengun-test-core"

              # Сначала базовая CoreData из Urho3D (если есть), поверх — наша пропатченная CoreData (Cinema4K.glsl, SSAO4K.glsl и т.д.)
              for cdir in "${urho3d}/share/Urho3D/Resources/CoreData" "${urho3d}/share/Urho3D/CoreData" "$src/CoreData"; do
                if [ -d "$cdir" ]; then
                  cp -rf "$cdir"/. "$out/share/chikengun/CoreData"/
                  chmod -R u+w "$out/share/chikengun/CoreData"
                fi
              done

              for ddir in "${urho3d}/share/Urho3D/Resources/Data" "${urho3d}/share/Urho3D/Data" "$src/Data"; do
                if [ -d "$ddir" ]; then
                  cp -rf "$ddir"/. "$out/share/chikengun/Data"/
                  chmod -R u+w "$out/share/chikengun/Data"
                fi
              done

              cp -rf ./generated_textures/. "$out/share/chikengun/Data/Textures"/
              [ -f "$src/bin/settings.ini" ] && cp "$src/bin/settings.ini" "$out/share/chikengun/settings.ini"

              ${if stdenv.isLinux then ''
                makeWrapper "$out/libexec/chikengun/MyShooter" "$out/bin/ChikenGun" \
                  --set CHIKENGUN_DATA_DIR "$out/share/chikengun/Data" \
                  --set CHIKENGUN_COREDATA_DIR "$out/share/chikengun/CoreData" \
                  --set CHIKENGUN_RESOURCE_PREFIX_PATHS "$out/share/chikengun" \
                  --prefix LD_LIBRARY_PATH : "/run/opengl-driver/lib:/run/opengl-driver-32/lib:${lib.makeLibraryPath commonRuntimeLibs}"
              '' else ''
                makeWrapper "$out/libexec/chikengun/MyShooter" "$out/bin/ChikenGun" \
                  --set CHIKENGUN_DATA_DIR "$out/share/chikengun/Data" \
                  --set CHIKENGUN_COREDATA_DIR "$out/share/chikengun/CoreData" \
                  --set CHIKENGUN_RESOURCE_PREFIX_PATHS "$out/share/chikengun" \
                  --prefix DYLD_LIBRARY_PATH : "${lib.makeLibraryPath [ sdlPkg ]}"
              ''}
              ln -sf ChikenGun "$out/bin/MyShooter"

              ${lib.optionalString stdenv.isLinux ''
                mkdir -p "$out/share/applications"
                printf '%s\n' \
                  '[Desktop Entry]' \
                  'Type=Application' \
                  'Name=CHIKENGUN 2.5 — Перо Судьбы' \
                  'Comment=Native C++17 / Urho3D 4K FPS Shooter with Banya, Casino & Pixel Arcade' \
                  "Exec=$out/bin/ChikenGun" \
                  'Terminal=false' \
                  'Categories=Game;ActionGame;' \
                  > "$out/share/applications/chikengun.desktop"
              ''}
              runHook postInstall
            '';

            meta = with lib; {
              description = "CHIKENGUN 2.5 — ПЕРО СУДЬБЫ (Native C++17 / Urho3D 4K FPS Shooter)";
              homepage = "https://github.com/rapemaster1488/CHIKENGUN2.1";
              license = licenses.mit;
              mainProgram = "ChikenGun";
              platforms = supportedSystems;
            };
          };
        in
        {
          default = chikengun;
          inherit chikengun urho3d;
        });

      apps = forAllSystems (system: pkgs: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/ChikenGun";
          meta.description = "Launch CHIKENGUN 2.5 — ПЕРО СУДЬБЫ";
        };
      });

      devShells = forAllSystems (system: pkgs:
        let
          inherit (pkgs) lib stdenv;
          urho3d = self.packages.${system}.urho3d;
          chikengun = self.packages.${system}.chikengun;
          sdlPkg = pkgs.SDL2 or pkgs.sdl2-compat;
          x11Libs = lib.optionals stdenv.isLinux [
            (pkgs.libX11 or pkgs.xorg.libX11)
            (pkgs.libXext or pkgs.xorg.libXext)
            (pkgs.libXrandr or pkgs.xorg.libXrandr)
            (pkgs.libXi or pkgs.xorg.libXi)
            (pkgs.libXcursor or pkgs.xorg.libXcursor)
            (pkgs.libXinerama or pkgs.xorg.libXinerama)
            (pkgs.libXxf86vm or pkgs.xorg.libXxf86vm)
          ];
          runtimeLibs = [ sdlPkg ] ++ lib.optionals stdenv.isLinux ([
            (pkgs.libGL or pkgs.mesa)
            (pkgs.libGLU or pkgs.glu)
            pkgs.alsa-lib
            pkgs.libpulseaudio
            (pkgs.udev or pkgs.systemdLibs or pkgs.systemd)
            pkgs.dbus
            pkgs.libxkbcommon
          ] ++ x11Libs);
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ chikengun ];
            packages = [
              pkgs.ninja
              pkgs.gnumake
              pkgs.git
            ];

            URHO3D_HOME = "${urho3d}";
            CMAKE_PREFIX_PATH = "${urho3d}";

            shellHook = ''
              export URHO3D_HOME="${urho3d}"
              ${if stdenv.isLinux then ''
                export LD_LIBRARY_PATH="/run/opengl-driver/lib:/run/opengl-driver-32/lib:${lib.makeLibraryPath runtimeLibs}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
              '' else ''
                export DYLD_LIBRARY_PATH="${lib.makeLibraryPath [ sdlPkg ]}''${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
              ''}
              echo "CHIKENGUN 2.5 Nix devShell ready (Urho3D 1.9.0 SDK: $URHO3D_HOME)"
              echo "  ./run.sh                 # Build & play immediately"
              echo "  ./build.sh --4k          # Build with full 4096x4096 PBR textures"
              echo "  ./build.sh --tests-only  # Run C++17 unit test suite"
            '';
          };
        });
    };
}
