# SPDX-FileCopyrightText: 2026 Pedro Soto
# SPDX-License-Identifier: GPL-3.0-or-later
cmake_minimum_required(VERSION 3.20)
project(downhill CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- Visor opcional (SDL3 + OpenGL). Los tests NO dependen de SDL3: una máquina
# o CI sin SDL3 puede configurar, compilar y ejecutar toda la suite igualmente. ---
option(DH_BUILD_VIEWER "Construir el visor dhview (SDL3 + OpenGL)" ON)
option(DH_FETCH_SDL3 "Descargar SDL3 con FetchContent si no hay instalación del sistema" OFF)
set(DH_SDL3_TAG "release-3.4.18" CACHE STRING "Etiqueta de SDL3 para FetchContent (fijada)")
option(DH_SANITIZE "Compilar con AddressSanitizer y UndefinedBehaviorSanitizer (GCC/Clang)" OFF)

if(DH_SANITIZE AND NOT MSVC)
  add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
  add_link_options(-fsanitize=address,undefined)
elseif(DH_SANITIZE AND MSVC)
  message(WARNING "DH_SANITIZE se ignora con MSVC en esta configuración mínima.")
endif()

if(DH_BUILD_VIEWER)
  find_package(SDL3 QUIET)
  if(NOT TARGET SDL3::SDL3 AND DH_FETCH_SDL3)
    include(FetchContent)
    message(STATUS "SDL3 no encontrado: FetchContent ${DH_SDL3_TAG}")
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG ${DH_SDL3_TAG}
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
  endif()
  if(TARGET SDL3::SDL3)
    find_package(OpenGL REQUIRED)
    add_executable(dhview src/main.cpp)
    target_link_libraries(dhview PRIVATE SDL3::SDL3 OpenGL::GL)
    # Empaquetado: `cpack -G DEB` (Linux) / ZIP (Windows, vía mingw-w64 o MSVC). No incluye datos del juego:
    # el usuario provee su propia ISO y la app extrae los assets en el primer arranque.
    install(TARGETS dhview RUNTIME DESTINATION bin)
  else()
    message(WARNING "SDL3 no encontrado: no se construye dhview. Instala SDL3 o usa -DDH_FETCH_SDL3=ON. Los tests sí se construyen.")
  endif()
endif()

set(CPACK_PACKAGE_NAME downhill-port)
set(CPACK_PACKAGE_VERSION 0.0.1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Community native PC port of Downhill Domination / Port nativo comunitario de Downhill Domination")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "dev")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
include(CPack)

enable_testing()
add_executable(ground_test tests/ground_test.cpp)
add_test(NAME ground_test COMMAND ground_test)
add_executable(contact_test tests/contact_test.cpp)
add_test(NAME contact_test COMMAND contact_test)
add_executable(ground_validate tests/ground_validate.cpp)
add_executable(ride_demo tests/ride_demo.cpp)
add_executable(sweep_cli tests/sweep_cli.cpp)
add_executable(ngp_col_check tests/ngp_col_check.cpp)
add_executable(pts_check tests/pts_check.cpp)          # lector .PTS nativo; sin argumentos corre el test sintético
add_test(NAME pts_check COMMAND pts_check)
add_executable(bike_test tests/bike_test.cpp)
add_test(NAME bike_test COMMAND bike_test)
add_executable(race_test tests/race_test.cpp)
add_test(NAME race_test COMMAND race_test)
add_executable(controls_test tests/controls_test.cpp)
add_test(NAME controls_test COMMAND controls_test)
add_executable(text_test tests/text_test.cpp)
add_test(NAME text_test COMMAND text_test)
add_executable(hud_test tests/hud_test.cpp)
add_test(NAME hud_test COMMAND hud_test)
add_executable(loadscreen_test tests/loadscreen_test.cpp)
add_test(NAME loadscreen_test COMMAND loadscreen_test)
add_executable(bike_demo tests/bike_demo.cpp)

add_executable(integrator_test tests/integrator_test.cpp)
add_test(NAME integrator_test COMMAND integrator_test)

add_executable(rider_test tests/rider_test.cpp)
add_test(NAME rider_test COMMAND rider_test)
add_executable(rider_mesh_test tests/rider_mesh_test.cpp)
add_test(NAME rider_mesh_test COMMAND rider_mesh_test)
add_executable(rider_mesh_check tests/rider_mesh_check.cpp)

add_executable(nga_test tests/nga_test.cpp)
add_test(NAME nga_test COMMAND nga_test)

# Oráculo de vuelo libre: usa un volcado numérico fuera del repo (DH_AIR_REF); sin él, SKIP.
add_executable(air_replay_check tests/air_replay_check.cpp)
add_test(NAME air_replay_check COMMAND air_replay_check $ENV{DH_AIR_REF})
add_executable(impact_replay_check tests/impact_replay_check.cpp)   # oráculo de impacto, solo manual (necesita nivel.col y volcado fuera del repo)
