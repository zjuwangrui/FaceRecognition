// ImagePrepare.cpp : Plugin 1 - frame capture, copy, and downsampling

#include "stdafx.h"
#include "ImagePrepare.h"
#include "BufStruct.h"
#include "ImageProc.h"
#include "math.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//	Note!
//
//		If this DLL is dynamically linked against the MFC DLLs, any functions
//		exported from this DLL which call into MFC must have the AFX_MANAGE_STATE
//		macro added at the very beginning of the function.
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		Please see MFC Technical Notes 33 and 58 for additional details.

/////////////////////////////////////////////////////////////////////////////
// CImagePrepareApp
BEGIN_MESSAGE_MAP(CImagePrepareApp, CWinApp)
	//{{AFX_MSG_MAP(CImagePrepareApp)
	//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

CImagePrepareApp::CImagePrepareApp()
{
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CImagePrepareApp object
CImagePrepareApp theApp;

// extern "C" __declspec(dllimport): import functions from TraceFeature.dll in C linkage
DLL_INP void InitFeatureBuffer(BUF_STRUCT* pBS);
DLL_INP void InitFeatureVector(FeatureVector* pThis);

char sInfo[] = "Plugin1-ImagePrepare: frame capture, copy, and downsample";
bool bLastPlugin = false;

// extern "C" _declspec(dllexport): export functions from this DLL in C linkage
// bLastPlugin=true means this is the last active plugin in the chain
DLL_EXP void ON_PLUGIN_BELAST(bool bLast)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState()); // switch MFC module state for DLL
	bLastPlugin = bLast;
}

// Returns a description string for this plugin
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

// Initialize a TRACE_OBJECT tracking structure to its default state
void InitTraceObject(TRACE_OBJECT* pThis, char name[8])
{
	memset(&pThis->rcObject, 0, sizeof(aRect)); // clear tracking region
	pThis->spdxObj      = 0;                    // horizontal velocity
	pThis->spdyObj      = 0;                    // vertical velocity
	pThis->nMinDist     = 0x7fffffff;           // minimum feature distance (reset to max)
	pThis->bBrokenTrace = false;                // tracking-lost flag
	pThis->nBrokenTimes = 0;                    // consecutive tracking-failure count
	pThis->bSaveit      = false;                // lost-tracking save flag
	// Initialize feature vectors for this tracking object
	InitFeatureVector(&pThis->fvObject);        // current feature
	InitFeatureVector(&pThis->fvObject_org);    // original (reference) feature
	strncpy(pThis->sName, name, 8);
	pThis->sName[7] = 0;
}

DLL_EXP void ON_PLUGINRUN(int w, int h, BYTE* pYBits, BYTE* pUBits, BYTE* pVBits, BYTE* pBuffer)
{
// pYBits size: w*h
// pUBits and pVBits size: w*h/2  (YUV422: one U/V sample per two horizontal pixels)
// pBuffer size: w*h*4  (allocated by main app in VideoCallbackProc)
// Note: w must be a multiple of 16
	AFX_MANAGE_STATE(AfxGetStaticModuleState());

	BUF_STRUCT* pBS = (BUF_STRUCT*)pBuffer;

	// ---- First frame only: initialize memory layout and all state ----
	if (pBS->bNotInited)
	{
		// 1. Set image size
		pBS->W = w;
		pBS->H = h;

		// 2. Layout all buffer pointers inside pBuffer (manual formula 5.5).
		//    Pointers must NOT be changed after initialization.
		pBS->colorBmp             = pBuffer + sizeof(BUF_STRUCT);
		pBS->grayBmp              = pBS->colorBmp;                               // shared start: Y plane = grayscale
		pBS->clrBmp_1d8           = pBS->grayBmp           + w*h*2;             // 1/2W x 1/4H color (YUV422 planar)
		pBS->grayBmp_1d16         = pBS->clrBmp_1d8        + w*h/4;             // 1/4W x 1/4H grayscale
		pBS->TempImage1d8         = pBS->grayBmp_1d16      + w*h/16;            // temporary working buffer
		pBS->lastImageQueue1d16m8 = pBS->TempImage1d8      + w*h/8;             // 8-frame history queue
		pBS->pOtherVars  = (OTHER_VARS*)(pBS->lastImageQueue1d16m8 + w*h/2);    // OTHER_VARS struct
		pBS->pOtherData  = (aBYTE*)pBS->pOtherVars + sizeof(OTHER_VARS);        // dynamic heap start
		for (int i = 0; i < 8; i++)
			pBS->pImageQueue[i] = pBS->lastImageQueue1d16m8 + i*(w*h/16);       // 8 individual frame pointers

		// 3. Initialize scalar fields (manual section 5 step 2)
		pBS->cur_allocSize    = 0;
		pBS->allocTimes       = 0;
		pBS->cur_maxallocsize = 0;
		pBS->bLastEyeChecked  = false;  // tracking model not yet established
		pBS->EyeBallConfirm   = true;
		pBS->EyePosConfirm    = true;
		pBS->nImageQueueIndex = -1;     // -1 means first frame not yet stored
		pBS->nLastImageIndex  = -1;

		// 4. Skin color histogram maps (manual formula 5.6 / 5.7)
		//    byHistMap_U[i]=1 if 85<=i<=126, byHistMap_V[i]=1 if 130<=i<=165
		for (int i = 0; i < 256; i++)
		{
			pBS->pOtherVars->byHistMap_U[i] = (i >= 85  && i <= 126) ? 1 : 0;
			pBS->pOtherVars->byHistMap_V[i] = (i >= 130 && i <= 165) ? 1 : 0;
		}

		// 5. Initialize nose, left eye, right eye tracking objects (manual table 5.3)
		InitTraceObject(&pBS->pOtherVars->objNose,          "Nose");
		InitTraceObject(&pBS->pOtherVars->objLefteye,       "LEye");
		InitTraceObject(&pBS->pOtherVars->objRighteye,      "REye");
		InitTraceObject(&pBS->pOtherVars->objLefteyeClose,  "LClose");
		InitTraceObject(&pBS->pOtherVars->objRighteyeClose, "RClose");

		// 6. Initialize global feature-processing buffer in TraceFeature.dll
		InitFeatureBuffer(pBS);

		// 7. Remaining space after fixed regions is the dynamic heap (manual formula 5.8)
		pBS->max_allocSize = w*h*17/16 - (int)sizeof(BUF_STRUCT) - (int)sizeof(OTHER_VARS);

		// 8. Initialize heap allocator (sets up pBS pointer inside ImageProc.lib)
		myHeapAllocInit(pBS);

		// 9. Clear initialization flag -- must be the very last step
		pBS->bNotInited = false;

		ShowDebugMessage("ImagePrepare init OK: W=%d H=%d heap=%d bytes", w, h, pBS->max_allocSize);
	}

	// ---- Every frame ----

	// Point display image at raw Y channel (other plugins may overwrite this later)
	pBS->displayImage = pYBits;

	// Copy planar YUV422 input into colorBmp: layout is [ Y(w*h) | U(w*h/2) | V(w*h/2) ]
	memcpy(pBS->colorBmp,           pYBits, w*h);
	memcpy(pBS->colorBmp + w*h,     pUBits, w*h/2);
	memcpy(pBS->colorBmp + w*h*3/2, pVBits, w*h/2);

	// Downsample to clrBmp_1d8 (1/2W x 1/4H, YUV422 planar)
	// Y: (w, h) -> (w/2, h/4)
	ReSample(pBS->colorBmp,            w,   h, w/2, h/4, false, true, pBS->clrBmp_1d8);
	// U: (w/2, h) -> (w/4, h/4)
	ReSample(pBS->colorBmp + w*h,      w/2, h, w/4, h/4, false, true, pBS->clrBmp_1d8 + (w/2)*(h/4));
	// V: (w/2, h) -> (w/4, h/4)
	ReSample(pBS->colorBmp + w*h*3/2,  w/2, h, w/4, h/4, false, true, pBS->clrBmp_1d8 + (w/2)*(h/4) + (w/4)*(h/4));

	// Downsample to grayBmp_1d16 (1/4W x 1/4H, grayscale Y channel only)
	ReSample(pBS->grayBmp, w, h, w/4, h/4, false, true, pBS->grayBmp_1d16);

	// Debug display: copy downsampled results back into pYBits so they appear on screen.
	// Only runs when ImagePrepare is the last active plugin (others disabled).
	if (bLastPlugin)
	{
		// Top-left: show grayBmp_1d16 (grayscale, 1/4 size)
		CopyToRect(pBS->grayBmp_1d16, pYBits, w/4, h/4, w, h, 0,   0, true);
		// Top-right: show clrBmp_1d8 (color, 1/2 width x 1/4 height)
		CopyToRect(pBS->clrBmp_1d8,   pYBits, w/2, h/4, w, h, w/2, 0, false);
	}
}

DLL_EXP void ON_PLUGINEXIT()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
}
