#---------------------------------------------------------------------
# General configuration
#---------------------------------------------------------------------
# The first version of CMake to support tiarmclang is 3.29
cmake_minimum_required(VERSION 3.29)

# Cross compiling for target system that is an embedded device
set(CMAKE_SYSTEM_NAME Generic)

set(CPU_FAMILY cc23xx)

# Get the standard directories
include(../cmake_inc.txt)

# Create the project
project(
    cc2340r5-blink
    LANGUAGES C
)

#---------------------------------------------------------------------
# Configuration related to SysConfig
#---------------------------------------------------------------------
# Directory to the generated SysConfig files
set(SYSCONFIG_GENDIR ${CMAKE_BINARY_DIR}/sysconfig_generated)

# SysConfig Input Script
set(SYSCONFIG_INPUT_FILE_NAME blink.syscfg)
set(SYSCONFIG_INPUT ${CMAKE_SOURCE_DIR}/src/${SYSCONFIG_INPUT_FILE_NAME})

# SysConfig stamp file.  Described in the block comment at the end.
set(SYSCONFIG_STAMP_FILE ${SYSCONFIG_GENDIR}/${SYSCONFIG_INPUT_FILE_NAME}.stamp)

# SysConfig invocation command line
        # actually CC2340R52E0RKPR, QFN40, 5x5mm
set(SYSCONFIG_CLI_CMD
    ${SYSCONFIG_CLI}
       --script ${SYSCONFIG_INPUT}
       -o ${SYSCONFIG_GENDIR}
       --compiler ticlang
       -s ${SDK_ROOT}/.metadata/product.json
)

# Ask SysConfig what files it generates.  Then categorize them.  This occurs
# when CMake configures the project.
execute_process(
    COMMAND ${SYSCONFIG_CLI_CMD} --listGeneratedFiles
    OUTPUT_VARIABLE SYSCONFIG_GEN_FILES_STR
)
#message("SYSCFG files: ${SYSCONFIG_GEN_FILES_STR}")
string(REPLACE "\n" ";" SYSCONFIG_GEN_FILES ${SYSCONFIG_GEN_FILES_STR})

set(SYSCONFIG_GEN_CFILES        ${SYSCONFIG_GEN_FILES})
set(SYSCONFIG_GEN_HFILES        ${SYSCONFIG_GEN_FILES})
set(SYSCONFIG_GEN_LNKCMD        ${SYSCONFIG_GEN_FILES})
set(SYSCONFIG_GEN_COMPILER_OPTS ${SYSCONFIG_GEN_FILES})

list(FILTER SYSCONFIG_GEN_CFILES        INCLUDE REGEX ".*\\.c$")
list(FILTER SYSCONFIG_GEN_HFILES        INCLUDE REGEX ".*\\.h$")
list(FILTER SYSCONFIG_GEN_LNKCMD        INCLUDE REGEX ".*\\.cmd\\.")
list(FILTER SYSCONFIG_GEN_COMPILER_OPTS INCLUDE REGEX ".*\\.opt$")

# Cause CMake to run when the SysConfig input is modified, because that may
# change the list of generated files
set_property(
    DIRECTORY
    APPEND
    PROPERTY CMAKE_CONFIGURE_DEPENDS ${SYSCONFIG_INPUT}
)

# Run SysConfig over the input script.  Described further in a block comment
# at the end.
add_custom_command(
    DEPENDS    ${SYSCONFIG_INPUT}
    OUTPUT     ${SYSCONFIG_STAMP_FILE}
    BYPRODUCTS ${SYSCONFIG_GEN_FILES}
    COMMAND    ${SYSCONFIG_CLI_CMD}
    COMMAND    ${CMAKE_COMMAND} -E touch ${SYSCONFIG_STAMP_FILE}
    COMMENT    "Running SysConfig on ${SYSCONFIG_INPUT}"
    VERBATIM
)
add_custom_target(SysConfig_build ALL DEPENDS ${SYSCONFIG_STAMP_FILE})

#---------------------------------------------------------------------
# Configuration related to compiling
#---------------------------------------------------------------------
# Compiler options which specify the variant of the Arm processor in the
# system.  These are often copied from an SDK example for that system.  They
# rarely change.
set(PROCESSOR_OPTIONS
    -march=thumbv6m
    -mcpu=cortex-m0plus
    -mfloat-abi=soft
    -mlittle-endian
    -mthumb
    -Wall
)

# These compiler options typically change as the project evolves.  Because
# CMake replaces an undefined variable with an empty string, one way to
# disable an option is to comment out that line.
set(OPTIMIZATION_OPTION -Oz)
# simplelink libraries compiled without LTO
#set(LTO_OPTION          -flto)
set(DEBUG_OPTION        -gdwarf-3)

# Compiler options
add_compile_options(
    ${PROCESSOR_OPTIONS}
    ${OPTIMIZATION_OPTION}
    ${LTO_OPTION}
    ${DEBUG_OPTION}
    @${SYSCONFIG_GEN_COMPILER_OPTS}
)

# C files
set(SOURCES
    ${CMAKE_SOURCE_DIR}/src/blink.c
    ${SYSCONFIG_GEN_CFILES}
)

# Cause C files to depend on the SysConfig generated compiler options file
message("OBJECT_DEPENDS = ${OBJECT_DEPENDS}")
set_source_files_properties(
    ${SOURCES}
    PROPERTIES OBJECT_DEPENDS
    ${SYSCONFIG_GEN_COMPILER_OPTS}
)

# Directories searched for header files
include_directories(
    ${SDK_ROOT}/source
    ${SDK_ROOT}/kernel/freertos
    ${SDK_ROOT}/source/ti/posix/ticlang
    ${SDK_ROOT}/source/third_party/freertos/include
    # use the GCC portable header files
    ${SDK_ROOT}/source/third_party/freertos/portable/GCC/ARM_CM0
    ${SYSCONFIG_GENDIR}
)

#---------------------------------------------------------------------
# Configuration related to linking
#---------------------------------------------------------------------
# Linker Options
add_link_options(
    ${LTO_OPTION}            # Required when compiling AND linking
    LINKER:--reread_libs
    LINKER:--warn_sections
    LINKER:--rom_model
    LINKER:--diag_wrap=off
    LINKER:--display_error_number
)

# Directories searched for libraries
link_directories(
    ${SDK_ROOT}/source
)

# Libraries and linker command files
link_libraries(
    ${CMAKE_SOURCE_DIR}/lpf3_freertos.cmd
    ${SYSCONFIG_GEN_LNKCMD}
)

#---------------------------------------------------------------------
# Final configuration that pulls everything together
#---------------------------------------------------------------------
add_executable(
    ${PROJECT_NAME}
    ${SOURCES}
)

#
# Add CRC to memory regions, do it quietly
#
add_custom_command(
  TARGET ${PROJECT_NAME}
  POST_BUILD
  COMMENT "Running crc_tool on ${PROJECT_NAME}"
  COMMAND ${CRC_TOOL} patch-image --quiet --elf ${PROJECT_NAME}.out --symbol-prefix ti_utils_build_GenMap_sym_CRC --output ${PROJECT_NAME}.out
)

add_custom_target(
  flash
  DEPENDS ${PROJECT_NAME}.out
  COMMENT "Run OpenOCD to flash ${PROJECT_NAME}"
  COMMAND openocd -c "source [find board/ti_lp_em_cc2340r53.cfg]" -c "program ${PROJECT_NAME}.out reset exit"
  # add to openocd cmd to enable debugging
  # --debug=2
)

