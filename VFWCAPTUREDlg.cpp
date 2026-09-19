// VFWCAPTUREDlg.cpp : implementation file
#include "stdafx.h"
#include "VFWCAPTURE.h"
#include "VFWCAPTUREDlg.h"
#include "vfw.h"
#include "BufStruct.h"
#include "ImageProc.h"
#pragma comment(lib,"vfw32.lib")
#pragma comment(lib,"ImageProc.lib")
#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif
#define YUV2 844715353
#define VUY2 353517448
#define YUY2 842094158
#define NV12 842094159
/////////////////////////////////////////////////////////////////////////////
// CAboutDlg dialog used for App About
class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	//{{AFX_DATA(CAboutDlg)
	enum { IDD = IDD_ABOUTBOX };
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CAboutDlg)
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	//{{AFX_MSG(CAboutDlg)
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
	//{{AFX_DATA_INIT(CAboutDlg)
	//}}AFX_DATA_INIT
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CAboutDlg)
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
	//{{AFX_MSG_MAP(CAboutDlg)
		// No message handlers
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()

/////////////////////////////////////////////////////////////////////////////
// CVFWCAPTUREDlg dialog
int CVFWCAPTUREDlg::nVideoW=0;
int CVFWCAPTUREDlg::nVideoH=0;
DWORD CVFWCAPTUREDlg::dwFormat = 0;
extern BYTE* pImgProcBuf;

CVFWCAPTUREDlg* theDlg=NULL;
void AddMessageToList(char* message)
{
	if( theDlg )
		theDlg->AddLogInfo(message);
}

CVFWCAPTUREDlg::CVFWCAPTUREDlg(CWnd* pParent /*=NULL*/)
	: CDialog(CVFWCAPTUREDlg::IDD, pParent)
{
	//{{AFX_DATA_INIT(CVFWCAPTUREDlg)
		// NOTE: the ClassWizard will add member initialization here
	//}}AFX_DATA_INIT
	// Note that LoadIcon does not require a subsequent DestroyIcon in Win32
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
    nPlugInNum = 0;
	m_nVideoWidth = 0;
	m_nVideoHeight = 0;
	m_sVideoFmt = _T("");
    pBuffer = NULL;
	m_bDSPFormatSim = FALSE;
	theDlg = this;
}

void CVFWCAPTUREDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
	//{{AFX_DATA_MAP(CVFWCAPTUREDlg)
    DDX_Control(pDX, IDC_LIST1, m_List);
	DDX_Control(pDX, IDC_LIST2, m_PluginList);
	DDX_Text(pDX, IDC_VIDEOW, m_nVideoWidth);
	DDX_Text(pDX, IDC_VIDEOH, m_nVideoHeight);
	DDX_Text(pDX, IDC_VIDEOFMTID, m_sVideoFmt);
    DDX_Check(pDX, IDC_DSPFORMAT, m_bDSPFormatSim);
    DDX_Control(pDX, IDC_PLUGIN_ENABLE, m_PluginEnable);
	//}}AFX_DATA_MAP
}

BEGIN_MESSAGE_MAP(CVFWCAPTUREDlg, CDialog)
	//{{AFX_MSG_MAP(CVFWCAPTUREDlg)	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_BN_CLICKED(IDC_START, OnStart)
	ON_WM_TIMER()
	ON_BN_CLICKED(IDC_SAVEIMG, OnSaveimg)
	ON_BN_CLICKED(IDC_VFSETUP, OnVfsetup)
	ON_BN_CLICKED(IDC_VFWSETUP, OnVfwsetup)
	ON_BN_CLICKED(IDC_DSPFORMAT, OnDspformat)
	ON_LBN_SELCHANGE(IDC_LIST2, OnSelchangeList2)
	ON_BN_CLICKED(IDC_PLUGIN_ENABLE, OnPluginEnable)
	ON_BN_CLICKED(IDC_ACTIVEALL, OnActiveall)
	//}}AFX_MSG_MAP
END_MESSAGE_MAP()
/////////////////////////////////////////////////////////////////////////////
// CVFWCAPTUREDlg message handlers

BOOL CVFWCAPTUREDlg::OnInitDialog()
{
	CDialog::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		CString strAboutMenu;
		strAboutMenu.LoadString(IDS_ABOUTBOX);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon
	
	// TODO: Add extra initialization here
    InitMessageOpFunction(&AddMessageToList);
	
	HANDLE hFind = NULL;
	WIN32_FIND_DATA w32fd;//struct,����ļ���9��������Ϣ
	char path[256];
	GetModuleFileName(NULL,path,256);//�����ظõ�ǰӦ�ó���ȫ·��
	char sDrv[256],sDir[256],sFname[256],sExt[256];
	_splitpath(path,sDrv,sDir,sFname,sExt);//�ֽ�·��
   	_makepath(path,sDrv,sDir,"","");//����·��
	strncpy(sDir,path,256);//�ַ�������
	//strncpy(path,sWorkDir,256);
	strncat(path,"PlugIn\\*.dll",256);//�ַ�������
	hFind = FindFirstFile(path,&w32fd);//����ָ���ļ�
	if( hFind!=INVALID_HANDLE_VALUE && hFind!=0 )
	{
		do 
		{
			if( nPlugInNum<32 )
			{
				AddLogInfo("One plugin found!-->%s<--",w32fd.cFileName);
				//
				strncpy(path,sDir,256);
				strncat(path,"Plugin\\",256);
				strncat(path,w32fd.cFileName,256);
				//��ȡ����ĺ���ָ��
				AllPlugIns[nPlugInNum].OpenPlugIn(path);
				if( AllPlugIns[nPlugInNum].bPlugInOk )
				{
					strncpy(sExt," ",256);
					strncat(sExt,w32fd.cFileName,256);
					m_PluginList.AddString(sExt);//
	     			AddLogInfo("Load PlugIn<%s> success!",path);
  					AllPlugIns[nPlugInNum].OnInitPlugIn(NULL);
                    nPlugInNum++;
				}
				else
				{
					AddLogInfo("Load PlugIn<%s> failed.",path);
				}
			}
		} while(FindNextFile(hFind,&w32fd));
		FindClose(hFind);
	}
	return TRUE;  // return TRUE  unless you set the focus to a control
}
void CVFWCAPTUREDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialog::OnSysCommand(nID, lParam);
	}
}
// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.
void CVFWCAPTUREDlg::OnPaint() 
{
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting
		SendMessage(WM_ICONERASEBKGND, (WPARAM) dc.GetSafeHdc(), 0);
		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;
		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialog::OnPaint();
	}
}

// The system calls this to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CVFWCAPTUREDlg::OnQueryDragIcon()
{
	return (HCURSOR) m_hIcon;
}
void CVFWCAPTUREDlg::OnStart() 
{
	// TODO: Add your control notification handler code here
	//	if( GetDlgItem(IDC_GRABFRAME)->IsWindowEnabled() )
//	{
//		capOverlay(m_hCapture,TRUE);
//		GetDlgItem(IDC_RECORD)->EnableWindow(TRUE);
//	}
//	else
//	{
		CAPTUREPARMS m_CaptureParams;
    //  ����"����" 
		m_hCapture = capCreateCaptureWindow("cap",WS_EX_CONTROLPARENT|WS_CHILD|WS_VISIBLE,10,6,640,480,m_hWnd,0);
	//  connect driver, retry up to 3 times to avoid infinite loop
		int nRetry = 0;
		while(!capDriverConnect(m_hCapture, 0) && nRetry < 3) nRetry++;
		if(nRetry >= 3) { AddLogInfo("Camera connect failed!"); return; }

    //  setup capture params
		capCaptureGetSetup(m_hCapture,&m_CaptureParams,sizeof(m_CaptureParams));
		m_CaptureParams.fYield = TRUE;
		capCaptureSetSetup(m_hCapture,&m_CaptureParams,sizeof(m_CaptureParams));

		// try to switch camera format to YUY2 640x480
		{
			BITMAPINFO bi;
			memset(&bi, 0, sizeof(bi));
			bi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
			bi.bmiHeader.biWidth       = 640;
			bi.bmiHeader.biHeight      = 480;
			bi.bmiHeader.biPlanes      = 1;
			bi.bmiHeader.biBitCount    = 16;
			bi.bmiHeader.biCompression = YUV2;
			bi.bmiHeader.biSizeImage   = 640 * 480 * 2;
			if(!capSetVideoFormat(m_hCapture, &bi, sizeof(bi)))
				AddLogInfo("Switch to YUY2 failed, please use VF Setup button.");
			else
				AddLogInfo("Switched to YUY2 640x480.");
		}

   //  use software preview instead of hardware overlay (not supported on modern OS)
		capPreview(m_hCapture, TRUE);
		capPreviewRate(m_hCapture, 33);
   //   ������Ӧ��ťΪ�״̬
   		GetDlgItem(IDC_SAVEIMG)->EnableWindow(true);
	   //	GetDlgItem(IDC_RECORD)->EnableWindow(TRUE);
   //   ��ֹ����ʼ����ť
	    GetDlgItem(IDC_START)->EnableWindow(false);
   //  ����һ��֡����ص�����
		if( capSetCallbackOnFrame(m_hCapture,(LPVOID)VideoCallbackProc) )
		 AddLogInfo("Success install capture callback!");

   // ��ȡ��Ƶ����
        DWORD dwSize = capGetVideoFormatSize(m_hCapture);//��Ƶ������С
		BYTE* pBuffer = new BYTE[dwSize];
		capGetVideoFormat(m_hCapture,pBuffer,dwSize);
	//
		LPBITMAPINFO pbi = (LPBITMAPINFO)pBuffer;
		nVideoW = pbi->bmiHeader.biWidth;
		nVideoH = pbi->bmiHeader.biHeight;
		dwFormat = pbi->bmiHeader.biCompression;
	//
		delete pBuffer;
		m_nVideoWidth = nVideoW;
		m_nVideoHeight = nVideoH;
		m_sVideoFmt.Format("0x%x",dwFormat);   
	//����33ms��ʱ��
	    SetTimer(1,33,NULL);
	//ˢ�´���
		UpdateData(false);
}
void  CVFWCAPTUREDlg::OnTimer(UINT nIDEvent) 
{
	// TODO: Add your message handler code here and/or call default
	//ץȡһ֡ͼ������,��capGrabFrame���,������ֹͣԤ��ģʽ���ص�
 	capGrabFrameNoStop(m_hCapture);
	CDialog::OnTimer(nIDEvent);
}


LRESULT CALLBACK CVFWCAPTUREDlg::VideoCallbackProc(HWND hWnd, LPVOID lpVHdr)
{
	 LPVIDEOHDR pVHdr = (LPVIDEOHDR)lpVHdr;
     //�����洢YUV2ƽ���ʽ���ƿռ�
     BYTE* pY = new BYTE[nVideoW*nVideoH*2];
	 BYTE* pU = pY+nVideoW*nVideoH;
	 BYTE* pV = pU+nVideoW*nVideoH/2;
				
	if( pVHdr )
	{
		if (dwFormat== YUV2  || dwFormat==RGB24)
        {
	     if( !pImgProcBuf ) 
						{
						pImgProcBuf = new BYTE[nVideoW*nVideoH*4];
						if( !pImgProcBuf )	return (LRESULT)FALSE;
						((BUF_STRUCT*)pImgProcBuf)->bNotInited = true;//���ó�ʼ�����
						}
			//ת����YUV2ƽ���ʽ
			if	 (dwFormat== YUV2)
			theDlg->SplitYUVData(nVideoW,nVideoH,pVHdr->lpData,pY,pU,pV);
            if	 (dwFormat== RGB24)
            theDlg->RGBtoYUV(nVideoW,nVideoH,pVHdr->lpData,pY,pU,pV);
		}
		else
        {
		theDlg->AddLogInfo("δ֪����Ƶ��ʽ---%d,frame_size=%d,video_size=%d",dwFormat,pVHdr->dwBufferLength,nVideoW*nVideoH);
					return FALSE;
		}
 //
			
					//�ҳ������Ч���
			        int theLast=-1;
					for(int i=0;i<theDlg->nPlugInNum;i++)
					{
						theDlg->AllPlugIns[i].OnPlugInBeLast(false);
						//strncmp �ַ���s1��s2��ǰsize���ַ���ͬ����������ֵΪ0
						if( theDlg->AllPlugIns[i].Enabled() && strncmp(theDlg->AllPlugIns[i].sName,"PZ_FinalProc.dll",16)!=0 )
							{
							theLast = i;
							}
					}
					if( theLast>=0 )	theDlg->AllPlugIns[theLast].OnPlugInBeLast(true);
					//���ε������в������
					for( int i=0;i<theDlg->nPlugInNum;i++)
					{
						if( theDlg->AllPlugIns[i].Enabled() )
							theDlg->AllPlugIns[i].OnPlugInRun(nVideoW,nVideoH,pY,pU,pV,pImgProcBuf);
					}
					//������ϵ�ͼ������ת��������ʾ
                if	 (dwFormat== YUV2)
					theDlg->CombineYUVData(nVideoW,nVideoH,pVHdr->lpData,pY,pU,pV);
                if	 (dwFormat== RGB24)
                theDlg->YUVtoRGB(nVideoW,nVideoH,pVHdr->lpData,pY,pU,pV);
			
	
	}	
	delete pY;
    return (LRESULT) TRUE ;
}

//
void CVFWCAPTUREDlg::AddLogInfo(LPCTSTR sMessage, ...)
{
	//����һ��VA_LIST�͵ı��������������ָ�������ָ��,typedef char *  va_list;
	va_list ap;
	char sBuffer[1024];
    //VA_START���ʼ��VA_LIST����, ( ap = (va_list)&v + _INTSIZEOF(v) ) ��ָ���һ����ε�λ��
	va_start(ap, sMessage);
    // vsprintf: format string with va_list arguments
	vsprintf(sBuffer,sMessage,ap);
	//���va_list����������εĻ�ȡ
	va_end(ap);
   //
 	m_List.SetCurSel(m_List.AddString(sBuffer));
   //  m_List.AddString(sBuffer);
	if( m_List.GetCount()>10000 )
		m_List.DeleteString(0);
}
void CVFWCAPTUREDlg::OnSaveimg() 
{
	// TODO: Add your control notification handler code here
	capFileSaveDIB(m_hCapture,"d:\\VFW_Cap.bmp");
	AfxMessageBox("�ļ��ɹ�������d:\\vfw_cap.bmp");
}
void CVFWCAPTUREDlg::OnVfsetup() 
{
	// TODO: Add your control notification handler code here
    capDlgVideoFormat(m_hCapture);
	BYTE* pTemp = pImgProcBuf;
	pImgProcBuf = NULL;
	delete pTemp;
}
void CVFWCAPTUREDlg::OnVfwsetup() 
{
	// TODO: Add your control notification handler code here
	capDlgVideoSource(m_hCapture);
	BYTE* pTemp = pImgProcBuf;
	pImgProcBuf = NULL;
	delete pTemp;
}
void CVFWCAPTUREDlg::OnDspformat() 
{
	// TODO: Add your control notification handler code here
		UpdateData();
}
void CVFWCAPTUREDlg::OnSelchangeList2() 
{
	// TODO: Add your control notification handler code here
	//GetCurSel()��õ�ǰѡ�����
	//SetCheck()ֻ�Ը�ѡ���뵥ѡ��ť��Ч�������������ø�ѡ���ѡ��ť��״̬
	m_PluginEnable.SetCheck(AllPlugIns[m_PluginList.GetCurSel()].bEnabled);	
	// EnableWindow(): ʹĳ���ؼ��򴰿ڿ���
	m_PluginEnable.EnableWindow(TRUE);
	AddLogInfo("PluginInfo:%s",AllPlugIns[m_PluginList.GetCurSel()].OnPlugInInfo());
}
void CVFWCAPTUREDlg::OnPluginEnable() 
{	// TODO: Add your control notification handler code here
	int id = m_PluginList.GetCurSel();
	AllPlugIns[id].bEnabled = m_PluginEnable.GetCheck();
	m_PluginList.DeleteString(id);
	CString sLine;
	sLine.Format("%c%s",AllPlugIns[id].bEnabled?'*':' ',AllPlugIns[id].sName);
	m_PluginList.InsertString(id,sLine);
	m_PluginList.SetCurSel(id);
}

void CVFWCAPTUREDlg::OnActiveall() 
{	// TODO: Add your control notification handler code here
		for(int i=0;i<nPlugInNum;i++)
	{
		AllPlugIns[i].bEnabled = IsDlgButtonChecked(IDC_ACTIVEALL);
		m_PluginList.DeleteString(i);
		CString sLine;
		sLine.Format("%c%s",AllPlugIns[i].bEnabled?'*':' ',AllPlugIns[i].sName);
		m_PluginList.InsertString(i,sLine);
		m_PluginList.SetCurSel(i);
	}
}
//
BOOL CVFWCAPTUREDlg::DestroyWindow() 
{	// TODO: Add your specialized code here and/or call the base class
	capDriverDisconnect(m_hCapture);
	return CDialog::DestroyWindow();
}
//��YUV422��Ƶ���ݸ�ʽ��������YUYVYUYV...��ת��ΪYUV422ƽ��ͼƬ��ʽ(��Y-U-V���)
void CVFWCAPTUREDlg::SplitYUVData(
	int w,int h,//ͼ���С
	BYTE *pYUV, //YUV422��Ƶͼ��ָ��
	BYTE *pY, BYTE *pU, BYTE *pV//YUV422ƽ��ͼƬָ��
	)
{
	BYTE* pLine = pYUV;
	BYTE* pLineY = pY;
 	BYTE* pLineU = pU;
	BYTE* pLineV = pV;
	for(int j=0;j<h;j++)
	{
		for(int i=0;i<w;i++)//һ�д���
		{
			pLineY[i] = pLine[i*2];
		}
		for(int i=0;i<w/2;i++)
		{
			pLineU[i] = pLine[i*4+1];
			pLineV[i] = pLine[i*4+3];
		}
		pLineY += w;
		pLineU += w/2;
		pLineV += w/2;
		pLine += w*2;
	}
}
//��YUV422ƽ��ͼƬ��ʽ(��Y-U-V���)ת��ΪYUV422��Ƶ���ݸ�ʽ��������YUYVYUYV...��

void CVFWCAPTUREDlg::CombineYUVData(
	    int w, int h,//ͼ���С
		BYTE *pYUV, //YUV422��Ƶͼ��ָ��
		BYTE *pY, BYTE *pU, BYTE *pV//YUV422ƽ��ͼƬָ��
		)
{
	BYTE* pLine = pYUV;
	BYTE* pLineY = pY;
	BYTE* pLineU = pU;
	BYTE* pLineV = pV;
	for(int j=0;j<h;j++)
	{
		//Y
		for(int i=0;i<w;i++)//һ�д���
		{
			pLine[i*2] = pLineY[i];
		}

		for( int i=0;i<w/2;i++)
		{
			pLine[i*4+1] = pLineU[i];
			pLine[i*4+3] = pLineV[i];
		}
		pLineY += w;
		pLineU += w/2;
		pLineV += w/2;
		pLine += w*2;//����
	}
}

//��YUV422ƽ��ͼƬ��ʽ(��Y-U-V���)ת��ΪRGB��Ƶ���ݸ�ʽ��������RGBRGB...��
void CVFWCAPTUREDlg::YUVtoRGB(
	    int w, int h,//ͼ���С
		BYTE *pRGB, //YUV422��Ƶͼ��ָ��
		BYTE *pY, BYTE *pU, BYTE *pV//YUV422ƽ��ͼƬָ��
		)
{

	BYTE* pLine = pRGB+w*(h-1)*3;//ָ��RGBͼ��ĵ�һ�У����洢�ռ����һ��
	BYTE* pLineY = pY;//ָ���һ��
	BYTE* pLineU = pU;
	BYTE* pLineV = pV;
	int r, g, b;
    int y, u, v;
	//odd
	for(int j=0;j<h;j++)
	{
		//Y
		for(int i=0;i<w;i++)//һ�д���
		{
		y=(int) pLineY[i];
		u=(int) pLineU[i/2];
        v=(int) pLineV[i/2];
         r = (int) (y + (1.370705 * (v-128)));
         g = (int) (y - (0.698001 * (v-128)) - (0.337633 * (u-128)));
         b = (int) (y + (1.732446 * (u-128)));
         if(r > 255) r = 255;
         if(g > 255) g = 255;
         if(b > 255) b = 255;
         if(r < 0) r = 0;
         if(g < 0) g = 0;
         if(b < 0) b = 0;
         pLine[3*i] =  (unsigned  char) b;
		 pLine[3*i+1] =(unsigned  char ) g;
         pLine[3*i+2] =(unsigned  char ) r;
		 }
		pLineY += w;//����һ��
		pLineU += w/2;
		pLineV += w/2;
		pLine -= w*3;//����һ��
	}
}

//��YUV422ƽ��ͼƬ��ʽ(��Y-U-V���)ת��ΪRGB��Ƶ���ݸ�ʽ��������RGBRGB...��
void CVFWCAPTUREDlg::RGBtoYUV(
	int w, int h,//ͼ���С
	BYTE* pRGB, //YUV422��Ƶͼ��ָ��
	BYTE* pY, BYTE* pU, BYTE* pV//YUV422ƽ��ͼƬָ��
)
{
	BYTE* pLine = pRGB + w * (h - 1) * 3;//ָ��RGBͼ��ĵ�һ�У����洢�ռ����һ��
	BYTE* pLineY = pY;
	BYTE* pLineU = pU;
	BYTE* pLineV = pV;
	int r, g, b;
	int y, u, v;
	for (int j = 0;j < h;j++)
	{
		//Y
		for (int i = 0;i < w;i++)//һ�д���
		{
			b = (int)pLine[3 * i];
			g = (int)pLine[3 * i + 1];
			r = (int)pLine[3 * i + 2];
			y = (int)(0.299 * r + 0.587 * g + 0.114 * b);
			if (y > 255) y = 255;
			if (y < 0) y = 0;
			pLineY[i] = (unsigned char)y;
			if (i % 2 == 0)
			{
				u = (int)(-0.1687 * r - 0.3313 * g + 0.5 * b + 128);
				v = (int)(0.5 * r - 0.4187 * g - 0.0813 * b + 128);
				pLineU[i / 2] = (unsigned char)u;
				pLineV[i / 2] = (unsigned char)v;
			}
		}

		pLineY += w;//����һ��
		pLineU += w / 2;
		pLineV += w / 2;
		pLine -= w * 3;//����һ��
	}
}