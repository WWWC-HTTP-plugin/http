/**************************************************************************

	WWWC (wwwc.dll)

	http.c

	Copyright (C) 1996-2003 by Nakashima Tomoaki. All rights reserved.
		http://www.nakka.com/
		nakka@nakka.com

**************************************************************************/


/**************************************************************************
	Include Files
**************************************************************************/

#define _INC_OLE
#include <windows.h>
#undef  _INC_OLE
#include <winsock.h>
#include <commctrl.h>
#include <richedit.h>
#ifdef THGHN
#include <process.h>
#endif
#include <time.h>

#include "String.h"
#include "httptools.h"
#include "httpfilter.h"
#include "http.h"
#include "StrTbl.h"
#include "wwwcdll.h"
#include "resource.h"
#include "global.h"
#include "md5.h"


/**************************************************************************
	Define
**************************************************************************/

#define ESC					0x1B		/* エスケープコード */

#define TH_EXIT				(WM_USER + 1200)

#define REQUEST_HEAD		0
#define REQUEST_GET			1

#define HTTP_MENU_CNT		6

#define USER_AGENT			"WWWC/1.04"


/**************************************************************************
	Global Variables
**************************************************************************/

static int srcLen;

static struct TPHOSTINFO *tpHostInfo;
static int HostInfoCnt;

HWND hWndList[100];


//外部参照
extern HINSTANCE ghinst;

extern int CheckType;
extern int TimeOut;

extern int Proxy;
extern char pServer[];
extern int pPort;
extern int pNoCache;
extern int pUsePass;
extern char pUser[];
extern char pPass[];

extern char BrowserPath[];
extern int TimeZone;
extern int HostNoCache;

extern char DateFormat[];
extern char TimeFormat[];


/**************************************************************************
	Local Function Prototypes
**************************************************************************/

//View
static DWORD CALLBACK EditStreamProc(DWORD dwCookie, LPBYTE pbBuf, LONG cb, LONG *pcb);
static BOOL CALLBACK ViewProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

//Title
static char *GetTitle(char *buf);

//Check
static void SetErrorString(struct TPITEM *tpItemInfo, char *buf, BOOL HeadFlag);
static char *CreateOldData(char *buf);
static int GetMetaString(struct TPITEM *tpItemInfo);
static int Head_GetDate(struct TPITEM *tpItemInfo, int CmpOption, BOOL *DateRet);
static int Head_GetSize(struct TPITEM *tpItemInfo, int CmpOption, int SetDate, BOOL *SizeRet);
static int Get_GetSize(struct TPITEM *tpItemInfo, int CmpOption, int SetDate);
static int Get_MD5Check(struct TPITEM *tpItemInfo, int SetDate);
static int HeaderFunc(HWND hWnd, struct TPITEM *tpItemInfo);

//MainWindow
static void SetSBText(HWND hWnd, struct TPITEM *tpItemInfo, char *msg);

//Socket
static void SocketClose(HWND hWnd, struct TPITEM *tpItemInfo, int FreeFlag);
static BOOL ConnectServer(int cSoc, unsigned long cIPaddr, int cPort);
static int HTTPSendRequest(HWND hWnd, struct TPITEM *tpItemInfo);
static BOOL CheckHead(struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP);
static int HTTPRecvData(HWND hWnd, struct TPITEM *tpItemInfo);
static int HTTPClose(HWND hWnd, struct TPITEM *tpItemInfo);

static unsigned int CALLBACK thdGetHostByName(struct TPGETHOSTBYNAME *tpGetHostByName);
static HANDLE _THAsyncGetHostByName(HWND hWnd, UINT wMsg, char *name, DWORD *thID);
static void FreeURLData(struct TPHTTP *tpHTTP);
static BOOL GetServerPort(HWND hWnd, struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP);
static BOOL CheckServerCheck(HWND hWnd, char *RealServer);

//Drag & Dropb
static BOOL WriteInternetShortcut(char *URL, char *path);

//Option
static LRESULT OptionNotifyProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK PropertyProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK PropertyCheckProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK PropertyOptionProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

static BOOL CALLBACK ProtocolPropertyCheckProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


/******************************************************************************

	EditStreamProc

	リッチエディットへの文字列の設定

******************************************************************************/

static DWORD CALLBACK EditStreamProc(DWORD dwCookie, LPBYTE pbBuf, LONG cb, LONG *pcb)
{
	int len;

	lstrcpyn((char *)pbBuf, (char *)dwCookie + srcLen, cb);
	*pcb = len = lstrlen((char *)pbBuf);
	srcLen += len;
    return FALSE;
}


/******************************************************************************

	ViewProc

	アイテムのヘッダー・ソース表示ダイアログ

******************************************************************************/

static BOOL CALLBACK ViewProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct TPITEM *tpItemInfo;
	struct TPHTTP *tpHTTP;
	EDITSTREAM eds;
	RECT DesktopRect, WindowRect;
	CHARRANGE seltext, settext;
	HWND fWnd;
	char *buf;
	char *ret;
	char *filter_title;
	long wl;
	int i;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		CreateWindowEx(WS_EX_CLIENTEDGE, "RICHEDIT",
			(LPSTR)NULL, WS_BORDER | WS_TABSTOP | WS_CHILD | WS_VSCROLL | WS_HSCROLL |
			ES_AUTOHSCROLL | ES_AUTOVSCROLL | ES_MULTILINE | ES_READONLY | ES_SAVESEL |
			ES_NOHIDESEL | ES_DISABLENOSCROLL,
			0, 0, 0, 0, hDlg, (HMENU)IDC_EDIT_VIEW, ghinst, NULL);
		ShowWindow(GetDlgItem(hDlg, IDC_EDIT_VIEW), SW_SHOW);

		GetWindowRect(GetDesktopWindow(), (LPRECT)&DesktopRect);
		GetWindowRect(hDlg, (LPRECT)&WindowRect);
		SetWindowPos(hDlg, HWND_TOP, (DesktopRect.right / 2) - ((WindowRect.right - WindowRect.left) / 2),
			(DesktopRect.bottom / 2) - ((WindowRect.bottom - WindowRect.top) / 2), 0, 0, SWP_NOSIZE);

		tpItemInfo = (struct TPITEM *)lParam;
		tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
		if(tpHTTP == NULL){
			DestroyWindow(hDlg);
			break;
		}

		SendDlgItemMessage(hDlg, IDC_EDIT_VIEW, EM_SETTEXTMODE, TM_RICHTEXT, 0);
		if(tpItemInfo->Param3 == 1){
			buf = (char *)GlobalAlloc(GMEM_FIXED, lstrlen(STR_TITLE_HEADVIEW) + lstrlen(tpItemInfo->CheckURL) + 1);
			if(buf != NULL){
				wsprintf(buf, STR_TITLE_HEADVIEW, tpItemInfo->CheckURL);
			}
			*(tpHTTP->buf + HeaderSize(tpHTTP->buf)) = '\0';
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), WM_SETTEXT, 0, (LPARAM)tpHTTP->buf);

		}else if(tpItemInfo->Param3 == 2){
			SetCursor(LoadCursor(NULL, IDC_WAIT));
			DeleteSizeInfo(tpHTTP->buf, tpHTTP->Size);

			filter_title = "";
			if(tpHTTP->FilterFlag == TRUE){
				//Shiftが押されている場合はフィルタを処理
				tpHTTP->FilterFlag = FilterCheck(tpItemInfo->CheckURL, tpHTTP->buf + HeaderSize(tpHTTP->buf), tpHTTP->Size);
				if(tpHTTP->FilterFlag == TRUE){
					filter_title = STR_TITLE_FILTER;
				}else if(FilterMatch(tpItemInfo->CheckURL) == TRUE){
					filter_title = STR_TITLE_FILTERERR;
				}
			}

			buf = SrcConv(tpHTTP->buf + HeaderSize(tpHTTP->buf), tpHTTP->Size);
			if(buf != NULL){
				i = GetHtml2RtfSize(buf);
				ret = (char *)GlobalAlloc(GMEM_FIXED, i + 1);
				if(ret != NULL){
					Html2Rtf(buf, ret);
					SendDlgItemMessage(hDlg, IDC_EDIT_VIEW, EM_LIMITTEXT, i + 1, 0);
					srcLen = 0;
					eds.dwCookie = (DWORD)ret;
					eds.dwError = 0;
					eds.pfnCallback = EditStreamProc;
					SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), EM_STREAMIN, SF_RTF, (LPARAM)&eds);
					GlobalFree(ret);
				}
				GlobalFree(buf);
			}
			SetCursor(LoadCursor(NULL, IDC_ARROW));

			buf = (char *)GlobalAlloc(GMEM_FIXED, lstrlen(STR_TITLE_SOURCEVIEW) + lstrlen(filter_title) + lstrlen(tpItemInfo->CheckURL) + 1);
			if(buf != NULL){
				wsprintf(buf, STR_TITLE_SOURCEVIEW, filter_title, tpItemInfo->CheckURL);
			}
		}
		if(buf != NULL){
			SetWindowText(hDlg, buf);
			GlobalFree(buf);
		}
		for(i = 0; i < 100; i++){
			if(hWndList[i] == NULL){
				hWndList[i] = hDlg;
				break;
			}
		}
		break;

	case WM_SIZE:
		GetWindowRect(hDlg, (LPRECT)&WindowRect);
		SetWindowPos(GetDlgItem(hDlg, IDC_EDIT_VIEW), 0, 0, 0,
			(WindowRect.right - WindowRect.left) - (GetSystemMetrics(SM_CXFRAME) * 2),
			(WindowRect.bottom - WindowRect.top) - GetSystemMetrics(SM_CYSIZE) - (GetSystemMetrics(SM_CXFRAME) * 2) - 30,
			SWP_NOZORDER);
		SetWindowPos(GetDlgItem(hDlg, IDOK), 0,
			(WindowRect.right - WindowRect.left) - (GetSystemMetrics(SM_CXFRAME) * 2) - 100,
			(WindowRect.bottom - WindowRect.top) - GetSystemMetrics(SM_CYSIZE) - (GetSystemMetrics(SM_CXFRAME) * 2) - 25,
			0, 0, SWP_NOZORDER | SWP_NOSIZE);
		SetWindowPos(GetDlgItem(hDlg, ID_BUTTON_COPY), 0,
			(WindowRect.right - WindowRect.left) - (GetSystemMetrics(SM_CXFRAME) * 2) - 200,
			(WindowRect.bottom - WindowRect.top) - GetSystemMetrics(SM_CYSIZE) - (GetSystemMetrics(SM_CXFRAME) * 2) - 25,
			0, 0, SWP_NOZORDER | SWP_NOSIZE);

		InvalidateRect(GetDlgItem(hDlg, IDOK), NULL, FALSE);
		UpdateWindow(GetDlgItem(hDlg, IDOK));
		InvalidateRect(GetDlgItem(hDlg, ID_BUTTON_COPY), NULL, FALSE);
		UpdateWindow(GetDlgItem(hDlg, ID_BUTTON_COPY));
		break;

	case WM_CLOSE:
		DestroyWindow(hDlg);
		for(i = 0; i < 100; i++){
			if(hWndList[i] == hDlg){
				hWndList[i] = NULL;
				break;
			}
		}
		break;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
		case ID_KEY_ESC:
			SendMessage(hDlg, WM_CLOSE, 0, 0);
			break;

		case ID_MENUITEM_COPY:
		case ID_BUTTON_COPY:
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), EM_EXGETSEL, 0, (LPARAM)&seltext);
			if(seltext.cpMin == seltext.cpMax){
				settext.cpMin = 0;
				settext.cpMax = SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), WM_GETTEXTLENGTH, 0, 0);
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), EM_EXSETSEL, 0, (LPARAM)&settext);
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), WM_COPY, 0, 0);
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), EM_EXSETSEL, 0, (LPARAM)&seltext);
			}else{
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), WM_COPY, 0, 0);
			}
			break;

		case ID_KEY_RETURN:
			SendMessage(hDlg, WM_COMMAND, GetDlgCtrlID(GetFocus()), 0);
			break;

		case ID_KEY_TAB:
			fWnd = GetWindow(GetFocus(), GW_HWNDNEXT);
			if(fWnd == NULL){
				fWnd = GetWindow(GetFocus(), GW_HWNDFIRST);
				SetFocus(fWnd);
				break;
			}
			wl = GetWindowLong(fWnd, GWL_STYLE);
			while((wl & WS_TABSTOP) == 0 || (wl & WS_VISIBLE) == 0){
				fWnd = GetWindow(fWnd, GW_HWNDNEXT);
				if(fWnd == NULL){
					fWnd = GetWindow(GetFocus(), GW_HWNDFIRST);
					break;
				}
				wl = GetWindowLong(fWnd, GWL_STYLE);
			}
			SetFocus(fWnd);
			break;

		case ID_MENUITEM_ALLSELECT:
			if(GetFocus() != GetDlgItem(hDlg, IDC_EDIT_VIEW)){
				break;
			}
			seltext.cpMin = 0;
			seltext.cpMax = SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), WM_GETTEXTLENGTH, 0, 0);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), EM_EXSETSEL, 0, (LPARAM)&seltext);
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


/******************************************************************************

	GetTitle

	HTMLのタイトルを取得

******************************************************************************/

static char *GetTitle(char *buf)
{
#define TITLE_TAG	"title"
	char *SjisBuf, *ret;
	int len;
	BOOL rc;

	//JIS を SJIS に変換
	SjisBuf = SrcConv(buf, lstrlen(buf) + 1);

	//タイトルタグのサイズを取得
	len = GetTagContentSize(SjisBuf, TITLE_TAG);
	if(len == 0){
		return NULL;
	}

	//タイトルを取得
	ret = (char *)GlobalAlloc(GMEM_FIXED, len + 1);
	if(ret == NULL){
		GlobalFree(SjisBuf);
		return NULL;
	}
	rc = GetTagContent(SjisBuf, TITLE_TAG, ret);
	GlobalFree(SjisBuf);

	if(rc == FALSE){
		GlobalFree(ret);
		ret = NULL;
	}
	//特殊文字の変換
	ConvHTMLChar(ret);
	return ret;
}



/******************************************************************************

	SetErrorString

	エラー文字列の設定

******************************************************************************/

static void SetErrorString(struct TPITEM *tpItemInfo, char *buf, BOOL HeadFlag)
{
	char *p, *r, *t;

	if(tpItemInfo->ErrStatus != NULL){
		GlobalFree(tpItemInfo->ErrStatus);
	}

	r = buf;
	if(HeadFlag == TRUE){
		for(; *r != ' '; r++);
		r = (*r == ' ') ? r + 1 : buf;
	}

	for(p = r; *p != '\0' && *p != '\r' && *p != '\n'; p++);

	tpItemInfo->ErrStatus = (char *)GlobalAlloc(GMEM_FIXED, p - r + 2);
	if(tpItemInfo->ErrStatus == NULL) return;

	t = tpItemInfo->ErrStatus;
	while(p > r){
		*(t++) = *(r++);
	}
	*t = '\0';
}


/******************************************************************************

	CreateOldData

	旧データの作成

******************************************************************************/

static char *CreateOldData(char *buf)
{
	char *ret;
	int len;

	if(buf == NULL || *buf == '\0'){
		return NULL;
	}
	len = lstrlen(buf);
	ret = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * (len + 1));
	if(ret != NULL){
		lstrcpy(ret, buf);
		if(*(ret + len - 1) == '*'){
			*(ret + len - 1) = '\0';
		}
	}
	return ret;
}


/******************************************************************************

	GetMetaString

	METAタグの取得

******************************************************************************/

static int GetMetaString(struct TPITEM *tpItemInfo)
{
#define DEF_META_TYPE		"name"
#define DEF_META_NAME		"wwwc"
#define DEF_META_CONTENT	"content"

#define NO_META				"no meta"

	struct TPHTTP *tpHTTP;
	char type[BUFSIZE];
	char name[BUFSIZE];
	char content[BUFSIZE];
	char *MetaContent;
	char *p;
	int CmpMsg = ST_DEFAULT;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return CmpMsg;
	}

	if(GetOptionString(tpItemInfo->Option1, type, OP1_TYPE) == FALSE){
		lstrcpy(type, DEF_META_TYPE);
	}
	if(GetOptionString(tpItemInfo->Option1, name, OP1_NAME) == FALSE){
		lstrcpy(name, DEF_META_NAME);
	}
	if(GetOptionString(tpItemInfo->Option1, content, OP1_CONTENT) == FALSE){
		lstrcpy(content, DEF_META_CONTENT);
	}

	//SJISに変換
	p = SrcConv(tpHTTP->buf + HeaderSize(tpHTTP->buf), tpHTTP->Size);

	if((MetaContent = GetMETATag(p, type, name, content)) != NULL){
		//特殊文字の変換
		ConvHTMLChar(MetaContent);

		if(LresultCmp(tpItemInfo->Date, MetaContent) != 0){
			CmpMsg = ST_UP;
		}
		if(CmpMsg == ST_UP || tpItemInfo->Date == NULL || *tpItemInfo->Date == '\0'){
			//旧更新日の設定
			if(tpItemInfo->OldDate != NULL){
				GlobalFree(tpItemInfo->OldDate);
			}
			tpItemInfo->OldDate = CreateOldData(tpItemInfo->Date);

			// アイテムに今回のチェック内容をセットする
			if(tpItemInfo->Date != NULL){
				GlobalFree(tpItemInfo->Date);
			}
			tpItemInfo->Date = (char *)GlobalAlloc(GPTR, lstrlen(MetaContent) + 1);
			if(tpItemInfo->Date != NULL){
				lstrcpy(tpItemInfo->Date, MetaContent);
			}
		}
		GlobalFree(MetaContent);

	}else{
		//METAタグを取得できなかったメッセージを設定
		if(tpItemInfo->Date != NULL){
			GlobalFree(tpItemInfo->Date);
		}
		tpItemInfo->Date = (char *)GlobalAlloc(GPTR, lstrlen(NO_META) + 1);
		if(tpItemInfo->Date != NULL){
			lstrcpy(tpItemInfo->Date, NO_META);
		}
	}
	GlobalFree(p);
	return CmpMsg;
}


/******************************************************************************

	Head_GetDate

	ヘッダから更新日時を取得

******************************************************************************/

static int Head_GetDate(struct TPITEM *tpItemInfo, int CmpOption, BOOL *DateRet)
{
	struct TPHTTP *tpHTTP;
	char chtime[BUFSIZE];
	char *headcontent;
	int CmpMsg = ST_DEFAULT;
	int len;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return ST_DEFAULT;
	}

	len = GetHeadContentSize(tpHTTP->buf, "Last-Modified:");
	headcontent = (char *)GlobalAlloc(GPTR, len + 2);
	if(headcontent == NULL){
		return ST_DEFAULT;
	}
	*DateRet = GetHeadContent(tpHTTP->buf, "Last-Modified:", headcontent);
	if(*DateRet == TRUE){
		if(CmpOption == 1 && tpItemInfo->DLLData1 != NULL && *tpItemInfo->DLLData1 != '\0' &&
			lstrcmp(tpItemInfo->DLLData1, headcontent) != 0){
			CmpMsg = ST_UP;
		}
		if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->DLLData1 == NULL || *tpItemInfo->DLLData1 == '\0'){
			//DLL用データに日付を格納
			if(tpItemInfo->DLLData1 != NULL){
				GlobalFree(tpItemInfo->DLLData1);
			}
			tpItemInfo->DLLData1 = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(headcontent) + 1);
			if(tpItemInfo->DLLData1 != NULL) lstrcpy(tpItemInfo->DLLData1, headcontent);
		}

		if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->Date == NULL || *tpItemInfo->Date == '\0'){
			//旧更新日の設定
			if(tpItemInfo->OldDate != NULL){
				GlobalFree(tpItemInfo->OldDate);
			}
			tpItemInfo->OldDate = CreateOldData(tpItemInfo->Date);

			//日付形式の変換
			if(DateConv(headcontent, chtime) == 0){
				lstrcpy(headcontent, chtime);
			}
			/* アイテムに今回のチェック内容をセットする */
			if(tpItemInfo->Date != NULL){
				GlobalFree(tpItemInfo->Date);
			}
			tpItemInfo->Date = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(headcontent) + ((CmpMsg == ST_UP) ? 2 : 1));
			if(tpItemInfo->Date != NULL){
				lstrcpy(tpItemInfo->Date, headcontent);
				if(CmpMsg == ST_UP){
					//日付の変更を示す記号の付加
					lstrcat(tpItemInfo->Date, "*");
				}
			}
		}
	}
	GlobalFree(headcontent);
	return CmpMsg;
}


/******************************************************************************

	Head_GetSize

	ヘッダからサイズを取得

******************************************************************************/

static int Head_GetSize(struct TPITEM *tpItemInfo, int CmpOption, int SetDate, BOOL *SizeRet)
{
	struct TPHTTP *tpHTTP;
	char buf[BUFSIZE];
	char *headcontent;
	int CmpMsg = ST_DEFAULT;
	int len;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return ST_DEFAULT;
	}

	/* サイズを取得する */
	len = GetHeadContentSize(tpHTTP->buf, "Content-Length:");
	headcontent = (char *)GlobalAlloc(GPTR, len + 2);
	if(headcontent == NULL){
		return ST_DEFAULT;
	}
	*SizeRet = GetHeadContent(tpHTTP->buf, "Content-Length:", headcontent);
	if(*SizeRet == TRUE){
		/* 前回チェック時のものと比較する */
		if(CmpOption == 1 && LresultCmp(tpItemInfo->Size, headcontent) != 0){
			CmpMsg = ST_UP;

			if(SetDate == 0 && CreateDateTime(buf) == 0){
				//旧更新日の設定
				if(tpItemInfo->OldDate != NULL){
					GlobalFree(tpItemInfo->OldDate);
				}
				tpItemInfo->OldDate = CreateOldData(tpItemInfo->Date);

				/* アイテムに現在の時刻を設定する */
				if(tpItemInfo->Date != NULL){
					GlobalFree(tpItemInfo->Date);
				}
				tpItemInfo->Date = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(buf) + 1);
				if(tpItemInfo->Date != NULL) lstrcpy(tpItemInfo->Date, buf);
			}
		}
		if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->Size == NULL || *tpItemInfo->Size == '\0'){
			//旧サイズの設定
			if(tpItemInfo->OldSize != NULL){
				GlobalFree(tpItemInfo->OldSize);
			}
			tpItemInfo->OldSize = CreateOldData(tpItemInfo->Size);

			/* アイテムに今回のチェック内容をセットする */
			if(tpItemInfo->Size != NULL){
				GlobalFree(tpItemInfo->Size);
			}
			tpItemInfo->Size = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(headcontent) + 1);
			if(tpItemInfo->Size != NULL) lstrcpy(tpItemInfo->Size, headcontent);
		}
	}
	GlobalFree(headcontent);
	return CmpMsg;
}


/******************************************************************************

	Get_GetSize

	HTMLのサイズを取得

******************************************************************************/

static int Get_GetSize(struct TPITEM *tpItemInfo, int CmpOption, int SetDate)
{
	struct TPHTTP *tpHTTP;
	char buf[BUFSIZE];
	char tmp[BUFSIZE];
	char *p;
	int CmpMsg = ST_DEFAULT;
	int head_size;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return ST_DEFAULT;
	}

	head_size = HeaderSize(tpHTTP->buf);
	if(GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 1){
		/* 受信したものからヘッダとタグを除去したサイズを計算する */
		p = SrcConv(tpHTTP->buf + head_size, tpHTTP->Size);
		tpHTTP->Size = DelTagSize(p);
		wsprintf(tmp, "%ld", tpHTTP->Size);
		GlobalFree(p);
	}else if(tpHTTP->FilterFlag == TRUE){
		tpHTTP->Size = lstrlen(tpHTTP->buf + head_size);
		wsprintf(tmp, "%ld", tpHTTP->Size);
	}else{
		/* 受信したものからヘッダを除去したサイズを計算する */
		wsprintf(tmp, "%ld", tpHTTP->Size - head_size);
	}

	/* 前回チェック時のものと比較する */
	if(CmpOption == 1 && LresultCmp(tpItemInfo->Size, tmp) != 0){
		CmpMsg = ST_UP;

		if(SetDate == 0 && CreateDateTime(buf) == 0){
			//旧更新日の設定
			if(tpItemInfo->OldDate != NULL){
				GlobalFree(tpItemInfo->OldDate);
			}
			tpItemInfo->OldDate = CreateOldData(tpItemInfo->Date);

			/* アイテムに現在の時刻を設定する */
			if(tpItemInfo->Date != NULL){
				GlobalFree(tpItemInfo->Date);
			}
			tpItemInfo->Date = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(buf) + 1);
			if(tpItemInfo->Date != NULL) lstrcpy(tpItemInfo->Date, buf);
		}
	}
	if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->Size == NULL || *tpItemInfo->Size == '\0'){
		//旧サイズの設定
		if(tpItemInfo->OldSize != NULL){
			GlobalFree(tpItemInfo->OldSize);
		}
		tpItemInfo->OldSize = CreateOldData(tpItemInfo->Size);

		/* アイテムに今回のチェック内容をセットする */
		if(tpItemInfo->Size != NULL){
			GlobalFree(tpItemInfo->Size);
		}
		tpItemInfo->Size = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(tmp) + 1);
		if(tpItemInfo->Size != NULL) lstrcpy(tpItemInfo->Size, tmp);
	}
	return CmpMsg;
}


/******************************************************************************

	Get_MD5Check

	MD5のダイジェスト値を取得してチェック

******************************************************************************/

static int Get_MD5Check(struct TPITEM *tpItemInfo, int SetDate)
{
	struct TPHTTP *tpHTTP;
	char buf[BUFSIZE];
	int CmpMsg = ST_DEFAULT;
    MD5_CTX context;
    unsigned char digest[16];
	char tmp[34];
	int i, len;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return ST_DEFAULT;
	}

	//ダイジェスト値の生成
    MD5Init(&context);
    MD5Update(&context, tpHTTP->buf + HeaderSize(tpHTTP->buf), lstrlen(tpHTTP->buf + HeaderSize(tpHTTP->buf)));
    MD5Final(digest, &context);

	for(i = 0, len = 0; i < 16; i++, len += 2){
		wsprintf(tmp + len, "%02x", digest[i]);
	}
	*(tmp + len) = '\0';

	/* 前回チェック時のものと比較する */
	if(LresultCmp(tpItemInfo->DLLData2, tmp) != 0){
		CmpMsg = ST_UP;

		if(SetDate == 0 && CreateDateTime(buf) == 0){
			//旧更新日の設定
			if(tpItemInfo->OldDate != NULL){
				GlobalFree(tpItemInfo->OldDate);
			}
			tpItemInfo->OldDate = CreateOldData(tpItemInfo->Date);

			/* アイテムに現在の時刻を設定する */
			if(tpItemInfo->Date != NULL){
				GlobalFree(tpItemInfo->Date);
			}
			tpItemInfo->Date = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(buf) + 1);
			if(tpItemInfo->Date != NULL) lstrcpy(tpItemInfo->Date, buf);
		}
	}
	if(CmpMsg == ST_UP || tpItemInfo->DLLData2 == NULL || *tpItemInfo->DLLData2 == '\0'){
		/* アイテムに今回のチェック内容をセットする */
		if(tpItemInfo->DLLData2 != NULL){
			GlobalFree(tpItemInfo->DLLData2);
		}
		tpItemInfo->DLLData2 = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(tmp) + 1);
		if(tpItemInfo->DLLData2 != NULL) lstrcpy(tpItemInfo->DLLData2, tmp);
	}
	return CmpMsg;
}


/******************************************************************************

	HeaderFunc

	ヘッダから更新情報を取得して処理を行う

******************************************************************************/

static int HeaderFunc(HWND hWnd, struct TPITEM *tpItemInfo)
{
	struct TPHTTP *tpHTTP;
	HWND vWnd;
	char tmp[BUFSIZE];
	char *titlebuf;
	char *headcontent;
	char *p, *r;
	int Status;
	int SizeRet = 0, DateRet = 0;
	int CmpMsg = ST_DEFAULT;
	int DateCheck;
	int SizeCheck;
	int SetDate = 0;
	int len;

	if(tpItemInfo->ErrStatus != NULL){
		GlobalFree(tpItemInfo->ErrStatus);
		tpItemInfo->ErrStatus = NULL;
	}
	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return CHECK_ERROR;
	}

	//HTTPのステータスを取得
	for(p = tpHTTP->buf; *p != ' ' && *p != '\0'; p++);
	if(*p == '\0'){
		SetErrorString(tpItemInfo, STR_ERROR_HEADER, FALSE);
		return CHECK_ERROR;
	}
	p++;
	for(r = tmp; *p != ' ' && *p != '\0'; p++, r++){
		*r = *p;
	}
	*r = '\0';
	if(*p == '\0'){
		SetErrorString(tpItemInfo, STR_ERROR_HEADER, FALSE);
		return CHECK_ERROR;
	}
	Status = atoi(tmp);

	//ツール
	switch(tpItemInfo->Param3){
	case 1:
	case 2:
		if(Status == 301 || Status == 302){
			break;
		}
		//ヘッダ、ソース表示
		//チェックの終了をWWWCに通知
		SendMessage(hWnd, WM_CHECKEND, ST_DEFAULT, (LPARAM)tpItemInfo);
		vWnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_VIEW), NULL, ViewProc, (long)tpItemInfo);
		ShowWindow(vWnd, SW_SHOW);
		SetFocus(GetDlgItem(vWnd, IDC_EDIT_VIEW));
		return CHECK_END;

	case 3:
		if(Status == 301 || Status == 302){
			break;
		}
		//タイトル取得
		if(Status == 200){
			if((titlebuf = GetTitle(tpHTTP->buf + HeaderSize(tpHTTP->buf))) != NULL){
				if(tpItemInfo->Title != NULL){
					GlobalFree(tpItemInfo->Title);
				}
				tpItemInfo->Title = GlobalAlloc(GPTR, lstrlen(titlebuf) + 1);
				lstrcpy(tpItemInfo->Title, titlebuf);
				GlobalFree(titlebuf);
				tpItemInfo->RefreshFlag = TRUE;
			}
		}
		SendMessage(hWnd,WM_CHECKEND,ST_DEFAULT,(LPARAM)tpItemInfo);
		return CHECK_END;
	}

	if(Status == 304){
		//更新が無かった場合
		SendMessage(hWnd, WM_CHECKEND, ST_DEFAULT, (LPARAM)tpItemInfo);
		return CHECK_END;
	}

	//チェック設定
	DateCheck = !GetOptionInt(tpItemInfo->Option1, OP1_NODATE);
	SizeCheck = !GetOptionInt(tpItemInfo->Option1, OP1_NOSIZE);

	switch(Status)
	{
	case 200:	/* 成功の場合 */
		if(tpItemInfo->user2 == REQUEST_HEAD){
			//HEAD
			//更新日時を取得
			CmpMsg |= Head_GetDate(tpItemInfo, DateCheck, &DateRet);
			//サイズを取得
			CmpMsg |= Head_GetSize(tpItemInfo, SizeCheck, DateRet, &SizeRet);
			if(SizeRet == 0 && DateRet == 0){
				//GETリクエスト送信のためのチェック待ち状態
				if(tpItemInfo->Option1 == NULL || *tpItemInfo->Option1 == '\0'){
					if(tpItemInfo->Option1 != NULL) GlobalFree(tpItemInfo->Option1);
					tpItemInfo->Option1 = (char *)GlobalAlloc(GPTR, 2);
					lstrcpy(tpItemInfo->Option1, "1");
				}else{
					*tpItemInfo->Option1 = '1';
				}
				return CHECK_NO;
			}else{
				//チェックの終了をWWWCに通知
				SendMessage(hWnd, WM_CHECKEND, CmpMsg, (LPARAM)tpItemInfo);
			}
			break;
		}

		//GET
		if(GetOptionInt(tpItemInfo->Option1, OP1_META) == 1){
			//METAタグを取得
			CmpMsg |= GetMetaString(tpItemInfo);
			SetDate = 1;
		}else{
			//ヘッダから更新日時を取得
			CmpMsg |= Head_GetDate(tpItemInfo, DateCheck, &SetDate);
		}
		if(GetHeadContentSize(tpHTTP->buf, "Content-Length:") > 0 &&
			GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 0 && tpHTTP->FilterFlag == FALSE){
			if(tpItemInfo->Option1 != NULL && *tpItemInfo->Option1 != '\0' && *tpItemInfo->Option1 != '2'){
				*tpItemInfo->Option1 = '0';
			}
			//ヘッダからサイズを取得
			CmpMsg |= Head_GetSize(tpItemInfo, SizeCheck, SetDate, &SizeRet);
		}
		if(SizeRet == 0){
			//バイト情報の除去
			tpHTTP->Size = DeleteSizeInfo(tpHTTP->buf, tpHTTP->Size);
			//フィルタ処理
			if(tpHTTP->FilterFlag == TRUE &&
				FilterCheck(tpItemInfo->CheckURL, tpHTTP->buf + HeaderSize(tpHTTP->buf), tpHTTP->Size) == FALSE){
				tpHTTP->FilterFlag = FALSE;
				if(tpItemInfo->Date != NULL){
					GlobalFree(tpItemInfo->Date);
				}
				tpItemInfo->Date = (char *)GlobalAlloc(GMEM_FIXED, lstrlen("filter error") + 1);
				if(tpItemInfo->Date != NULL){
					lstrcpy(tpItemInfo->Date, "filter error");
				}
				SetDate = 1;
			}
			//HTMLのサイズを取得
			CmpMsg |= Get_GetSize(tpItemInfo, SizeCheck, SetDate);
		}
		if(GetOptionInt(tpItemInfo->Option1, OP1_MD5) == 1){
			//MD5のダイジェスト値でチェック
			CmpMsg |= Get_MD5Check(tpItemInfo, SetDate);
		}
		//チェックの終了をWWWCに通知
		SendMessage(hWnd, WM_CHECKEND, CmpMsg, (LPARAM)tpItemInfo);
		break;

	case 301:	/* 移動の場合 */
		if(tpItemInfo->Param4 > 10){
			tpItemInfo->Param4 = 0;
			SetErrorString(tpItemInfo, STR_ERROR_MOVE, FALSE);
			SendMessage(hWnd, WM_CHECKEND, ST_ERROR, (LPARAM)tpItemInfo);
			break;
		}
		len = GetHeadContentSize(tpHTTP->buf, "Location:");
		headcontent = (char *)GlobalAlloc(GPTR, len + 1);
		if(headcontent != NULL && GetHeadContent(tpHTTP->buf, "Location:", headcontent) == TRUE){
			tpItemInfo->Param4++;
			if(tpItemInfo->Param2 != 0){
				GlobalFree((HGLOBAL)tpItemInfo->Param2);
			}
			tpItemInfo->Param2 = (long)GlobalAlloc(GPTR, lstrlen(headcontent) + 1);
			if(tpItemInfo->Param2 != 0) lstrcpy((char*)tpItemInfo->Param2, headcontent);

			//チェックするURLを変更
			if(tpItemInfo->CheckURL != NULL){
				GlobalFree((HGLOBAL)tpItemInfo->CheckURL);
			}
			tpItemInfo->CheckURL = (char *)GlobalAlloc(GPTR, lstrlen(headcontent) + 1);
			if(tpItemInfo->CheckURL != NULL) lstrcpy(tpItemInfo->CheckURL, headcontent);
			GlobalFree(headcontent);

			//リクエストの初期化
			return CHECK_NO;
		}else{
			if(headcontent != NULL) GlobalFree(headcontent);
			SetErrorString(tpItemInfo, tpHTTP->buf, TRUE);
			SendMessage(hWnd, WM_CHECKEND, ST_ERROR, (LPARAM)tpItemInfo);
		}
		break;

	case 302:	/* 移動の場合 */
	case 307:
		if(tpItemInfo->Param4 > 10){
			tpItemInfo->Param4 = 0;
			SetErrorString(tpItemInfo, STR_ERROR_MOVE, FALSE);
			SendMessage(hWnd, WM_CHECKEND, ST_ERROR, (LPARAM)tpItemInfo);
			break;
		}
		len = GetHeadContentSize(tpHTTP->buf, "Location:");
		headcontent = (char *)GlobalAlloc(GPTR, len + 1);
		if(headcontent != NULL && GetHeadContent(tpHTTP->buf, "Location:", headcontent) == TRUE){
			tpItemInfo->Param4++;
			if(tpItemInfo->Param2 != 0){
				GlobalFree((HGLOBAL)tpItemInfo->Param2);
			}
			tpItemInfo->Param2 = (long)CreateMoveURL(tpHTTP->Path, headcontent);
			GlobalFree(headcontent);

			//リクエストの初期化
			return CHECK_NO;
		}else{
			if(headcontent != NULL) GlobalFree(headcontent);
			SetErrorString(tpItemInfo, tpHTTP->buf, TRUE);
			SendMessage(hWnd, WM_CHECKEND, ST_ERROR, (LPARAM)tpItemInfo);
		}
		break;

	case 408:	/* サーバタイムアウトの場合 */
		SetErrorString(tpItemInfo, tpHTTP->buf, TRUE);
		SendMessage(hWnd, WM_CHECKEND, ST_TIMEOUT, (LPARAM)tpItemInfo);
		break;

	default:	/* エラーの場合 */
		SetErrorString(tpItemInfo, tpHTTP->buf, TRUE);
		SendMessage(hWnd, WM_CHECKEND, ST_ERROR, (LPARAM)tpItemInfo);
		break;
	}
	return CHECK_END;
}


/******************************************************************************

	SetSBText

	WWWCのステータスバーにテキストを表示する

******************************************************************************/

static void SetSBText(HWND hWnd, struct TPITEM *tpItemInfo, char *msg)
{
	char *buf, *p;

	//ステータスバーにテキストを送る。
	if(tpItemInfo->Param2 != 0){
		buf = (char *)LocalAlloc(LMEM_FIXED, lstrlen(msg) + lstrlen((char *)tpItemInfo->Param2) + 4);
		if(buf == NULL){
			return;
		}
		p = iStrCpy(buf, msg);
		*(p++) = ' ';
		*(p++) = '(';
		p = iStrCpy(p, (char *)tpItemInfo->Param2);
		*(p++) = ')';
		*(p++) = '\0';
		SendMessage(GetDlgItem(hWnd, WWWC_SB), SB_SETTEXT, (WPARAM)0 | 0, (LPARAM)buf);
		LocalFree(buf);
	}else{
		SendMessage(GetDlgItem(hWnd, WWWC_SB), SB_SETTEXT, (WPARAM)0 | 0, (LPARAM)msg);
	}
}


/******************************************************************************

	SocketClose

	ソケットを閉じる

******************************************************************************/

static void SocketClose(HWND hWnd, struct TPITEM *tpItemInfo, int FreeFlag)
{
	if(tpItemInfo->hGetHost1 != NULL){
#ifdef THGHN
		MSG msg;

		SetCursor(LoadCursor(NULL, IDC_WAIT));
		PostThreadMessage((DWORD)tpItemInfo->hGetHost2, TH_EXIT, 0, 0);
		while(tpItemInfo->hGetHost1 != NULL && WaitForSingleObject(tpItemInfo->hGetHost1, 0) == WAIT_TIMEOUT){
			PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE);
		}
		SetCursor(LoadCursor(NULL, IDC_ARROW));
		CloseHandle(tpItemInfo->hGetHost1);
#else
		WSACancelAsyncRequest(tpItemInfo->hGetHost1);
#endif
		tpItemInfo->hGetHost1 = NULL;
	}

	//ソケットを閉じる
	if(tpItemInfo->Soc1 != 0){
		/* ソケットイベントの通知を取り消す */
		WSAAsyncSelect(tpItemInfo->Soc1, hWnd, 0, 0);
		/* ソケットを破棄する */
		closesocket(tpItemInfo->Soc1);
		tpItemInfo->Soc1 = 0;

	}

	//メモリを解放する
	if(tpItemInfo->Param1 != 0){
		if((HGLOBAL)((struct TPHTTP *)tpItemInfo->Param1)->buf != NULL){
			GlobalFree((HGLOBAL)((struct TPHTTP *)tpItemInfo->Param1)->buf);
		}
		GlobalFree((HGLOBAL)tpItemInfo->Param1);
		tpItemInfo->Param1 = 0;
	}

	if(FreeFlag == 0){
		if(tpItemInfo->Param2 != 0){
			GlobalFree((HGLOBAL)tpItemInfo->Param2);
			tpItemInfo->Param2 = 0;
		}
		tpItemInfo->Param3 = 0;
	}
}


/******************************************************************************

	ConnectServer

	サーバに接続

******************************************************************************/

static BOOL ConnectServer(int cSoc, unsigned long cIPaddr, int cPort)
{
	struct sockaddr_in serversockaddr;

	/* サーバのIPアドレスとポート番号を設定 */
	serversockaddr.sin_family = AF_INET;
	serversockaddr.sin_addr.s_addr = cIPaddr;						/* IPアドレス */
	serversockaddr.sin_port = htons((unsigned short)cPort);		/* ポート番号 */
	memset(serversockaddr.sin_zero,(int)0,sizeof(serversockaddr.sin_zero));

	/* サーバに接続する */
	if(connect(cSoc,(struct sockaddr *)&serversockaddr,sizeof(serversockaddr)) == SOCKET_ERROR){
		if(WSAGetLastError() != WSAEWOULDBLOCK){
			return FALSE;
		}
	}
	return TRUE;
}


/******************************************************************************

	HTTPSendRequest

	HTTPリクエストの送信

******************************************************************************/

static int HTTPSendRequest(HWND hWnd, struct TPITEM *tpItemInfo)
{
	struct TPHTTP *tpHTTP;
	char ConvChar[4];
	char BaseStr1[BUFSIZE];
	char BaseStr2[BUFSIZE];
	char user[BUFSIZE];
	char pass[BUFSIZE];
	char buf[BUFSIZE];
	char *p;
	char *SendBuf;
	char *s;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return CHECK_ERROR;
	}

	s = SendBuf = (char *)GlobalAlloc(GPTR, (lstrlen(tpHTTP->Path) * 3) + lstrlen(tpHTTP->hSvName) + 1024);
	if(SendBuf == NULL){
		return CHECK_ERROR;
	}

	//リクエストメソッド (HTTP/1.1 : RFC 2616)
	s = (tpItemInfo->user2 == REQUEST_HEAD) ? iStrCpy(s, "HEAD ") : iStrCpy(s, "GET ");

	//URIに使えない文字の変換 (RFC 2396)
	for(p = tpHTTP->Path; *p != '\0'; p++){
		if(*p == '/' || *p == '.' || *p == ':' ||
			(*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
			*p == '@' || *p == '&' || *p == '=' || *p == '+' || *p == '$' || *p == ',' ||
			*p == '-' || *p == '_' || *p == '!' || *p == '~' || *p == '*' || *p == '\'' ||
			*p == '(' || *p == ')' || *p == '?' || *p == ';' || *p == '%'){
			*(s++) = *p;
		}else if(*p == '#'){
			break;
		}else{
			*(s++) = '%';
			wsprintf(ConvChar, "%02X", (unsigned char)*p);
			*(s++) = *ConvChar;
			*(s++) = *(ConvChar + 1);
		}
	}
	s = iStrCpy(s, " HTTP/1.1\r\nHost: ");
	s = iStrCpy(s, tpHTTP->hSvName);

	if(tpHTTP->hPort != 80){
		wsprintf(buf,":%d",tpHTTP->hPort);
		s = iStrCpy(s, buf);
	}

	//Accept , User-Agent
	s = iStrCpy(s, "\r\nAccept: */*\r\nUser-Agent: ");
	s = iStrCpy(s, USER_AGENT);

	//If-Modified-Since
	if(tpItemInfo->user2 == REQUEST_GET && tpItemInfo->Param3 == 0 &&
		tpItemInfo->DLLData1 != NULL && *tpItemInfo->DLLData1 != '\0' &&
		GetOptionInt(tpItemInfo->Option1, OP1_NODATE) == 0 &&
		GetOptionInt(tpItemInfo->Option1, OP1_META) == 0 &&
		GetOptionInt(tpItemInfo->Option1, OP1_MD5) == 0 &&
		GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 0){
		//GETで更新日でチェックする場合は強制的にIMSを発行
		s = iStrCpy(s, "\r\nIf-Modified-Since: ");
		s = iStrCpy(s, tpItemInfo->DLLData1);
	}

	//キャッシュ制御
	if(Proxy == 1 && pNoCache == 1){
		s = iStrCpy(s, "\r\nPragma: no-cache\r\nCache-Control: no-cache");
	}

	//接続制御
	s = iStrCpy(s, "\r\nConnection: close");

	//プロキシ認証
	if(Proxy == 1 && pUsePass == 1){
		wsprintf(BaseStr1, "%s:%s", pUser, pPass);
		eBase(BaseStr1, BaseStr2);

		s = iStrCpy(s, "\r\nProxy-Authorization: Basic ");
		s = iStrCpy(s, BaseStr2);
	}

	//ページ認証
	*BaseStr2 = '\0';
	if(tpHTTP->user[0] != '\0'){
		lstrcpy(user, tpHTTP->user);

		if(tpHTTP->pass[0] != '\0'){
			lstrcpy(pass, tpHTTP->pass);
		}
		wsprintf(BaseStr1, "%s:%s", user, pass);
		eBase(BaseStr1, BaseStr2);
	}

	if(GetOptionInt(tpItemInfo->Option2, OP2_USEPASS) == 1){
		if(GetOptionString(tpItemInfo->Option2, user, OP2_USER) == FALSE){
			*user = '\0';
		}
		if(GetOptionString(tpItemInfo->Option2, BaseStr2, OP2_PASS) == FALSE){
			*pass = '\0';
		}else{
			dPass(BaseStr2, pass);
		}
		wsprintf(BaseStr1, "%s:%s", user, pass);
		eBase(BaseStr1, BaseStr2);
	}
	if(*BaseStr2 != '\0'){
		s = iStrCpy(s, "\r\nAuthorization: Basic ");
		s = iStrCpy(s, BaseStr2);
	}
	s = iStrCpy(s, "\r\n\r\n");

	//リクエストヘッダの送信
	if(send(tpItemInfo->Soc1, SendBuf, lstrlen(SendBuf), 0) == SOCKET_ERROR){
		GlobalFree(SendBuf);

		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_SEND, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd, tpItemInfo, 0);
		SetErrorString(tpItemInfo, STR_ERROR_SEND, FALSE);
		return CHECK_ERROR;
	}
	GlobalFree(SendBuf);

	SetSBText(hWnd, tpItemInfo, STR_STATUS_RESPONSE);
	return CHECK_SUCCEED;
}


/******************************************************************************

	CheckHead

	受信済みデータに必要な情報を取得しているかチェック

******************************************************************************/

static BOOL CheckHead(struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP)
{
	char *p;

	//すべて受信する必要がある場合
	if(tpItemInfo->Param3 >= 2 || tpHTTP->FilterFlag == TRUE ||
		GetOptionInt(tpItemInfo->Option1, OP1_META) == 1 ||
		GetOptionInt(tpItemInfo->Option1, OP1_MD5) == 1 ||
		GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 1){
		tpHTTP->HeadCheckFlag = TRUE;
		return FALSE;
	}

	//ヘッダをすべて受信しているかチェック
	p = tpHTTP->buf;
	while(1){
		p = StrNextContent(p);
		if(*p == '\0'){
			return FALSE;
		}
		if(*p == '\r' || *p == '\n'){
			break;
		}
	}

	//ヘッダ表示の場合は受信を打ち切る
	if(tpItemInfo->Param3 == 1){
		return TRUE;
	}

	//ヘッダにサイズが設定されているかチェック
	if(tpItemInfo->Param3 == 0 && GetHeadContentSize(tpHTTP->buf, "Content-Length:") <= 0){
		tpHTTP->HeadCheckFlag = TRUE;
		return FALSE;
	}
	return TRUE;
}


/******************************************************************************

	HTTPRecvData

	データを受信して蓄積

******************************************************************************/

static int HTTPRecvData(HWND hWnd, struct TPITEM *tpItemInfo)
{
	struct TPHTTP *tpHTTP;
	char RecvBuf[RECVSIZE];	/* 受信するバッファ */
	char buf[BUFSIZE];
	char *wkbuf, *p;
	int buf_len;			/* 受信したバイト数 */
	int ret;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return CHECK_ERROR;
	}

	//データを受信する
	buf_len = recv(tpItemInfo->Soc1, RecvBuf, RECVSIZE - 1, 0);
	if(buf_len == SOCKET_ERROR){
		if(WSAGetLastError() == WSAEWOULDBLOCK){
			return CHECK_SUCCEED;
		}
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_RECV, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd, tpItemInfo, 0);
		SetErrorString(tpItemInfo, STR_ERROR_RECV, FALSE);
		return CHECK_ERROR;
	}
	*(RecvBuf + buf_len) = '\0';		/* 受信したバッファの後ろにNULLを付加する */

	tpHTTP->Size += buf_len;

	//受信バイト数をステータスバーに出力
	wsprintf(buf, STR_STATUS_RECV, tpHTTP->Size);
	SetSBText(hWnd, tpItemInfo, buf);

	//バッファに受信文字列を蓄積する
	wkbuf = (char *)GlobalAlloc(GMEM_FIXED, tpHTTP->Size + 1);
	if(wkbuf != NULL){
		p = iStrCpy(wkbuf, tpHTTP->buf);
		lstrcpy(p, RecvBuf);
		if(tpHTTP->buf != NULL) GlobalFree((HGLOBAL)tpHTTP->buf);
		tpHTTP->buf = wkbuf;
		if(tpHTTP->HeadCheckFlag == FALSE && CheckHead(tpItemInfo, tpHTTP) == TRUE){
			ret = HeaderFunc(hWnd, tpItemInfo);
			SetSBText(hWnd,tpItemInfo, STR_STATUS_CHECKEND);

			/* ソケットを破棄する */
			SocketClose(hWnd, tpItemInfo, (ret == CHECK_NO) ? 1 : 0);
			return ret;
		}
	}
	return CHECK_SUCCEED;
}


/******************************************************************************

	HTTPClose

	サーバから切断

******************************************************************************/

static int HTTPClose(HWND hWnd, struct TPITEM *tpItemInfo)
{
	struct TPHTTP *tpHTTP;
	char RecvBuf[RECVSIZE];			/* 受信するバッファ */
	char *wkbuf, *p;
	int buf_len;					/* 受信したバイト数 */
	int ret;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return 0;
	}

	WSAAsyncSelect(tpItemInfo->Soc1, hWnd, 0, 0);

	/* 受信バッファにデータがあればすべて受信する */
	while((buf_len = recv(tpItemInfo->Soc1, RecvBuf, RECVSIZE - 1, 0)) > 0 ){
		*(RecvBuf + buf_len) = '\0';	/* バッファの後ろにNULLを付加する */

		tpHTTP->Size += buf_len;

		wkbuf = (char *)GlobalAlloc(GMEM_FIXED, tpHTTP->Size + 1);
		if(wkbuf != NULL){
			p = iStrCpy(wkbuf, tpHTTP->buf);
			lstrcpy(p, RecvBuf);
			if(tpHTTP->buf != NULL) GlobalFree((HGLOBAL)tpHTTP->buf);
			tpHTTP->buf = wkbuf;
			if(tpHTTP->HeadCheckFlag == FALSE && CheckHead(tpItemInfo, tpHTTP) == TRUE){
				break;
			}
		}
	}

	ret = HeaderFunc(hWnd, tpItemInfo);
	SetSBText(hWnd,tpItemInfo, STR_STATUS_CHECKEND);

	/* ソケットを破棄する */
	SocketClose(hWnd, tpItemInfo, (ret == CHECK_NO) ? 1 : 0);
	return ret;
}


/******************************************************************************

	HTTP_FreeItem

	アイテム情報の解放

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_FreeItem(struct TPITEM *tpItemInfo)
{
	if(tpItemInfo->Param1 != 0){
		if((HGLOBAL)((struct TPHTTP *)tpItemInfo->Param1)->buf != NULL){
			GlobalFree((HGLOBAL)((struct TPHTTP *)tpItemInfo->Param1)->buf);
		}
		FreeURLData((struct TPHTTP *)tpItemInfo->Param1);
		GlobalFree((HGLOBAL)tpItemInfo->Param1);
		tpItemInfo->Param1 = 0;
	}
	if(tpItemInfo->Param2 != 0){
		GlobalFree((HGLOBAL)tpItemInfo->Param2);
		tpItemInfo->Param2 = 0;
	}
	return 0;
}


/******************************************************************************

	HTTP_InitItem

	アイテムの初期化

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_InitItem(HWND hWnd, struct TPITEM *tpItemInfo)
{
	if(tpItemInfo->ErrStatus != NULL){
		GlobalFree(tpItemInfo->ErrStatus);
		tpItemInfo->ErrStatus = NULL;
	}
	if((tpItemInfo->Status & ST_UP) == 0){
		/* 更新マーク(*)を除去する */
		if(tpItemInfo->Size != NULL && *tpItemInfo->Size != '\0' &&
			*(tpItemInfo->Size + lstrlen(tpItemInfo->Size) - 1) == '*'){
			*(tpItemInfo->Size + lstrlen(tpItemInfo->Size) - 1) = '\0';
		}
		if(tpItemInfo->Date != NULL && *tpItemInfo->Date != '\0' &&
			*(tpItemInfo->Date + lstrlen(tpItemInfo->Date) - 1) == '*'){
			*(tpItemInfo->Date + lstrlen(tpItemInfo->Date) - 1) = '\0';
		}
		if(tpItemInfo->DLLData2 != NULL && *tpItemInfo->DLLData2 != '\0' &&
			*(tpItemInfo->DLLData2 + lstrlen(tpItemInfo->DLLData2) - 1) == '*'){
			*(tpItemInfo->DLLData2 + lstrlen(tpItemInfo->DLLData2) - 1) = '\0';
		}
	}
	return 0;
}


/******************************************************************************

	HTTP_Initialize

	チェック開始の初期化

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Initialize(HWND hWnd, struct TPITEM *tpItemInfo)
{
	tpItemInfo->user1 = 0;
	tpItemInfo->user2 = 0;
	tpItemInfo->Param4 = 0;
	return 0;
}


/******************************************************************************

	HTTP_Cancel

	チェックのキャンセル

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Cancel(HWND hWnd, struct TPITEM *tpItemInfo)
{
	struct TPHTTP *tpHTTP;
	HWND vWnd;
	char *titlebuf;

	switch(tpItemInfo->Param3){
	case 1:
	case 2:
		//ヘッダ、ソース表示
		vWnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_VIEW), NULL, ViewProc, (long)tpItemInfo);
		ShowWindow(vWnd, SW_SHOW);
		break;

	case 3:
		//タイトル取得
		tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
		if(tpHTTP == NULL){
			return 0;
		}

		if((titlebuf = GetTitle(tpHTTP->buf + HeaderSize(tpHTTP->buf))) != NULL){
			if(tpItemInfo->Title != NULL){
				GlobalFree(tpItemInfo->Title);
			}
			tpItemInfo->Title = GlobalAlloc(GPTR, lstrlen(titlebuf) + 1);
			if(tpItemInfo->Title != NULL){
				lstrcpy(tpItemInfo->Title, titlebuf);
			}
			GlobalFree(titlebuf);
			tpItemInfo->RefreshFlag = TRUE;
		}
		break;
	}
	SocketClose(hWnd,tpItemInfo, 0);
	return 0;
}


/******************************************************************************

	HTTP_Timer

	タイマー (1秒間隔)

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Timer(HWND hWnd, struct TPITEM *tpItemInfo)
{
	if(tpItemInfo->user1 == -1){
		return CHECK_SUCCEED;
	}

	tpItemInfo->user1++;

	if(tpItemInfo->user1 == TimeOut){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_TIMEOUT, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd, tpItemInfo, 0);
		SendMessage(hWnd, WM_CHECKEND, ST_TIMEOUT, (LPARAM)tpItemInfo);
		return CHECK_END;
	}
	return CHECK_SUCCEED;
}


/******************************************************************************

	thdGetHostByName

	ホスト名からIPアドレスを取得

******************************************************************************/

#ifdef THGHN
static unsigned int CALLBACK thdGetHostByName(struct TPGETHOSTBYNAME *tpGetHostByName)
{
	struct hostent *serverhostent;
	unsigned long serveraddr;		/* サーバのIPアドレス */
	HANDLE hEvent;
	MSG msg;

	//順番待ちイベント
	hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, "GHBN_EVENT");
	//他のスレッドが gethostbyname を行っている間は待機する
	while(WaitForSingleObject(hEvent, 1000) == WAIT_TIMEOUT){
		PeekMessage(&msg, NULL, TH_EXIT, TH_EXIT, PM_NOREMOVE);
		if(msg.message == TH_EXIT){
			//キャンセル
			GlobalFree(tpGetHostByName->name);
			GlobalFree(tpGetHostByName);
			_endthreadex(0);
			return 0;
		}
	}
	ResetEvent(hEvent);

	//gethostbyname
	serverhostent = gethostbyname(tpGetHostByName->name);
	if(serverhostent == NULL){
		SendMessage(tpGetHostByName->hWnd, tpGetHostByName->wMsg, (WPARAM)tpGetHostByName->hth, 0);
	}else{
		serveraddr = *((unsigned long *)((serverhostent->h_addr_list)[0]));
		SendMessage(tpGetHostByName->hWnd, tpGetHostByName->wMsg, (WPARAM)tpGetHostByName->hth, serveraddr);
	}

	SetEvent(hEvent);

	GlobalFree(tpGetHostByName->name);
	GlobalFree(tpGetHostByName);

	_endthreadex(0);
	return 0;
}
#endif


/******************************************************************************

	_THAsyncGetHostByName

	ホスト名からIPアドレスを取得するスレッドの作成

******************************************************************************/

#ifdef THGHN
static HANDLE _THAsyncGetHostByName(HWND hWnd, UINT wMsg, char *name, DWORD *thID)
{
	struct TPGETHOSTBYNAME *tpGetHostByName;

	tpGetHostByName = (struct TPGETHOSTBYNAME *)GlobalAlloc(GPTR,sizeof(struct TPGETHOSTBYNAME));
	if(tpGetHostByName == NULL){
		return 0;
	}
	tpGetHostByName->hWnd = hWnd;
	tpGetHostByName->wMsg = wMsg;

	if((tpGetHostByName->name = (char *)GlobalAlloc(GPTR, lstrlen(name) + 1)) == NULL){
		GlobalFree(tpGetHostByName);
		return 0;
	}
	lstrcpy(tpGetHostByName->name, name);

	//スレッドのハンドルを得るためにスレッドを停止状態で作成
	tpGetHostByName->hth = (HANDLE)_beginthreadex(NULL, 0, thdGetHostByName,
		(void *)tpGetHostByName, CREATE_SUSPENDED, (unsigned *)thID);
	if(tpGetHostByName->hth == 0){
		GlobalFree(tpGetHostByName->name);
		GlobalFree(tpGetHostByName);
		return 0;
	}
	//スレッドを開始する
	ResumeThread(tpGetHostByName->hth);
	return tpGetHostByName->hth;
}
#endif


/******************************************************************************

	FreeURLData

	URL情報の解放

******************************************************************************/

static void FreeURLData(struct TPHTTP *tpHTTP)
{
	if(tpHTTP->SvName != NULL) GlobalFree((HGLOBAL)tpHTTP->SvName);
	if(tpHTTP->Path != NULL) GlobalFree((HGLOBAL)tpHTTP->Path);
	if(tpHTTP->hSvName != NULL) GlobalFree((HGLOBAL)tpHTTP->hSvName);
	if(tpHTTP->user != NULL) GlobalFree((HGLOBAL)tpHTTP->user);
	if(tpHTTP->pass != NULL) GlobalFree((HGLOBAL)tpHTTP->pass);
}


/******************************************************************************

	GetServerPort

	接続するサーバとポートを取得する

******************************************************************************/

static BOOL GetServerPort(HWND hWnd, struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP)
{
	char ItemProxy[BUFSIZE];
	char ItemPort[BUFSIZE];
	char *px;
	int pt;
	int ProxyFlag;

	ProxyFlag = GetOptionInt(tpItemInfo->Option2, OP2_NOPROXY);
	if(ProxyFlag == 0 && GetOptionInt(tpItemInfo->Option2, OP2_SETPROXY) == 1){
		//アイテムに設定されたプロキシを使用
		ProxyFlag = 2;
		GetOptionString(tpItemInfo->Option2, ItemProxy, OP2_PROXY);
		GetOptionString(tpItemInfo->Option2, ItemPort, OP2_PORT);

		px = ItemProxy;
		pt = atoi(ItemPort);

	}else if(ProxyFlag == 0 && Proxy == 1){
		px = pServer;
		pt = pPort;
	}

	if((ProxyFlag == 0 && Proxy == 1) || ProxyFlag == 2){
		//Proxyのサーバ名とポート番号を取得
		tpHTTP->SvName = (char *)GlobalAlloc(GPTR, lstrlen(px) + 1);
		if(tpHTTP->SvName == NULL){
			if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
				tpItemInfo->user1 = -1;
				MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
			}
			SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
			return FALSE;
		}
		tpHTTP->Port = GetURL((char *)px, tpHTTP->SvName, NULL, pt, NULL, NULL);
		if(tpHTTP->Port == -1){
			lstrcpy(tpHTTP->SvName, px);
			tpHTTP->Port = pt;
		}

		//Proxyに渡すURLを設定
		tpHTTP->Path = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
		if(tpHTTP->Path == NULL){
			if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
				tpItemInfo->user1 = -1;
				MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
			}
			SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
			return FALSE;
		}
		lstrcpy(tpHTTP->Path, (char *)tpItemInfo->Param2);

		//取得するURLからサーバ名とポート番号を取得
		tpHTTP->hSvName = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
		if(tpHTTP->hSvName == NULL){
			if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
				tpItemInfo->user1 = -1;
				MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
			}
			SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
			return FALSE;
		}
		tpHTTP->user = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
		if(tpHTTP->user == NULL){
			if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
				tpItemInfo->user1 = -1;
				MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
			}
			SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
			return FALSE;
		}
		tpHTTP->pass = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
		if(tpHTTP->pass == NULL){
			if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
				tpItemInfo->user1 = -1;
				MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
			}
			SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
			return FALSE;
		}
		tpHTTP->hPort = GetURL((char *)tpItemInfo->Param2, tpHTTP->hSvName, NULL, 80, tpHTTP->user, tpHTTP->pass);
		if(tpHTTP->Port == -1){
			if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
				tpItemInfo->user1 = -1;
				MessageBox(hWnd, STR_ERROR_URL, tpItemInfo->CheckURL, MB_ICONERROR);
			}
			SetErrorString(tpItemInfo, STR_ERROR_URL, FALSE);
			return FALSE;
		}
		return TRUE;
	}

	/* URLからサーバ名とパスを取得する */
	tpHTTP->SvName = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
	if(tpHTTP->SvName == NULL){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
		return FALSE;
	}
	tpHTTP->Path = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
	if(tpHTTP->Path == NULL){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
		return FALSE;
	}
	tpHTTP->user = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
	if(tpHTTP->user == NULL){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
		return FALSE;
	}
	tpHTTP->pass = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
	if(tpHTTP->pass == NULL){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
		return FALSE;
	}
	tpHTTP->Port = GetURL((char *)tpItemInfo->Param2, tpHTTP->SvName, tpHTTP->Path, 80, tpHTTP->user, tpHTTP->pass);
	if(tpHTTP->Port == -1){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_URL, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_URL, FALSE);
		return FALSE;
	}
	tpHTTP->hSvName = (char *)GlobalAlloc(GPTR, lstrlen(tpHTTP->SvName) + 1);
	if(tpHTTP->hSvName == NULL){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
		return FALSE;
	}
	lstrcpy(tpHTTP->hSvName, tpHTTP->SvName);
	tpHTTP->hPort = tpHTTP->Port;
	return TRUE;
}


/******************************************************************************

	CheckServerCheck

	既にチェック中のサーバかどうかチェックする

******************************************************************************/

static BOOL CheckServerCheck(HWND hWnd, char *RealServer)
{
	struct TPITEM **CheckItemList;
	char *SvName;
	int i, j;
	int Port;

	j = SendMessage(hWnd, WM_GETCHECKLISTCNT, 0, 0);
	CheckItemList = (struct TPITEM **)SendMessage(hWnd, WM_GETCHECKLIST, 0, 0);
	if(CheckItemList == NULL){
		return FALSE;
	}
	for(i = 0; i < j; i++){
		if(*(CheckItemList + i) == NULL){
			continue;
		}
		SvName = (char *)GlobalAlloc(GPTR, lstrlen((*(CheckItemList + i))->CheckURL) + 1);
		if(SvName == NULL) return FALSE;
		Port = GetURL((*(CheckItemList + i))->CheckURL, SvName, NULL, 80, NULL, NULL);
		if(Port != -1 && lstrcmp(RealServer, SvName) == 0){
			GlobalFree(SvName);
			return TRUE;
		}
		GlobalFree(SvName);
	}
	return FALSE;
}


/******************************************************************************

	GetHostCashe

	ホスト情報のキャッシュからIPアドレスを取得

******************************************************************************/

unsigned long GetHostCashe(char *HostName)
{
	int hash;
	int i;

	if(tpHostInfo == NULL){
		return -1;
	}

	hash = str2hash(HostName);
	for(i = 0; i < HostInfoCnt; i++){
		if((tpHostInfo + i)->hash != hash){
			continue;
		}
		if((tpHostInfo + i)->HostName != NULL &&
			lstrcmpi((tpHostInfo + i)->HostName, HostName) == 0){
			return (tpHostInfo + i)->addr;
		}
	}
	return -1;
}


/******************************************************************************

	SetHostCashe

	IPアドレスをキャッシュに追加

******************************************************************************/

static void SetHostCashe(char *HostName, unsigned long addr)
{
	struct TPHOSTINFO *TmpHostInfo;
	int Index = -1, i;

	if(GetHostCashe(HostName) != -1){
		return;
	}
	if(tpHostInfo != NULL){
		for(i = 0; i < HostInfoCnt; i++){
			if((tpHostInfo + i)->HostName == NULL){
				Index = i;
				break;
			}
		}
	}

	if(Index != -1){
		TmpHostInfo = tpHostInfo;
	}else{
		TmpHostInfo = (struct TPHOSTINFO *)LocalAlloc(LPTR, sizeof(struct TPHOSTINFO) * (HostInfoCnt + 1));
		if(TmpHostInfo == NULL){
			return;
		}

		if(tpHostInfo != NULL){
			CopyMemory(TmpHostInfo, tpHostInfo, sizeof(struct TPHOSTINFO) * HostInfoCnt);
		}
		Index = HostInfoCnt;
	}

	(TmpHostInfo + Index)->HostName = (char *)LocalAlloc(LMEM_FIXED, lstrlen(HostName) + 1);
	if((TmpHostInfo + Index)->HostName == NULL){
		if(TmpHostInfo != tpHostInfo){
			LocalFree(TmpHostInfo);
		}
		return;
	}
	lstrcpy((TmpHostInfo + Index)->HostName, HostName);
	(TmpHostInfo + Index)->hash = str2hash(HostName);
	(TmpHostInfo + Index)->addr = addr;

	if(TmpHostInfo != tpHostInfo){
		HostInfoCnt++;
		if(tpHostInfo != NULL){
			LocalFree(tpHostInfo);
		}
		tpHostInfo = TmpHostInfo;
	}
}


/******************************************************************************

	DeleteHostCashe

	キャッシュからホスト情報を削除

******************************************************************************/

static void DeleteHostCashe(char *HostName)
{
	int hash;
	int i;

	if(tpHostInfo == NULL){
		return;
	}

	hash = str2hash(HostName);
	for(i = 0; i < HostInfoCnt; i++){
		if((tpHostInfo + i)->hash != hash){
			continue;
		}
		if((tpHostInfo + i)->HostName != NULL &&
			lstrcmpi((tpHostInfo + i)->HostName, HostName) == 0){
			LocalFree((tpHostInfo + i)->HostName);
			(tpHostInfo + i)->hash = 0;
			(tpHostInfo + i)->HostName = NULL;
			break;
		}
	}
}


/******************************************************************************

	HTTP_Start

	チェックの開始

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Start(HWND hWnd, struct TPITEM *tpItemInfo)
{
	unsigned long serveraddr;		/* サーバのIPアドレス */
	struct TPHTTP *tpHTTP;

	tpHTTP = (struct TPHTTP *)GlobalAlloc(GPTR, sizeof(struct TPHTTP));
	if(tpHTTP == NULL){
		return CHECK_ERROR;
	}
	tpItemInfo->Param1 = (long)tpHTTP;

	//タイマーの初期化
	tpItemInfo->user1 = 0;

	//チェック以外の場合に表示するURLの情報を使用する
	if(tpItemInfo->Param2 == 0){
		if(tpItemInfo->Param3 == 3 && tpItemInfo->ViewURL != NULL && *tpItemInfo->ViewURL != '\0'){
			if((tpItemInfo->Param2 = (long)GlobalAlloc(GPTR, lstrlen(tpItemInfo->ViewURL) + 1)) == 0){
				SocketClose(hWnd, tpItemInfo, 0);
				return CHECK_ERROR;
			}
			lstrcpy((char*)tpItemInfo->Param2, tpItemInfo->ViewURL);
		}else{
			if((tpItemInfo->Param2 = (long)GlobalAlloc(GPTR, lstrlen(tpItemInfo->CheckURL) + 1)) == 0){
				SocketClose(hWnd, tpItemInfo, 0);
				return CHECK_ERROR;
			}
			lstrcpy((char*)tpItemInfo->Param2, tpItemInfo->CheckURL);
		}
	}

	//リクエストメソッドの選択
	switch(tpItemInfo->Param3)
	{
	case 0:		//チェック
	case 1:		//ヘッダ表示
		tpItemInfo->user2 = GetOptionInt(tpItemInfo->Option1, OP1_REQTYPE);
		if(CheckType == 1 || tpItemInfo->user2 == 2 ||
			GetOptionInt(tpItemInfo->Option1, OP1_META) == 1 ||
			GetOptionInt(tpItemInfo->Option1, OP1_MD5) == 1 ||
			GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 1){
			tpItemInfo->user2 = REQUEST_GET;
		}
		if(tpItemInfo->user2 == REQUEST_GET){
			//フィルタ
			tpHTTP->FilterFlag = FilterMatch(tpItemInfo->CheckURL);
		}
		break;

	case 2:		//ソース表示
	case 3:		//タイトル取得
		tpItemInfo->user2 = REQUEST_GET;
		tpHTTP->FilterFlag = (GetAsyncKeyState(VK_SHIFT) < 0) ? TRUE : FALSE;
		break;
	}

	//接続サーバとポート番号を取得
//	*tpHTTP->SvName = '\0';
	tpHTTP->Port = 0;
	if(GetServerPort(hWnd, tpItemInfo, tpHTTP) == FALSE){
		SocketClose(hWnd, tpItemInfo, 0);
		return CHECK_ERROR;
	}

	/* 既に同じサーバへチェックを行っている場合はチェックをしない */
	if(CheckServerCheck(hWnd, tpHTTP->hSvName) == TRUE){
		FreeURLData(tpHTTP);
		GlobalFree(tpHTTP);
		tpItemInfo->Param1 = 0;
		return CHECK_NO;
	}

	/* svNameにドットで区切った10進数のIPアドレスが入っている場合、serveraddrに32bit整数のIPアドレスが返ります */
	serveraddr = inet_addr((char *)tpHTTP->SvName);
	if(serveraddr == -1 && HostNoCache == 0){
		serveraddr = GetHostCashe(tpHTTP->SvName);
	}
	if(serveraddr == -1){

		SetSBText(hWnd, tpItemInfo, STR_STATUS_GETHOST);

		/* サーバ名からサーバのホスト情報を取得する */
		/* ホスト情報が取得できると、ウィンドウにWSOCK_GETHOSTが通知されるようにする */
		memset(tpHTTP->gHostEnt, 0, MAXGETHOSTSTRUCT);
#ifdef THGHN
		tpItemInfo->hGetHost1 = _THAsyncGetHostByName(hWnd, WM_WSOCK_GETHOST, tpHTTP->SvName, (DWORD *)&tpItemInfo->hGetHost2);
#else
		tpItemInfo->hGetHost1 = WSAAsyncGetHostByName(hWnd, WM_WSOCK_GETHOST, tpHTTP->SvName, tpHTTP->gHostEnt, MAXGETHOSTSTRUCT);
#endif
		if(tpItemInfo->hGetHost1 == NULL){
			if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
				tpItemInfo->user1 = -1;
				MessageBox(hWnd, STR_ERROR_GETHOST_TH, tpItemInfo->CheckURL, MB_ICONERROR);
			}
			SocketClose(hWnd, tpItemInfo, 0);
			return CHECK_ERROR;
		}
		return CHECK_SUCCEED;
	}

	/* socにソケットを作成します */
	tpItemInfo->Soc1 = socket(PF_INET, SOCK_STREAM, 0);
	if(tpItemInfo->Soc1 == INVALID_SOCKET){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_SOCKET, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		tpItemInfo->Soc1 = 0;
		SocketClose(hWnd, tpItemInfo, 0);
		return CHECK_ERROR;
	}
	tpHTTP->buf = (char *)GlobalAlloc(GPTR, sizeof(char));
	tpHTTP->Size = 0;
	*tpHTTP->buf = '\0';

	/* 接続、受信、切断のイベントをウィンドウメッセージで通知されるようにする */
	if(WSAAsyncSelect(tpItemInfo->Soc1, hWnd, WM_WSOCK_SELECT, FD_CONNECT | FD_READ | FD_CLOSE) == SOCKET_ERROR){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_SELECT, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd, tpItemInfo, 0);
		return CHECK_ERROR;
	}

	SetSBText(hWnd,tpItemInfo, STR_STATUS_CONNECT);

	/* サーバに接続する */
	if(ConnectServer(tpItemInfo->Soc1, serveraddr, tpHTTP->Port) == FALSE){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_CONNECT, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd, tpItemInfo, 0);
		SetErrorString(tpItemInfo, STR_ERROR_CONNECT, FALSE);
		return CHECK_ERROR;
	}
	return CHECK_SUCCEED;
}


/******************************************************************************

	HTTP_Gethost

	ホスト名からIPを取得

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Gethost(HWND hWnd, WPARAM wParam, LPARAM lParam, struct TPITEM *tpItemInfo)
{
#ifndef THGHN
	struct hostent *serverhostent;
#endif
	unsigned long serveraddr;		/* サーバのIPアドレス */
	struct TPHTTP *tpHTTP;

	if(tpItemInfo->hGetHost1 == NULL){
		SocketClose(hWnd, tpItemInfo, 0);
		return CHECK_ERROR;
	}
#ifdef THGHN
	CloseHandle(tpItemInfo->hGetHost1);
#endif
	tpItemInfo->hGetHost1 = NULL;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		SocketClose(hWnd, tpItemInfo, 0);
		return CHECK_ERROR;
	}

#ifdef THGHN
	if(lParam == 0){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_GETHOST, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd,tpItemInfo, 0);
		SetErrorString(tpItemInfo, STR_ERROR_GETHOST, FALSE);
		return CHECK_ERROR;
	}
	serveraddr = lParam;
#else
	if(WSAGETASYNCERROR(lParam) != 0){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_GETHOST, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd,tpItemInfo, 0);
		SetErrorString(tpItemInfo, STR_ERROR_GETHOST, FALSE);
		return CHECK_ERROR;
	}
	serverhostent = (struct hostent *)tpHTTP->gHostEnt;
	serveraddr = *((unsigned long *)((serverhostent->h_addr_list)[0]));
#endif
	if(HostNoCache == 0){
		SetHostCashe(tpHTTP->SvName, serveraddr);
	}

	tpItemInfo->user1 = 0;

	/* socにソケットを作成します */
	tpItemInfo->Soc1 = socket(PF_INET, SOCK_STREAM, 0);
	if(tpItemInfo->Soc1 == INVALID_SOCKET){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_SOCKET, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		tpItemInfo->Soc1 = 0;
		SocketClose(hWnd, tpItemInfo, 0);
		return CHECK_ERROR;
	}

	tpHTTP->buf = (char *)GlobalAlloc(GPTR, sizeof(char));
	tpHTTP->Size = 0;
	*tpHTTP->buf = '\0';

	/* ソケットイベントをウィンドウメッセージで通知されるようにする */
	if(WSAAsyncSelect(tpItemInfo->Soc1, hWnd, WM_WSOCK_SELECT, FD_CONNECT | FD_READ | FD_CLOSE) == SOCKET_ERROR){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_SELECT, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd, tpItemInfo, 0);
		return CHECK_ERROR;
	}

	SetSBText(hWnd, tpItemInfo, STR_STATUS_CONNECT);

	/* サーバに接続する */
	if(ConnectServer(tpItemInfo->Soc1, serveraddr, tpHTTP->Port) == FALSE){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			MessageBox(hWnd, STR_ERROR_CONNECT, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SocketClose(hWnd, tpItemInfo, 0);
		SetErrorString(tpItemInfo, STR_ERROR_CONNECT, FALSE);
		return CHECK_ERROR;
	}
	return CHECK_SUCCEED;
}


/******************************************************************************

	HTTP_Select

	ソケットのselect

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Select(HWND hWnd, WPARAM wParam, LPARAM lParam, struct TPITEM *tpItemInfo)
{
	struct TPHTTP *tpHTTP;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		SocketClose(hWnd, tpItemInfo, 0);
		return CHECK_ERROR;
	}

	/* エラーの判定 */
	if(WSAGETSELECTERROR(lParam) != 0){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			tpItemInfo->user1 = -1;
			switch(tpHTTP->Status)
			{
			case 0:
				MessageBox(hWnd, STR_ERROR_CONNECT, tpItemInfo->CheckURL, MB_ICONERROR);
				break;
			case 1:
				MessageBox(hWnd, STR_ERROR_RECV, tpItemInfo->CheckURL, MB_ICONERROR);
				break;
			}
		}
		switch(tpHTTP->Status)
		{
		case 0:
			DeleteHostCashe(tpHTTP->SvName);
			SetErrorString(tpItemInfo, STR_ERROR_CONNECT, FALSE);
			break;
		case 1:
			SetErrorString(tpItemInfo, STR_ERROR_RECV, FALSE);
			break;
		}
		SocketClose(hWnd, tpItemInfo, 0);
		return -1;
	}

	//タイマーを初期化
	tpItemInfo->user1 = 0;

	/* ソケットイベント毎の処理を行う */
	switch(WSAGETSELECTEVENT(lParam))
	{
	case FD_CONNECT:					/* サーバへの接続が開始した事を示すイベント */
		tpHTTP->Status = 1;
		/* HTTPリクエストを送信する */
		return HTTPSendRequest(hWnd, tpItemInfo);

	case FD_READ:						/* 受信バッファにデータがある事を示すイベント */
		/* データを受信して蓄積する */
		return HTTPRecvData(hWnd, tpItemInfo);

	case FD_CLOSE:						/* サーバへの接続が終了した事を示すイベント */
		/* 接続を終了する */
		return HTTPClose(hWnd, tpItemInfo);
	}
	return CHECK_ERROR;
}


/******************************************************************************

	HTTP_ItemCheckEnd

	アイテムのチェック終了通知

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_ItemCheckEnd(HWND hWnd, struct TPITEM *tpItemInfo)
{
	return 0;
}


/******************************************************************************

	HTTP_GetItemText

	クリップボード用アイテムのテキストの設定

******************************************************************************/

__declspec(dllexport) HANDLE CALLBACK HTTP_GetItemText(struct TPITEM *tpItemInfo)
{
	HANDLE hMemText;
	char *buf;
	char *URL = NULL;

	if(tpItemInfo->ViewURL != NULL && *tpItemInfo->ViewURL != '\0'){
		URL = tpItemInfo->ViewURL;
	}else if(tpItemInfo->CheckURL != NULL && *tpItemInfo->CheckURL != '\0'){
		URL = tpItemInfo->CheckURL;
	}

	if(URL == NULL){
		return NULL;
	}

	if((hMemText = GlobalAlloc(GHND, lstrlen(URL) + 1)) == NULL){
		return NULL;
	}
	if((buf = GlobalLock(hMemText)) == NULL){
		GlobalFree(hMemText);
		return NULL;
	}
	lstrcpy(buf, URL);
	GlobalUnlock(hMemText);
	return hMemText;
}


/******************************************************************************

	WriteInternetShortcut

	インターネットショートカットの作成

******************************************************************************/

static BOOL WriteInternetShortcut(char *URL, char *path)
{
#define IS_STR		"[InternetShortcut]\r\nURL="
	HANDLE hFile;
	char *buf, *p;
	DWORD ret;
	int len;

	len = lstrlen(IS_STR) + lstrlen(URL) + 2;
	p = buf = (char *)LocalAlloc(LMEM_FIXED, len + 1);
	if(buf == NULL){
		return FALSE;
	}
	p = iStrCpy(p, IS_STR);
	p = iStrCpy(p, URL);
	p = iStrCpy(p, "\r\n");

	/* ファイルを開く */
	hFile = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == NULL || hFile == (HANDLE)-1){
		LocalFree(buf);
		return FALSE;
	}

	/* ファイルに書き込む */
	if(WriteFile(hFile, buf, len, &ret, NULL) == FALSE){
		LocalFree(buf);
		CloseHandle(hFile);
		return FALSE;
	}

	/* メモリの解放 */
	LocalFree(buf);
	/* ファイルを閉じる */
	CloseHandle(hFile);
	return TRUE;
}


/******************************************************************************

	HTTP_CreateDropItem

	アイテムのドロップファイルの設定

******************************************************************************/

__declspec(dllexport) BOOL CALLBACK HTTP_CreateDropItem(struct TPITEM *tpItemInfo, char *fPath, char *iPath, char *ret)
{
	char buf[BUFSIZE];
	char ItemName[BUFSIZE];
	char *URL;
	char *p;
	BOOL rc;

	if(tpItemInfo == NULL){
		return FALSE;
	}

	if(iPath != NULL){
		p = iStrCpy(buf, fPath);
		p = iStrCpy(p, "\\");
		p = iStrCpy(p, iPath);
	}else{
		lstrcpy(buf, fPath);
	}

	URL = NULL;

	lstrcpyn(ItemName, tpItemInfo->Title, BUFSIZE - lstrlen(buf) - 6);
	BadNameCheck(ItemName, '-');

	p = iStrCpy(ret, buf);
	p = iStrCpy(p, "\\");
	p = iStrCpy(p, ItemName);
	p = iStrCpy(p, ".url");

	if(tpItemInfo->ViewURL != NULL && *tpItemInfo->ViewURL != '\0'){
		URL = tpItemInfo->ViewURL;
	}else{
		URL = tpItemInfo->CheckURL;
	}

	if(URL == NULL){
		return FALSE;
	}

	rc = WriteInternetShortcut(URL, ret);
//	rc = WritePrivateProfileString("InternetShortcut", "URL", URL, ret);
//	WritePrivateProfileString(NULL, NULL, NULL, ret);
	return rc;
}


/******************************************************************************

	HTTP_DropFile

	ドロップされたファイルに対する処理

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_DropFile(HWND hWnd, char *FileType, char *FilePath, struct TPITEM *tpItemInfo)
{
	char URL[MAXSIZE];
	char *p, *r;

	if(lstrcmpi(FileType,"url") == 0){
		GetPrivateProfileString("InternetShortcut", "URL", "", URL, MAXSIZE - 1, FilePath);

		for(r = p = FilePath; *p != '\0'; p++){
			if(IsDBCSLeadByte((BYTE)*p) == TRUE){
				p++;
			}else if(*p == '\\' || *p == '/'){
				r = p + 1;
			}
		}
		if(tpItemInfo->Title != NULL){
			GlobalFree(tpItemInfo->Title);
		}
		tpItemInfo->Title = GlobalAlloc(GPTR, lstrlen(r) + 1);
		if(tpItemInfo->Title != NULL){
			lstrcpy(tpItemInfo->Title, r);
		}
		if(lstrlen(tpItemInfo->Title) > 4){
			if(lstrcmpi((tpItemInfo->Title + lstrlen(tpItemInfo->Title) - 4), ".url") == 0){
				*(tpItemInfo->Title + lstrlen(tpItemInfo->Title) - 4) = '\0';
			}
		}

		if(tpItemInfo->CheckURL != NULL){
			GlobalFree(tpItemInfo->CheckURL);
		}
		tpItemInfo->CheckURL = GlobalAlloc(GPTR, lstrlen(URL) + 1);
		if(tpItemInfo->CheckURL != NULL){
			lstrcpy(tpItemInfo->CheckURL, URL);
		}
		return DROPFILE_NEWITEM;
	}
	return DROPFILE_NONE;
}


/******************************************************************************

	HTTP_ExecItem

	アイテムの実行

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_ExecItem(HWND hWnd, char *Action, struct TPITEM *tpItemInfo)
{
	char *p = NULL;
	int rc = -1;

	if(lstrcmp(Action, "open") == 0){
		//開く
		if(tpItemInfo->ViewURL != NULL){
			p = tpItemInfo->ViewURL;
		}
		if(p == NULL || *p == '\0' && tpItemInfo->CheckURL != NULL){
			p = tpItemInfo->CheckURL;
		}
		if(p == NULL || *p == '\0'){
			MessageBox(hWnd, STR_ERR_MSG_URLOPEN, STR_ERR_TITLE_URLOPEN, MB_ICONEXCLAMATION);
			return -1;
		}
		rc = ExecItem(hWnd, p, NULL);

	}else if(lstrcmp(Action, "checkopen") == 0){
		//チェックするURLで開く
		if(tpItemInfo->CheckURL != NULL){
			p = tpItemInfo->CheckURL;
		}
		if(p == NULL || *p == '\0'){
			MessageBox(hWnd, STR_ERR_MSG_URLOPEN, STR_ERR_TITLE_URLOPEN, MB_ICONEXCLAMATION);
			return -1;
		}
		rc = ExecItem(hWnd, p, NULL);

	}else if(lstrcmp(Action, "header") == 0){
		//ヘッダ表示
		tpItemInfo->Param3 = 1;
		SendMessage(hWnd, WM_ITEMCHECK, 0, (LPARAM)tpItemInfo);

	}else if(lstrcmp(Action, "source") == 0){
		//ソース表示
		tpItemInfo->Param3 = 2;
		SendMessage(hWnd, WM_ITEMCHECK, 0, (LPARAM)tpItemInfo);

	}else if(lstrcmp(Action, "gettitle") == 0){
		//タイトル取得
		tpItemInfo->Param3 = 3;
		SendMessage(hWnd, WM_ITEMCHECK, 0, (LPARAM)tpItemInfo);
	}
	// 1 を返すとアイコンを初期化する。
	return rc;
}


/******************************************************************************

	HTTP_GetInfo

	プロトコル情報

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_GetInfo(struct TPPROTOCOLINFO *tpInfo)
{
	int i = 0;

#ifdef THGHN
	CreateEvent(NULL, TRUE, TRUE, "GHBN_EVENT");
#endif

	lstrcpy(tpInfo->Scheme, "http://\thttps://");
	lstrcpy(tpInfo->NewMenu, STR_GETINFO_HTTP_NEWMENU);
	lstrcpy(tpInfo->FileType, "url");	//	複数ある場合は \t で区切る 例) "txt\tdoc\tdat"

	tpInfo->tpMenu = (struct TPPROTOCOLMENU *)GlobalAlloc(GPTR, sizeof(struct TPPROTOCOLMENU) * HTTP_MENU_CNT);
	tpInfo->tpMenuCnt = HTTP_MENU_CNT;

	lstrcpy(tpInfo->tpMenu[i].Name, STR_GETINFO_HTTP_OPEN);
	lstrcpy(tpInfo->tpMenu[i].Action, "open");
	tpInfo->tpMenu[i].Default = TRUE;
	tpInfo->tpMenu[i].Flag = 0;

	i++;
	lstrcpy(tpInfo->tpMenu[i].Name, STR_GETINFO_HTTP_CHECKOPEN);
	lstrcpy(tpInfo->tpMenu[i].Action, "checkopen");
	tpInfo->tpMenu[i].Default = FALSE;
	tpInfo->tpMenu[i].Flag = 0;

	i++;
	lstrcpy(tpInfo->tpMenu[i].Name, "-");
	lstrcpy(tpInfo->tpMenu[i].Action, "");
	tpInfo->tpMenu[i].Default = FALSE;
	tpInfo->tpMenu[i].Flag = 0;

	i++;
	lstrcpy(tpInfo->tpMenu[i].Name, STR_GETINFO_HTTP_HEADER);
	lstrcpy(tpInfo->tpMenu[i].Action, "header");
	tpInfo->tpMenu[i].Default = FALSE;
	tpInfo->tpMenu[i].Flag = 1;

	i++;
	lstrcpy(tpInfo->tpMenu[i].Name, STR_GETINFO_HTTP_SOURCE);
	lstrcpy(tpInfo->tpMenu[i].Action, "source");
	tpInfo->tpMenu[i].Default = FALSE;
	tpInfo->tpMenu[i].Flag = 1;

	i++;
	lstrcpy(tpInfo->tpMenu[i].Name, STR_GETINFO_HTTP_GETTITLE);
	lstrcpy(tpInfo->tpMenu[i].Action, "gettitle");
	tpInfo->tpMenu[i].Default = FALSE;
	tpInfo->tpMenu[i].Flag = 1;
	return 0;
}


/******************************************************************************

	HTTP_EndNotify

	WWWCの終了の通知

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_EndNotify(void)
{
#ifdef THGHN
	HANDLE hEvent;
#endif
	int i;

	for(i = 0; i < 100; i++){
		if(hWndList[i] != NULL){
			SendMessage(hWndList[i], WM_CLOSE, 0, 0);
		}
	}

#ifdef THGHN
	hEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, "GHBN_EVENT");
	if(hEvent != NULL){
		CloseHandle(hEvent);
	}
#endif

	if(tpHostInfo != NULL){
		for(i = 0; i < HostInfoCnt; i++){
			if((tpHostInfo + i)->HostName != NULL){
				LocalFree((tpHostInfo + i)->HostName);
			}
		}
		LocalFree(tpHostInfo);
		tpHostInfo = NULL;
	}

	FreeFilter();
	return 0;
}
/* End of source */
