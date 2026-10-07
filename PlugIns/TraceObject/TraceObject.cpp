// TraceObject.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "TraceObject.h"
#include "BufStruct.h"
#include "ImageProc.h"
#include "TraceFeatureApi.h"
#include <limits.h>
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

//
//	Note!
//
//		If this DLL is dynamically linked against the MFC
//		DLLs, any functions exported from this DLL which
//		call into MFC must have the AFX_MANAGE_STATE macro
//		added at the very beginning of the function.
//
//		For example:
//
//		extern "C" BOOL PASCAL EXPORT ExportedFunction()
//		{
//			AFX_MANAGE_STATE(AfxGetStaticModuleState());
//			// normal function body here
//		}
//
//		It is very important that this macro appear in each
//		function, prior to any calls into MFC.  This means that
//		it must appear as the first statement within the 
//		function, even before any object variable declarations
//		as their constructors may generate calls into the MFC
//		DLL.
//
//		Please see MFC Technical Notes 33 and 58 for additional
//		details.
//

/////////////////////////////////////////////////////////////////////////////
// CTraceObjectApp

BEGIN_MESSAGE_MAP(CTraceObjectApp, CWinApp)
	//{{AFX_MSG_MAP(CTraceObjectApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTraceObjectApp construction

CTraceObjectApp::CTraceObjectApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CTraceObjectApp object

CTraceObjectApp theApp;
bool bLastPlugin = false;
#define GOOD_TRACE_DIST 50
#define BAD_TRACE_DIST 80
#define MAX_DIST_TO_ORG 150

static bool ValidRect(const aRect& rc, int w, int h)
{
	return rc.width > 0 && rc.height > 0 && rc.left >= 0 && rc.top >= 0 &&
		rc.left + rc.width <= w && rc.top + rc.height <= h;
}

static aRect FaceToFull(const aRect& face, int w, int h)
{
	aRect result = {face.left * 2, face.top * 4, face.width * 2, face.height * 4};
	if (result.left < 0) { result.width += result.left; result.left = 0; }
	if (result.top < 0) { result.height += result.top; result.top = 0; }
	if (result.left + result.width > w) result.width = w - result.left;
	if (result.top + result.height > h) result.height = h - result.top;
	return result;
}

static int SearchMargin(const TRACE_OBJECT* object)
{
	if (object->bSaveit) return 20;
	if (object->nMinDist <= GOOD_TRACE_DIST) return 5;
	if (object->nMinDist <= BAD_TRACE_DIST) return 10;
	return 15;
}

static bool BuildSearchRange(const aRect& objectRect, const aRect& face,
	int margin, int speedX, int speedY, aRect* range)
{
	int minLeft = objectRect.left - margin + speedX;
	int minTop = objectRect.top - margin + speedY;
	int maxLeft = objectRect.left + margin + speedX;
	int maxTop = objectRect.top + margin + speedY;
	if (minLeft < face.left) minLeft = face.left;
	if (minTop < face.top) minTop = face.top;
	int faceMaxLeft = face.left + face.width - objectRect.width;
	int faceMaxTop = face.top + face.height - objectRect.height;
	if (maxLeft > faceMaxLeft) maxLeft = faceMaxLeft;
	if (maxTop > faceMaxTop) maxTop = faceMaxTop;
	if (maxLeft < minLeft || maxTop < minTop)
		return false;
	range->left = minLeft;
	range->top = minTop;
	range->width = maxLeft - minLeft + objectRect.width;
	range->height = maxTop - minTop + objectRect.height;
	return true;
}

static void MarkTrackingFailure(TRACE_OBJECT* object, bool saveFeature)
{
	if (!object) return;
	object->nMinDist = INT_MAX;
	object->nBrokenTimes++;
	object->bBrokenTrace = true;
	object->bSaveit = true;
	object->spdxObj = object->spdyObj = 0;
	if (saveFeature)
		CopyFeatureVector(&object->fvObject, &object->fvObject_org);
}

static bool TrackObject(TRACE_OBJECT* object, const aBYTE* image, int w, int h,
	const aRect& face)
{
	if (!object) return false;
	if (!ValidRect(object->rcObject, w, h) || !ValidRect(face, w, h))
	{
		MarkTrackingFailure(object, true);
		return false;
	}
	aRect old = object->rcObject;
	int margin = SearchMargin(object);
	aRect range;
	if (!BuildSearchRange(old, face, margin, object->spdxObj, object->spdyObj, &range))
	{
		MarkTrackingFailure(object, true);
		return false;
	}
	int minDist = INT_MAX;
	FeatureVector best;
	InitFeatureVector(&best);
	aPOINT point = CompareFromImage(&object->fvObject, (aBYTE*)image, w, h,
		old, range, &minDist, &best);
	if (minDist == INT_MAX)
	{
		MarkTrackingFailure(object, true);
		return false;
	}
	int oldCenterX = old.left + old.width / 2;
	int oldCenterY = old.top + old.height / 2;
	int newCenterX = point.x + old.width / 2;
	int newCenterY = point.y + old.height / 2;
	int nextSpeedX = object->spdxObj;
	int nextSpeedY = object->spdyObj;
	int denominator = minDist + 256;
	if (denominator > 0)
	{
		nextSpeedX = (int)(((long long)object->spdxObj * minDist +
			(long long)(newCenterX - oldCenterX) * 256) / denominator);
		nextSpeedY = (int)(((long long)object->spdyObj * minDist +
			(long long)(newCenterY - oldCenterY) * 256) / denominator);
	}
	int originalDistance = FV_Distance(&object->fvObject_org, &best, 0, 0);
	if (originalDistance > MAX_DIST_TO_ORG)
	{
		MarkTrackingFailure(object, true);
		return false;
	}
	object->nMinDist = minDist;
	object->rcObject.left = point.x;
	object->rcObject.top = point.y;
	object->spdxObj = nextSpeedX;
	object->spdyObj = nextSpeedY;
	if (minDist <= GOOD_TRACE_DIST)
	{
		CopyFeatureVector(&object->fvObject, &best);
		object->bSaveit = false;
		object->bBrokenTrace = false;
		object->nBrokenTimes = 0;
	}
	else if (minDist <= BAD_TRACE_DIST)
	{
		CopyFeatureVector(&object->fvObject, &best);
		UpdateVectorsFrom(&object->fvObject, &object->fvObject_org, 50);
		object->bSaveit = false;
		object->nBrokenTimes++;
		if (object->nBrokenTimes >= 10) object->bBrokenTrace = true;
	}
	else
	{
		object->nBrokenTimes++;
		if (object->bSaveit)
			object->bBrokenTrace = true;
		object->bSaveit = true;
		CopyFeatureVector(&object->fvObject, &object->fvObject_org);
	}
	if (object->bBrokenTrace)
	{
		object->rcObject = old;
		object->spdxObj = object->spdyObj = 0;
	}
	return !object->bBrokenTrace;
}


DLL_EXP void ON_PLUGIN_BELAST(bool bLast)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	bLastPlugin = bLast;
}

char sInfo[] = "Plugin3-TraceObject: eye and nose tracking";

DLL_EXP LPCTSTR ON_PLUGININFO(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	return sInfo;
}

DLL_EXP void ON_INITPLUGIN(LPVOID lpParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	//theApp.dlg.Create(IDD_PLUGIN_SETUP);
	//theApp.dlg.ShowWindow(SW_HIDE);
}

DLL_EXP int ON_PLUGINCTRL(int nMode,void* pParameter)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	int nRet = 0;
	switch(nMode)
	{
	case 0:
		{
			//theApp.dlg.ShowWindow(SW_SHOWNORMAL);
			//theApp.dlg.SetWindowPos(NULL,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_FRAMECHANGED);
		}
		break;
	}
	return nRet;
}


/*******************************************************************/
//�۱Ǹ���
/*******************************************************************/
DLL_EXP void ON_PLUGINRUN(int w,int h,BYTE* pYBits,BYTE* pUBits,BYTE* pVBits,BYTE* pBuffer)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	if (!pBuffer || !pYBits || w <= 0 || h <= 0) return;
	BUF_STRUCT* pBS = (BUF_STRUCT*)pBuffer;
	if (pBS->bNotInited || !pBS->grayBmp || !pBS->pOtherVars || !pBS->bLastEyeChecked) return;
	aRect face = FaceToFull(pBS->rcnFace, w, h);
	if (!ValidRect(face, w, h)) return;
	TRACE_OBJECT* left = &pBS->pOtherVars->objLefteye;
	TRACE_OBJECT* right = &pBS->pOtherVars->objRighteye;
	TRACE_OBJECT* nose = &pBS->pOtherVars->objNose;
	bool allOk = TrackObject(left, pBS->grayBmp, w, h, face);
	allOk = TrackObject(right, pBS->grayBmp, w, h, face) && allOk;
	allOk = TrackObject(nose, pBS->grayBmp, w, h, face) && allOk;
	if (allOk)
	{
		pBS->ptTheLeftEye.x = left->rcObject.left + left->rcObject.width / 2;
		pBS->ptTheLeftEye.y = left->rcObject.top + left->rcObject.height / 2;
		pBS->ptTheRightEye.x = right->rcObject.left + right->rcObject.width / 2;
		pBS->ptTheRightEye.y = right->rcObject.top + right->rcObject.height / 2;
		pBS->ptTheNose.x = nose->rcObject.left + nose->rcObject.width / 2;
		pBS->ptTheNose.y = nose->rcObject.top + nose->rcObject.height / 2;
		pBS->EyePosConfirm = true;
		pBS->nFVTop = 1;
		if (bLastPlugin)
		{
			DrawRectangle(pYBits, w, h, left->rcObject, RGB(255, 220, 0), true);
			DrawRectangle(pYBits, w, h, right->rcObject, RGB(255, 220, 0), true);
			DrawRectangle(pYBits, w, h, nose->rcObject, RGB(180, 180, 0), true);
		}
	}
	else
	{
		pBS->bLastEyeChecked = false;
		pBS->EyePosConfirm = false;
		pBS->EyeBallConfirm = false;
	}
}
/*******************************************************************/

/*******************************************************************/
DLL_EXP void ON_PLUGINEXIT()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//ģ��״̬�л�
	//theApp.dlg.DestroyWindow();
}

