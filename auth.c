#include <windows.h>
#include <stdio.h>
#include <tchar.h>
#include <time.h>

#include "wwwcdll.h"
#include "resource.h"
#include "http.h"
#include "String.h"
#include "def.h"
#include "auth.h"



// SHA1
int GetSha1(IN LPBYTE OrgnData, IN int OrgnDataSize, OUT LPBYTE HashData) {
	BOOL ret;
	HCRYPTPROV hProv;
	HCRYPTHASH hHash;
	int HashDataSize = 20;


	// CSP�n���h��
	ret = CryptAcquireContext(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT);
	if ( ret == FALSE ) {
		return 0;
	}

	// �n�b�V���I�u�W�F�N�g�����
	ret = CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash);
	if ( ret == FALSE ) {
		CryptReleaseContext(hProv, 0);
		return 0;
	}

	// �n�b�V���I�u�W�F�N�g�Ɍ��f�[�^��������
	ret = CryptHashData(hHash, OrgnData, OrgnDataSize, 0);
	if ( ret == FALSE ) {
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return 0;
	}
	
	// �n�b�V���f�[�^�𓾂�
	ret = CryptGetHashParam(hHash, HP_HASHVAL, HashData, &HashDataSize, 0);
	if ( ret == FALSE ) {
		CryptDestroyHash(hHash);
		CryptReleaseContext(hProv, 0);
		return 0;
	}

	// �n�b�V���n���h�������
	CryptDestroyHash(hHash);
	CryptReleaseContext(hProv, 0);


	return HashDataSize;
}



// HMAC-SHA1
int GetHmacSha1(IN LPBYTE KeyData, IN int KeyDataSize, IN LPBYTE MessData, IN int MessDataSize, OUT LPBYTE HmacData) {
	BYTE Hash_Key[20];
	LPBYTE KeyIpadMes;
	BYTE Hash_KeyIpadMes[20];
	int HashDataSize;
	BYTE KeyOpadHKIM[84] = {0}; // 64+20
	int i;


	// �u���b�N�T�C�Y���傫�����͈�x�n�b�V��������
	if ( KeyDataSize > 64 ) {
		KeyDataSize = GetSha1(KeyData, KeyDataSize, Hash_Key); // hash
		if ( KeyDataSize == 0 ) {
			return 0;
		}
		KeyData = Hash_Key;
	}


	// hash( (key XOR ipad) + message )
	KeyIpadMes = M_ALLOC_Z(64 + MessDataSize);
	if ( KeyIpadMes == NULL ) {
		return 0;
	}
	memcpy(KeyIpadMes, KeyData, KeyDataSize); // ��
	for ( i = 0; i < 64; i++ ) { // ipad
		*(KeyIpadMes + i) ^= 0x36;
	}
	memcpy(KeyIpadMes + 64, MessData, MessDataSize); // message

	HashDataSize = GetSha1(KeyIpadMes, 64 + MessDataSize, Hash_KeyIpadMes); // hash
	M_FREE(KeyIpadMes);
	if ( HashDataSize == 0 ) {
		return 0;
	}


	// hash( (key XOR opad) + Hash_KeyIpadMes )
	memcpy(KeyOpadHKIM, KeyData, KeyDataSize); // ��
	for ( i = 0; i < 64; i++ ) { // opad
		*(KeyOpadHKIM + i) ^= 0x5c;
	}
	memcpy(KeyOpadHKIM + 64, Hash_KeyIpadMes, HashDataSize); // Hash_KeyIpadMes

	HashDataSize = GetSha1(KeyOpadHKIM, 64 + HashDataSize, HmacData); // hash
	if ( HashDataSize == 0 ) {
		return 0;
	}


	return HashDataSize;
}



// �w��o�C�g����Base64������
// String.c / eBase ���
void eBasen(IN const LPBYTE buf, LPTSTR ret, int InputLen)
{
	char tmp, tmp2;
	LPTSTR r;
	int c, i;
	static LPCTSTR Base = _T("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/");

	i = 0;
	r = ret;
//	while(*(buf + i) != '\0'){
	while( i < InputLen ){
		c = (*(buf + i) & 0xFC) >> 2;
		*(r++) = *(Base + c);
		i++;

//		if(*(buf + i) == '\0'){
//			*(buf + i) = 0;
		if( i == InputLen){
			tmp2 = (char)(*(buf + i - 1) << 4) & 0x30;
			tmp = (char)(*(buf + i) >> 4) & 0xF;
			c = tmp2 | tmp;
			*(r++) = *(Base + c);
			*(r++) = _T('=');
			*(r++) = _T('=');
			break;
		}
		tmp2 = (char)(*(buf + i - 1) << 4) & 0x30;
		tmp = (char)(*(buf + i) >> 4) & 0xF;
		c = tmp2 | tmp;
		*(r++) = *(Base + c);

		if( i + 1 == InputLen){
			c = (char)(*(buf + i) << 2) & 0x3C;
			*(r++) = *(Base + c);
			*(r++) = _T('=');
			break;
		}

		tmp2 = (char)(*(buf + i) << 2) & 0x3C;
		tmp = (char)(*(buf + i + 1) >> 6) & 0x3;
		c = tmp2 | tmp;
		*(r++) = *(Base + c);
		i++;

		c = *(buf + i) & 0x3F;
		*(r++) = *(Base + c);
		i++;
	}

	*r = _T('\0');
}



// ������� URI �G���R�[�h�AEncStrSize �� NULL ���݂�
BOOL ConvStrToUri(IN LPCSTR OriStr, OUT LPSTR EncStr, IN int EncStrSize) {
	static LPCSTR Hex = "0123456789ABCDEF";
	int i = 0, e = 0;


	if ( OriStr == NULL || EncStr == NULL ) {
		return FALSE;
	}

	for ( ; e + 1 < EncStrSize && OriStr[i] != '\0'; i++ ) {
		if (
				( OriStr[i] >= '0' && OriStr[i] <= '9' )
				||
				( OriStr[i] >= 'A' && OriStr[i] <= 'Z' )
				||
				( OriStr[i] >= 'a' && OriStr[i] <= 'z' )
				||
				OriStr[i] == '-'
				||
				OriStr[i] == '.'
				||
				OriStr[i] == '_'
				||
				OriStr[i] == '~'
		) {
			EncStr[e++] = OriStr[i];

//		} else if ( OriStr[i] == _T(' ') ) {
//			EncStr[e++] = _T('+');

		} else {
			if ( e + 4 > EncStrSize ) {
				break;
			}
			EncStr[e++] = '%';
			EncStr[e++] = Hex[ (char)OriStr[i] >> 4 ];
			EncStr[e++] = Hex[ OriStr[i] & 0xf ];
		}
	}

	EncStr[e] = '\0';
	return ( OriStr[i] == '\0' );
}



// JSON String���G�Ɏ擾
BOOL GetJsonStr(IN LPCTSTR JsonText, IN LPCTSTR Name, OUT LPCTSTR *ValueStart, OUT LPCTSTR *ValueEnd) {
	LPCTSTR Start = NULL, End = NULL; 


	if ( JsonText == NULL || Name == NULL || ValueStart == NULL || ValueEnd == NULL ) {
		return FALSE;
	}


	// ���O
	Start = _tcsstr(JsonText, Name);
	if ( Start == NULL ) {
		return FALSE;
	}
	// �l�̎n�܂�
	Start += lstrlen(Name);
	for (; *Start <= _T(' ') || *Start == _T(':'); Start++);
	if ( *Start == _T('"') ) {
		Start++;
	} else {
		return FALSE;
	}
	// �l�̏I���
	End = _tcschr(Start, _T('"'));
	if ( End == NULL ) {
		return FALSE;
	}


	*ValueStart = Start;
	*ValueEnd = End;
	return TRUE;
}



// JSON Number ���G�Ɏ擾
BOOL GetJsonNum(IN LPCTSTR JsonText, IN LPCTSTR Name, OUT int *Value) {
	LPCTSTR Start = NULL;


	if ( JsonText == NULL || Name == NULL || Value == NULL ) {
		return FALSE;
	}


	// ���O
	Start = _tcsstr(JsonText, Name);
	if ( Start == NULL ) {
		return FALSE;
	}
	// �l�̎n�܂�
	Start += lstrlen(Name);
	for (; *Start <= _T(' ') || *Start == _T(':'); Start++);
	if ( *Start == _T('"') ) {
		Start++;
	}


	*Value = _ttoi(Start);
	return TRUE;
}



// qsort �p�̕������r�֐�
int CompStr(const void *Str1, const void *Str2) {
	return lstrcmp(*(LPTSTR *)Str1, *(LPTSTR *)Str2);
}



// �A�C�e���I�v�V���������񂩂�w��̐ݒ�l�̃|�C���^��Ԃ�
// �w��ʒu�̐ݒ�l��������Ȃ�������A
// Count�Ɍ��L�ݒ�l���i�ʒu + 1�j�����ANULL��Ԃ� 
LPTSTR GetOptionPtr(LPCTSTR Option, int Index, OUT int *Count) {
	int IndexCount = 0;


	if ( Option == NULL ) {
		return NULL;
	}


	while ( *Option != _T('\0') && IndexCount < Index ) {
		if ( *Option == _T(';') && *(Option + 1) == _T(';') ) {
			Option++;
			IndexCount++;
		}
		Option++;
	}


	return IndexCount == Index && *Option != _T('\0') ?
		(LPTSTR)Option :
		(LPTSTR)((Count != NULL ? *Count = IndexCount : 0), NULL);
}



// �A�C�e���I�v�V�����̎w�荀�ڂɕ������ݒ�
BOOL SetOptionStr(LPTSTR Option, int OptionSize, int Index, LPCTSTR Value) {
	int Len = 0, IndexCount = 0, NewValueLen = 0, OldValueLen = 0, AddLen = 0;
	LPTSTR Target = NULL, After = NULL;


	if ( Option == NULL || Value == NULL ) {
		return FALSE;
	}


	// �ݒ�Ώۂ̕���
	Target = GetOptionPtr(Option, Index, &IndexCount);

	// ���݂̕�����
	Len = lstrlen(Option);
	// ���͒l�̕�����
	NewValueLen = lstrlen(Value);

	if ( Target != NULL ) {
		// ���ݒl�̕�����
		After = _tcsstr(Target, _T(";;"));
		if ( After == NULL ) {
			return FALSE;
		}
		OldValueLen = (int)(After - Target);
		// �T�C�Y
		if ( Len - OldValueLen + NewValueLen + 1 > OptionSize ) {
			return FALSE;
		}
		// �㑱�̈ʒu����
		if ( NewValueLen - OldValueLen ) {
			_tmemmove(After - OldValueLen + NewValueLen, After, (lstrlen(After) + 1));
		}

	} else {
		// �Ώە������Ȃ��̂ō��
		// �ǉ����镶����
		AddLen = (Index - IndexCount) * 2 + NewValueLen + 2;
		// �T�C�Y
		if ( Len + AddLen + 1 > OptionSize ) {
			return FALSE;
		}
		// �ǉ����𖄂߂�
		Target = Option + Len;
		_tmemset(Target, _T(';'), AddLen);
		*(Target + AddLen) =_T('\0');
		Target += (Index - IndexCount) * 2;
	}

	// �X�V
	_tmemcpy(Target, Value, NewValueLen);


	return TRUE;
}



// �A�C�e���I�v�V�����̎w�荀�ڂɐ��l��ݒ�
BOOL SetOptionNum(LPTSTR Option, int OptionSize, int Index, UINT Value) {
	TCHAR ValueStr[11] = {0};

	_ultot_s(Value, ValueStr, 11, 10);

	return SetOptionStr(Option, OptionSize, Index, ValueStr);
}



// WSSE�w�b�_�l�����
BOOL MakeWsseValue(LPTSTR Username, LPTSTR Password, OUT LPTSTR WsseValue, int WsseValueSize) {
	BYTE PasswordDigest[21];
	LPBYTE OrgData;
	int PasswordLen;

	time_t t;
	struct tm tmt;
	int i;


	PasswordLen = lstrlen(Password);
	OrgData = M_ALLOC_Z(40 + PasswordLen);
	if ( OrgData == NULL ) {
		return FALSE;
	}

	// Nonce
	time(&t);
	srand((unsigned int)t);
	for ( i = 0; i < 20; i++ ) {
		*(OrgData + i) = rand() % 256;
	}

	// Created
	gmtime_s(&tmt, &t);
	_tcsftime(OrgData + 20, 21, _T("%Y-%m-%dT%H:%M:%SZ"), &tmt);

	// PasswordDigest
	memcpy(OrgData + 40, Password, PasswordLen);
	GetSha1(OrgData, 40 + PasswordLen, PasswordDigest);
	*(PasswordDigest + 20) = 0;

	// UsernameToken
	_tcscpy_s(WsseValue, WsseValueSize, _T("UsernameToken Username=\""));
	_tcscat_s(WsseValue, WsseValueSize, Username);

	// PasswordDigest
	_tcscat_s(WsseValue, WsseValueSize, _T("\", PasswordDigest=\""));
	eBasen(PasswordDigest, WsseValue + lstrlen(WsseValue), 20);

	// Nonce
	_tcscat_s(WsseValue, WsseValueSize, _T("\", Nonce=\""));
	eBasen(OrgData, WsseValue + lstrlen(WsseValue), 20);

	// Created
	_tcscat_s(WsseValue, WsseValueSize, _T("\", Created=\""));
	_tcsncat_s(WsseValue, WsseValueSize, OrgData + 20, 20);
	_tcscat_s(WsseValue, WsseValueSize, _T("\""));
	M_FREE(OrgData);


	return TRUE;
}



struct OAUTHCONFIG * GetOauthConfig(LPCTSTR HostName, LPCTSTR PathName) {
	struct OAUTHCONFIG *OauthConfigTemp;
	int i = 0;

	OauthConfigTemp = OauthConfig;

	while ( OauthConfigTemp != NULL ) {
		// �z�X�g����v
		if ( lstrcmp(OauthConfigTemp->HostName, HostName) == 0 ) {
			// �p�X���ݒ�Ȃ�
			if ( HAS_STR(OauthConfigTemp->PathName) == FALSE ) {
				break;

			// �p�X���ݒ肠��
			} else  {
				PathName++;
				for ( i = 0;
						*(OauthConfigTemp->PathName + i) != _T('\0')
							&& *(PathName + i) != _T('\0');
						i++
				) {
					if ( *(OauthConfigTemp->PathName + i)
							!= ( *(PathName + i) == _T('/') ? _T('_') : *(PathName + i) )
					) {
						break;
					}
				}

				// �p�X���ݒ���Ō�܂Ō���
				if ( *(OauthConfigTemp->PathName + i) == _T('\0') ) {
					break;
				}
			}
		}

		OauthConfigTemp = OauthConfigTemp->Next;
	}

	return OauthConfigTemp;
}



// OAuth1.0�w�b�_�l�����
BOOL MakeOauthValue1(IN LPCTSTR ReqUri, IN struct OAUTHPARAM *OauthParam, OUT LPTSTR OauthValue, IN int OauthValueSize) {
	struct OAUTHCONFIG *ItemOauthConfig = NULL;
	int i = 0, QueryLen = 0, MessageLen = 0, KeyLen= 0, DigestSize = 0, ParamCount = 0;
	LPTSTR Message = NULL, Key = NULL, ReqUriBase = NULL, ReqUriQuery = NULL,
		QueryTemp = NULL, *ParamList = NULL, Param = NULL;
	TCHAR Timestamp[20] = {0}, DigestB64[29] = {0};
	BYTE Digest[20] = {0};

#ifdef _UNICODE
	LPBYTE TempAnsi = NULL, TempAnsi2 = NULL;
#endif


	if ( OauthParam == NULL || OauthParam->OauthConfig == NULL ) {
		return FALSE;
	}
	ItemOauthConfig = OauthParam->OauthConfig;


	// UNIX Time
	_itot_s((int)time(NULL), Timestamp, 11, 10);


	// �w�b�_
	_tcscpy_s(OauthValue, OauthValueSize, _T("OAuth "));
	if ( HAS_STR(ItemOauthConfig->Realm) ) {
		_tcscat_s(OauthValue, OauthValueSize, _T("realm=\""));
		_tcscat_s(OauthValue, OauthValueSize, ItemOauthConfig->Realm);
		_tcscat_s(OauthValue, OauthValueSize, _T("\", "));
	}
	_tcscat_s(OauthValue, OauthValueSize, _T("oauth_version=\"1.0\", oauth_consumer_key=\""));
	_tcscat_s(OauthValue, OauthValueSize, _T(ItemOauthConfig->ClientIdentifier));
	_tcscat_s(OauthValue, OauthValueSize, _T("\", oauth_timestamp=\""));
	_tcscat_s(OauthValue, OauthValueSize, Timestamp);
	_tcscat_s(OauthValue, OauthValueSize, _T("\", oauth_nonce=\""));
	_tcscat_s(OauthValue, OauthValueSize, Timestamp);
	if ( HAS_STR(OauthParam->Identifier) ) {
		_tcscat_s(OauthValue, OauthValueSize, _T("\", oauth_token=\""));
		_tcscat_s(OauthValue, OauthValueSize, OauthParam->Identifier);
	}
	if ( HAS_STR(OauthParam->CallbackUri) ) {
		_tcscat_s(OauthValue, OauthValueSize, _T("\", oauth_callback=\""));
		_tcscat_s(OauthValue, OauthValueSize, OauthParam->CallbackUri);
	}
	if ( HAS_STR(OauthParam->Verifier) ) {
		_tcscat_s(OauthValue, OauthValueSize, _T("\", oauth_verifier=\""));
		_tcscat_s(OauthValue, OauthValueSize, OauthParam->Verifier);
	}
	_tcscat_s(OauthValue, OauthValueSize, _T("\", oauth_signature_method=\""));


	// ����
	// Key
	KeyLen = lstrlen(ItemOauthConfig->ClientSecret) + lstrlen(OauthParam->Secret) + 1;
	Key = S_ALLOC(KeyLen);
	if ( Key == NULL ) {
		return FALSE;
	}
	lstrcpy(Key, ItemOauthConfig->ClientSecret);
	lstrcat(Key, _T("&"));
	if ( OauthParam->Secret != NULL ) {
		_tcscat_s(Key, KeyLen + 1, OauthParam->Secret);
	}

	// HMAC-SHA
	if ( ItemOauthConfig->SignMethod == OAUTH_SIGN_HMACSHA1 ) {
		// Uri
		ReqUriQuery = _tcschr(ReqUri, _T('?'));
		// �N�G���[
		if ( ReqUriQuery != NULL ) {
			QueryTemp = ReqUriQuery;

			// �x�[�X URI
			ReqUriBase = S_ALLOC(ReqUriQuery - ReqUri);
			if ( ReqUriBase == NULL ) {
				goto ERR;
			}
			lstrcpyn(ReqUriBase, ReqUri, (int)(ReqUriQuery - ReqUri) + 1); // ��������NULL�����K�v
			ReqUri = ReqUriBase;

			// �N�G���[�̐�
			for (
				ParamCount = 1;
				*(++QueryTemp) != _T('\0');
				*QueryTemp == _T('&') && ParamCount++
			);

			// �ʏ���
			ParamList = M_ALLOC_Z(sizeof(LPTSTR) * (ParamCount + 8));
			if ( ParamList == NULL ) {
				goto ERR;
			}
			QueryTemp = ReqUriQuery;
			for ( i = 0; i < ParamCount; i++, ReqUriQuery = QueryTemp ) {
				// ������
				QueryTemp = _tcschr(++ReqUriQuery, _T('&'));
				QueryLen = QueryTemp != NULL ? (int)(QueryTemp - ReqUriQuery) : lstrlen(ReqUriQuery);
				// �p�����[�^���X�g�ɃR�s�[
				Param = ParamList[i] = S_ALLOC_Z(QueryLen * 3); // % �ϊ���3�{
				if ( Param == NULL ) {
					goto ERR;
				}
				lstrcpyn(Param + QueryLen * 2, ReqUriQuery, QueryLen + 1);
				ConvStrToUri(Param + QueryLen * 2, Param, QueryLen * 3 + 1);
				MessageLen += lstrlen(Param) + 3;
				ReqUriQuery += QueryLen;
			}
		} else {
			// �N�G���[�Ȃ�
			ParamList = M_ALLOC_Z(sizeof(LPTSTR) * 8);
			if ( ParamList == NULL ) {
				goto ERR;
			}
		}

		// �p�����[�^
		if ( HAS_STR(OauthParam->CallbackUri) ) {
			Param = ParamList[ParamCount] = S_ALLOC(17 + lstrlen(OauthParam->CallbackUri) * 3);
			if ( Param == NULL ) {
				goto ERR;
			}
			lstrcpy(Param, _T("oauth_callback%3D"));
			ConvStrToUri(OauthParam->CallbackUri, Param + 17, lstrlen(OauthParam->CallbackUri) * 3 + 1);
			MessageLen += lstrlen(Param) + 3;
			ParamCount++;
		}

		Param = ParamList[ParamCount] = S_ALLOC(21 + lstrlen(ItemOauthConfig->ClientIdentifier) * 3);
		if ( Param == NULL ) {
			goto ERR;
		}
		lstrcpy(Param, _T("oauth_consumer_key%3D"));
		ConvStrToUri(ItemOauthConfig->ClientIdentifier, Param + 21, lstrlen(ItemOauthConfig->ClientIdentifier) * 3 + 1);
		MessageLen += lstrlen(Param) + 3;
		ParamCount++;

		Param = ParamList[ParamCount] = S_ALLOC(24);
		if ( Param == NULL ) {
			goto ERR;
		}
		lstrcpy(Param, _T("oauth_nonce%3D"));
		lstrcat(Param, Timestamp);
		MessageLen += 27;
		ParamCount++;

		Param = ParamList[ParamCount] = S_ALLOC(34);
		if ( Param == NULL ) {
			goto ERR;
		}
		lstrcpy(Param, _T("oauth_signature_method%3DHMAC-SHA1"));
		MessageLen += 37;
		ParamCount++;

		Param = ParamList[ParamCount] = S_ALLOC(28);
		if ( Param == NULL ) {
			goto ERR;
		}
		lstrcpy(Param, _T("oauth_timestamp%3D"));
		lstrcat(Param, Timestamp);
		MessageLen += 31;
		ParamCount++;

		if ( HAS_STR(OauthParam->Identifier) ) {
			Param = ParamList[ParamCount] = S_ALLOC(14 + lstrlen(OauthParam->Identifier) * 3);
			if ( Param == NULL ) {
				goto ERR;
			}
			lstrcpy(Param, _T("oauth_token%3D"));
			ConvStrToUri(OauthParam->Identifier, Param + 14, lstrlen(OauthParam->Identifier) * 3 + 1);
			MessageLen += lstrlen(Param) + 3;
			ParamCount++;
		}

		if ( HAS_STR(OauthParam->Verifier) ) {
			Param = ParamList[ParamCount] = S_ALLOC(17 + lstrlen(OauthParam->Verifier) * 3);
			if ( Param == NULL ) {
				goto ERR;
			}
			lstrcpy(Param, _T("oauth_verifier%3D"));
			ConvStrToUri(OauthParam->Verifier, Param + 17, lstrlen(OauthParam->Verifier) * 3 + 1);
			MessageLen += lstrlen(Param) + 3;
			ParamCount++;
		}

		Param = ParamList[ParamCount] = S_ALLOC(19);
		if ( Param == NULL ) {
			goto ERR;
		}
		lstrcpy(Param, _T("oauth_version%3D1.0"));
		MessageLen += 22;
		ParamCount++;

		// �p�����[�^���X�g���\�[�g
		qsort( ParamList, ParamCount, sizeof(LPTSTR), CompStr );

		// Message = Method + '&' + EncodeUri(Uri) + '&' + EncodeUri(Params);
		MessageLen += lstrlen(ReqUri) * 3 + 5;
		Message = S_ALLOC(MessageLen);
		if ( Message == NULL ) {
			goto ERR;
		}

		// Method
		_tcscpy_s(Message, MessageLen, OauthParam->ReqMethod == REQUEST_HEAD ? _T("HEAD&") : OauthParam->ReqMethod == REQUEST_POST ? _T("POST&") : _T("GET&") );

		// URI
		ConvStrToUri(ReqUri, Message + lstrlen(Message), MessageLen - lstrlen(Message));
		M_FREE(ReqUriBase);
		ReqUriBase = NULL;
		lstrcat(Message, _T("&"));

		// Params
		for( i = 0; i < ParamCount && ParamList[i] != NULL; i++ ) {
			lstrcat(Message, ParamList[i]);
			lstrcat(Message, _T("%26"));
		}
		MessageLen = lstrlen(Message) - 3;
		*(Message + MessageLen) = _T('\0');
		for ( i = 0; i < ParamCount; i++ ) {
			M_FREE(ParamList[i]);
		}
		M_FREE(ParamList);
		ParamList = NULL;

#ifdef _UNICODE
		TempAnsi = M_ALLOC(KeyLen + 1);
		if ( TempAnsi == NULL ) {
			goto ERR;
		}
		if ( WideCharToMultiByte(CP_ACP, 0, Key, -1, TempAnsi, KeyLen + 1, NULL, NULL) == 0 ) {
			M_FREE(TempAnsi);
			TempAnsi = NULL;
			goto ERR;
		}
		M_FREE(Key);
		(LPBYTE)Key = TempAnsi;
		TempAnsi = NULL;

		TempAnsi2 = M_ALLOC(MessageLen + 1);
		if ( TempAnsi2 == NULL ) {
			goto ERR;
		}
		if ( WideCharToMultiByte(CP_ACP, 0, Key, -1, TempAnsi2, MessageLen + 1, NULL, NULL) == 0 ) {
			M_FREE(TempAnsi2);
			TempAnsi2 = NULL;
			goto ERR;
		}
		M_FREE(Message);
		(LPBYTE)Message = TempAnsi2;
		TempAnsi2 = NULL;
#endif

		// Digest
		DigestSize = GetHmacSha1((LPBYTE)Key, KeyLen, (LPBYTE)Message, MessageLen, Digest);
		M_FREE(Key);
		M_FREE(Message);

		if ( DigestSize == 0 ) {
			return FALSE;
		}
		eBasen(Digest, DigestB64, DigestSize);

		// �w�b�_
		_tcscat_s(OauthValue, OauthValueSize, _T("HMAC-SHA1\", oauth_signature=\""));
		ConvStrToUri(DigestB64, OauthValue + lstrlen(OauthValue), OauthValueSize - lstrlen(OauthValue));

	} else if ( ItemOauthConfig->SignMethod == OAUTH_SIGN_PLAINTEXT ) {
	// PLAINTEXT
		_tcscat_s(OauthValue, OauthValueSize, _T("PLAINTEXT\", oauth_signature=\""));
		ConvStrToUri(Key, OauthValue + lstrlen(OauthValue), OauthValueSize - lstrlen(OauthValue));
		M_FREE(Key);
		Key = NULL;

//	} else if ( SignMethod == OAUTH_SIGN_RSASHA1 ) {
	// RSA-SHA1
//		_tcscat_s(OauthValue, OauthValueSize, _T("RSA-SHA1\", oauth_signature=\""));
//	}

	} else {
		return FALSE;
	}

	// �Y
	_tcscat_s(OauthValue, OauthValueSize, _T("\""));


	return TRUE;


	ERR :
		M_FREE(Key);
		M_FREE(ReqUriBase);
		M_FREE(Message);
		if ( ParamList != NULL ) {
			for ( i = 0; i < ParamCount; i++ ) {
				M_FREE(ParamList[i]);
			}
			M_FREE(ParamList);
		}
		return FALSE;
}



// OAuth2.0�A�N�Z�X�g�[�N�������t���b�V��
BOOL GetOauthTokenRefresh(struct TPITEM *tpItemInfo, struct OAUTHCONFIG *ItemOauthConfig, LPTSTR AccessToken, LPTSTR Refresh) {
	struct TPITEM *OauthItem = NULL;
	struct THARGS *ThArgs = NULL;
	struct TPHTTP *tpHTTP = NULL;
	LPTSTR NewOption2 = NULL, RefreshToken = NULL, NewAccessToken = NULL, NewRefresh = NULL;
	int OptionCount = 0, Len = 0;
	BOOL Ret = FALSE;
	LPCTSTR OptionPostFormat = _T("3;;;;;;;;;;;;;;;;;;grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s;;");


	OauthItem = M_ALLOC_Z(sizeof(struct TPITEM));
	if ( OauthItem == NULL ) {
		goto ERR;
	}

	tpHTTP = M_ALLOC_Z(sizeof(struct TPHTTP));
	if( tpHTTP == NULL ){
		goto ERR;
	}
	OauthItem->Param1 = (long)tpHTTP;


	// URI
	OauthItem->CheckURL = S_ALLOC(lstrlen(ItemOauthConfig->TokenUri));
	if( OauthItem->CheckURL == NULL ){
		goto ERR;
	}
	lstrcpy(OauthItem->CheckURL, ItemOauthConfig->TokenUri);
	OauthItem->Param2 = (long)OauthItem->CheckURL;


	// �{���擾�̂���
	OauthItem->Param3 = 2;
	// POST
	OauthItem->user2 = REQUEST_POST;


	// �I�v�V�����^�u
	if ( HAS_STR(tpItemInfo->Option2) ) {
		OauthItem->Option2 = S_ALLOC(lstrlen(tpItemInfo->Option2));
		if ( OauthItem->Option2 == NULL ) {
			goto ERR;
		}
		lstrcpy(OauthItem->Option2, tpItemInfo->Option2);
		// �F�؂𖳌�
		SetOptionNum(OauthItem->Option2, lstrlen(tpItemInfo->Option2) + 1, OP2_USEPASS, 0);
	}


	// POST�f�[�^
	// ���t���b�V���g�[�N��
	RefreshToken = _tcschr(Refresh, _T('/'));
	if ( RefreshToken == NULL ) {
		goto ERR;
	}
	RefreshToken++;

	// �`�F�b�N�^�u��POST
	Len = lstrlen(OptionPostFormat) + lstrlen(RefreshToken)
		+ lstrlen(ItemOauthConfig->ClientIdentifier) + lstrlen(ItemOauthConfig->ClientSecret);
	OauthItem->Option1 = S_ALLOC(Len);
	if ( OauthItem->Option1 == NULL ) {
		goto ERR;
	}
	_stprintf_s(OauthItem->Option1, Len + 1, 
		OptionPostFormat, RefreshToken, ItemOauthConfig->ClientIdentifier, ItemOauthConfig->ClientSecret);


	//�ڑ��T�[�o�ƃ|�[�g�ԍ����擾
	if( GetServerPort(NULL, OauthItem, tpHTTP) == FALSE ){
		goto ERR;
	}


	// �ʃX���ւ̈���
	ThArgs = M_ALLOC_Z(sizeof(struct THARGS));
	if ( ThArgs == NULL ) {
		goto ERR;
	}
	// �l
	ThArgs->tpItemInfo = OauthItem;
	OauthItem->ITEM_MARK = (ITEM_MARK_TYPE)ThArgs;


	// ���N�G�X�g
	if ( WinetReq(ThArgs, OauthItem, tpHTTP) != ST_DEFAULT
		|| tpHTTP->StatusCode != 200
		|| ParseOauthCredentials2(tpHTTP->Body, &NewAccessToken, &NewRefresh, ItemOauthConfig) == FALSE
	) {
		goto ERR;
	}


	// �R�s�[
	lstrcpy(AccessToken, NewAccessToken);

	switch ( lstrlen(NewRefresh) ) {
		case 2: // 0/
			*Refresh = _T('0');
			lstrcpy(Refresh + 1, RefreshToken - 1);
			break;

		case 11: // 1111111111/
			if ( *NewRefresh != _T('0') ) {
				_tmemcpy(Refresh, NewRefresh, 11);
				break;
			}

		default: // 0/rrr 1111111111/rrr
			_tcscpy_s(Refresh, BUFSIZE, NewRefresh);
			break;
	}

	M_FREE(NewRefresh);
	NewRefresh = S_ALLOC(lstrlen(Refresh) * 2);
	if ( NewRefresh == NULL ) {
		goto ERR;
	}
	ePass(Refresh, NewRefresh);

	// �X�V
	Len = lstrlen(tpItemInfo->Option2) + lstrlen(AccessToken) + lstrlen(NewRefresh);
	NewOption2 = S_ALLOC(Len);
	if ( NewOption2 == NULL ) {
		goto ERR;
	}
	lstrcpy(NewOption2, tpItemInfo->Option2);
	if ( SetOptionStr(NewOption2, Len + 1, OP2_USER, AccessToken) == FALSE ) {
		goto ERR;
	}
	if ( SetOptionStr(NewOption2, Len + 1, OP2_PASS, NewRefresh) == FALSE ) {
		goto ERR;
	}
	M_FREE(tpItemInfo->Option2);
	tpItemInfo->Option2 = NewOption2;
	NewOption2 = NULL;


	Ret = TRUE;

END :
	M_FREE(NewOption2);
	M_FREE(NewAccessToken);
	M_FREE(NewRefresh);
	WinetClear(OauthItem);
	M_FREE(OauthItem->Option1);
	M_FREE(OauthItem->Option2);
	M_FREE(OauthItem);

	return Ret;


ERR :
	Ret = FALSE;
	goto END;
}



// OAuth2.0�w�b�_�l�����
BOOL MakeOauthValue2(struct TPITEM *tpItemInfo, struct OAUTHCONFIG *ItemOauthConfig, IN LPTSTR AccessToken, IN LPTSTR Refresh, OUT LPTSTR OauthValue, IN int OauthValueSize) {
	time_t ExpireSec;


	// �g�[�N���̗L������
	ExpireSec = _ttoi(Refresh);
	if ( ExpireSec != 0 && ExpireSec - time(NULL) < 60 ) {
		// �����؂��Refresh
		if ( GetOauthTokenRefresh(tpItemInfo, ItemOauthConfig, AccessToken, Refresh) == FALSE ) {
			return FALSE;
		}
	}

	if ( _tcsnicmp(AccessToken, _T("Bearer "), 7) == 0 ) {
		_tcscpy_s(OauthValue, OauthValueSize, AccessToken);
		return TRUE;
	}


	return FALSE;
}



// OAuth�w�b�_�l�����
BOOL MakeOauthValue(struct TPITEM *tpItemInfo, IN LPTSTR Identifier, IN LPTSTR Secret, OUT LPTSTR OauthValue, IN int OauthValueSize) {
	struct TPHTTP *tpHTTP = (struct TPHTTP *)tpItemInfo->Param1;
	BOOL Ret = FALSE;
	struct OAUTHCONFIG *ItemOauthConfig = NULL;
	struct OAUTHPARAM *OauthParam = NULL;


	switch ( tpItemInfo->ITEM_STATE ) {
		case 0 : // �ʏ�
			ItemOauthConfig = GetOauthConfig(tpHTTP->hSvName, tpHTTP->Path);
			if ( ItemOauthConfig == NULL ) {
				return FALSE;
			}
			switch ( ItemOauthConfig->Version ) {
				case 1 :
					OauthParam = M_ALLOC_Z(sizeof(struct OAUTHPARAM));
					if ( OauthParam == NULL ) {
						return FALSE;
					}
					OauthParam->ReqMethod = tpItemInfo->user2;
					OauthParam->Identifier = Identifier;
					OauthParam->Secret = Secret;
					OauthParam->OauthConfig = ItemOauthConfig;
					Ret = MakeOauthValue1(tpItemInfo->CheckURL, OauthParam, OauthValue, OauthValueSize);
					M_FREE(OauthParam);
					break;

				case 2 :
					Ret = MakeOauthValue2(tpItemInfo, ItemOauthConfig, Identifier, Secret, OauthValue, OauthValueSize);
					break;
			}
			break;

		case OAUTH_STATE_TEMP1 :
		case OAUTH_STATE_TOKEN1 :
			OauthParam = (struct OAUTHPARAM *)tpItemInfo->ITEM_OAUTH;
			Ret = MakeOauthValue1(tpItemInfo->CheckURL, OauthParam, OauthValue, OauthValueSize);
			break;

	}


	return Ret;
}



// OAuth 1.0 �� temporary credentials �𓾂�
BOOL GetOauthTemp(HWND hDlg, struct TPITEM *tpItemInfo) {
	struct OAUTHCONFIG *ItemOauthConfig = NULL;
	struct TPITEM *OauthItem = NULL;
	struct OAUTHPARAM *OauthParam = NULL;
	int NewLen = 0;


	if ( tpItemInfo == NULL || tpItemInfo->ITEM_OAUTH == NULL ) {
		return FALSE;
	}
	ItemOauthConfig = tpItemInfo->ITEM_OAUTH;


	// �擾�p�ݒ�
	OauthItem = M_ALLOC_Z(sizeof(struct TPITEM));
	if ( OauthItem == NULL ) {
		return FALSE;
	}
	tpItemInfo->ITEM_OAUTH = (ITEM_OAUTH_TYPE)OauthItem;
	tpItemInfo->ITEM_STATE = OauthItem->ITEM_STATE = (ITEM_STATE_TYPE)OAUTH_STATE_TEMP1;
	
	// URI
	OauthItem->CheckURL = S_ALLOC(lstrlen(ItemOauthConfig->TemporaryUri));
	if ( OauthItem->CheckURL == NULL ) {
		return FALSE;
	}
	lstrcpy(OauthItem->CheckURL, ItemOauthConfig->TemporaryUri);
	OauthItem->Param2 = (long)OauthItem->CheckURL;
	// �\�[�X�\���p�t���O�A�{�̎擾�֐i�ނ���
	OauthItem->Param3 = 2;


	// �I�v�V�����^�u�̊����ݒ���R�s�[
	NewLen = lstrlen(tpItemInfo->Option2) + 11;
	OauthItem->Option2 = S_ALLOC_Z(NewLen);
	if ( OauthItem->Option2 == NULL ) {
		return FALSE;
	}
	if ( HAS_STR(tpItemInfo->Option2) ) {
		lstrcpy(OauthItem->Option2, tpItemInfo->Option2);
	}
	// OAuth�F�؂�����
	SetOptionNum(OauthItem->Option2, NewLen + 1, OP2_USEPASS, AUTH_OAUTH_ON);


	// �`�F�b�N�^�u�Ń��N�G�X�g���\�b�h
	OauthItem->Option1 = S_ALLOC_Z(3);
	if ( OauthItem->Option1 == NULL ) {
		return FALSE;
	}
	wsprintf(OauthItem->Option1, _T("%d;;"), ItemOauthConfig->ReqMethod);


	// OAuth �p�����[�^
	OauthParam = M_ALLOC_Z(sizeof(struct OAUTHPARAM));
	if ( OauthParam == NULL ) {
		return FALSE;
	}
	OauthParam->ReqMethod = ItemOauthConfig->ReqMethod;
	OauthParam->CallbackUri = ItemOauthConfig->CallbackUri;
	OauthParam->OauthConfig = ItemOauthConfig;
	OauthItem->ITEM_OAUTH = (ITEM_OAUTH_TYPE)OauthParam;


	// ���N�G�X�g
	if ( HTTP_Start(hDlg, OauthItem) == CHECK_ERROR ) {
		return FALSE;
	}


	return TRUE;
}



// OAuth 2.0 �̍ŏ��A�F��URI������ĊJ��
BOOL OpenAuthorizationUri(HWND hDlg, struct TPITEM *tpItemInfo) {
	struct OAUTHCONFIG *ItemOauthConfig = NULL;
	LPTSTR AuthorizationUri = NULL;
	TCHAR QueryFormat[] = {_T("%s?response_type=code&client_id=%s&redirect_uri=%s\0scope=%s")};
	int Len = 0;


	if ( tpItemInfo == NULL || tpItemInfo->ITEM_OAUTH == NULL ) {
		return FALSE;
	}
	ItemOauthConfig = tpItemInfo->ITEM_OAUTH;


	tpItemInfo->ITEM_STATE = (ITEM_STATE_TYPE)OAUTH_STATE_VAR2;

	// URI
	if ( HAS_STR(ItemOauthConfig->Scope) ) {
		*(QueryFormat + lstrlen(QueryFormat)) = _T('&');
	}
	if ( _tcschr(ItemOauthConfig->AuthorizationUri, _T('?')) ) {
		*(QueryFormat + 2) = _T('&');
	}
	Len = lstrlen(QueryFormat)
		+ lstrlen(ItemOauthConfig->AuthorizationUri)
		+ lstrlen(ItemOauthConfig->ClientIdentifier)
		+ lstrlen(ItemOauthConfig->CallbackUri)
		+ lstrlen(ItemOauthConfig->Scope);
	AuthorizationUri = S_ALLOC(Len);
	if ( AuthorizationUri == NULL ) {
		return FALSE;
	}
	_stprintf_s(AuthorizationUri, Len + 1, QueryFormat,
		ItemOauthConfig->AuthorizationUri,
		ItemOauthConfig->ClientIdentifier,
		ItemOauthConfig->CallbackUri,
		ItemOauthConfig->Scope
	);


	// �J��
	ShellExecute(NULL, NULL, AuthorizationUri, NULL, NULL, SW_SHOW);
	M_FREE(AuthorizationUri);
	EnableWindow(GetDlgItem(hDlg, IDC_EDIT_GETOAUTH_VERIFIER), TRUE);
	EnableWindow(GetDlgItem(hDlg, IDC_BUTTON_GETOAUTH_GETTOKEN), TRUE);


	return TRUE;
}



// OAuth �̃g�[�N���擾�����̊J�n
BOOL GetOauthStart(HWND hDlg, struct TPITEM *tpItemInfo) {

	if ( tpItemInfo == NULL || tpItemInfo->ITEM_OAUTH == NULL ) {
		return FALSE;
	}


	if ( ((struct OAUTHCONFIG *)tpItemInfo->ITEM_OAUTH)->Version == 1 ) {
		// OAuth 1.0
		return GetOauthTemp(hDlg, tpItemInfo);

	} else 	if ( ((struct OAUTHCONFIG *)tpItemInfo->ITEM_OAUTH)->Version == 2 ) {
		// OAuth 2.0
		return OpenAuthorizationUri(hDlg, tpItemInfo);
	}


	return FALSE;
}



// OAuth 1.0 �̃g�[�N���擾���X�|���X����N���f���V�����𒊏o
// ������������ă|�C���^�̃|�C���^�ɓ����
BOOL ParseOauthCredentials1(IN LPCTSTR Credentials, OUT LPTSTR *Identifier, OUT LPTSTR *Secret ) {
	LPCTSTR NameIdentifier = _T("oauth_token="), NameSecret = _T("oauth_token_secret="),
		Param = NULL, ParamEnd = NULL;
	LPTSTR Value = NULL;


	if ( Credentials == NULL || Identifier == NULL || Secret == NULL ) {
		return FALSE;
	}


	// Identifier
	// ���O��T��
	Param = _tcsstr(Credentials, NameIdentifier);
	if ( Param == NULL ) {
		return FALSE;
	}
	// �l�̕��������̃����������
	Param += lstrlen(NameIdentifier);
	ParamEnd = _tcschr(Param, _T('&'));
	if ( ParamEnd == NULL ) {
		ParamEnd = Param + lstrlen(Param);
	}
	Value = S_ALLOC(ParamEnd - Param);
	if ( Value == NULL ) {
		return FALSE;
	}
	// �R�s�[
	*Identifier = Value;
	for ( ; Param < ParamEnd; *(Value++) = (TCHAR)*Param++ );
	*Value = _T('\0');

	
	// Secret
	// ���O��T��
	Param = _tcsstr(Credentials, NameSecret);
	if ( Param == NULL ) {
		return FALSE;
	}
	// �l�̕��������̃����������
	Param += lstrlen(NameSecret);
	ParamEnd = _tcschr(Param, _T('&'));
	if ( ParamEnd == NULL ) {
		ParamEnd = Param + lstrlen(Param);
	}
	Value = S_ALLOC(ParamEnd - Param);
	if ( Value == NULL ) {
		return FALSE;
	}
	// �R�s�[
	*Secret = Value;
	for ( ; Param < ParamEnd; *(Value++) = (TCHAR)*Param++ );
	*Value = _T('\0');

	
	return TRUE;
}



// OAuth 2.0 �̃g�[�N���擾���X�|���X����N���f���V�����𒊏o
// ������������ă|�C���^�̃|�C���^�ɓ����
BOOL ParseOauthCredentials2(IN LPCTSTR JsonText, OUT LPTSTR *Token, OUT LPTSTR *Refresh, struct OAUTHCONFIG *ItemOauthConfig) {
	LPCTSTR NameType = _T("\"token_type\""),
		NameToken = _T("\"access_token\""),
		NameRefresh = _T("\"refresh_token\""),
		NameExpire = _T("\"expires_in\""); 
	LPTSTR ValueStart = NULL, ValueEnd = NULL,ValueStart2 = NULL, ValueEnd2 = NULL, Value = NULL;
	int Len = 0, Len2 = 0, Expire = 0;
	time_t UtcSec;


	if ( JsonText == NULL || Token == NULL || Refresh == NULL || ItemOauthConfig == NULL ) {
		return FALSE;
	}


	// TokenType
	if ( GetJsonStr(JsonText, NameType, &ValueStart, &ValueEnd) == FALSE ) {
		return FALSE;
	}
	// AccessToken
	if ( GetJsonStr(JsonText, NameToken, &ValueStart2, &ValueEnd2) == FALSE ) {
		return FALSE;
	}

	// �l�̕��������̃����������
	// "TokenType AccessToken"
	Len = (int)(ValueEnd - ValueStart);
	Len2 = (int)(ValueEnd2 - ValueStart2);
	Value = S_ALLOC(Len + 1 + Len2);
	if ( Value == NULL ) {
		return FALSE;
	}

	// �R�s�[
	// TokenType
	*Token = Value;
	_tmemcpy(Value, ValueStart, Len);
	*(Value + Len) = _T(' ');
	// AccessToken
	lstrcpyn(Value + Len + 1, ValueStart2, Len2 + 1);


	// Expire �C��
	Expire = 0;
	GetJsonNum(JsonText, NameExpire, &Expire);
	// RefreshToken �C��
	ValueStart = NULL;
	ValueEnd = NULL;
	GetJsonStr(JsonText, NameRefresh, &ValueStart, &ValueEnd);

	// ���������̃����������
	// "Expire/RefreshToken"
	Len = (int)(ValueEnd - ValueStart);
	Value = S_ALLOC(11 + Len * 3);
	if ( Value == NULL ) {
		return FALSE;
	}

	// �R�s�[
	// Expire
	*Refresh = Value;
	if ( Expire == 0 && ItemOauthConfig->Expire == 0 ) {
		*Value++ = _T('0');
	} else {
		time(&UtcSec);
		if ( Expire ) {
			UtcSec += Expire;
		} else {
			UtcSec += ItemOauthConfig->Expire;
		}
		_stprintf_s(Value, 11, _T("%d"), UtcSec);
		Value += 10;
	}
	*Value++ = _T('/');
	// RefreshToken
	if ( Len ) {
		lstrcpyn(Value + Len * 2, ValueStart, Len + 1);
		if ( ConvStrToUri(Value + Len * 2, Value, Len * 3) == FALSE ) {
			return FALSE;
		}
	} else {
		*Value = _T('\0');
	}

	
	return TRUE;
}



// OAuth 1.0 �� token credentials �𓾂�
BOOL GetOauthToken1(HWND hDlg, struct TPITEM *tpItemInfo, LPTSTR Verifier) {
	struct TPITEM *OauthItem = NULL;
	struct OAUTHPARAM *OauthParam = NULL;
	struct OAUTHCONFIG *ItemOauthConfig = NULL;


	if ( tpItemInfo == NULL
		|| tpItemInfo->ITEM_OAUTH == NULL
		|| Verifier == NULL
	) {
		return FALSE;
	}


	OauthItem = (struct TPITEM *)tpItemInfo->ITEM_OAUTH;
	OauthParam = (struct OAUTHPARAM *)OauthItem->ITEM_OAUTH;
	ItemOauthConfig = OauthParam->OauthConfig;
	tpItemInfo->ITEM_STATE = OauthItem->ITEM_STATE = (ITEM_STATE_TYPE)OAUTH_STATE_TOKEN1;

	// �擾�p�ݒ�
	OauthParam->CallbackUri = NULL;
	OauthParam->Verifier = Verifier;
	// URI
	OauthItem->CheckURL = S_ALLOC(lstrlen(ItemOauthConfig->TokenUri));
	if ( OauthItem->CheckURL == NULL ) {
		return FALSE;
	}
	lstrcpy(OauthItem->CheckURL, ItemOauthConfig->TokenUri);
	OauthItem->Param2 = (long)OauthItem->CheckURL;
	// �\�[�X�\���p�t���O�A�{�̎擾�֐i�ނ���
	OauthItem->Param3 = 2;


	// ���N�G�X�g
	if ( HTTP_Start(hDlg, OauthItem) == CHECK_ERROR ) {
		return FALSE;
	}

	return TRUE;
}



// OAuth 2.0 �� token credentials �𓾂�
BOOL GetOauthToken2(HWND hDlg, struct TPITEM *tpItemInfo, LPTSTR Verifier) {
	struct TPITEM *OauthItem = NULL;
	struct OAUTHCONFIG *ItemOauthConfig = NULL;
	LPCTSTR OptionPostFormat = _T("3;;;;;;;;;;;;;;;;;;grant_type=authorization_code&code=%s&client_id=%s&client_secret=%s&redirect_uri=%s;;");
	int Len = 0;


	if ( tpItemInfo == NULL
		|| tpItemInfo->ITEM_OAUTH == NULL
		|| Verifier == NULL
	) {
		return FALSE;
	}


	ItemOauthConfig = (struct OAUTHCONFIG *)tpItemInfo->ITEM_OAUTH;

	// �擾�p�ݒ�
	OauthItem = M_ALLOC_Z(sizeof(struct TPITEM));
	if ( OauthItem == NULL ) {
		return FALSE;
	}
	tpItemInfo->ITEM_OAUTH = (ITEM_OAUTH_TYPE)OauthItem;
	tpItemInfo->ITEM_STATE = OauthItem->ITEM_STATE = (ITEM_STATE_TYPE)OAUTH_STATE_TOKEN2;
	OauthItem->ITEM_OAUTH = (ITEM_OAUTH_TYPE)ItemOauthConfig;
	// URI
	OauthItem->CheckURL = S_ALLOC(lstrlen(ItemOauthConfig->TokenUri));
	if ( OauthItem->CheckURL == NULL ) {
		return FALSE;
	}
	lstrcpy(OauthItem->CheckURL, ItemOauthConfig->TokenUri);
	OauthItem->Param2 = (long)OauthItem->CheckURL;

	// �\�[�X�\���p�t���O�A�{�̎擾�֐i�ނ���
	OauthItem->Param3 = 2;


	// �I�v�V�����^�u�̊����ݒ���R�s�[
	if ( HAS_STR(tpItemInfo->Option2) ) {
		OauthItem->Option2 = S_ALLOC(lstrlen(tpItemInfo->Option2));
		if ( OauthItem->Option2 == NULL ) {
			return FALSE;
		}
		lstrcpy(OauthItem->Option2, tpItemInfo->Option2);
		// �F�؂𖳌�
		SetOptionNum(OauthItem->Option2, lstrlen(tpItemInfo->Option2) + 1, OP2_USEPASS, AUTH_OFF);
	}


	// �`�F�b�N�^�u��POST
	Len = lstrlen(OptionPostFormat)
		+ lstrlen(Verifier)
		+ lstrlen(ItemOauthConfig->ClientIdentifier)
		+ lstrlen(ItemOauthConfig->ClientSecret)
		+ lstrlen(ItemOauthConfig->CallbackUri);

	OauthItem->Option1 = S_ALLOC(Len);
	if ( OauthItem->Option1 == NULL ) {
		return FALSE;
	}
	_stprintf_s(OauthItem->Option1, Len + 1,
		OptionPostFormat, Verifier, ItemOauthConfig->ClientIdentifier,
		ItemOauthConfig->ClientSecret, ItemOauthConfig->CallbackUri);

	M_FREE(Verifier);


	// ���N�G�X�g
	if ( HTTP_Start(hDlg, OauthItem) == CHECK_ERROR ) {
		return FALSE;
	}


	return TRUE;
}



// OAuth �� token credentials �𓾂�
BOOL GetOauthToken(HWND hDlg, struct TPITEM *tpItemInfo, LPTSTR Verifier) {

	if ( tpItemInfo == NULL
		|| tpItemInfo->ITEM_OAUTH == NULL
		|| Verifier == NULL
	) {
		return FALSE;
	}


	if ( GET_OAUTH_VAR(tpItemInfo) == 1 ) {
		// OAuth 1.0
		return GetOauthToken1(hDlg, tpItemInfo, Verifier);

	} else if ( GET_OAUTH_VAR(tpItemInfo) == 2 ) {
		// OAuth 2.0
		return GetOauthToken2(hDlg, tpItemInfo, Verifier);
	}


	return FALSE;
}
