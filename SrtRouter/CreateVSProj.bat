mkdir build
cd build

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Professional\VC\Auxiliary\Build\vcvars64.bat"
call "C:\BuildTools2019\VC\Auxiliary\Build\vcvars64.bat"

cmake -G "Visual Studio 16"  -DCMAKE_GENERATOR_PLATFORM=x64 -DCMAKE_SYSTEM_VERSION=8.1 -DCMAKE_BUILD_TYPE=Release ..