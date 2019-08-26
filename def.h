#include <windows.h>



// 自動チェックでGETへ
#define CONTINUE_GET					8


#define	ITEM_SITEURL					OldSize
#define ITEM_FILTER						OldDate
#define ITEM_UPINFO						ErrStatus
#define ITEM_PAGEDATA					DLLData2
#define ITEM_CANCEL						user1

#define ITEM_MARK						Soc1
#define ITEM_MARK_TYPE					int

#define ITEM_OAUTH						hGetHost1
#define ITEM_OAUTH_TYPE					HANDLE

#define ITEM_STATE						Soc2
#define ITEM_STATE_TYPE					int


#define M_ALLOC(SIZE)					GlobalAlloc(GMEM_FIXED, SIZE)
#define M_ALLOC_Z(SIZE)					GlobalAlloc(GPTR, SIZE)
#define M_FREE(VAR)						(VAR != NULL ? VAR = GlobalFree(VAR) : NULL)
#define M_SIZE(VAR)						GlobalSize(VAR)

#define S_ALLOC(LEN)					M_ALLOC(((LEN) + 1) * sizeof(TCHAR))
#define S_ALLOC_Z(LEN)					M_ALLOC_Z(((LEN) + 1) * sizeof(TCHAR))

#define HAS_STR(VAR)					(VAR != NULL && *(VAR) != _T('\0'))

#define STR_MEM_ERR						_T("メモリ確保失敗")


#define AUTH_OFF						0
#define AUTH_BASIC						0
#define	AUTH_BASIC_ON					1
#define AUTH_WSSE						2
#define	AUTH_WSSE_ON					3
#define	AUTH_OAUTH						4
#define	AUTH_OAUTH_ON					5



#ifdef _UNICODE
#define _tmemset wmemset
#define _tmemcpy wmemcpy
#define _tmemmove wmemmove
#else
#define _tmemset memset
#define _tmemcpy memcpy
#define _tmemmove memmove
#endif



#ifdef _DEBUG
#define MB(MSG, TITLE)					MessageBox(NULL, MSG, TITLE, MB_OK)
#else
#define MB(MSG, TITLE)
#endif

