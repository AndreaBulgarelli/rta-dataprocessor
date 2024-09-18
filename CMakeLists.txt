cmake_minimum_required(VERSION 3.10)

project(DataProcessor)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_INSTALL_PREFIX "/usr/local")

set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/lib;/usr/local/lib")  

set(BOOST_ROOT "/usr/local")
set(BOOST_LIBRARYDIR "/usr/local/lib")


find_package(spdlog REQUIRED)
find_package(ZeroMQ REQUIRED)  
find_package(Boost 1.73.0 REQUIRED COMPONENTS iostreams filesystem program_options regex)


set(Avro_INCLUDE_DIR "/usr/local/include/avro") 
set(Spdlog_INCLUDE_DIR "/usr/local/include/spdlog")
set(Boost_INCLUDE_DIR "/usr/local/include/boost")
set(Avro_LIBRARY "/usr/local/lib/libavrocpp.so")  
set(Spdlog_LIBRARY "/usr/local/lib/libspdlog.a")
set(Boost_LIBRARY "/usr/local/lib/libboost_filesystem.so.1.73.0")


set(Boost_USE_STATIC_LIBS OFF)  
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

include_directories(${CMAKE_SOURCE_DIR}/include ${Avro_INCLUDE_DIR} ${Boost_INCLUDE_DIR} ${Spdlog_INCLUDE_DIR} "/include")

link_directories("/usr/local/lib")  

file(GLOB RTADP_PROTO_FILES "${CMAKE_SOURCE_DIR}/src/rtadp-proto/*.cpp")
file(GLOB SRC_FILES "${CMAKE_SOURCE_DIR}/src/*.cpp")

set(SOURCES ${RTADP_PROTO_FILES} ${SRC_FILES})


add_executable(ProcessDataConsumer1 ${SOURCES})

target_link_libraries(ProcessDataConsumer1
    zmq 
    ${Avro_LIBRARY}
    ${Spdlog_LIBRARY}
    ${Boost_LIBRARY}  
    fmt
    pthread
)


