// TraceFeature.cpp : Defines the initialization routines for the DLL.
//
#include "stdafx.h"
#include "TraceFeature.h"
#include "TraceFeatureApi.h"
#include "ImageProc.h"
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
// CTraceFeatureApp

BEGIN_MESSAGE_MAP(CTraceFeatureApp, CWinApp)
	//{{AFX_MSG_MAP(CTraceFeatureApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CTraceFeatureApp construction

CTraceFeatureApp::CTraceFeatureApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CTraceFeatureApp object
CTraceFeatureApp theApp;
#define ByteToVectorDouble(a) ((int)(a))/255.0
#define WordToVectorDouble(a) ((int)(a))/65535.0
#define MAX_RH	256			//�������ƥ��������Χ�ĸ߶�
aBYTE* pBuffer;//256*4*8=8196�ֽڵĻ���������
//pLineSumC��pLineSumD��Ϊ�˿�������һ����������õģ������뿴InitFromImageCols������˵��
aPOINT* pLineSumC[2]={NULL,NULL};
aPOINT* pLineSumD[2][2]={{NULL,NULL},{NULL,NULL}};
/*******************************************************************/
//��ʼ��ȫ��ָ����� pLineSumC��pLineSumD
/*******************************************************************/
DLL_EXP void InitFeatureBuffer(BUF_STRUCT* pBS)
{
	pBuffer = (aBYTE*)pBS->pOtherVars->FeaProcBuf;
	pLineSumC[0] = (aPOINT*)pBuffer;
	pLineSumC[1] = pLineSumC[0]+MAX_RH;
	pLineSumD[0][0] = pLineSumC[1]+MAX_RH;
	pLineSumD[0][1] = pLineSumD[0][0]+MAX_RH;
	pLineSumD[1][0] = pLineSumD[0][1]+MAX_RH;
	pLineSumD[1][1] = pLineSumD[1][0]+MAX_RH;
	//����ռ��MAX_RH*6*8�ֽ�=12K bytes,ʣ��4K bytes
	pBuffer = (aBYTE*)(pLineSumD[1][1]+MAX_RH);
}
/*******************************************************************/
//��ʼ�������������������Ϊ������ṹ����
/*******************************************************************/
// Feature-vector extraction, comparison and update helpers
/*******************************************************************/
typedef char FeatureVectorStorageMustFit[
	(sizeof(FeatureVector4P) * 5 <= MAX_FEA_SIZE) ? 1 : -1];

static FeatureVector4P* Root(FeatureVector* pThis)
{
	return pThis ? (FeatureVector4P*)pThis->Vector : NULL;
}

static const FeatureVector4P* Root(const FeatureVector* pThis)
{
	return pThis ? (const FeatureVector4P*)pThis->Vector : NULL;
}

static void BindFeatureVector(FeatureVector* pThis)
{
	if (!pThis) return;
	FeatureVector4P* root = Root(pThis);
	FeatureVector4P* nodes = (FeatureVector4P*)pThis->Vector;
	root->pNL_LeftTop = nodes + 1;
	root->pNL_RightTop = nodes + 2;
	root->pNL_LeftBottom = nodes + 3;
	root->pNL_RightBottom = nodes + 4;
}

static void CopyFeatureData(FeatureVector* destination, const FeatureVector* source)
{
	if (!destination || !source) return;
	FeatureVector4P* dst = Root(destination);
	const FeatureVector4P* src = Root(source);
	for (int i = 0; i < 5; i++)
	{
		dst[i].nLevels = src[i].nLevels;
		for (int j = 0; j < 4; j++) dst[i].Vector[j] = src[i].Vector[j];
		dst[i].faceColor_WeightCenter = src[i].faceColor_WeightCenter;
	}
	destination->size = source->size;
	BindFeatureVector(destination);
}

static int ClampCoordinate(long long value)
{
	if (value < -1024) return -1024;
	if (value > 1024) return 1024;
	return (int)value;
}

static bool ValidRect(const aRect& rc, int w, int h)
{
	return rc.width > 0 && rc.height > 0 && rc.left >= 0 && rc.top >= 0 &&
		rc.left + rc.width <= w && rc.top + rc.height <= h;
}

static void ExtractQuadrants(const aBYTE* image, int lineWidth, const aRect& rc,
	FeatureVector4P* feature)
{
	for (int quadrant = 0; quadrant < 4; quadrant++)
	{
		int left = rc.left + (quadrant & 1) * rc.width / 2;
		int top = rc.top + (quadrant >> 1) * rc.height / 2;
		int right = rc.left + ((quadrant & 1) + 1) * rc.width / 2;
		int bottom = rc.top + ((quadrant >> 1) + 1) * rc.height / 2;
		long long sum = 0, xSum = 0, ySum = 0;
		for (int y = top; y < bottom; y++)
			for (int x = left; x < right; x++)
			{
				int value = image[y * lineWidth + x];
				sum += value;
				xSum += (long long)value * (x - rc.left);
				ySum += (long long)value * (y - rc.top);
			}
		int cx = (left + right) / 2 - rc.left;
		int cy = (top + bottom) / 2 - rc.top;
		if (sum > 0)
		{
			cx = (int)(xSum / sum);
			cy = (int)(ySum / sum);
		}
		feature->Vector[quadrant].x = ClampCoordinate((long long)cx * 2048 / rc.width - 1024);
		feature->Vector[quadrant].y = ClampCoordinate((long long)cy * 2048 / rc.height - 1024);
	}
}

DLL_EXP void InitFeatureVector(FeatureVector* pThis)
{
	if (!pThis) return;
	memset(pThis, 0, sizeof(*pThis));
	pThis->size = sizeof(FeatureVector4P) * 5;
	FeatureVector4P* nodes = (FeatureVector4P*)pThis->Vector;
	nodes[0].nLevels = 2;
	for (int i = 1; i < 5; i++) nodes[i].nLevels = 1;
	BindFeatureVector(pThis);
}

DLL_EXP bool CopyFeatureVector(FeatureVector* pDest, const FeatureVector* pSource)
{
	if (!pDest || !pSource) return false;
	CopyFeatureData(pDest, pSource);
	return true;
}

DLL_EXP bool ExtractFeatureFromImage(FeatureVector* pFV, const aBYTE* pImageBits,
	int nLineW, int nH, aRect rcSample)
{
	if (!pFV || !pImageBits || nLineW <= 0 || nH <= 0 ||
		rcSample.width < 4 || rcSample.height < 4 || !ValidRect(rcSample, nLineW, nH))
		return false;
	InitFeatureVector(pFV);
	FeatureVector4P* root = Root(pFV);
	ExtractQuadrants(pImageBits, nLineW, rcSample, root);
	for (int i = 0; i < 4; i++)
	{
		aRect child = rcSample;
		child.left += (i & 1) * rcSample.width / 2;
		child.top += (i >> 1) * rcSample.height / 2;
		child.width = rcSample.width / 2;
		child.height = rcSample.height / 2;
		ExtractQuadrants(pImageBits, nLineW, child, root + i + 1);
	}
	return true;
}

DLL_EXP bool UpdateVectorsFrom(FeatureVector* pFV, FeatureVector* aFV, int nOrgWeight)
{
	if (!pFV || !aFV) return false;
	int weight = nOrgWeight < 0 ? 0 : (nOrgWeight > 100 ? 100 : nOrgWeight);
	FeatureVector result;
	InitFeatureVector(&result);
	FeatureVector4P* dst = Root(&result);
	const FeatureVector4P* first = Root(pFV);
	const FeatureVector4P* second = Root(aFV);
	for (int i = 0; i < 5; i++)
	{
		dst[i].nLevels = first[i].nLevels;
		for (int j = 0; j < 4; j++)
		{
			dst[i].Vector[j].x = (first[i].Vector[j].x * (100 - weight) + second[i].Vector[j].x * weight) / 100;
			dst[i].Vector[j].y = (first[i].Vector[j].y * (100 - weight) + second[i].Vector[j].y * weight) / 100;
		}
	}
	CopyFeatureData(pFV, &result);
	return true;
}

DLL_EXP aPOINT CompareFromImage(FeatureVector* pFV, aBYTE* pImageBits, int nLineW, int nH,
	aRect rcSampleRC, aRect rcRange, int* nMinDist, FeatureVector* theMinFV)
{
	aPOINT result = {0, 0};
	if (nMinDist) *nMinDist = INT_MAX;
	if (!pFV || !pImageBits || !nMinDist || rcSampleRC.width < 4 || rcSampleRC.height < 4)
		return result;
	int minLeft = rcRange.left < 0 ? 0 : rcRange.left;
	int minTop = rcRange.top < 0 ? 0 : rcRange.top;
	int maxLeft = rcRange.left + rcRange.width - rcSampleRC.width;
	int maxTop = rcRange.top + rcRange.height - rcSampleRC.height;
	int imageMaxLeft = nLineW - rcSampleRC.width;
	int imageMaxTop = nH - rcSampleRC.height;
	if (maxLeft > imageMaxLeft) maxLeft = imageMaxLeft;
	if (maxTop > imageMaxTop) maxTop = imageMaxTop;
	if (!ValidRect(rcSampleRC, nLineW, nH) || maxLeft < minLeft || maxTop < minTop)
		return result;
	for (int y = minTop; y <= maxTop; y++)
		for (int x = minLeft; x <= maxLeft; x++)
		{
			aRect candidate = {x, y, rcSampleRC.width, rcSampleRC.height};
			FeatureVector current;
			if (!ExtractFeatureFromImage(&current, pImageBits, nLineW, nH, candidate)) continue;
			int distance = FV_Distance(pFV, &current, 0, 0);
			if (distance < *nMinDist)
			{
				*nMinDist = distance;
				result.x = x;
				result.y = y;
				if (theMinFV) CopyFeatureData(theMinFV, &current);
			}
		}
	return result;
}

DLL_EXP int FV_Distance(FeatureVector* pFV, FeatureVector* aFV, int nFaceClrWeight, int nLevelWeight)
{
	if (!pFV || !aFV) return INT_MAX;
	const FeatureVector4P* first = Root(pFV);
	const FeatureVector4P* second = Root(aFV);
	long long distance = 0;
	for (int i = 0; i < 4; i++)
	{
		distance += abs(first[0].Vector[i].x - second[0].Vector[i].x);
		distance += abs(first[0].Vector[i].y - second[0].Vector[i].y);
	}
	return distance > INT_MAX ? INT_MAX : (int)distance;
}
