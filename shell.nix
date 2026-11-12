# Classic nix-shell environment for systems without Nix Flakes enabled.
# Usage:
#   nix-shell --run ./run.sh
#   nix-shell --run "./build.sh --tests-only"
{ pkgs ? import <nixpkgs> {} }:

let
  inherit (pkgs) lib stdenv;
  sdlPkg = pkgs.SDL2 or pkgs.sdl2-compat;

  optPkg = names:
    let
      found = lib.findFirst (n: pkgs ? ${n} || (pkgs ? xorg && pkgs.xorg ? ${n})) null names;
    in
    if found == null then [ ]
    else [ (if pkgs ? ${found} then pkgs.${found} else pkgs.xorg.${found}) ];

  x11Libs = lib.optionals stdenv.isLinux (
    optPkg [ "libX11" ]
    ++ optPkg [ "libXext" ]
    ++ optPkg [ "libXrandr" ]
    ++ optPkg [ "libXi" ]
    ++ optPkg [ "libXcursor" ]
    ++ optPkg [ "libXinerama" ]
    ++ optPkg [ "libXxf86vm" ]
    ++ optPkg [ "libXrender" ]
    ++ optPkg [ "libXfixes" ]
    ++ optPkg [ "libXScrnSaver" ]
    ++ optPkg [ "libxcb" ]
  );

  linuxLibs = lib.optionals stdenv.isLinux ([
    (pkgs.libGL or pkgs.mesa)
    (pkgs.libGLU or pkgs.glu)
    pkgs.alsa-lib
    pkgs.libpulseaudio
    (pkgs.udev or pkgs.systemdLibs or pkgs.systemd)
    pkgs.dbus
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
    echo "CHIKENGUN 2.5 nix-shell ready. Run ./run.sh (uses local Urho3D or builds cached static Urho3D) or ./build.sh --tests-only."
  '';
}
