rem -Ox -RTCsu 
cl mineclone.cpp -I.\include -W3 -ZI -link -out:mineclone.exe opengl32.lib SDL2.lib openvr_api.lib -debug
