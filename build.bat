rem -Ox -RTCsu 
rem openvr_api.lib
cl mineclone.cpp -Ox -I.\include -W3 -ZI -link -out:mineclone.exe opengl32.lib SDL2.lib -debug
