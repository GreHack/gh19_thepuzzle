cmake_minimum_required (VERSION 3.14)
set (PROJECT_NAME Main)

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/src SOURCES)

project (${PROJECT_NAME})
add_executable(${PROJECT_NAME} ${SOURCES})
