#include <windows.h>
#include "oniguruma.h"



#ifdef UNICODE
#define _ONIG_ENCODING_DEFAULT				ONIG_ENCODING_UTF16_LE
#else
#define _ONIG_ENCODING_DEFAULT				ONIG_ENCODING_SJIS
#endif

#define _ONIG_OPTION_DEFAULT				ONIG_OPTION_MULTILINE | ONIG_OPTION_IGNORECASE | ONIG_OPTION_CAPTURE_GROUP
#define _ONIG_SYNTAX_DEFAULT				ONIG_SYNTAX_DEFAULT



typedef OnigUChar *		OnigStr;



int CountResultLen(LPCTSTR ReplaceStr, regex_t *RegExp, OnigRegion *Region);
LPTSTR MakeResultStr(LPCTSTR TargetStr, LPCTSTR ReplaceStr, regex_t *RegExp, OnigRegion *Region, OUT LPTSTR ResultStr);
LPTSTR RegReplaceAll(LPCTSTR TargetStr, LPCTSTR PatternStr, LPCTSTR ReplaceStr);
int RegMatch(LPCTSTR TargetStr, int StartIndex, LPCTSTR PatternStr, OnigRegion *Region, BOOL Reverse);
LPTSTR ListReplace(LPCTSTR OriginStr, LPTSTR RegStrList);

