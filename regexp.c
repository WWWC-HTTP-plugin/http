
// onig.lib�Aonig.dll ���g��

#include <windows.h>
#include <tchar.h>

#include "oniguruma.h"
#include "regexp.h"
#include "def.h"




// �u����̕������𐔂���
int CountResultLen(LPCTSTR ReplaceStr, regex_t *RegExp, OnigRegion *Region) {
	int ResultLen = 0, BackRefNum;
	LPCTSTR rp = ReplaceStr, p;


	while (  *rp != _T('\0') ) {
		// 2�o�C�g����
		if ( IsDBCSLeadByte(*rp) == TRUE && *(rp + 1) != _T('\0') ) {
			rp += 2;
			ResultLen += 2;

		// �G�X�P�[�v�ƌ���Q��
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
					// �����ʂ͌��Ă��Ȃ�
					if ( (*(rp + 2) == _T('<') || *(rp + 2) == _T('\'')) ) {
						rp += 3;
						if ( *rp >= _T('0') && *rp <= _T('9') ) {
							// �ԍ��w��Q��
							BackRefNum = atoi(rp);
							while ( *rp >= _T('0') && *rp <= _T('9') ) rp++;
							rp++;
							if ( BackRefNum >= 0 && BackRefNum < Region->num_regs ) {
								ResultLen += Region->end[BackRefNum] - Region->beg[BackRefNum];
							}
							break;
						
						} else if ( (*rp >= _T('A') && *rp <= _T('Z')) || (*rp >= _T('a') && *rp <= _T('z')) || *rp == _T('_') ) {
							// ���O�w��Q��
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
					// ����Q�Ƃ��ۂ��Ȃ���΁���

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



// �u�����ʂ̕���������A���ʕ�����̏I���ӏ���Ԃ�
LPTSTR MakeResultStr(LPCTSTR TargetStr, LPCTSTR ReplaceStr, regex_t *RegExp, OnigRegion *Region, OUT LPTSTR ResultStr) {
	int BackRefNum, RegionLen;
	LPTSTR sp = ResultStr;
	LPCTSTR rp = ReplaceStr, p;


	while ( *rp != _T('\0') ) {
		// 2�o�C�g����
		if ( IsDBCSLeadByte(*rp) == TRUE && *(rp + 1) != _T('\0') ) {
			*sp++ = *rp++;
			*sp++ = *rp++;

		// �G�X�P�[�v�ƌ���Q��
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
					// �����ʂ͌��Ă��Ȃ�
					if ( (*(rp + 2) == _T('<') || *(rp + 2) == _T('\'')) ) {
						rp += 3;
						if ( *rp >= _T('0') && *rp <= _T('9') ) {
							// �ԍ��w��Q��
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
							// ���O�w��Q��
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
					// ����Q�Ƃ��ۂ��Ȃ���΁���

				default :
					*sp++ = *rp++;
					break;
			}

		// �ʏ핶��
		} else {
			*sp++ = *rp++; 
		}
	}

	*sp = _T('\0');

	// �u�����ʂ̌��Ԃ�
	return sp;
}



// �Ώە������S�Ēu������
// �u�����ʂ̃������̈�����
// 1����}�b�`���Ȃ���� NULL ���Ԃ�
LPTSTR RegReplaceAll(LPCTSTR TargetStr, LPCTSTR PatternStr, LPCTSTR ReplaceStr) {
	regex_t *RegExp;
	LPCTSTR TargetStrEnd, SearchStart, SearchEnd;
	OnigRegion *Region;
	int TargetLen, ResultLen;
	LPTSTR ResultStr = NULL, ResultTemp = NULL;


	// ���K�\���I�u�W�F�N�g�����
	if ( onig_new(&RegExp, (OnigStr)PatternStr, (OnigStr)(PatternStr + lstrlen(PatternStr)),
					_ONIG_OPTION_DEFAULT, _ONIG_ENCODING_DEFAULT, _ONIG_SYNTAX_DEFAULT, NULL)
				!= ONIG_NORMAL
	) {
		onig_free(RegExp);
		return NULL;
	}

	// �����Ώېݒ�
	TargetLen = lstrlen(TargetStr);
	TargetStrEnd = TargetStr + TargetLen;
	SearchStart = TargetStr;
	SearchEnd = TargetStrEnd;

	// �������āA������Ȃ��Ȃ�܂Ń��[�v
	Region = onig_region_new(); // ���� new & free ���Ȃ��Ă��������ۂ�
	while ( onig_search(RegExp, (OnigStr)TargetStr, (OnigStr)TargetStrEnd,
				(OnigStr)SearchStart, (OnigStr)SearchEnd, Region, 0) != ONIG_MISMATCH	) {
		// �}�b�`�����̒u���㕶����
		ResultLen = CountResultLen(ReplaceStr, RegExp, Region);
		// �u�����ʑS�̂̕�����
		ResultLen += Region->beg[0] + (TargetLen - *Region->end);
		// ������
		ResultTemp = S_ALLOC(ResultLen);
		if ( ResultTemp == NULL ) {
			break;
		}

		// �}�b�`�͈͂̑O���R�s�[
		strncpy_s(ResultTemp, ResultLen + 1, TargetStr, *Region->beg);

		// �}�b�`�͈͂�u��
		// �}�b�`�I���ӏ����Ԃ�̂Ŏ��̊J�n�ʒu�Ƃ���
		SearchStart = MakeResultStr(TargetStr, ReplaceStr, RegExp, Region, ResultTemp + *Region->beg);

		// �}�b�`�͈͂̌���R�s�[
		strcat_s(ResultTemp, ResultLen + 1, TargetStr + *Region->end);

		M_FREE(ResultStr);
		ResultStr = ResultTemp;

		// ���̌����Ώېݒ�
		TargetLen = ResultLen;
		TargetStr = ResultStr;
		TargetStrEnd = TargetStr + TargetLen;
		SearchEnd = TargetStrEnd;
	}

	onig_region_free(Region, 1);
	onig_free(RegExp);

	return ResultStr;
}



// �������ă}�b�`�ʒu��Ԃ�
int RegMatch(LPCTSTR TargetStr, int StartIndex, LPCTSTR PatternStr, OnigRegion *Region, BOOL Reverse) {
	regex_t *RegExp;
	int Ret;

	// ���K�\���I�u�W�F�N�g�����
	Ret = onig_new(&RegExp, (OnigStr)PatternStr, (OnigStr)(PatternStr + lstrlen(PatternStr)),
		_ONIG_OPTION_DEFAULT, _ONIG_ENCODING_DEFAULT, _ONIG_SYNTAX_DEFAULT, NULL);

	// �������Ă��̂܂ܕԂ�
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


// �����ƒu����1�s���Ƃ̃��X�g�ɂ��u��
// �u�����ʂ̓�����������ĕԂ��A�܂��� NULL
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

