CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

PROJECT(influxdb_c_ut)

FILE(REMOVE_RECURSE ./tmp)
FILE(COPY main_cc DESTINATION ./tmp)
FILE(RENAME ./tmp/main_cc ./tmp/main.cc)
FILE(COPY ./tmp/main.cc DESTINATION .)
FILE(REMOVE_RECURSE ./tmp)

LINK_LIBRARIES(emock gtest gtest_main pthread dl)

SET(CMAKE_BUILD_TYPE "Debug")
SET(CMAKE_CXX_FLAGS_DEBUG "$ENV{CXXFLAGS} -O0 -Wall -g2 -ggdb")
SET(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

ADD_EXECUTABLE(influxdb_c_ut main.cc)