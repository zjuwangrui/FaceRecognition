// ImageProc.cpp : Defines the initialization routines for the DLL.
//

#include "stdafx.h"
#include "ImageProc.h"
#include "bufstruct.h"
#define DLL_EXP extern "C" _declspec(dllexport)
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
// CImageProcApp

BEGIN_MESSAGE_MAP(CImageProcApp, CWinApp)
	//{{AFX_MSG_MAP(CImageProcApp)
		// NOTE - the ClassWizard will add and remove mapping macros here.
		//    DO NOT EDIT what you see in these blocks of generated code!
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CImageProcApp construction

CImageProcApp::CImageProcApp()
{
	// TODO: add construction code here,
	// Place all significant initialization in InitInstance
}

/////////////////////////////////////////////////////////////////////////////
// The one and only CImageProcApp object

CImageProcApp theApp;

// Lookup cell struct for bilinear interpolation resampling
struct LookupCell{
    UINT x;
    UINT i;
    UINT k;
    UINT ksx;
    UINT xsi;
};

// Global pointer to shared buffer struct (set by myHeapAllocInit)
BUF_STRUCT * pBS = NULL;

void (*AddMessageToList)(char * message)=NULL;

DLL_EXP void InitMessageOpFunction(void (*AMTL)(char *))
{
	AddMessageToList = AMTL;
}

DLL_EXP void ShowDebugMessage(char* format,...)
{
	va_list ap;
	char sBuffer[1024];
	va_start(ap, format);
	vsprintf(sBuffer,format,ap);
	va_end(ap);
	if( AddMessageToList )
		(*AddMessageToList)(sBuffer);
}

/*******************************************************************/
// Initialize global heap pointer
/*******************************************************************/
DLL_EXP void myHeapAllocInit(BUF_STRUCT* pBufStruct)
{
	ASSERT(pBufStruct);
	pBS = pBufStruct;
	pBS->cur_allocSize = 0;
	pBS->allocTimes = 0;
	pBS->cur_maxallocsize = 0;
}

/*******************************************************************/
// Allocate a block of 'size' bytes from the dynamic heap in pBuffer
/*******************************************************************/
DLL_EXP aBYTE* myHeapAlloc(int size)
{
	ASSERT(pBS->cur_allocSize+size<pBS->max_allocSize);
	ASSERT(size>0);
	if( (size%4)!=0 )	size = (size+3)/4*4;
	*((int*)(pBS->pOtherData+pBS->cur_allocSize)) = size;
	pBS->cur_allocSize+=size+sizeof(int);
	pBS->allocTimes++;
	if( pBS->cur_allocSize>pBS->cur_maxallocsize )
		{
		pBS->cur_maxallocsize = pBS->cur_allocSize;
		ShowDebugMessage("heap peak: %d bytes",pBS->cur_maxallocsize);
		}
	return pBS->pOtherData+pBS->cur_allocSize-size;
}

/*******************************************************************/
// Free the most recently allocated block from the dynamic heap
/*******************************************************************/
DLL_EXP void myHeapFree(aBYTE* ptr)
{
	ASSERT(pBS->allocTimes>0);
	ASSERT(((pBS->pOtherData+pBS->cur_allocSize)-ptr)==*((int*)(ptr-sizeof(int))) );
	pBS->cur_allocSize -= *((int*)(ptr-sizeof(int)))+sizeof(int);
	pBS->allocTimes--;
}

/*******************************************************************/
// Draw a single pixel; bGray=true writes Y channel only
/*******************************************************************/
DLL_EXP void PutPixel(
	aBYTE* ThisImage,int W,int H, // image pointer and size
	int x,int y,                  // pixel coordinates
	COLORREF Color,               // color in packed RGB(Y,U,V) format
	bool bGray)                   // true=grayscale, false=YUV color
{
    ASSERT(ThisImage);
    int R,G,B;
    aBYTE  * lpBits = ThisImage;
    if(x<0 || x>=W) return;
    if(y<0 || y>=H) return;

    R = GetRValue(Color); // low byte  -> Y (or R)
    G = GetGValue(Color); // mid byte  -> U (or G)
    B = GetBValue(Color); // high byte -> V (or B)

    if( bGray )
		lpBits[W*y+x] = (aBYTE)G;
	else
		{
        lpBits[W*y+x] = R;              //Y
        lpBits[W*H+W*y/2+x/2] = G;      //U
        lpBits[W*H+W*H/2+W*y/2+x/2] = B;//V
		}
}

/*******************************************************************/
// Draw a horizontal line from (nvSX,nvY) to (nvEX,nvY)
/*******************************************************************/
DLL_EXP void DrawHLine(
		aBYTE* ThisImage,int W,int H, // image pointer and size
		int nvSX,int nvEX,            // start and end x
		int nvY,                      // y position
		COLORREF Color,               // line color
		bool bGray                    // true=grayscale, false=YUV color
		)
{
    int i;
	ASSERT(ThisImage);
    if( nvEX > nvSX )
        for(i=nvSX;i<=nvEX;i++)
             PutPixel(ThisImage,W,H, i,nvY,Color,bGray);
    else
        for(i=nvEX;i<=nvSX;i++)
             PutPixel(ThisImage,W,H, i,nvY,Color,bGray);
}

/*******************************************************************/
// Draw a vertical line from (nvX,nvSY) to (nvX,nvEY)
/*******************************************************************/
DLL_EXP void DrawVLine(
			aBYTE* ThisImage,int W,int H, // image pointer and size
			int nvX,                      // x position
			int nvSY,int nvEY,            // start and end y
			COLORREF Color,
			bool bGray)                   // true=grayscale, false=YUV color
{
    int i;
	ASSERT(ThisImage);
    if( nvEY > nvSY )
         for(i=nvSY;i<=nvEY;i++)
          PutPixel(ThisImage,W,H, nvX,i,Color,bGray);
    else
         for(i=nvEY;i<=nvSY;i++)
         PutPixel(ThisImage,W,H, nvX,i,Color,bGray);
}

/*******************************************************************/
// Draw the four edges of rectangle rc
/*******************************************************************/
DLL_EXP void DrawRectangle(
	aBYTE* ThisImage,int W,int H, // image pointer and size
	aRect rc,                     // rectangle (left,top,width,height)
	COLORREF Color,               // edge color
	bool bGray                    // true=grayscale, false=YUV color
	)
{
	ASSERT(ThisImage);

	DrawHLine(ThisImage,W,H,rc.left,RECT_RIGHT(rc),rc.top,Color,bGray);         // top edge
	DrawVLine(ThisImage,W,H,rc.left,rc.top,RECT_BOTTOM(rc),Color,bGray);        // left edge
	DrawHLine(ThisImage,W,H,rc.left,RECT_RIGHT(rc),RECT_BOTTOM(rc),Color,bGray);// bottom edge
	DrawVLine(ThisImage,W,H,RECT_RIGHT(rc),rc.top,RECT_BOTTOM(rc),Color,bGray); // right edge
}

/*******************************************************************/
// Draw a cross (+) centered at (nvX,nvY) with arm length nSize
/*******************************************************************/
DLL_EXP void DrawCross(
	aBYTE* ThisImage,int W,int H, // image pointer and size
	int nvX,int nvY,              // center coordinates
	int nSize,                    // arm length in pixels
	COLORREF Color,               // cross color
	bool bGray)
{
	ASSERT(ThisImage);
	DrawHLine(ThisImage,W,H,nvX-nSize,nvX+nSize,nvY,Color,bGray); // horizontal arm
	DrawVLine(ThisImage,W,H,nvX,nvY-nSize,nvY+nSize,Color,bGray); // vertical arm
}

/*******************************************************************/
// Copy ThisImage (W x H) into a rectangular region of anImage
/*******************************************************************/
DLL_EXP void CopyToRect(
	         aBYTE* ThisImage, // source image pointer
			 aBYTE* anImage,  // destination image pointer
			 int W,int H,     // source image size
		     int DestW,int DestH, // destination image size
			 int nvLeft,int nvTop,// top-left corner in destination
			 bool bGray       // true=grayscale, false=YUV color
					  )
{
    aBYTE  * lpSrc = ThisImage;
    aBYTE  * lpDes = anImage;
    aBYTE  * lps, * lpd;
    int h;
	ASSERT(ThisImage && anImage);
    ASSERT( nvLeft+W<=DestW && nvTop+H<=DestH && nvLeft>=0 && nvTop>=0 );
	//Y
    for(h=0;h<H;h++){
        lpd = lpDes+(nvTop+h)*DestW+nvLeft;
        lps = lpSrc+(DWORD)h*W;
		memcpy(lpd,lps,W);
    }
	if( bGray )	return;
	//U
	lpSrc += W*H;
	lpDes += DestW*DestH;
    for(h=0;h<H;h++){
        lpd = lpDes+(nvTop+h)*(DestW/2)+nvLeft/2;
        lps = lpSrc+(DWORD)h*W/2;
		memcpy(lpd,lps,W/2);
    }
	//V
	lpSrc += W*H/2;
	lpDes += DestW*DestH/2;
    for(h=0;h<H;h++){
        lpd = lpDes+(nvTop+h)*(DestW/2)+nvLeft/2;
        lps = lpSrc+(DWORD)h*W/2;
		memcpy(lpd,lps,W/2);
    }
}

/*******************************************************************/
// Grayscale bilinear interpolation resampling (internal helper)
/*******************************************************************/
bool GrayLinearIns(
				   aBYTE* ThisImage,int Width,int Height, // source image pointer and size
				   int ResWidth,int ResHeight,            // target image size
				   aBYTE* result)                         // target image pointer
{
    UINT i,j,u,v,k,l,gray=0;
    UINT x=0,y=0,tempx=0,tempy=0;
    UINT ksx,xsi,lsy,ysj;
    aBYTE* pOringinImage;
    aBYTE* pScaleImage;
    struct LookupCell *RowLookup,*ColLookup;
	ASSERT(result);
    pOringinImage=ThisImage;
    pScaleImage=result;
    RowLookup = (LookupCell*)myHeapAlloc(ResHeight*sizeof(LookupCell));
    ColLookup = (LookupCell*)myHeapAlloc(ResWidth*sizeof(LookupCell));
    for(v=0;v<ResHeight;v++)
	{   RowLookup[v].x = v*100*(Height-1)/(ResHeight-1);
        RowLookup[v].i = RowLookup[v].x/100; // upper neighbor row
        if( RowLookup[v].i>=Height-1 )	RowLookup[v].i = Height-2;
        RowLookup[v].k = RowLookup[v].i+1;   // lower neighbor row
        // weight for lower neighbor (interpolation unit = 100)
		RowLookup[v].ksx = RowLookup[v].k*100-RowLookup[v].x;
        // weight for upper neighbor
        RowLookup[v].xsi = RowLookup[v].x-RowLookup[v].i*100;
    }
    for(u=0;u<ResWidth;u++)
	{   ColLookup[u].x = u*100*(Width-1)/(ResWidth-1);
        ColLookup[u].i = ColLookup[u].x/100; // left neighbor col
        if( ColLookup[u].i>=Width-1 )	ColLookup[u].i = Width-2;
        ColLookup[u].k = ColLookup[u].i+1;   // right neighbor col
        ColLookup[u].ksx = ColLookup[u].k*100-ColLookup[u].x; // weight for right neighbor
        ColLookup[u].xsi = ColLookup[u].x-ColLookup[u].i*100; // weight for left neighbor
    }
    // bilinear interpolation
    for(v=0;v<ResHeight;v++){
        for(u=0;u<ResWidth;u++){
            i = ColLookup[u].i;
            j = RowLookup[v].i;
            k = ColLookup[u].k;
            l = RowLookup[v].k;
            ksx = ColLookup[u].ksx;
            lsy = RowLookup[v].ksx;
            xsi = ColLookup[u].xsi;
            ysj = RowLookup[v].xsi;

            gray =  pOringinImage[l*Width+i]*ksx*ysj
                    +pOringinImage[l*Width+k]*xsi*ysj
                    +pOringinImage[j*Width+k]*xsi*lsy
                    +pOringinImage[j*Width+i]*ksx*lsy;
            pScaleImage[v*ResWidth+u] = (aBYTE)(gray/10000);
        }
    }
	myHeapFree((aBYTE*)ColLookup);
	myHeapFree((aBYTE*)RowLookup);
    return true;
}

/*******************************************************************/
// Color (YUV422) bilinear interpolation resampling (internal helper)
/*******************************************************************/
bool ColorLinearIns(
					aBYTE* ThisImage,int Width,int Height, // source image pointer and size
					int ResWidth,int ResHeight,            // target image size
					aBYTE* result                          // target image pointer
					)
{
	ASSERT(result);
	GrayLinearIns(ThisImage,Width,Height,ResWidth,ResHeight,result);
	GrayLinearIns(ThisImage+Width*Height,Width/2,Height,ResWidth/2,ResHeight,result+ResWidth*ResHeight);
	GrayLinearIns(ThisImage+Width*Height+Width*Height/2,Width/2,Height,
		          ResWidth/2,ResHeight,result+ResWidth*ResHeight+ResWidth*ResHeight/2);
	return true;
}

/*******************************************************************/
// Union-find: find root with path-halving (helper for RegionMark)
/*******************************************************************/
static aBYTE uf_find(aBYTE* parent, aBYTE x)
{
    while (parent[x] != x) {
        parent[x] = parent[parent[x]];
        x = parent[x];
    }
    return x;
}

/*******************************************************************/
// 2D minimum filter (binary erosion): each pixel becomes the minimum
// value in its SW x SW neighborhood. In-place; uses temp heap buffer.
/*******************************************************************/
DLL_EXP void Minimum_2D(aBYTE* lpImage, int Width, int Height, int SW)
{
    if (SW <= 1) return;
    int HW = SW / 2;
    aBYTE* temp = myHeapAlloc(Width * Height);
    for (int y = 0; y < Height; y++) {
        for (int x = 0; x < Width; x++) {
            aBYTE minVal = 255;
            int y0 = y - HW; if (y0 < 0) y0 = 0;
            int y1 = y + HW; if (y1 >= Height) y1 = Height - 1;
            int x0 = x - HW; if (x0 < 0) x0 = 0;
            int x1 = x + HW; if (x1 >= Width)  x1 = Width  - 1;
            for (int ny = y0; ny <= y1; ny++)
                for (int nx = x0; nx <= x1; nx++) {
                    aBYTE v = lpImage[ny * Width + nx];
                    if (v < minVal) minVal = v;
                }
            temp[y * Width + x] = minVal;
        }
    }
    memcpy(lpImage, temp, Width * Height);
    myHeapFree(temp);
}

/*******************************************************************/
// 2D maximum filter (binary dilation): each pixel becomes the maximum
// value in its SW x SW neighborhood. In-place; uses temp heap buffer.
/*******************************************************************/
DLL_EXP void Maximum_2D(aBYTE* lpImage, int Width, int Height, int SW)
{
    if (SW <= 1) return;
    int HW = SW / 2;
    aBYTE* temp = myHeapAlloc(Width * Height);
    for (int y = 0; y < Height; y++) {
        for (int x = 0; x < Width; x++) {
            aBYTE maxVal = 0;
            int y0 = y - HW; if (y0 < 0) y0 = 0;
            int y1 = y + HW; if (y1 >= Height) y1 = Height - 1;
            int x0 = x - HW; if (x0 < 0) x0 = 0;
            int x1 = x + HW; if (x1 >= Width)  x1 = Width  - 1;
            for (int ny = y0; ny <= y1; ny++)
                for (int nx = x0; nx <= x1; nx++) {
                    aBYTE v = lpImage[ny * Width + nx];
                    if (v > maxVal) maxVal = v;
                }
            temp[y * Width + x] = maxVal;
        }
    }
    memcpy(lpImage, temp, Width * Height);
    myHeapFree(temp);
}

/*******************************************************************/
// Connected component labeling (4-connectivity, two-pass union-find).
// Non-zero pixels are foreground; on return each holds its label (1..N).
// Returns N = number of distinct regions (max 254).
/*******************************************************************/
DLL_EXP int RegionMark(aBYTE* BinImage, int Width, int Height)
{
    aBYTE parent[256];
    for (int i = 0; i < 256; i++) parent[i] = (aBYTE)i;

    aBYTE* labels = myHeapAlloc(Width * Height);
    memset(labels, 0, Width * Height);
    aBYTE nextLabel = 1;

    // First pass: assign provisional labels, merge equivalences
    for (int y = 0; y < Height; y++) {
        for (int x = 0; x < Width; x++) {
            if (BinImage[y * Width + x] == 0) continue;
            aBYTE top  = (y > 0) ? labels[(y-1)*Width + x] : 0;
            aBYTE left = (x > 0) ? labels[y*Width + x - 1] : 0;
            if (!top && !left) {
                if (nextLabel < 255) labels[y*Width+x] = nextLabel++;
                else labels[y*Width+x] = 254;
            } else if (!top) {
                labels[y*Width+x] = left;
            } else if (!left) {
                labels[y*Width+x] = top;
            } else {
                aBYTE ra = uf_find(parent, top);
                aBYTE rb = uf_find(parent, left);
                if (ra != rb) { if (ra < rb) parent[rb] = ra; else parent[ra] = rb; }
                labels[y*Width+x] = uf_find(parent, ra < rb ? ra : rb);
            }
        }
    }

    // Second pass: compact relabeling 1..N, write final labels into BinImage
    aBYTE remap[256];
    memset(remap, 0, sizeof(remap));
    aBYTE nRegions = 0;
    for (int k = 0; k < Width * Height; k++) {
        if (labels[k] == 0) { BinImage[k] = 0; continue; }
        aBYTE root = uf_find(parent, labels[k]);
        if (remap[root] == 0) remap[root] = ++nRegions;
        BinImage[k] = remap[root];
    }

    myHeapFree(labels);
    return nRegions;
}

/*******************************************************************/
// Find the axis-aligned bounding rect of all pixels equal to 'gray'.
// Writes rect to *p_rcnRect and pixel count to *p_graySum.
// Returns false if no matching pixel is found.
/*******************************************************************/
DLL_EXP bool GetEspGrayRect(aBYTE* ThisImage, int W, int H, aBYTE gray, aRect* p_rcnRect, int* p_graySum)
{
    int minX = W, maxX = -1, minY = H, maxY = -1, count = 0;
    for (int y = 0; y < H; y++) {
        for (int x = 0; x < W; x++) {
            if (ThisImage[y * W + x] != gray) continue;
            if (x < minX) minX = x;
            if (x > maxX) maxX = x;
            if (y < minY) minY = y;
            if (y > maxY) maxY = y;
            count++;
        }
    }
    if (maxX < 0) {
        if (p_rcnRect) memset(p_rcnRect, 0, sizeof(aRect));
        if (p_graySum) *p_graySum = 0;
        return false;
    }
    if (p_rcnRect) {
        p_rcnRect->left   = minX;
        p_rcnRect->top    = minY;
        p_rcnRect->width  = maxX - minX + 1;
        p_rcnRect->height = maxY - minY + 1;
    }
    if (p_graySum) *p_graySum = count;
    return true;
}

/*******************************************************************/
// Resample ThisImage (Width x Height) to (newWidth x newHeight).
// InsMode=true: bilinear interpolation; false: nearest-neighbor.
// bGray=true: Y channel only; false: full YUV422 color.
/*******************************************************************/
DLL_EXP bool ReSample(aBYTE* ThisImage,int Width,int Height,
					  int newWidth,int newHeight,
					  bool InsMode,
					  bool bGray,
					  aBYTE *result
					  )
{
    int *HSampleTable;
    int *VSampleTable;
    int i,j;
    float rsWRatio = float(Width)/float(newWidth);
    float rsHRatio = float(Height)/float(newHeight);
    aBYTE *lpDes;
    aBYTE *lpSrc;
    aBYTE *lpLineSrc,*lpLineDes;
    if( (Width==newWidth) && (Height==newHeight) )
        return true;
	ASSERT(result);
    if( !InsMode )
	{   // nearest-neighbor downsampling
        HSampleTable = (int*)myHeapAlloc(newWidth*sizeof(int));
       	VSampleTable = (int*)myHeapAlloc(newHeight*sizeof(int));
        // build horizontal sample index table
		for(i=0;i<newWidth;i++)
		{ HSampleTable[i] = int(float(i)*rsWRatio);
          if( HSampleTable[i]>Width-1 ) HSampleTable[i] = Width-1;
		}
        // build vertical sample index table
        for(i=0;i<newHeight;i++)
		{   VSampleTable[i] = int(float(i)*rsHRatio);
            if( VSampleTable[i]>Height-1 ) VSampleTable[i] = Height-1;
        }
		//Y channel
        lpDes = result;
        lpSrc = ThisImage;
        for(j=0;j<newHeight;j++)
		{   lpLineSrc = lpSrc+VSampleTable[j]*Width;
            lpLineDes = lpDes+j*newWidth;
            for(i=0;i<newWidth;i++) lpLineDes[i] = lpLineSrc[HSampleTable[i]];
        }
		if( bGray )
		{ myHeapFree((aBYTE*)VSampleTable);
	      myHeapFree((aBYTE*)HSampleTable);
		  return true;
		}
		//U channel
        lpDes += newWidth*newHeight;
        lpSrc += Width*Height;
        for(j=0;j<newHeight;j++)
		{   lpLineSrc = lpSrc+VSampleTable[j]*Width/2;
            lpLineDes = lpDes+j*newWidth/2;
            for(i=0;i<newWidth/2;i++) lpLineDes[i] = lpLineSrc[HSampleTable[i*2]/2];
        }
		//V channel
        lpDes += newWidth*newHeight/2;
        lpSrc += Width*Height/2;
        for(j=0;j<newHeight;j++)
		{   lpLineSrc = lpSrc+VSampleTable[j]*Width/2;
            lpLineDes = lpDes+j*newWidth/2;
            for(i=0;i<newWidth/2;i++) lpLineDes[i] = lpLineSrc[HSampleTable[i*2]/2];
        }
        myHeapFree((aBYTE*)VSampleTable);
        myHeapFree((aBYTE*)HSampleTable);
    }
    else
	{   // bilinear interpolation
        if( bGray )	GrayLinearIns(ThisImage,Width,Height,newWidth,newHeight,result);
		else		ColorLinearIns(ThisImage,Width,Height,newWidth,newHeight,result);
    }
    return true;
}
