# Configuration universelle : Utilise le compilateur ARM d'openSUSE sous Linux
if(WIN32)
    set(PICO_TOOLCHAIN_PATH "$ENV{HOME}/.pico-sdk/toolchain/14_2_Rel1" CACHE PATH "Path to toolchain")
else()
    # Sous Linux, openSUSE gère le compilateur globalement, on désactive le chemin en dur du SDK
    set(PICO_TOOLCHAIN_PATH "/usr" CACHE PATH "Path to toolchain")
endif()

# Configuration absolue des compilateurs openSUSE (/opt/cross)
set(CMAKE_C_COMPILER "/opt/cross/bin/arm-none-eabi-gcc" CACHE PATH "" FORCE)
set(CMAKE_ASM_COMPILER "/opt/cross/bin/arm-none-eabi-gcc" CACHE PATH "" FORCE)
set(CMAKE_CXX_COMPILER "/opt/cross/bin/arm-none-eabi-g++" CACHE PATH "" FORCE)
set(CMAKE_MAKE_PROGRAM "ninja" CACHE INTERNAL "" FORCE)




cmake_minimum_required(VERSION 3.13)



set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD waveshare_rp2040_pizero CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
# == Gardez vos lignes de début intactes jusqu'à pico_sdk_import.cmake ==
include(pico_sdk_import.cmake)

# --- AJOUTEZ CES DEUX LIGNES ICI ---
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_WORKS TRUE)
# -----------------------------------

# Déclaration du projet
project(PSNEERP C CXX ASM)



# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:-include;cstddef>")

if(NOT WIN32)
    include_directories(SYSTEM "/usr/arm-none-eabi/include")
    add_compile_options("-D_STDATOMIC_H_")
endif()


# ... (Le reste de votre fichier demeure inchangé) ...


# 2. Désactiver l'inclusion du fichier C++ défaillant du SDK Pico
set(PICO_CXX_ENABLE_EXCEPTIONS 0 CACHE INTERNAL "")
add_compile_definitions(PICO_CXX_ENABLE_EXCEPTIONS=0)

# Add executable. Default name is the project name, version 0.1
add_executable(PSNEERP PSNEERP.c )
target_compile_options(PSNEERP PRIVATE "-save-temps")


pico_set_program_name(PSNEERP "PSNEERP")
pico_set_program_version(PSNEERP "0.1")

pico_generate_pio_header(PSNEERP ${CMAKE_CURRENT_LIST_DIR}/psneerp.pio)
target_include_directories(PSNEERP PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_usb(PSNEERP 0)
pico_enable_stdio_uart(PSNEERP 1)
# Add the standard library to the build
target_link_libraries(PSNEERP 
    pico_stdlib 
    pico_multicore
    hardware_pio      # <--- AJOUTE ÇA (pour hardware/pio.h)
    hardware_clocks   # <--- AJOUTE ÇA (pour clock_get_hz)
    hardware_uart 
)

target_link_options(PSNEERP PRIVATE "-nostdlib++")

# Add the standard include files to the build
target_include_directories(PSNEERP PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

pico_add_extra_outputs(PSNEERP)



# Active l'UART et désactive l'USB pour stdio
pico_enable_stdio_uart(PSNEERP 1)
#pico_enable_stdio_usb(PSNEERP 0)


# Supprime le fichier d'options C++ problématique du SDK Pico avant la compilation
get_target_property(PICO_SOURCES PSNEERP SOURCES)
list(REMOVE_ITEM PICO_SOURCES "/home/psnee/.pico-sdk/sdk/2.2.0/src/rp2_common/pico_cxx_options/new_delete.cpp")
set_target_properties(PSNEERP PROPERTIES SOURCES "${PICO_SOURCES}")
