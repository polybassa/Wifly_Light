/*
   Copyright (C) 2012 - 2014 Nils Weiss, Patrick Bruenn.

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

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glut.h>
#include <pthread.h>
#include <stdio.h>
#include <time.h>

#include "platform.h"

static pthread_mutex_t g_led_mutex = PTHREAD_MUTEX_INITIALIZER;
static uns8 g_led_status[NUM_OF_LED * 3];

static void gl_print_sphere(GLfloat x, GLfloat y, float r, float g, float b)
{
    glColor4f(r, g, b, 1.0);
    glTranslatef(x, y, -50.0);
    glutSolidSphere(1.0, 16, 16);
    glTranslatef(-x, -y, +50.0);
}

static void gl_display(void)
{
    static unsigned long frames = 0;
    static struct timespec lastTime;
    static struct timespec nextTime;
    time_t seconds;

    clock_gettime(CLOCK_MONOTONIC, &lastTime);

    for ( ; ; ) {
#ifndef SHOW_FPS
        static const struct timespec NANOSLEEP_TIME = {0, 500000000};
        nanosleep(&NANOSLEEP_TIME, NULL);
#endif
        frames++;
        glClear(GL_COLOR_BUFFER_BIT);
        glLoadIdentity();

        clock_gettime(CLOCK_MONOTONIC, &nextTime);

        seconds = nextTime.tv_sec - lastTime.tv_sec;
        if (seconds > 0) {
#ifdef SHOW_FPS
            long nanos = nextTime.tv_nsec - lastTime.tv_nsec;
            long millis = seconds * 1000 + nanos / 1000 / 1000;
            lastTime = nextTime;
            printf("%f fps\n", 1000.0f * frames / millis);
#endif
            frames = 0;
        }

        unsigned int i;
        pthread_mutex_lock(&g_led_mutex);
        for (i = 0; i < NUM_OF_LED; i++) {
            float x = -16.0 + 2.0 * (i % 8);
            float y = 2.0 * (i / 8);
            float r = (float)g_led_status[3 * i] / 255.0;
            float g = (float)g_led_status[3 * i + 1] / 255.0;
            float b = (float)g_led_status[3 * i + 2] / 255.0;
            gl_print_sphere(x, y, r, g, b);
        }
        pthread_mutex_unlock(&g_led_mutex);
        glFlush();
    }
}

void* gl_start(void* unused)
{
    int argc = 1;
    glutInit(&argc, NULL);
    glutInitWindowSize(300, 300);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("WyLight LED simulation");
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

void SPI_Init()
{
    pthread_t glThread;

    pthread_create(&glThread, 0, gl_start, 0);
}

char SPI_Send(uns8 data)
{
    int i;
    for (i = 3 * NUM_OF_LED - 1; i > 0; i--) {
        g_led_status[i] = g_led_status[i - 1];
    }
    g_led_status[0] = data;
    return g_led_status[0];
}

void SPI_SendLedBuffer(uns8* array, uns8 length)
{
    //array must be the address of the first byte
    uns8* end;
    //calculate where the end is
    end = array + length;
    //send all

    pthread_mutex_lock(&g_led_mutex);
    for ( ; array < end; array++) {
        SPI_Send(*array);
    }
    pthread_mutex_unlock(&g_led_mutex);
}
