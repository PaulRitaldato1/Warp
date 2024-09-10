@echo off
mkdir build
cd build
cmake ../ -G "Visual Studio 2022"
mkdir Debug
mkdir Release
cd ..