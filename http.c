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

#define ESC					0x1B		/* �G�X�P�[�v�R�[�h */

#define TH_EXIT				(WM_USER + 1200)


#define HTTP_MENU_CNT		8



/**************************************************************************
	Global Variables
**************************************************************************/

static int srcLen;

HWND hWndList[100];


//�O���Q��
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

	���b�`�G�f�B�b�g�ւ̕�����̐ݒ�

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

	�A�C�e���̃w�b�_�[�E�\�[�X�\���_�C�A���O

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
			// �w�b�_�\��
			// ���������m��
			HttpQueryInfo(
				((struct THARGS*)tpItemInfo->ITEM_MARK)->hHttpReq,
				HTTP_QUERY_RAW_HEADERS_CRLF,
				NULL,
				&HeadersLen, // �Ԃ�̓������T�C�Y
				NULL
			);
			Headers = M_ALLOC(HeadersLen);
			// �{�擾
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
			// �\�[�X�\��
			SetCursor(LoadCursor(NULL, IDC_WAIT));

			filter_title = "";
			if(tpHTTP->FilterFlag == TRUE){
				//Shift��������Ă���ꍇ�̓t�B���^������

				if ( GetOptionInt(tpItemInfo->Option1, OP1_META) == 2 ) {
					// �t�B�[�h
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
							filter_title = _T(" (�\�[�X�u��)");
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

	HTML�̃^�C�g�����擾

******************************************************************************/

static char *GetTitle(char *buf, int BufSize)
{
#define TITLE_TAG	"title"
	char *SjisBuf, *ret;
	int len;
	BOOL rc;

	//JIS �� SJIS �ɕϊ�
	SjisBuf = SrcConv(buf, BufSize + 2);

	//�^�C�g���^�O�̃T�C�Y���擾
	len = GetTagContentSize(SjisBuf, TITLE_TAG);
	if(len == 0){
		return NULL;
	}

	//�^�C�g�����擾
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
	//���ꕶ���̕ϊ�
	ConvHTMLChar(ret);

	return ret;
}

/******************************************************************************

	GetRssItem

	RSS-Item���擾

******************************************************************************/

static char *GetRssItem(char *buf)
{
	char *content;
	char *p, *tmp;
	char chtime[BUFSIZE];
	int len;
	int i;

	// RSS����
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

	//�^�C�g���̎擾
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

	//�X�V���̎擾
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

	�G���[������̐ݒ�

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



// yyyy/MM/dd hh:mm ��16���������r�A�V : -1�A�� : 1�A�� : 0
// �t�B�[�h�Œl������ꂽ���̂݌Ă΂��O��
int DateCompare(LPTSTR OldDate, LPTSTR NewDate) {
	// �������� NO_META �Ȃ�X�V�Ƃ���
	if ( lstrcmp(OldDate, _T("no meta")) == 0 ) {
		return -1;
	}
	return lstrcmpn(OldDate, NewDate, 16);
}


/******************************************************************************

	GetMetaString

	META�^�O�̎擾

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
	//SJIS�ɕϊ�
	p = SrcConv(tmp, tpHTTP->Size);
	GlobalFree(tmp);

	// �t�B�[�h
	if( (MetaContent = GetRssItem(p)) != NULL ) {
		//���ꕶ���̕ϊ�
		ConvHTMLChar(MetaContent);

		CompRet = DateCompare(tpItemInfo->Date, MetaContent);
		if ( CompRet == -1 ) {
			CmpMsg = ST_UP;
			// �X�V���
			M_FREE(tpItemInfo->ITEM_UPINFO);
			tpItemInfo->ITEM_UPINFO = MakeUpInfo(MetaContent + 17, lstrlen(MetaContent) - 17, tpItemInfo, 1);
			// �V�����̍X�V�}�[�N��t����
			lstrcat(MetaContent, _T("*"));
		}
	// meta�^�O	
	} else if ( (MetaContent = GetMETATag(p, type, name, content)) != NULL ) {
		//���ꕶ���̕ϊ�
		ConvHTMLChar(MetaContent);

		if(LresultCmp(tpItemInfo->Date, MetaContent) != 0){
			CmpMsg = ST_UP;
			// �X�V���
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
		// ���������^����ꂽ��ߋ��ɖ߂�Ȃ��Ȃ�̂Ŗ��X�V�ł������͕ς��Ă���
			M_FREE(tpItemInfo->Date);
			tpItemInfo->Date = MetaContent;
		}

	}else{
		//META�^�O���擾�ł��Ȃ��������b�Z�[�W��ݒ�
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


// ������������� LastModified ��Ԃ�
// TRUE  : �w�b�_�������ă������������
// FALSE : �w�b�_���Ȃ��ă����������Ȃ�����
BOOL GetLastModified(struct TPITEM *tpItemInfo, OUT LPTSTR *LastModified) {
	int LastModifiedSize = 0;

	// 1�񎎂�
	HttpQueryInfo(
		((struct THARGS*)tpItemInfo->ITEM_MARK)->hHttpReq,
		HTTP_QUERY_LAST_MODIFIED,
		NULL,
		&LastModifiedSize,
		NULL
	);

	// �w�b�_���Ȃ����ۂ�
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

	// ��������������̂�2��ڂ̌��ʂɂ�炸 TRUE
	return TRUE;
}



/******************************************************************************

	Head_GetDate

	�w�b�_����X�V�������擾

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
			//DLL�p�f�[�^�ɓ��t���i�[
			if(tpItemInfo->DLLData1 != NULL){
				GlobalFree(tpItemInfo->DLLData1);
			}
			tpItemInfo->DLLData1 = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(headcontent) + 1);
			if(tpItemInfo->DLLData1 != NULL) lstrcpy(tpItemInfo->DLLData1, headcontent);
		}

		if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->Date == NULL || *tpItemInfo->Date == '\0'){
			//���t�`���̕ϊ�
			if(DateConv(headcontent, chtime) == 0){
				lstrcpy(headcontent, chtime);
			}
			/* �A�C�e���ɍ���̃`�F�b�N���e���Z�b�g���� */
			if(tpItemInfo->Date != NULL){
				GlobalFree(tpItemInfo->Date);
			}
			tpItemInfo->Date = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(headcontent) + ((CmpMsg == ST_UP) ? 2 : 1));
			if(tpItemInfo->Date != NULL){
				lstrcpy(tpItemInfo->Date, headcontent);
				if(CmpMsg == ST_UP){
					//���t�̕ύX�������L���̕t��
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

	�w�b�_����T�C�Y���擾

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

	// HEAD�`�F�b�N�\�t���O
	*SizeRet = TRUE;

	// �T�C�Y�Ƃ�
	for( ContentLengthSize = 2, i = tpHTTP->ContentLength; i /= 10; ContentLengthSize++ );
	headcontent = S_ALLOC(ContentLengthSize);
	_itot_s(tpHTTP->ContentLength, headcontent, ContentLengthSize, 10);

	// �O��`�F�b�N���̂��̂Ɣ�r����
	if ( CmpOption == 1 && tpItemInfo->Size != NULL && *tpItemInfo->Size != _T('\0') ) {
		// �������r�p�ɍX�V�}�[�N������
		OldSizeLen = lstrlen(tpItemInfo->Size);
		if ( *(tpItemInfo->Size + OldSizeLen - 1) == _T('*') ) {
			*(tpItemInfo->Size + OldSizeLen - 1) = _T('\0');
			i = -1;
		}
		// ��r
		if ( lstrcmp(tpItemInfo->Size, headcontent) != 0 ) {
			CmpMsg = ST_UP;
			*(headcontent + ContentLengthSize - 1) = _T('*');
			*(headcontent + ContentLengthSize) = _T('\0');

			if(SetDate == 0 && CreateDateTime(buf) == 0){
				// �A�C�e���Ɍ��݂̎�����ݒ肷��
				M_FREE(tpItemInfo->Date);
				tpItemInfo->Date = S_ALLOC(lstrlen(buf));
				if(tpItemInfo->Date != NULL) lstrcpy(tpItemInfo->Date, buf);
			}
		} else if ( i == -1 ) {
			*(tpItemInfo->Size + OldSizeLen - 1) = _T('*');
		}
	}

	if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->Size == NULL || *tpItemInfo->Size == _T('\0') ){
		// �A�C�e���ɍ���̃`�F�b�N���e���Z�b�g����
		M_FREE(tpItemInfo->Size);
		tpItemInfo->Size = headcontent;
	} else {
		M_FREE(headcontent);
	}


	return CmpMsg;
}


/******************************************************************************

	Get_GetSize

	HTML�̃T�C�Y���擾

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
		/* ��M�������̂���w�b�_�ƃ^�O�����������T�C�Y���v�Z���� */
		p = SrcConv(tpHTTP->Body, tpHTTP->Size);
		tpHTTP->Size = DelTagSize(p);
		wsprintf(headcontent, "%ld", tpHTTP->Size);
		GlobalFree(p);
	}else if(tpHTTP->FilterFlag == TRUE){
		tpHTTP->Size = lstrlen(tpHTTP->Body);
		wsprintf(headcontent, "%ld", tpHTTP->Size);
	}else{
		/* ��M�������̂���w�b�_�����������T�C�Y���v�Z���� */
		wsprintf(headcontent, "%ld", tpHTTP->Size);
	}
	ContentLengthSize = lstrlen(headcontent) + 1;

	// �O��`�F�b�N���̂��̂Ɣ�r����
	if ( CmpOption == 1 && tpItemInfo->Size != NULL && *tpItemInfo->Size != _T('\0') ) {
		// �������r�p�ɍX�V�}�[�N������
		OldSizeLen = lstrlen(tpItemInfo->Size);
		if ( *(tpItemInfo->Size + OldSizeLen - 1) == _T('*') ) {
			*(tpItemInfo->Size + OldSizeLen - 1) = _T('\0');
			i = -1;
		}
		// ��r
		if ( lstrcmp(tpItemInfo->Size, headcontent) != 0 ) {
			CmpMsg = ST_UP;
			*(headcontent + ContentLengthSize - 1) = _T('*');
			*(headcontent + ContentLengthSize) = _T('\0');

			if(SetDate == 0 && CreateDateTime(buf) == 0){
				// �A�C�e���Ɍ��݂̎�����ݒ肷��
				M_FREE(tpItemInfo->Date);
				tpItemInfo->Date = S_ALLOC(lstrlen(buf));
				if(tpItemInfo->Date != NULL) lstrcpy(tpItemInfo->Date, buf);
			}
		} else if ( i == -1 ) {
			*(tpItemInfo->Size + OldSizeLen - 1) = _T('*');
		}
	}

	if(CmpMsg == ST_UP || CmpOption == 0 || tpItemInfo->Size == NULL || *tpItemInfo->Size == '\0'){
		/* �A�C�e���ɍ���̃`�F�b�N���e���Z�b�g���� */
		M_FREE(tpItemInfo->Size);
		tpItemInfo->Size = S_ALLOC(ContentLengthSize);
		if(tpItemInfo->Size != NULL) lstrcpy(tpItemInfo->Size, headcontent);
	}
	return CmpMsg;
}


/******************************************************************************

	Get_MD5Check

	MD5�̃_�C�W�F�X�g�l���擾���ă`�F�b�N

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

	//�_�C�W�F�X�g�l�̐���
    MD5Init(&context);
    MD5Update(&context, tpHTTP->Body, lstrlen(tpHTTP->Body));
    MD5Final(digest, &context);

	for(i = 0, len = 0; i < 16; i++, len += 2){
		wsprintf(tmp + len, "%02x", digest[i]);
	}
	*(tmp + len) = '\0';

	/* �O��`�F�b�N���̂��̂Ɣ�r���� */
	if(LresultCmp(tpItemInfo->DLLData2, tmp) != 0){
		CmpMsg = ST_UP;

		if(SetDate == 0 && CreateDateTime(buf) == 0){
			/* �A�C�e���Ɍ��݂̎�����ݒ肷�� */
			if(tpItemInfo->Date != NULL){
				GlobalFree(tpItemInfo->Date);
			}
			tpItemInfo->Date = (char *)GlobalAlloc(GMEM_FIXED, sizeof(char) * lstrlen(buf) + 1);
			if(tpItemInfo->Date != NULL) lstrcpy(tpItemInfo->Date, buf);
		}
	}
	if(CmpMsg == ST_UP || tpItemInfo->DLLData2 == NULL || *tpItemInfo->DLLData2 == '\0'){
		/* �A�C�e���ɍ���̃`�F�b�N���e���Z�b�g���� */
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

	�w�b�_����X�V�����擾���ď������s��

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


	//�c�[��
	switch(tpItemInfo->Param3){
	case 1:
	case 2:
		//�w�b�_�A�\�[�X�\��
		//�`�F�b�N�̏I����WWWC�ɒʒm
		vWnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_VIEW), NULL, ViewProc, (long)tpItemInfo);
		ShowWindow(vWnd, SW_SHOW);
		SetFocus(GetDlgItem(vWnd, IDC_EDIT_VIEW));
		return ST_DEFAULT;

	case 3:
		//�^�C�g���擾
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
		//�X�V�����������ꍇ
		return ST_DEFAULT;
	}

	//�`�F�b�N�ݒ�
	DateCheck = !GetOptionInt(tpItemInfo->Option1, OP1_NODATE);
	SizeCheck = !GetOptionInt(tpItemInfo->Option1, OP1_NOSIZE);


	switch(tpHTTP->StatusCode)
	{
	case 100:
	case 200:	/* �����̏ꍇ */
		if(tpItemInfo->user2 == REQUEST_HEAD){
			//HEAD
			//�X�V�������擾
			CmpMsg |= Head_GetDate(tpItemInfo, DateCheck, &DateRet);
			//�T�C�Y���擾
			CmpMsg |= Head_GetSize(tpItemInfo, SizeCheck, DateRet, &SizeRet);
			if(SizeRet == 0 && DateRet == 0){
				//GET���N�G�X�g���M�̂��߂̃`�F�b�N�҂����
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
			case 1 : //META�^�O���擾
				CmpMsg |= GetMetaString(tpItemInfo);
				SetDate = 1;
				M_FREE(tpItemInfo->DLLData1);
				GetLastModified(tpItemInfo, &tpItemInfo->DLLData1);
				break;

			case 2 : // �t�B�[�h
				CmpMsg = GetEntries(tpItemInfo);
				if ( CmpMsg != ST_ERROR ) {
					M_FREE(tpItemInfo->DLLData1);
					GetLastModified(tpItemInfo, &tpItemInfo->DLLData1);
				}
				return CmpMsg;

			case 3 : // �����N���o
				CmpMsg = GetLinks(tpItemInfo);
				return CmpMsg;

			default : //�w�b�_����X�V�������擾
				CmpMsg |= Head_GetDate(tpItemInfo, DateCheck, &SetDate);
				break;
		}
		//�w�b�_����T�C�Y���擾
		if( tpHTTP->ContentLength != -1
					&& GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE) == 0
					&& tpHTTP->FilterFlag == FALSE
				){
			CmpMsg |= Head_GetSize(tpItemInfo, SizeCheck, SetDate, &SizeRet);
			// ������ GET �Ȃ� HEAD �ɕύX
			if( tpItemInfo->Option1 != NULL
						&& *tpItemInfo->Option1 == _T('1')
						&& GetOptionInt(tpItemInfo->Option1, OP1_META) == 0
						&& GetOptionInt(tpItemInfo->Option1, OP1_MD5) == 0
					) {
				*tpItemInfo->Option1 = _T('0');
			}
		}
		if(SizeRet == 0){
			//�t�B���^����
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
			//HTML�̃T�C�Y���擾
			CmpMsg |= Get_GetSize(tpItemInfo, SizeCheck, SetDate);
		}
		if(GetOptionInt(tpItemInfo->Option1, OP1_MD5) == 1){
			//MD5�̃_�C�W�F�X�g�l�Ń`�F�b�N
			CmpMsg |= Get_MD5Check(tpItemInfo, SetDate);
		}
		break;

	default:	/* �G���[�̏ꍇ */
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

		// ���񂪃G���[�Ȃ琳�펞�ɍX�V�ƂȂ�悤��
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

	WWWC�̃X�e�[�^�X�o�[�Ƀe�L�X�g��\������

******************************************************************************/

static void SetSBText(HWND hWnd, struct TPITEM *tpItemInfo, char *msg)
{
	char *buf, *p;

	//�X�e�[�^�X�o�[�Ƀe�L�X�g�𑗂�B
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

	��M�ς݃f�[�^�ɕK�v�ȏ����擾���Ă��邩�`�F�b�N

******************************************************************************/

static BOOL CheckHead(struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP)
{
//	char *p;

	// ��Ύ擾���Ȃ�
	if ( tpItemInfo->user2 == REQUEST_HEAD || tpHTTP->StatusCode == 304 || tpItemInfo->Param3 == 1) {
		return TRUE;
	}


	//���ׂĎ�M����K�v������ꍇ
	if( tpItemInfo->Param3 == 2 ){
		// �\�[�X�\��
		return FALSE;
	}

	// �������͎擾����
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

	// ���ʂ͎擾���Ȃ�
	if ( tpHTTP->StatusCode != 200 ) {
		return TRUE;
	}

	//�w�b�_�ɃT�C�Y���ݒ肳��Ă��邩�`�F�b�N
	if(tpItemInfo->Param3 == 0 && tpHTTP->ContentLength == -1 ){
		return FALSE;
	}

	return TRUE;
}



/******************************************************************************

	HTTP_FreeItem

	�A�C�e�����̉��

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

	�A�C�e���̏�����

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_InitItem(HWND hWnd, struct TPITEM *tpItemInfo)
{

	if(tpItemInfo->ErrStatus != NULL && GetOptionInt(tpItemInfo->Option1, OP1_META) == 0 ){
		GlobalFree(tpItemInfo->ErrStatus);
		tpItemInfo->ErrStatus = NULL;
	}

	if((tpItemInfo->Status & ST_UP) == 0){
		/* �X�V�}�[�N(*)���������� */
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

	�`�F�b�N�J�n�̏�����

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

	�`�F�b�N�̃L�����Z��

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
		//�w�b�_�A�\�[�X�\��
		vWnd = CreateDialogParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_VIEW), NULL, ViewProc, (long)tpItemInfo);
		ShowWindow(vWnd, SW_SHOW);
		break;

	case 3:
		//�^�C�g���擾
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

	// �������ƃn���h�����������
	// �L�����Z������ _Select ���Ă΂�Ȃ�
	// �^�C�~���O�͑��v�Ȃ̂��H
	WinetClear(tpItemInfo);


	return 0;
}



/******************************************************************************

	FreeURLData

	URL���̉��

******************************************************************************/

static void FreeURLData(struct TPHTTP *tpHTTP)
{
	if(tpHTTP->SvName != NULL) GlobalFree((HGLOBAL)tpHTTP->SvName);
	if(tpHTTP->Path != NULL) GlobalFree((HGLOBAL)tpHTTP->Path);
	if(tpHTTP->hSvName != NULL) GlobalFree((HGLOBAL)tpHTTP->hSvName);
}


/******************************************************************************

	GetServerPort

	�ڑ�����T�[�o�ƃ|�[�g���擾����

******************************************************************************/

BOOL GetServerPort(HWND hWnd, struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP)
{
	char ItemProxy[BUFSIZE];
	char ItemPort[BUFSIZE];
	char *px;
	char *pt;
	int ProxyFlag;

/* http>
	�v���O�C���ݒ�R���̃O���[�o���ϐ� Proxy ��
	1 : Proxy�g��
	0 : Proxy�g��Ȃ�

	�A�C�e���ݒ�R���̂����� ProxyFlag ��
	2 : �A�C�e����Proxy�ݒ���g��
	1 : Proxy����
	0 : Proxy�����ɂ��Ă��Ȃ�

	tpHTTP->ProxyFlag �͍ŏI���ʂƂ���
	TRUE  : Proxy�g��
	FALSE : Proxy�g��Ȃ�
*/

	ProxyFlag = GetOptionInt(tpItemInfo->Option2, OP2_NOPROXY);
	if(ProxyFlag == 0 && GetOptionInt(tpItemInfo->Option2, OP2_SETPROXY) == 1){
		//�A�C�e���ɐݒ肳�ꂽ�v���L�V���g�p
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
		//Proxy�̃T�[�o���ƃ|�[�g�ԍ����擾
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

	/* URL����T�[�o���ƃp�X���擾���� */
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

	�`�F�b�N�̊J�n

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


	// �A�C�e���쐬����Ȃ�A�t�B�[�h���ۂ�URL�����Đݒ�
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
	
	// feed �X�L�[��
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

	// ���T�C�Y������
	if ( tpItemInfo->ITEM_SITEURL != NULL
				&& *tpItemInfo->ITEM_SITEURL >= _T('0')
				&& *tpItemInfo->ITEM_SITEURL <= _T('9')
	) {
		*tpItemInfo->ITEM_SITEURL = _T('\0');
	}


	//�`�F�b�N�ȊO�̏ꍇ�ɕ\������URL�̏����g�p����
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


	//���N�G�X�g���\�b�h�̑I��
	switch(tpItemInfo->Param3)
	{
	case 0:		//�`�F�b�N
	case 1:		//�w�b�_�\��
		tpItemInfo->user2 = GetOptionInt(tpItemInfo->Option1, OP1_REQTYPE);
		// �v���g�R���ݒ肩�A�C�e���ݒ�ŏ��GET
		if(CheckType == 1 || tpItemInfo->user2 == 2	) {
			tpItemInfo->user2 = REQUEST_GET;
		}
		// �������{�����K�v
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
			//�t�B���^
			tpHTTP->FilterFlag = FilterMatch(tpItemInfo->CheckURL);
		}
		break;

	case 2:		//�\�[�X�\��
	case 3:		//�^�C�g���擾
		tpItemInfo->user2 = (GetOptionInt(tpItemInfo->Option1, OP1_REQTYPE) == REQUEST_POST) ? REQUEST_POST : REQUEST_GET;
		tpHTTP->FilterFlag = (GetAsyncKeyState(VK_SHIFT) < 0) ? TRUE : FALSE;
		break;
	}

	//�ڑ��T�[�o�ƃ|�[�g�ԍ����擾
	if(GetServerPort(hWnd, tpItemInfo, tpHTTP) == FALSE){
		WinetCheckEnd(hWnd, tpItemInfo, ST_ERROR);
		return CHECK_ERROR;
	}


	// �ʃX���ւ̈���
	ThArgs = M_ALLOC_Z(sizeof(struct THARGS));
	if ( ThArgs == NULL ) {
		WinetCheckEnd(hWnd, tpItemInfo, ST_ERROR);
		return CHECK_ERROR;
	}
	// �l
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
		SetErrorString(tpItemInfo, _T("���N�G�X�g�X���b�h�쐬���s"), FALSE);
		WinetCheckEnd(hWnd, tpItemInfo, 1);
		return CHECK_ERROR;
	}
	tpItemInfo->ITEM_MARK = (ITEM_MARK_TYPE)ThArgs;
	ResumeThread(ThArgs->hThread);

	return CHECK_SUCCEED;
}



/******************************************************************************

	HTTP_Select

	�\�P�b�g��select

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

	�A�C�e���̃`�F�b�N�I���ʒm

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_ItemCheckEnd(HWND hWnd, struct TPITEM *tpItemInfo)
{
	return 0;
}


/******************************************************************************

	HTTP_GetItemText

	�N���b�v�{�[�h�p�A�C�e���̃e�L�X�g�̐ݒ�

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

	�C���^�[�l�b�g�V���[�g�J�b�g�̍쐬

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

	/* �t�@�C�����J�� */
	hFile = CreateFile(path, GENERIC_READ | GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if(hFile == NULL || hFile == (HANDLE)-1){
		LocalFree(buf);
		return FALSE;
	}

	/* �t�@�C���ɏ������� */
	if(WriteFile(hFile, buf, len, &ret, NULL) == FALSE){
		LocalFree(buf);
		CloseHandle(hFile);
		return FALSE;
	}

	/* �������̉�� */
	LocalFree(buf);
	/* �t�@�C������� */
	CloseHandle(hFile);
	return TRUE;
}


/******************************************************************************

	HTTP_CreateDropItem

	�A�C�e���̃h���b�v�t�@�C���̐ݒ�

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

	�h���b�v���ꂽ�t�@�C���ɑ΂��鏈��

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

	�A�C�e���̎��s

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_ExecItem(HWND hWnd, char *Action, struct TPITEM *tpItemInfo)
{
	char *p = NULL;
	int rc = -1;

	if(lstrcmp(Action, "open") == 0){
		//�J��
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
		//�`�F�b�N����URL�ŊJ��
		if(tpItemInfo->CheckURL != NULL){
			p = tpItemInfo->CheckURL;
		}
		if(p == NULL || *p == '\0'){
			MessageBox(hWnd, STR_ERR_MSG_URLOPEN, STR_ERR_TITLE_URLOPEN, MB_ICONEXCLAMATION);
			return -1;
		}
		rc = ExecItem(hWnd, p, NULL);

	}else if(lstrcmp(Action, "header") == 0){
		//�w�b�_�\��
		tpItemInfo->Param3 = 1;
		SendMessage(hWnd, WM_ITEMCHECK, 0, (LPARAM)tpItemInfo);

	}else if(lstrcmp(Action, "source") == 0){
		//�\�[�X�\��
		tpItemInfo->Param3 = 2;
		SendMessage(hWnd, WM_ITEMCHECK, 0, (LPARAM)tpItemInfo);

	}else if(lstrcmp(Action, "gettitle") == 0){
		//�^�C�g���擾
		tpItemInfo->Param3 = 3;
		SendMessage(hWnd, WM_ITEMCHECK, 0, (LPARAM)tpItemInfo);

	} else if ( lstrcmp(Action, "siteopen") == 0 ) {
		// �T�C�gURL���J��
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


	// 1 ��Ԃ��ƃA�C�R��������������B
	return rc;
}


/******************************************************************************

	HTTP_GetInfo

	�v���g�R�����

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_GetInfo(struct TPPROTOCOLINFO *tpInfo)
{
	int i = 0;

	lstrcpy(tpInfo->Scheme, _T("http://\thttps://\tfeed:"));
	lstrcpy(tpInfo->NewMenu, STR_GETINFO_HTTP_NEWMENU);
	lstrcpy(tpInfo->FileType, "url");	//	��������ꍇ�� \t �ŋ�؂� ��) "txt\tdoc\tdat"

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
	lstrcpy(tpInfo->tpMenu[i].Name, _T("�T�C�g���J��(&B)"));
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
	lstrcpy(tpInfo->tpMenu[i].Name, "�X�V���̃N���A(&A)");
	lstrcpy(tpInfo->tpMenu[i].Action, "clear");
	tpInfo->tpMenu[i].Default = FALSE;
	tpInfo->tpMenu[i].Flag = 1;


	return 0;
}


/******************************************************************************

	HTTP_EndNotify

	WWWC�̏I���̒ʒm

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

	�ȉ�WinInet�p

******************************************************************************/

// �ʃX���b�h�Ń��N�G�X�g�J�n
DWORD WINAPI WinetStart(struct THARGS *ThArgs) {
	struct TPITEM *tpItemInfo = ThArgs->tpItemInfo;
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	int CmpMsg;

	CmpMsg = WinetReq(ThArgs, tpItemInfo, tpHTTP);

	PostMessage(ThArgs->hWnd, WM_WSOCK_SELECT, (WPARAM)tpItemInfo->ITEM_MARK, (LPARAM)CmpMsg);
	_endthreadex(0);

	return 0;
}


// �G���[���b�Z�[�W�̃v���V�[�W��
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



// �������ƃn���h�����������
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


// �`�F�b�N���āA���ʂ� WWWC �֒ʒm���ďI��
int WinetCheckEnd(HWND hWnd, struct TPITEM *tpItemInfo, int CmpMsg) {
	struct THARGS *ThArgs = (struct THARGS *)tpItemInfo->ITEM_MARK;
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	int ExecFlag;
	HWND hDlg;


	// �`�F�b�N
	if ( CmpMsg == ST_DEFAULT ) {
		CmpMsg = HeaderFunc(hWnd, tpItemInfo);
	}


	// �X�V���Ɏ��s
	if ( (CmpMsg == ST_UP) && ((ExecFlag = GetOptionInt(tpItemInfo->Option2, OP2_EXEC)) & 1) ) {
		if ( ExecOptionCommand(tpItemInfo,
						( tpItemInfo->ViewURL != NULL && *tpItemInfo->ViewURL != _T('\0') ) 
							? tpItemInfo->ViewURL : tpItemInfo->CheckURL
					) == FALSE
				) {
			SetErrorString(tpItemInfo, STR_MEM_ERR, FALSE);
			goto END;
		}

		// �X�V�ʒm���Ȃ�
		if ( ExecFlag & 2 ) {
			HTTP_InitItem(hWnd, tpItemInfo);
			CmpMsg = ST_DEFAULT;
		}

	// �G���[��ʒm
	} else if ( CmpMsg == ST_ERROR && ErrorNotify && tpItemInfo->Param3 == 0 ) {
		hDlg = CreateDialog(ghinst, (LPCTSTR)IDD_DIALOG_ERR, NULL, (DLGPROC)ErrorMessageProc);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_ERR_INFO), tpItemInfo->ErrStatus);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_ERR_TITLE), tpItemInfo->Title);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_ERR_URL), tpItemInfo->CheckURL);
		SetWindowText(GetDlgItem(hDlg, IDC_STATIC_ERR_DATE), tpItemInfo->CheckDate);
	}

	END:

	// �������ƃn���h�����������
	WinetClear(tpItemInfo);

	// GET�ł��Ȃ���
	if ( CmpMsg == CONTINUE_GET ) {
		return CHECK_NO;
	}

	SendMessage(hWnd, WM_CHECKEND, (WPARAM)CmpMsg, (LPARAM)tpItemInfo);
	SetSBText(hWnd, tpItemInfo, STR_STATUS_CHECKEND);


	return CHECK_END;
}



// Set-Cookie��ˍ�
// �����������
LPTSTR SetCookie(LPCTSTR BeforeCookie, LPCTSTR NewCookie) {
	int CookieNameLen = 0, CookieValueLen = 0, CookieNameIdx, Hit = 0;
	LPTSTR AfterCookie, AfterCookiePtr;
	LPCTSTR CookieValuePtr;


	CookieValuePtr = NewCookie;
	// ���O�̕�����
	while ( *CookieValuePtr != _T('=') && *CookieValuePtr != _T(';') && *CookieValuePtr != _T('\0') && *CookieValuePtr != _T(' ') ) {
		CookieValuePtr++;
		CookieNameLen++;
	}
	while ( *CookieValuePtr == _T(' ') ) CookieValuePtr++;
	if ( CookieNameLen == 0 || *CookieValuePtr != _T('=') ) return NULL;

	// =
	CookieValuePtr++; 
	while ( *CookieValuePtr == _T(' ') ) CookieValuePtr++;

	// �l�̊J�n�|�C���^
	// CookieValuePtr

	// �l�̕�����
	while ( *(CookieValuePtr + CookieValueLen) != _T(';') && *(CookieValuePtr + CookieValueLen) != _T('\0') ) {
		CookieValueLen++;
	}

	AfterCookiePtr = AfterCookie = S_ALLOC(lstrlen(BeforeCookie) + CookieNameLen + CookieValueLen + 2);
	if ( AfterCookie == NULL ) return NULL;

	while ( *BeforeCookie != _T('\0') ) {
		while ( *BeforeCookie == _T(' ') ) BeforeCookie++;
		for ( CookieNameIdx = 0; CookieNameIdx < CookieNameLen; CookieNameIdx++ ) {
			if ( *(BeforeCookie + CookieNameIdx) != *(NewCookie + CookieNameIdx) ) {
				// Set-Cookie �ƈႤ����
				break;
			}
		}
		if ( CookieNameIdx == CookieNameLen && *(BeforeCookie + CookieNameLen) == _T('=')) {
			// Set-Cookie �Ɠ�������
			_tcsncpy_s(AfterCookiePtr, CookieNameLen + 1, NewCookie, CookieNameLen);
			AfterCookiePtr += CookieNameLen;
			*AfterCookiePtr++ = _T('=');
			_tcsncpy_s(AfterCookiePtr, CookieValueLen + 1, CookieValuePtr, CookieValueLen);
			AfterCookiePtr += CookieValueLen;
			*AfterCookiePtr++ = _T(';');
			while ( *BeforeCookie != _T('\0') && *BeforeCookie != _T(';') ) BeforeCookie++;
			Hit = 1;
		} else {
			// Set-Cookie �ƈႤ����
			while ( *BeforeCookie != _T('\0') && *BeforeCookie != _T(';') ) *AfterCookiePtr++ = *BeforeCookie++;
			*AfterCookiePtr++ = _T(';');
		}
		if ( *BeforeCookie == _T(';') ) BeforeCookie++;
	}

	if ( Hit ) {
		// �Ō�� ; ���I�[�ɕς���
		*(AfterCookiePtr - 1) = _T('\0');
	} else {
		// ��Cookie�Ȃ�
		_tcsncpy_s(AfterCookiePtr, CookieNameLen + 1, NewCookie, CookieNameLen);
		AfterCookiePtr += CookieNameLen;
		*AfterCookiePtr++ = _T('=');
		_tcsncpy_s(AfterCookiePtr, CookieValueLen + 1, CookieValuePtr, CookieValueLen);
	}


	return AfterCookie;
}



// ���N�G�X�g
int WinetReq(struct THARGS *ThArgs, struct TPITEM *tpItemInfo, struct TPHTTP *tpHTTP) {

	// WWWC
	int CmpMsg = ST_DEFAULT;
	// �Ȃ�ł�
	TCHAR buf[BIGSIZE];
	DWORD dwBuf, dwLen;
	BOOL bFlag;
	// �G���[���b�Z�[�W
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


	// WinInet �J�n
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

	// �^�C���A�E�g
	dwBuf = TimeOut * 1000;
	InternetSetOption(ThArgs->hInet, INTERNET_OPTION_CONNECT_TIMEOUT, &dwBuf, sizeof(dwBuf));
	InternetSetOption(ThArgs->hInet, INTERNET_OPTION_SEND_TIMEOUT, &dwBuf, sizeof(dwBuf));
	InternetSetOption(ThArgs->hInet, INTERNET_OPTION_RECEIVE_TIMEOUT , &dwBuf, sizeof(dwBuf));


	// �T�[�o�ڑ��ݒ�
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


	// If-Modified-Since ���g����
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
	

	// ���N�G�X�g�����
	ThArgs->hHttpReq = HttpOpenRequest(
		ThArgs->hHttpCon, // hConnect
		(tpItemInfo->user2 == REQUEST_HEAD) ? _T("HEAD") : (tpItemInfo->user2 == REQUEST_POST) ? _T("POST") : _T("GET"), // ���\�b�h
		tpHTTP->Path, //path, // ObjectName
		_T("HTTP/1.1"), // Version
		GetOptionString(tpItemInfo->Option2, buf, OP2_REFERRER) ? buf : NULL, // Referer
		NULL, // AcceptTypes
		0
			| ( bFlag ? INTERNET_FLAG_RELOAD : 0 ) // ��ɃA�N�Z�X
			| INTERNET_FLAG_EXISTING_CONNECT // �ڑ����ė��p
			| ( tpHTTP->SecureFlag ? INTERNET_FLAG_SECURE : 0 ) // HTTPS
			| ( GetOptionInt(tpItemInfo->Option2, OP2_COOKIEFLAG) & 1 ? 0 : INTERNET_FLAG_NO_COOKIES ) // Cookie�������Ŏg�p���邩
			| ( GetOptionNum(tpItemInfo->ITEM_FILTER, REQ_REDIRECT) ? 0 : INTERNET_FLAG_NO_AUTO_REDIRECT) // ���_�C���N�g���邩
		, // Flags
		0 // Context
	);
	if ( ThArgs->hHttpReq == NULL ) {
		goto WINET_ERR;
	}


	// �ؖ����G���[�𖳎�
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


	// �w�b�_�����

	//�L���b�V������
	if(Proxy == 1 && pNoCache == 1){
		_tcscat_s(Headers, BIGSIZE, _T("\r\nPragma: no-cache\r\nCache-Control: no-cache"));
	}


	//�v���L�V�F��
	if(tpHTTP->ProxyFlag == 1 && pUsePass == 1){
		wsprintf(BaseStr1, _T("%s:%s"), pUser, pPass);
		eBase(BaseStr1, BaseStr2);
		_tcscat_s(Headers, BIGSIZE, _T("\r\nProxy-Authorization: Basic "));
		_tcscat_s(Headers, BIGSIZE, BaseStr2);
	}


	// �y�[�W�F��
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
					ErrMsg = _T("OAuth���s");
					goto REQ_ERR;
				}
				break;
		}
	}


	// �N�b�L�[
	if(GetOptionString(tpItemInfo->Option2, buf, OP2_COOKIE) == TRUE) {
		_tcscat_s(Headers, BIGSIZE, _T("\r\nCookie: "));
		_tcscat_s(Headers, BIGSIZE, buf);
	}


	// POST�f�[�^
	if( GetOptionInt(tpItemInfo->Option1, OP1_REQTYPE) == REQUEST_POST ) {
		if( GetOptionString(tpItemInfo->Option1, poststring, OP1_POST) == FALSE ) {
			*poststring = _T('\0');
		}
		_tcscat_s(Headers, BIGSIZE, _T("\r\nContent-Type: application/x-www-form-urlencoded"));
		dwLen = lstrlen(poststring);
	} else {
		dwLen = 0;
	}


	// ���N�G�X�g�𑗂�
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


	// �X�e�[�^�X�R�[�h
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
		// ����Set-Cookie�̌J��Ԃ�
		dwLen = 0;
		dwBuf = 0;
		while ( 1 ) {
			// �T�C�Y�擾
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

			// �{�擾
			HttpQueryInfo(
				ThArgs->hHttpReq,
				HTTP_QUERY_SET_COOKIE,
				NewCookie,
				&dwLen,
				&dwBuf
			);

			// �˂����킹
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
				ErrMsg = _T("SetCookie���s");
				goto REQ_ERR;
			}
		}

		if ( AfterCookie != NULL ) {
			// �VCookie��ۑ�
			OptionPtr = tpItemInfo->Option2;
			AfterOptionPtr = AfterOption = S_ALLOC(M_SIZE(OptionPtr) + M_SIZE(AfterCookie));
			if ( AfterOption == NULL ) {
				M_FREE(AfterCookie);
				goto MEM_ERR;
			}

			// Cookie�ݒ�̑O���R�s�[
			for ( dwBuf = 0; dwBuf < OP2_COOKIE; dwBuf ++ ) {
				while ( *OptionPtr != _T(';') || *(OptionPtr + 1) != _T(';') ) *AfterOptionPtr++ = *OptionPtr++;
				*AfterOptionPtr++ = _T(';');
				*AfterOptionPtr++ = _T(';');
				OptionPtr += 2;
			}

			// �VCookie���R�s�[
			lstrcpy(AfterOptionPtr, AfterCookie);
			AfterOptionPtr += lstrlen(AfterCookie);
			*AfterOptionPtr++ = _T(';');
			*AfterOptionPtr++ = _T(';');
			M_FREE(AfterCookie);

			// Cookie�ݒ�̌���R�s�[
			while ( *OptionPtr != _T(';') || *(OptionPtr + 1) != _T(';') ) OptionPtr++;
			OptionPtr += 2;
			lstrcpy(AfterOptionPtr, OptionPtr);

			// �ۑ�
			M_FREE(tpItemInfo->Option2);
			tpItemInfo->Option2 = AfterOption;
		}
	}

	// �w�b�_�`�F�b�N�͓��e�擾���Ȃ�
	if( CheckHead(tpItemInfo, tpHTTP) == TRUE ){
		goto REQ_END;
	}

	// ���e
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
			// �������ƒǉ����𕹂���������
			BodyTemp = M_ALLOC(tpHTTP->Size + RECVSIZE);
			if ( BodyTemp == NULL ) {
				goto MEM_ERR;
			}
			// �����𕡐�
			if ( tpHTTP->Size > 0 ) {
				memcpy(BodyTemp, tpHTTP->Body, tpHTTP->Size);
			}

			// �擾
			bFlag = InternetReadFile(ThArgs->hHttpReq, BodyTemp + tpHTTP->Size, RECVSIZE - 2, &dwLen);
			if ( bFlag == FALSE ) {
				M_FREE(BodyTemp);
				goto WINET_ERR;
			}

			// �����������
			M_FREE(tpHTTP->Body);
			// �V���������L�^
			tpHTTP->Body = BodyTemp;
			// �T�C�Y�v
			tpHTTP->Size += dwLen;

			//��M�o�C�g�����X�e�[�^�X�o�[�ɏo��
			wsprintf(buf, STR_STATUS_RECV, tpHTTP->Size);
			SetSBText(ThArgs->hWnd, tpItemInfo, buf);
		} while ( dwLen != 0 );
	}

	// �Ō��NULL
	*(tpHTTP->Body + tpHTTP->Size) = '\0';
	*(tpHTTP->Body + tpHTTP->Size + 1) = '\0';


	// ���N�G�X�g�����܂�
	REQ_END :
	return CmpMsg;


	// �G���[
	WINET_ERR : 
	if ( tpItemInfo->ITEM_CANCEL ) {
		goto REQ_END; // ��
	}
	dwBuf = WinetGetErrMsg(buf, sizeof(buf));
	if ( dwBuf == ERROR_INTERNET_TIMEOUT ) {
		CmpMsg = ST_TIMEOUT;
		SetErrorString(tpItemInfo, buf, FALSE);
		goto REQ_END; // ��
	}
	ErrMsg = buf;
	goto REQ_ERR; // ����

	MEM_ERR :
	ErrMsg = STR_MEM_ERR;
	// ��

	REQ_ERR :
	CmpMsg = ST_ERROR;
	SetErrorString(tpItemInfo, ErrMsg, FALSE);
	goto REQ_END; // ������
}



// URL�����s
BOOL ExecOptionCommand(struct TPITEM* tpItemInfo, LPTSTR Url) {
	LPTSTR Command, Param;
	TCHAR EndChar;
	int CommandLen = 0;


	// ������
	Command = S_ALLOC(BUFSIZE);
	if ( Command == NULL ) return FALSE;
	Param = S_ALLOC(BUFSIZE + lstrlen(Url));
	if ( Param == NULL ) {
		M_FREE(Command);
		return FALSE;
	}

	// �ݒ�𓾂�
	GetOptionString(tpItemInfo->Option2, Command, OP2_COMMAND);

	// �t�@�C�����ƈ����𕪂���
	// �t�@�C�����̏I��蕶��
	if ( *Command == _T('"') ) {
		EndChar = _T('"');
		CommandLen = 1;
	} else {
		EndChar = _T(' ');
	}
	// �t�@�C�����̏I����T��
	for ( ; *(Command + CommandLen) != _T('\0') && *(Command + CommandLen) != EndChar; CommandLen++ );
	if ( *(Command + CommandLen) == _T('"') ) {
		CommandLen++;
	}
	// �����𕪂���
	if ( *(Command + CommandLen) != _T('\0') ) {
		lstrcpy(Param, Command + CommandLen + 1);
		*(Command + CommandLen) = _T('\0');
	} else {
		*Param = _T('\0');
	}

	// URL��������
	lstrcat(Param, Url);

	// ���s
	ShellExecute(NULL, NULL, Command, Param, NULL, SW_SHOW);
	M_FREE(Command);
	M_FREE(Param);


	return TRUE;
}



// WinInet �̃G���[���b�Z�[�W���擾
int WinetGetErrMsg(OUT LPTSTR Buf, IN int BufSize) {
	int ErrNum = 0, Ret, i;

	if ( Buf == NULL || BufSize < 1 ) return 0;

	ErrNum = GetLastError();
	*Buf = _T('\0');
	Ret = FormatMessage(
		FORMAT_MESSAGE_IGNORE_INSERTS
			| ( ErrNum >= 12000 && ErrNum < 13000
				? FORMAT_MESSAGE_FROM_HMODULE : FORMAT_MESSAGE_FROM_SYSTEM )
		, // ���͌��Ə������@�̃I�v�V����
		GetModuleHandle(_T("wininet.dll")), // ���b�Z�[�W�̓��͌�
		ErrNum, // ���b�Z�[�W���ʎq
		0, // ���ꎯ�ʎq
		Buf, // ���b�Z�[�W�o�b�t�@
		BufSize, // ���b�Z�[�W�o�b�t�@�̍ő�T�C�Y (TCHAR)
		NULL // �����̃��b�Z�[�W�}���V�[�P���X����Ȃ�z��
	);

	for ( i = 0; i < BufSize && *(Buf + i) != _T('\0') && *(Buf + i) != _T('\r') && *(Buf + i) != _T('\n'); i++ );
	_stprintf_s(Buf + i, BufSize - i, _T(" (%d)"), ErrNum);


	return ErrNum;
}




/* End of source */
