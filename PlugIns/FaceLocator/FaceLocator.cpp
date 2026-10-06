// FaceLocator.cpp : Plugin 2 - skin color face detection and localization
//
// Algorithm overview (manual section 5, part 3):
//   Input : clrBmp_1d8  (1/2W x 1/4H, YUV422 planar)
//   Output: pBS->rcnFace, pBS->nFacePixelNum, face rectangle drawn on pYBits
//
//   Steps:
//     1. Equalize the Y plane to compensate for illumination
//     2. Skin color modeling  -> binary tempImage (1/4W x 1/4H)
//     3. Morphological: 3x3 opening, 3x3 closing
//     4. Connected component labeling (4-connectivity)
//     5. Keep largest region (= face), compute bounding rect
//     6. Store results, draw display box, free tempImage

#include "stdafx.h"
#include "FaceLocator.h"
#include "BufStruct.h"
#include "ImageProc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

/////////////////////////////////////////////////////////////////////////////
// CFaceLocatorApp
BEGIN_MESSAGE_MAP(CFaceLocatorApp, CWinApp)
	//{{AFX_MSG_MAP(CFaceLocatorApp)
	//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CFaceLocatorApp::CFaceLocatorApp()
{
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CFaceLocatorApp object
CFaceLocatorApp theApp;

char sInfo[] = "Plugin2-FaceLocator: skin color face detection";
bool bLastPlugin = false;

DLL_EXP void ON_PLUGIN_BELAST(bool bLast)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	bLastPlugin = bLast;
}

DLL_EXP LPCTSTR ON_PLUGININFO(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return sInfo;
}

DLL_EXP void ON_INITPLUGIN(LPVOID lpParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
}

DLL_EXP int ON_PLUGINCTRL(int nMode, void* pParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	return 0;
}

/*****************************************************************************
 *  Face detection and localization
 *****************************************************************************/

static void EqualizeLuminance(aBYTE* image, int width, int height)
{
	int histogram[256];
	aBYTE lookup[256];
	int pixelCount = width * height;
	int cumulative = 0;
	int firstCumulative = 0;
	int i;

	if (!image || width <= 0 || height <= 0 || pixelCount <= 0)
		return;

	memset(histogram, 0, sizeof(histogram));
	for (i = 0; i < pixelCount; i++)
		histogram[image[i]]++;

	for (i = 0; i < 256; i++)
	{
		cumulative += histogram[i];
		if (cumulative > 0)
		{
			firstCumulative = cumulative;
			break;
		}
	}

	// A constant image has no contrast to equalize.
	if (firstCumulative == pixelCount)
		return;

	cumulative = 0;
	for (i = 0; i < 256; i++)
	{
		int value;
		cumulative += histogram[i];
		value = (cumulative <= firstCumulative)
			? 0
			: (cumulative - firstCumulative) * 255 /
			  (pixelCount - firstCumulative);
		lookup[i] = (aBYTE)value;
	}

	for (i = 0; i < pixelCount; i++)
		image[i] = lookup[image[i]];
}

// Erode a binary image with an N x N structuring element using 2D minimum filter
static void Erode(aBYTE* img, int w, int h, int N)
{
	Minimum_2D(img, w, h, N);
}

// Dilate a binary image with an N x N structuring element using 2D maximum filter
static void Dilate(aBYTE* img, int w, int h, int N)
{
	Maximum_2D(img, w, h, N);
}

DLL_EXP void ON_PLUGINRUN(int w, int h, BYTE* pYBits, BYTE* pUBits, BYTE* pVBits, BYTE* pBuffer)
{
// pYBits  size: w * h
// pUBits, pVBits size: w * h / 2
// pBuffer size: w * h * 4
// Note: w must be a multiple of 16
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	BUF_STRUCT* pBS = (BUF_STRUCT*)pBuffer;

	// ---- Step 1: Illumination compensation ---------------------------------
	// Equalize only the Y plane. U/V remain unchanged because the skin model
	// below uses fixed chrominance ranges.
	int yWidth = w / 2;
	int yHeight = h / 4;
	EqualizeLuminance(pBS->clrBmp_1d8, yWidth, yHeight);

	// Dimensions of the working binary image (same as U/V plane of clrBmp_1d8)
	int tw = w / 4;  // tempImage width  = 1/4 W
	int th = h / 4;  // tempImage height = 1/4 H

	// Allocate temporary binary image from the dynamic heap
	aBYTE* tempImage = myHeapAlloc(tw * th);
	if (!tempImage)
	{
		ShowDebugMessage("FaceLocator: myHeapAlloc failed, heap exhausted");
		return;
	}

	// ---- Step 2: Skin color modeling ----------------------------------------
	// clrBmp_1d8 layout (YUV422 planar at 1/2W x 1/4H):
	//   Y : (w/2)*(h/4) bytes starting at clrBmp_1d8
	//   U : (w/4)*(h/4) bytes starting at clrBmp_1d8 + (w/2)*(h/4)
	//   V : (w/4)*(h/4) bytes starting at clrBmp_1d8 + (w/2)*(h/4) + (w/4)*(h/4)
	// U and V planes are exactly tw*th pixels, matching tempImage dimensions.
	// IMPORTANT: U and V must NOT be modified during processing (used later).
	aBYTE* pU_1d8 = pBS->clrBmp_1d8 + (w / 2) * (h / 4);
	aBYTE* pV_1d8 = pBS->clrBmp_1d8 + (w / 2) * (h / 4) + tw * th;

	// Condition: U in [85,126] AND V in [130,165] -> skin pixel (formula 5.9)
	// byHistMap_U/V are lookup tables set to 1 inside the range, 0 outside
	int skinCount = 0;
	for (int k = 0; k < tw * th; k++)
	{
		if (pBS->pOtherVars->byHistMap_U[pU_1d8[k]] &&
		    pBS->pOtherVars->byHistMap_V[pV_1d8[k]])
		{
			tempImage[k] = 255;
			skinCount++;
		}
		else
		{
			tempImage[k] = 0;
		}
	}
	ShowDebugMessage("FaceLocator: skin pixels=%d / %d", skinCount, tw * th);

	// ---- Step 3: Morphological processing -----------------------------------
	// 3x3 opening (erode then dilate): removes isolated noise points
	Erode(tempImage, tw, th, 3);
	Dilate(tempImage, tw, th, 3);
	// Use a small 3x3 closing so nearby background regions are not bridged
	// into the face candidate at this already-downsampled resolution.
	Dilate(tempImage, tw, th, 7);
	Erode(tempImage, tw, th, 7);
	ShowDebugMessage("FaceLocator: morphology done");

	// ---- Step 4: Connected component labeling (4-connectivity) -------------
	// RegionMark labels each connected region 1..N in tempImage and returns N.
	// Pixel values become region labels (byte, so max 255 regions).
	int nMaxMark = RegionMark(tempImage, tw, th);
	ShowDebugMessage("FaceLocator: RegionMark -> %d regions", nMaxMark);

	// ---- Step 5: Find and keep largest region (= face) ----------------------
	if (nMaxMark <= 0)
	{
		// No skin region detected at all
		ShowDebugMessage("FaceLocator: no skin region, face not found");
		memset(&pBS->rcnFace, 0, sizeof(aRect));
		pBS->nFacePixelNum = 0;
		myHeapFree(tempImage);
		return;
	}

	// Count pixels for each label (labels are 1..nMaxMark, max 255)
	int counts[256];
	memset(counts, 0, sizeof(counts));
	for (int k = 0; k < tw * th; k++)
	{
		aBYTE label = tempImage[k];
		if (label > 0)
			counts[label]++;
	}

	// Find the label with the most pixels (= face region)
	int faceLabel = 1, maxCount = 0;
	for (int l = 1; l <= nMaxMark && l <= 255; l++)
	{
		if (counts[l] > maxCount)
		{
			maxCount  = counts[l];
			faceLabel = l;
		}
	}
	ShowDebugMessage("FaceLocator: face label=%d pixels=%d", faceLabel, maxCount);

	// Keep only face region (set to 255), remove everything else (set to 0)
	for (int k = 0; k < tw * th; k++)
		tempImage[k] = (tempImage[k] == (aBYTE)faceLabel) ? 255 : 0;

	// ---- Step 6: Compute face bounding rectangle ----------------------------
	aRect rcInTemp;
	memset(&rcInTemp, 0, sizeof(aRect));
	int nPixelCount = 0;
	bool bFound = GetEspGrayRect(tempImage, tw, th, 255, &rcInTemp, &nPixelCount);

	if (!bFound || nPixelCount < 10)
	{
		ShowDebugMessage("FaceLocator: bounding rect not found or region too small");
		memset(&pBS->rcnFace, 0, sizeof(aRect));
		pBS->nFacePixelNum = 0;
		myHeapFree(tempImage);
		return;
	}

	// ---- Neck exclusion: trim bounding rect bottom to face aspect ratio ----
	// A face is roughly as wide as it is tall (W:H ~ 1:1.5 at most).
	// If the skin region is much taller than wide it is grabbing the neck;
	// trim from the bottom to remove the neck portion.
	ShowDebugMessage("FaceLocator: rcInTemp before trim=(%d,%d,%d,%d)",
		rcInTemp.left, rcInTemp.top, rcInTemp.width, rcInTemp.height);
	{
		int maxH = rcInTemp.width * 6 / 5;
		if (maxH < 1) maxH = 1;
		if (rcInTemp.height > maxH)
			rcInTemp.height = maxH;
	}
	ShowDebugMessage("FaceLocator: rcInTemp after trim=(%d,%d,%d,%d)",
		rcInTemp.left, rcInTemp.top, rcInTemp.width, rcInTemp.height);

	// ---- Step 7: Store results in pBS ---------------------------------------
	// tempImage is tw x th (1/4W x 1/4H).
	// clrBmp_1d8 Y channel is (w/2) x (h/4) = 2*tw x th.
	// Horizontal coordinates must be scaled x2; vertical stays the same.
	// nFacePixelNum also x2 because each tempImage pixel = 2 Y pixels in clrBmp_1d8.
	pBS->rcnFace.left   = rcInTemp.left  * 2;
	pBS->rcnFace.top    = rcInTemp.top;
	pBS->rcnFace.width  = rcInTemp.width * 2;
	pBS->rcnFace.height = rcInTemp.height;
	pBS->nFacePixelNum  = nPixelCount * 2;
	ShowDebugMessage("FaceLocator: rcnFace=(%d,%d,%d,%d) pixelNum=%d",
		pBS->rcnFace.left, pBS->rcnFace.top,
		pBS->rcnFace.width, pBS->rcnFace.height,
		pBS->nFacePixelNum);

	// ---- Step 8: Write face mask into Y channel of clrBmp_1d8 --------------
	// Upscale tempImage (tw x th) horizontally x2 into clrBmp_1d8 Y channel (2*tw x th).
	// This overwrites the Y channel, which is intentional and allowed.
	// U and V channels are untouched.
	ReSample(tempImage, tw, th, w / 2, h / 4, false, true, pBS->clrBmp_1d8);

	// ---- Step 9: Draw face bounding box on full-resolution display ----------
	// rcnFace is in clrBmp_1d8 space (w/2 x h/4).
	// pYBits is (w x h): horizontal x2, vertical x4.
	aRect rcDisplay;
	rcDisplay.left   = pBS->rcnFace.left   * 2;
	rcDisplay.top    = pBS->rcnFace.top    * 4;
	rcDisplay.width  = pBS->rcnFace.width  * 2;
	rcDisplay.height = pBS->rcnFace.height * 4;
	// Clamp to image bounds so all four edges are drawn (out-of-bounds lines are silently skipped)
	if (rcDisplay.left < 0) { rcDisplay.width  += rcDisplay.left; rcDisplay.left = 0; }
	if (rcDisplay.top  < 0) { rcDisplay.height += rcDisplay.top;  rcDisplay.top  = 0; }
	if (rcDisplay.left + rcDisplay.width  > w) rcDisplay.width  = w - rcDisplay.left;
	if (rcDisplay.top  + rcDisplay.height > h) rcDisplay.height = h - rcDisplay.top;
	DrawRectangle(pYBits, w, h, rcDisplay, RGB(200, 200, 200), true); // white-ish box

	// ---- Debug display (only when this is the last active plugin) -----------
	// Show the binary face mask at the top-left corner of the screen.
	if (bLastPlugin)
	{
		// Top-left: binary face mask (tempImage, tw x th)
		CopyToRect(tempImage, pYBits, tw, th, w, h, 0, 0, true);
		// Top-right: downsampled Y image from clrBmp_1d8 for comparison
		// (clrBmp_1d8 Y channel was just overwritten with the mask, show grayBmp_1d16 instead)
		CopyToRect(pBS->grayBmp_1d16, pYBits, tw, th, w, h, w - tw, 0, true);
	}

	// ---- Free temporary memory ----------------------------------------------
	myHeapFree(tempImage);
}

DLL_EXP void ON_PLUGINEXIT()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
}
