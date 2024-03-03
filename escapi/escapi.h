#pragma once

/* Extremely Simple Capture API */

#define ESCAPI		__declspec(dllexport)

struct SimpleCapParams
{
	/* Target buffer.
	 * Must be at least mWidth * mHeight * sizeof(int) of size!
	 */
	int * mTargetBuf;
	/* Buffer width */
	int mWidth;
	/* Buffer height */
	int mHeight;
};

struct SimpleRes
{
	UINT32 mWidth;
	UINT32 mHeight;
};

enum CAPTURE_PROPETIES
{
	CAPTURE_BRIGHTNESS,
	CAPTURE_CONTRAST,
	CAPTURE_HUE,
	CAPTURE_SATURATION,
	CAPTURE_SHARPNESS,
	CAPTURE_GAMMA,
	CAPTURE_COLORENABLE,
	CAPTURE_WHITEBALANCE,
	CAPTURE_BACKLIGHTCOMPENSATION,
	CAPTURE_GAIN,
	CAPTURE_PAN,
	CAPTURE_TILT,
	CAPTURE_ROLL,
	CAPTURE_ZOOM,
	CAPTURE_EXPOSURE,
	CAPTURE_IRIS,
	CAPTURE_FOCUS,
	CAPTURE_PROP_MAX
};


/* return the number of capture devices found */
ESCAPI int countCaptureDevices();

/* initCapture tries to open the video capture device.
 * Returns 0 on failure, 1 on success.
 * Note: Capture parameter values must not change while capture device
 *       is in use (i.e. between initCapture and deinitCapture).
 *       Do *not* free the target buffer, or change its pointer!
 */
ESCAPI int initCapture(unsigned int deviceno, struct SimpleCapParams *aParams);

/* deinitCapture closes the video capture device. */
ESCAPI void deinitCapture(unsigned int deviceno);

/* doCapture requests video frame to be captured. */
ESCAPI void doCapture(unsigned int deviceno);

/* isCaptureDone returns 1 when the requested frame has been captured.*/
ESCAPI int isCaptureDone(unsigned int deviceno);

/* Get the user-friendly name of a capture device. */
ESCAPI void getCaptureDeviceName(unsigned int deviceno, char *namebuffer, int bufferlength);

ESCAPI void getCaptureDeviceGUID(unsigned int deviceno, char *namebuffer, int bufferlength);

ESCAPI void getCaptureDeviceResolution(unsigned int deviceno, SimpleRes* resList, int* resnum);

/*
	On properties -
	- Not all cameras support properties at all.
	- Not all properties can be set to auto.
	- Not all cameras support all properties.
	- Messing around with camera properties may lead to weird results, so YMMV.
*/

/* Gets value (0..1) of a camera property (see CAPTURE_PROPERTIES, above) */
ESCAPI float getCapturePropertyValue(unsigned int deviceno, int prop);
/* Set camera property to a value (0..1) and whether it should be set to auto. */
ESCAPI int setCaptureProperty(unsigned int deviceno, int prop, float value, int autoval);

/*
	All error situations in ESCAPI are considered catastrophic. If such should
	occur, the following functions can be used to check which line reported the
	error, and what the HRESULT of the error was. These may help figure out
	what the problem is.
*/

/* Return line number of error, or 0 if no catastrophic error has occurred. */
ESCAPI int getCaptureErrorLine(unsigned int deviceno);
/* Return HRESULT of the catastrophic error, or 0 if none. */
ESCAPI int getCaptureErrorCode(unsigned int deviceno);


// Options accepted by above:
// Return raw data instead of converted rgb. Using this option assumes you know what you're doing.
#define CAPTURE_OPTION_RAWDATA 1 
// Mask to check for valid options - all options OR:ed together.
#define CAPTURE_OPTIONS_MASK		(CAPTURE_OPTION_RAWDATA) 


//function

