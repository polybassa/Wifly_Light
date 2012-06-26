//Nils Wei� 
//20.04.2012
//Compiler CC5x 

#include "ledstrip.h"
//private struct
struct max_changes_struct
	{
		uns8 red;
		uns8 green;
		uns8 blue;
	};

//private function
uns8 ledstrip_get_change(uns8 destinationvalue, uns8 currentvalue);
uns8 ledstrip_calc_change(uns8 *change_array,struct max_changes_struct maxChange );

#define FOR_EACH_MASKED_LED_DO(BLOCK) { \
	char *address = pCmd->addr; \
	char r = pCmd->red; \
	char g = pCmd->green; \
	char b = pCmd->blue; \
	char k,mask; \
	mask = 0x01; \
	for(k = 0; k < (NUM_OF_LED * 3); k++) {	\
		if(0 != (*address & mask)) { \
			BLOCK \
		} else { \
			k++; k++; \
		} \
		mask = mask << 1; \
		if(0 == mask) { \
			address++; \
			mask = 0x01; \
		} \
	} \
}

void ledstrip_init(void)
{
	char k;
	for(k = 0;k < (NUM_OF_LED * 3); k++)
	{	
		gLedBuf.led_array[k] = 0;
		gLedBuf.led_destination[k] = 0;
		gLedBuf.led_fade_operation = 0;
		gLedBuf.led_run_operation = 0;
	}
}

/***
*** This funktion sets the values of the global LedBuffer
*** only Led's where the address bit is 1 will be set to the new color
***/
void ledstrip_set_color(struct cmd_set_color *pCmd)
{
	FOR_EACH_MASKED_LED_DO(
			gLedBuf.led_array[k] = b;
			gLedBuf.led_destination[k] = b;
			k++;
			
			gLedBuf.led_array[k] = g;
			gLedBuf.led_destination[k] = g;
			k++;
			
			gLedBuf.led_array[k] = r;
			gLedBuf.led_destination[k] = r;
	);
	spi_send_ledbuf(&gLedBuf.led_array[0]);
	// Disable other functions
	gLedBuf.led_fade_operation = 0;
	gLedBuf.led_run_operation = 0;
}
/***
* This funktion sets the destination color configurates
* the timer 2 PR2 Register. If the settings are done, the bit led_fade_operation is 1.
* 
**/
void ledstrip_set_fade(struct cmd_set_fade *pCmd)
{	
	char temp;
	struct max_changes_struct maxChange;
	FOR_EACH_MASKED_LED_DO(
			temp = gLedBuf.led_array[k];
			gLedBuf.led_destination[k] = b;
			maxChange.blue = ledstrip_get_change(b,temp);
			k++;
			
			temp = gLedBuf.led_array[k];
			gLedBuf.led_destination[k] = g;
			maxChange.green = ledstrip_get_change(g,temp);
			k++;
			
			temp = gLedBuf.led_array[k];
			gLedBuf.led_destination[k] = r;
			maxChange.red = ledstrip_get_change(r,temp);
	);
	timer_set_for_fade(pCmd->timevalue);
	gLedBuf.led_fade_operation = 1;
}


void ledstrip_do_fade()
{
#ifndef X86
	char fade_finish:1 = TRUE;
#else
	char fade_finish = TRUE;
#endif	
	
	char temp_current,temp_destination;
	
	char i;
	for(i = 0; i < (NUM_OF_LED*3);i++)
	{
		temp_current = gLedBuf.led_array[i];
		temp_destination = gLedBuf.led_destination[i];
		
		if(temp_current > temp_destination)
		{
			gLedBuf.led_array[i] = --temp_current;
			fade_finish = FALSE;
		}
		else if(temp_current < temp_destination)	
		{
			gLedBuf.led_array[i] = ++temp_current;
			fade_finish = FALSE;
		}		
	}	
	if(fade_finish) 
	{
		gLedBuf.led_fade_operation = FALSE;
		//send Fade Done
		USARTsend('F');
		USARTsend('D');
	}
	else 
		spi_send_ledbuf(&gLedBuf.led_array[0]);
}

//This funktion returns the differenz between the currentvalue of a led and the destinationvalue

uns8 ledstrip_get_change(uns8 destinationvalue, uns8 currentvalue)
{
	char temp;
	if(destinationvalue > currentvalue)
		return temp = destinationvalue - currentvalue;
	else
		return temp = currentvalue - destinationvalue;	
}

uns8 ledstrip_calc_change(uns8 *change_array,struct max_changes_struct maxChange )
{
/***
* To Do... Teile alle drei werte durch 2 (>>) solange bis 
- alle Werte kleiner als 16 sind
- mindestens ein Wert 1 ist
- ist ein Wert 1, dann Teile diesen nichtmehr
- ist ein Wert null, f�hre ebenfalls keine operationen an diesem durch
**/
return 0;
}
