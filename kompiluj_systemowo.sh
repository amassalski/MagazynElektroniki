#!/bin/bash
echo "=== KOMPILACJA Z SYSTEMOWYMI BIBLIOTEKAMI ==="

# Utwórz prosty CMakeLists.txt
cat > CMakeLists_simple.txt << 'CMAKEEND'
cmake_minimum_required(VERSION 3.16)
project(DomowyMagazyn)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "\${CMAKE_CXX_FLAGS} -DwxDEBUG_LEVEL=0 -DNDEBUG")

file(GLOB_RECURSE SRC_FILES "src/*.cpp")
add_executable(\${PROJECT_NAME} WIN32 \${SRC_FILES})

# Użyj systemowych include
target_include_directories(\${PROJECT_NAME} PRIVATE
    src
    /mingw64/include/wx-3.2
    /mingw64/lib/wx/include/x86_64-w64-mingw32-msw-unicode-3.2
    /mingw64/include/postgresql/server
)

target_compile_definitions(\${PROJECT_NAME} PRIVATE
    UNICODE _UNICODE __WXMSW__
)

# Linkowanie z systemowymi bibliotekami
target_link_libraries(\${PROJECT_NAME}
    -lwx_mswu_core-3.2
    -lwx_baseu-3.2
    -lwx_mswu_adv-3.2
    pq
    -luxtheme
    -loleacc
    -lpng
    -lz
    -lversion
    -mwindows
)
CMAKEEND

mv CMakeLists_simple.txt CMakeLists.txt

# Kompiluj
rm -rf build
cmake -B build -G "Ninja"
cmake --build build --config Release

if [ $? -eq 0 ]; then
    echo "SUKCES! build/DomowyMagazyn.exe"
else
    echo "BŁĄD"
fi
