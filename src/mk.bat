@echo off
@"D:\dspic\C30\bin\pic30-gcc.exe" -Os -mcpu=30F3014 -c -x c "%mkfile%.c" -o"%mkfile%.o" -Wall  -fpack-struct
@"D:\dspic\C30\bin\pic30-gcc.exe" -Wl,""%mkfile%.o","D:\dspic\C30\lib\libp30F3014-coff.a",-L"D:\dspic\C30\lib",--script="D:\dspic\C30\support\gld\p30f4013.gld",--heap=128,--stack=128,-Map=""%mkfile%.map",--warn-section-align,-o""%mkfile%.cof"
@"D:\dspic\C30\bin\pic30-bin2hex.exe" "%mkfile%.cof"
copy sdr24.hex Sdr24AdcV25.hex > NULL
del /q sdr24.hex
