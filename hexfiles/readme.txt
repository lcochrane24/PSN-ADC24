Files in this folder:

sdr24boot.hex - This is the bootloader for the DSPic30F3014 microcontroller. You must flash this bootloader first
                using a programer that can flash the 40 pin DSPic30F3014 part.

Sdr24AdcV25.hex - Once you have the bootload intalled place the chip into the ADC board and use the DspBoot.exe 
                  program to flash the firmware onto the CPU.
                  
DspBoot.exe - This Windows command line program is used to flash the firmware onto the microcontroller.

              Usage: DspBoot [ -p Port_Number ] [ -b Baud_Rate ] [ -t ] Hex_File
	      
	             Optional Parameters:
    	                -p Port Number 1 through 8 Default=1
    	                -b Baud Rate 9600, 19200, 38400, 57600 Default=38400
    	                -t Display Dspic CPU Type

