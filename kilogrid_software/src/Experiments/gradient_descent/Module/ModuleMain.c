#include <avr/io.h>
#include <avr/wdt.h>
#include <util/delay.h>     // delay macros
#include <avr/interrupt.h>

#include "module.h"
#include "moduleLED.h"
#include "moduleIR.h"
#include "CAN.h"

#include "../communication.h"

volatile uint8_t cell_colour[4] = {0, 0, 0, 0};
uint8_t colour2[3] = {0x68, 0x4C, 0x75};
cell_num_t cell_id[4] = {CELL_00, CELL_01, CELL_02, CELL_03};


void setup()
{
    cell_colour[0] = (configuration[0]);
    cell_colour[1] = (configuration[1]);
    cell_colour[2] = (configuration[2]);
    cell_colour[3] = (configuration[3]);
}

void loop()
{
	for(int i = 0; i < 4; ++i)
	{

		switch(cell_colour[i])
		{
		 case 0:
		   set_LED_with_brightness(cell_id[i], BLUE, HIGH);
		 break;
		 case 1:
		   set_LED_with_brightness(cell_id[i], GREEN, HIGH);
		 break;
     case 2:
       set_LED_with_brightness(cell_id[i], RED, HIGH);
     break;
     case 2:
       set_LED_with_brightness(cell_id[i], CYAN, HIGH);
     break;
		 default:
		 break;
		}
	}
}


int main() {
    module_init();

    module_start(setup, loop);

    return 0;
}
