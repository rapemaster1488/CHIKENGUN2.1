# Classic nix-shell environment for systems without Nix Flakes enabled.
# Usage:
#   nix-shell --run ./run.sh
#   nix-shell --run "./build.sh --tests-only"
{ pkgs ? import <nixpkgs> {} }:

let
  inherit (pkgs) lib stdenv;
  sdlPkg = pkgs.SDL2 or pkgs.sdl2-compat;

  x11Libs = lib.optionals stdenv.isLinux [
    (pkgs.libX11 or pkgs.xorg.libX11)
    (pkgs.libXext or pkgs.xorg.libXext)
    (pkgs.libXrandr or pkgs.xorg.libXrandr)
    (pkgs.libXi or pkgs.xorg.libXi)
    (pkgs.libXcursor or pkgs.xorg.libXcursor)
    (pkgs.libXinerama or pkgs.xorg.libXinerama)
    (pkgs.libXxf86vm or pkgs.xorg.libXxf86vm)
    (pkgs.libXrender or pkgs.xorg.libXrender)
    (pkgs.libXfixes or pkgs.xorg.libXfixes)
    (pkgs.libxcb or pkgs.xorg.libxcb)
  ];

  linuxLibs = lib.optionals stdenv.isLinux ([
    (pkgs.libGL or pkgs.mesa)
    (pkgs.libGLU or pkgs.glu)
    pkgs.alsa-lib
    pkgs.libpulseaudio
    (pkgs.udev or pkgs.systemdLibs or pkgs.systemd)
    pkgs.libxkbcommon
  ] ++ x11Libs);

  darwinLibs = lib.optionals stdenv.isDarwin (
    [ pkgs.libiconv ]
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

  runtimeLibs = [ sdlPkg ] ++ linuxLibs;
in
pkgs.mkShell {
  nativeBuildInputs = [
    pkgs.cmake
    pkgs.ninja
    pkgs.gnumake
    pkgs.pkg-config
    pkgs.git
    pkgs.python3
  ];

  buildInputs = runtimeLibs ++ darwinLibs;

  shellHook = ''
    ${if stdenv.isLinux then ''
      export LD_LIBRARY_PATH="/run/opengl-driver/lib:/run/opengl-driver-32/lib:${lib.makeLibraryPath runtimeLibs}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    '' else ''
      export DYLD_LIBRARY_PATH="${lib.makeLibraryPath [ sdlPkg ]}''${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
    ''}
    echo "CHIKENGUN 2.5 nix-shell ready. Run ./run.sh (builds cached static Urho3D on first run) or ./build.sh --tests-only."
  '';
}
