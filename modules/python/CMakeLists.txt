set (PYTHON_VERSION "AUTO" CACHE STRING "Version of the installed development library" )

if ("${PYTHON_VERSION}" STREQUAL "AUTO")
  find_package(PythonInterp)
  find_package(PythonLibs)

  if (PYTHONLIBS_FOUND)
    option(ENABLE_PYTHON "Enable Python module" ON)
  else()
    option(ENABLE_PYTHON "Enable Python module" OFF)
  endif()

  if (NOT ENABLE_PYTHON)
    return()
  endif ()

  if (NOT PYTHONLIBS_FOUND)
    message(FATAL_ERROR "Python module enabled, but python library not found.")
  endif()

else ()
  find_package(PythonInterp EXACT "${PYTHON_VERSION}" REQUIRED)
  find_package(PythonLibs EXACT "${PYTHON_VERSION}" REQUIRED)
endif ()

MESSAGE(STATUS "Found python ${PYTHONLIBS_VERSION_STRING}")

set(PYTHON_SOURCES
    python-module.h
    python-config.h
    python-config.c
    python-helpers.h
    python-helpers.c
    python-main.h
    python-main.c
    python-plugin.c
    python-value-pairs.c
    python-value-pairs.h
    python-dest.c
    python-dest.h
    python-tf.c
    python-tf.h
    python-parser.c
    python-parser.h
    python-logmsg.h
    python-logmsg.c
    python-debugger.c
    python-debugger.h
    python-logparser.h
    python-logparser.c
    compat/compat-python.c
    ${CMAKE_CURRENT_BINARY_DIR}/python-grammar.c
    ${CMAKE_CURRENT_BINARY_DIR}/python-grammar.h
)

set (PYTHON_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

generate_y_from_ym(modules/python/python-grammar)

bison_target(PythonGrammar
    ${CMAKE_CURRENT_BINARY_DIR}/python-grammar.y
    ${CMAKE_CURRENT_BINARY_DIR}/python-grammar.c
    COMPILE_FLAGS ${BISON_FLAGS})

add_subdirectory(pylib)
add_library(mod-python SHARED ${PYTHON_SOURCES})
target_include_directories (mod-python PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories (mod-python PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories (mod-python PRIVATE ${PYTHON_INCLUDE_DIRS})
target_link_libraries(mod-python PRIVATE syslog-ng ${PYTHON_LIBRARIES} ${Eventlog_LIBRARIES} ${IVYKIS_LIBRARIES})

include(CheckFunctionExists)
if (PYTHONLIBS_VERSION_STRING VERSION_LESS 3)
  target_compile_definitions(mod-python PRIVATE SYSLOG_NG_ENABLE_PYTHONv2=1)
else ()
  target_compile_definitions(mod-python PRIVATE SYSLOG_NG_ENABLE_PYTHONv3=1)
endif ()

add_dependencies(mod-python SetupPylib)
install(TARGETS mod-python DESTINATION lib/syslog-ng/)

add_test_subdirectory(tests)
