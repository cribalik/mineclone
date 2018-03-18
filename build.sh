#!/usr/bin/env bash
# RELEASE_FLAGS="-O3"
g++ mineclone.cpp -o mineclone.out -Wall -Wno-unused-function -Wno-unused-but-set-variable -std=c++11 -g ${RELEASE_FLAGS} -ffast-math -Iinclude -L. -lGL -lSDL2 -ldl #-fsanitize=address
