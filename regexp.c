
// onig.lib、onig.dll を使う

#include <windows.h>
#include <tchar.h>

#include "oniguruma.h"
#include "regexp.h"
#include "def.h"




// 置換後の文字数を数える
int CountResultLen(LPCTSTR ReplaceStr, regex_t *RegExp, OnigRegion *Region) {
	int ResultLen = 0, BackRefNum;
	LPCTSTR rp = ReplaceStr, p;


	while (  *rp != _T('\0') ) {
		// 2バイト文字
		if ( IsDBCSLeadByte(*rp) == TRUE && *(rp + 1) != _T('\0') ) {
			rp += 2;
			ResultLen += 2;

		// エスケープと後方参照
		} else if ( *rp == _T('\\') ) {
			switch ( *(rp + 1) ) {
				case _T('\\') :
					rp += 2;
					ResultLen++;
					break;

				case _T('0') :
				case _T('1') :
				case _T('2') :
				case _T('3') :
				case _T('4') :
				case _T('5') :
				case _T('6') :
				case _T('7') :
				case _T('8') :
				case _T('9') :
					rp++;
					BackRefNum = atoi(rp);
					if ( BackRefNum >= 0 && BackRefNum < Region->num_regs ) {
						ResultLen += Region->end[BackRefNum] - Region->beg[BackRefNum];
					}
					while ( *rp >= _T('0') && *rp <= _T('9') ) rp++;
					break;

				case _T('k') :
					// 閉じ括弧は見ていない
					if ( (*(rp + 2) == _T('<') || *(rp + 2) == _T('\'')) ) {
						rp += 3;
						if ( *rp >= _T('0') && *rp <= _T('9') ) {
							// 番号指定参照
							BackRefNum = atoi(rp);
							while ( *rp >= _T('0') && *rp <= _T('9') ) rp++;
							rp++;
							if ( BackRefNum >= 0 && BackRefNum < Region->num_regs ) {
								ResultLen += Region->end[BackRefNum] - Region->beg[BackRefNum];
							}
							break;
						
						} else if ( (*rp >= _T('A') && *rp <= _T('Z')) || (*rp >= _T('a') && *rp <= _T('z')) || *rp == _T('_') ) {
							// 名前指定参照
							p = rp;
							while ( (*p >= _T('A') && *p <= _T('Z')) || (*p >= _T('a') && *p <= _T('z')) || (*p >= _T('0') && *p <= _T('9')) || *p == _T('_')) p++;
							BackRefNum = onig_name_to_backref_number(RegExp, rp, p, Region);
							rp = ++p;
							if ( BackRefNum >= 0 && BackRefNum < Region->num_regs ) {
								ResultLen += Region->end[BackRefNum] - Region->beg[BackRefNum];
							}
							break;
						} else {
							rp -= 3;
						}
					}
					// 後方参照っぽくなければ↓へ

				default :
					rp++;
					ResultLen++;
					break;
			}
		} else {
			rp++;
			ResultLen++;
		}
	}

	return ResultLen;
}



// 置換結果の文字列を作り、結果文字列の終了箇所を返す
LPTSTR MakeResultStr(LPCTSTR TargetStr, LPCTSTR ReplaceStr, regex_t *RegExp, OnigRegion *Region, OUT LPTSTR ResultStr) {
	int BackRefNum, RegionLen;
	LPTSTR sp = ResultStr;
	LPCTSTR rp = ReplaceStr, p;


	while ( *rp != _T('\0') ) {
		// 2バイト文字
		if ( IsDBCSLeadByte(*rp) == TRUE && *(rp + 1) != _T('\0') ) {
			*sp++ = *rp++;
			*sp++ = *rp++;

		// エスケープと後方参照
		} else if ( *rp == _T('\\') ) {
			switch ( *(rp + 1) ) {
				case _T('\\') :
					*sp++ = *rp;
					rp += 2;
					break;

				case _T('0') :
				case _T('1') :
				case _T('2') :
				case _T('3') :
				case _T('4') :
				case _T('5') :
				case _T('6') :
				case _T('7') :
				case _T('8') :
				case _T('9') :
					rp++;
					BackRefNum = atoi(rp);
					if ( BackRefNum >= 0 && BackRefNum < Region->num_regs ) {

						strncpy_s(sp, *(Region->end + BackRefNum) - *(Region->beg + BackRefNum) + 1, TargetStr + Region->beg[BackRefNum], Region->end[BackRefNum] - Region->beg[BackRefNum]);
						sp += *(Region->end + BackRefNum) - *(Region->beg + BackRefNum);
					}
					while ( *rp >= _T('0') && *rp <= _T('9') ) rp++;
					break;

				case _T('k') :
					// 閉じ括弧は見ていない
					if ( (*(rp + 2) == _T('<') || *(rp + 2) == _T('\'')) ) {
						rp += 3;
						if ( *rp >= _T('0') && *rp <= _T('9') ) {
							// 番号指定参照
							BackRefNum = atoi(rp);
							while ( *rp >= _T('0') && *rp <= _T('9') ) rp++;
							rp++;
							if ( BackRefNum >= 0 && BackRefNum < Region->num_regs ) {
								RegionLen = *(Region->end + BackRefNum) - *(Region->beg + BackRefNum);
								_tcsncpy_s(sp, RegionLen + 1, TargetStr + *(Region->beg + BackRefNum), RegionLen);
								sp += RegionLen;
							}
							break;
						
						} else if ( (*rp >= _T('A') && *rp <= _T('Z')) || (*rp >= _T('a') && *rp <= _T('z')) || *rp == _T('_') ) {
							// 名前指定参照
							p = rp;
							while ( (*p >= _T('A') && *p <= _T('Z')) || (*p >= _T('a') && *p <= _T('z')) || (*p >= _T('0') && *p <= _T('9')) || *p == _T('_')) p++;
							BackRefNum = onig_name_to_backref_number(RegExp, rp, p, Region);
							rp = ++p;
							if ( BackRefNum >= 0 && BackRefNum < Region->num_regs ) {
								RegionLen = *(Region->end + BackRefNum) - *(Region->beg + BackRefNum);
								_tcsncpy_s(sp, RegionLen + 1, TargetStr + *(Region->beg + BackRefNum), RegionLen);
								sp += RegionLen;
							}
							break;
						} else {
							rp -= 3;
						}
					}
					// 後方参照っぽくなければ↓へ

				default :
					*sp++ = *rp++;
					break;
			}

		// 通常文字
		} else {
			*sp++ = *rp++; 
		}
	}

	*sp = _T('\0');

	// 置換結果の後を返す
	return sp;
}



// 対象文字列を全て置換する
// 置換結果のメモリ領域を作る
// 1回もマッチしなければ NULL が返る
LPTSTR RegReplaceAll(LPCTSTR TargetStr, LPCTSTR PatternStr, LPCTSTR ReplaceStr) {
	regex_t *RegExp;
	LPCTSTR TargetStrEnd, SearchStart, SearchEnd;
	OnigRegion *Region;
	int TargetLen, ResultLen;
	LPTSTR ResultStr = NULL, ResultTemp = NULL;


	// 正規表現オブジェクトを作る
	if ( onig_new(&RegExp, (OnigStr)PatternStr, (OnigStr)(PatternStr + lstrlen(PatternStr)),
					_ONIG_OPTION_DEFAULT, _ONIG_ENCODING_DEFAULT, _ONIG_SYNTAX_DEFAULT, NULL)
				!= ONIG_NORMAL
	) {
		onig_free(RegExp);
		return NULL;
	}

	// 検索対象設定
	TargetLen = lstrlen(TargetStr);
	TargetStrEnd = TargetStr + TargetLen;
	SearchStart = TargetStr;
	SearchEnd = TargetStrEnd;

	// 検索して、見つからなくなるまでループ
	Region = onig_region_new(); // 毎回 new & free しなくてもいいっぽい
	while ( onig_search(RegExp, (OnigStr)TargetStr, (OnigStr)TargetStrEnd,
				(OnigStr)SearchStart, (OnigStr)SearchEnd, Region, 0) != ONIG_MISMATCH	) {
		// マッチ部分の置換後文字数
		ResultLen = CountResultLen(ReplaceStr, RegExp, Region);
		// 置換結果全体の文字数
		ResultLen += Region->beg[0] + (TargetLen - *Region->end);
		// メモリ
		ResultTemp = S_ALLOC(ResultLen);
		if ( ResultTemp == NULL ) {
			break;
		}

		// マッチ範囲の前をコピー
		strncpy_s(ResultTemp, ResultLen + 1, TargetStr, *Region->beg);

		// マッチ範囲を置換
		// マッチ終了箇所が返るので次の開始位置とする
		SearchStart = MakeResultStr(TargetStr, ReplaceStr, RegExp, Region, ResultTemp + *Region->beg);

		// マッチ範囲の後をコピー
		strcat_s(ResultTemp, ResultLen + 1, TargetStr + *Region->end);

		M_FREE(ResultStr);
		ResultStr = ResultTemp;

		// 次の検索対象設定
		TargetLen = ResultLen;
		TargetStr = ResultStr;
		TargetStrEnd = TargetStr + TargetLen;
		SearchEnd = TargetStrEnd;
	}

	onig_region_free(Region, 1);
	onig_free(RegExp);

	return ResultStr;
}



// 検索してマッチ位置を返す
int RegMatch(LPCTSTR TargetStr, int StartIndex, LPCTSTR PatternStr, OnigRegion *Region, BOOL Reverse) {
	regex_t *RegExp;
	int Ret;

	// 正規表現オブジェクトを作る
	Ret = onig_new(&RegExp, (OnigStr)PatternStr, (OnigStr)(PatternStr + lstrlen(PatternStr)),
		_ONIG_OPTION_DEFAULT, _ONIG_ENCODING_DEFAULT, _ONIG_SYNTAX_DEFAULT, NULL);

	// 検索してそのまま返す
	if ( Ret == ONIG_NORMAL ) {
		Ret = onig_search(RegExp,
			(OnigStr)TargetStr,
			(OnigStr)(TargetStr + lstrlen(TargetStr)),
			(OnigStr)(TargetStr + StartIndex),
			(OnigStr)(Reverse ? TargetStr : TargetStr + lstrlen(TargetStr)),
			Region,
			0
		);
	}

	onig_free(RegExp);
	return Ret;
}


// 検索と置換が1行ごとのリストによる置換
// 置換結果はメモリを作って返す、または NULL
//LPTSTR ListReplace(struct TPITEM *tpItemInfo, LPCTSTR OriginStr) {
LPTSTR ListReplace(LPCTSTR OriginStr, LPTSTR RegStrList) {
	LPCTSTR TargetStr;
	LPTSTR Context, PatternStr, ReplaceStr,
		ResultTemp = NULL, ResultStr = NULL;

	
	Context = RegStrList;
	TargetStr = OriginStr;
	while ( (PatternStr = _tcstok_s(NULL, _T("\r\n"), &Context)) != NULL ) {
		ReplaceStr = _tcstok_s(NULL, _T("\r\n"), &Context);
		if ( ReplaceStr == NULL ) {
			break;
		}

		ResultTemp = RegReplaceAll(TargetStr, PatternStr, ReplaceStr);
		if (ResultTemp != NULL ) {
			M_FREE(ResultStr);
			TargetStr = ResultStr = ResultTemp;
		}
	}

	return ResultStr;
}

