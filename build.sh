# RELEASE_FLAGS="-O3"
g++ mineclone.cpp -std=c++11 -g ${RELEASE_FLAGS} -ffast-math -Iinclude -L. -lGL -lSDL2 -ldl #-fsanitize=address
