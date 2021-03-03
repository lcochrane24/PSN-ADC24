@Echo off
"D:\dspic\mplab\MPLAB ASM30 Suite\bin\pic30-as.exe" -p=30F3014 "bootldr.s" -o"bootldr.o"
"D:\dspic\C30\bin\pic30-ld.exe" "bootldr.o" --script="D:\dspic\mplab\MPLAB ASM30 Suite\Support\gld\p30f3014.gld" --heap=256 --stack=256 -Map="bootldr.map" -o"bootldr.cof"
"D:\dspic\C30\bin\pic30-bin2hex.exe" "bootldr.cof"
@ copy /q bootldr.hex sdr24boot3014.hex
@ del /q bootldr.hex
