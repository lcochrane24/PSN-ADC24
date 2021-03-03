cl -c TestSdr24.cpp
cl -c TestSdr24Comm.cpp
cl -c TestSdr24GPS.cpp
cl -c display.cpp
cl TestSdr24.obj  TestSdr24Comm.obj TestSdr24GPS.obj display.obj Ws2_32.lib
