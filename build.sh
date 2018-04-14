#!/usr/bin/env bash
# RELEASE_FLAGS="-O3"
g++ mineclone.cpp -o mineclone.out -Wall -Wextra -Wno-unused-function -Wno-unused-but-set-variable -std=c++11 -g ${RELEASE_FLAGS} -ffast-math -Iinclude -L. -lGL -lSDL2 -ldl -fno-sanitize-recover # -fsanitize=undefined -fsanitize=address
