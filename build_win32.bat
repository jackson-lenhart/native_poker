@echo off

mkdir .\build
pushd .\build
cl -FC -Zi C:\projects\native_poker\win32_poker.cpp user32.lib gdi32.lib
popd