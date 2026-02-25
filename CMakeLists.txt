cmake_minimum_required(VERSION 3.29)
project(PROYECTO)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1. Configuraciones obligatorias para que Qt funcione internamente
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_PREFIX_PATH "C:/msys64/mingw64")
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(PROYECTO main.cpp)

target_link_libraries(PROYECTO PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# Copiar carpeta SPRITES al directorio de salida
add_custom_command(TARGET PROYECTO POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/SPRITES
        $<TARGET_FILE_DIR:PROYECTO>/SPRITES
)

# Desplegar plugins de Qt (incluye imageformats para PNG)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${CMAKE_PREFIX_PATH}/bin")
if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET PROYECTO POST_BUILD
        COMMAND ${WINDEPLOYQT_EXECUTABLE} --no-translations "$<TARGET_FILE:PROYECTO>"
        COMMENT "Desplegando Qt (plugins de imagen para PNG)..."
    )
endif()
