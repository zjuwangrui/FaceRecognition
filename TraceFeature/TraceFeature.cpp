// TraceFeature.cpp : Defines the initialization routines for the DLL.
//
#include "stdafx.h"
#include "TraceFeature.h"
#include "bufstruct.h"
#include "ImageProc.h"

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
DLL_EXP void InitFeatureVector( FeatureVector* pThis)
{
	int i;
	ASSERT(pThis);
    //����������FeatureVector4P��ʽ���
	FeatureVector4P* pVector = (FeatureVector4P*)pThis->Vector;
	pVector->pNL_LeftTop		= (FeatureVector4P*)((aBYTE*)pVector+sizeof(FeatureVector4P));
	pVector->pNL_RightTop		= (FeatureVector4P*)((aBYTE*)pVector+2*sizeof(FeatureVector4P));
	pVector->pNL_LeftBottom		= (FeatureVector4P*)((aBYTE*)pVector+3*sizeof(FeatureVector4P));
	pVector->pNL_RightBottom	= (FeatureVector4P*)((aBYTE*)pVector+4*sizeof(FeatureVector4P));
	pThis->size = sizeof(FeatureVector4P)*5;//
	pVector->nLevels = 2;
	pVector->pNL_LeftTop->nLevels = 1;
	pVector->pNL_RightTop->nLevels = 1;
	pVector->pNL_LeftBottom->nLevels = 1;
	pVector->pNL_RightBottom->nLevels = 1;
    for(i=0;i<4;i++)
    	{
        pVector->Vector[i].x = pVector->Vector[i].y = 0;//�Ľ�����
        //��������Ľ�����
        pVector->pNL_LeftTop->Vector[i].x = pVector->pNL_LeftTop->Vector[i].y = 0;
        pVector->pNL_RightTop->Vector[i].x = pVector->pNL_RightTop->Vector[i].y = 0;
        pVector->pNL_LeftBottom->Vector[i].x = pVector->pNL_LeftBottom->Vector[i].y = 0;
        pVector->pNL_RightBottom->Vector[i].x = pVector->pNL_RightBottom->Vector[i].y = 0;
    	}
}

// Stub: update feature vector by blending with another (to be implemented in Plugin 3)
DLL_EXP bool UpdateVectorsFrom(FeatureVector* pFV, FeatureVector* aFV, int nOrgWeight)
{
	return false;
}

// Stub: search image patch matching pFV within rcRange; returns best-match position (to be implemented in Plugin 3)
DLL_EXP aPOINT CompareFromImage(FeatureVector* pFV, aBYTE* pImageBits, int nLineW, int nH,
                                aRect rcSampleRC, aRect rcRange, int* nMinDist, FeatureVector* theMinFV)
{
	aPOINT pt = {0, 0};
	if (nMinDist) *nMinDist = 0x7fffffff;
	return pt;
}

// Stub: compute distance between two feature vectors (to be implemented in Plugin 3)
DLL_EXP int FV_Distance(FeatureVector* pFV, FeatureVector* aFV, int nFaceClrWeight, int nLevelWeight)
{
	return 0x7fffffff;
}

