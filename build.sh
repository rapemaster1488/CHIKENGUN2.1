#!/bin/bash

URHO3D="/home/larp/urho3d"
BUILD_DIR="build"
SRC="src/MyApp.cpp"

mkdir -p $BUILD_DIR

g++ $SRC \
    -o $BUILD_DIR/MyShooter \
    -std=c++17 \
    -I$URHO3D/build/include \
    -I$URHO3D/build/include/Urho3D/ThirdParty \
    -I$URHO3D/build/include/Urho3D/ThirdParty/SDL \
    -L$URHO3D/build/lib \
    -lUrho3D \
    -lGL \
    -lX11 \
    -lpthread \
    -lrt \
    -ldl \
    -lm
