// Replace1.h: interface for the CReplace class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_REPLACE1_H__AE9596A3_1C9B_11D6_8344_0001029EB3A0__INCLUDED_)
#define AFX_REPLACE1_H__AE9596A3_1C9B_11D6_8344_0001029EB3A0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#include "scmem.h"

#define FLAGS_REPLACE		0x01
#define FLAGS_RENAME		0x02
#define FLAGS_SUBDIRS		0x04
#define FLAGS_MATCHCASE		0x08
#define FLAGS_NOCHANGES		0x10

typedef struct REPLACE_INFO
{
	int iFlags;
	HWND hResultsDlg;
	SCMem Path;
	SCMem File;
	SCMem Replace;
	SCMem ReplaceWith;
	HANDLE hContinue;
}*PREPLACE_INFO;


class CReplace  
{
public:
	BOOL Replace(PREPLACE_INFO pInfo);
	CReplace();
	virtual ~CReplace();
private:
	BOOL RenameFile(LPCTSTR szNewFileName, LPCTSTR szOldFileName);
	BOOL ReplaceInFile(LPCTSTR szFileName, LPCTSTR szFileTitle);
	void AddToList(LPCTSTR szCol1, LPCTSTR szCol2);
	void AddToStatusWnd(LPCTSTR szStatus);
	BOOL ProcessDir(LPCTSTR szPath);
	PREPLACE_INFO m_pInfo;
};

#endif // !defined(AFX_REPLACE1_H__AE9596A3_1C9B_11D6_8344_0001029EB3A0__INCLUDED_)
