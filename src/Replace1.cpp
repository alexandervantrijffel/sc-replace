// Replace1.cpp: implementation of the CReplace class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "resource.h"

#include "Replace1.h"

#include "shlobj.h"  // ListView stuff
#include "stdio.h" // FILE, fopen


LPCTSTR GetLastErrorString(LPTSTR szMsg, DWORD nSize)
{
	FormatMessage( FORMAT_MESSAGE_FROM_SYSTEM, 
		NULL, GetLastError(), NULL,
		szMsg, nSize, NULL);
	
	return szMsg;
}

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CReplace::CReplace()
{
	m_pInfo=0;
}

CReplace::~CReplace()
{
}

BOOL CReplace::ProcessDir(LPCTSTR szPath)
{
	BOOL bRet=TRUE;

	SCMem Dir = szPath;
	Dir += "\\";
	WIN32_FIND_DATA fd={0};
	HANDLE hFind = FindFirstFile(Dir + m_pInfo->File, &fd);
	
	if (INVALID_HANDLE_VALUE==hFind)
	{
		AddToList("Error", "No files found");
		return FALSE;
	}

	AddToList("(Directory)", Dir);
	
	SCMem szFileName="";
	SCMem szFileTitle="";
	BOOL bFileRenamed;

	FILETIME ftNow;
	GetSystemTimeAsFileTime(&ftNow);
	
	do
	{
		if (m_pInfo->hContinue&&WaitForSingleObject(m_pInfo->hContinue, 10)==WAIT_OBJECT_0)
		{
			bRet=-1;
			
			// make sure that the other instances of CReplace::ProcesDir() get here
			SetEvent(m_pInfo->hContinue);
			break;
		}
		// check for dot(s)
		if (fd.cFileName[0] == '.')
			continue;
	
		szFileName = Dir;
		szFileName += fd.cFileName;
		
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			if (m_pInfo->iFlags & FLAGS_SUBDIRS)
				bRet *= ProcessDir(szFileName);
			else
				continue;
		}
		else 
		{
			// disregard the file if it's created after we started the search (.bak files)
			if (fd.ftLastWriteTime.dwHighDateTime > ftNow.dwHighDateTime)
			 continue;

			AddToStatusWnd(fd.cFileName);
			bFileRenamed=FALSE;

			if (m_pInfo->iFlags & FLAGS_RENAME)
			{
				szFileTitle=fd.cFileName;
				
				if ( szFileTitle.Replace(m_pInfo->Replace, m_pInfo->ReplaceWith
					, m_pInfo->iFlags & FLAGS_MATCHCASE))
				{
					szFileName = Dir;
					szFileName += szFileTitle;
					if (RenameFile(szFileName, Dir + fd.cFileName) )
					{
					    bFileRenamed=TRUE;
						AddToList(fd.cFileName, szFileTitle);					
					}
					else
					{
						if (char* pszErr=new char[320])
						{
							strcpy(pszErr, "Unable to rename - ");
							GetLastErrorString(pszErr+strlen(pszErr), 256);
							AddToList(fd.cFileName, pszErr);
							delete[] pszErr;
						}
					}

					// reset szFileName to the original if renaming failed or
					// if the user don't wan't to make any changes
					if (!bFileRenamed || m_pInfo->iFlags & FLAGS_NOCHANGES)
						szFileName = Dir + fd.cFileName;

				}
			}
			if (m_pInfo->iFlags & FLAGS_REPLACE)
			{
				ReplaceInFile(szFileName, (bFileRenamed)?szFileTitle:fd.cFileName);					
			}
		
		}
	}while(FindNextFile(hFind, &fd));

	FindClose(hFind);
	return bRet;
}


void CReplace::AddToList(LPCTSTR szCol1, LPCTSTR szCol2)
{
	
	if (!m_pInfo->hResultsDlg)
		return;
	
	HWND hList=GetDlgItem(m_pInfo->hResultsDlg, IDC_RESULTS);

	LVITEM lvi={0};
	lvi.mask = LVIF_TEXT;
	lvi.iItem = ListView_GetItemCount(hList);
	lvi.iSubItem=0;
	lvi.pszText = (LPTSTR)szCol1;
	ListView_InsertItem(hList, &lvi);

	lvi.iSubItem = 1;
	lvi.pszText = (LPTSTR)szCol2;
	ListView_SetItem(hList, &lvi);
}

BOOL CReplace::Replace(PREPLACE_INFO pInfo)
{
	m_pInfo = pInfo;
	BOOL bRet = ProcessDir(m_pInfo->Path);
	
	LPCTSTR szStatus;
	switch (bRet)
	{
		case 0:		szStatus = "Errors encountered";
			break;
		case -1:	szStatus = "Cancelled";
			break;
		default:	szStatus = "Finished";
	}
	AddToStatusWnd(szStatus);
	

	return bRet;
} 

BOOL CReplace::ReplaceInFile(LPCTSTR szFileName, LPCTSTR szFileTitle)
{
	SCMem szFileBak = szFileName;
	szFileBak += ".bak";
	
	FILE* fpo=0, *fpi=0;
	
	if (!(m_pInfo->iFlags & FLAGS_NOCHANGES))
	{
		// test backupfilename
		if (!(fpo = fopen(szFileBak, "w")))
		{
			// try with a extra extension
			szFileBak += ".scr";
			if (!(fpo = fopen(szFileBak, "w")))
			{
				AddToList(szFileTitle, "Unable to create intermediate file (disk full?)");
				return FALSE;
			}
		}
		fclose(fpo);

		// copy inputfile to backup
		if (!CopyFile(szFileName, szFileBak, FALSE))
		{
			AddToList(szFileTitle, "Unable to create backup file");
			return FALSE;
		}
				
		// open input (bakfile)
		if (!(fpi=fopen(szFileBak, "rb")))
		{
			// try to delete it
			DeleteFile(szFileBak);
			AddToList(szFileTitle, "Unable to open intermediate file");
			return FALSE;
		}

		// open outputfile (original filename)
		if (!(fpo=fopen(szFileName, "wb")))
		{
			SetFileAttributes(szFileName, FILE_ATTRIBUTE_ARCHIVE);
			if (!(fpo=fopen(szFileName, "wb")))
			{
				// todo: vragen om bevestiging van het overschrijven van readonlybestanden
				AddToList(szFileTitle, "Unable to open file");
				fclose(fpi);
				DeleteFile(szFileBak);
				return FALSE;
			}
		}
	}
	else
	{	
		// only open input file in readmode
		if (!(fpi=fopen(szFileName, "rb")))
			return FALSE;
	}

	UINT nDiff = __max(m_pInfo->Replace.SafeStrlen(),m_pInfo->ReplaceWith.SafeStrlen())
		- __min(m_pInfo->Replace.SafeStrlen(),m_pInfo->ReplaceWith.SafeStrlen());
	
	const int LineWidth=96;
	
	char* szLine = new char[ 2*LineWidth + nDiff*LineWidth 
		/ m_pInfo->Replace.SafeStrlen()];
	
	BOOL bLineChanged=FALSE;
	UINT icLine=0, icSame=0;
	UINT LenToReplace = m_pInfo->Replace.SafeStrlen();
	UINT LenReplaceWith = m_pInfo->ReplaceWith.SafeStrlen();

	while(fread(&szLine[icLine++], 1, 1, fpi))
	{	
		if ( SCMem::IsSameChar(
			   szLine[icLine-1]
			, m_pInfo->Replace[icSame]
			, m_pInfo->iFlags & FLAGS_MATCHCASE) )
			icSame++;
		else
			icSame=0;

		if (icSame>=LenToReplace)
		{
			// if the string to replace overlaps to lines
			if (icLine<icSame)
			{	//icsame 22 - Len replacewith 3
				int icPrevLine = icSame-icLine;
				if (fpo)
				{
					// set de filepointer terug met het aantal karakters van 
					// de te vervangen string die in de vorige szLine zaten
					fseek(fpo, - icPrevLine, SEEK_CUR);
					// schrijf de vervangende string
					fwrite(m_pInfo->ReplaceWith, 1, LenReplaceWith, fpo);
					// begin met een nieuwe szLine
					icLine=0;

					AddToList(szFileTitle, m_pInfo->ReplaceWith);
				}
			}
			else
			{
				memcpy(&szLine[icLine-icSame], m_pInfo->ReplaceWith, LenReplaceWith);
				icLine += LenReplaceWith-icSame;
				bLineChanged=TRUE;
			}
			
			icSame=0;
		}
		
		if (icLine>=LineWidth)
		{
			if (fpo)
			{
				// write current line to file
				if (!fwrite(szLine, 1, icLine, fpo))
				{
					AddToList(szFileTitle, "unable to write to output file");
					break;
				}
			}
			
			// add to list if something has changed
			if (bLineChanged)
			{
				// end with null
				szLine[icLine] = '\0';
				AddToList(szFileTitle, szLine);
			}
			
			bLineChanged = FALSE;
			
			// start with new line
			icLine=0;
			continue;
		}
	}
	BOOL bRet = feof(fpi);

	// add to list if something has changed
	if (bLineChanged)
	{
		// end with null
		szLine[icLine-1] = '\0';
		AddToList(szFileTitle, szLine);
	}
	
	// write last piece of line
	if (bRet&&icLine>1&&fpo)
	{
		if (!fwrite(szLine, 1, icLine-1, fpo))
			bRet = FALSE;
	}

	fclose(fpi);
	if (fpo)
		fclose(fpo);
	
	delete[] szLine;

	if (bRet)
	{
		if (fpo)
		{
			if (!DeleteFile(szFileBak))
			{
				// als szFileName een read-only bestand was is 
				// szFileBak dat ook
				SetFileAttributes(szFileBak, FILE_ATTRIBUTE_NORMAL);
				DeleteFile(szFileBak);
			}
		}
	}
	else
		AddToList(szFileName, "I/O Error");

	return bRet;
}

BOOL CReplace::RenameFile(LPCTSTR szNewFileName, LPCTSTR szOldFileName)
{
	if (m_pInfo->iFlags & FLAGS_NOCHANGES)
		return TRUE;

	BOOL bRet= CopyFile( szOldFileName, szNewFileName, TRUE);

	if (!bRet&& GetLastError() == ERROR_FILE_EXISTS)
	{
		char* pszMsg = new char[512];
		
		sprintf(pszMsg, "The file '%s' already exists.\nOverwrite?", szNewFileName);
		if (IDYES == AFXMSGBOX(m_pInfo->hResultsDlg, pszMsg, MB_YESNO|MB_ICONQUESTION) )
			bRet = CopyFile(szOldFileName, szNewFileName, FALSE);
		
		delete[] pszMsg;
	}
	
	if (bRet)
		DeleteFile(szOldFileName);

	return bRet;
}


void CReplace::AddToStatusWnd(LPCTSTR szStatus)
{
	if (m_pInfo->hResultsDlg)
		SetDlgItemText(m_pInfo->hResultsDlg, IDC_STATUS, szStatus);
}
