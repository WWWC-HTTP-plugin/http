

#define OAUTH_SIGN_HMACSHA1				0
#define OAUTH_SIGN_RSASHA1				1
#define OAUTH_SIGN_PLAINTEXT			2

#define OAUTH_STATE_FLAG_ITEM			0x0100
#define OAUTH_STATE_FLAG_PARAM			0x0200

#define OAUTH_STATE_VAR1				0x0001
#define OAUTH_STATE_VAR2				0x0002

#define OAUTH_STATE_TEMP1				(OAUTH_STATE_VAR1 | OAUTH_STATE_FLAG_ITEM | OAUTH_STATE_FLAG_PARAM | 0x0010)
#define OAUTH_STATE_TOKEN1				(OAUTH_STATE_VAR1 | OAUTH_STATE_FLAG_ITEM | OAUTH_STATE_FLAG_PARAM | 0x0020)
#define OAUTH_STATE_TOKEN2				(OAUTH_STATE_VAR2 | OAUTH_STATE_FLAG_ITEM | 0x0010)

#define GET_OAUTH_VAR(ITEM)				((ITEM) != NULL ? ((int)(((struct TPITEM*)(ITEM))->ITEM_STATE) & 0x000F) : 0)
#define HAS_OAUTH_ITEM(ITEM)			((ITEM) != NULL && ((int)(((struct TPITEM*)(ITEM))->ITEM_STATE) & OAUTH_STATE_FLAG_ITEM) && ((struct TPITEM*)(ITEM))->ITEM_OAUTH != NULL)
#define HAS_OAUTH_PARAM(ITEM)			((ITEM) != NULL && ((int)(((struct TPITEM*)(ITEM))->ITEM_STATE) & OAUTH_STATE_FLAG_PARAM) && ((struct TPITEM*)(ITEM))->ITEM_OAUTH != NULL)


struct OAUTHCONFIG {
	TCHAR HostName[BUFSIZE];
	TCHAR PathName[BUFSIZE];
	int Version;
	int ReqMethod;
	int SignMethod;
	int Expire;
	TCHAR Realm[BUFSIZE];
	TCHAR ClientIdentifier[BUFSIZE];
	TCHAR ClientSecret[BUFSIZE];
	TCHAR TemporaryUri[BUFSIZE];
	TCHAR CallbackUri[BUFSIZE];
	TCHAR Scope[BUFSIZE];
	TCHAR AuthorizationUri[BUFSIZE];
	TCHAR TokenUri[BUFSIZE];
	struct OAUTHCONFIG *Next;
};


struct OAUTHPARAM {
	int ReqMethod;
	LPTSTR Identifier;
	LPTSTR Secret;
	LPTSTR CallbackUri;
	LPTSTR Verifier;
	struct OAUTHCONFIG *OauthConfig;
};



int GetSha1(IN LPBYTE OrgnData, IN int OrgnDataSize, OUT LPBYTE HashData);
int GetHmacSha1(IN LPBYTE KeyData, IN int KeyDataSize, IN LPBYTE MessData, IN int MessDataSize, OUT LPBYTE HmacData);

BOOL ConvStrToUri(IN LPCSTR OriStr, OUT LPSTR EncStr, IN int EncStrSize);

BOOL MakeWsseValue(LPTSTR user, LPTSTR pass, OUT LPTSTR WsseValue, int WsseValueSize);

struct OAUTHCONFIG * GetOauthConfig(LPCTSTR HostName, LPCTSTR PathName);
BOOL MakeOauthValue(struct TPITEM *tpItemInfo, IN LPTSTR Identifier, IN LPTSTR Secret, OUT LPTSTR OauthValue, IN int OauthValueSize);
BOOL GetOauthStart(HWND hDlg, struct TPITEM *tpItemInfo);
BOOL ParseOauthCredentials1(IN LPCSTR Credentials, OUT LPTSTR *Token, OUT LPTSTR *Secret);
BOOL ParseOauthCredentials2(IN LPCSTR Credentials, OUT LPTSTR *Token, OUT LPTSTR *Refresh, struct OAUTHCONFIG *ItemOauthConfig);
BOOL GetOauthToken(HWND hDlg, struct TPITEM *tpItemInfo, LPTSTR Verifier);



extern struct OAUTHCONFIG *OauthConfig;
