#!/usr/bin/env bash
# RELEASE_FLAGS="-O3"
g++ mineclone.cpp -v -std=c++11 -g ${RELEASE_FLAGS} -ffast-math -Iinclude -L. -lSDL2 -ldl -fsanitize=address -framework CoreFoundation
