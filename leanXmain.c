/*	
	leanXstream, a streaming video server for the LeanXcam.
	Copyright (C) 2009 Reto Baettig
	
	This library is free software; you can redistribute it and/or modify it
	under the terms of the GNU Lesser General Public License as published by
	the Free Software Foundation; either version 2.1 of the License, or (at
	your option) any later version.
	
	This library is distributed in the hope that it will be useful, but
	WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
	General Public License for more details.
	
	You should have received a copy of the GNU Lesser General Public License
	along with this library; if not, write to the Free Software Foundation,
	Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "inc/oscar.h"
#include "leanXmotion.h"
#include "leanXalgos.h"
#include "leanXip.h"
#include "leanXtools.h"
#include <stdio.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define REG_AEC_AGC_ENABLE 0xaf
#define CAM_REG_RESERVED_0x20 0x20
#define CAM_REG_CHIP_CONTROL 0x07
#define BUF_SIZE 1000

/*! @brief The framework module dependencies of this application. */
struct OSC_DEPENDENCY deps[] = {
	{ "log", OscLogCreate, OscLogDestroy },
	{ "bmp", OscBmpCreate, OscBmpDestroy },
	{ "cam", OscCamCreate, OscCamDestroy },
	{ "vis", OscVisCreate, OscVisDestroy },
	{ "gpio", OscGpioCreate, OscGpioDestroy },
	{ "jpg", OscJpgCreate, OscJpgDestroy }
};

/*! @brief The system state and buffers of this application. */
struct SYSTEM {
	void *hFramework;
	void *hFileNameReader;

	int32 shutterWidth; /* Microseconds */
	uint8 frameBuffer1[OSC_CAM_MAX_IMAGE_WIDTH * OSC_CAM_MAX_IMAGE_HEIGHT]; 
	uint8 frameBuffer2[OSC_CAM_MAX_IMAGE_WIDTH * OSC_CAM_MAX_IMAGE_HEIGHT];
	uint8 doubleBufferIDs[2]; /* The frame buffer IDs of the frame
				   * buffers creating a double buffer. */
} sys;

/*********************************************************************//*!
 * @brief Initialize framework and system parameters
 *
 * @param s Pointer to the system state 
 *//*********************************************************************/
void initSystem(struct SYSTEM *s)
{
	OscCreate(&s->hFramework);
	
	/******* Load the framework module dependencies. **********/
	OscLoadDependencies(s->hFramework, deps, sizeof(deps)/sizeof(struct OSC_DEPENDENCY));

	OscLogSetConsoleLogLevel(WARN);
	OscLogSetFileLogLevel(WARN);
	
	#if defined(OSC_HOST)
		/* Setup file name reader (for host compiled version); read constant image */
		OscFrdCreate(s->hFramework);
		OscFrdCreateConstantReader(&s->hFileNameReader, "EAN13Example.bmp");
		OscCamSetFileNameReader(s->hFileNameReader);
	#endif
	
	/* Configure camera */
	OscCamPresetRegs();
	/* Set AGC and AEC */
	OscCamSetRegisterValue(REG_AEC_AGC_ENABLE, 0x3);
        /* Turn on continuous capture for this application. */
        OscCamSetRegisterValue(CAM_REG_CHIP_CONTROL, 0x388);
        /* Set the undocumented reserved almighty Micron register to the
           "optimal" value. */
        OscCamSetRegisterValue(CAM_REG_RESERVED_0x20, 0x3d5);

	OscCamSetAreaOfInterest(0, 0, OSC_CAM_MAX_IMAGE_WIDTH, OSC_CAM_MAX_IMAGE_HEIGHT);
	OscCamSetupPerspective(OSC_CAM_PERSPECTIVE_180DEG_ROTATE);

	OscCamSetFrameBuffer(0, OSC_CAM_MAX_IMAGE_WIDTH * OSC_CAM_MAX_IMAGE_HEIGHT, s->frameBuffer1, TRUE); 
	OscCamSetFrameBuffer(1, OSC_CAM_MAX_IMAGE_WIDTH * OSC_CAM_MAX_IMAGE_HEIGHT, s->frameBuffer2, TRUE); 

	s->doubleBufferIDs[0] = 0;
	s->doubleBufferIDs[1] = 1;
	OscCamCreateMultiBuffer(2, s->doubleBufferIDs); 

	OscCamSetupCapture(OSC_CAM_MULTI_BUFFER); 

} /* initSystem */

/*********************************************************************//*!
 * @brief Shut down system and close framework
 *
 * @param s Pointer to the system state 
 *//*********************************************************************/
void cleanupSystem(struct SYSTEM *s)
{
	/* Destroy modules */
	#if defined(OSC_HOST)
		OscFrdDestroy(s->hFramework);
	#endif

	OscUnloadDependencies(s->hFramework, deps, sizeof(deps)/sizeof(struct OSC_DEPENDENCY));
	/* Destroy framework */
	OscDestroy(s->hFramework);
} /* cleanupSysteim */

/*********************************************************************//*!
 * @brief Compress debayered BGR Picture and write it to a .JPG file
 *
 * Caution: There is no filename checking and no safety net yet ;-)
 *
 * @param pic A BGR Image
 * @param tmpbuf A temporary buffer for the compression algorithm
 * @param filename The filename where the compressed image has to be stored
 *//*********************************************************************/
void writeJPG(struct OSC_PICTURE *pic, unsigned char *jpgbuf, char *filename)
{
	unsigned char *jpgPicEnd;
	FILE *fp;

	jpgPicEnd = jpgbuf;
	jpgPicEnd = OscJpgEncode(pic, jpgbuf, 1024);

	fp = fopen (filename, "wb");
	fwrite (jpgbuf, 1, jpgPicEnd - jpgbuf, fp);
	fclose(fp);
}
/*********************************************************************//*!
 * @brief  The main program
 * 
 * Opens the camera and reads pictures as fast as possible
 * Makes a debayering of the image
 * Writes the debayered image to a buffer which can be read by
 * TCP clients on Port 8111. Several concurrent clients are allowed.
 * The simplest streaming video client looks like this:
 * 
 * nc 192.168.1.10 8111 | mplayer - -demuxer rawvideo -rawvideo w=376:h=240:format=bgr24:fps=100
 * 
 * Writes every 10th picture to a .jpg file in the Web Server Directory
 *//*********************************************************************/
int main(const int argc, const char * argv[])
{
	struct OSC_PICTURE calcPic;
	struct OSC_PICTURE rawPic;
	unsigned char *tmpbuf;
	int loops=0;	
	int numalarm=0;
	char filename[100];
	
	initSystem(&sys);

	ip_start_server();

	/* setup variables */
	rawPic.width = OSC_CAM_MAX_IMAGE_WIDTH;
	rawPic.height = OSC_CAM_MAX_IMAGE_HEIGHT;
	rawPic.type = OSC_PICTURE_GREYSCALE;

	/* calcPic width, height etc. are set in the debayering algos */
	calcPic.data = malloc(3 * OSC_CAM_MAX_IMAGE_WIDTH * OSC_CAM_MAX_IMAGE_HEIGHT);
	if (calcPic.data == 0)
		fatalerror("Did not get memory\n");
	tmpbuf = malloc(500000);
	if (tmpbuf == 0)
		fatalerror("Did not get memory\n");

	
	#if defined(OSC_TARGET)
		/* Take a picture, first time slower ;-) */
		usleep(10000); OscGpioTriggerImage(); usleep(10000);
		OscLog(DEBUG,"Triggered CAM ");
	#endif

	while(true) {

		OscCamReadPicture(OSC_CAM_MULTI_BUFFER, (void *) &rawPic.data, 0, 0);
		/* Take a picture */
		usleep(2000);
		OscCamSetupCapture(OSC_CAM_MULTI_BUFFER); 

		#if defined(OSC_TARGET)
			OscGpioTriggerImage();
		#else
			usleep(10000);
		#endif

		if (is_alarm(&rawPic)) {
			OscGpioSetTestLed(TRUE);
			printf("alarm\n");
			sprintf(filename, "/home/httpd/alarm_pic%02i.jpg", numalarm%16);
			writeJPG(&calcPic, tmpbuf, filename);
			numalarm++;
		} else {
			OscGpioSetTestLed(FALSE);
		}

		fastdebayerBGR(rawPic, &calcPic, NULL);

		ip_send_all((char *)calcPic.data, calcPic.width*calcPic.height*
                        OSC_PICTURE_TYPE_COLOR_DEPTH(calcPic.type)/8);

		loops+=1;
		if (loops%20 == 0) {
			writeJPG(&calcPic, tmpbuf, "/home/httpd/liveimage.jpg");
		}


                ip_do_work();
	
	}

	ip_stop_server();

	cleanupSystem(&sys);

	return 0;
} /* main */
