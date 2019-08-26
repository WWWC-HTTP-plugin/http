/**************************************************************************

	WWWC (wwwc.dll)

	httpprop.c

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
#include <tchar.h>

#include "String.h"
#include "httptools.h"
#include "http.h"
#include "StrTbl.h"
#include "wwwcdll.h"
#include "resource.h"

#include "def.h"
#include "feed.h"
#include "auth.h"
#include <wininet.h>


/**************************************************************************
	Define
**************************************************************************/

#define WM_LV_EVENT					(WM_USER + 300)		//���X�g�r���[�C�x���g

#define MAXPROPITEM					100


/**************************************************************************
	Global Variables
**************************************************************************/

struct TPITEM *gItemInfo;
int PropRet;

struct ITEMPROP
{
	HWND hWnd;
	struct TPITEM *tpItemInfo;
};
struct ITEMPROP ItemProp[MAXPROPITEM];


extern char app_path[];
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

extern char AppName[30][BUFSIZE];
extern int AppCnt;

extern BOOL InfoToDate;
extern BOOL InfoToTitle;
extern BOOL DoubleDecodeAndAmp;
extern BOOL EachNotify;
extern BOOL ErrorNotify;


/**************************************************************************
	Local Function Prototypes
**************************************************************************/

static LRESULT OptionNotifyProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void AddItemInfo(HWND hWnd, struct TPITEM *tpItemInfo);
static struct TPITEM *GetItemInfo(HWND hWnd);
static void DeleteItemInfo(struct TPITEM *tpItemInfo);
static void DrawScrollControl(LPDRAWITEMSTRUCT lpDrawItem, UINT i);
static BOOL CALLBACK PropertyProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static void ReqTypeEnable(HWND hDlg);
static BOOL CALLBACK PropertyCheckProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK PropertyOptionProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK PropertyAdditionalProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
static BOOL CALLBACK ProtocolPropertyCheckProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);


/******************************************************************************

	OptionNotifyProc

	�_�C�A���O�̒ʒm����������

******************************************************************************/

static LRESULT OptionNotifyProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	PSHNOTIFY *pshn = (PSHNOTIFY FAR *) lParam;
	NMHDR *lpnmhdr = (NMHDR FAR *)&pshn->hdr;

	switch(lpnmhdr->code)
	{
	case PSN_APPLY:
		SendMessage(hDlg, WM_COMMAND, IDOK, 0);
		PropRet = 0;
		break;

	case PSN_QUERYCANCEL:
		SendMessage(hDlg, WM_COMMAND, IDPCANCEL, 0);
		break;

	case PSN_RESET:
		PropRet = -1;
		break;

	default:
		return PSNRET_NOERROR;
	}
	return PSNRET_NOERROR;
}


/******************************************************************************

	AddItemInfo

	�A�C�e����񃊃X�g�ɃA�C�e������o�^

******************************************************************************/

static void AddItemInfo(HWND hWnd, struct TPITEM *tpItemInfo)
{
	int i;

	for(i = 0; i < MAXPROPITEM; i++){
		if(ItemProp[i].hWnd == NULL){
			ItemProp[i].hWnd = hWnd;
			ItemProp[i].tpItemInfo = tpItemInfo;
			break;
		}
	}
}


/******************************************************************************

	GetItemInfo

	�A�C�e����񃊃X�g����A�C�e����������

******************************************************************************/

static struct TPITEM *GetItemInfo(HWND hWnd)
{
	int i;

	for(i = 0; i < MAXPROPITEM; i++){
		if(ItemProp[i].hWnd == hWnd){
			return ItemProp[i].tpItemInfo;
		}
	}
	return NULL;
}


/******************************************************************************

	DeleteItemInfo

	�A�C�e����񃊃X�g����A�C�e�������폜

******************************************************************************/

static void DeleteItemInfo(struct TPITEM *tpItemInfo)
{
	int i;

	for(i = 0; i < MAXPROPITEM; i++){
		if(ItemProp[i].tpItemInfo == tpItemInfo){
			ItemProp[i].hWnd = NULL;
			ItemProp[i].tpItemInfo = NULL;
			break;
		}
	}
}


/******************************************************************************

	DrawScrollControl

	�X�N���[���o�[�̃{�^���̕`��

******************************************************************************/

static void DrawScrollControl(LPDRAWITEMSTRUCT lpDrawItem, UINT i)
{
	#define FOCUSRECT_SIZE		3

	if(lpDrawItem->itemState & ODS_DISABLED){
		//�g�p�s�\
		i |= DFCS_INACTIVE;
	}
	if(lpDrawItem->itemState & ODS_SELECTED){
		//�I��
		i |= DFCS_PUSHED;
	}

	//�t���[���R���g���[���̕`��
	DrawFrameControl(lpDrawItem->hDC, &(lpDrawItem->rcItem), DFC_SCROLL, i);

	//�t�H�[�J�X
	if(lpDrawItem->itemState & ODS_FOCUS){
		lpDrawItem->rcItem.left += FOCUSRECT_SIZE;
		lpDrawItem->rcItem.top += FOCUSRECT_SIZE;
		lpDrawItem->rcItem.right -= FOCUSRECT_SIZE;
		lpDrawItem->rcItem.bottom -= FOCUSRECT_SIZE;
		DrawFocusRect(lpDrawItem->hDC, &(lpDrawItem->rcItem));
	}
}


/******************************************************************************

	PropertyProc

	�A�C�e���̃v���p�e�B�ݒ�_�C�A���O

******************************************************************************/

static BOOL CALLBACK PropertyProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct TPITEM *tpItemInfo;
	char tmp[BUFSIZE];
	char *buf;
	char *p, *r;
	int len;

	RECT WinPos = {0};

	switch (uMsg)
	{
	case WM_INITDIALOG:
		AddItemInfo(GetParent(hDlg), gItemInfo);
		tpItemInfo = gItemInfo;

		/* �A�C�e���̏�񂪋�łȂ��ꍇ�̓A�C�e���̓��e��\������ */
		/* �^�C�g�� */
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_TITLE), WM_SETTEXT, 0,
			(LPARAM)((tpItemInfo->Title != NULL) ? tpItemInfo->Title : STR_NEWITEMNAME));

		/* �`�F�b�N����URL */
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_URL), WM_SETTEXT, 0,
			(LPARAM)((tpItemInfo->CheckURL != NULL) ? tpItemInfo->CheckURL : "http://"));
		/* �J��URL */
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEWURL), WM_SETTEXT, 0,
			(LPARAM)((tpItemInfo->ViewURL != NULL) ? tpItemInfo->ViewURL : ""));

		/* �R�����g */
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_COMMENT), EM_LIMITTEXT, MAXSIZE - 2, 0);
		if(tpItemInfo->Comment != NULL){
			buf = (char *)GlobalAlloc(GPTR, lstrlen(tpItemInfo->Comment) + 1);
			if(buf != NULL){
				for(p = tpItemInfo->Comment, r = buf ;*p != '\0'; p++){
					if(IsDBCSLeadByte(*p) == TRUE){
						*(r++) = *(p++);
						*(r++) = *p;
					}else if(*p == '\\' && *(p + 1) == 'n'){
						*(r++) = '\r';
						*(r++) = '\n';
						p++;
					}else if(*p == '\\' && *(p + 1) == '\\'){
						*(r++) = '\\';
						p++;
					}else{
						*(r++) = *p;
					}
				}
				*r = '\0';
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_COMMENT), WM_SETTEXT, 0, (LPARAM)buf);
				GlobalFree(buf);
			}
		}

		/* �T�C�Y */
		if(tpItemInfo->Size != NULL){
			wsprintf(tmp, "%s �o�C�g", tpItemInfo->Size);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_SIZE), WM_SETTEXT, 0, (LPARAM)tmp);
		}

		/* �X�V���� */
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_UPDATE), WM_SETTEXT, 0,
			(LPARAM)((tpItemInfo->Date != NULL) ? tpItemInfo->Date : ""));

		/* �`�F�b�N�������� */
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_CHECKDATE), WM_SETTEXT, 0,
			(LPARAM)((tpItemInfo->CheckDate != NULL) ? tpItemInfo->CheckDate : ""));

		/* �G���[��� */
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_ERRORINFO), WM_SETTEXT, 0,
			(LPARAM)((tpItemInfo->ErrStatus != NULL) ? tpItemInfo->ErrStatus : ""));

		// �T�C�gURL
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_SITEURL), WM_SETTEXT, 0,
			(LPARAM)(
				(tpItemInfo->ITEM_SITEURL != NULL 
					&& ( *tpItemInfo->ITEM_SITEURL >= '0' && *tpItemInfo->ITEM_SITEURL <= '9') == FALSE
				) ? tpItemInfo->ITEM_SITEURL : ""
			)
		);

		break;

	case WM_DRAWITEM:
		if((UINT)wParam != IDC_BUTTON_CHANGE){
			return FALSE;
		}
		//�{�^���̕`��
		DrawScrollControl((LPDRAWITEMSTRUCT)lParam, DFCS_SCROLLDOWN);
		break;

	case WM_NOTIFY:
		return OptionNotifyProc(hDlg, uMsg, wParam, lParam);

	case WM_COMMAND:
		switch(wParam)
		{
		case IDOK:
			tpItemInfo = GetItemInfo(GetParent(hDlg));

			/* �^�C�g����ݒ肷�� */
			if(tpItemInfo->Title != NULL){
				GlobalFree(tpItemInfo->Title);
			}
			len = SendMessage(GetDlgItem(hDlg, IDC_EDIT_TITLE), WM_GETTEXTLENGTH, 0, 0) + 1;
			tpItemInfo->Title = GlobalAlloc(GPTR, len + 1);
			if(tpItemInfo->Title != NULL){
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_TITLE), WM_GETTEXT, len, (LPARAM)tpItemInfo->Title);
			}

			/* URL��ݒ肷�� */
			if(tpItemInfo->CheckURL != NULL){
				len = SendMessage(GetDlgItem(hDlg, IDC_EDIT_URL), WM_GETTEXTLENGTH, 0, 0) + 1;
				buf = GlobalAlloc(GPTR, len + 1);
				if(buf != NULL){
					SendMessage(GetDlgItem(hDlg, IDC_EDIT_URL), WM_GETTEXT, len, (LPARAM)buf);
					if(lstrcmp(tpItemInfo->CheckURL, buf) != 0){
						if(tpItemInfo->Size != NULL){
							GlobalFree(tpItemInfo->Size);
							tpItemInfo->Size = NULL;
						}
						if(tpItemInfo->Date != NULL){
							GlobalFree(tpItemInfo->Date);
							tpItemInfo->Date = NULL;
						}
						if(tpItemInfo->DLLData1 != NULL){
							GlobalFree(tpItemInfo->DLLData1);
							tpItemInfo->DLLData1 = NULL;
						}
						if(tpItemInfo->DLLData2 != NULL){
							GlobalFree(tpItemInfo->DLLData2);
							tpItemInfo->DLLData2 = NULL;
						}
					}
					GlobalFree(buf);
				}
				GlobalFree(tpItemInfo->CheckURL);
			}
			len = SendMessage(GetDlgItem(hDlg, IDC_EDIT_URL), WM_GETTEXTLENGTH, 0, 0) + 1;
			tpItemInfo->CheckURL = GlobalAlloc(GPTR, len + 1);
			if(tpItemInfo->CheckURL != NULL){
				SendMessage(GetDlgItem(hDlg,IDC_EDIT_URL), WM_GETTEXT, len, (LPARAM)tpItemInfo->CheckURL);
			}

			/* �J��URL��ݒ肷�� */
			if(tpItemInfo->ViewURL != NULL){
				GlobalFree(tpItemInfo->ViewURL);
			}
			len = SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEWURL), WM_GETTEXTLENGTH, 0, 0) + 1;
			tpItemInfo->ViewURL = GlobalAlloc(GPTR, len + 1);
			if(tpItemInfo->ViewURL != NULL){
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEWURL), WM_GETTEXT, len, (LPARAM)tpItemInfo->ViewURL);
			}

			/* �R�����g��ݒ肷�� */
			if(tpItemInfo->Comment != NULL){
				GlobalFree(tpItemInfo->Comment);
			}
			len = SendMessage(GetDlgItem(hDlg, IDC_EDIT_COMMENT), WM_GETTEXTLENGTH, 0, 0) + 1;
			tpItemInfo->Comment = (char *)GlobalAlloc(GPTR, len + 1);
			if(tpItemInfo->Comment != NULL){
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_COMMENT), WM_GETTEXT, len, (LPARAM)tpItemInfo->Comment);
			}

			// �T�C�gURL
			M_FREE(tpItemInfo->ITEM_SITEURL);
			len = SendMessage(GetDlgItem(hDlg, IDC_EDIT_SITEURL), WM_GETTEXTLENGTH, 0, 0) + 1;
			tpItemInfo->ITEM_SITEURL = S_ALLOC(len);
			if(tpItemInfo->ITEM_SITEURL != NULL){
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_SITEURL), WM_GETTEXT, len, (LPARAM)tpItemInfo->ITEM_SITEURL);
			}

			break;

		case IDC_BUTTON_GETURL:
			DialogBoxParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_URLLIST), hDlg, URLListroc, (LPARAM)hDlg);
			break;

		case IDC_BUTTON_CHANGE:		//URL�̓���ւ�
			/* �`�F�b�N����URL�̎擾 */
			len = SendMessage(GetDlgItem(hDlg, IDC_EDIT_URL), WM_GETTEXTLENGTH, 0, 0) + 1;
			p = GlobalAlloc(GPTR, len + 1);
			if(p != NULL){
				SendMessage(GetDlgItem(hDlg,IDC_EDIT_URL), WM_GETTEXT, len, (LPARAM)p);
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_VIEWURL), WM_SETTEXT, 0, (LPARAM)p);
				GlobalFree(p);
			}
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}

static void ReqTypeEnable(HWND hDlg)
{
	if(CheckType == 1 ||
		(IsDlgButtonChecked(hDlg, IDC_CHECK_SIZE) != 0 && IsDlgButtonChecked(hDlg, IDC_CHECK_NOTAGSIZE) != 0) ||
		IsDlgButtonChecked(hDlg, IDC_CHECK_MD5) != 0 ||
		IsDlgButtonChecked(hDlg, IDC_CHECK_META) != 0 ||
		IsDlgButtonChecked(hDlg, IDC_CHECK_FEED) != 0 ||
		IsDlgButtonChecked(hDlg, IDC_CHECK_DRAW) != 0 ) {
		EnableWindow(GetDlgItem(hDlg, IDC_RADIO_REQTYPE_AUTO), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_RADIO_REQTYPE_GET), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_RADIO_REQTYPE_POST), FALSE);
		EnableWindow(GetDlgItem(hDlg, IDC_EDIT_POST), FALSE);
	}else{
		EnableWindow(GetDlgItem(hDlg, IDC_RADIO_REQTYPE_AUTO), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_RADIO_REQTYPE_GET), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_RADIO_REQTYPE_POST), TRUE);
		EnableWindow(GetDlgItem(hDlg, IDC_EDIT_POST), TRUE);
		if(IsDlgButtonChecked(hDlg, IDC_RADIO_REQTYPE_POST) == 0){
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_POST), FALSE);
		}else{
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_POST), TRUE);
		}
	}
}



void ShowSetMeta(HWND hDlg, BOOL EnableFlag) {
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_META_TYPE), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_META_TYPENAME), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_META_CONTENT), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_META_GROUP), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_META_TYPE), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_META_TYPENAME), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_META_CONTENT), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_META_INFO), EnableFlag);

	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_DATE), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_FEED), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_DRAW), !EnableFlag);
}



void ShowSetFeed(HWND hDlg, BOOL EnableFlag) {
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_FEED_GROUP), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_FEED_URL), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_FEED_TITLE), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_FEED_CATEGORY), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_FEED_CONTENT), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_FEED_DESCRIPTION), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_FEED_URL), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_FEED_TITLE), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_FEED_CATEGORY), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_FEED_CONTENT), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_FEED_DESCRIPTION), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_FEED_INFO), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_COMBO_FEED_URL), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_COMBO_FEED_TITLE), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_COMBO_FEED_CATEGORY), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_COMBO_FEED_CONTENT), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_COMBO_FEED_DESCRIPTION), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_FEED_SOURCE), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_CHECK_FEED_SOURCE), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_FEED_SOURCE), EnableFlag);

	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_DATE), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_SIZE), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_MD5), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_META), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_DRAW), !EnableFlag);

	if ( EnableFlag == FALSE ) {
		EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOTAGSIZE), IsDlgButtonChecked(hDlg, IDC_CHECK_SIZE));
	} else {
		EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOTAGSIZE), FALSE);
	}
}


void ShowSetDraw(HWND hDlg, BOOL EnableFlag) {
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_DRAW_GROUP), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_DRAW_ORDER), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_DRAW_URLP), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_DRAW_URLF), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_DRAW_INFOP), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_STATIC_DRAW_INFOF), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_RADIO_DRAW_ORDER_TOP), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_RADIO_DRAW_ORDER_BOTTOM), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_RADIO_DRAW_ORDER_ALL), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_DRAW_URLP), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_DRAW_URLF), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_DRAW_INFOP), EnableFlag);
	ShowWindow(GetDlgItem(hDlg, IDC_EDIT_DRAW_INFOF), EnableFlag);

	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_DATE), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_SIZE), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_MD5), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_META), !EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_CHECK_FEED), !EnableFlag);

	if ( EnableFlag == FALSE ) {
		EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOTAGSIZE), IsDlgButtonChecked(hDlg, IDC_CHECK_SIZE));
	} else {
		EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOTAGSIZE), FALSE);
	}
}


void SetFeedComboBox(HWND hDlg, DWORD ControlID, int SetNum) {
	HWND hCbWnd;
	char ComboIndex[] = FEED_FILTER_TYPE_INDEX;

	hCbWnd = GetDlgItem(hDlg, ControlID);
	SendMessage(hCbWnd, CB_ADDSTRING, 0, (LPARAM)_T("�s�g�p"));
	SendMessage(hCbWnd, CB_ADDSTRING, 0, (LPARAM)_T("�I��"));
	SendMessage(hCbWnd, CB_ADDSTRING, 0, (LPARAM)_T("�I�� (OR)"));
	SendMessage(hCbWnd, CB_ADDSTRING, 0, (LPARAM)_T("���O"));
	SendMessage(hCbWnd, CB_SETCURSEL, ComboIndex[SetNum], 0);
}


void EnableSetAuth(HWND hDlg, BOOL EnableFlag) {
	EnableWindow(GetDlgItem(hDlg, IDC_EDIT_USER), EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PASS), EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_RADIO_AUTHTYPE_BASIC), EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_RADIO_AUTHTYPE_WSSE), EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_RADIO_AUTHTYPE_OAUTH), EnableFlag);
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_GETOAUTH), EnableFlag && IsDlgButtonChecked(hDlg, IDC_RADIO_AUTHTYPE_OAUTH));
}


// �ݒ��ۑ��p�ɂ܂Ƃ߂�
// OptionText�̑傫���͏\���ɗp�ӂ��Ă�������
//void SetOptionText(LPTSTR OptionText, LPTSTR OptionName, LPTSTR OptionValue) {
void SetOptionText(LPTSTR OptionText, LPTSTR OptionName, HWND hDlg, DWORD ControlID) {
	LPTSTR vp, tp;
	TCHAR buf[BIGSIZE];


	// �R���g���[���̕�����𓾂�
	*buf = _T('\0');
	SendMessage(GetDlgItem(hDlg, ControlID), WM_GETTEXT, BIGSIZE, (LPARAM)buf);
	if ( *buf == _T('\0') ) {
		return;
	}

	// �ݒ薼
	lstrcat(OptionText, OptionName);
	lstrcat(OptionText, _T("="));

	// �G�X�P�[�v���Ȃ���R�s�[
	vp = buf;
	tp = OptionText + lstrlen(OptionText);
	while ( *vp != _T('\0') ) {
		switch ( *vp ) {
			case _T('\\') :
				*tp++ = _T('\\');
				*tp = _T('\\');
				break;

			case _T('\r') :
				if ( *(vp + 1) == _T('\n') ) vp++ ;
			case _T('\n') :
				if ( *(vp + 1) != _T('\0') ) { // �Ō�̉��s�͖���
					*tp = _T('|');
				} else {
					tp--;
				}
				break;

			case _T('|') :
				*tp++ = _T('\\');
				*tp = _T('|');
				break;

			case _T(';') :
				*tp++ = _T('\\');
				*tp = _T(';');
				break;

			default :
				if ( IsDBCSLeadByte( *vp ) != FALSE && *(vp + 1) != _T('\0')) {
					*tp++ = *vp++;
				}
				*tp = ( (unsigned)*vp <= 0x20 || *vp == 0x7f ) ? _T(' ') : *vp;
				break;
		}
		tp++;
		vp++;
	}

	*tp++ = _T(';');
	*tp++ = _T(';');
	*tp = _T('\0');
}



// �ݒ�l�̐擪�|�C���^��Ԃ�
LPTSTR GetOptionValue(LPTSTR OptionText, LPTSTR OptionName) {
	LPTSTR tp, np;
	int NameLen;

	if ( OptionText == NULL || *OptionText == _T('\0') ) {
		return NULL;
	}

	// �ݒ�l�̉ӏ�
	NameLen = lstrlen(OptionName);
	tp = OptionText;
	while ( (np = _tcsstr(tp, OptionName)) != NULL ) {
		if ( (np == OptionText || (*(np - 1) == _T(';') && *(np - 2) == _T(';')))
					&& *(np + NameLen) == _T('=')
		) {
			break;
		}
		tp = np + NameLen;
	}
	if ( np == NULL ) {
		return NULL;
	}
	return np + NameLen + 1;
}

// �ݒ萔�l��Ԃ�
int GetOptionNum(LPTSTR OptionText, LPTSTR OptionName) {
	LPTSTR Value;

	Value = GetOptionValue(OptionText, OptionName);
	return ( Value == NULL ) ? 0 : _ttoi(Value);
}

// ������������ĕԂ�
LPTSTR GetOptionText(LPTSTR OptionText, LPTSTR OptionName, BOOL MultiLine) {
	LPTSTR vp, vep, vmp, vmpt;

	vp = GetOptionValue(OptionText, OptionName);
	if ( vp == NULL ) {
		return NULL;
	}

	// �ݒ�̕������𓾂āA���̕��̃�����
	// ������傫���Ȃ鎖�͂Ȃ��͂�
	vep = _tcsstr(vp, _T(";;"));
	if ( vep == NULL ) {
		return NULL;
	}
	vmp = S_ALLOC((vep - vp) * 2);
	if ( vmp == NULL ) {
		return NULL;
	}

	// �G�X�P�[�v��߂�
	for ( vmpt = vmp; vp < vep; vp++, vmpt++ ) {
		switch (*vp) {
			case _T('\\') :
				switch ( *(vp + 1) ) {
					case _T('\\') :
					case _T('|') :
					case _T(';') :
						vp++;
					default :
						*vmpt = *vp;
						break;
				}
				break;
			case _T('|') :
				if ( MultiLine != FALSE ) {
					*vmpt++ = _T('\r');
					*vmpt = _T('\n');
					break;
				}
			default :
				*vmpt = *vp;
				break;
		}
	}
	*vmpt = _T('\0');

	return vmp;
}

// �ݒ肩��l�����o���ă_�C�A���O�ɓ\��t��
void SetOptionEdit(HWND hDlg, DWORD ControlID, LPTSTR OptionText, LPTSTR OptionName) {
	LPTSTR DlgText;

	DlgText = GetOptionText(OptionText, OptionName, TRUE);
	SendMessage(GetDlgItem(hDlg, ControlID), WM_SETTEXT, 0, (LPARAM)DlgText);
	M_FREE(DlgText);
}



/******************************************************************************

	PropertyCheckProc

	�A�C�e���̃`�F�b�N�ݒ�_�C�A���O

******************************************************************************/

static BOOL CALLBACK PropertyCheckProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct TPITEM *tpItemInfo;
	char buf[BIGSIZE];
	char type[BUFSIZE];
	char name[BUFSIZE];
	char content[BUFSIZE];
	char poststring[BIGSIZE];
	int ReqType;
	BOOL EnableFlag;
	int OptionLen, CmbIdx;
	char FeedFilterTypeIndex[] = FEED_FILTER_TYPE_INDEX;


	switch (uMsg)
	{
	case WM_INITDIALOG:
		tpItemInfo = GetItemInfo(GetParent(hDlg));

		CheckDlgButton(hDlg, IDC_CHECK_REDIRECT, GetOptionNum(tpItemInfo->ITEM_FILTER, REQ_REDIRECT));

		SendMessage(GetDlgItem(hDlg, IDC_EDIT_POST), EM_LIMITTEXT, BIGSIZE - 2, 0);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_TYPE), EM_LIMITTEXT, BUFSIZE - 2, 0);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_TYPENAME), EM_LIMITTEXT, BUFSIZE - 2, 0);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_CONTENT), EM_LIMITTEXT, BUFSIZE - 2, 0);

		switch(GetOptionInt(tpItemInfo->Option1, OP1_REQTYPE))
		{
		case 1:
			SetWindowText(GetDlgItem(hDlg, IDC_RADIO_REQTYPE_AUTO), STR_REQTYPE_AUTO_GET);
		case 0:
			CheckDlgButton(hDlg, IDC_RADIO_REQTYPE_AUTO, 1);
			break;

		case 2:
			CheckDlgButton(hDlg, IDC_RADIO_REQTYPE_GET, 1);
			break;

		case 3:
			CheckDlgButton(hDlg, IDC_RADIO_REQTYPE_POST, 1);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_POST), TRUE);
			break;
		}

		if(GetOptionString(tpItemInfo->Option1, buf, OP1_POST) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_POST), WM_SETTEXT, 0, (LPARAM)buf);
		}

		CheckDlgButton(hDlg, IDC_CHECK_DATE, !GetOptionInt(tpItemInfo->Option1, OP1_NODATE));
		CheckDlgButton(hDlg, IDC_CHECK_SIZE, !GetOptionInt(tpItemInfo->Option1, OP1_NOSIZE));
		CheckDlgButton(hDlg, IDC_CHECK_NOTAGSIZE, GetOptionInt(tpItemInfo->Option1, OP1_NOTAGSIZE));
		CheckDlgButton(hDlg, IDC_CHECK_MD5, GetOptionInt(tpItemInfo->Option1, OP1_MD5));

		switch ( GetOptionInt(tpItemInfo->Option1, OP1_META) ) {
			case 1 : // META�^�O
				CheckDlgButton(hDlg, IDC_CHECK_META, TRUE);
				ShowSetMeta(hDlg, TRUE);
				break;
			case 2 : // �t�B�[�h
				CheckDlgButton(hDlg, IDC_CHECK_FEED, TRUE);
				ShowSetFeed(hDlg, TRUE);
				break;
			case 3 : // �����N���o
				CheckDlgButton(hDlg, IDC_CHECK_DRAW, TRUE);
				ShowSetDraw(hDlg, TRUE);
				break;
		}

		if(GetOptionString(tpItemInfo->Option1, buf, OP1_TYPE) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_TYPE), WM_SETTEXT, 0, (LPARAM)buf);
		}
		if(GetOptionString(tpItemInfo->Option1, buf, OP1_NAME) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_TYPENAME), WM_SETTEXT, 0, (LPARAM)buf);
		}
		if(GetOptionString(tpItemInfo->Option1, buf, OP1_CONTENT) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_CONTENT), WM_SETTEXT, 0, (LPARAM)buf);
		}

		if(IsDlgButtonChecked(hDlg, IDC_CHECK_SIZE) == 0){
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOTAGSIZE), FALSE);
		}

#define SetFeedCB(CID, NAME)	SetFeedComboBox(hDlg, CID, GetOptionNum(tpItemInfo->ITEM_FILTER, NAME))
		SetFeedCB(IDC_COMBO_FEED_URL, FEED_FILTER_TYPE_URL);
		SetFeedCB(IDC_COMBO_FEED_TITLE, FEED_FILTER_TYPE_TITLE);
		SetFeedCB(IDC_COMBO_FEED_CATEGORY, FEED_FILTER_TYPE_CATEGORY);
		SetFeedCB(IDC_COMBO_FEED_CONTENT, FEED_FILTER_TYPE_CONTENT);
		SetFeedCB(IDC_COMBO_FEED_DESCRIPTION, FEED_FILTER_TYPE_DESCRIPTION);

#define SetOptEdit(CID, NAME)	SetOptionEdit(hDlg, CID, tpItemInfo->ITEM_FILTER, NAME)
		SetOptEdit(IDC_EDIT_FEED_URL, FEED_FILTER_URL);
		SetOptEdit(IDC_EDIT_FEED_TITLE, FEED_FILTER_TITLE);
		SetOptEdit(IDC_EDIT_FEED_CATEGORY, FEED_FILTER_CATEGORY);
		SetOptEdit(IDC_EDIT_FEED_CONTENT, FEED_FILTER_CONTENT);
		SetOptEdit(IDC_EDIT_FEED_DESCRIPTION, FEED_FILTER_DESCRIPTION);
		SetOptEdit(IDC_EDIT_FEED_SOURCE, FEED_FILTER_SOURCE);

		CheckDlgButton(hDlg, IDC_CHECK_FEED_SOURCE, GetOptionNum(tpItemInfo->ITEM_FILTER, FEED_FILTER_TYPE_SOURCE));

		switch ( GetOptionNum(tpItemInfo->ITEM_FILTER, DRAW_FILTER_ORDER) ) {
			case DRAW_BOTTOM : CheckDlgButton(hDlg, IDC_RADIO_DRAW_ORDER_BOTTOM, TRUE); break;
			case DRAW_ALL : CheckDlgButton(hDlg, IDC_RADIO_DRAW_ORDER_ALL, TRUE); break;
			default : CheckDlgButton(hDlg, IDC_RADIO_DRAW_ORDER_TOP, TRUE); break;
		}
		SetOptEdit(IDC_EDIT_DRAW_URLP, DRAW_FILTER_URLP);
		SetOptEdit(IDC_EDIT_DRAW_URLF, DRAW_FILTER_URLF);
		SetOptEdit(IDC_EDIT_DRAW_INFOP, DRAW_FILTER_INFOP);
		SetOptEdit(IDC_EDIT_DRAW_INFOF, DRAW_FILTER_INFOF);

		break;

	case WM_NOTIFY:
		return OptionNotifyProc(hDlg, uMsg, wParam, lParam);

	case WM_COMMAND:
		switch(wParam)
		{
		case IDC_RADIO_REQTYPE_AUTO:
		case IDC_RADIO_REQTYPE_GET:
		case IDC_RADIO_REQTYPE_POST:
			if(IsDlgButtonChecked(hDlg, IDC_RADIO_REQTYPE_POST) == 0){
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_POST), FALSE);
			}else{
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_POST), TRUE);
			}
			break;

		case IDC_CHECK_SIZE:
			if(IsDlgButtonChecked(hDlg, IDC_CHECK_SIZE) == 0){
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOTAGSIZE), FALSE);
			}else{
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOTAGSIZE), TRUE);
			}
			break;

		case IDC_CHECK_META:
			EnableFlag = (IsDlgButtonChecked(hDlg, IDC_CHECK_META) == 0) ? FALSE : TRUE;
			ShowSetMeta(hDlg, EnableFlag);

		case IDC_CHECK_NOTAGSIZE:
		case IDC_CHECK_MD5:
			break;

		case IDC_CHECK_FEED:
			EnableFlag = (IsDlgButtonChecked(hDlg, IDC_CHECK_FEED) == 0) ? FALSE : TRUE;
			ShowSetFeed(hDlg, EnableFlag);
			break;

		case IDC_CHECK_DRAW:
			EnableFlag = (IsDlgButtonChecked(hDlg, IDC_CHECK_DRAW) == 0) ? FALSE : TRUE;
			ShowSetDraw(hDlg, EnableFlag);
			break;

		case IDOK:
			tpItemInfo = GetItemInfo(GetParent(hDlg));

			if(IsDlgButtonChecked(hDlg, IDC_CHECK_DATE) == 0 &&
				IsDlgButtonChecked(hDlg, IDC_CHECK_SIZE) == 0 &&
				IsDlgButtonChecked(hDlg, IDC_CHECK_MD5) == 0 &&
				IsDlgButtonChecked(hDlg, IDC_CHECK_META) == 0){
				CheckDlgButton(hDlg, IDC_CHECK_DATE, 1);
				CheckDlgButton(hDlg, IDC_CHECK_SIZE, 1);
			}

			ReqType = GetOptionInt(tpItemInfo->Option1, OP1_REQTYPE);
			ReqType = (IsDlgButtonChecked(hDlg, IDC_RADIO_REQTYPE_GET) == 1)
				? 2 : ((IsDlgButtonChecked(hDlg, IDC_RADIO_REQTYPE_POST) == 1)
				? 3 : (ReqType == 2 || ReqType == 3)
				? 0 : ReqType);

			SendMessage(GetDlgItem(hDlg, IDC_EDIT_POST), WM_GETTEXT, BIGSIZE - 1, (LPARAM)poststring);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_TYPE), WM_GETTEXT, BUFSIZE - 1, (LPARAM)type);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_TYPENAME), WM_GETTEXT, BUFSIZE - 1, (LPARAM)name);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_META_CONTENT), WM_GETTEXT, BUFSIZE - 1, (LPARAM)content);

			if(tpItemInfo->Option1 != NULL){
				GlobalFree(tpItemInfo->Option1);
			}
			tpItemInfo->Option1 = (char *)GlobalAlloc(GPTR,
				32 + lstrlen(type) + lstrlen(name) + lstrlen(content) + lstrlen(poststring));
			if(tpItemInfo->Option1 != NULL){
				wsprintf(tpItemInfo->Option1, "%d;;%d;;%d;;%d;;%d;;%s;;%s;;%s;;%d;;%s",
					ReqType,
					!IsDlgButtonChecked(hDlg, IDC_CHECK_DATE),
					!IsDlgButtonChecked(hDlg, IDC_CHECK_SIZE),
					IsDlgButtonChecked(hDlg, IDC_CHECK_NOTAGSIZE),
					IsDlgButtonChecked(hDlg, IDC_CHECK_DRAW) ? 3 : IsDlgButtonChecked(hDlg, IDC_CHECK_FEED) ? 2 : IsDlgButtonChecked(hDlg, IDC_CHECK_META),
					type, name, content,
					IsDlgButtonChecked(hDlg, IDC_CHECK_MD5),
					poststring);
			}

			// �t�B�[�h�ݒ�
			// �����������v���ă�����
#define GetDlgLen(CID)	(int)SendMessage(GetDlgItem(hDlg, CID), WM_GETTEXTLENGTH, 0, 0);
			OptionLen = GetDlgLen(IDC_EDIT_FEED_URL);
			OptionLen += GetDlgLen(IDC_EDIT_FEED_TITLE);
			OptionLen += GetDlgLen(IDC_EDIT_FEED_CATEGORY);
			OptionLen += GetDlgLen(IDC_EDIT_FEED_CONTENT);
			OptionLen += GetDlgLen(IDC_EDIT_FEED_DESCRIPTION);
			OptionLen += GetDlgLen(IDC_EDIT_FEED_SOURCE);
			OptionLen += GetDlgLen(IDC_EDIT_DRAW_URLP);
			OptionLen += GetDlgLen(IDC_EDIT_DRAW_URLF);
			OptionLen += GetDlgLen(IDC_EDIT_DRAW_INFOP);
			OptionLen += GetDlgLen(IDC_EDIT_DRAW_INFOF);
			OptionLen += lstrlen(
				REQ_REDIRECT
				FEED_FILTER_URL
				FEED_FILTER_TITLE
				FEED_FILTER_CATEGORY
				FEED_FILTER_CONTENT
				FEED_FILTER_DESCRIPTION
				FEED_FILTER_SOURCE
				FEED_FILTER_TYPE_URL
				FEED_FILTER_TYPE_TITLE
				FEED_FILTER_TYPE_CATEGORY
				FEED_FILTER_TYPE_CONTENT
				FEED_FILTER_TYPE_DESCRIPTION
				FEED_FILTER_TYPE_SOURCE
				DRAW_FILTER_ORDER
				DRAW_FILTER_URLP
				DRAW_FILTER_URLF
				DRAW_FILTER_INFOP
				DRAW_FILTER_INFOF
			); // �ݒ薼
			OptionLen *= 2; // ��؂�t���A�S�G�X�P�[�v�ł��ς���

			M_FREE(tpItemInfo->ITEM_FILTER);
			tpItemInfo->ITEM_FILTER = S_ALLOC_Z(OptionLen);
			// REDIRECT
			if ( IsDlgButtonChecked(hDlg, IDC_CHECK_REDIRECT) ) {
				wsprintf(tpItemInfo->ITEM_FILTER + lstrlen(tpItemInfo->ITEM_FILTER),
					REQ_REDIRECT _T("=%d;;"), 1);
			}

			// ���͗�
#define SetOptText(NAME, CID)	SetOptionText(tpItemInfo->ITEM_FILTER, NAME, hDlg, CID);
			SetOptText(FEED_FILTER_URL, IDC_EDIT_FEED_URL);
			SetOptText(FEED_FILTER_TITLE, IDC_EDIT_FEED_TITLE);
			SetOptText(FEED_FILTER_CATEGORY, IDC_EDIT_FEED_CATEGORY);
			SetOptText(FEED_FILTER_CONTENT, IDC_EDIT_FEED_CONTENT);
			SetOptText(FEED_FILTER_DESCRIPTION, IDC_EDIT_FEED_DESCRIPTION);
			SetOptText(FEED_FILTER_SOURCE, IDC_EDIT_FEED_SOURCE);
			// �R���{�{�b�N�X
#define SetOptNum(NAME, CID)	if ( CmbIdx = SendMessage(GetDlgItem(hDlg, CID), CB_GETCURSEL, 0, 0) ) wsprintf(tpItemInfo->ITEM_FILTER + lstrlen(tpItemInfo->ITEM_FILTER), NAME _T("=%d;;"), FeedFilterTypeIndex[CmbIdx]);
			SetOptNum(FEED_FILTER_TYPE_URL, IDC_COMBO_FEED_URL);
			SetOptNum(FEED_FILTER_TYPE_TITLE, IDC_COMBO_FEED_TITLE);
			SetOptNum(FEED_FILTER_TYPE_CATEGORY, IDC_COMBO_FEED_CATEGORY);
			SetOptNum(FEED_FILTER_TYPE_CONTENT, IDC_COMBO_FEED_CONTENT);
			SetOptNum(FEED_FILTER_TYPE_DESCRIPTION, IDC_COMBO_FEED_DESCRIPTION);
			if ( IsDlgButtonChecked(hDlg, IDC_CHECK_FEED_SOURCE) ) {
				wsprintf(tpItemInfo->ITEM_FILTER + lstrlen(tpItemInfo->ITEM_FILTER),
					FEED_FILTER_TYPE_SOURCE _T("=%d;;"), 1);
			}

			// �����N���o
			SetOptText(DRAW_FILTER_URLP, IDC_EDIT_DRAW_URLP);
			SetOptText(DRAW_FILTER_URLF, IDC_EDIT_DRAW_URLF);
			SetOptText(DRAW_FILTER_INFOP, IDC_EDIT_DRAW_INFOP);
			SetOptText(DRAW_FILTER_INFOF, IDC_EDIT_DRAW_INFOF);
			if ( IsDlgButtonChecked(hDlg, IDC_RADIO_DRAW_ORDER_TOP) == FALSE ) {
				wsprintf(tpItemInfo->ITEM_FILTER + lstrlen(tpItemInfo->ITEM_FILTER),
					DRAW_FILTER_ORDER _T("=%d;;"),
					IsDlgButtonChecked(hDlg, IDC_RADIO_DRAW_ORDER_BOTTOM) ? DRAW_BOTTOM :
						IsDlgButtonChecked(hDlg, IDC_RADIO_DRAW_ORDER_ALL) ? DRAW_ALL : DRAW_TOP
				);
			}

			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}



// OAuth �̃A�N�Z�X�g�[�N���𓾂�_�C�A���O�{�b�N�X�̃v���V�[�W��
BOOL CALLBACK GetOauthTokenProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam) {

	struct TPITEM *tpItemInfo = NULL, *OauthItem = NULL;
	struct TPHTTP *OauthHttp = NULL;
	struct OAUTHPARAM *OauthParam = NULL;
	struct OAUTHCONFIG *ItemOauthConfig;
	LPTSTR Identifier = NULL, Secret = NULL;
	LPTSTR OauthAuthorizeUri = NULL, Verifier = NULL;
	HWND OptionTab;
	int Ret;


	switch ( msg ) {
		case WM_INITDIALOG : // ���ꂽ
			// �_�C�A���O�Ƀz�X�g����\��
			SetWindowText(GetDlgItem(hDlg, IDC_STATIC_GETOAUTH_HOSTNAME), (LPTSTR)lParam);
			SetFocus(GetDlgItem(hDlg, IDC_BUTTON_GETOAUTH_START));
			break;

		case WM_COMMAND : // ���삳�ꂽ
			switch( LOWORD(wParam) ) {
				case IDC_BUTTON_GETOAUTH_START : // �J�n
					EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_GETOAUTH_START), FALSE);
					tpItemInfo = GetItemInfo(GetParent(hDlg));
					Ret = GetOauthStart(hDlg, tpItemInfo);
					if ( Ret == FALSE ) {
						SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_SETSEL, (WPARAM)0, (LPARAM)0);
						SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_REPLACESEL, 0, (LPARAM)_T("�ꎞ�F�؎擾���s\r\n\r\n"));
						goto ERR;
					}
					break;

				case IDC_BUTTON_GETOAUTH_GETTOKEN :
					Ret = GetWindowTextLength(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_VERIFIER));
					if ( Ret == 0 ) {
						break;
					}
					EnableWindow(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_VERIFIER), FALSE);
					EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_GETOAUTH_GETTOKEN), FALSE);

					Verifier = S_ALLOC(Ret * 3);
					if ( Verifier == NULL ) {
						goto ERR;
					}
					GetWindowText(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_VERIFIER), Verifier + Ret * 2, Ret + 1);
					ConvStrToUri(Verifier + Ret * 2, Verifier, Ret * 3 + 1);

					tpItemInfo = GetItemInfo(GetParent(hDlg));
					Ret = GetOauthToken(hDlg, tpItemInfo, Verifier);
					if ( Ret == FALSE ) {
						SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_SETSEL, (WPARAM)0, (LPARAM)0);
						SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_REPLACESEL, 0, (LPARAM)_T("�g�[�N���擾���s\r\n\r\n"));
						goto ERR;
					}
					break;

				case IDOK : // OK �ŕۑ�
					tpItemInfo = GetItemInfo(GetParent(hDlg));
					OauthItem = (struct TPITEM *)tpItemInfo->ITEM_OAUTH;
					OauthParam = (struct OAUTHPARAM *)OauthItem->ITEM_OAUTH;
					OptionTab = FindWindowEx(GetParent(hDlg), NULL, (LPCSTR)32770, NULL);
					SetWindowText(GetDlgItem(OptionTab, IDC_EDIT_USER), OauthParam->Identifier);
					SetWindowText(GetDlgItem(OptionTab, IDC_EDIT_PASS), OauthParam->Secret);
					// ��

				case IDCANCEL : // �L�����Z��
					tpItemInfo = GetItemInfo(GetParent(hDlg));
					if ( HAS_OAUTH_ITEM(tpItemInfo) ) {
						OauthItem = (struct TPITEM *)tpItemInfo->ITEM_OAUTH;
						WinetClear(OauthItem);
						// Param2 = CheckURL �� WinetClear() �ŉ���ς݂̂͂�
						M_FREE(OauthItem->ErrStatus);
						M_FREE(OauthItem->Option1);
						M_FREE(OauthItem->Option2);
						if ( HAS_OAUTH_PARAM(OauthItem) ) {
							OauthParam = (struct OAUTHPARAM *)OauthItem->ITEM_OAUTH;
							M_FREE(OauthParam->Identifier);
							M_FREE(OauthParam->Secret);
							M_FREE(OauthParam->Verifier);
							M_FREE(OauthParam);
						}
						M_FREE(OauthItem);
						tpItemInfo->ITEM_OAUTH = NULL;
					}
					tpItemInfo->ITEM_STATE = (ITEM_STATE_TYPE)0;
					// �_�C�A���O�{�b�N�X�I��
					EndDialog(hDlg, 0);
					break;
			}

			return FALSE;

		case WM_WSOCK_SELECT :
			OauthItem = ((struct THARGS *)wParam)->tpItemInfo;
			OauthHttp = (struct TPHTTP *)OauthItem->Param1;
			if ( OauthHttp == NULL ) {
				goto ERR;
			}
			if ( OauthHttp->Body != NULL ) {
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_SETSEL, (WPARAM)0, (LPARAM)0);
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_REPLACESEL, 0, (LPARAM)OauthHttp->Body);
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_REPLACESEL, 0, (LPARAM)_T("\r\n\r\n"));
				SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_SETSEL, (WPARAM)0, (LPARAM)0);
			}

			if ( OauthHttp->StatusCode == 200
				&& GET_OAUTH_VAR(OauthItem) == 1
				&& ParseOauthCredentials1(OauthHttp->Body, &Identifier, &Secret)
			) {
				OauthParam = (struct OAUTHPARAM *)OauthItem->ITEM_OAUTH;
				if ( OauthItem->ITEM_STATE == (ITEM_STATE_TYPE)OAUTH_STATE_TEMP1 ) {
					// GetOauthTemp 
					ItemOauthConfig = OauthParam->OauthConfig;
					OauthAuthorizeUri = S_ALLOC(lstrlen(ItemOauthConfig->AuthorizationUri) + lstrlen(Identifier) + 13);
					if ( OauthAuthorizeUri == NULL ) {
						goto ERR;
					}
					lstrcpy(OauthAuthorizeUri, ItemOauthConfig->AuthorizationUri);
					lstrcat(OauthAuthorizeUri, _tcschr(ItemOauthConfig->AuthorizationUri, _T('?')) ? _T("&oauth_token=") : _T("?oauth_token="));
					lstrcat(OauthAuthorizeUri, Identifier);
					SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_SETSEL, (WPARAM)0, (LPARAM)0);
					SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_REPLACESEL, 0, (LPARAM)OauthAuthorizeUri);
					SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_REPLACESEL, 0, (LPARAM)_T("\r\n\r\n"));
					SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_SETSEL, (WPARAM)0, (LPARAM)0);
					ShellExecute(NULL, NULL, OauthAuthorizeUri, NULL, NULL, SW_SHOW);
					M_FREE(OauthAuthorizeUri);
					EnableWindow(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_VERIFIER), TRUE);
					EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_GETOAUTH_GETTOKEN), TRUE);

				} else {
					// GetOauthToken
					M_FREE(OauthParam->Identifier);
					M_FREE(OauthParam->Secret);
					EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
					EnableWindow(GetDlgItem(hDlg, IDC_STATIC_GETOAUTH_SUCCESS), TRUE);
				}

				OauthParam->Identifier = Identifier;
				OauthParam->Secret = Secret;
				Identifier = NULL;
				Secret = NULL;

			} else if ( OauthHttp->StatusCode == 200
						&& GET_OAUTH_VAR(OauthItem) == 2
						&& ParseOauthCredentials2(OauthHttp->Body, &Identifier, &Secret, (struct OAUTHCONFIG *)OauthItem->ITEM_OAUTH)
			) {
				OauthParam = M_ALLOC_Z(sizeof(struct OAUTHPARAM));
				if ( OauthParam == NULL ) {
					goto ERR;
				}
				OauthItem->ITEM_OAUTH = (ITEM_OAUTH_TYPE)OauthParam;
				(int)OauthItem->ITEM_STATE |= OAUTH_STATE_FLAG_PARAM;
				OauthParam->Identifier = Identifier;
				OauthParam->Secret = Secret;
				Identifier = NULL;
				Secret = NULL;
				EnableWindow(GetDlgItem(hDlg, IDOK), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_STATIC_GETOAUTH_SUCCESS), TRUE);

			} else {
				if ( HAS_STR(OauthItem->ErrStatus) == FALSE ) {
					// �G���[���b�Z�[�W�ɃX�e�[�^�X���b�Z�[�W������
					// ������
					Ret = 0;
					HttpQueryInfo(((struct THARGS *)wParam)->hHttpReq,
						HTTP_QUERY_STATUS_TEXT, NULL, &Ret, NULL);
					M_FREE(OauthItem->ErrStatus);
					OauthItem->ErrStatus = M_ALLOC_Z(Ret + 5 * sizeof(TCHAR));
					// �X�e�[�^�X�R�[�h
					_itot_s(OauthHttp->StatusCode, OauthItem->ErrStatus, 4, 10);
					*(OauthItem->ErrStatus + 3) = _T(' ');
					// �{�擾
					HttpQueryInfo(((struct THARGS *)wParam)->hHttpReq,
						HTTP_QUERY_STATUS_TEXT, OauthItem->ErrStatus + 4, &Ret, NULL);
				}
				goto ERR;
			}

			WinetClear(OauthItem);
			break;
	}

	return FALSE;


ERR :
	if (
		( OauthItem != NULL
			|| (HAS_OAUTH_ITEM(tpItemInfo)
				&& (OauthItem = (struct TPITEM *)tpItemInfo->ITEM_OAUTH)))
		&& HAS_STR(OauthItem->ErrStatus)
	) {
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_SETSEL, (WPARAM)0, (LPARAM)0);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_REPLACESEL, 0, (LPARAM)OauthItem->ErrStatus);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_REPLACESEL, 0, (LPARAM)_T("\r\n\r\n"));
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_LOG), EM_SETSEL, (WPARAM)0, (LPARAM)0);
		MessageBox(hDlg, OauthItem->ErrStatus, _T("OAuth �ݒ莸�s"), MB_ICONERROR);
	}
	M_FREE(Identifier);
	M_FREE(Secret);
	return FALSE;
}



/******************************************************************************

	PropertyOptionProc

	�A�C�e���̃I�v�V�����ݒ�_�C�A���O

******************************************************************************/

static BOOL CALLBACK PropertyOptionProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct TPITEM *tpItemInfo;
	char buf[BIGSIZE];
	char proxy[BUFSIZE];
	char port[BUFSIZE];
	char user[BUFSIZE];
	char pass[BIGSIZE];
	char useragent[BUFSIZE];
	char referrer[BUFSIZE];
	char cookie[BIGSIZE];
	char command[BUFSIZE];
	BOOL EnableFlag;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		tpItemInfo = GetItemInfo(GetParent(hDlg));

		/* �A�C�e���̏�񂪋�łȂ��ꍇ�̓A�C�e���̓��e��\������ */
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_PROXY), EM_LIMITTEXT, BUFSIZE - 2, 0);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_PROXY_PORT), EM_LIMITTEXT, BUFSIZE - 2, 0);

		CheckDlgButton(hDlg, IDC_CHECK_NOPROXY, GetOptionInt(tpItemInfo->Option2, OP2_NOPROXY));
		CheckDlgButton(hDlg, IDC_CHECK_SETPROXY, GetOptionInt(tpItemInfo->Option2, OP2_SETPROXY));

		if(GetOptionString(tpItemInfo->Option2, buf, OP2_PROXY) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PROXY), WM_SETTEXT, 0, (LPARAM)buf);
		}

		if(GetOptionString(tpItemInfo->Option2, buf, OP2_PORT) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PROXY_PORT), WM_SETTEXT, 0, (LPARAM)buf);
		}

		EnableFlag = GetOptionInt(tpItemInfo->Option2, OP2_USEPASS);

		// �F�ؕ���
		switch ( EnableFlag ) {
			case AUTH_OAUTH_ON:
			case AUTH_OAUTH:
				CheckDlgButton(hDlg, IDC_RADIO_AUTHTYPE_OAUTH, TRUE);
				break;
			case AUTH_WSSE_ON:
			case AUTH_WSSE:
				CheckDlgButton(hDlg, IDC_RADIO_AUTHTYPE_WSSE, TRUE);
				break;
			default :
				CheckDlgButton(hDlg, IDC_RADIO_AUTHTYPE_BASIC, TRUE);
		}

		// �F�؂��邩
		if ( EnableFlag & 1 ) {
			CheckDlgButton(hDlg, IDC_CHECK_USEPASS, TRUE);
			EnableSetAuth(hDlg, TRUE);
		}

		if(GetOptionString(tpItemInfo->Option2, buf, OP2_USER) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_USER), WM_SETTEXT, 0, (LPARAM)buf);
		}

		if(GetOptionString(tpItemInfo->Option2, buf, OP2_PASS) == TRUE){
			dPass(buf, pass);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PASS), WM_SETTEXT, 0, (LPARAM)pass);
		}

		// User-Agent
		if(GetOptionString(tpItemInfo->Option2, buf, OP2_USERAGENT) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_USERAGENT), WM_SETTEXT, 0, (LPARAM)buf);
		}

		// Referrer
		if(GetOptionString(tpItemInfo->Option2, buf, OP2_REFERRER) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_REFERRER), WM_SETTEXT, 0, (LPARAM)buf);
		}

		// Cookie
		if(GetOptionString(tpItemInfo->Option2, buf, OP2_COOKIE) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_COOKIE), WM_SETTEXT, 0, (LPARAM)buf);
		}

		if(IsDlgButtonChecked(hDlg, IDC_CHECK_NOPROXY) != 0){
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK_SETPROXY), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY_PORT), FALSE);
		}
		if(IsDlgButtonChecked(hDlg, IDC_CHECK_SETPROXY) == 0){
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY_PORT), FALSE);
		}

		EnableFlag = GetOptionInt(tpItemInfo->Option2, OP2_COOKIEFLAG);
		CheckDlgButton(hDlg, IDC_CHECK_SETCOOKIE, (EnableFlag & 2) != 0);

		if ( EnableFlag & 1 ) {
			CheckDlgButton(hDlg, IDC_CHECK_AUTOCOOKIE, TRUE);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COOKIE), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK_SETCOOKIE), FALSE);
		}

		CheckDlgButton(hDlg, IDC_CHECK_IGNORECERTERROR, GetOptionInt(tpItemInfo->Option2, OP2_IGNORECERTERROR));


		EnableFlag = GetOptionInt(tpItemInfo->Option2, OP2_EXEC);
		if ( EnableFlag & 1 ) {
			CheckDlgButton(hDlg, IDC_CHECK_EXEC, TRUE);
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NONOTIFY), TRUE);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMMAND), TRUE);
		}

		if ( EnableFlag & 2 ) {
			CheckDlgButton(hDlg, IDC_CHECK_NONOTIFY, TRUE);
		}

		if(GetOptionString(tpItemInfo->Option2, buf, OP2_COMMAND) == TRUE){
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_COMMAND), WM_SETTEXT, 0, (LPARAM)buf);
		}

		break;

	case WM_NOTIFY:
		return OptionNotifyProc(hDlg, uMsg, wParam, lParam);

	case WM_COMMAND:
		switch(wParam)
		{
		case IDC_CHECK_NOPROXY:
			if(IsDlgButtonChecked(hDlg, IDC_CHECK_NOPROXY) != 0){
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_SETPROXY), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY_PORT), FALSE);
			}else{
				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_SETPROXY), TRUE);
				if(IsDlgButtonChecked(hDlg, IDC_CHECK_SETPROXY) != 0){
					EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY), TRUE);
					EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY_PORT), TRUE);
				}
			}
			break;

		case IDC_CHECK_SETPROXY:
			EnableFlag = (IsDlgButtonChecked(hDlg, IDC_CHECK_SETPROXY) == 0) ? FALSE : TRUE;

			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY), EnableFlag);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PROXY_PORT), EnableFlag);
			break;

		case IDC_CHECK_USEPASS:
			EnableFlag = (IsDlgButtonChecked(hDlg, IDC_CHECK_USEPASS) == 0) ? FALSE : TRUE;
			EnableSetAuth(hDlg, EnableFlag);
			break;

		case IDOK:
			tpItemInfo = GetItemInfo(GetParent(hDlg));

			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PROXY), WM_GETTEXT, BUFSIZE - 1, (LPARAM)proxy);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PROXY_PORT), WM_GETTEXT, BUFSIZE - 1, (LPARAM)port);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_USER), WM_GETTEXT, BUFSIZE - 1, (LPARAM)user);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PASS), WM_GETTEXT, BUFSIZE - 1, (LPARAM)buf);
			ePass(buf, pass);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_USERAGENT), WM_GETTEXT, BUFSIZE - 1, (LPARAM)useragent);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_REFERRER), WM_GETTEXT, BUFSIZE - 1, (LPARAM)referrer);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_COOKIE), WM_GETTEXT, BIGSIZE - 1, (LPARAM)cookie);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_COMMAND), WM_GETTEXT, BUFSIZE - 1, (LPARAM)command);
			

			if(tpItemInfo->Option2 != NULL){
				GlobalFree(tpItemInfo->Option2);
			}
			tpItemInfo->Option2 = (char *)GlobalAlloc(GPTR,
				35 + lstrlen(proxy) + lstrlen(port) + lstrlen(user) + lstrlen(pass) + lstrlen(useragent) + lstrlen(referrer) + lstrlen(cookie) + lstrlen(command));
			if(tpItemInfo->Option2 != NULL){
				wsprintf(tpItemInfo->Option2, "%d;;%d;;%s;;%s;;%d;;%s;;%s;;%s;;%s;;%s;;%d;;%d;;%d;;%s",
					IsDlgButtonChecked(hDlg, IDC_CHECK_NOPROXY),
					IsDlgButtonChecked(hDlg, IDC_CHECK_SETPROXY),
					proxy, port,
					IsDlgButtonChecked(hDlg, IDC_CHECK_USEPASS) + (IsDlgButtonChecked(hDlg, IDC_RADIO_AUTHTYPE_WSSE) ? AUTH_WSSE : IsDlgButtonChecked(hDlg, IDC_RADIO_AUTHTYPE_OAUTH) ? AUTH_OAUTH : 0),
					user, pass, useragent, referrer, cookie,
					IsDlgButtonChecked(hDlg, IDC_CHECK_AUTOCOOKIE) | (IsDlgButtonChecked(hDlg, IDC_CHECK_SETCOOKIE) << 1),
					IsDlgButtonChecked(hDlg, IDC_CHECK_IGNORECERTERROR),
					IsDlgButtonChecked(hDlg, IDC_CHECK_EXEC) | (IsDlgButtonChecked(hDlg, IDC_CHECK_NONOTIFY) << 1),
					command
				);
			}
			break;

		case IDC_CHECK_AUTOCOOKIE :
			// ���͗��̗L������
			EnableFlag = !IsDlgButtonChecked(hDlg, IDC_CHECK_AUTOCOOKIE);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COOKIE), EnableFlag);
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK_SETCOOKIE), EnableFlag);
			break;

		case IDC_CHECK_IGNORECERTERROR :
			// �x���ƃL�����Z��
			if ( IsDlgButtonChecked(hDlg, IDC_CHECK_IGNORECERTERROR) != 0 ) {
				if ( MessageBox(hDlg, "���댯", "�댯", MB_OKCANCEL | MB_ICONWARNING) != IDOK ) {
					CheckDlgButton(hDlg, IDC_CHECK_IGNORECERTERROR, BST_UNCHECKED);
				}
			}
			break;

		case IDC_CHECK_EXEC :
			EnableFlag = IsDlgButtonChecked(hDlg, IDC_CHECK_EXEC);
			EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NONOTIFY), EnableFlag);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_COMMAND), EnableFlag);
			break;

		case IDC_RADIO_AUTHTYPE_BASIC :
		case IDC_RADIO_AUTHTYPE_WSSE :
			EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_GETOAUTH), FALSE);
			break;

		case IDC_RADIO_AUTHTYPE_OAUTH :
			EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_GETOAUTH), TRUE);
			break;

		case IDC_BUTTON_GETOAUTH :
			tpItemInfo = GetItemInfo(GetParent(hDlg));

			// �쐬����Ǝv����
			if ( tpItemInfo == NULL || HAS_STR(tpItemInfo->CheckURL) == FALSE ) {
				MessageBox(hDlg, _T("�A�C�e����񂪂���܂���\n�g�[�N���擾�̑O�Ɉ�x�A�C�e���쐬���������Ă�������"), _T("OAuth �ݒ莸�s"), MB_ICONERROR);
				break;
			}
			// �ݒ肪�Ȃ�
			GetURL(tpItemInfo->CheckURL, buf, pass, NULL);
			tpItemInfo->ITEM_OAUTH = (ITEM_OAUTH_TYPE)GetOauthConfig(buf, pass);
			if ( tpItemInfo->ITEM_OAUTH == NULL ) {
				_tcscat_s(buf, BUFSIZE, _T(" �p�� OAuth �ݒ肪����܂���"));
				MessageBox(hDlg, buf, _T("OAuth �ݒ莸�s"), MB_ICONERROR);
				break;
			}

			// �_�C�A���O�\��
			EnableFlag = (int)DialogBoxParam(ghinst, (LPCSTR)IDD_DIALOG_GETOAUTH, hDlg, (DLGPROC)GetOauthTokenProc, (LPARAM)buf);
			// �G���[
			if ( EnableFlag == -1 ) {
				FormatMessage(
					FORMAT_MESSAGE_FROM_SYSTEM, // ����t���O
					NULL, // ���b�Z�[�W��`�ʒu
					GetLastError(), // ���b�Z�[�WID
					0, // ����ID
					buf, // �o�b�t�@�̃A�h���X
					BIGSIZE, // �o�b�t�@�̃T�C�Y
					NULL // �}����̔z��̃A�h���X
				);
				MessageBox(hDlg, buf, _T("IDC_BUTTON_GETOAUTHTOKEN"), MB_OK | MB_ICONERROR);
			}
			break;


		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


/******************************************************************************

	HTTP_Property

	�A�C�e���̃v���p�e�B�\��

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_Property(HWND hWnd, struct TPITEM *tpItemInfo)
{
#define sizeof_PROPSHEETHEADER		40	//�Â��R�����R���g���[���΍�
	PROPSHEETPAGE psp;
	PROPSHEETHEADER psh;
	HPROPSHEETPAGE hpsp[3];
	char *title;
	int i;

	for(i = 0; i < MAXPROPITEM; i++){
		if(ItemProp[i].hWnd == NULL){
			break;
		}
	}
	if(i >= MAXPROPITEM){
		return -1;
	}

	if(tpItemInfo->Title != NULL){
		title = (char *)GlobalAlloc(GPTR, lstrlen(tpItemInfo->Title) + lstrlen(STR_TITLE_PROP) + 1);
		if(title != NULL){
			wsprintf(title, STR_TITLE_PROP, tpItemInfo->Title);
		}
	}else{
		title = (char *)GlobalAlloc(GPTR, lstrlen(STR_TITLE_ADDITEM) + 1);
		if(title != NULL){
			lstrcpy(title, STR_TITLE_ADDITEM);
		}
	}

	psp.dwSize = sizeof(PROPSHEETPAGE);
	psp.dwFlags = PSP_DEFAULT;
	psp.hInstance = ghinst;

	psp.pszTemplate = MAKEINTRESOURCE(IDD_DIALOG_HTTPPROP);
	psp.pfnDlgProc = PropertyProc;
	hpsp[0] = CreatePropertySheetPage(&psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_DIALOG_HTTPPROP_CHECK);
	psp.pfnDlgProc = PropertyCheckProc;
	hpsp[1] = CreatePropertySheetPage(&psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_DIALOG_HTTPPROP_OPTION);
	psp.pfnDlgProc = PropertyOptionProc;
	hpsp[2] = CreatePropertySheetPage(&psp);

	ZeroMemory(&psh, sizeof(PROPSHEETHEADER));
	psh.dwSize = sizeof_PROPSHEETHEADER;
	psh.dwFlags = PSH_NOAPPLYNOW;
	psh.hInstance = ghinst;
	psh.hwndParent = hWnd;
	psh.pszCaption = title;
	psh.nPages = sizeof(hpsp) / sizeof(HPROPSHEETPAGE);
	psh.phpage = hpsp;
	psh.nStartPage = 0;

	gItemInfo = tpItemInfo;
	PropRet = -1;

	PropertySheet(&psh);

	DeleteItemInfo(tpItemInfo);
	if(title != NULL){
		GlobalFree(title);
	}
	return PropRet;
}


/******************************************************************************

	ProtocolPropertyCheckProc

	�v���g�R���̃v���p�e�B�ݒ�_�C�A���O

******************************************************************************/

static BOOL CALLBACK ProtocolPropertyCheckProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char buf[BUFSIZE];
	BOOL EnableFlag;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		wsprintf(buf, "%ld", TimeOut);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_TIMEOUIT), WM_SETTEXT, 0, (LPARAM)buf);

		CheckDlgButton(hDlg, IDC_CHECK_REQGET, CheckType);
		CheckDlgButton(hDlg, IDC_CHECK_ERRORNOTIFY, ErrorNotify);

		CheckDlgButton(hDlg, IDC_CHECK_PROXY, Proxy);
		EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PSERVER), Proxy);
		EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PPORT), Proxy);
		EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOCACHE), Proxy);
		EnableWindow(GetDlgItem(hDlg, IDC_CHECK_USEPASS), Proxy);
		EnableWindow(GetDlgItem(hDlg, IDC_EDIT_USER), Proxy);
		EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PASS), Proxy);

		SendMessage(GetDlgItem(hDlg, IDC_EDIT_PSERVER), WM_SETTEXT, 0, (LPARAM)pServer);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_PPORT), WM_SETTEXT, 0, (LPARAM)pPort);

		CheckDlgButton(hDlg, IDC_CHECK_NOCACHE, pNoCache);

		CheckDlgButton(hDlg, IDC_CHECK_USEPASS, pUsePass);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_USER), WM_SETTEXT, 0, (LPARAM)pUser);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_PASS), WM_SETTEXT, 0, (LPARAM)pPass);

		if(IsDlgButtonChecked(hDlg, IDC_CHECK_USEPASS) == 0){
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_USER), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PASS), FALSE);
		}
		break;

	case WM_NOTIFY:
		return OptionNotifyProc(hDlg, uMsg, wParam, lParam);

	case WM_COMMAND:
		switch(wParam)
		{
		case IDC_CHECK_PROXY:
			if(IsDlgButtonChecked(hDlg, IDC_CHECK_PROXY) == 1){
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PSERVER), TRUE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PPORT), TRUE);

				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOCACHE), TRUE);

				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_USEPASS),TRUE);
				if(IsDlgButtonChecked(hDlg, IDC_CHECK_USEPASS) == 1){
					EnableWindow(GetDlgItem(hDlg, IDC_EDIT_USER), TRUE);
					EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PASS), TRUE);
				}
			}else{
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PSERVER), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PPORT), FALSE);

				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_NOCACHE), FALSE);

				EnableWindow(GetDlgItem(hDlg, IDC_CHECK_USEPASS), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_USER), FALSE);
				EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PASS), FALSE);
			}
			break;

		case IDC_CHECK_USEPASS:
			EnableFlag = (IsDlgButtonChecked(hDlg, IDC_CHECK_USEPASS) == 1) ? TRUE : FALSE;

			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_USER), EnableFlag);
			EnableWindow(GetDlgItem(hDlg, IDC_EDIT_PASS), EnableFlag);
			break;

		case IDOK:
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_TIMEOUIT), WM_GETTEXT, BUFSIZE - 1, (LPARAM)buf);
			TimeOut = atoi(buf);
			CheckType = IsDlgButtonChecked(hDlg, IDC_CHECK_REQGET);

			ErrorNotify = IsDlgButtonChecked(hDlg, IDC_CHECK_ERRORNOTIFY);


			Proxy = IsDlgButtonChecked(hDlg, IDC_CHECK_PROXY);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PSERVER), WM_GETTEXT, BUFSIZE - 1, (LPARAM)pServer);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PPORT), WM_GETTEXT, PORTSIZE, (LPARAM)pPort);

			pNoCache = IsDlgButtonChecked(hDlg, IDC_CHECK_NOCACHE);

			pUsePass = IsDlgButtonChecked(hDlg, IDC_CHECK_USEPASS);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_USER), WM_GETTEXT, BUFSIZE - 1, (LPARAM)pUser);
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_PASS), WM_GETTEXT, BUFSIZE - 1, (LPARAM)pPass);

			wsprintf(buf, "%ld", TimeOut);
			WritePrivateProfileString("CHECK", "TimeOut", buf, app_path);
			wsprintf(buf, "%ld", CheckType);
			WritePrivateProfileString("CHECK", "CheckType", buf, app_path);
			wsprintf(buf, "%ld", ErrorNotify);
			WritePrivateProfileString("CHECK", "ErrorNotify", buf, app_path);

			wsprintf(buf, "%ld", Proxy);
			WritePrivateProfileString("PROXY", "Proxy", buf, app_path);
			WritePrivateProfileString("PROXY", "pServer", pServer, app_path);
			WritePrivateProfileString("PROXY", "pPort", pPort, app_path);

			wsprintf(buf, "%ld", pNoCache);
			WritePrivateProfileString("PROXY", "pNoCache", buf, app_path);

			wsprintf(buf, "%ld", pUsePass);
			WritePrivateProfileString("PROXY", "pUsePass", buf, app_path);
			WritePrivateProfileString("PROXY", "pUser", pUser, app_path);
			ePass(pPass, buf);
			WritePrivateProfileString("PROXY", "pPass", buf, app_path);
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


// �t�B�[�h�^�u
static BOOL CALLBACK ProtocolPropertyFeedProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char buf[BUFSIZE];

	switch ( uMsg ) {
		case WM_INITDIALOG:
			if ( InfoToDate ) {
				CheckDlgButton(hDlg, IDC_CHECK_INFOTODATE, TRUE);
			}
			if ( InfoToTitle ) {
				CheckDlgButton(hDlg, IDC_CHECK_INFOTOTITLE, TRUE);
			}
			if ( DoubleDecodeAndAmp ) {
				CheckDlgButton(hDlg, IDC_CHECK_DOUBLEDECODEANDAMP, TRUE);
			}
			if ( EachNotify ) {
				CheckDlgButton(hDlg, IDC_CHECK_EACHNOTIFY, TRUE);
			}
			break;

		case WM_NOTIFY:
			return OptionNotifyProc(hDlg, uMsg, wParam, lParam);
			break;

		case WM_COMMAND:
			switch( wParam ) {
				case IDOK:
					InfoToDate = IsDlgButtonChecked(hDlg, IDC_CHECK_INFOTODATE);
					_itoa_s(InfoToDate, buf, BUFSIZE, 10);
					WritePrivateProfileString("FEED", "InfoToDate", buf, app_path);

					InfoToTitle = IsDlgButtonChecked(hDlg, IDC_CHECK_INFOTOTITLE);
					_itoa_s(InfoToTitle, buf, BUFSIZE, 10);
					WritePrivateProfileString("FEED", "InfoToTitle", buf, app_path);

					DoubleDecodeAndAmp = IsDlgButtonChecked(hDlg, IDC_CHECK_DOUBLEDECODEANDAMP);
					_itoa_s(DoubleDecodeAndAmp, buf, BUFSIZE, 10);
					WritePrivateProfileString("FEED", "DoubleDecodeAndAmp", buf, app_path);

					EachNotify = IsDlgButtonChecked(hDlg, IDC_CHECK_EACHNOTIFY);
					_itoa_s(EachNotify, buf, BUFSIZE, 10);
					WritePrivateProfileString("FEED", "EachNotify", buf, app_path);
					break;
			}
			break;

		default:
			return FALSE;
	}
	return TRUE;
}



/******************************************************************************

	AddAppNameProc

	�A�v���P�[�V�������̒ǉ��_�C�A���O

******************************************************************************/

static BOOL CALLBACK AddAppNameProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char *buf;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		if((char *)lParam == NULL){
			EndDialog(hDlg, FALSE);
			break;
		}
		SetWindowLong(hDlg, GWL_USERDATA, lParam);
		SendMessage(GetDlgItem(hDlg, IDC_EDIT_APPNAME), WM_SETTEXT, 0, lParam);
		break;

	case WM_CLOSE:
		EndDialog(hDlg, FALSE);
		break;

	case WM_COMMAND:
		switch(wParam)
		{
		case IDOK:
			buf = (char *)GetWindowLong(hDlg, GWL_USERDATA);
			if(buf == NULL){
				EndDialog(hDlg, FALSE);
				break;
			}
			SendMessage(GetDlgItem(hDlg, IDC_EDIT_APPNAME), WM_GETTEXT, BUFSIZE - 1, (LPARAM)buf);
			EndDialog(hDlg, TRUE);
			break;

		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			break;
		}
		break;

	default:
		return FALSE;
	}
	return TRUE;
}


/******************************************************************************

	SetDDEAppProc

	DDE�̃A�v���P�[�V��������o�^����_�C�A���O

******************************************************************************/

static BOOL CALLBACK SetDDEAppProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	char buf[BUFSIZE];
	int i;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		for(i = 0; i < AppCnt; i++){
			if(*(AppName[i]) != '\0'){
				SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_ADDSTRING, 0, (LPARAM)AppName[i]);
			}
		}
		ShowWindow(GetDlgItem(hDlg, IDOK), SW_HIDE);
		ShowWindow(GetDlgItem(hDlg, IDCANCEL), SW_HIDE);
		break;

	case WM_NOTIFY:
		return OptionNotifyProc(hDlg, uMsg, wParam, lParam);

	case WM_COMMAND:
		switch(wParam)
		{
		case IDC_BUTTON_ADD:
			if(SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETCOUNT, 0, 0) >= 30){
				break;
			}
			*buf = '\0';
			if(DialogBoxParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_ADDAPPNAME), hDlg, AddAppNameProc, (LPARAM)buf) == FALSE){
				break;
			}
			if(*buf == '\0'){
				break;
			}
			i = SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_ADDSTRING, 0, (LPARAM)buf);
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_SETCURSEL, i, 0);
			break;

		case IDC_BUTTON_EDIT:
			i = SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETCURSEL, 0, 0);
			if(i < 0){
				break;
			}
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETTEXT, i, (LPARAM)buf);
			if(DialogBoxParam(ghinst, MAKEINTRESOURCE(IDD_DIALOG_ADDAPPNAME), hDlg, AddAppNameProc, (LPARAM)buf) == FALSE){
				break;
			}
			if(*buf == '\0'){
				break;
			}
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_DELETESTRING, i, 0);
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_INSERTSTRING, i, (LPARAM)buf);
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_SETCURSEL, i, 0);
			break;

		case IDC_BUTTON_DELETE:
			i = SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETCURSEL, 0, 0);
			if(i < 0 || MessageBox(hDlg, STR_Q_MSG_DEL, STR_Q_TITLE_DEL, MB_ICONQUESTION | MB_YESNO) == IDNO){
				break;
			}
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_DELETESTRING, i, 0);
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_SETCURSEL, i, 0);
			break;

		case IDC_BUTTON_UP:
			i = SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETCURSEL, 0, 0);
			if(i <= 0){
				break;
			}
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETTEXT, i, (LPARAM)buf);
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_DELETESTRING, i, 0);
			i--;
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_INSERTSTRING, i, (LPARAM)buf);
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_SETCURSEL, i, 0);
			break;

		case IDC_BUTTON_DOWN:
			i = SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETCURSEL, 0, 0);
			if(i < 0 || i >= SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETCOUNT, 0, 0) - 1){
				break;
			}
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETTEXT, i, (LPARAM)buf);
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_DELETESTRING, i, 0);
			i++;
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_INSERTSTRING, i, (LPARAM)buf);
			SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_SETCURSEL, i, 0);
			break;

		case IDOK:
			AppCnt = SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETCOUNT, 0, 0);
			if(AppCnt > 30) AppCnt = 30;

			WritePrivateProfileString("DDE", NULL, NULL, app_path);

			wsprintf(buf, "%ld",AppCnt);
			WritePrivateProfileString("DDE", "AppCnt", buf, app_path);

			for(i = 0; i < AppCnt; i++){
				SendMessage(GetDlgItem(hDlg, IDC_LIST_APP), LB_GETTEXT, i, (LPARAM)AppName[i]);
				wsprintf(buf, "AppName_%d", i);
				WritePrivateProfileString("DDE", buf, AppName[i], app_path);
			}
			EndDialog(hDlg, TRUE);
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}


/******************************************************************************

	ViewProtocolProperties

	�v���g�R���̃v���p�e�B�\��

******************************************************************************/

__declspec(dllexport) int CALLBACK HTTP_ProtocolProperty(HWND hWnd)
{
#define sizeof_PROPSHEETHEADER		40	//�Â��R�����R���g���[���΍�
	PROPSHEETPAGE psp;
	PROPSHEETHEADER psh;
	HPROPSHEETPAGE hpsp[3];

	psp.dwSize = sizeof(PROPSHEETPAGE);
	psp.dwFlags = PSP_DEFAULT;
	psp.hInstance = ghinst;

	psp.pszTemplate = MAKEINTRESOURCE(IDD_DIALOG_PROTOCOLPROP_CHECK);
	psp.pfnDlgProc = ProtocolPropertyCheckProc;
	hpsp[0] = CreatePropertySheetPage(&psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_DIALOG_SETDDEAPP);
	psp.pfnDlgProc = SetDDEAppProc;
	hpsp[1] = CreatePropertySheetPage(&psp);

	psp.pszTemplate = MAKEINTRESOURCE(IDD_DIALOG_PROTOCOLPROP_FEED);
	psp.pfnDlgProc = ProtocolPropertyFeedProc;
	hpsp[2] = CreatePropertySheetPage(&psp);

	ZeroMemory(&psh, sizeof(PROPSHEETHEADER));
	psh.dwSize = sizeof_PROPSHEETHEADER;
	psh.dwFlags = PSH_NOAPPLYNOW;
	psh.hInstance = ghinst;
	psh.hwndParent = hWnd;
	psh.pszCaption = STR_TITLE_SETHTTP;
	psh.nPages = sizeof(hpsp) / sizeof(HPROPSHEETPAGE);
	psh.phpage = hpsp;
	psh.nStartPage = 0;

	PropRet = -1;
	PropertySheet(&psh);
	return 1;
}
/* End of source */
