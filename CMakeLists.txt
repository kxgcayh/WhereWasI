cmake_minimum_required(VERSION 3.21)

project(WhereWasI VERSION 1.0.0 LANGUAGES CXX)

find_package(CommonLibSSE CONFIG REQUIRED)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.h src/*.h)

# Struct-independent (NoStructUse) on purpose: NG runtime-data accessors pick the layout per runtime.
# STRUCT_DEPENDENT would make SKSE treat it as pre-1.6.629 only.
add_commonlibsse_plugin(${PROJECT_NAME}
    AUTHOR "Kautsar Al Bana"
    SOURCES ${HEADERS} ${SOURCES}
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE include src)
target_precompile_headers(${PROJECT_NAME} PRIVATE include/PCH.h)
