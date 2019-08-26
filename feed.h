

#define REQ_REDIRECT					_T("ReqRdir")

#define FEED_FILTER_URL					_T("FeedFilUrl")
#define FEED_FILTER_TITLE				_T("FeedFilTit")
#define FEED_FILTER_CATEGORY			_T("FeedFilCat")
#define FEED_FILTER_CONTENT				_T("FeedFilCon")
#define FEED_FILTER_DESCRIPTION			_T("FeedFilDes")
#define FEED_FILTER_SOURCE				_T("FeedFilSou")

#define FEED_FILTER_TYPE_URL			_T("FeedFilTypeUrl")
#define FEED_FILTER_TYPE_TITLE			_T("FeedFilTypeTit")
#define FEED_FILTER_TYPE_CATEGORY		_T("FeedFilTypeCat")
#define FEED_FILTER_TYPE_CONTENT		_T("FeedFilTypeCon")
#define FEED_FILTER_TYPE_DESCRIPTION	_T("FeedFilTypeDes")
#define FEED_FILTER_TYPE_SOURCE			_T("FeedFilTypeSou")

#define DRAW_FILTER_ORDER				_T("DrawFilOrder")
#define DRAW_FILTER_URLP				_T("DrawFilUrlP")
#define DRAW_FILTER_URLF				_T("DrawFilUrlF")
#define DRAW_FILTER_INFOP				_T("DrawFilInfoP")
#define DRAW_FILTER_INFOF				_T("DrawFilInfoF")

#define FEED_TYPE_RSS					0x00010000
#define FEED_TYPE_ATOM					0x00020000
#define FEED_TYPE_RSS1					(FEED_TYPE_RSS | 100)
#define FEED_TYPE_RSS2					(FEED_TYPE_RSS | 200)
#define FEED_TYPE_ATOM03				(FEED_TYPE_ATOM | 30)
#define FEED_TYPE_ATOM1					(FEED_TYPE_ATOM | 100)

#define DRAW_TOP						0
#define DRAW_BOTTOM						1
#define DRAW_ALL						2

#define FEED_FILTER_TYPE_INDEX			{0, 1, 3, 2}



int GetEntries(struct TPITEM *tpItemInfo);
BOOL AddEntryTitle(struct TPITEM *tpItemInfo, LPTSTR UpInfo);
int GetLinks(struct TPITEM *tpItemInfo);
LPTSTR MakeUpInfo(LPCTSTR EntryTitle, int EntryTitleLen, struct TPITEM *tpItemInfo, int EntryCount);



// httprop.c
int GetOptionNum(LPTSTR OptionText, LPTSTR OptionName);
LPTSTR GetOptionText(LPTSTR OptionText, LPTSTR OptionName, BOOL MultiLine);


typedef struct ELEMINFO {
	LPSTR STag;
	LPSTR ETag;
	LPSTR Cont;
	int ElemLen;
	int STagLen;
	int ETagLen;
	int ContLen;
} ELEMINFO;

