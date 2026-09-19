#ifndef __ANTISLEEP_IMAGEPROC_H__
#define __ANTISLEEP_IMAGEPROC_H__
#include "BufStruct.h"

#ifndef RGB 
#define RGB(r,g,b) ( (DWORD) ( ( (aBYTE)(r) | ((WORD)((aBYTE)(g))<<8) ) | (((DWORD)(aBYTE)(b))<<16)  ) )
#endif
#define TRGB(y,u,v) 	RGB(	OFBYTE(int(y+1.14*(v-128)+0.5)),\
								OFBYTE(int(y-0.394*(u-128)-0.581*(v-128)+0.5)),\
								OFBYTE(int(y+2.032*(u-128)+0.5)) )
#define TYUV(r,g,b) 	RGB(	OFBYTE(int(0.299*(r)+0.587*(g)+0.114*(b)+0.5)),\
								OFBYTE(int(-0.147*(r)-0.289*(g)+0.436*(b)+0.5+128)),\
								OFBYTE(int(0.615*(r)-0.515*(g)-0.100*(b)+0.5+128)))
#define TRGB1(y,u,v) 	RGB(	OFBYTE(int(y+1.402*(v-128)+0.5)),\
								OFBYTE(int(y-0.34414*(u-128)-0.71414*(v-128)+0.5)),\
								OFBYTE(int(y+1.772*(u-128)+0.5)) )
#define TYUV1(r,g,b) 	RGB(	OFBYTE(int(0.299*(r)+0.587*(g)+0.114*(b)+0.5)),\
								OFBYTE(int(-0.1687*(r)-0.3313*(g)+0.5*(b)+0.5+128)),\
								OFBYTE(int(0.5*(r)-0.4187*(g)-0.0813*(b)+0.5+128)))
#define TRGB2(y,u,v) 	RGB(	OFBYTE(int(1.164*(y-16)+1.596*(v-128)+0.5)),\
								OFBYTE(int(1.164*(y-16)-0.392*(u-128)-0.813*(v-128)+0.5)),\
								OFBYTE(int(1.164*(y-16)+2.017*(u-128)+0.5)) )
#define TYUV2(r,g,b) 	RGB(	OFBYTE(int(0.257*(r)+0.504*(g)+0.098*(b)+0.5+16)),\
								OFBYTE(int(-0.148*(r)-0.291*(g)+0.439*(b)+0.5+128)),\
								OFBYTE(int(0.439*(r)-0.368*(g)-0.071*(b)+0.5+128)))


extern "C" {
	DLL_IMP void InitMessageOpFunction(void(* AMTL)(char *));
	DLL_IMP void myHeapAllocInit(BUF_STRUCT* pBufStruct);
	DLL_IMP aBYTE* myHeapAlloc(int size);
	DLL_IMP void myHeapFree(aBYTE * ptr);
	DLL_IMP void ColorNormalize(aBYTE* pYUY2Bits,int w,int h);
	DLL_IMP void HistoTrans(aBYTE* pGrayImage,int Width,int Height,aBYTE *Param);
	DLL_IMP void MultiImages(aBYTE* ThisImage,aBYTE* MultiImage,int W,int H,int nPostDiv,aBYTE* result);
	DLL_IMP bool GaoSi_Blur(aBYTE* ThisImage,int w,int h,int Center);
	DLL_IMP void PixelValueMultify(aBYTE *ThisImage,int W,int H,int nMultiVal,aBYTE* result);
	DLL_IMP void Minimum_2D(aBYTE  *lpImage,int Width,int Height,int SW);
	DLL_IMP void Maximum_2D(aBYTE  *lpImage,int Width,int Height,int SW);
	DLL_IMP aBYTE Binarize_1(aBYTE  * lpImage,int Width,int Height,aBYTE Thres);
	DLL_IMP aBYTE Binarize_2(aBYTE  * lpImage,int Width,int Height,int Ratio/*,int nRatioDiv*/);
	DLL_IMP void Maximum_Hori_Bin(aBYTE  *lpImage,int Width,int Height,int SW);
	DLL_IMP void Minimum_Hori_Bin(aBYTE  *lpImage,int Width,int Height,int SW);
	DLL_IMP void Maximum_Vert_Bin(aBYTE  *lpImage,int Width,int Height,int SW);
	DLL_IMP void Minimum_Vert_Bin(BYTE  *lpImage,int Width,int Height,int SW);
	DLL_IMP int RegionMark(aBYTE  * BinImage,int Width,int Height);
	DLL_IMP void DeletePiece(aBYTE* ThisImage,int Width,int Height);
	DLL_IMP bool GetEspGrayRect(aBYTE* ThisImage,int W,int H, aBYTE gray, aRect *p_rcnRect,int* p_graySum);
	DLL_IMP void CopyRectTo(aBYTE* ThisImage,aBYTE* anDIBitmap,int Width,int Height,/*int DestW,int DestH,*/aRect rcvRect, bool bGray );
	DLL_IMP void CopyToRect(aBYTE* ThisImage,aBYTE* anDIBitmap,int W,int H,int DestW,int DestH,int nvLeft,int nvTop,bool bGray);
	DLL_IMP bool ReSample(aBYTE* ThisImage,int Width,int Height,int newWidth,int newHeight,bool InsMode,bool bGray,aBYTE *result);
	DLL_IMP void DrawHLine(aBYTE* ThisImage,int W,int H,int nvSX,int nvEX,int nvY,COLORREF Color,bool bGray);
	DLL_IMP void DrawVLine(aBYTE* ThisImage,int W,int H,int nvX,int nvSY,int nvEY,COLORREF Color,bool bGray);
	DLL_IMP void DrawRectangle(aBYTE* ThisImage,int W,int H,aRect rc,COLORREF Color,bool bGray);
	DLL_IMP void DrawCross(aBYTE* ThisImage,int W,int H,int nvX,int nvY,int nSize,COLORREF Color,bool bGray);
	DLL_IMP void ShowDebugMessage(char* format,...);
}

#endif
