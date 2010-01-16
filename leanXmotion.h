/*	leanXmotion.h
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

/*!@file leanXmotion.h
 * @Configurable simple motion detection tools
 */
#ifndef H_LEANXMOTION
#define H_LEANXMOTION

/* Configuration of the alarms */
#define NUMFIELDS_X 8
#define NUMFIELDS_Y 8
#define NUMFIELDS (NUMFIELDS_X*NUMFIELDS_Y)
#define ALARM_THRESHOLD_LOW 4
#define ALARM_THRESHOLD_HIGH (NUMFIELDS/4*3)
#define SENSITIVITY 3

bool is_alarm(struct OSC_PICTURE *pic);

#endif
