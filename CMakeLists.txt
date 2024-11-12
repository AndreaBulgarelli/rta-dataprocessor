cmake_minimum_required(VERSION 3.10)

project(DataProcessor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_INSTALL_PREFIX "/usr/local")
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;/usr/local/lib")  

set(AVRO_CPP_PATH "/usr/local/include/avro")

set(INC_DIR
    "include"  # Include directory within the small project
    "${AVRO_CPP_PATH}"             # Avro include directory
)

find_library(AVRO_CPP_LIBRARY avrocpp_s REQUIRED PATHS ${AVRO_CPP_PATH}/../../lib)

list(APPEND LIBS 
    avrocpp_s
)

link_directories(${AVRO_LIBRARY_DIR})


set(BOOST_ROOT "/usr/local")
set(BOOST_LIBRARYDIR "/usr/local/lib")


find_package(spdlog REQUIRED)
find_package(ZeroMQ REQUIRED)  
find_package(Boost 1.73.0 REQUIRED COMPONENTS iostreams filesystem program_options regex)


# set(Avro_INCLUDE_DIR "/usr/local/include/avro") 
set(Spdlog_INCLUDE_DIR "/usr/local/include/spdlog")
set(Boost_INCLUDE_DIR "/usr/local/include/boost")
set(Avro_LIBRARY "/usr/local/lib/libavrocpp.so")  
set(Spdlog_LIBRARY "/usr/local/lib/libspdlog.a")
# set(Boost_LIBRARY "/usr/local/lib/libboost_filesystem.so.1.73.0")


set(Boost_USE_STATIC_LIBS OFF)  
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

include_directories(${CMAKE_SOURCE_DIR}/include ${Boost_INCLUDE_DIR} ${Spdlog_INCLUDE_DIR} ${INC_DIR} "/include")

link_directories("/usr/local/lib")  

file(GLOB RTADP_PROTO_FILES "${CMAKE_SOURCE_DIR}/src/rtadp-proto/*.cpp")
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

set(SOURCES ${RTADP_PROTO_FILES} ${SRC_FILES})


add_executable(ProcessDataConsumer1 ${SOURCES})
target_compile_options(ProcessDataConsumer1 PRIVATE -g)
target_link_libraries(ProcessDataConsumer1
    zmq 
    # ${Avro_LIBRARY}
    ${LIBS}
    ${Spdlog_LIBRARY}
    ${Boost_LIBRARY}  
    #fmt
    pthread
)


