#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <time.h>

#include "wwwcdll.h"
#include "String.h"
#include "http.h"
#include "feed.h"
#include "regexp.h"
#include "def.h"
#include "oniguruma.h"


extern BOOL InfoToDate;
extern BOOL InfoToTitle;
extern BOOL DoubleDecodeAndAmp;
extern BOOL EachNotify;



// XML�̓��ꕶ�������v���[���e�L�X�g�ɕϊ�
// String.c / ConvHTMLChar ���
void ConvXMLChar(char *buf, int *Len) {
	char *r, *t, *s, *AfterCharP;
	char AfterChar = '\0';
	wchar_t UnicodeNum;
	char Sjis[2] = {'\0', '\0'};

	AfterCharP = buf + *Len;
	AfterChar = *AfterCharP;
	*AfterCharP = '\0';

	r = t = buf;

	while(*r != '\0'){
		if(IsDBCSLeadByte((BYTE)*r) == TRUE){
			*(t++) = *(r++);
			*(t++) = *(r++);
			continue;
		}
		switch(*r)
		{
		case '\r':
		case '\n':
		case '\t':
		case ' ':
			if ( *(t - 1) != ' ' && t != buf) {
				// �X�y�[�X�͘A�������Ȃ�
				*(t++) = ' ';
			}
			r++;
			break;

		case '&':
			r++;
			if(lstrcmpni(r, "amp;", lstrlen("amp;")) == 0){
				r += lstrlen("amp;");
				if ( DoubleDecodeAndAmp ) {
						// &amp;amp; �݂�����2�d�G�X�P�[�v��߂�
						*(--r) = '&';
						continue;
				} else {
					*(t++) = '&';
				}
			}else if(lstrcmpni(r, "quot;", lstrlen("quot;")) == 0){
				r += lstrlen("quot;");
				*(t++) = '"';
			}else if(lstrcmpni(r, "lt;", lstrlen("lt;")) == 0){
				r += lstrlen("lt;");
				*(t++) = '<';
			}else if(lstrcmpni(r, "gt;", lstrlen("gt;")) == 0){
				r += lstrlen("gt;");
				*(t++) = '>';
			}else if(lstrcmpni(r, "nbsp;", lstrlen("nbsp;")) == 0){
				r += lstrlen("nbsp;");
				if ( *(t - 1) != ' ' && t != buf) {
					*(t++) = ' ';
				}
			}else if(lstrcmpni(r, "reg;", lstrlen("reg;")) == 0){
				r += lstrlen("reg;");
				*(t++) = 'R';
			}else if(lstrcmpni(r, "copy;", lstrlen("copy;")) == 0){
				r += lstrlen("copy;");
				*(t++) = 'c';
			}else if(*r == '#'){
				if ( *(r + 1) == 'x' ) {
					// 16�i
					UnicodeNum = (wchar_t)strtol(r + 2, &s, 16);
					if ( *s != ';' ) {
						*(t++) = *(r - 1);
						break;
					}
				} else {
					for(s = r + 1; *s >= '0' && *s <= '9'; s++);
					if(*s != ';'){
						*(t++) = *(r - 1);
						break;
					}else{
						UnicodeNum = atoi(r + 1);
					}
				}
				// Unicode�ԍ���Shift_JIS��
				if ( UnicodeNum <= 0x20 || UnicodeNum == 0x7f ) {
					if ( *(t - 1) != ' ' && t != buf) {
						*(t++) = ' ';
					}
				} else if ( UnicodeNum < 0x7f ) {
					*(t++) = (char)UnicodeNum;
				} else if ( UnicodeNum == 0x301C ) { // �`
					*(t++) = '\x81';
					*(t++) = '\x60';
				} else {
					WideCharToMultiByte(CP_ACP, 0, &UnicodeNum, 1, Sjis, 2, NULL, NULL);
					*t++ = *Sjis;
					*Sjis = '\0';
					if ( *(Sjis + 1) != '\0' ) {
						*t++ = *(Sjis + 1);
						*(Sjis + 1) = '\0';
					}
				}
				r = s + 1;
			}else{
				*(t++) = *(r - 1);
			}
			break;

		case '<' :
			if ( lstrcmpni(r + 1, "![CDATA[", lstrlen("![CDATA[")) == 0 ) {
				r += lstrlen("![CDATA[") + 1;

			} else {
				while ( *r != '\0' && *r != '>' ) r++;
				if ( *(t - 1) != ' ' && t != buf) {
					*(t++) = ' ';
				}
			}
			break;

		case '>' :
			r++;
			break;

		case ']' :
			if ( lstrcmpni(r + 1, "]>", lstrlen("]>")) == 0 ) {
				r += lstrlen("]>") +1 ;
			} else {
				*(t++) = *(r++);
			}
			break;
		default:
			*(t++) = *(r++);
		}
	}

	// �����̃X�y�[�X�𖳎�
	while ( *(t - 1) == ' ' ) {
		t--;
	}

	*AfterCharP = AfterChar;

	*Len = (int)(t - buf);
}



// ���t�������Unix�b��
time_t DateToUnixTime(LPTSTR Date, int DateLen) {
	struct tm tmDate;
	int i = 0;
	long TzSec = 0;
	time_t RetTime;
	LPTSTR AfterCharP = NULL;
	TCHAR AfterChar = _T('\0');

	if ( Date == NULL || DateLen == 0 ) {
		return 0;
	}

	if ( DateLen > 0 ) {
		AfterCharP = Date + DateLen;
		AfterChar = *AfterCharP;
		*AfterCharP = _T('\0');
	}

	// �X�y�[�X
//	while ( *Date == _T(' ') || *Date == _T('\t') || *Date == _T('\r') || *Date == _T('\n') ) {
//		 Date++;
//	}

	// �����̌����͕s��Ƃ���

	// �N
	tmDate.tm_year = atoi(Date) - 1900;
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// ��
	tmDate.tm_mon = atoi(Date) - 1;
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// ��
	tmDate.tm_mday = atoi(Date);
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// ��
	tmDate.tm_hour = atoi(Date);
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// ��
	tmDate.tm_min = atoi(Date);
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// �b
	tmDate.tm_sec = atoi(Date);
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );

	// ���n���ԂƂ���UTC��UNIX�b�ɕϊ������
	RetTime = mktime(&tmDate);

	// ���n���Ԃ����UTC�̍��̕b��
	// mktime�̌ザ��Ȃ��Ǝg���Ȃ��H
	_get_timezone(&TzSec); 

	// ���t�̎��ԑ�
	if ( *Date == _T('+') 
				&& *(Date + 1) != _T('\0')
				&& *(Date + 2) != _T('\0')
				&& *(Date + 3) != _T('\0')
				&& *(Date + 4) != _T('\0')
				&& *(Date + 5) != _T('\0')
			) {
		TzSec += (*(++Date) - 48) * 36000;
		TzSec += (*(++Date) - 48) * 3600;
		Date++;
		TzSec += (*(++Date) - 48) * 600;
		TzSec += (*(++Date) - 48) * 60;
	} else if ( *Date == _T('-')
				&& *(Date + 1) != _T('\0')
				&& *(Date + 2) != _T('\0')
				&& *(Date + 3) != _T('\0')
				&& *(Date + 4) != _T('\0')
				&& *(Date + 5) != _T('\0')
			) {
		TzSec -= (*(++Date) - 48) * 36000;
		TzSec -= (*(++Date) - 48) * 3600;
		Date++;
		TzSec -= (*(++Date) - 48) * 600;
		TzSec -= (*(++Date) - 48) * 60;
	} else if ( *Date != _T('Z') ) {
		// ���ԑт炵�����̂��Ȃ��̂��̂͌��n���Ԃ�
		TzSec = 0;
	}

	// ���n���ԂƂ���Ă��܂���������������
	RetTime -= TzSec;

	if ( AfterChar != _T('\0') ) {
		*AfterCharP = AfterChar;
	}
	return  RetTime;
}



// �{����2822�H
time_t Date822ToUnixTime(LPTSTR Date, int DateLen) {
	struct tm tmDate;
	int i;
	long TzSec = 0;
	time_t RetTime;
	LPTSTR AfterCharP = NULL;
	TCHAR AfterChar;

	if ( DateLen == 0 ) {
		return 0;
	}

	if ( DateLen > 0 ) {
		AfterCharP = Date + DateLen;
		AfterChar = *AfterCharP;
		*AfterCharP = _T('\0');
	}

	// �X�y�[�X
//	while ( *Date == _T(' ') || *Date == _T('\t') || *Date == _T('\r') || *Date == _T('\n') ) {
//		 Date++;
//	}

	// �����̌����͕s��Ƃ���

	// �j��
	for ( i = 0; *Date != _T('\0') && i < 5;  Date++, i++);

	// ��
	tmDate.tm_mday = atoi(Date);
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// ��
	// Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec
	switch ( *Date ) {
		case _T('F') : tmDate.tm_mon = 1; break;
		case _T('S') : tmDate.tm_mon = 8; break;
		case _T('O') : tmDate.tm_mon = 9; break;
		case _T('N') : tmDate.tm_mon = 10; break;
		case _T('D') : tmDate.tm_mon = 11; break;
		case _T('J') : 
			if ( *(Date + 1) == _T('u')) {
				if ( *(Date + 2) == _T('n') ) {
					tmDate.tm_mon = 5;
					break;
				}
				if ( *(Date + 2) == _T('l') ) {
					tmDate.tm_mon = 6;
					break;
				}
			}
			tmDate.tm_mon = 0;
			break;
		case _T('M') :
			if ( *(Date + 2) == _T('r') ) {
				tmDate.tm_mon = 2;
				break;
			}
			tmDate.tm_mon = 4;
			break;
		case _T('A') :
			if ( *(Date + 1) == _T('p') ) {
				tmDate.tm_mon = 3;
				break;
			}
			tmDate.tm_mon = 7;
			break;
		default : tmDate.tm_mon = 0;
	}
	for ( i = 0; *Date != _T('\0') && i < 4;  Date++, i++);

	// �N
	tmDate.tm_year = atoi(Date) - 1900;
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// ��
	tmDate.tm_hour = atoi(Date);
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// ��
	tmDate.tm_min = atoi(Date);
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// �b
	tmDate.tm_sec = atoi(Date);
	for ( ; *Date >= _T('0') && *Date <= _T('9'); Date++ );
	if ( *Date != _T('\0') ) Date++;

	// ���n���ԂƂ���UTC��UNIX�b�ɕϊ������
	RetTime = mktime(&tmDate);

	// ���n���Ԃ����UTC�̍��̕b��
	// mktime�̌ザ��Ȃ��Ǝg���Ȃ��H
	_get_timezone(&TzSec); 

	// ���t�̎��ԑ�

	if ( *Date == _T('+')
				&& *(Date + 1) != _T('\0')
				&& *(Date + 2) != _T('\0')
				&& *(Date + 3) != _T('\0')
				&& *(Date + 4) != _T('\0')
			) {
		TzSec += (*(++Date) - 48) * 36000;
		TzSec += (*(++Date) - 48) * 3600;
		TzSec += (*(++Date) - 48) * 600;
		TzSec += (*(++Date) - 48) * 60;
	} else if ( *Date == _T('-')
				&& *(Date + 1) != _T('\0')
				&& *(Date + 2) != _T('\0')
				&& *(Date + 3) != _T('\0')
				&& *(Date + 4) != _T('\0')
			) {
		TzSec -= (*(++Date) - 48) * 36000;
		TzSec -= (*(++Date) - 48) * 3600;
		TzSec -= (*(++Date) - 48) * 600;
		TzSec -= (*(++Date) - 48) * 60;
	} else if ( ( *Date == _T('G') && *(Date + 1) == _T('M') && *(Date + 2) == _T('T') )
				|| ( *Date == _T('U') && *(Date + 1) == _T('T') && *(Date + 2) == _T('C') )
				|| ( *Date == _T('Z') && *(Date + 1) == _T('\0') )
			){
		// UTC�͉������Ȃ�
	} else {
		// ���ԑт炵�����̂��Ȃ��̂��̂͌��n���ԂƂ݂Ȃ�
		TzSec = 0;
	}

	// ���n���ԂƂ���Ă��܂���������������
	RetTime -= TzSec;

	if ( AfterChar != _T('\0') ) {
		*AfterCharP = AfterChar;
	}
	return  RetTime;
}



// �����񂩂瑮���l�̊J�n�����|�C���^�ƕ������𓾂�
// �����l���܂߁A������ "name=value" �Ɍ딚����
LPTSTR GetAttPtr(IN LPTSTR Doc, IN int DocLen, LPCTSTR AttName, OUT int *AttLen) {
	LPTSTR DocTemp, Att, Ret = NULL;
	int AttNameLen, ValueLen;
	TCHAR AfterChar = _T('\0'), EndOfValue;

	// ���e���Ȃ�
	if ( Doc == NULL || DocLen == 0 ) {
		return NULL;
	}

	// �w��͈͂ɏI�[��t���Ă���
	if ( DocLen > 0 ) {
		AfterChar = *(Doc + DocLen);
		*(Doc + DocLen) = _T('\0');
	}

	AttNameLen = lstrlen(AttName);

	// ������T��
	DocTemp = Doc;
	while ( Att = _tcsstr(DocTemp, AttName) ) {
		// �����������ʑ�������
		switch ( *(Att - 1) ) {
			case _T(' ') :
			case _T('\t') :
			case _T('\r') :
			case _T('\n') : break;
			default :
				DocTemp = Att + AttNameLen;
				continue;
		}

		// �擪�������ʑ�������
		Att += AttNameLen;
		switch ( *Att ) {
			case _T('>') :
			case _T('/') : goto RET;
			case _T('=') : break;
			case _T(' ') :
			case _T('\t') :
			case _T('\r') :
			case _T('\n') :
				// �X�y�[�X
				while ( *Att == _T(' ') || *Att == _T('\t') || *Att == _T('\r') || *Att == _T('\n') ) {
					Att++;
				}
				if ( *Att == _T('=') ) {
					break;
				}
				// ����
			default :
				DocTemp = Att;
				continue;
		}
		break;
	}
	if ( Att == NULL ) goto RET;

	// �l�܂Ői�߂�
	// =
	Att++;
	// �X�y�[�X
	while ( *Att == _T(' ') || *Att == _T('\t') || *Att == _T('\r') || *Att == _T('\n') ) {
		Att++;
	}

	// ���p��
	switch ( *Att ) {
		case _T('"') :
			EndOfValue = _T('"');
			Att++;
			break;
		case _T('\'') :
			EndOfValue = _T('\'');
			Att++;
			break;
		default :
			EndOfValue = _T(' ');
			break;
	}

	// �Ō�܂Ő����Ȃ���i�߂�
	for ( ValueLen = 0; *(Att + ValueLen) != EndOfValue && *(Att + ValueLen) != _T('>') &&*(Att + ValueLen) != _T('\0'); ValueLen++ );

	// ����
	if ( ValueLen ) {
		if ( AttLen != NULL ) {
			*AttLen = ValueLen;
		}
		Ret = Att;
	}

	// �I�[�Ɋ���������߂��ĕԂ�
	RET : {
		if ( AfterChar != _T('\0') ) {
			*(Doc + DocLen) = AfterChar;
		}
		return Ret;
	}
}



// �����񂩂�^�O�̊J�n�����|�C���^�ƕ������𓾂�
LPTSTR GetTagPtr(IN LPTSTR Doc, IN int DocLen, IN LPCTSTR TagName, BOOL isETag, OUT int *TagLen) {
	LPTSTR Tag, TagO, TagC, DocTemp, Ret = NULL;
	TCHAR AfterChar = _T('\0');
	int TagNameLen;

	// ���e���Ȃ�
	if ( Doc == NULL || DocLen == 0 ) {
		return NULL;
	}

	// �w��͈͂ɏI�[��t���Ă���
	if ( DocLen > 0 ) {
		AfterChar = *(Doc + DocLen);
		*(Doc + DocLen) = _T('\0');
	}

	// ����������
	if ( isETag ) {
		TagNameLen = lstrlen(TagName) + 2;
		Tag = S_ALLOC_Z(TagNameLen);
		if ( Tag == NULL ) {
			return NULL;
		}
		*Tag = _T('<');
		*(Tag + 1) = _T('/');
		lstrcat(Tag + 2, TagName);
	} else {
		TagNameLen = lstrlen(TagName) + 1;
		Tag = S_ALLOC_Z(TagNameLen);
		if ( Tag == NULL ) {
			return NULL;
		}
		*Tag = _T('<');
		lstrcat(Tag + 1, TagName);
	}

	// �^�O��T��
	DocTemp = Doc;
	while ( TagO = _tcsstr(DocTemp, Tag) ) {
		// �擪�������ʃ^�O����
		switch ( *(TagO + TagNameLen) ) {
			case _T('>') :
			case _T('/') :
			case _T(' ') :
			case _T('\t') :
			case _T('\r') :
			case _T('\n') : break;
			default :
				DocTemp = TagO + TagNameLen;
				continue;
		}
		break;
	}
	M_FREE(Tag);
	if ( TagO == NULL ) goto RET;

	// �^�O�̏I����؂�q ">" ��T��
	TagC = _tcschr(TagO, _T('>'));
	if ( TagC == NULL ) goto RET;

	// ���ʂ�Ԃ�
	if ( TagLen != NULL ) {
		*TagLen = (int)(TagC - TagO) + 1;
	}
	Ret = TagO;

	// �I�[�Ɋ���������߂��ĕԂ�
	RET : {
		if ( AfterChar != _T('\0') ) {
			*(Doc + DocLen) = AfterChar;
		}
		return Ret;
	}
}



// �����񂩂�v�f�̊e�����|�C���^�ƕ������𓾂�
LPTSTR GetElemPtr(IN LPTSTR Doc, IN int DocLen, IN LPCTSTR ElemName, OUT ELEMINFO *ElemInfo) {
	LPTSTR STag, ETag;
	int STagLen, ETagLen;

	// 0����
	memset(ElemInfo, 0, sizeof(ELEMINFO));

	// ���e���Ȃ�
	if ( Doc == NULL || DocLen == 0 ) {
		return NULL;
	}

	// �J�n�^�O�𓾂�
	STag = GetTagPtr(Doc, DocLen, ElemName, FALSE, &STagLen);
	if ( STag == NULL ) {
		return NULL;
	}

	// �J�n�^�O�̌ォ��2�����ڂŋ�v�f��
	if ( *(STag + STagLen - 2) == _T('/') ) {
		ElemInfo->STag = STag;
		ElemInfo->ElemLen = STagLen;
		ElemInfo->STagLen = STagLen;
		return STag;
	}

	// �I���^�O�𓾂�
	if ( DocLen > 0 ) {
		DocLen -= (int)(STag - Doc) - STagLen;
	}
	ETag = GetTagPtr(STag + STagLen, DocLen, ElemName, TRUE, &ETagLen);
	if ( ETag == NULL ) {
		return NULL;
	}

	// ���ʂ�Ԃ�
	ElemInfo->STag = STag;
	ElemInfo->STagLen = STagLen;
	ElemInfo->ETag = ETag;
	ElemInfo->ETagLen = ETagLen;
	ElemInfo->Cont = STag + STagLen;
	ElemInfo->ContLen = (int)(ETag - STag) - STagLen;
	ElemInfo->ElemLen = (int)(ETag - STag) + ETagLen;
	return STag;
}



// ������Ɏw��̑����l�����邩
BOOL HasAttValue(LPTSTR Doc, int DocLen, LPTSTR AttName, LPTSTR AttValue) {
	LPTSTR TagAtt;
	int i, AttValueLen;

	// �w��̑����l�𓾂�
	TagAtt = GetAttPtr(Doc, DocLen, AttName, NULL);
	if ( TagAtt == NULL ) {
		return FALSE;
	}

	// �����l���r
	for ( i = 0, AttValueLen = lstrlen(AttValue); i < AttValueLen; i++, TagAtt++, AttValue++ ) {
		if ( *TagAtt != *AttValue ) {
			return FALSE;
		}
	}

	return TRUE;
}



// Atom��URL�̃|�C���^�𓾂�
LPTSTR GetAtomLinkPtr(IN LPTSTR Doc, IN int DocLen, OUT int *LinkLen) {
	LPTSTR Link = NULL, LinkTag;
	int LinkTagLen;
	TCHAR AfterChar = _T('\0');


	// ���e���Ȃ�
	if ( Doc == NULL || DocLen == 0 ) {
		return NULL;
	}

	// �w��͈͂ɏI�[��t���Ă���
	if ( DocLen > 0 ) {
		AfterChar = *(Doc + DocLen);
		*(Doc + DocLen) = _T('\0');
	}

	// �Ƃ肠�����ŏ���link
	LinkTag = GetTagPtr(Doc, -1, _T("link"), FALSE, &LinkTagLen);
	if ( LinkTag == NULL ) {
		goto RET;
	}

	Link = GetAttPtr(LinkTag, LinkTagLen, _T("href"), LinkLen);

	// alternate ����Ȃ���Α����T���A����������΍ŏ��̂��g��
	if( HasAttValue(LinkTag, LinkTagLen, _T("rel"), _T("alternate")) == FALSE ) {
		while ( (LinkTag = GetTagPtr(LinkTag + LinkTagLen, -1, _T("link"), FALSE, &LinkTagLen)) != NULL ) {
			if ( HasAttValue(LinkTag, LinkTagLen, _T("rel"), _T("alternate")) == TRUE ) {
				Link = GetAttPtr(LinkTag, LinkTagLen, _T("href"), LinkLen);
				break;
			}
		}
	}

	// �I�[�Ɋ���������߂��ĕԂ�
	RET : {
		if ( AfterChar != _T('\0') ) {
			*(Doc + DocLen) = AfterChar;
		}
		return Link;
	}
}



// Atom��feed�^�O����BaseUrl�𓾂邩�`�F�b�NURL������
// �����������A���s���� NULL
LPTSTR GetBaseUrl(LPTSTR Body, LPTSTR CheckURL) {
	LPTSTR BaseUrl, BaseUrlPtr; 
	int BaseUrlLen;


	BaseUrlPtr = GetTagPtr(Body, -1, _T("feed"), FALSE, &BaseUrlLen);
	BaseUrlPtr = GetAttPtr(BaseUrlPtr, BaseUrlLen, _T("xml:base"), &BaseUrlLen);
	if ( BaseUrlPtr == NULL ) {
		BaseUrlLen = lstrlen(CheckURL);
		BaseUrl = S_ALLOC(BaseUrlLen);
		if ( BaseUrl == NULL ) return NULL;
		lstrcpy(BaseUrl, CheckURL);
	} else {
		ConvXMLChar(BaseUrlPtr, &BaseUrlLen);
		BaseUrl = S_ALLOC(BaseUrlLen);
		if ( BaseUrl == NULL ) return NULL;
		_tcsncpy_s(BaseUrl, BaseUrlLen + 1, BaseUrlPtr, BaseUrlLen);
	}
	BaseUrlPtr = _tcschr(BaseUrl, _T('#'));
	if ( BaseUrlPtr != NULL ) {
		*BaseUrlPtr = _T('\0');
	}
	BaseUrlPtr = _tcschr(BaseUrl, _T('?'));
	if ( BaseUrlPtr != NULL ) {
		*BaseUrlPtr = _T('\0');
	}
	BaseUrlPtr = _tcsrchr(BaseUrl, _T('/'));
	if ( BaseUrlPtr != NULL ) {
		*(BaseUrlPtr + 1) = _T('\0');
	}

	return BaseUrl;
}



// URI�X�L�[���������Ă��邩
BOOL HasUriScheme(LPCTSTR Uri) {
	while ( *Uri >= _T('a') && *Uri <= _T('z') ) Uri++;
	if ( *Uri == _T(':') ) return TRUE;
	return FALSE;
}



// Atom�̑���URI�Q�Ƃ����S��URI�ɂ���
// �����������Ƃ��͂����Ԃ�l�ɂ��A����ȊO��NULL
LPTSTR MakeFullUri(IN LPTSTR Uri, IN OUT int *UriLen, IN LPTSTR BaseUri, IN int BaseUriLen) {
	LPTSTR FullUri;
	int HostLen = 0;

	if ( HasUriScheme(Uri) ) {
		// ���URI
		return NULL;

	} else if ( *Uri == _T('/') ) {
		// ��΃p�X�A�z�X�g�����̕������𐔂��ĉ�����
		// �X�L�[��
		while ( *(BaseUri + HostLen) >= _T('a') && *(BaseUri + HostLen) <= _T('z') ) {
			HostLen++;
		}
		// :// ���ߑł�
		if ( *(BaseUri + HostLen) == _T(':') && *(BaseUri + HostLen + 1) == _T('/') && *(BaseUri + HostLen + 2) == _T('/') ) {
			HostLen += 3;
		}
		// �h���C��
		while ( *(BaseUri + HostLen) != _T('0') && *(BaseUri + HostLen) != _T('/') ) {
			HostLen++;
		}
		// ���v���ă�����
		*UriLen += HostLen;
		FullUri = S_ALLOC(*UriLen);
		if ( FullUri == NULL ) {
			return NULL;
		}
		_tcsncpy_s(FullUri, *UriLen + 1, BaseUri, HostLen);
		_tcsncat_s(FullUri, *UriLen + 1, Uri, *UriLen - HostLen);

	} else {
		// ���΃p�X�͎蔲��
		if ( *Uri == _T('.') && *(Uri + 1) == _T('/') ) {
			Uri += 2;
			*UriLen -= 2;
		}
		*UriLen += BaseUriLen;
		FullUri = S_ALLOC(*UriLen);
		if ( FullUri == NULL ) {
			return NULL;
		}
		lstrcpy(FullUri, BaseUri);
		_tcsncat_s(FullUri, *UriLen + 1, Uri, *UriLen - BaseUriLen);
	}

	return FullUri;
}



// �A�C�e�������狌�L������������������
int CountItemTitleLen(struct TPITEM *tpItemInfo) {
	int ItemTitleLen, OldInfoLen;
	LPTSTR p;

	ItemTitleLen = ( tpItemInfo->Title != NULL ) ? lstrlen(tpItemInfo->Title) : 0;
	OldInfoLen = ( tpItemInfo->ITEM_UPINFO != NULL ) ? lstrlen(tpItemInfo->ITEM_UPINFO) : 0;
	if ( ItemTitleLen >= OldInfoLen && tpItemInfo->Title != NULL && tpItemInfo->ITEM_UPINFO != NULL ) {
		p = tpItemInfo->Title;
		while ( p = _tcsstr(p, tpItemInfo->ITEM_UPINFO) ) {
			if ( p == NULL || p - tpItemInfo->Title < 3 ) {
				break;
			}
			if ( *(p - 1) == _T(' ')
					&& *(p - 2) == _T('|')
					&& *(p - 3) == _T(' ')
					&& *(p + OldInfoLen) == _T('\0')
			) {
				ItemTitleLen = (int)(p - tpItemInfo->Title) - 3;
				break;
			}
			p++;
		}
	}

	return ItemTitleLen;
}



// �L�������A�C�e�����ɒǉ�
BOOL AddEntryTitle(struct TPITEM *tpItemInfo, LPTSTR UpInfo) {
	LPTSTR NewTitle;
	int ItemTitleLen;

	// �ΏۊO
	if ( InfoToTitle == 0 || GetOptionInt(tpItemInfo->Option1, OP1_META) < 2 ) {
		return FALSE;
	}

	if ( UpInfo == NULL ) {
		UpInfo = _T("");
	}

	ItemTitleLen = CountItemTitleLen(tpItemInfo);

	// �V�A�C�e���������
	NewTitle = S_ALLOC(ItemTitleLen + lstrlen(UpInfo) + 3);
	if ( NewTitle == NULL ) {
		return FALSE;
	}
	_tcsncpy_s(NewTitle, ItemTitleLen + 1, tpItemInfo->Title, ItemTitleLen);
	if ( *UpInfo != _T('\0') ) {
		lstrcat(NewTitle, _T(" | "));
		lstrcat(NewTitle, UpInfo);
	}
	// �V�A�C�e�����ɒu��������
	M_FREE(tpItemInfo->Title);
	tpItemInfo->Title = NewTitle;

	return TRUE;
}



// �t�B�[�h�`�F�b�N�̃I�v�V�����ŐV���̉ۂ����߂�
// TRUE  : ����
// FALSE : ����
BOOL CompareCheckOption(LPTSTR TargetStr, int TargetStrLen, LPTSTR OptionText, LPTSTR OptionName, int OptionTypeNum) {
	int Ret;
	BOOL Match;
	LPTSTR PatternStr;
	regex_t *RegExp;


	switch ( OptionTypeNum ) {
		case 1 : case 3 : Match = TRUE; break; // �I���A��v�����狖��
		case 2 : Match = FALSE; break; // ���O�A��v�����疳��
		default :return TRUE; // �`�F�b�N����
	}

	// �����������
	PatternStr = GetOptionText(OptionText, OptionName, FALSE);
	if ( PatternStr == NULL ) {
		// �ݒ肪�Ȃ��̂͑S����
		return TRUE;
	}

	// ���K�\���I�u�W�F�N�g�����
	Ret = onig_new(&RegExp, (OnigStr)PatternStr, (OnigStr)(PatternStr + lstrlen(PatternStr)),
		_ONIG_OPTION_DEFAULT, _ONIG_ENCODING_DEFAULT, _ONIG_SYNTAX_DEFAULT, NULL);
	if ( Ret != ONIG_NORMAL ) {
		M_FREE(PatternStr);
		onig_free(RegExp);
		return Ret;
	}

	// ����
	Ret = onig_search(RegExp, (OnigStr)TargetStr, (OnigStr)(TargetStr + TargetStrLen),
		(OnigStr)TargetStr, (OnigStr)(TargetStr + TargetStrLen), NULL, 0)
			== ONIG_MISMATCH ? !Match : Match;

	// �����
	M_FREE(PatternStr);
	onig_free(RegExp);
	return Ret;
}



// Links �̒��� Link �����邩
// �O�オ�󔒂������� strstr �Ȋ���
BOOL LinksLink(LPCTSTR Links, LPCTSTR Link, int LinkLen) {
	LPCTSTR LinksTemp = NULL;

	if ( Links == NULL ) {
		return FALSE;
	}

	LinksTemp = Links;
	while ( (LinksTemp = _tcsstr(LinksTemp, Link)) != NULL ) {
		if ( (LinksTemp == Links || *(LinksTemp - 1) == _T(' '))
					&& 	*(LinksTemp + LinkLen) == _T(' ')
		) {
			return TRUE;
		}
		LinksTemp += LinkLen;
	}

	return FALSE;
}



// �X�V�������
// �����������
LPTSTR MakeUpInfo(LPCTSTR EntryTitle, int EntryTitleLen, struct TPITEM *tpItemInfo, int EntryCount) {
	LPTSTR UpInfo;
	int ItemTitleLen;


	UpInfo = S_ALLOC(EntryTitleLen + 10);
	if ( UpInfo == NULL ) {
		return NULL;
	}
	*UpInfo = _T('\0');

	if ( EntryTitle == NULL ) {
		EntryTitle = _T("");
	}

	// �擪���A�C�e�����Ȃ�ȗ�
	ItemTitleLen = CountItemTitleLen(tpItemInfo);
	if ( ItemTitleLen <= EntryTitleLen 
				&& CompareString(LOCALE_USER_DEFAULT, 0, tpItemInfo->Title, ItemTitleLen, EntryTitle, ItemTitleLen)
					== CSTR_EQUAL
	) {
		// �L�����̕��������āA�A�C�e�������������̐擪�������Ȃ�A���̕�����������
		EntryTitle += ItemTitleLen;
		EntryTitleLen -= ItemTitleLen;
		// �ȗ���̐擪�󔒂�����
		if ( *EntryTitle == _T(' ') ) {
			EntryTitle++;
			EntryTitleLen--;
		} else if ( *EntryTitle == '\x81' && *(EntryTitle + 1) == '\x40' ) {
			EntryTitle += 2;
			EntryTitleLen -= 2;
		}
	}

	// �X�V���
	if ( EntryTitleLen > 0 ) {
		_tcsncpy_s(UpInfo, EntryTitleLen + 1, EntryTitle, EntryTitleLen);
		if ( EntryCount > 1 ) {
			*(UpInfo + EntryTitleLen) = _T(' ');
			*(UpInfo + EntryTitleLen + 1) = _T('/');
			*(UpInfo + EntryTitleLen + 2) = _T(' ');
			EntryTitleLen += 3;
		}
	}
	if ( EntryCount > 1 ) {
		_stprintf_s(UpInfo + EntryTitleLen, 8, _T("�S%d��"), EntryCount > 999 ? 999 : EntryCount);
	}

	return UpInfo;
}



// �X�V���������
// �����������
LPTSTR MakeUpDate(time_t NewDate, LPCTSTR UpInfo) {
	LPTSTR UpDate;
	struct tm tmNewDate;

	UpDate = S_ALLOC(InfoToDate ? (UpInfo ==NULL ? 0 : lstrlen(UpInfo)) + 20 : 19);
	if ( UpDate == NULL ) {
		return NULL;
	}

	localtime_s(&tmNewDate, &NewDate);
	_tcsftime(UpDate, 20, _T("%Y/%m/%d %H:%M:%S"), &tmNewDate);
	if ( InfoToDate && UpInfo != NULL ) {
		*(UpDate + 19) = _T(' ');
		lstrcpy(UpDate + 20, UpInfo);
	}

	return UpDate;
}



// UP��ԂȂ����������Ɏc���āA�V��URL�W�������������蒼��
void MakeNewLinks(LPTSTR *NewLinks, int NewLinksLen, struct TPITEM *tpItemInfo) {
	LPTSTR NewLinksTemp;

	// �������Ȃ�
	if ( tpItemInfo->ViewURL == NULL
			|| (tpItemInfo->Status & ST_UP) == 0 ) return;

	NewLinksLen += lstrlen(tpItemInfo->ViewURL);
	NewLinksTemp = S_ALLOC(NewLinksLen);
	if ( NewLinksTemp != NULL ) {
		lstrcpy(NewLinksTemp, *NewLinks);
		lstrcat(NewLinksTemp, tpItemInfo->ViewURL);
		M_FREE(*NewLinks);
		*NewLinks = NewLinksTemp;
	}
}



// �t�B�[�h
int GetEntries(struct TPITEM *tpItemInfo) {
	// WWWC�p
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	int CmpMsg = ST_DEFAULT;

	// XML�p
	DWORD FeedType;
	TCHAR EntryTag[16], DateTag[16], CatTag[16], ContTag[16], DescTag[16];
	LPTSTR Body, Doc, RegStrList;
	ELEMINFO elEntry, elDate, elLink, elTitle, elTemp;

	// Atom�p
	LPTSTR BaseUrl = NULL, LinkTemp = NULL;
	int BaseUrlLen;

	// �L���`�F�b�N�p
	int EntryCount = 0, EntryLinkLen, NewLinksLen = 0, OptionTypeNum,
		EntryTitleLen = 0, EntryTempLen, EntryCatLen;
	time_t OldDate, NewDate, NowDate, EntryDate;
	LPTSTR EntryLink, NewLinks = NULL, NewLinksTemp, UpInfo, EntryTitle = NULL,
		EntryTemp, EntryCat;
	BOOL Pass;
	TCHAR AfterChar;
	int OrFlag = 0;


	// �ʒʒm�p
	struct TPITEM *UpItemInfo = NULL, *NextItemInfo = NULL;
	int ItemTitleLen = 0, ExecFlag;


	// Shift_JIS �ɕϊ�
	Body = SrcConv(tpHTTP->Body, tpHTTP->Size);
	// �\�[�X�u��
	if ( GetOptionNum(tpItemInfo->ITEM_FILTER, FEED_FILTER_TYPE_SOURCE) != 0
				&& (RegStrList = GetOptionText(tpItemInfo->ITEM_FILTER, FEED_FILTER_SOURCE, TRUE))
					!= NULL
	) {
		Doc = ListReplace(Body, RegStrList);
		if ( Doc != NULL ) {
			M_FREE(Body);
			Body = Doc;
			Doc = NULL;
		}
		M_FREE(RegStrList);
	}

	// �t�B�[�h�̌`�����m�F
	if ( GetTagPtr(Body, -1, _T("pubDate"), FALSE, NULL) != NULL ) {
		// RSS 2.0
		FeedType = FEED_TYPE_RSS2;
		_tcscpy_s(EntryTag, _countof(EntryTag), _T("item"));
		_tcscpy_s(DateTag, _countof(DateTag), _T("pubDate"));
		_tcscpy_s(CatTag, _countof(CatTag), _T("category"));
		_tcscpy_s(ContTag, _countof(ContTag), _T("content:encoded"));
		_tcscpy_s(DescTag, _countof(DescTag), _T("description"));

	} else if ( GetTagPtr(Body, -1, _T("updated"), FALSE, NULL) ) {
		// Atom 1.0
		FeedType = FEED_TYPE_ATOM1;
		_tcscpy_s(EntryTag, _countof(EntryTag), _T("entry"));
		_tcscpy_s(DateTag, _countof(DateTag), _T("updated"));
		_tcscpy_s(CatTag, _countof(CatTag), _T("category"));
		_tcscpy_s(ContTag, _countof(ContTag), _T("content"));
		_tcscpy_s(DescTag, _countof(DescTag), _T("summary"));
		// BaseUrl
		BaseUrl = GetBaseUrl(Body, tpItemInfo->CheckURL);
		if ( BaseUrl == NULL ) {
			SetErrorString(tpItemInfo, STR_MEM_ERR, FALSE);
			M_FREE(Body);
			return ST_ERROR;
		}
		BaseUrlLen = lstrlen(BaseUrl);

	} else if ( GetTagPtr(Body, -1, _T("modified"), FALSE, NULL) ) {
		// Atom 0.3
		FeedType = FEED_TYPE_ATOM03;
		_tcscpy_s(EntryTag, _countof(EntryTag), _T("entry"));
		_tcscpy_s(DateTag, _countof(DateTag), _T("modified"));
		_tcscpy_s(CatTag, _countof(CatTag), _T("dc:subject"));
		_tcscpy_s(ContTag, _countof(ContTag), _T("content"));
		_tcscpy_s(DescTag, _countof(DescTag), _T("summary"));
		// BaseUrl
		BaseUrl = GetBaseUrl(Body, tpItemInfo->CheckURL);
		if ( BaseUrl == NULL ) {
			SetErrorString(tpItemInfo, STR_MEM_ERR, FALSE);
			M_FREE(Body);
			return ST_ERROR;
		}
		BaseUrlLen = lstrlen(BaseUrl);

	} else if ( GetTagPtr(Body, -1, _T("dc:date"), FALSE, NULL) ) {
		// RSS 1.0
		// ���ł�dc���g���Ă��邱�Ƃ������̂ōŌ�
		FeedType = FEED_TYPE_RSS1;
		_tcscpy_s(EntryTag, _countof(EntryTag), _T("item"));
		_tcscpy_s(DateTag, _countof(DateTag), _T("dc:date"));
		_tcscpy_s(CatTag, _countof(CatTag), _T("dc:subject"));
		_tcscpy_s(ContTag, _countof(ContTag), _T("content:encoded"));
		_tcscpy_s(DescTag, _countof(DescTag), _T("description"));
	} else {
		// �t�B�[�h����Ȃ�����

		// LINK �v�f����t�B�[�h�� URL ��T��
		EntryLink = Body;
		EntryLinkLen = 0;
		while ( (EntryLink = GetTagPtr(EntryLink + EntryLinkLen, -1, _T("link"), FALSE, &EntryLinkLen)) != NULL ) {
			// �t�B�[�h��
			if ( HasAttValue(EntryLink, EntryLinkLen, _T("type"), _T("application/rss+xml")) 
						|| HasAttValue(EntryLink, EntryLinkLen, _T("type"), _T("application/atom+xml"))
						|| HasAttValue(EntryLink, EntryLinkLen, _T("type"), _T("application/rdf+xml"))
			) {
				// href �����l
				EntryLink = GetAttPtr(EntryLink, EntryLinkLen, _T("href"), &EntryLinkLen);
				if ( EntryLink != NULL ) {
					break;
				} 
			}
		}

		if ( EntryLink == NULL ) {
			// ������ۂ� URL �Ȃ�
			SetErrorString(tpItemInfo, _T("�t�B�[�h�`���s��"), FALSE);
			M_FREE(Body);
			return ST_ERROR;

		} else {
			// ������ۂ� URL �� CheckURL �ɂ��čă��N�G�X�g
			ConvXMLChar(EntryLink, &EntryLinkLen);

			// ���� URL
			if ( HasUriScheme(EntryLink) == FALSE ) {
				BaseUrl = M_ALLOC(M_SIZE(tpItemInfo->CheckURL));
				if ( BaseUrl == NULL ) {
					goto MEM_ERR;
				}
				lstrcpy(BaseUrl, tpItemInfo->CheckURL);
				LinkTemp = _tcschr(BaseUrl, _T('#'));
				if ( LinkTemp != NULL ) {
					*LinkTemp = _T('\0');
				}
				LinkTemp = _tcschr(BaseUrl, _T('?'));
				if ( LinkTemp != NULL ) {
					*LinkTemp = _T('\0');
				}
				LinkTemp = _tcsrchr(BaseUrl, _T('/'));
				if ( LinkTemp != NULL ) {
					*(LinkTemp + 1) = _T('\0');
				}
				LinkTemp = MakeFullUri(EntryLink, &EntryLinkLen, BaseUrl, lstrlen(BaseUrl));
				if ( LinkTemp == NULL ) {
					goto MEM_ERR;
				}
				// CheckURL �ɐݒ�
				M_FREE(tpItemInfo->CheckURL);
				tpItemInfo->CheckURL = LinkTemp;
				M_FREE(BaseUrl);
				M_FREE(Body);
				return CONTINUE_GET;
			}

			// ��� URL
			LinkTemp = S_ALLOC(EntryLinkLen);
			if ( LinkTemp == NULL ) {
				goto MEM_ERR;
			}
			_tcsncpy_s(LinkTemp, EntryLinkLen + 1, EntryLink, EntryLinkLen);
			M_FREE(tpItemInfo->CheckURL);
			tpItemInfo->CheckURL = LinkTemp;
			M_FREE(Body);
			return CONTINUE_GET;
		}
	}


	// �T�C�g�^�C�g���𓾂�
	if ( tpItemInfo->Title == NULL 
				|| *tpItemInfo->Title == _T('\0')
				|| lstrcmp(tpItemInfo->Title, "�V�����A�C�e��") == 0
	) {
		if ( GetElemPtr(Body, -1, _T("title"), &elTemp) != NULL && elTemp.ContLen > 0 ) {
			ConvXMLChar(elTemp.Cont, &elTemp.ContLen);
			EntryTitle = S_ALLOC(elTemp.ContLen);
			if ( EntryTitle == NULL ) {
				goto MEM_ERR;
			}
			_tcsncpy_s(EntryTitle, elTemp.ContLen + 1, elTemp.Cont, elTemp.ContLen);
			M_FREE(tpItemInfo->Title);
			tpItemInfo->Title = EntryTitle;
			EntryTitle = NULL;
		}
	}


	// �T�C�gURL�𓾂�
	// �ŏ���link������딚�����肤�邯�Ǒ債�����ł͂Ȃ�
	if ( tpItemInfo->ITEM_SITEURL == NULL || *tpItemInfo->ITEM_SITEURL == _T('\0') ) {
		M_FREE(tpItemInfo->ITEM_SITEURL);

		if ( FeedType & FEED_TYPE_ATOM ) {
			EntryLink = GetAtomLinkPtr(Body, -1, &EntryLinkLen);
			if ( EntryLink == NULL ) {
				M_FREE(BaseUrl);
				M_FREE(Body);
				return CmpMsg;
			}
			ConvXMLChar(EntryLink, &EntryLinkLen);

			tpItemInfo->ITEM_SITEURL = MakeFullUri(EntryLink, &EntryLinkLen, BaseUrl, BaseUrlLen);
			if ( tpItemInfo->ITEM_SITEURL == NULL ) {
				tpItemInfo->ITEM_SITEURL = S_ALLOC(EntryLinkLen);
				if ( tpItemInfo->ITEM_SITEURL == NULL ) {
					goto MEM_ERR;
				}
				_tcsncpy_s(tpItemInfo->ITEM_SITEURL, EntryLinkLen + 1, EntryLink, EntryLinkLen);
			}

		} else {
			if ( GetElemPtr(Body, -1, _T("link"), &elLink) != NULL && elLink.ContLen > 0 ) {
				ConvXMLChar(elLink.Cont, &elLink.ContLen);
				tpItemInfo->ITEM_SITEURL = S_ALLOC(elLink.ContLen);
				if ( tpItemInfo->ITEM_SITEURL == NULL ) {
					goto MEM_ERR;
				}
				_tcsncpy_s(tpItemInfo->ITEM_SITEURL, elLink.ContLen + 1, elLink.Cont, elLink.ContLen);
			}
		}
	}


	// �L���Ń��[�v
	// �ŏ��̋L�����ŐV���Ƃ͌��ߕt���Ȃ�
	elTitle.ElemLen = 0;

	// ���t���r�p�ɐ��l��
	OldDate = DateToUnixTime(tpItemInfo->Date, -1 );
	time(&NowDate);
	NewDate = OldDate;

	Doc = Body;
	while ( GetElemPtr(Doc, -1, EntryTag, &elEntry) != NULL ) {
		Doc = elEntry.STag + elEntry.ElemLen;

		// �L���̍X�V����
		if ( GetElemPtr(elEntry.Cont, elEntry.ContLen, DateTag, &elDate) == NULL ) {
			continue;
		}
		ConvXMLChar(elDate.Cont, &elDate.ContLen);

		// �t�B�[�h�`���ɔ�������t�`�������e
		if ( *elDate.Cont >= _T('0') && *elDate.Cont <= _T('9') ) {
			EntryDate = DateToUnixTime(elDate.Cont, elDate.ContLen);
		} else {
			EntryDate = Date822ToUnixTime(elDate.Cont, elDate.ContLen);
		}

		// �Â��L���͎̂Ă�
		if ( EntryDate <= OldDate ) {
			continue;
		}

		// �����L���𖳎�
		if ( EntryDate > NowDate ) {
			continue;
		}

		// �ŐV�L�����X�V�����Ƃ��Ă���
		if ( EntryDate > NewDate ) {
			NewDate = EntryDate;
		}

		// �L��URL�����炤
		if ( FeedType & FEED_TYPE_ATOM ) {
			EntryLink = GetAtomLinkPtr(elEntry.Cont, elEntry.ContLen, &EntryLinkLen);
			if ( EntryLink == NULL ) {
				continue;
			}
			ConvXMLChar(EntryLink, &EntryLinkLen);

			M_FREE(LinkTemp);
			LinkTemp = MakeFullUri(EntryLink, &EntryLinkLen, BaseUrl, BaseUrlLen);
			if ( LinkTemp != NULL ) {
				// ����URI�Q�Ƃ�����URI�������
				EntryLink = LinkTemp;
			}

		} else {
			if (
					(
						GetElemPtr(elEntry.Cont, elEntry.ContLen, _T("link"), &elLink) == NULL
						|| elLink.ContLen == 0
					)
					&&
					// RSS2�ł�guid��URI�Ƃ��邱�Ƃ��ł���
					(
						FeedType != FEED_TYPE_RSS2
						|| GetElemPtr(elEntry.Cont, elEntry.ContLen, _T("guid"), &elLink) == NULL
						|| elLink.ContLen == 0
						|| (
							(EntryTemp = GetAttPtr(elLink.STag, elLink.STagLen, _T("isPermaLink"), &EntryTempLen))
							&&
							EntryTempLen == 5
							&&
							_tcsncmp(EntryTemp, _T("false"), 5) == 0
						)
					)
			) {
				continue;
			}
			EntryLink = elLink.Cont;
			EntryLinkLen = elLink.ContLen;
			ConvXMLChar(EntryLink, &EntryLinkLen);
		}

		// ����Ȃ��L�����̂Ă�
		OrFlag = 0;

		// link�Ŏ��
		if ( (OptionTypeNum = GetOptionNum(tpItemInfo->ITEM_FILTER, FEED_FILTER_TYPE_URL)) != 0 ) {
			if ( CompareCheckOption(EntryLink ,EntryLinkLen ,tpItemInfo->ITEM_FILTER, FEED_FILTER_URL, OptionTypeNum) == FALSE ) {
				if ( OptionTypeNum == 3 ) {
					OrFlag = 1;
				} else {
					continue;
				}
			} else if ( OptionTypeNum == 3 ) {
				OrFlag = 2;
			}
		}

		// title�Ŏ��
		if ( (OptionTypeNum = GetOptionNum(tpItemInfo->ITEM_FILTER, FEED_FILTER_TYPE_TITLE)) != 0
				&& ( OptionTypeNum != 3
					|| ( OptionTypeNum == 3 && OrFlag != 2 && ( OrFlag = 1 ) ) )
		) {
			if ( GetElemPtr(elEntry.Cont, elEntry.ContLen, _T("title"), &elTitle) != NULL ) {
				ConvXMLChar(elTitle.Cont, &elTitle.ContLen);
				if ( CompareCheckOption(elTitle.Cont, elTitle.ContLen ,tpItemInfo->ITEM_FILTER, FEED_FILTER_TITLE, OptionTypeNum) == FALSE ) {
					if ( OptionTypeNum != 3 ) {
						continue;
					}
				} else if ( OptionTypeNum == 3 ) {
					OrFlag = 2;
				}
			} else if ( OptionTypeNum == 1 ) {
				continue;
			}
		}

		// category�Ŏ�́A�^�O�͕������肤��
		if ( (OptionTypeNum = GetOptionNum(tpItemInfo->ITEM_FILTER, FEED_FILTER_TYPE_CATEGORY)) != 0
				&& ( OptionTypeNum != 3
					|| ( OptionTypeNum == 3 && OrFlag != 2 && ( OrFlag = 1 ) ) )
		) {
			EntryTemp = elEntry.Cont;
			EntryTempLen = elEntry.ContLen;

			Pass = ( OptionTypeNum == 1 ) ? FALSE : TRUE;

			while ( GetElemPtr(EntryTemp, EntryTempLen, CatTag, &elTemp) != NULL ) {
				EntryTemp = elTemp.STag + elTemp.ElemLen;
				EntryTempLen = elEntry.ContLen - (EntryTemp - elEntry.Cont);

				if ( FeedType == FEED_TYPE_ATOM1 ) {
					EntryCat = GetAttPtr(elTemp.STag, elTemp.STagLen, _T("term"), &EntryCatLen);
					if ( EntryCat == NULL ) {
						continue;
					}
				} else {
					EntryCat = elTemp.Cont;
					EntryCatLen = elTemp.ContLen;
				}

				ConvXMLChar(EntryCat, &EntryCatLen);
				if ( CompareCheckOption(EntryCat, EntryCatLen, tpItemInfo->ITEM_FILTER, FEED_FILTER_CATEGORY, OptionTypeNum) == FALSE ) {
					// ���O�Ȃ璼���ɏI��
					if ( OptionTypeNum == 2 ) {
						Pass = FALSE;
						break;
					} 
				} else {
					// �I���Ȃ璼���ɏI��
					if ( OptionTypeNum == 1 ) {
						Pass = TRUE;
						break;
					}
					if ( OptionTypeNum == 3 ) {
						OrFlag = 2;
						break;
					}
				}
			}

			if ( Pass == FALSE ) {
				continue;
			}
		}

		// content�Ŏ��
		if ( (OptionTypeNum = GetOptionNum(tpItemInfo->ITEM_FILTER, FEED_FILTER_TYPE_CONTENT)) != 0
				&& ( OptionTypeNum != 3
					|| ( OptionTypeNum == 3 && OrFlag != 2 && ( OrFlag = 1 ) ) )
		) {
			if ( GetElemPtr(elEntry.Cont, elEntry.ContLen, ContTag, &elTemp) != NULL ) {
				ConvXMLChar(elTemp.Cont, &elTemp.ContLen);
				if ( CompareCheckOption(elTemp.Cont, elTemp.ContLen ,tpItemInfo->ITEM_FILTER, FEED_FILTER_CONTENT, OptionTypeNum) == FALSE ) {
					if ( OptionTypeNum != 3 ) {
						continue;
					}
				} else if ( OptionTypeNum == 3 ) {
					OrFlag = 2;
				}
			} else if ( OptionTypeNum == 1 ) {
				continue;
			}
		}

		// description�Ŏ��
		if ( (OptionTypeNum = GetOptionNum(tpItemInfo->ITEM_FILTER, FEED_FILTER_TYPE_DESCRIPTION)) != 0
				&& ( OptionTypeNum != 3
					|| ( OptionTypeNum == 3 && OrFlag != 2 && ( OrFlag = 1 ) ) )
		) {
			if ( GetElemPtr(elEntry.Cont, elEntry.ContLen, DescTag, &elTemp) != NULL ) {
				ConvXMLChar(elTemp.Cont, &elTemp.ContLen);
				if ( CompareCheckOption(elTemp.Cont, elTemp.ContLen ,tpItemInfo->ITEM_FILTER, FEED_FILTER_DESCRIPTION, OptionTypeNum) == FALSE ) {
					if ( OptionTypeNum != 3 ) {
						continue;
					}
				} else if ( OptionTypeNum == 3 ) {
					OrFlag = 2;
				}
			} else if ( OptionTypeNum == 1 ) {
				continue;
			}
		}

		// �I�� (OR) �ō��v�Ȃ�
		if ( OrFlag == 1 ) {
			continue;
		}

		// �d��URL
		AfterChar = *(EntryLink + EntryLinkLen);
		*(EntryLink + EntryLinkLen) = _T('\0');
		if ( LinksLink(NewLinks, EntryLink, EntryLinkLen) ) {
			*(EntryLink + EntryLinkLen) = AfterChar;
			continue;
		}
		*(EntryLink + EntryLinkLen) = AfterChar;

		// �ʒm���@
		if ( EachNotify && EntryCount > 0 ) {
			// �ʂɍX�V�ʒm
			// �l���Ȃ��̂͏���Ƃ��āA�X�V���Ȃ�
			if ( tpItemInfo->Date == NULL || *tpItemInfo->Date == _T('\0') ) {
				continue;
			}

			// �ˋ�̃A�C�e�����
			UpItemInfo = M_ALLOC_Z(sizeof(struct TPITEM));
			if ( UpItemInfo == NULL ) goto MEM_ERR;
			UpItemInfo->iSize = sizeof(struct TPITEM);
			UpItemInfo->hItem = 0;

			// �L��URL
			UpItemInfo->CheckURL = S_ALLOC(EntryLinkLen);
			if ( UpItemInfo->CheckURL == NULL ) {
				M_FREE(UpItemInfo);
				goto MEM_ERR;
			}
			_tcsncpy_s(UpItemInfo->CheckURL, EntryLinkLen + 1, EntryLink, EntryLinkLen);

			// �L����
			if ( elTitle.ElemLen == 0 ) {
				if ( GetElemPtr(elEntry.Cont, elEntry.ContLen, _T("title"), &elTitle) != NULL ) {
					ConvXMLChar(elTitle.Cont, &elTitle.ContLen);
				}
				elTitle.ElemLen = 0;
			}
			UpItemInfo->ITEM_UPINFO = elTitle.Cont;
			UpItemInfo->Param1 = elTitle.ContLen;

			// �X�V����ݒ�
			UpItemInfo->Param2 = (long)EntryDate;

			// �t���p
			UpItemInfo->hGetHost1 = NextItemInfo;
			NextItemInfo = UpItemInfo;

			// �V�������𑝂₳�Ȃ�
			continue;

		} else {
			// �܂Ƃ߂ĊJ��URL��
			// �V���L��URL�p������
			NewLinksLen += EntryLinkLen + 1;
			NewLinksTemp = S_ALLOC(NewLinksLen);
			if ( NewLinksTemp == NULL ) {
				goto MEM_ERR;
			}
			if ( NewLinks != NULL ) {
				lstrcpy(NewLinksTemp, NewLinks);
				M_FREE(NewLinks);
			} else {
				*NewLinksTemp = _T('\0');
			}
			NewLinks = NewLinksTemp;
			_tcsncat_s(NewLinks, NewLinksLen + 1, EntryLink, EntryLinkLen);
			*(NewLinks + NewLinksLen - 1) = _T(' ');
			*(NewLinks + NewLinksLen) = _T('\0');

			// �ŏ��̋L�������^�C�g�������炤
			if ( EntryCount == 0 ) {
				if ( elTitle.ElemLen == 0 ) {
					if ( GetElemPtr(elEntry.Cont, elEntry.ContLen, _T("title"), &elTitle) != NULL ) {
						ConvXMLChar(elTitle.Cont, &elTitle.ContLen);
					}
				}
				EntryTitle = elTitle.Cont;
				EntryTitleLen = elTitle.ContLen;
				elTitle.ElemLen = 0;
			}
		}

		// ����
		EntryCount++;
	}


	// �X�V
	if ( EntryCount > 0 ) {
		// �V��URL��ݒ�
		MakeNewLinks(&NewLinks, NewLinksLen, tpItemInfo);
		M_FREE(tpItemInfo->ViewURL);
		tpItemInfo->ViewURL = NewLinks;

		// �X�V�������
		UpInfo = MakeUpInfo(EntryTitle, EntryTitleLen, tpItemInfo, EntryCount);

		// �A�C�e������ݒ�
		AddEntryTitle(tpItemInfo, UpInfo);

		// �X�V����ݒ�
		M_FREE(tpItemInfo->ITEM_UPINFO);
		tpItemInfo->ITEM_UPINFO = UpInfo;

		// �X�V����ݒ�
		// �l���Ȃ��̂͏���Ƃ��āA�X�V���Ȃ�
		if ( tpItemInfo->Date != NULL ) {
			if ( *tpItemInfo->Date != _T('\0') ) {
				CmpMsg = ST_UP;
			}
			M_FREE(tpItemInfo->Date);
		}
		tpItemInfo->Date = MakeUpDate(NewDate, UpInfo);

		// �ʒʒm
		if ( UpItemInfo != NULL ) {
			ExecFlag = GetOptionInt(tpItemInfo->Option2, OP2_EXEC);
			ItemTitleLen = CountItemTitleLen(tpItemInfo);
		}
		while ( UpItemInfo != NULL ) {
			// ���s
			if ( ExecFlag & 1 ) {
				if ( ExecOptionCommand(tpItemInfo, UpItemInfo->CheckURL) == FALSE ) goto MEM_ERR;
			}
			// �X�V�ʒm
			if ( ExecFlag == 3 ) {
				// ���Ȃ�
				M_FREE(UpItemInfo->CheckURL);
				NextItemInfo = UpItemInfo->hGetHost1;
				M_FREE(UpItemInfo);
				UpItemInfo = NextItemInfo;

			} else {
				// �A�C�e�����ƍX�V���
				// �A�C�e�����ƋL�������̃�����������ăA�C�e�������R�s�[
				UpItemInfo->Title = S_ALLOC(ItemTitleLen + (InfoToTitle && UpItemInfo->Param1 ? UpItemInfo->Param1 + 3 : 0));
				if ( UpItemInfo->Title != NULL ) {
					_tcsncpy_s(UpItemInfo->Title, ItemTitleLen + 1, tpItemInfo->Title, ItemTitleLen);
				}

				// �X�V����ݒ�
				UpInfo = MakeUpInfo(UpItemInfo->ITEM_UPINFO, UpItemInfo->Param1, UpItemInfo, 1);
				if ( UpInfo != NULL ) {
					UpItemInfo->ITEM_UPINFO = UpInfo;
					// �A�C�e�����ɉ�����
					if ( InfoToTitle && *UpInfo != _T('\0') && UpItemInfo->Title != NULL ) {
						lstrcat(UpItemInfo->Title, _T(" | "));
						lstrcat(UpItemInfo->Title, UpInfo);
					}
				}

				// �X�V����ݒ�
				UpItemInfo->Date = MakeUpDate(UpItemInfo->Param2, UpInfo);

				// �`�F�b�N��
				UpItemInfo->CheckDate = S_ALLOC(lstrlen(tpItemInfo->CheckDate));
				if ( UpItemInfo->CheckDate != NULL ) {
					lstrcpy(UpItemInfo->CheckDate, tpItemInfo->CheckDate);
				}

				// �������ōX�V��񂪃G���[���Ƃ��ď������̖h�����߁A�t�B�[�h�ݒ��t����
				UpItemInfo->Option1 = S_ALLOC(15);
				if ( UpItemInfo->Option1 != NULL ) {
					lstrcpy(UpItemInfo->Option1, _T("0;;0;;0;;0;;2;;"));
				}

				SendMessage(((struct THARGS *)(tpItemInfo->ITEM_MARK))->hWnd, WM_CHECKEND, (WPARAM)ST_UP, (LPARAM)UpItemInfo);
				UpItemInfo = UpItemInfo->hGetHost1;
			}
		}

	} else if ( tpItemInfo->Date == NULL || *tpItemInfo->Date == _T('\0')) {
		// ����ŋL���Ȃ��Ȃ猻�ݓ���������
		// �������Ȃ��Ǝ�������񈵂��ŐV������������
		M_FREE(tpItemInfo->Date);
		tpItemInfo->Date = MakeUpDate(NowDate, NULL);
	}


END :
	// ���������
	M_FREE(BaseUrl);
	M_FREE(LinkTemp);
	M_FREE(Body);


	return CmpMsg;


	// �G���[
MEM_ERR :
	SetErrorString(tpItemInfo, STR_MEM_ERR, FALSE);
	M_FREE(NewLinks);
	CmpMsg = ST_ERROR;
	goto END;

}





// �����N���o
int GetLinks(struct TPITEM *tpItemInfo) {
	// WWWC�p
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	int CmpMsg = ST_DEFAULT;

	// �����p
	LPTSTR Body, UrlPattern, UrlFormat, InfoPattern, InfoFormat, RangeEnd;
	int Order, BodyLen, StartIndex, Ret;
	BOOL Reverse;
	regex_t *RegExp;
	OnigRegion *Region = NULL;
	OnigErrorInfo ErrInfo;

	// �X�V�`�F�b�N�p
	LPTSTR EntryLink = NULL, NewLinks = NULL, NewLinksTemp = NULL, PageData = NULL,
		PageDataTemp = NULL, EntryTitle = NULL, UpInfo = NULL;
	int	EntryCount = 0, EntryLinkLen, NewLinksLen = 0, PageDataLen = 0, EntryTitleLen = 0;
	time_t NewDate;

	// �ʒʒm�p
	struct TPITEM *UpItemInfo = NULL, *NextItemInfo = NULL;
	int ItemTitleLen = 0, ExecFlag;


	// �ݒ�
	Order = GetOptionNum(tpItemInfo->ITEM_FILTER, DRAW_FILTER_ORDER);
	Reverse = (Order == DRAW_BOTTOM) ? TRUE : FALSE;
	UrlPattern = GetOptionText(tpItemInfo->ITEM_FILTER, DRAW_FILTER_URLP, FALSE);
	if ( UrlPattern == NULL ) {
		SetErrorString(tpItemInfo, _T("URL�����ݒ肪�Ȃ�"), FALSE);
		return ST_ERROR;
	}
	UrlFormat = GetOptionText(tpItemInfo->ITEM_FILTER, DRAW_FILTER_URLF, FALSE);
	if ( UrlFormat == NULL ) {
		SetErrorString(tpItemInfo, _T("URL���`�ݒ肪�Ȃ�"), FALSE);
		M_FREE(UrlPattern);
		return ST_ERROR;
	}
	InfoPattern = GetOptionText(tpItemInfo->ITEM_FILTER, DRAW_FILTER_INFOP, FALSE);
	InfoFormat = GetOptionText(tpItemInfo->ITEM_FILTER, DRAW_FILTER_INFOF, FALSE);

	// Shift_JIS �ɕϊ�
	Body = SrcConv(tpHTTP->Body, tpHTTP->Size);
	BodyLen = lstrlen(Body);

	// ���K�\���I�u�W�F�N�g�Ƃ������
	Ret = onig_new(&RegExp, (OnigStr)UrlPattern, (OnigStr)UrlPattern + lstrlen(UrlPattern),
		_ONIG_OPTION_DEFAULT, _ONIG_ENCODING_DEFAULT, _ONIG_SYNTAX_DEFAULT, &ErrInfo);
	if ( Ret != ONIG_NORMAL ){
		EntryTitle = S_ALLOC(ONIG_MAX_ERROR_MESSAGE_LEN);
		if ( EntryTitle == NULL ) {
			goto MEM_ERR;
		}
		onig_error_code_to_str((OnigStr)EntryTitle, Ret, &ErrInfo);
		SetErrorString(tpItemInfo, EntryTitle, FALSE);
		goto ERR;
	}
	Region = onig_region_new();
	StartIndex = Reverse ? BodyLen : 0;
	RangeEnd = Reverse ? Body : Body + BodyLen;


	//onig_region_free(Region, 0) // ���Ȃ��Ă��������Ă�����ۂ�
	for ( ; onig_search(RegExp, (OnigStr)Body, (OnigStr)(Body + BodyLen),
					(OnigStr)(Body + StartIndex), (OnigStr)RangeEnd, Region, 0)
						!= ONIG_MISMATCH;
				StartIndex = Reverse ? *Region->beg : *Region->end
	) {
		// ���`�㕶����
		EntryLinkLen = CountResultLen(UrlFormat, RegExp, Region);
		if ( EntryLinkLen == 0 ) {
			continue;
		}
		// ������
		M_FREE(EntryLink);
		EntryLink = S_ALLOC(EntryLinkLen);
		if ( EntryLink == NULL ) {
			goto MEM_ERR;
		}
		// �ϊ�
		MakeResultStr(Body, UrlFormat, RegExp, Region, EntryLink);
		ConvXMLChar(EntryLink, &EntryLinkLen);
		*(EntryLink + EntryLinkLen) = _T('\0');

		// ���o�ƏI���̔���
		if ( Order == DRAW_ALL ) {
			// �S��
			// ����̊��o�Ȃ玟��
			if ( LinksLink(PageData, EntryLink, EntryLinkLen) != FALSE ) {
				continue;
			}
			// �y�[�W���ɉ�����
			PageDataLen += EntryLinkLen + 1;
			PageDataTemp = S_ALLOC(PageDataLen);
			if ( PageDataTemp == NULL ) {
				goto MEM_ERR;
			}
			if ( PageData != NULL ) {
				lstrcpy(PageDataTemp, PageData);
				M_FREE(PageData);
			} else {
				*PageDataTemp = _T('\0');
			}
			PageData = PageDataTemp;
			lstrcat(PageData, EntryLink);
			*(PageData + PageDataLen - 1) = _T(' ');
			*(PageData + PageDataLen) = _T('\0');
			// �O��̊��o�Ȃ玟��
			if ( LinksLink(tpItemInfo->ITEM_PAGEDATA, EntryLink, EntryLinkLen) != FALSE ) {
				continue;
			}

		} else {
			//����
			// �O��Ɠ����Ń��[�v���I����
			if ( lstrcmp(EntryLink, tpItemInfo->ITEM_PAGEDATA) == 0 ) {
				break;
			}
			// ����̊��o�Ȃ玟��
			if ( LinksLink(NewLinks, EntryLink, EntryLinkLen) != FALSE ) {
				continue;
			}

// test>
			while ( UpItemInfo != NULL ) {
				if ( lstrcmp(EntryLink, UpItemInfo->CheckURL) == 0 ) {
					EntryLinkLen = 0;
					break;
				}
				UpItemInfo = UpItemInfo->hGetHost1;
			}
			UpItemInfo = NextItemInfo;
			if ( EntryLinkLen == 0 ) {
				continue;
			}
// <test


			// �V��1���ڂ̓y�[�W���ɕۑ�
			if ( PageData == NULL ) {
				PageData = S_ALLOC(EntryLinkLen);
				if ( PageData == NULL ) goto MEM_ERR;
				lstrcpy(PageData, EntryLink);
			}
		}

		// �ʒm���@
		if ( EachNotify && EntryCount > 0 ) {
			// �ʂɒʒm
			// �l���Ȃ��̂͏���Ƃ��āA�X�V���Ȃ�
			if ( tpItemInfo->ITEM_PAGEDATA == NULL || *tpItemInfo->ITEM_PAGEDATA == _T('\0') ) {
				continue;
			}

			// �ˋ�̃A�C�e�����
			UpItemInfo = M_ALLOC_Z(sizeof(struct TPITEM));
			if ( UpItemInfo == NULL ) goto MEM_ERR;
			UpItemInfo->iSize = sizeof(struct TPITEM);
			UpItemInfo->hItem = 0;

			// URL
			UpItemInfo->CheckURL = EntryLink;
			EntryLink = NULL;

			// �L����
			if ( InfoPattern == NULL && InfoFormat != NULL ) {
				UpItemInfo->Param1 = CountResultLen(InfoFormat, RegExp, Region);
				UpItemInfo->ITEM_UPINFO = S_ALLOC(UpItemInfo->Param1);
				if ( UpItemInfo->ITEM_UPINFO == NULL ) {
					M_FREE(UpItemInfo->CheckURL);
					M_FREE(UpItemInfo);
					goto MEM_ERR;
				}
				MakeResultStr(Body, InfoFormat, RegExp, Region, UpItemInfo->ITEM_UPINFO);
				ConvXMLChar(UpItemInfo->ITEM_UPINFO, &UpItemInfo->Param1);
			}

			// �t���p
			UpItemInfo->hGetHost1 = NextItemInfo;
			NextItemInfo = UpItemInfo;

			// �V�������𑝂₳�Ȃ�
			continue;

		} else {
			// �܂Ƃ߂ĊJ��URL��
			// �V���L��URL�p������
			NewLinksLen += EntryLinkLen + 1;
			NewLinksTemp = S_ALLOC(NewLinksLen);
			if ( NewLinksTemp == NULL ) {
				goto MEM_ERR;
			}
			if ( NewLinks != NULL ) {
				lstrcpy(NewLinksTemp, NewLinks);
				M_FREE(NewLinks);
			} else {
				*NewLinksTemp = _T('\0');
			}
			NewLinks = NewLinksTemp;
			_tcsncat_s(NewLinks, NewLinksLen + 1, EntryLink, EntryLinkLen);
			*(NewLinks + NewLinksLen - 1) = _T(' ');
			*(NewLinks + NewLinksLen) = _T('\0');

			// �L����
			if ( EntryCount == 0 && InfoPattern == NULL && InfoFormat != NULL ) {
				EntryTitleLen = CountResultLen(InfoFormat, RegExp, Region);
				EntryTitle = S_ALLOC(EntryTitleLen);
				if ( EntryTitle == NULL ) {
					goto MEM_ERR;
				}
				MakeResultStr(Body, InfoFormat, RegExp, Region, EntryTitle);
				ConvXMLChar(EntryTitle, &EntryTitleLen);
			}
		}

		// ����
		EntryCount++;
	}


	// �X�V
	if ( EntryCount > 0 ) {
		// �V��URL��ݒ�
		MakeNewLinks(&NewLinks, NewLinksLen, tpItemInfo);
		M_FREE(tpItemInfo->ViewURL);
		tpItemInfo->ViewURL = NewLinks;

		// �X�V�������
		if ( InfoPattern != NULL && InfoFormat != NULL ) {
			onig_region_free(Region, 0);
			onig_free(RegExp);

			Ret = onig_new(&RegExp, (OnigStr)InfoPattern, (OnigStr)InfoPattern + lstrlen(InfoPattern),
				_ONIG_OPTION_DEFAULT, _ONIG_ENCODING_DEFAULT, _ONIG_SYNTAX_DEFAULT, &ErrInfo);
			if ( Ret ==  ONIG_NORMAL ) {
				if ( onig_search(RegExp, (OnigStr)Body, (OnigStr)(Body + BodyLen), (OnigStr)Body,
							(OnigStr)(Body + BodyLen), Region, 0) != ONIG_MISMATCH ) {
					EntryTitleLen = CountResultLen(InfoFormat, RegExp, Region);
					EntryTitle = S_ALLOC(EntryTitleLen);
					if ( EntryTitle != NULL ) {
						MakeResultStr(Body, InfoFormat, RegExp, Region, EntryTitle);
						ConvXMLChar(EntryTitle, &EntryTitleLen);
					}
				}
			} else {
				EntryTitle = S_ALLOC(ONIG_MAX_ERROR_MESSAGE_LEN);
				if ( EntryTitle != NULL ) {
					EntryTitleLen = onig_error_code_to_str((OnigStr)EntryTitle, Ret, &ErrInfo);
				}
			}
		}
		UpInfo = MakeUpInfo(EntryTitle, EntryTitleLen, tpItemInfo, EntryCount);

		// �A�C�e������ݒ�
		AddEntryTitle(tpItemInfo, UpInfo);

		// �X�V����ݒ�
		M_FREE(tpItemInfo->ITEM_UPINFO);
		tpItemInfo->ITEM_UPINFO = UpInfo;

		// Last-Modified
		M_FREE(tpItemInfo->DLLData1);
		GetLastModified(tpItemInfo, &tpItemInfo->DLLData1);
	
		// �X�V����ݒ�
		M_FREE(tpItemInfo->Date);
		if ( tpItemInfo->DLLData1 == NULL ) { 
			time(&NewDate);
			tpItemInfo->Date = MakeUpDate(NewDate, UpInfo);
		} else {
			tpItemInfo->Date = S_ALLOC(InfoToDate ? (UpInfo == NULL ? 0 : lstrlen(UpInfo)) + 20 : 19);
			if ( tpItemInfo->Date != NULL ) {
				DateConv(tpItemInfo->DLLData1, tpItemInfo->Date);
				if ( InfoToDate ) {
					lstrcat(tpItemInfo->Date, _T(" "));
					lstrcat(tpItemInfo->Date, UpInfo);
				}
			}
		}


		// ����̔�r�p�Ƀy�[�W����ۑ�
		// �l���Ȃ��̂͏���Ƃ��āA�X�V�ɂ��Ȃ�
		if ( tpItemInfo->ITEM_PAGEDATA != NULL ) {
			if ( *tpItemInfo->ITEM_PAGEDATA != _T('\0') ) {
				CmpMsg = ST_UP;
			}
			M_FREE(tpItemInfo->ITEM_PAGEDATA);
		}
		tpItemInfo->ITEM_PAGEDATA = PageData;


		// �ʒʒm
		if ( UpItemInfo != NULL ) {
			ExecFlag = GetOptionInt(tpItemInfo->Option2, OP2_EXEC);
			ItemTitleLen = CountItemTitleLen(tpItemInfo);
		}
		while ( UpItemInfo != NULL ) {
			// ���s
			if ( ExecFlag & 1 ) {
				if ( ExecOptionCommand(tpItemInfo, UpItemInfo->CheckURL) == FALSE ) goto MEM_ERR;
			}

			// �X�V�ʒm
			if ( ExecFlag == 3 ) {
				// ���Ȃ�
				M_FREE(UpItemInfo->CheckURL);
				M_FREE(UpItemInfo->ITEM_UPINFO);
				NextItemInfo = UpItemInfo->hGetHost1;
				M_FREE(UpItemInfo);
				UpItemInfo = NextItemInfo;

			} else {
				// �A�C�e�����ƍX�V���
				if ( UpItemInfo->ITEM_UPINFO == NULL ) {
					// �ʂ̍X�V���Ȃ�
					UpItemInfo->Title = S_ALLOC(lstrlen(tpItemInfo->Title));
					if ( UpItemInfo->Title != NULL ) {
						lstrcpy(UpItemInfo->Title, tpItemInfo->Title);
					}
					UpItemInfo->ITEM_UPINFO = S_ALLOC(lstrlen(tpItemInfo->ITEM_UPINFO));
					if ( UpItemInfo->ITEM_UPINFO != NULL ) {
						lstrcpy(UpItemInfo->ITEM_UPINFO, tpItemInfo->ITEM_UPINFO);
					}
					UpItemInfo->Date = S_ALLOC(lstrlen(tpItemInfo->Date));
					if ( UpItemInfo->Date != NULL ) {
						lstrcpy(UpItemInfo->Date, tpItemInfo->Date);
					}

				} else {
					// �ʂ̍X�V��񂠂�
					// �A�C�e�����ƋL�������̃�����������ăA�C�e�������R�s�[
					UpItemInfo->Title = S_ALLOC(ItemTitleLen + (InfoToTitle && UpItemInfo->Param1 ? UpItemInfo->Param1 + 3 : 0));
					if ( UpItemInfo->Title != NULL ) { 
						_tcsncpy_s(UpItemInfo->Title, ItemTitleLen + 1, tpItemInfo->Title, ItemTitleLen);
					}
				
					// �X�V����ݒ�
					UpInfo = MakeUpInfo(UpItemInfo->ITEM_UPINFO, UpItemInfo->Param1, UpItemInfo, 1);
					if ( UpInfo != NULL ) {
						M_FREE(UpItemInfo->ITEM_UPINFO);
						UpItemInfo->ITEM_UPINFO = UpInfo;
						// �A�C�e�����ɉ�����
						if ( InfoToTitle && *UpInfo != _T('\0') && UpItemInfo->Title != NULL ) {
							lstrcat(UpItemInfo->Title, _T(" | "));
							lstrcat(UpItemInfo->Title, UpInfo);
						}
					}

					// �X�V��
					UpItemInfo->Date = S_ALLOC(InfoToDate ? (UpInfo == NULL ? 0 : lstrlen(UpInfo)) + 20 : 19);
					if ( UpItemInfo->Date != NULL ) {
						_tcsncpy_s(UpItemInfo->Date, 20, tpItemInfo->Date, 19);
						if ( InfoToDate && UpInfo != NULL && *UpInfo != _T('\0') ) {
							lstrcat(UpItemInfo->Date, _T(" "));
							lstrcat(UpItemInfo->Date, UpInfo);
						}				
					}
					UpInfo = NULL;
				}

				// �`�F�b�N��
				UpItemInfo->CheckDate = S_ALLOC(lstrlen(tpItemInfo->CheckDate));
				if ( UpItemInfo->CheckDate != NULL ) {
					lstrcpy(UpItemInfo->CheckDate, tpItemInfo->CheckDate);
				}

				// �������ōX�V��񂪃G���[���Ƃ��ď������̖h�����߁A�����N���o�ݒ��t����
				UpItemInfo->Option1 = S_ALLOC(15);
				if ( UpItemInfo->Option1 != NULL ) {
					lstrcpy(UpItemInfo->Option1, _T("0;;0;;0;;0;;3;;"));
				}

				SendMessage(((struct THARGS *)(tpItemInfo->ITEM_MARK))->hWnd, WM_CHECKEND, (WPARAM)ST_UP, (LPARAM)UpItemInfo);
				UpItemInfo = UpItemInfo->hGetHost1;
			}
		}

	} else if ( EntryLink == NULL ) {
		// �}�b�`����
		SetErrorString(tpItemInfo, _T("URL�������s"), FALSE);
		goto ERR;

	} else {
		// �X�V���̓A�C�e�����ɕۑ�����̂ŁA���X�V���̂݉��
		M_FREE(PageData);
		// Last-Modified
		M_FREE(tpItemInfo->DLLData1);
		GetLastModified(tpItemInfo, &tpItemInfo->DLLData1);
	}


END:
	// ���
	if ( Region != NULL ) onig_region_free(Region, 1);
	onig_free(RegExp);
	M_FREE(UrlPattern);
	M_FREE(UrlFormat);
	M_FREE(InfoPattern);
	M_FREE(InfoFormat);
	M_FREE(Body);
	M_FREE(EntryLink);
	M_FREE(EntryTitle);


	return CmpMsg;


	// �G���[
MEM_ERR :
	SetErrorString(tpItemInfo, STR_MEM_ERR, FALSE);

ERR : 
	M_FREE(NewLinks);
	M_FREE(PageData);
	M_FREE(UpInfo);
	CmpMsg = ST_ERROR;
	goto END;
}
