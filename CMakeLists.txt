cmake_minimum_required(VERSION 3.2)
project(mbed_test)

set (MBED_PATH mbed/abea610beb85)

# It's best to hide all the details of setting up the variable SRCS in a CMake
# macro. The macro can then be called in all the project CMake list files to add
# sources.
#
# The macro first computes the path of the source file relative to the project
# root for each argument. If the macro is invoked from inside a project sub
# directory the new value of the variable SRCS needs to be propagated to the
# parent folder by using the PARENT_SCOPE option.
#
# Source: http://stackoverflow.com/questions/7046956/populating-srcs-from-cmakelists-txt-in-subdirectories
macro (add_sources)
    file (RELATIVE_PATH _relPath "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
    foreach (_src ${ARGN})
        if (_relPath)
            list (APPEND SRCS "${_relPath}/${_src}")
        else()
            list (APPEND SRCS "${_src}")
        endif()
    endforeach()
    if (_relPath)
        # propagate to parent directory
        set (SRCS ${SRCS} PARENT_SCOPE)
    endif()
endmacro()

set(TOOLCHAIN armgcc)

set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_ASM_COMPILER arm-none-eabi-gcc)
set(SIZE_COMMAND arm-none-eabi-size)
set(OBJCOPY_COMMAND arm-none-eabi-objcopy)

set(MAIN_TARGET        ${PROJECT_NAME}.elf)
enable_language(ASM)

message(STATUS "C compiler  : ${CMAKE_C_COMPILER}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "Size command: ${SIZE_COMMAND}")
message(STATUS "Main target : ${MAIN_TARGET}")

set(MAIN_TARGET ${PROJECT_NAME}.elf)
enable_language(ASM)

add_sources(main.cpp)
add_sources(MotorDriverManagerRS485/MotorDriverManagerRS485.cpp)
add_sources(CisecoManager/CisecoManager.cpp)

include_directories(
        ${MBED_PATH}/
        ${MBED_PATH}/TARGET_LPC1768
        ${MBED_PATH}/TARGET_LPC1768/TARGET_NXP/
        ${MBED_PATH}/TARGET_LPC1768/TARGET_NXP/TARGET_LPC176X/
        ${MBED_PATH}/TARGET_LPC1768/TARGET_NXP/TARGET_LPC176X/TARGET_MBED_LPC1768
        EthernetInterface
        EthernetInterface/Socket
        PixelArray
        DoubleCoilGun
        MCP3021
        MotorDriverManagerRS485
        CisecoManager
)

add_definitions(-DDEVICE_SERIAL)
add_definitions(-DDEVICE_PWMOUT)

############################################################################
# By now, we've traversed all subdirectories and have collected everything that
# needs to be built. We can define the build targets.
############################################################################
# add MbedTest as a build target depending on all the sources
add_executable(${MAIN_TARGET} ${SRCS})

# Add a post-build dependency like printing size of the
# resulting binary and copying to the target.
if(${CMAKE_SYSTEM_NAME} MATCHES "Darwin")
    set(CMAKE_CXX_LINK_FLAGS "")
endif()
add_custom_command(
        TARGET ${MAIN_TARGET}
        COMMAND ${SIZE_COMMAND} ${MAIN_TARGET}
        COMMAND arm-none-eabi-objcopy -O binary ${MAIN_TARGET} ${PROJECT_NAME}.bin # convert .elf to .hex
)