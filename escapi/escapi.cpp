// escapi.cpp : 定义静态库的函数。
//

#define WIN32_LEAN_AND_MEAN             // 从 Windows 头文件中排除极少使用的内容

#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include "escapi.h"
#include "conversion.h"
#include "capture.h"
#include "scopedrelease.h"
#include "choosedeviceparam.h"

#define MAXDEVICES 16
struct SimpleCapParams gParams[MAXDEVICES];
ESCCapture *gDevice[MAXDEVICES] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
int gDoCapture[MAXDEVICES];
int gOptions[MAXDEVICES];


static void CheckForFail(int deviceno)
{
	if (!gDevice[deviceno])
		return;

	if (gDevice[deviceno]->mRedoFromStart)
	{
		gDevice[deviceno]->mRedoFromStart = 0;
		gDevice[deviceno]->deinitCapture();
		HRESULT hr = gDevice[deviceno]->initCapture(deviceno);
		if (FAILED(hr))
		{
			delete gDevice[deviceno];
			gDevice[deviceno] = 0;
		}
	}
}

int countCaptureDevices()
{
	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED(hr)) return 0;

	hr = MFStartup(MF_VERSION);

	if (FAILED(hr)) return 0;

	// choose device
	IMFAttributes *attributes = NULL;
	hr = MFCreateAttributes(&attributes, 1);
	ScopedRelease<IMFAttributes> attributes_s(attributes);

	if (FAILED(hr)) return 0;

	hr = attributes->SetGUID(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
	);
	if (FAILED(hr)) return 0;

	ChooseDeviceParam param = { 0 };
	hr = MFEnumDeviceSources(attributes, &param.mDevices, &param.mCount);

	if (FAILED(hr)) return 0;

	return param.mCount;
}

int initCapture(unsigned int deviceno, struct SimpleCapParams *aParams)
{
	if (deviceno > MAXDEVICES)
		return 0;
	if (aParams == NULL || aParams->mHeight <= 0 || aParams->mWidth <= 0 || aParams->mTargetBuf == 0)
		return 0;
	gDoCapture[deviceno] = 0;
	gParams[deviceno] = *aParams;
	gOptions[deviceno] = 0;

	if (gDevice[deviceno])
	{
		gDevice[deviceno]->deinitCapture();
		delete gDevice[deviceno];
		gDevice[deviceno] = 0;
	}
	gDevice[deviceno] = new ESCCapture;
	HRESULT hr = gDevice[deviceno]->initCapture(deviceno);
	if (FAILED(hr))
	{
		delete gDevice[deviceno];
		gDevice[deviceno] = 0;
		return 0;
	}
	return 1;
}

void deinitCapture(unsigned int deviceno)
{
	if (deviceno > MAXDEVICES)
		return;
	if (gDevice[deviceno])
	{
		gDevice[deviceno]->deinitCapture();
		delete gDevice[deviceno];
		gDevice[deviceno] = 0;
	}
}

void doCapture(unsigned int deviceno)
{
	if (deviceno > MAXDEVICES)
		return;
	CheckForFail(deviceno);
	gDoCapture[deviceno] = -1;
}

int isCaptureDone(unsigned int deviceno)
{
	if (deviceno > MAXDEVICES)
		return 0;
	CheckForFail(deviceno);
	if (gDoCapture[deviceno] == 1)
		return 1;
	return 0;
}

void getCaptureDeviceName(unsigned int deviceno, char *namebuffer, int bufferlength)
{
	if (deviceno > MAXDEVICES || !namebuffer || bufferlength <= 0)
		return;

	int i;
	namebuffer[0] = 0;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED(hr)) return;

	hr = MFStartup(MF_VERSION);

	if (FAILED(hr)) return;

	// choose device
	IMFAttributes *attributes = NULL;
	hr = MFCreateAttributes(&attributes, 1);
	ScopedRelease<IMFAttributes> attributes_s(attributes);

	if (FAILED(hr)) return;

	hr = attributes->SetGUID(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
	);

	if (FAILED(hr)) return;

	ChooseDeviceParam param = { 0 };
	hr = MFEnumDeviceSources(attributes, &param.mDevices, &param.mCount);

	if (FAILED(hr)) return;

	if (deviceno < (signed)param.mCount)
	{
		WCHAR *name = 0;
		UINT32 namelen = 255;
		hr = param.mDevices[deviceno]->GetAllocatedString(
			MF_DEVSOURCE_ATTRIBUTE_FRIENDLY_NAME,
			&name,
			&namelen
		);
		if (SUCCEEDED(hr) && name)
		{
			i = 0;
			while (i < bufferlength - 1 && i < (signed)namelen && name[i] != 0)
			{
				namebuffer[i] = (char)name[i];
				i++;
			}
			namebuffer[i] = 0;

			CoTaskMemFree(name);
		}
	}
}

void getCaptureDeviceGUID(unsigned int deviceno, char *namebuffer, int bufferlength)
{
	if (deviceno > MAXDEVICES || !namebuffer || bufferlength <= 0)
		return;

	int i, j;
	namebuffer[0] = 0;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (FAILED(hr)) return;

	hr = MFStartup(MF_VERSION);

	if (FAILED(hr)) return;

	// choose device
	IMFAttributes *attributes = NULL;
	hr = MFCreateAttributes(&attributes, 1);
	ScopedRelease<IMFAttributes> attributes_s(attributes);

	if (FAILED(hr)) return;

	hr = attributes->SetGUID(
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE,
		MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_GUID
	);

	if (FAILED(hr)) return;

	ChooseDeviceParam param = { 0 };
	hr = MFEnumDeviceSources(attributes, &param.mDevices, &param.mCount);

	if (FAILED(hr)) return;

	if (deviceno < (signed)param.mCount)
	{
		WCHAR *name = 0;
		UINT32 namelen = 255;
		hr = param.mDevices[deviceno]->GetAllocatedString(
			MF_DEVSOURCE_ATTRIBUTE_SOURCE_TYPE_VIDCAP_SYMBOLIC_LINK,
			&name,
			&namelen
		);
		if (SUCCEEDED(hr) && name)
		{
			i = 0;
			j = 0;
			bool start = false;
			while (i < bufferlength - 1 && i < (signed)namelen && name[i] != 0)
			{
				if (name[i] == '}') {
					start = false;
				}
				if (start) {
					namebuffer[j] = (char)name[i];
					j++;
				}
				if (name[i] == '{') {
					start = true;
				}
				i++;
			}
			namebuffer[j] = 0;

			CoTaskMemFree(name);
		}
	}
}

void getCaptureDeviceResolution(unsigned int deviceno, SimpleRes* resList, int* resnum)
{
	if (deviceno > MAXDEVICES || !resList || !resnum)
		return;

	if (!gDevice[deviceno]) {
		gDevice[deviceno] = new ESCCapture;
		HRESULT hr = gDevice[deviceno]->initCapture(deviceno);
		if (FAILED(hr))
		{
			delete gDevice[deviceno];
			gDevice[deviceno] = 0;
			return;
		}
	}

	gDevice[deviceno]->getResolution(resList, resnum);
}

float getCapturePropertyValue(unsigned int deviceno, int prop)
{
	if (deviceno > MAXDEVICES)
		return 0;
	if (!gDevice[deviceno])
		return 0;
	CheckForFail(deviceno);

	float val;
	int autoval;
	gDevice[deviceno]->getProperty(prop, val, autoval);
	return val;
}

int setCaptureProperty(unsigned int deviceno, int prop, float value, int autoval)
{
	if (deviceno > MAXDEVICES)
		return 0;
	if (!gDevice[deviceno])
		return 0;
	CheckForFail(deviceno);

	return gDevice[deviceno]->setProperty(prop, value, autoval);
}

int getCaptureErrorLine(unsigned int deviceno)
{
	if (deviceno > MAXDEVICES)
		return 0;
	if (!gDevice[deviceno])
		return 0;
	return gDevice[deviceno]->mErrorLine;
}

int getCaptureErrorCode(unsigned int deviceno)
{
	if (deviceno > MAXDEVICES)
		return 0;
	if (!gDevice[deviceno])
		return 0;
	return gDevice[deviceno]->mErrorCode;
}
