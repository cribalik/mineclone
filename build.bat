rem -Ox
cl mineclone.cpp -I.\include -W3 -ZI -link -out:mineclone.exe opengl32.lib SDL2.lib -debug
