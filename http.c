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
#include <commctrl.h>
#include <richedit.h>
#include <time.h>
#include <tchar.h>
#include <stdio.h>

#include <wininet.h>
#include <process.h>

#include "String.h"
#include "httptools.h"
#include "httpfilter.h"
#include "http.h"
#include "StrTbl.h"
#include "wwwcdll.h"
#include "resource.h"
#include "global.h"
#include "md5.h"

#include "def.h"
#include "feed.h"
#include "auth.h"
#include "regexp.h"



/**************************************************************************
	Define
**************************************************************************/

#define ESC					0x1B		/* エスケープコード */

#define TH_EXIT				(WM_USER + 1200)


#define HTTP_MENU_CNT		8



/**************************************************************************
	Global Variables
**************************************************************************/

static int srcLen;

HWND hWndList[100];


//外部参照
extern HINSTANCE ghinst;

extern int CheckType;
extern int TimeOut;

extern int Proxy;
extern char pServer[];
extern char pPort[];
extern int pNoCache;
extern int pUsePass;
extern char pUser[];
extern char pPass[];

extern char BrowserPath[];
extern int TimeZone;
extern int HostNoCache;

extern char DateFormat[];
extern char TimeFormat[];

extern BOOL ErrorNotify;


/**************************************************************************
	Local Function Prototypes
**************************************************************************/

//View
static DWORD CALLBACK EditStreamProc(DWORD dwCookie, LPBYTE pbBuf, LONG cb, LONG *pcb);
static BOOL CALLBACK ViewProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

//Title
static char *GetTitle(char *buf, int BufSize);

//Check
static int GetMetaString(struct TPITEM *tpItemInfo);
static int Head_GetDate(struct TPITEM *tpItemInfo, int CmpOption, BOOL *DateRet);
static int Head_GetSize(struct TPITEM *tpItemInfo, int CmpOption, int SetDate, BOOL *SizeRet);
static int Get_GetSize(struct TPITEM *tpItemInfo, int CmpOption, int SetDate);
static int Get_MD5Check(struct TPITEM *tpItemInfo, int SetDate);
static int HeaderFunc(HWND hWnd, struct TPITEM *tpItemInfo);

static char *GetRssItem(char *buf);

//MainWindow
static void SetSBText(HWND hWnd, struct TPITEM *tpItemInfo, char *msg);

static void FreeURLData(struct TPHTTP *tpHTTP);

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
	LPTSTR Headers, RegStrList;
	int HeadersLen;

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
			// ヘッダ表示
			// メモリを確保
			HttpQueryInfo(
				((struct THARGS*)tpItemInfo->ITEM_MARK)->hHttpReq,
				HTTP_QUERY_RAW_HEADERS_CRLF,
				NULL,
				&HeadersLen, // 返りはメモリサイズ
				NULL
			);
			Headers = M_ALLOC(HeadersLen);
			// 本取得
			if ( Headers != NULL ) { 
				HttpQueryInfo(
					((struct THARGS*)tpItemInfo->ITEM_MARK)->hHttpReq,
					HTTP_QUERY_RAW_HEADERS_CRLF,
					Headers,
					&HeadersLen,
					NULL
				);
			}
			buf = S_ALLOC(lstrlen(STR_TITLE_HEADVIEW) + lstrlen(tpItemInfo->CheckURL));
			if(buf != NULL){
				wsprintf(buf, STR_TITLE_HEADVIEW, tpItemInfo->CheckURL);
			}
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEW), WM_SETTEXT, 0, (LPARAM)Headers);
			M_FREE(Headers);

		}else if(tpItemInfo->Param3 == 2){
			// ソース表示
			SetCursor(LoadCursor(NULL, IDC_WAIT));

			filter_title = "";
			if(tpHTTP->FilterFlag == TRUE){
				//Shiftが押されている場合はフィルタを処理

				if ( GetOptionInt(tpItemInfo->Option1, OP1_META) == 2 ) {
					// フィード
					buf = SrcConv(tpHTTP->Body, tpHTTP->Size);
					if ( buf != NULL ) {
						M_FREE(tpHTTP->Body);
						tpHTTP->Body = buf;
					}
					if ( GetOptionNum(tpItemInfo->ITEM_FILTER, FEED_FILTER_TYPE_SOURCE) != 0
								&& (RegStrList = GetOptionText(tpItemInfo->ITEM_FILTER, FEED_FILTER_SOURCE, TRUE))
									!= NULL
					) {
						buf = ListReplace(tpHTTP->Body, RegStrList);
						if ( buf != NULL ) {
							M_FREE(tpHTTP->Body);
							tpHTTP->Body = buf;
							tpHTTP->Size = lstrlen(buf);
							filter_title = _T(" (ソース置換)");
						}
						M_FREE(RegStrList);
					}

				} else {
					tpHTTP->FilterFlag = FilterCheck(tpItemInfo->CheckURL, tpHTTP->Body, tpHTTP->Size);
					if(tpHTTP->FilterFlag == TRUE){
						filter_title = STR_TITLE_FILTER;
					}else if(FilterMatch(tpItemInfo->CheckURL) == TRUE){
						filter_title = STR_TITLE_FILTERERR;
					}
				}
			}

			buf = SrcConv(tpHTTP->Body, tpHTTP->Size);
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

static char *GetTitle(char *buf, int BufSize)
{
#define TITLE_TAG	"title"
	char *SjisBuf, *ret;
	int len;
	BOOL rc;

	//JIS を SJIS に変換
	SjisBuf = SrcConv(buf, BufSize + 2);

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

	GetRssItem

	RSS-Itemを取得

******************************************************************************/

static char *GetRssItem(char *buf)
{
	char *content;
	char *p, *tmp;
	char chtime[BUFSIZE];
	int len;
	int i;

	// RSS判定
	if( strstr(buf, "<?xml") == NULL ) return NULL;
	if( strstr(buf, "<item>") != NULL ){		// RSS 2.0
		p = strstr(buf, "<item>");
	} else if( strstr(buf, "<item ") != NULL){	// RSS 1.0
		p = strstr(buf, "<item ");
	} else if( strstr(buf, "<entry>") != NULL){	// Atom
		p = strstr(buf, "<entry>");
	} else {
		return NULL;
	}
	if( strstr(p, "<title") == NULL ) return NULL;

	//タイトルの取得
	len = GetTagContentSize(p, "title");
	if(len == 0) return NULL;
	content = (char *)GlobalAlloc(GMEM_FIXED, len + 19);
	if( content == NULL ) return NULL;
//	strncpy(content, "0000/00/00 00:00 ", 17);
	strncpy_s(content, len + 19, "0000/00/00 00:00 ", 17);
	if( GetTagContent(p, "title", content + 17) == FALSE ){
		GlobalFree(content);
		return NULL;
	}

	//更新日の取得
	// RSS 1.0 Dublin Core
	if( (len = GetTagContentSize(p, "dc:date")) != 0 ){
		tmp = (char *)GlobalAlloc(GMEM_FIXED, len + 1);
		if( tmp == NULL ){
			GlobalFree(content);
			return NULL;
		}
		GetTagContent(p, "dc:date", tmp);
	}
	// RSS 2.0
	else if( (len = GetTagContentSize(p, "pubdate")) != 0 ){
		tmp = (char *)GlobalAlloc(GMEM_FIXED, len + 1);
		if( tmp == NULL ){
			GlobalFree(content);
			return NULL;
		}
		if( GetTagContent(p, "pubdate", tmp) && DateConv(tmp, chtime) == 0 ) lstrcpy(tmp, chtime);
		else tmp[0] = '\0';
	}
	// Atom 1.0
	else if( (len = GetTagContentSize(p, "updated")) != 0 ){
		tmp = (char *)GlobalAlloc(GMEM_FIXED, len + 1);
		if( tmp == NULL ){
			GlobalFree(content);
			return NULL;
		}
		GetTagContent(p, "updated", tmp);
	}
	// Atom 0.3
	else if( (len = GetTagContentSize(p, "modified")) != 0 ){
		tmp = (char *)GlobalAlloc(GMEM_FIXED, len + 1);
		if( tmp == NULL ){
			GlobalFree(content);
			return NULL;
		}
		GetTagContent(p, "modified", tmp);
	}
	else {
		return content;
	}

	if( lstrlen(tmp) > 16 ) tmp[16] = '\0';
	for(i = 0; tmp[i] != '\0'; i++){
		if( tmp[i] >= '0' && tmp[i] <= '9' ) content[i] = tmp[i];
	}
	GlobalFree(tmp);

	return content;
}


/******************************************************************************

	SetErrorString

	エラー文字列の設定

******************************************************************************/

void SetErrorString(struct TPITEM *tpItemInfo, char *buf, BOOL HeadFlag)
{
	char *p, *r, *t;

	AddEntryTitle(tpItemInfo, buf);

	if (tpItemInfo->ErrStatus != NULL){
		GlobalFree(tpItemInfo->ErrStatus);
	}

	if ( buf == NULL ) {
		tpItemInfo->ErrStatus = NULL;
		return;
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



// yyyy/MM/dd hh:mm の16文字分を比較、新 : -1、古 : 1、同 : 0
// フィードで値が得られた時のみ呼ばれる前提
int DateCompare(LPTSTR OldDate, LPTSTR NewDate) {
	// 旧日時が NO_META なら更新とする
	if ( lstrcmp(OldDate, _T("no meta")) == 0 ) {
		return -1;
	}
	return lstrcmpn(OldDate, NewDate, 16);
}


/******************************************************************************

	GetMetaString

	METAタグの取得

******************************************************************************/
//#include <stdio.h>
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
	char *tmp;
	int CmpMsg = ST_DEFAULT;

	int CompRet = 0;


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

	tmp = GlobalAlloc(GMEM_FIXED, GlobalSize(tpHTTP->Body));
	if(tmp == NULL)	return CmpMsg;
	memcpy(tmp, tpHTTP->Body, GlobalSize(tpHTTP->Body));
	//SJISに変換
	p = SrcConv(tmp, tpHTTP->Size);
	GlobalFree(tmp);

	// フィード
	if( (MetaContent = GetRssItem(p)) != NULL ) {
		//特殊文字の変換
		ConvHTMLChar(MetaContent);

		CompRet = DateCompare(tpItemInfo->Date, MetaContent);
		if ( CompRet == -1 ) {
			CmpMsg = ST_UP;
			// 更新情報
			M_FREE(tpItemInfo->ITEM_UPINFO);
			tpItemInfo->ITEM_UPINFO = MakeUpInfo(MetaContent + 17, lstrlen(MetaContent) - 17, tpItemInfo, 1);
			// 新日時の更新マークを付ける
			lstrcat(MetaContent, _T("*"));
		}
	// metaタグ	
	} else if ( (MetaContent = GetMETATag(p, type, name, content)) != NULL ) {
		//特殊文字の変換
		ConvHTMLChar(MetaContent);

		if(LresultCmp(tpItemInfo->Date, MetaContent) != 0){
			CmpMsg = ST_UP;
			// 更新情報
			M_FREE(tpItemInfo->ITEM_UPINFO);
			tpItemInfo->ITEM_UPINFO = S_ALLOC(lstrlen(MetaContent));
			if ( tpItemInfo->ITEM_UPINFO != NULL ) {
				lstrcpy(tpItemInfo->ITEM_UPINFO, MetaContent);
				*(tpItemInfo->ITEM_UPINFO + lstrlen(MetaContent) - 1) = _T('\0');
			}
		}

	}

	if ( MetaContent != NULL ) {
		if(CmpMsg == ST_UP || CompRet || tpItemInfo->Date == NULL || *tpItemInfo->Date == '\0'){
		// 未来日が与えられたら過去に戻れなくなるので未更新でも日時は変えておく
			M_FREE(tpItemInfo->Date);
			tpItemInfo->Date = MetaContent;
		}

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


// メモリを作って LastModified を返す
// TRUE  : ヘッダがあってメモリを作った
// FALSE : ヘッダがなくてメモリを作らなかった
BOOL GetLastModified(struct TPITEM *tpItemInfo, OUT LPTSTR *LastModified) {
	int LastModifiedSize = 0;

	// 1回試す
	HttpQueryInfo(
		((struct THARGS*)tpItemInfo->ITEM_MARK)->hHttpReq,
		HTTP_QUERY_LAST_MODIFIED,
		NULL,
		&LastModifiedSize,
		NULL
	);

	// ヘッダがないっぽい
	if ( LastModifiedSize == 0 ) {
		return FALSE;
	}

	*LastModified = M_ALLOC(LastModifiedSize);
	if( *LastModified == NULL ){
		return FALSE;
	}

	HttpQueryInfo(
		((struct THARGS*)tpItemInfo->ITEM_MARK)->hHttpReq,
		HTTP_QUERY_LAST_MODIFIED,
		*LastModified,
		&LastModifiedSize,
		NULL
	);

	// メモリを作ったので2回目の結果によらず TRUE
	return TRUE;
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

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return ST_DEFAULT;
	}


	*DateRet = GetLastModified(tpItemInfo, &headcontent);

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
		GlobalFree(headcontent);
	}
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
	int ContentLengthSize, i, OldSizeLen;


	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if ( tpHTTP == NULL || tpHTTP->ContentLength == -1 ) {
		return ST_DEFAULT;
	}

	// HEADチェック可能フラグ
	*SizeRet = TRUE;

	// サイズとか
	for( ContentLengthSize = 2, i = tpHTTP->ContentLength; i /= 10; ContentLengthSize++ );
	headcontent = S_ALLOC(ContentLengthSize);
	_itot_s(tpHTTP->ContentLength, headcontent, ContentLengthSize, 10);

	// 前回チェック時のものと比較する
	if ( CmpOption == 1 && tpItemInfo->Size != NULL && *tpItemInfo->Size != _T('\0') ) {
		// 文字列比較用に更新マークを除く
		OldSizeLen = lstrlen(tpItemInfo->Size);
		if ( *(tpItemInfo->Size + OldSizeLen - 1) == _T('*') ) {
			*(tpItemInfo->Size + OldSizeLen - 1) = _T('\0');
			i = -1;
		}
		// 比較
		if ( lstrcmp(tpItemInfo->Size, headcontent) != 0 ) {
			CmpMsg = ST_UP;
			*(headcontent + ContentLengthSize - 1) = _T('*');
			*(headcontent + ContentLengthSize) = _T('\0');

			if(SetDate == 0 && CreateDateTime(buf) == 0){
				// アイテムに現在の時刻を設定する
				M_FREE(tpItemInfo->Date);
				tpItemInfo->Date = S_ALLOC(lstrlen(buf));
				if(tpItemInfo->Date != NULL) lstrcpy(tpItemInfo->Date, buf);
			}
		} else if ( i == -1 ) {
			*(tpItemInfo->Size + OldSizeLen - 1) = _T('*');
		}
	}

	if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->Size == NULL || *tpItemInfo->Size == _T('\0') ){
		// アイテムに今回のチェック内容をセットする
		M_FREE(tpItemInfo->Size);
		tpItemInfo->Size = headcontent;
	} else {
		M_FREE(headcontent);
	}


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
	char headcontent[BUFSIZE];
	char *p;
	int CmpMsg = ST_DEFAULT;
	int ContentLengthSize, i, OldSizeLen;

	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return ST_DEFAULT;
	}

	if(GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 1){
		/* 受信したものからヘッダとタグを除去したサイズを計算する */
		p = SrcConv(tpHTTP->Body, tpHTTP->Size);
		tpHTTP->Size = DelTagSize(p);
		wsprintf(headcontent, "%ld", tpHTTP->Size);
		GlobalFree(p);
	}else if(tpHTTP->FilterFlag == TRUE){
		tpHTTP->Size = lstrlen(tpHTTP->Body);
		wsprintf(headcontent, "%ld", tpHTTP->Size);
	}else{
		/* 受信したものからヘッダを除去したサイズを計算する */
		wsprintf(headcontent, "%ld", tpHTTP->Size);
	}
	ContentLengthSize = lstrlen(headcontent) + 1;

	// 前回チェック時のものと比較する
	if ( CmpOption == 1 && tpItemInfo->Size != NULL && *tpItemInfo->Size != _T('\0') ) {
		// 文字列比較用に更新マークを除く
		OldSizeLen = lstrlen(tpItemInfo->Size);
		if ( *(tpItemInfo->Size + OldSizeLen - 1) == _T('*') ) {
			*(tpItemInfo->Size + OldSizeLen - 1) = _T('\0');
			i = -1;
		}
		// 比較
		if ( lstrcmp(tpItemInfo->Size, headcontent) != 0 ) {
			CmpMsg = ST_UP;
			*(headcontent + ContentLengthSize - 1) = _T('*');
			*(headcontent + ContentLengthSize) = _T('\0');

			if(SetDate == 0 && CreateDateTime(buf) == 0){
				// アイテムに現在の時刻を設定する
				M_FREE(tpItemInfo->Date);
				tpItemInfo->Date = S_ALLOC(lstrlen(buf));
				if(tpItemInfo->Date != NULL) lstrcpy(tpItemInfo->Date, buf);
			}
		} else if ( i == -1 ) {
			*(tpItemInfo->Size + OldSizeLen - 1) = _T('*');
		}
	}

	if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->Size == NULL || *tpItemInfo->Size == '\0'){
		/* アイテムに今回のチェック内容をセットする */
		M_FREE(tpItemInfo->Size);
		tpItemInfo->Size = S_ALLOC(ContentLengthSize);
		if(tpItemInfo->Size != NULL) lstrcpy(tpItemInfo->Size, headcontent);
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
    MD5Update(&context, tpHTTP->Body, lstrlen(tpHTTP->Body));
    MD5Final(digest, &context);

	for(i = 0, len = 0; i < 16; i++, len += 2){
		wsprintf(tmp + len, "%02x", digest[i]);
	}
	*(tmp + len) = '\0';

	/* 前回チェック時のものと比較する */
	if(LresultCmp(tpItemInfo->DLLData2, tmp) != 0){
		CmpMsg = ST_UP;

		if(SetDate == 0 && CreateDateTime(buf) == 0){
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
	char *titlebuf;
	int SizeRet = 0, DateRet = 0;
	int CmpMsg = ST_DEFAULT;
	int DateCheck;
	int SizeCheck;
	int SetDate = 0;
	TCHAR HeaderText[64];
	int HeaderSize;


	if(tpItemInfo->ErrStatus != NULL && GetOptionInt(tpItemInfo->Option1, OP1_META) == 0 ){
		GlobalFree(tpItemInfo->ErrStatus);
		tpItemInfo->ErrStatus = NULL;
	}
	tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	if(tpHTTP == NULL){
		return ST_ERROR;
	}


	//ツール
	switch(tpItemInfo->Param3){
	case 1:
	case 2:
		//ヘッダ、ソース表示
		//チェックの終了をWWWCに通知
		vWnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_VIEW), NULL, ViewProc, (long)tpItemInfo);
		ShowWindow(vWnd, SW_SHOW);
		SetFocus(GetDlgItem(vWnd, IDC_EDIT_VIEW));
		return ST_DEFAULT;

	case 3:
		//タイトル取得
		if(tpHTTP->StatusCode == 200 ){
			if((titlebuf = GetTitle(tpHTTP->Body, tpHTTP->Size)) != NULL){
				if(tpItemInfo->Title != NULL){
					GlobalFree(tpItemInfo->Title);
				}
				tpItemInfo->Title = GlobalAlloc(GPTR, lstrlen(titlebuf) + 1);
				lstrcpy(tpItemInfo->Title, titlebuf);
				GlobalFree(titlebuf);
				tpItemInfo->RefreshFlag = TRUE;
			}
			return ST_DEFAULT;
		} else {
			break;
		}
	}

	if(tpHTTP->StatusCode == 304){
		//更新が無かった場合
		return ST_DEFAULT;
	}

	//チェック設定
	DateCheck = !GetOptionInt(tpItemInfo->Option1, OP1_NODATE);
	SizeCheck = !GetOptionInt(tpItemInfo->Option1, OP1_NOSIZE);


	switch(tpHTTP->StatusCode)
	{
	case 100:
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
				return CONTINUE_GET;
			}
			break;
		}

		//GET or POST
		switch ( GetOptionInt(tpItemInfo->Option1, OP1_META) ) {
			case 1 : //METAタグを取得
				CmpMsg |= GetMetaString(tpItemInfo);
				SetDate = 1;
				M_FREE(tpItemInfo->DLLData1);
				GetLastModified(tpItemInfo, &tpItemInfo->DLLData1);
				break;

			case 2 : // フィード
				CmpMsg = GetEntries(tpItemInfo);
				if ( CmpMsg != ST_ERROR ) {
					M_FREE(tpItemInfo->DLLData1);
					GetLastModified(tpItemInfo, &tpItemInfo->DLLData1);
				}
				return CmpMsg;

			case 3 : // リンク抽出
				CmpMsg = GetLinks(tpItemInfo);
				return CmpMsg;

			default : //ヘッダから更新日時を取得
				CmpMsg |= Head_GetDate(tpItemInfo, DateCheck, &SetDate);
				break;
		}
		//ヘッダからサイズを取得
		if( tpHTTP->ContentLength != -1
					&& GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 0
					&& tpHTTP->FilterFlag == FALSE
				){
			CmpMsg |= Head_GetSize(tpItemInfo, SizeCheck, SetDate, &SizeRet);
			// 自動で GET なら HEAD に変更
			if( tpItemInfo->Option1 != NULL
						&& *tpItemInfo->Option1 == _T('1')
						&& GetOptionInt(tpItemInfo->Option1, OP1_META) == 0
						&& GetOptionInt(tpItemInfo->Option1, OP1_MD5) == 0
					) {
				*tpItemInfo->Option1 = _T('0');
			}
		}
		if(SizeRet == 0){
			//フィルタ処理
			if(tpHTTP->FilterFlag == TRUE &&
				FilterCheck(tpItemInfo->CheckURL, tpHTTP->Body, tpHTTP->Size) == FALSE){
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
		break;

	default:	/* エラーの場合 */
		_itot_s(tpHTTP->StatusCode, HeaderText, 4, 10);
		*(HeaderText + 3) = _T(' ');
		*(HeaderText + 4) = _T('\0');
		HeaderSize = sizeof(HeaderText) - (4 * sizeof(TCHAR));
		HttpQueryInfo(
			((struct THARGS*)tpItemInfo->ITEM_MARK)->hHttpReq,
			HTTP_QUERY_STATUS_TEXT,
			HeaderText + 4,
			&HeaderSize,
			NULL
		);

		SetErrorString(tpItemInfo, HeaderText, FALSE);
		if ( tpHTTP->StatusCode == 408 ) {
			CmpMsg = ST_TIMEOUT;
		} else {
			CmpMsg = ST_ERROR;
		}

		// 初回がエラーなら正常時に更新となるように
		if ( tpItemInfo->Date == NULL || *tpItemInfo->Date == _T('\0') ) {
			M_FREE(tpItemInfo->Date);
			tpItemInfo->Date = S_ALLOC(lstrlen(tpItemInfo->CheckDate));
			if ( tpItemInfo->Date != NULL ) {
				lstrcpy(tpItemInfo->Date, tpItemInfo->CheckDate);
			}
		}
		if ( tpItemInfo->Size == NULL || *tpItemInfo->Size == _T('\0') ) {
			M_FREE(tpItemInfo->Size);
			tpItemInfo->Size = S_ALLOC(1);
			if ( tpItemInfo->Size != NULL ) {
				*tpItemInfo->Size = _T('0');
				*(tpItemInfo->Size + 1) = _T('\0');
			}
		}
		break;
	}
	return CmpMsg;
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

	CheckHead

	受信済みデータに必要な情報を取得しているかチェック

******************************************************************************/

static BOOL CheckHead(struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP)
{
//	char *p;

	// 絶対取得しない
	if ( tpItemInfo->user2 == REQUEST_HEAD || tpHTTP->StatusCode == 304 || tpItemInfo->Param3 == 1) {
		return TRUE;
	}


	//すべて受信する必要がある場合
	if( tpItemInfo->Param3 == 2 ){
		// ソース表示
		return FALSE;
	}

	// 成功時は取得する
	if ( tpHTTP->StatusCode == 200
				&& ( tpItemInfo->Param3 == 3
					|| tpHTTP->FilterFlag == TRUE
					|| GetOptionInt(tpItemInfo->Option1, OP1_META)
					|| GetOptionInt(tpItemInfo->Option1, OP1_MD5) == 1
					|| GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 1
				)
			) {
		return FALSE;
	}

	// 普通は取得しない
	if ( tpHTTP->StatusCode != 200 ) {
		return TRUE;
	}

	//ヘッダにサイズが設定されているかチェック
	if(tpItemInfo->Param3 == 0 && tpHTTP->ContentLength == -1 ){
		return FALSE;
	}

	return TRUE;
}



/******************************************************************************

	HTTP_FreeItem

	アイテム情報の解放

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_FreeItem(struct TPITEM *tpItemInfo)
{
	if(tpItemInfo->Param1 != 0){
		if((HGLOBAL)((struct TPHTTP *)tpItemInfo->Param1)->Body != NULL){
			GlobalFree((HGLOBAL)((struct TPHTTP *)tpItemInfo->Param1)->Body);
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

	if(tpItemInfo->ErrStatus != NULL && GetOptionInt(tpItemInfo->Option1, OP1_META) == 0 ){
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
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	HWND vWnd;
	char *titlebuf;


	tpItemInfo->ITEM_CANCEL = 1;

	switch(tpItemInfo->Param3){
	case 1:
	case 2:
		//ヘッダ、ソース表示
		vWnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_VIEW), NULL, ViewProc, (long)tpItemInfo);
		ShowWindow(vWnd, SW_SHOW);
		break;

	case 3:
		//タイトル取得
		if(tpHTTP == NULL){
//			return 0;
			break;
		}

		if((titlebuf = GetTitle(tpHTTP->Body, tpHTTP->Size)) != NULL){
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

	// メモリとハンドルを解放する
	// キャンセル時は _Select が呼ばれない
	// タイミングは大丈夫なのか？
	WinetClear(tpItemInfo);


	return 0;
}



/******************************************************************************

	FreeURLData

	URL情報の解放

******************************************************************************/

static void FreeURLData(struct TPHTTP *tpHTTP)
{
	if(tpHTTP->SvName != NULL) GlobalFree((HGLOBAL)tpHTTP->SvName);
	if(tpHTTP->Path != NULL) GlobalFree((HGLOBAL)tpHTTP->Path);
	if(tpHTTP->hSvName != NULL) GlobalFree((HGLOBAL)tpHTTP->hSvName);
}


/******************************************************************************

	GetServerPort

	接続するサーバとポートを取得する

******************************************************************************/

BOOL GetServerPort(HWND hWnd, struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP)
{
	char ItemProxy[BUFSIZE];
	char ItemPort[BUFSIZE];
	char *px;
	char *pt;
	int ProxyFlag;

/* http>
	プラグイン設定由来のグローバル変数 Proxy は
	1 : Proxy使う
	0 : Proxy使わない

	アイテム設定由来のここの ProxyFlag は
	2 : アイテムのProxy設定を使う
	1 : Proxy無効
	0 : Proxy無効にしていない

	tpHTTP->ProxyFlag は最終結果として
	TRUE  : Proxy使う
	FALSE : Proxy使わない
*/

	ProxyFlag = GetOptionInt(tpItemInfo->Option2, OP2_NOPROXY);
	if(ProxyFlag == 0 && GetOptionInt(tpItemInfo->Option2, OP2_SETPROXY) == 1){
		//アイテムに設定されたプロキシを使用
		ProxyFlag = 2;
		GetOptionString(tpItemInfo->Option2, ItemProxy, OP2_PROXY);
		GetOptionString(tpItemInfo->Option2, ItemPort, OP2_PORT);

		px = ItemProxy;
		pt = ItemPort;

		tpHTTP->ProxyFlag = 2;

	}else if(ProxyFlag == 0 && Proxy == 1){
		px = pServer;
		pt = pPort;
		tpHTTP->ProxyFlag = 1;

	} else {
		tpHTTP->ProxyFlag = 0;
	}

	if( tpHTTP->ProxyFlag ) {
		//Proxyのサーバ名とポート番号を取得
		tpHTTP->SvName = (char *)GlobalAlloc(GPTR, lstrlen(px) + lstrlen(pt) + 2);
		if(tpHTTP->SvName == NULL){
			if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
				MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
			}
			SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
			return FALSE;
		}
		lstrcpy(tpHTTP->SvName, px);
		if ( *pt != '\0' ) {
			lstrcat(tpHTTP->SvName, ":");
			lstrcat(tpHTTP->SvName, pt);
		}
	}

	/* URLからサーバ名とパスを取得する */
	tpHTTP->hSvName = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
	if(tpHTTP->hSvName == NULL){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
		return FALSE;
	}
	tpHTTP->Path = (char *)GlobalAlloc(GPTR, lstrlen((char *)tpItemInfo->Param2) + 1);
	if(tpHTTP->Path == NULL){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			MessageBox(hWnd, STR_ERROR_MEMORY, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_MEMORY, FALSE);
		return FALSE;
	}

	tpHTTP->hPort = GetURL((char *)tpItemInfo->Param2, tpHTTP->hSvName, tpHTTP->Path, &tpHTTP->SecureFlag);
	if(tpHTTP->hPort == -1){
		if(tpItemInfo->Param3 == 1 || tpItemInfo->Param3 == 2){
			MessageBox(hWnd, STR_ERROR_URL, tpItemInfo->CheckURL, MB_ICONERROR);
		}
		SetErrorString(tpItemInfo, STR_ERROR_URL, FALSE);
		return FALSE;
	}

	return TRUE;
}



/******************************************************************************

	HTTP_Start

	チェックの開始

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Start(HWND hWnd, struct TPITEM *tpItemInfo)
{
	struct THARGS *ThArgs = NULL;
	struct TPHTTP *tpHTTP;
	LPTSTR p;


	tpHTTP = M_ALLOC_Z(sizeof(struct TPHTTP));
	if(tpHTTP == NULL){
		return CHECK_ERROR;
	}
	tpItemInfo->Param1 = (long)tpHTTP;


	// アイテム作成直後なら、フィードっぽいURLか見て設定
	if ( tpItemInfo->Option1 == NULL
				&& ( _tcsstr(tpItemInfo->CheckURL, _T("feed")) != NULL
					|| _tcsstr(tpItemInfo->CheckURL, _T("rss")) != NULL
					|| _tcsstr(tpItemInfo->CheckURL, _T("atom")) != NULL
					|| _tcsstr(tpItemInfo->CheckURL, _T("rdf")) != NULL
					|| _tcsstr(tpItemInfo->CheckURL, _T("xml")) != NULL
				)
	) {
		tpItemInfo->Option1 = S_ALLOC(15);
		if ( tpItemInfo->Option1 != NULL ) {
			lstrcpy(tpItemInfo->Option1, _T("0;;0;;0;;0;;2;;"));
		}
	
	// feed スキーム
	} else if ( *tpItemInfo->CheckURL == _T('f')
				&& *(tpItemInfo->CheckURL + 1) == _T('e')
				&& *(tpItemInfo->CheckURL + 2) == _T('e')
				&& *(tpItemInfo->CheckURL + 3) == _T('d')
				&& *(tpItemInfo->CheckURL + 4) == _T(':')
	) {
		if ( lstrlen(tpItemInfo->Option1) >= 15) {
			*(tpItemInfo->Option1 + 12) = _T('2');
		}

		// feed:http(s):
		if ( *(tpItemInfo->CheckURL + 5) == _T('h')
					&& *(tpItemInfo->CheckURL + 6) == _T('t')
					&& *(tpItemInfo->CheckURL + 7) == _T('t')
					&& *(tpItemInfo->CheckURL + 8) == _T('p')
					&& ( *(tpItemInfo->CheckURL + 9) == _T(':')
						|| (*(tpItemInfo->CheckURL + 9) == _T('s')
							&& *(tpItemInfo->CheckURL + 10) == _T(':'))
					)
		) {
			p = tpItemInfo->CheckURL + 5;
			while ( *p != _T('\0') ) {
				*(p - 5) = *p++;
			}
			*(p - 5) = _T('\0');

		// feed://
		} else {
			*tpItemInfo->CheckURL = _T('h');
			*(tpItemInfo->CheckURL + 1) = _T('t');
			*(tpItemInfo->CheckURL + 2) = _T('t');
			*(tpItemInfo->CheckURL + 3) = _T('p');
		}
	}

	// 旧サイズを消す
	if ( tpItemInfo->ITEM_SITEURL != NULL
				&& *tpItemInfo->ITEM_SITEURL >= _T('0')
				&& *tpItemInfo->ITEM_SITEURL <= _T('9')
	) {
		*tpItemInfo->ITEM_SITEURL = _T('\0');
	}


	//チェック以外の場合に表示するURLの情報を使用する
	if(tpItemInfo->Param2 == 0){
		if(tpItemInfo->Param3 == 3 && tpItemInfo->ViewURL != NULL && *tpItemInfo->ViewURL != '\0' && GetOptionInt(tpItemInfo->Option1, OP1_META) < 2 ){
			if((tpItemInfo->Param2 = (long)GlobalAlloc(GPTR, lstrlen(tpItemInfo->ViewURL) + 1)) == 0){
				WinetCheckEnd(hWnd, tpItemInfo, ST_ERROR);
				return CHECK_ERROR;
			}
			lstrcpy((char*)tpItemInfo->Param2, tpItemInfo->ViewURL);
		}else{
			if((tpItemInfo->Param2 = (long)GlobalAlloc(GPTR, lstrlen(tpItemInfo->CheckURL) + 1)) == 0){
				WinetCheckEnd(hWnd, tpItemInfo, ST_ERROR);
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
		// プロトコル設定かアイテム設定で常にGET
		if(CheckType == 1 || tpItemInfo->user2 == 2	) {
			tpItemInfo->user2 = REQUEST_GET;
		}
		// 自動かつ本文が必要
		if ( tpItemInfo->user2 == 0 &&
					(
						( GetOptionInt(tpItemInfo->Option1, OP1_NOSIZE) == 0 &&
							GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) ) ||
						GetOptionInt(tpItemInfo->Option1, OP1_META) ||
						GetOptionInt(tpItemInfo->Option1, OP1_MD5)
					)
				) {
			tpItemInfo->user2 = REQUEST_GET;
			*(tpItemInfo->Option1) = _T('1');
		}

		if(tpItemInfo->user2 == REQUEST_GET || tpItemInfo->user2 == REQUEST_POST){
			//フィルタ
			tpHTTP->FilterFlag = FilterMatch(tpItemInfo->CheckURL);
		}
		break;

	case 2:		//ソース表示
	case 3:		//タイトル取得
		tpItemInfo->user2 = (GetOptionInt(tpItemInfo->Option1, OP1_REQTYPE) == REQUEST_POST) ? REQUEST_POST : REQUEST_GET;
		tpHTTP->FilterFlag = (GetAsyncKeyState(VK_SHIFT) < 0) ? TRUE : FALSE;
		break;
	}

	//接続サーバとポート番号を取得
	if(GetServerPort(hWnd, tpItemInfo, tpHTTP) == FALSE){
		WinetCheckEnd(hWnd, tpItemInfo, ST_ERROR);
		return CHECK_ERROR;
	}


	// 別スレへの引数
	ThArgs = M_ALLOC_Z(sizeof(struct THARGS));
	if ( ThArgs == NULL ) {
		WinetCheckEnd(hWnd, tpItemInfo, ST_ERROR);
		return CHECK_ERROR;
	}
	// 値
	ThArgs->hWnd = hWnd;
	ThArgs->tpItemInfo = tpItemInfo;

	ThArgs->hThread = (HANDLE)_beginthreadex( 
		NULL, // security
		0, // stack_size
		WinetStart, // start_address
		ThArgs, // arglist
		CREATE_SUSPENDED, // initflag
		NULL // thrdaddr
	);
	if ( ThArgs->hThread == 0 ) {
		SetErrorString(tpItemInfo, _T("リクエストスレッド作成失敗"), FALSE);
		WinetCheckEnd(hWnd, tpItemInfo, 1);
		return CHECK_ERROR;
	}
	tpItemInfo->ITEM_MARK = (ITEM_MARK_TYPE)ThArgs;
	ResumeThread(ThArgs->hThread);

	return CHECK_SUCCEED;
}



/******************************************************************************

	HTTP_Select

	ソケットのselect

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Select(HWND hWnd, WPARAM wParam, LPARAM lParam, struct TPITEM *tpItemInfo)
{
	if ( tpItemInfo->ITEM_CANCEL ) {
		return CHECK_END;
	}
	return WinetCheckEnd(hWnd, tpItemInfo, (int)lParam);
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

	} else if ( lstrcmp(Action, "siteopen") == 0 ) {
		// サイトURLを開く
		if(tpItemInfo->ITEM_SITEURL != NULL){
			p = tpItemInfo->ITEM_SITEURL;
		}
		if(p == NULL || *p == '\0'){
			MessageBox(hWnd, STR_ERR_MSG_URLOPEN, STR_ERR_TITLE_URLOPEN, MB_ICONEXCLAMATION);
			return -1;
		}
		rc = ExecItem(hWnd, p, NULL);

	} else if ( lstrcmp(Action, "clear") == 0 ) {
		M_FREE(tpItemInfo->Size);
		M_FREE(tpItemInfo->Date);
		M_FREE(tpItemInfo->CheckDate);
		M_FREE(tpItemInfo->ErrStatus);
		M_FREE(tpItemInfo->DLLData1);
		M_FREE(tpItemInfo->DLLData2);
		rc = 1;
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

	lstrcpy(tpInfo->Scheme, _T("http://\thttps://\tfeed:"));
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
	lstrcpy(tpInfo->tpMenu[i].Name, _T("サイトを開く(&B)"));
	lstrcpy(tpInfo->tpMenu[i].Action, _T("siteopen"));
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

	i++;
	lstrcpy(tpInfo->tpMenu[i].Name, "更新情報のクリア(&A)");
	lstrcpy(tpInfo->tpMenu[i].Action, "clear");
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
	int i;

	for(i = 0; i < 100; i++){
		if(hWndList[i] != NULL){
			SendMessage(hWndList[i], WM_CLOSE, 0, 0);
		}
	}

	FreeFilter();
	return 0;
}





/******************************************************************************

	以下WinInet用

******************************************************************************/

// 別スレッドでリクエスト開始
DWORD WINAPI WinetStart(struct THARGS *ThArgs) {
	struct TPITEM *tpItemInfo = ThArgs->tpItemInfo;
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	int CmpMsg;

	CmpMsg = WinetReq(ThArgs, tpItemInfo, tpHTTP);

	PostMessage(ThArgs->hWnd, WM_WSOCK_SELECT, (WPARAM)tpItemInfo->ITEM_MARK, (LPARAM)CmpMsg);
	_endthreadex(0);

	return 0;
}


// エラーメッセージのプロシージャ
BOOL CALLBACK ErrorMessageProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch( uMsg ){
		case WM_INITDIALOG :
			return TRUE;

		case WM_COMMAND :
			switch ( wParam ) {
				case IDOK :
				case IDCANCEL :
					DestroyWindow(hDlg);
					break;
			}
			return TRUE;

		case WM_CLOSE :
			DestroyWindow(hDlg);
			return TRUE;
	}

	return FALSE;
}



// メモリとハンドルを解放する
void WinetClear(struct TPITEM *tpItemInfo) {
	struct THARGS *ThArgs = (struct THARGS *)tpItemInfo->ITEM_MARK;
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;


	if ( ThArgs != NULL ) {
		if ( ThArgs->hHttpReq != NULL ) {
			InternetCloseHandle(ThArgs->hHttpReq);
		}
		if ( ThArgs->hHttpCon != NULL ) {
			InternetCloseHandle(ThArgs->hHttpCon);
		}
		if ( ThArgs->hInet != NULL ) {
			InternetCloseHandle(ThArgs->hInet);
		}
		if ( ThArgs->hThread != NULL ) {
			CloseHandle(ThArgs->hThread);
		}
		M_FREE(ThArgs);
		tpItemInfo->ITEM_MARK = (ITEM_MARK_TYPE)NULL;
	}

	if( tpHTTP != NULL ){
		M_FREE(tpHTTP->Body);
		FreeURLData(tpHTTP);
		M_FREE(tpHTTP);
		tpItemInfo->Param1 = 0;
	}

	M_FREE((void *)tpItemInfo->Param2);
	tpItemInfo->Param3 = 0;


	return;
}


// チェックして、結果を WWWC へ通知して終了
int WinetCheckEnd(HWND hWnd, struct TPITEM *tpItemInfo, int CmpMsg) {
	struct THARGS *ThArgs = (struct THARGS *)tpItemInfo->ITEM_MARK;
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	int ExecFlag;
	HWND hDlg;


	// チェック
	if ( CmpMsg == ST_DEFAULT ) {
		CmpMsg = HeaderFunc(hWnd, tpItemInfo);
	}


	// 更新時に実行
	if ( (CmpMsg == ST_UP) && ((ExecFlag = GetOptionInt(tpItemInfo->Option2, OP2_EXEC)) & 1) ) {
		if ( ExecOptionCommand(tpItemInfo,
						( tpItemInfo->ViewURL != NULL && *tpItemInfo->ViewURL != _T('\0') ) 
							? tpItemInfo->ViewURL : tpItemInfo->CheckURL
					) == FALSE
				) {
			SetErrorString(tpItemInfo, STR_MEM_ERR, FALSE);
			goto END;
		}

		// 更新通知しない
		if ( ExecFlag & 2 ) {
			HTTP_InitItem(hWnd, tpItemInfo);
			CmpMsg = ST_DEFAULT;
		}

	// エラーを通知
	} else if ( CmpMsg == ST_ERROR && ErrorNotify && tpItemInfo->Param3 == 0 ) {
		hDlg = CreateDialog(ghinst, (LPCTSTR)IDD_DIALOG_ERR, NULL, (DLGPROC)ErrorMessageProc);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_ERR_INFO), tpItemInfo->ErrStatus);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_ERR_TITLE), tpItemInfo->Title);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_ERR_URL), tpItemInfo->CheckURL);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_ERR_DATE), tpItemInfo->CheckDate);
	}

	END:

	// メモリとハンドルを解放する
	WinetClear(tpItemInfo);

	// GETでやりなおし
	if ( CmpMsg == CONTINUE_GET ) {
		return CHECK_NO;
	}

	SendMessage(hWnd, WM_CHECKEND, (WPARAM)CmpMsg, (LPARAM)tpItemInfo);
	SetSBText(hWnd, tpItemInfo, STR_STATUS_CHECKEND);


	return CHECK_END;
}



// Set-Cookieを突合
// メモリを作る
LPTSTR SetCookie(LPCTSTR BeforeCookie, LPCTSTR NewCookie) {
	int CookieNameLen = 0, CookieValueLen = 0, CookieNameIdx, Hit = 0;
	LPTSTR AfterCookie, AfterCookiePtr;
	LPCTSTR CookieValuePtr;


	CookieValuePtr = NewCookie;
	// 名前の文字数
	while ( *CookieValuePtr != _T('=') && *CookieValuePtr != _T(';') && *CookieValuePtr != _T('\0') && *CookieValuePtr != _T(' ') ) {
		CookieValuePtr++;
		CookieNameLen++;
	}
	while ( *CookieValuePtr == _T(' ') ) CookieValuePtr++;
	if ( CookieNameLen == 0 || *CookieValuePtr != _T('=') ) return NULL;

	// =
	CookieValuePtr++; 
	while ( *CookieValuePtr == _T(' ') ) CookieValuePtr++;

	// 値の開始ポインタ
	// CookieValuePtr

	// 値の文字数
	while ( *(CookieValuePtr + CookieValueLen) != _T(';') && *(CookieValuePtr + CookieValueLen) != _T('\0') ) {
		CookieValueLen++;
	}

	AfterCookiePtr = AfterCookie = S_ALLOC(lstrlen(BeforeCookie) + CookieNameLen + CookieValueLen + 2);
	if ( AfterCookie == NULL ) return NULL;

	while ( *BeforeCookie != _T('\0') ) {
		while ( *BeforeCookie == _T(' ') ) BeforeCookie++;
		for ( CookieNameIdx = 0; CookieNameIdx < CookieNameLen; CookieNameIdx++ ) {
			if ( *(BeforeCookie + CookieNameIdx) != *(NewCookie + CookieNameIdx) ) {
				// Set-Cookie と違うもの
				break;
			}
		}
		if ( CookieNameIdx == CookieNameLen && *(BeforeCookie + CookieNameLen) == _T('=')) {
			// Set-Cookie と同じもの
			_tcsncpy_s(AfterCookiePtr, CookieNameLen + 1, NewCookie, CookieNameLen);
			AfterCookiePtr += CookieNameLen;
			*AfterCookiePtr++ = _T('=');
			_tcsncpy_s(AfterCookiePtr, CookieValueLen + 1, CookieValuePtr, CookieValueLen);
			AfterCookiePtr += CookieValueLen;
			*AfterCookiePtr++ = _T(';');
			while ( *BeforeCookie != _T('\0') && *BeforeCookie != _T(';') ) BeforeCookie++;
			Hit = 1;
		} else {
			// Set-Cookie と違うもの
			while ( *BeforeCookie != _T('\0') && *BeforeCookie != _T(';') ) *AfterCookiePtr++ = *BeforeCookie++;
			*AfterCookiePtr++ = _T(';');
		}
		if ( *BeforeCookie == _T(';') ) BeforeCookie++;
	}

	if ( Hit ) {
		// 最後の ; を終端に変える
		*(AfterCookiePtr - 1) = _T('\0');
	} else {
		// 旧Cookieなし
		_tcsncpy_s(AfterCookiePtr, CookieNameLen + 1, NewCookie, CookieNameLen);
		AfterCookiePtr += CookieNameLen;
		*AfterCookiePtr++ = _T('=');
		_tcsncpy_s(AfterCookiePtr, CookieValueLen + 1, CookieValuePtr, CookieValueLen);
	}


	return AfterCookie;
}



// リクエスト
int WinetReq(struct THARGS *ThArgs, struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP) {

	// WWWC
	int CmpMsg = ST_DEFAULT;
	// なんでも
	TCHAR buf[BIGSIZE];
	DWORD dwBuf, dwLen;
	BOOL bFlag;
	// エラーメッセージ
	LPTSTR ErrMsg = NULL;
	// HTTP
	TCHAR Headers[BIGSIZE] = _T("Accept: */*");
	TCHAR user[BUFSIZE];
	TCHAR pass[BUFSIZE];
	TCHAR BaseStr1[BUFSIZE];
	TCHAR BaseStr2[BIGSIZE];
	TCHAR poststring[BIGSIZE];
	LPBYTE BodyTemp;
	// Set-Cookie
	LPTSTR BeforeCookie = NULL, NewCookie, AfterCookie = NULL,
		OptionPtr, AfterOption, AfterOptionPtr;


	// WinInet 開始
	ThArgs->hInet = InternetOpen(
		GetOptionString(tpItemInfo->Option2, buf, OP2_USERAGENT) ? buf : USER_AGENT,
		tpHTTP->ProxyFlag ? INTERNET_OPEN_TYPE_PROXY : INTERNET_OPEN_TYPE_DIRECT,
		tpHTTP->SvName,
		NULL,
		0
	);
	if ( ThArgs->hInet == NULL ) {
		goto WINET_ERR;
	}

	// タイムアウト
	dwBuf = TimeOut * 1000;
	InternetSetOption(ThArgs->hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwBuf, sizeof(dwBuf));
	InternetSetOption(ThArgs->hInet, INTERNET_OPTION_SEND_TIMEOUT, &dwBuf, sizeof(dwBuf));
	InternetSetOption(ThArgs->hInet, INTERNET_OPTION_RECEIVE_TIMEOUT , &dwBuf, sizeof(dwBuf));


	// サーバ接続設定
	ThArgs->hHttpCon = InternetConnect(
		ThArgs->hInet, //
		tpHTTP->hSvName, // ServerName
		tpHTTP->hPort, // Port
		NULL, // UserName
		NULL, // Password
		INTERNET_SERVICE_HTTP, // Service
		0, // Flags
		0 // Context
	);
	if ( ThArgs->hHttpCon == NULL ) {
		goto WINET_ERR;
	}


	// If-Modified-Since を使うか
	if ( (tpItemInfo->user2 == REQUEST_GET || tpItemInfo->user2 == REQUEST_POST)
				&& tpItemInfo->Param3 == 0
				&& tpItemInfo->DLLData1 != NULL
				&& *tpItemInfo->DLLData1 != _T('\0')
	) {
		bFlag = FALSE;
		_tcscat_s(Headers, BIGSIZE, _T("\r\nIf-Modified-Since: "));
		_tcscat_s(Headers, BIGSIZE, tpItemInfo->DLLData1);
	} else {
		bFlag = TRUE;
	}
	

	// リクエストを作る
	ThArgs->hHttpReq = HttpOpenRequest(
		ThArgs->hHttpCon, // hConnect
		(tpItemInfo->user2 == REQUEST_HEAD) ? _T("HEAD") : (tpItemInfo->user2 == REQUEST_POST) ? _T("POST") : _T("GET"), // メソッド
		tpHTTP->Path, //path, // ObjectName
		_T("HTTP/1.1"), // Version
		GetOptionString(tpItemInfo->Option2, buf, OP2_REFERRER) ? buf : NULL, // Referer
		NULL, // AcceptTypes
		0
			| ( bFlag ? INTERNET_FLAG_RELOAD : 0 ) // 常にアクセス
			| INTERNET_FLAG_EXISTING_CONNECT // 接続を再利用
			| ( tpHTTP->SecureFlag ? INTERNET_FLAG_SECURE : 0 ) // HTTPS
			| ( GetOptionInt(tpItemInfo->Option2, OP2_COOKIEFLAG) & 1 ? 0 : INTERNET_FLAG_NO_COOKIES ) // Cookieを自動で使用するか
			| ( GetOptionNum(tpItemInfo->ITEM_FILTER, REQ_REDIRECT) ? 0 : INTERNET_FLAG_NO_AUTO_REDIRECT) // リダイレクトするか
		, // Flags
		0 // Context
	);
	if ( ThArgs->hHttpReq == NULL ) {
		goto WINET_ERR;
	}


	// 証明書エラーを無視
	if ( tpHTTP->SecureFlag && GetOptionInt(tpItemInfo->Option2, OP2_IGNORECERTERROR) ) {
		dwBuf = 
			SECURITY_FLAG_IGNORE_REVOCATION
			| SECURITY_FLAG_IGNORE_UNKNOWN_CA
			| SECURITY_FLAG_IGNORE_CERT_CN_INVALID
			| SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
		dwLen = sizeof(dwBuf);
		InternetSetOption(
			ThArgs->hHttpReq,
			INTERNET_OPTION_SECURITY_FLAGS,
			&dwBuf,
			dwLen
		);
	}


	// ヘッダを作る

	//キャッシュ制御
	if(Proxy == 1 && pNoCache == 1){
		_tcscat_s(Headers, BIGSIZE, _T("\r\nPragma: no-cache\r\nCache-Control: no-cache"));
	}


	//プロキシ認証
	if(tpHTTP->ProxyFlag == 1 && pUsePass == 1){
		wsprintf(BaseStr1, _T("%s:%s"), pUser, pPass);
		eBase(BaseStr1, BaseStr2);
		_tcscat_s(Headers, BIGSIZE, _T("\r\nProxy-Authorization: Basic "));
		_tcscat_s(Headers, BIGSIZE, BaseStr2);
	}


	// ページ認証
	dwBuf = GetOptionInt(tpItemInfo->Option2, OP2_USEPASS);
	if( dwBuf & 1 ){
		if(GetOptionString(tpItemInfo->Option2, user, OP2_USER) == FALSE){
			*user = _T('\0');
		}
		if(GetOptionString(tpItemInfo->Option2, BaseStr2, OP2_PASS) == FALSE){
			*pass = _T('\0');
		}else{
			dPass(BaseStr2, pass);
		}

		switch ( dwBuf ) {
			case AUTH_BASIC_ON: // Basic
				wsprintf(BaseStr1, _T("%s:%s"), user, pass);
				eBase(BaseStr1, BaseStr2);
				if(*BaseStr2 != _T('\0')){
					_tcscat_s(Headers, BIGSIZE, _T("\r\nAuthorization: Basic "));
					_tcscat_s(Headers, BIGSIZE, BaseStr2);
				}
				break;

			case AUTH_WSSE_ON: // WSSE
				if( MakeWsseValue(user, pass, BaseStr2, _countof(BaseStr2)) ) {
					_tcscat_s(Headers, BIGSIZE, _T("\r\nX-WSSE: "));
					_tcscat_s(Headers, BIGSIZE, BaseStr2);
				}
				break;

			case AUTH_OAUTH_ON: // OAuth
				if ( MakeOauthValue(tpItemInfo, user, pass, BaseStr2, _countof(BaseStr2)) ) {
					_tcscat_s(Headers, BIGSIZE, _T("\r\nAuthorization: "));
					_tcscat_s(Headers, BIGSIZE, BaseStr2);
				} else {
					ErrMsg = _T("OAuth失敗");
					goto REQ_ERR;
				}
				break;
		}
	}


	// クッキー
	if(GetOptionString(tpItemInfo->Option2, buf, OP2_COOKIE) == TRUE) {
		_tcscat_s(Headers, BIGSIZE, _T("\r\nCookie: "));
		_tcscat_s(Headers, BIGSIZE, buf);
	}


	// POSTデータ
	if( GetOptionInt(tpItemInfo->Option1, OP1_REQTYPE) == REQUEST_POST ) {
		if( GetOptionString(tpItemInfo->Option1, poststring, OP1_POST) == FALSE ) {
			*poststring = _T('\0');
		}
		_tcscat_s(Headers, BIGSIZE, _T("\r\nContent-Type: application/x-www-form-urlencoded"));
		dwLen = lstrlen(poststring);
	} else {
		dwLen = 0;
	}


	// リクエストを送る
	bFlag = HttpSendRequest(
		ThArgs->hHttpReq, // hRequest
		Headers, // Headers
		-1, // HeadersLength
		dwLen > 0 ? poststring : NULL,
		dwLen
	);
	if ( bFlag == FALSE ) {
		goto WINET_ERR;
	}


	// ステータスコード
	dwLen = sizeof(tpHTTP->StatusCode);
	HttpQueryInfo(
		ThArgs->hHttpReq,
		HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
		&tpHTTP->StatusCode,
		&dwLen,
		NULL
	);

	// Content-Length
	dwLen = sizeof(tpHTTP->ContentLength);
	tpHTTP->ContentLength = -1;
	HttpQueryInfo(
		ThArgs->hHttpReq,
		HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER,
		&tpHTTP->ContentLength,
		&dwLen,
		NULL
	);

	// Set-Cookie
	dwBuf = GetOptionInt(tpItemInfo->Option2, OP2_COOKIEFLAG);
	// AutoCookie == 0 && SetCookie
	if ( (dwBuf & 1) == 0 && dwBuf & 2 ) {
		// 複数Set-Cookieの繰り返し
		dwLen = 0;
		dwBuf = 0;
		while ( 1 ) {
			// サイズ取得
			HttpQueryInfo(
				ThArgs->hHttpReq,
				HTTP_QUERY_SET_COOKIE,
				NULL,
				&dwLen,
				&dwBuf
			);
			if ( GetLastError() == ERROR_HTTP_HEADER_NOT_FOUND ) break;
			NewCookie = M_ALLOC(dwLen);
			if ( NewCookie == NULL ) {
				M_FREE(AfterCookie);
				goto MEM_ERR;
			}

			// 本取得
			HttpQueryInfo(
				ThArgs->hHttpReq,
				HTTP_QUERY_SET_COOKIE,
				NewCookie,
				&dwLen,
				&dwBuf
			);

			// 突き合わせ
			if ( BeforeCookie == NULL ) {
				BeforeCookie = M_ALLOC(M_SIZE(tpItemInfo->Option2));
				if ( BeforeCookie == NULL ) {
					M_FREE(NewCookie);
					goto MEM_ERR;
				}
				GetOptionString(tpItemInfo->Option2, BeforeCookie, OP2_COOKIE);
			} else {
				BeforeCookie = AfterCookie;
			}
			AfterCookie = SetCookie(BeforeCookie, NewCookie);
			M_FREE(BeforeCookie);
			M_FREE(NewCookie);
			if ( AfterCookie == NULL ) {
				ErrMsg = _T("SetCookie失敗");
				goto REQ_ERR;
			}
		}

		if ( AfterCookie != NULL ) {
			// 新Cookieを保存
			OptionPtr = tpItemInfo->Option2;
			AfterOptionPtr = AfterOption = S_ALLOC(M_SIZE(OptionPtr) + M_SIZE(AfterCookie));
			if ( AfterOption == NULL ) {
				M_FREE(AfterCookie);
				goto MEM_ERR;
			}

			// Cookie設定の前をコピー
			for ( dwBuf = 0; dwBuf < OP2_COOKIE; dwBuf ++ ) {
				while ( *OptionPtr != _T(';') || *(OptionPtr + 1) != _T(';') ) *AfterOptionPtr++ = *OptionPtr++;
				*AfterOptionPtr++ = _T(';');
				*AfterOptionPtr++ = _T(';');
				OptionPtr += 2;
			}

			// 新Cookieをコピー
			lstrcpy(AfterOptionPtr, AfterCookie);
			AfterOptionPtr += lstrlen(AfterCookie);
			*AfterOptionPtr++ = _T(';');
			*AfterOptionPtr++ = _T(';');
			M_FREE(AfterCookie);

			// Cookie設定の後をコピー
			while ( *OptionPtr != _T(';') || *(OptionPtr + 1) != _T(';') ) OptionPtr++;
			OptionPtr += 2;
			lstrcpy(AfterOptionPtr, OptionPtr);

			// 保存
			M_FREE(tpItemInfo->Option2);
			tpItemInfo->Option2 = AfterOption;
		}
	}

	// ヘッダチェックは内容取得しない
	if( CheckHead(tpItemInfo, tpHTTP) == TRUE ){
		goto REQ_END;
	}

	// 内容
	tpHTTP->Size = 0;
	tpHTTP->Body = NULL;
	if ( tpHTTP->ContentLength == 0 ) {
		tpHTTP->Body = M_ALLOC(2);
		if (tpHTTP->Body == NULL ) {
			goto MEM_ERR;
		}

	} else if ( tpHTTP->ContentLength != -1 ) {
		tpHTTP->Body = M_ALLOC(tpHTTP->ContentLength + 2);
		if (tpHTTP->Body == NULL ) {
			goto MEM_ERR;
		}
		bFlag = InternetReadFile(ThArgs->hHttpReq, tpHTTP->Body, tpHTTP->ContentLength, &dwLen);
		if ( bFlag == FALSE ) {
			goto WINET_ERR;
		}
		tpHTTP->Size = tpHTTP->ContentLength;

	} else {
		do {
			// 既存分と追加分を併せたメモリ
			BodyTemp = M_ALLOC(tpHTTP->Size + RECVSIZE);
			if ( BodyTemp == NULL ) {
				goto MEM_ERR;
			}
			// 既存を複製
			if ( tpHTTP->Size > 0 ) {
				memcpy(BodyTemp, tpHTTP->Body, tpHTTP->Size);
			}

			// 取得
			bFlag = InternetReadFile(ThArgs->hHttpReq, BodyTemp + tpHTTP->Size, RECVSIZE - 2, &dwLen);
			if ( bFlag == FALSE ) {
				M_FREE(BodyTemp);
				goto WINET_ERR;
			}

			// 旧メモリ解放
			M_FREE(tpHTTP->Body);
			// 新メモリを記録
			tpHTTP->Body = BodyTemp;
			// サイズ計
			tpHTTP->Size += dwLen;

			//受信バイト数をステータスバーに出力
			wsprintf(buf, STR_STATUS_RECV, tpHTTP->Size);
			SetSBText(ThArgs->hWnd, tpItemInfo, buf);
		} while ( dwLen != 0 );
	}

	// 最後にNULL
	*(tpHTTP->Body + tpHTTP->Size) = '\0';
	*(tpHTTP->Body + tpHTTP->Size + 1) = '\0';


	// リクエストおしまい
	REQ_END :
	return CmpMsg;


	// エラー
	WINET_ERR : 
	if ( tpItemInfo->ITEM_CANCEL ) {
		goto REQ_END; // ↑
	}
	dwBuf = WinetGetErrMsg(buf, sizeof(buf));
	if ( dwBuf == ERROR_INTERNET_TIMEOUT ) {
		CmpMsg = ST_TIMEOUT;
		SetErrorString(tpItemInfo, buf, FALSE);
		goto REQ_END; // ↑
	}
	ErrMsg = buf;
	goto REQ_ERR; // ↓↓

	MEM_ERR :
	ErrMsg = STR_MEM_ERR;
	// ↓

	REQ_ERR :
	CmpMsg = ST_ERROR;
	SetErrorString(tpItemInfo, ErrMsg, FALSE);
	goto REQ_END; // ↑↑↑
}



// URLを実行
BOOL ExecOptionCommand(struct TPITEM* tpItemInfo, LPTSTR Url) {
	LPTSTR Command, Param;
	TCHAR EndChar;
	int CommandLen = 0;


	// メモリ
	Command = S_ALLOC(BUFSIZE);
	if ( Command == NULL ) return FALSE;
	Param = S_ALLOC(BUFSIZE + lstrlen(Url));
	if ( Param == NULL ) {
		M_FREE(Command);
		return FALSE;
	}

	// 設定を得る
	GetOptionString(tpItemInfo->Option2, Command, OP2_COMMAND);

	// ファイル名と引数を分ける
	// ファイル名の終わり文字
	if ( *Command == _T('"') ) {
		EndChar = _T('"');
		CommandLen = 1;
	} else {
		EndChar = _T(' ');
	}
	// ファイル名の終わりを探す
	for ( ; *(Command + CommandLen) != _T('\0') && *(Command + CommandLen) != EndChar; CommandLen++ );
	if ( *(Command + CommandLen) == _T('"') ) {
		CommandLen++;
	}
	// 引数を分ける
	if ( *(Command + CommandLen) != _T('\0') ) {
		lstrcpy(Param, Command + CommandLen + 1);
		*(Command + CommandLen) = _T('\0');
	} else {
		*Param = _T('\0');
	}

	// URLを引数に
	lstrcat(Param, Url);

	// 実行
	ShellExecute(NULL, NULL, Command, Param, NULL, SW_SHOW);
	M_FREE(Command);
	M_FREE(Param);


	return TRUE;
}



// WinInet のエラーメッセージを取得
int WinetGetErrMsg(OUT LPTSTR Buf, IN int BufSize) {
	int ErrNum = 0, Ret, i;

	if ( Buf == NULL || BufSize < 1 ) return 0;

	ErrNum = GetLastError();
	*Buf = _T('\0');
	Ret = FormatMessage(
		FORMAT_MESSAGE_IGNORE_INSERTS
			| ( ErrNum >= 12000 && ErrNum < 13000
				? FORMAT_MESSAGE_FROM_HMODULE : FORMAT_MESSAGE_FROM_SYSTEM )
		, // 入力元と処理方法のオプション
		GetModuleHandle(_T("wininet.dll")), // メッセージの入力元
		ErrNum, // メッセージ識別子
		0, // 言語識別子
		Buf, // メッセージバッファ
		BufSize, // メッセージバッファの最大サイズ (TCHAR)
		NULL // 複数のメッセージ挿入シーケンスからなる配列
	);

	for ( i = 0; i < BufSize && *(Buf + i) != _T('\0') && *(Buf + i) != _T('\r') && *(Buf + i) != _T('\n'); i++ );
	_stprintf_s(Buf + i, BufSize - i, _T(" (%d)"), ErrNum);


	return ErrNum;
}




/* End of source */
