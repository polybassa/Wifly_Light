/**
 Copyright (C) 2012 Nils Weiss, Patrick Brünn.
 
 This file is part of Wifly_Light.
 
 Wifly_Light is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 Wifly_Light is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with Wifly_Light.  If not, see <http://www.gnu.org/licenses/>. */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "ledstrip.h"

void addCRC(char byte, char* p_crcH, char* p_crcL) {}
void newCRC(char* p_crcH, char* p_crcL) {}

void timer_init(){}
void timer_set_for_fade(char value){}
void* timer_interrupt(void* unused)
{
	for(;;)
	{
		usleep(1000 * CYCLE_TMMS);
		ledstrip_update_fade();
	}
}

void USARTinit() {}
void USARTsend(char ch)
{
	FILE* gUSART = fopen("out_usart.txt", "a+");
	fputc(ch, gUSART);
	fclose(gUSART);
}


static uns8 gEEPROM[0x100];
char EEPROM_RD(uns8 adress)
{
	return gEEPROM[adress];
}


void EEPROM_WR(uns8 adress, uns8 data)
{
	gEEPROM[adress] = data;
}

#include <pthread.h>
bit g_led_off = 1;
pthread_mutex_t g_led_mutex = PTHREAD_MUTEX_INITIALIZER;
static uns8 g_led_status[NUM_OF_LED*3];
void spi_init() {}
char spi_send(char data)
{
	int i;
	for(i = 3*NUM_OF_LED - 1; i > 0; i--)
	{
		g_led_status[i] = g_led_status[i-1];
	}
	g_led_status[0] = data;
	return g_led_status[0];
}

void spi_send_ledbuf(uns8 *array)//!!! CHECK if GIE=0 during the sendroutine improves the result
{
	//array must be the address of the first byte
	uns8* end;
	//calculate where the end is
	end = array + (NUM_OF_LED * 3);
	//send all

	pthread_mutex_lock(&g_led_mutex);
	for(; array < end; array++)
	{
		spi_send(*array);
	}
	pthread_mutex_unlock(&g_led_mutex);
}

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <time.h>

void gl_print_text(char* text, GLfloat x, GLfloat y)
{
	glRasterPos3f(0, 0, 0.0);
	glTranslatef(x, y, 0.0);
	while(*text != 0)
	{
		glutBitmapCharacter(GLUT_BITMAP_TIMES_ROMAN_10, *text++);
	}
	glTranslatef(-x, -y, 0.0);
}

void gl_print_sphere(GLfloat x, GLfloat y, float r, float g, float b)
{
	glColor4f(r, g, b, 1.0);
	glTranslatef(x, y, -50.0);
	glutSolidSphere(1.0, 16, 16);
	glTranslatef(-x, -y, +50.0);
} 

void gl_display(void)
{
	static unsigned long frames = 0;
	static struct timespec lastTime;
	static struct timespec nextTime;
	time_t seconds;

	clock_gettime(CLOCK_MONOTONIC, &lastTime);

	for(;;) {
		frames++;
		glClear(GL_COLOR_BUFFER_BIT);
		glLoadIdentity();

		clock_gettime(CLOCK_MONOTONIC, &nextTime);

		seconds = nextTime.tv_sec - lastTime.tv_sec;
		if(seconds > 0)
		{
			long nanos = nextTime.tv_nsec - lastTime.tv_nsec;
			long millis = seconds*1000 + nanos/1000/1000;
			lastTime = nextTime;
			printf("%f fps\n", 1000.0f*frames/millis);
			frames = 0;
		}

		if(!g_led_off)
		{
			unsigned int i;
			pthread_mutex_lock(&g_led_mutex);
			for(i = 0; i < NUM_OF_LED; i++)
			{
				float x = -16.0 + 2.0 * (i % 8);
				float y = 2.0 * (i / 8);			
				float r = (float)g_led_status[3*i] / 255.0;
				float g = (float)g_led_status[3*i+1] / 255.0;
				float b = (float)g_led_status[3*i+2] / 255.0;
				gl_print_sphere(x, y, r, g, b);
			}
			pthread_mutex_unlock(&g_led_mutex);
		}
		glFlush();
	}
}

void* gl_start(void* unused)
{
	int argc = 1;
	glutInit(&argc, NULL);
	glutInitWindowSize(300, 300);
	glutInitWindowPosition(0, 0);
	glutCreateWindow("Wifly_Light LED simulation");
	glutDisplayFunc(gl_display);
	glClearColor(0.0, 0.0, 0.0, 0.0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	gluPerspective(45.0, 1.0, 50.0, 100.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glViewport(0, 0, 300, 300);
	glutMainLoop();
	return 0;
}

