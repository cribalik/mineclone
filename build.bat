rem -Ox -RTCsu 
if not exist openvr_api.dll (
	cl mineclone.cpp -Ox -I.\include -W3 -ZI -link -out:mineclone.exe opengl32.lib SDL2.lib -debug
) else (
	cl mineclone.cpp -DVR_ENABLED -Ox -I.\include -W3 -ZI -link -out:mineclone.exe opengl32.lib SDL2.lib openvr_api.lib -debug
)
