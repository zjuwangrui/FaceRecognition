// VFWCAPTUREDlg.h : header file
#if !defined(AFX_VFWCAPTUREDLG_H__5C6A6FE5_1AE3_4857_BD45_A5FC47BD7D70__INCLUDED_)
#define AFX_VFWCAPTUREDLG_H__5C6A6FE5_1AE3_4857_BD45_A5FC47BD7D70__INCLUDED_
#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000
/////////////////////////////////////////////////////////////////////////////
// class ThePlugIns
//���庯��ָ�롣��ʽ����������(*������)(������)��
typedef void (WINAPIV *ON_INITPLUGIN)(LPVOID);//WINAPIV������C��������
typedef void (WINAPIV *ON_PLUGINRUN)(int,int,BYTE*,BYTE*,BYTE*,BYTE*);
typedef void (WINAPIV *ON_PLUGINEXIT)(void);
typedef void (WINAPIV *ON_PLUGIN_BELAST)(bool);
typedef int (WINAPIV *ON_PLUGINCTRL)(int,void*);
typedef LPCTSTR (WINAPIV *ON_PLUGININFO)(void);
class ThePlugIns
{
	HINSTANCE hInst;
public:
	BOOL Enabled(){
		return bPlugInOk && bEnabled;
	}
    BOOL				bPlugInOk;//�ɹ���ȡ����ĺ���ָ����
	//
	ON_PLUGINRUN		OnPlugInRun;
	ON_INITPLUGIN		OnInitPlugIn;
	ON_PLUGINEXIT		OnPlugInExit;
	ON_PLUGIN_BELAST	OnPlugInBeLast;
	ON_PLUGINCTRL		OnPlugInCtrl;
	ON_PLUGININFO		OnPlugInInfo;
	BOOL				bEnabled;
	char				sName[256];//��Ų���ļ���
	//�޲ι��캯������ʼ��
	ThePlugIns()
	{
		OnPlugInRun			= NULL;
		OnInitPlugIn		= NULL;
		OnPlugInExit		= NULL;
		OnPlugInCtrl		= NULL;
		OnPlugInInfo		= NULL;
		OnPlugInBeLast		= NULL;
		hInst				= NULL;
		bPlugInOk			= FALSE;
		bEnabled			= FALSE;
	};
	//��������
	~ThePlugIns()
	{
        //�ͷŶ�̬���ӿ�
		if( hInst )
			FreeLibrary(hInst);
            hInst = NULL;
	}
	//��ȡ����ĺ���ָ��
	BOOL OpenPlugIn(LPCTSTR sPlugInPath)
	{
		//���ض�̬���ӿ�
		hInst = LoadLibrary(sPlugInPath);
		if( hInst!=NULL)//
		{
             //��ȡ��̬���ӿ���ĺ�����ַ��
			OnPlugInRun			= (ON_PLUGINRUN)GetProcAddress(hInst,"ON_PLUGINRUN");
			OnInitPlugIn		= (ON_INITPLUGIN)GetProcAddress(hInst,"ON_INITPLUGIN");
			OnPlugInExit		= (ON_PLUGINEXIT)GetProcAddress(hInst,"ON_PLUGINEXIT");
			OnPlugInBeLast      = (ON_PLUGIN_BELAST)GetProcAddress(hInst,"ON_PLUGIN_BELAST");
			OnPlugInCtrl		= (ON_PLUGINCTRL)GetProcAddress(hInst,"ON_PLUGINCTRL");
			OnPlugInInfo		= (ON_PLUGININFO)GetProcAddress(hInst,"ON_PLUGININFO");
			if( OnPlugInRun && OnInitPlugIn && OnPlugInExit && OnPlugInBeLast )
			{
				char drv[256],dir[256],fname[256],ext[256];
				//�ֽ�·��,void _splitpath( const char *path, char *drive,
				// char *dir, char *fname, char *ext );
				_splitpath(sPlugInPath,drv,dir,fname,ext);
				//   char*strncpy(char*dest,char*src,size_tn);
				//��src��ָ����ַ�������src��ַ��ʼ��ǰn���ֽڸ��Ƶ�dest��ָ�������У�������dest
				strncpy(sName,fname,256);
				// char *strncat(char *dest,char *src,int n);
				//��src��ָ�ַ�����ǰn���ַ�����
				strncat(sName,ext,256);
				bPlugInOk = TRUE;
			}
			else
			{
				OnPlugInRun			= NULL;
				OnInitPlugIn		= NULL;
				OnPlugInExit		= NULL;
				OnPlugInBeLast		= NULL;
				OnPlugInCtrl		= NULL;
				OnPlugInInfo		= NULL;
			}
		}
		return bPlugInOk;
	}
  };
/////////////////////////////////////////////////////////////////////////////
// CVFWCAPTUREDlg dialog
class CVFWCAPTUREDlg : public CDialog
{
// Construction
public:
	CVFWCAPTUREDlg(CWnd* pParent = NULL);	// standard constructor
    void AddLogInfo(LPCTSTR sMessage, ...);
// Dialog Data
	//{{AFX_DATA(CVFWCAPTUREDlg)
	enum { IDD = IDD_VFWCAPTURE_DIALOG };
    CListBox	m_List;
	CListBox    m_PluginList;
    CButton	m_PluginEnable;
    BOOL	m_bDSPFormatSim;
    CString m_sVideoFmt;
	int m_nVideoHeight;
	int m_nVideoWidth;
		// NOTE: the ClassWizard will add data members here
	//}}AFX_DATA

	// ClassWizard generated virtual function overrides
	//{{AFX_VIRTUAL(CVFWCAPTUREDlg)
	public:
	virtual BOOL DestroyWindow();
	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV support
	//}}AFX_VIRTUAL

// Implementation
protected:
	static LRESULT CALLBACK VideoCallbackProc(HWND hWnd, LPVOID lpVHdr);
	void CombineYUVData(int w,int h,BYTE *pYUV, BYTE *pY, BYTE *pU, BYTE *pV);
	void SplitYUVData(int w,int h,BYTE* pYUV,BYTE* pY,BYTE* pU,BYTE* pV);
    void YUVtoRGB(int w,int h,BYTE* pYUV,BYTE* pY,BYTE* pU,BYTE* pV);
    void RGBtoYUV(int w,int h,BYTE* pYUV,BYTE* pY,BYTE* pU,BYTE* pV);
	HICON m_hIcon;
    HWND  m_hCapture;
	ThePlugIns AllPlugIns[32];
    int nPlugInNum;
    BYTE* pBuffer;//process imge
    static int nVideoW,nVideoH;
    static DWORD dwFormat;
	// Generated message map functions
	//{{AFX_MSG(CVFWCAPTUREDlg)
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	afx_msg void OnStart();
    afx_msg void OnTimer(UINT nIDEvent);
	afx_msg void OnSaveimg();
	afx_msg void OnVfsetup();
	afx_msg void OnVfwsetup();
	afx_msg void OnDspformat();
	afx_msg void OnSelchangeList2();
	afx_msg void OnPluginEnable();
	afx_msg void OnActiveall();
	//}}AFX_MSG
	DECLARE_MESSAGE_MAP()
};
//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.
#endif // !defined(AFX_VFWCAPTUREDLG_H__5C6A6FE5_1AE3_4857_BD45_A5FC47BD7D70__INCLUDED_)



