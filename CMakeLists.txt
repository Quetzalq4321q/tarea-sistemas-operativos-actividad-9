cmake_minimum_required(VERSION 3.16)
project(so_main_20240614T154818Z_001 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Threads REQUIRED)

function(add_independent_target target_name source_file)
    add_executable(${target_name} "${source_file}")
    target_link_libraries(${target_name} PRIVATE Threads::Threads)
    if (MSVC)
        target_compile_options(${target_name} PRIVATE /utf-8)
    else()
        target_compile_options(${target_name} PRIVATE -finput-charset=UTF-8 -fexec-charset=UTF-8)
    endif()
endfunction()

# Proyectos originales de so-main (cada archivo como ejecutable independiente)
add_independent_target(exclusionMutua00 "so-main/exclusion_mutua/exclusionMutua00.cpp")
add_independent_target(exclusionMutua01 "so-main/exclusion_mutua/exclusionMutua01.cpp")
add_independent_target(exclusionMutua02 "so-main/exclusion_mutua/exclusionMutua02.cpp")

add_independent_target(filosofos00 "so-main/filosofos_comensales/filosofos00.cpp")
add_independent_target(filosofos01 "so-main/filosofos_comensales/filosofos01.cpp")
add_independent_target(filosofos02 "so-main/filosofos_comensales/filosofos02.cpp")
add_independent_target(filosofos03 "so-main/filosofos_comensales/filosofos03.cpp")
add_independent_target(filosofos04 "so-main/filosofos_comensales/filosofos04.cpp")

add_independent_target(monitor00 "so-main/monitores/monitor00.cpp")
add_independent_target(monitor01 "so-main/monitores/monitor01.cpp")
add_independent_target(monitor02 "so-main/monitores/monitor02.cpp")
add_independent_target(monitor03 "so-main/monitores/monitor03.cpp")
add_independent_target(monitor04 "so-main/monitores/monitor04.cpp")

add_independent_target(productores00 "so-main/productor_consumidor/productores00.cpp")
add_independent_target(productores01 "so-main/productor_consumidor/productores01.cpp")
add_independent_target(productores02 "so-main/productor_consumidor/productores02.cpp")

add_independent_target(pthread_example "so-main/pthread/pthread-example.cpp")

# Proyectos completos en la raíz
add_independent_target(completo_1_original "Completo_1 Filosofos_Exclusion.cpp")
add_independent_target(completo_2_original "Completo_2 Filosofos_Exclusión.cpp")

# Subproyecto con todas las versiones mejoradas
add_subdirectory(UPGRADES)
