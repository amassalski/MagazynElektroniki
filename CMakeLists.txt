cmake_minimum_required(VERSION 3.16)
project(DomowyMagazyn VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Konsola do debugowania (cmake .. -DDM_CONSOLE=ON)
option(DM_CONSOLE "Buduj z oknem konsoli (debug)" OFF)
option(DM_BUILD_TESTS "Buduj testy warstwy bazy danych (wymagają PostgreSQL)" OFF)

# --- Zależności ---------------------------------------------------------
# --- wxWidgets przez wx-config (z poprawką ścieżek MSYS2) ---
find_program(WX_CONFIG NAMES wx-config wx-config-3.2 REQUIRED)
get_filename_component(MINGW_BIN "${CMAKE_CXX_COMPILER}" DIRECTORY)
get_filename_component(MINGW_ROOT "${MINGW_BIN}" DIRECTORY)   # np. G:/MSYS2/mingw64

execute_process(COMMAND sh "${WX_CONFIG}" --cxxflags
                OUTPUT_VARIABLE WX_CXXFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
execute_process(COMMAND sh "${WX_CONFIG}" --libs html,core,base,net
                OUTPUT_VARIABLE WX_LIBS OUTPUT_STRIP_TRAILING_WHITESPACE)

string(REPLACE "/mingw64/" "${MINGW_ROOT}/" WX_CXXFLAGS "${WX_CXXFLAGS}")
string(REPLACE "/mingw64/" "${MINGW_ROOT}/" WX_LIBS     "${WX_LIBS}")
separate_arguments(WX_CXXFLAGS NATIVE_COMMAND "${WX_CXXFLAGS}")
separate_arguments(WX_LIBS     NATIVE_COMMAND "${WX_LIBS}")

find_package(PostgreSQL REQUIRED)
find_package(Threads REQUIRED)
find_package(Gettext)

# --- Źródła -------------------------------------------------------------
file(GLOB_RECURSE ALL_SRC CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

# --- Kamera USB (opcjonalnie, OpenCV: tylko moduły core i videoio) ----------------
# DM_KAMERA: AUTO (domyślnie: jeśli OpenCV jest dostępne), ON (wymagaj) albo OFF (bez kamery)
set(DM_KAMERA "AUTO" CACHE STRING "Obsługa kamery USB przez OpenCV: AUTO, ON albo OFF")
set(DM_KAMERA_WLACZONA OFF)
set(DM_KAMERA_INC "")
set(DM_KAMERA_LIBS "")
if(NOT DM_KAMERA STREQUAL "OFF")
    # Menedżery pakietów na macOS (MacPorts: /opt/local, Homebrew: /opt/homebrew i /usr/local) nie są w domyślnych
    # ścieżkach CMake, a MacPorts trzyma pliki OpenCV w nietypowych miejscach (libexec/opencv4/cmake, include/opencv4).
    set(DM_PREFIXY "")
    foreach(p /opt/local /opt/homebrew /usr/local)
        if(EXISTS "${p}")
            list(APPEND DM_PREFIXY "${p}")
        endif()
    endforeach()
    set(DM_OPENCV_CMAKE_PODPOWIEDZI "")
    foreach(p ${DM_PREFIXY})
        list(APPEND DM_OPENCV_CMAKE_PODPOWIEDZI
             "${p}/libexec/opencv4/cmake" "${p}/libexec/opencv3/cmake"
             "${p}/opt/opencv/lib/cmake/opencv4" "${p}/lib/cmake/opencv4")
    endforeach()
    find_package(OpenCV QUIET COMPONENTS core videoio HINTS ${DM_OPENCV_CMAKE_PODPOWIEDZI})
    if(OpenCV_FOUND)
        set(DM_KAMERA_WLACZONA ON)
        set(DM_KAMERA_INC ${OpenCV_INCLUDE_DIRS})
        set(DM_KAMERA_LIBS ${OpenCV_LIBS})
    else()
        set(DM_OPENCV_INC_PODPOWIEDZI "")
        set(DM_OPENCV_LIB_PODPOWIEDZI "")
        foreach(p ${DM_PREFIXY})
            list(APPEND DM_OPENCV_INC_PODPOWIEDZI "${p}/include/opencv4" "${p}/include/opencv" "${p}/include")
            list(APPEND DM_OPENCV_LIB_PODPOWIEDZI "${p}/lib")
        endforeach()
        find_path(DM_OPENCV_INC opencv2/videoio.hpp HINTS ${DM_OPENCV_INC_PODPOWIEDZI} PATH_SUFFIXES opencv4 opencv)
        find_library(DM_OPENCV_CORE NAMES opencv_core HINTS ${DM_OPENCV_LIB_PODPOWIEDZI})
        find_library(DM_OPENCV_VIDEOIO NAMES opencv_videoio HINTS ${DM_OPENCV_LIB_PODPOWIEDZI})
        if(DM_OPENCV_INC AND DM_OPENCV_CORE AND DM_OPENCV_VIDEOIO)
            set(DM_KAMERA_WLACZONA ON)
            set(DM_KAMERA_INC ${DM_OPENCV_INC})
            set(DM_KAMERA_LIBS ${DM_OPENCV_VIDEOIO} ${DM_OPENCV_CORE})
        endif()
    endif()
    if(DM_KAMERA STREQUAL "ON" AND NOT DM_KAMERA_WLACZONA)
        message(FATAL_ERROR "DM_KAMERA=ON, ale nie znaleziono OpenCV (core, videoio). Podaj -DOpenCV_DIR=<katalog z OpenCVConfig.cmake> albo -DCMAKE_PREFIX_PATH=<prefiks>")
    endif()
endif()
message(STATUS "Kamera USB (OpenCV): ${DM_KAMERA_WLACZONA}")
function(dm_kamera cel)
    if(DM_KAMERA_WLACZONA)
        target_compile_definitions(${cel} PRIVATE DM_KAMERA=1)
        target_include_directories(${cel} PRIVATE ${DM_KAMERA_INC})
        target_link_libraries(${cel} PRIVATE ${DM_KAMERA_LIBS})
    endif()
endfunction()

add_executable(${PROJECT_NAME} ${ALL_SRC})
dm_kamera(${PROJECT_NAME})

# macOS: wx-config zwraca "-framework Cocoa -framework IOKit ...", a po rozbiciu na słowa
# CMake traktowałby nazwy frameworków jak biblioteki (-lIOKit). Dlatego frameworki
# podajemy jawnie, a ze zmiennej WX_LIBS usuwamy ich nazwy.
set(DM_WX_FRAMEWORKS "")
if(APPLE)
    set(DM_WX_FRAMEWORKS IOKit Carbon Cocoa QuartzCore AudioToolbox System OpenGL)
    foreach(FRAMEWORK IN LISTS DM_WX_FRAMEWORKS)
        target_link_options(${PROJECT_NAME} PRIVATE "SHELL:-framework ${FRAMEWORK}")
    endforeach()
    list(FILTER WX_LIBS EXCLUDE REGEX "^-framework$")
    list(FILTER WX_LIBS EXCLUDE REGEX "^(IOKit|Carbon|Cocoa|QuartzCore|AudioToolbox|System|OpenGL)$")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE UNICODE _UNICODE
    DM_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE ${WX_CXXFLAGS})
endif()

target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src")

target_link_libraries(${PROJECT_NAME} PRIVATE ${WX_LIBS} PostgreSQL::PostgreSQL Threads::Threads)

if(WIN32 AND NOT DM_CONSOLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# wx-config dodaje -mwindows (brak konsoli); przy DM_CONSOLE=ON wymuszamy konsolę
if(WIN32 AND DM_CONSOLE AND NOT MSVC)
    target_link_options(${PROJECT_NAME} PRIVATE -mconsole)
endif()

# --- Skrypty SQL obok programu ------------------------------------------
# Program szuka skryptów w katalogu "sql" obok pliku exe.
add_custom_target(dm_sql ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/sql" "${CMAKE_CURRENT_BINARY_DIR}/sql"
    COMMENT "Kopiowanie skryptów SQL")
add_dependencies(${PROJECT_NAME} dm_sql)

# --- Biblioteki DLL (MSYS2/MinGW) -----------------------------------------
# Kopiuje do katalogu z exe wszystkie DLL z /mingw64, których program używa, żeby
# dało się go uruchomić dwuklikiem. Zawsze nadpisuje (cp -f), więc stare DLL
# w katalogu build nie zasłaniają świeżo zbudowanych.
find_program(DM_LDD ldd)
if(WIN32 AND MINGW AND DM_LDD)
    option(DM_KOPIUJ_DLL "Po zbudowaniu skopiuj biblioteki DLL obok exe" ON)
    if(DM_KOPIUJ_DLL)
        add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
            COMMAND sh -c "ldd '$<TARGET_FILE:${PROJECT_NAME}>' | grep -i '/mingw64/' | awk '{print $3}' | xargs -I{} cp -f {} '$<TARGET_FILE_DIR:${PROJECT_NAME}>' || true"
            COMMENT "Kopiowanie bibliotek DLL"
            VERBATIM)
    endif()
endif()

# --- Testy warstwy bazy danych (bez wxWidgets) -----------------------------
if(DM_BUILD_TESTS)
    enable_testing()
    add_executable(test_bazy
        tests/test_bazy.cpp
        src/BazaDanych/PolaczenieBazy.cpp
        src/BazaDanych/ManagerBazyDanych.cpp
        src/Narzedzia/Konfiguracja.cpp
        src/Narzedzia/Liczby.cpp
        src/Narzedzia/Miejsce.cpp
        src/Logika/SzeregiE.cpp
        src/Logika/ImportCSVElementow.cpp
        src/Logika/ImportElementow.cpp
        src/Logika/KatalogCSV.cpp
        src/Logika/LogikaKatalogu.cpp
        src/Logika/LogikaMagazynu.cpp
        src/Eksport/EksportCSV.cpp
        src/Import/ImportKategorii.cpp
        src/Import/ParserCSV.cpp
        src/Import/MiniXML.cpp
        src/Import/ImportBOM.cpp
        src/Import/ImportKiCad.cpp
        src/Import/ImportEagle.cpp
        src/Logika/DopasowanieBOM.cpp
        src/Logika/LogikaProjektow.cpp
        src/Logika/LogikaSklepow.cpp
        src/Logika/LogikaRaportow.cpp
        src/Pomoc/PomocKontekstowa.cpp
        src/Eksport/EksportRaport.cpp)
    target_include_directories(test_bazy PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(test_bazy PRIVATE PostgreSQL::PostgreSQL)
    # Parametry połączenia: zmienne środowiskowe DM_TEST_HOST, DM_TEST_PORT, DM_TEST_DB,
    # DM_TEST_USER, DM_TEST_PASSWORD. Skrypty SQL: DM_TEST_SQL.
    add_test(NAME test_bazy COMMAND test_bazy)

    # Parsery list części (CSV, KiCad, Eagle): bez bazy danych
    add_executable(test_import
        tests/test_import.cpp
        src/Import/ParserCSV.cpp
        src/Import/MiniXML.cpp
        src/Import/ImportBOM.cpp
        src/Import/ImportKiCad.cpp
        src/Import/ImportEagle.cpp)
    target_include_directories(test_import PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_import COMMAND test_import)

    # Katalog elementów z CSV: bez bazy danych
    add_executable(test_katalog tests/test_katalog.cpp src/Logika/KatalogCSV.cpp src/Import/ParserCSV.cpp)
    target_include_directories(test_katalog PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_katalog COMMAND test_katalog)

    # Odczyt parametrów z tekstu datasheetu (heurystyka): bez bazy danych
    add_executable(test_datasheet tests/test_datasheet.cpp src/Logika/OdczytDatasheetu.cpp)
    target_include_directories(test_datasheet PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_datasheet COMMAND test_datasheet)

    # Sprawdzanie aktualizacji (porównanie wersji, plik wersja.txt): bez bazy danych
    add_executable(test_aktualizacje tests/test_aktualizacje.cpp src/Logika/Aktualizacje.cpp)
    target_include_directories(test_aktualizacje PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_aktualizacje COMMAND test_aktualizacje)

    # Kalkulatory (dzielnik, rezystor LED, filtr RC): bez bazy danych
    add_executable(test_kalkulatory tests/test_kalkulatory.cpp src/Logika/Kalkulatory.cpp src/Logika/SzeregiE.cpp
        src/Narzedzia/Liczby.cpp src/Narzedzia/Miejsce.cpp)
    target_include_directories(test_kalkulatory PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_kalkulatory COMMAND test_kalkulatory)

    # Integracja z Eagle (skrypt atrybutów, ULP): bez bazy danych
    add_executable(test_eagle tests/test_eagle.cpp
        src/Eksport/EksportEagle.cpp src/Import/ParserCSV.cpp src/Import/MiniXML.cpp src/Import/ImportBOM.cpp
        src/Import/ImportKiCad.cpp src/Import/ImportEagle.cpp)
    target_include_directories(test_eagle PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_eagle COMMAND test_eagle)

    # Szeregi E6-E96, zapis wartości RKM, generator i import elementów z CSV: bez bazy danych
    add_executable(test_szeregi tests/test_szeregi.cpp
        src/Logika/SzeregiE.cpp src/Logika/ImportCSVElementow.cpp src/Import/ParserCSV.cpp
        src/Narzedzia/Liczby.cpp src/Narzedzia/Miejsce.cpp)
    target_include_directories(test_szeregi PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_szeregi COMMAND test_szeregi)

    # Dekoder pasków kolorów i kodów SMD: bez bazy danych
    add_executable(test_kody_elementow tests/test_kody_elementow.cpp
        src/Logika/KodyElementow.cpp src/Logika/SzeregiE.cpp src/Narzedzia/Liczby.cpp src/Narzedzia/Miejsce.cpp)
    target_include_directories(test_kody_elementow PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_kody_elementow COMMAND test_kody_elementow)

    # Kody kreskowe (EAN-13, EAN-8, Code128): bez bazy danych
    add_executable(test_kody tests/test_kody.cpp src/Narzedzia/KodKreskowy.cpp src/Eksport/DrukujEtykiety.cpp)
    target_include_directories(test_kody PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_kody COMMAND test_kody)


    # Zestawienie zakupów i parsery stron sklepów: bez bazy i bez sieci
    add_executable(test_plan tests/test_plan.cpp src/Logika/PlanZakupow.cpp)
    target_include_directories(test_plan PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_plan COMMAND test_plan)

    add_executable(test_strony tests/test_strony.cpp src/Wyszukiwanie/ParserStrony.cpp src/Wyszukiwanie/MiniJSON.cpp)
    target_include_directories(test_strony PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_test(NAME test_strony COMMAND test_strony)

    # Klient HTTP, sprawdzanie cen, datasheety: lokalny serwer testowy na gniazdach POSIX + baza danych
    if(NOT WIN32)
        add_executable(test_siec EXCLUDE_FROM_ALL tests/test_siec.cpp
            src/Wyszukiwanie/KlientHTTP.cpp src/Wyszukiwanie/SprawdzanieCen.cpp
            src/Wyszukiwanie/WyszukiwanieDatasheet.cpp src/Wyszukiwanie/ParserStrony.cpp src/Wyszukiwanie/MiniJSON.cpp
            src/Logika/Aktualizacje.cpp src/Logika/LogikaSklepow.cpp src/Logika/LogikaMagazynu.cpp src/BazaDanych/PolaczenieBazy.cpp
            src/BazaDanych/ManagerBazyDanych.cpp src/Narzedzia/Konfiguracja.cpp src/Narzedzia/Liczby.cpp
            src/Eksport/EksportCSV.cpp src/Import/ImportKategorii.cpp src/Import/ParserCSV.cpp)
        target_compile_options(test_siec PRIVATE ${WX_CXXFLAGS})
        target_include_directories(test_siec PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
        target_link_libraries(test_siec PRIVATE ${WX_LIBS} PostgreSQL::PostgreSQL Threads::Threads)
        foreach(FRAMEWORK IN LISTS DM_WX_FRAMEWORKS)
            target_link_options(test_siec PRIVATE "SHELL:-framework ${FRAMEWORK}")
        endforeach()
        add_test(NAME test_siec COMMAND test_siec)
        set_tests_properties(test_siec PROPERTIES ENVIRONMENT "DM_TEST_SQL=${CMAKE_CURRENT_SOURCE_DIR}/sql")
    endif()

    # Test okien (wymaga ekranu; na serwerze: xvfb-run ctest)
    set(DM_SRC_BEZ_MAIN ${ALL_SRC})
    list(REMOVE_ITEM DM_SRC_BEZ_MAIN "${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp")
    add_executable(test_gui EXCLUDE_FROM_ALL tests/test_gui.cpp ${DM_SRC_BEZ_MAIN})
    target_compile_definitions(test_gui PRIVATE UNICODE _UNICODE DM_VERSION="${PROJECT_VERSION}")
    target_compile_options(test_gui PRIVATE ${WX_CXXFLAGS})
    target_include_directories(test_gui PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(test_gui PRIVATE ${WX_LIBS} PostgreSQL::PostgreSQL Threads::Threads)
    foreach(FRAMEWORK IN LISTS DM_WX_FRAMEWORKS)
        target_link_options(test_gui PRIVATE "SHELL:-framework ${FRAMEWORK}")
    endforeach()
    dm_kamera(test_gui)
    add_test(NAME test_gui COMMAND test_gui)
    set_tests_properties(test_bazy test_gui PROPERTIES
        ENVIRONMENT "DM_TEST_SQL=${CMAKE_CURRENT_SOURCE_DIR}/sql;DM_TEST_POMOC=${CMAKE_CURRENT_SOURCE_DIR}/pomoc/pola.txt;DM_POMOC=${CMAKE_CURRENT_SOURCE_DIR}/pomoc")

    # Pomoc HTML i opisy pól: aktualność wygenerowanych plików i spójność odnośników
    find_program(DM_PYTHON NAMES python3 python)
    if(DM_PYTHON)
        add_test(NAME test_pomoc_html
                 COMMAND "${DM_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/narzedzia/zbuduj_pomoc.py" --sprawdz)
    endif()
endif()

# --- Tłumaczenia --------------------------------------------------------
set(PL_PO "${CMAKE_CURRENT_SOURCE_DIR}/lang/pl.po")

if(GETTEXT_MSGFMT_EXECUTABLE AND EXISTS "${PL_PO}")
    set(PL_MO_DIR "${CMAKE_CURRENT_BINARY_DIR}/pl/LC_MESSAGES")
    set(PL_MO "${PL_MO_DIR}/DomowyMagazyn.mo")

    add_custom_command(
        OUTPUT  "${PL_MO}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${PL_MO_DIR}"
        COMMAND ${GETTEXT_MSGFMT_EXECUTABLE} -o "${PL_MO}" "${PL_PO}"
        DEPENDS "${PL_PO}"
        COMMENT "Kompilacja tłumaczeń polskich")

    add_custom_target(translations ALL DEPENDS "${PL_MO}")
    add_dependencies(${PROJECT_NAME} translations)

    install(FILES "${PL_MO}" DESTINATION pl/LC_MESSAGES)
else()
    message(STATUS "Tłumaczenia pominięte (brak msgfmt lub lang/pl.po)")
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)
install(FILES pomoc/pomoc_pl.htm pomoc/pola.txt DESTINATION pomoc)