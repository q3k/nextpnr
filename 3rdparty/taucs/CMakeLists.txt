cmake_minimum_required(VERSION 3.3)

if (MSVC)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} /D_DEBUG")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} /W4")
else()
    set(CMAKE_C_FLAGS_DEBUG "-fPIC -ggdb -pipe")
    set(CMAKE_C_FLAGS_RELEASE "-fPIC -O3 -g")
endif()

if(WIN32)
    set(TAUCS_OSTYPE win32)
else()
    set(TAUCS_OSTYPE linux)
endif()

add_executable(taucs_configurator configurator/taucs_config.c)
set(ENV_CMD ${CMAKE_COMMAND} -E env "OSTYPE=${TAUCS_OSTYPE}")
set(TAUCS_CONFIG_IN ${CMAKE_CURRENT_SOURCE_DIR}/taucsconfig.in)
set(TAUCS_CONFIG_DIR ${CMAKE_CURRENT_BINARY_DIR}/build/linux)
set(TAUCS_CONFIG_BUILD ${TAUCS_CONFIG_DIR}/taucs_config_build.h)
set(TAUCS_CONFIG_TESTS ${TAUCS_CONFIG_DIR}/taucs_config_tests.h)

add_custom_command(OUTPUT ${TAUCS_CONFIG_BUILD}
        COMMAND ${ENV_CMD} ./taucs_configurator in=${TAUCS_CONFIG_IN}
        DEPENDS taucs_configurator ${TAUCS_CONFIG_IN}
)

file(WRITE ${TAUCS_CONFIG_TESTS} "#define TAUCS_CORE\n#define TAUCS_BLAS_NOUNDERSCORE\n#include <cblas.h>\n")
# Need to build similar set of files twice with different flags
set(TAUCS_GENERAL_FILES src/taucs_sn_llt.c src/taucs_linsolve.c src/taucs_logging.c src/taucs_memory.c src/taucs_timer.c
        src/taucs_ccs_base.c src/taucs_vec_base.c src/taucs_ccs_ops.c src/taucs_ccs_io.c src/taucs_ccs_factor_llt.c
        src/taucs_ccs_solve_llt.c src/taucs_complex.c src/taucs_ccs_ooc_llt.c src/taucs_ccs_ooc_lu.c
        src/taucs_ooc_io.c src/taucs_malloc.c)

set(TAUCS_DOUBLE_FILES src/taucs_sn_llt.c src/taucs_ccs_base.c src/taucs_vec_base.c src/taucs_ccs_ops.c src/taucs_ccs_io.c
        src/taucs_ccs_factor_llt.c src/taucs_ccs_solve_llt.c src/taucs_complex.c src/taucs_ccs_ooc_llt.c src/taucs_ccs_ooc_lu.c
        src/taucs_iter.c src/taucs_vaidya.c src/taucs_recvaidya.c src/taucs_gremban.c src/taucs_ccs_xxt.c src/taucs_ccs_generators.c)

set(TAUCS_GENERAL_FOLDER ${CMAKE_CURRENT_BINARY_DIR}/src/general/)
set(TAUCS_DOUBLE_FOLDER ${CMAKE_CURRENT_BINARY_DIR}/src/double/)

file(COPY ${TAUCS_GENERAL_FILES} DESTINATION ${TAUCS_GENERAL_FOLDER})
file(COPY ${TAUCS_DOUBLE_FILES} DESTINATION ${TAUCS_DOUBLE_FOLDER})

aux_source_directory(${TAUCS_GENERAL_FOLDER} TAUCS_GENERAL_SRC)
aux_source_directory(${TAUCS_DOUBLE_FOLDER} TAUCS_DOUBLE_SRC)

add_library(taucs_general OBJECT ${TAUCS_GENERAL_SRC} ${TAUCS_CONFIG_BUILD})
add_library(taucs_double OBJECT ${TAUCS_DOUBLE_SRC} ${TAUCS_CONFIG_BUILD})
target_compile_definitions(taucs_general PRIVATE TAUCS_CORE_GENERAL)
target_compile_definitions(taucs_double PRIVATE TAUCS_CORE_DOUBLE)

target_include_directories(taucs_general PRIVATE src external/src  ${TAUCS_CONFIG_DIR})
target_include_directories(taucs_double PRIVATE src external/src  ${TAUCS_CONFIG_DIR})

add_library(taucs STATIC ${TAUCS_CONFIG_BUILD} $<TARGET_OBJECTS:taucs_general> $<TARGET_OBJECTS:taucs_double>)
target_include_directories(taucs PRIVATE src external/src ${TAUCS_CONFIG_DIR})
target_link_libraries(taucs PRIVATE blas f2c m)
