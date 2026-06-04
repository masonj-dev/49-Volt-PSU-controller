This code is for an Arduino GIGA R1 and specifically for the 480 MHz ARM Cortex-M7. 

Along with the GIGA disply shild and two sensors this program reads a 0-60v input 
and outputs it to the screan where the user canm use the arrows to increase or decrase 
the persentage slider to change the power suply voltage. The output from the arduino to
the controller is 06v +-.03v. 

The input voltage sensor curently used is the Adfruit ADS1115 with a volatage divder 
before the A0 pin. The voltage divider is a 220k Ohms on the positive side and 10k ohms 
on the negitive side. Wiring diagram soon to come.

