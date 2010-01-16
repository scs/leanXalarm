/*	leanXmotion.c
	Copyright (C) 2008 Reto Bättig
	
	This program is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.
	
	This program is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
	General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

/*!@file leanXmotion.c
 * @Configurable simple motion detection tools
 */

#include <stdlib.h>
#include <string.h>
#include "inc/oscar.h"
#include "leanXmotion.h"

/* Internal state */
uint32 Old_Sums[NUMFIELDS_X][NUMFIELDS_Y];
uint32 Sums[NUMFIELDS_X][NUMFIELDS_Y];

static bool Field_Active[NUMFIELDS_X][NUMFIELDS_Y] = {
	{1, 1, 1, 1, 1, 1, 1, 1}, 
	{1, 1, 1, 1, 1, 1, 1, 1}, 
	{1, 1, 1, 1, 1, 1, 1, 1}, 
	{1, 1, 1, 1, 1, 1, 1, 1}, 
	{1, 1, 1, 1, 1, 1, 1, 1}, 
	{1, 1, 1, 1, 1, 1, 1, 1}, 
	{1, 1, 1, 1, 1, 1, 1, 1}, 
	{1, 1, 1, 1, 1, 1, 1, 1} 
};

uint32 sum(struct OSC_PICTURE *pic, int tile_x, int tile_y) 
{
	int x, y;
	uint32 retval=0;
	uint8 *pix = pic->data;
	int fromx = pic->width/NUMFIELDS_X*tile_x;
	int fromy = pic->height/NUMFIELDS_Y*tile_y;
	int tox = fromx+pic->width/NUMFIELDS_X;
	int toy = fromy+pic->height/NUMFIELDS_Y;

	for (y=fromy; y<toy; y++) {
		for (x=fromx; x<tox; x++) {
			retval += pix[y*pic->width+x];
		}
	}
	return retval;
}

void mark(struct OSC_PICTURE *pic, int tile_x, int tile_y) 
{
	int x, y;
	uint8 *pix = pic->data;
	int fromx = pic->width/NUMFIELDS_X*tile_x;
	int fromy = pic->height/NUMFIELDS_Y*tile_y;
	int tox = fromx+pic->width/NUMFIELDS_X;
	int toy = fromy+pic->height/NUMFIELDS_Y;

	for (y=fromy; y<toy; y++) {
		pix[fromx+y*pic->width] = 255;
		pix[tox-1+y*pic->width] = 255;
	}
	for (x=fromx; x<tox; x++) {
		pix[x+fromy*pic->width] = 255;
		pix[x+(toy-1)*pic->width] = 255;
	}
}

/* 
 * is_alarm 
 */ 
bool is_alarm(struct OSC_PICTURE *pic)
{
	int x, y;
	int changed = 0;
	int numpix;

	numpix = pic->width/NUMFIELDS_X * pic->height/NUMFIELDS_Y;
	
	for (y=0; y<NUMFIELDS_Y; y++) 
		for (x=0; x<NUMFIELDS_X; x++) {
			Sums[x][y]=sum(pic, x, y);
			
			if (abs(Sums[x][y]-Old_Sums[x][y])/numpix > SENSITIVITY) {
				if (Field_Active[x][y]) 
					changed++;
				mark(pic, x, y);
			}
		}

	memcpy(Old_Sums, Sums, sizeof(Sums));

	return ((changed >= ALARM_THRESHOLD_LOW) && (changed < ALARM_THRESHOLD_HIGH));
}
