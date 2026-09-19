// ImagePrepare.cpp : Defines the initialization routines for the DLL.

#include "stdafx.h"
#include "ImagePrepare.h"
//
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
// CImagePrepareApp
BEGIN_MESSAGE_MAP(CImagePrepareApp, CWinApp)
	//{{AFX_MSG_MAP(CImagePrepareApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
//
/////////////////////////////////////////////////////////////////////////////
// CImagePrepareApp construction
CImagePrepareApp::CImagePrepareApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}
/////////////////////////////////////////////////////////////////////////////
// The one and only CImagePrepareApp object
CImagePrepareApp theApp;
//extern "C"__declspec(dllimport)表示，兼容C方式导入函数
DLL_INP void InitFeatureBuffer(BUF_STRUCT* pBS);
DLL_INP void InitFeatureVector(FeatureVector* pThis);

char sInfo[] = "人脸跟踪-摄像头视频流图片截取处理插件";
bool bLastPlugin = false;
//extern "C" _declspec(dllexport)表示，兼容C方式导出函数
//bLastPlugin为真时，表示为最后一个有效插件
DLL_EXP void ON_PLUGIN_BELAST(bool bLast)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	bLastPlugin = bLast;
}
//插件名称
DLL_EXP LPCTSTR ON_PLUGININFO(void)
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	return sInfo;
}
//
DLL_EXP void ON_INITPLUGIN(LPVOID lpParameter)
{   
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	//theApp.dlg.Create(IDD_PLUGIN_SETUP);
	//theApp.dlg.ShowWindow(SW_HIDE);
}
DLL_EXP int ON_PLUGINCTRL(int nMode,void* pParameter)
{
//模块状态切换
	AFX_MANAGE_STATE(AfxGetStaticModuleState());
	int nRet = 0;
	return nRet;
}
//初始化跟踪体
void InitTraceObject(TRACE_OBJECT* pThis,char name[8])
{
        memset(&pThis->rcObject,0,sizeof(aRect));//跟踪体区域
        pThis->spdxObj = pThis->spdyObj = 0;//跟踪体运动方向、速度
        pThis->nMinDist = 0x7fffffff;//最小特征差距
        pThis->bBrokenTrace = false;//跟踪失败标记
        pThis->nBrokenTimes = 0;//疑似丢失跟踪次数
        pThis->bSaveit= false;//丢失跟踪标记
		//初始化跟踪体特征向量
        InitFeatureVector(&pThis->fvObject);//当前特征
        InitFeatureVector(&pThis->fvObject_org);//原始特征
		strncpy(pThis->sName,name,8);
		pThis->sName[7] = 0;
}

DLL_EXP void ON_PLUGINRUN(int w,int h,BYTE* pYBits,BYTE* pUBits,BYTE* pVBits,BYTE* pBuffer)
{
//pYBits 大小为w*h
//pUBits 和 pVBits 的大小为 w*h/2
//pBuffer 的大小为 w*h*6
//下面算法都基于一个假设，即w是16的倍数
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	//ShowDebugMessage("与printf函数用法相似,X=%d,Y=%d\n",10,5);

     //请在下面编写相应处理程序






	//下面的步骤用于测试图像产生的结果是否正确。
	/*
		if( bLastPlugin )
		{
		
			//测试grayBmp_1d16:将grayBmp_1d16复制到显示图片的左上角
			CopyToRect(pBufStruct->grayBmp_1d16, pYBits, w/4, h/4, w, h, 0, 0, true);
			//测试colorBmp:将clrBmp_1d8复制到显示图片的右上角
			CopyToRect(pBufStruct->clrBmp_1d8,  pYBits, w/2, h/4, w, h, w/2, 0, false);
		}
    */
}
DLL_EXP void ON_PLUGINEXIT()
{
	AFX_MANAGE_STATE(AfxGetStaticModuleState());//模块状态切换
	//theApp.dlg.DestroyWindow();
}
