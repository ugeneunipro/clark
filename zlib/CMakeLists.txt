cmake_minimum_required(VERSION 2.8)

project(zlib)

file(GLOB SRCS *.c)
file(GLOB HDRS *.h)

add_library(z STATIC ${HDRS} ${SRCS})

