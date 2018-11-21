// Replace.cpp : Defines the entry point for the application.
//
// History:
// 
// version 1.0.001
// added/changed:
// added shell command support (included SCR in contextmenu's of explorer for folders)
// minor bug: if the user don't want to include subdirs SCR reported a warning
// when it tried to change the name of a folder
//
// ToDo:(A = afgehandeld)
// 1. als een gebruiker een bestand uit een andere directory kiest
//    dan de directory die gespecificeerd is in IDC_DIRECTORY
//    moet IDC_DIRECTORY aangepast worden
//
// 2. + knop naast IDC_BROWSEF om extra bestanden toe te voegen
//
//A3. Case-Sensitive/InSensitive keuze verwerken
//	  als een bestand eenzelfde naam moet krijgen alleen met een 
//    ander case dient het bestand 2 keer hernoemd te worden anders
//    accepteert windows het waarschijnlijk niet
//
// 4. Text voor elke regel kunnen plaatsen
//
// 5. Knop om alle gegevens te wissen
//
//A6. Lege Replace With toestaan
//
//A7. Thread netjes synchroniseren: als het programma wordt afgesloten
//	  moet dat alle lopende threads informeren zodat zij het gealloceerde
//    geheugen kunnen vrijgeven
//
//A8. Mogelijkheid tot het stoppen van het vervangen
//
//A9. Testen: replacement string met 1 karakter meer, 1 karakter minder
//	  en evenveel karakters als de te vervangen string
//
//A10.Soms worden .bak.bak bestanden aangemaakt
//
// 11. backup mogelijkheden
//
// 12. dialoogvenster maken voor als read-only bestanden overschreven moeten
//     worden of als een bestand al bestaat met de mogelijkheden om
//     het dialoogvenster niet meer op te laten komen
//	   (don't ask me this again for this session)
//
// 13. Foldernamen kunnen maken
//

#include "stdafx.h"
#include "resource.h"

#include "shlobj.h"  // browse for folder
#include "commdlg.h" // browse for file
#include "Prsht.h" // propertysheets
#include "shellapi.h"

#include "replace1.h"
#include "scmem.h"

HINSTANCE g_hInstance=0;
HANDLE g_hBrowse=0;
HWND g_hResults=0;
HANDLE g_hReplaceThread=0;
HANDLE g_hReplaceContinue=0;

BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DialogProcResults(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL CALLBACK DialogProcAbout( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

long RegCreateFullKeyPath(const HKEY hRootKey, LPCTSTR szPath, REGSAM samDesired, HKEY* phKeyResult);
BOOL RegSetting(LPCTSTR szName, LPTSTR szValue, BOOL bStore);
void RegisterForExplorer();

#define IDP_UNKNOWN		"Operation failed due to a unknown error"
#define IDP_FILTER		"All files (*.*)|*.*|Text files|*.txt;*.log;*.ini|Web files|*.html;*.htm;*.js;*.php;*.inc;*.asp;*.css;*.cfm;*.cfml;*.shtm;*.shtml|Source files|*.c;*.cpp;*.def;*.asm;*.h||";
#define IDP_NOCHANGES	"Nothing to change:\nThe replace and replace with fields are the same!"

#define IDP_SHELLCMD	"Folder\\Shell\\Replace with SCR"
#define IDP_SHELLCMD2	"\\command"
#define IDP_SCREPLACECONTINUE "ContinueSCR"
#define WM_REPLACEACTIVE WM_APP+1


UINT ReplaceThread(LPVOID lpParam)
{
	PREPLACE_INFO pInfo = (PREPLACE_INFO) lpParam;
	int iItem=0;
	
	ListView_DeleteAllItems(GetDlgItem(pInfo->hResultsDlg, IDC_RESULTS));
	
	pInfo->hContinue = OpenEvent(EVENT_ALL_ACCESS, FALSE, IDP_SCREPLACECONTINUE);

	::SendMessage(pInfo->hResultsDlg, WM_REPLACEACTIVE, TRUE, 0L);

	CReplace rep;
	rep.Replace(pInfo);

	if (pInfo->hContinue)
		CloseHandle(pInfo->hContinue);

	::SendMessage(pInfo->hResultsDlg, WM_REPLACEACTIVE, FALSE, 0L);

	delete pInfo;
	return 0;
}


int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	// supported cmdlineoptions:
	// -d (directory to start with)
	g_hInstance=hInstance;

	g_hBrowse = LoadImage(g_hInstance, MAKEINTRESOURCE(IDB_BROWSE)
		, IMAGE_BITMAP, 0, 0, 0);

	RegisterForExplorer();

	HPROPSHEETPAGE hpsp[3];
	PROPSHEETPAGE psp={0};
	psp.dwSize = sizeof(psp);
	psp.dwFlags = PSP_DEFAULT;
	psp.hInstance = g_hInstance;
	
	// main page
	psp.pszTemplate = MAKEINTRESOURCE(IDD_DLG_MAIN);
	psp.pfnDlgProc = &DialogProc;
	hpsp[0] = CreatePropertySheetPage(&psp);

	// status page
	psp.pszTemplate = MAKEINTRESOURCE(IDD_DLG_RESULTS);
	psp.pfnDlgProc = &DialogProcResults;
	hpsp[1] = CreatePropertySheetPage(&psp);

	// about page
	psp.pszTemplate = MAKEINTRESOURCE(IDD_DLG_ABOUT);
	psp.pfnDlgProc = &DialogProcAbout;
	hpsp[2] = CreatePropertySheetPage(&psp);

	// propertysheet
	PROPSHEETHEADER psh={0};
	psh.dwSize = sizeof(psh);
	psh.dwFlags = PSH_NOAPPLYNOW;
	psh.pszCaption = "SCR   -   The replacement tool";
	psh.nPages = 3;
	psh.nStartPage = 0;
	psh.phpage = &hpsp[0];
	
	// start sheets & papers
	PropertySheet(&psh);

	// cleanup
 	if (g_hBrowse)
		DeleteObject(g_hBrowse);

	return 0;
}

void BrowseForFolder(HWND hwndParent)
{
	LPMALLOC pMalloc;
	/* Gets the Shell's default allocator */
	if (::SHGetMalloc(&pMalloc) != NOERROR)
	{
		AFXMSGBOX(hwndParent, IDP_UNKNOWN, MB_ICONSTOP);
		return;
	}
	
	char szPath[MAX_PATH]="";
	LPITEMIDLIST pidl;
		
	BROWSEINFO bi={0};

	bi.hwndOwner = hwndParent;
	bi.pszDisplayName = szPath;
	bi.ulFlags = BIF_BROWSEINCLUDEFILES|BIF_EDITBOX;
	
					
	// This next call issues the dialog box.
	if ((pidl = ::SHBrowseForFolder(&bi)) != NULL)
	{
		if (::SHGetPathFromIDList(pidl, szPath))
		{ 
			// At this point pszBuffer contains the selected path */.
			// check if the user selected a file
			if (!(GetFileAttributes(szPath)&FILE_ATTRIBUTE_DIRECTORY))
			{
				// get filename
				LPTSTR szFileName = strrchr(szPath, '\\');
				SetDlgItemText(hwndParent, IDC_FILE, szFileName+1);
				
				// exclude filename from szPath
				*szFileName = '\0';
			}

			SetDlgItemText(hwndParent, IDC_DIRECTORY, szPath);

		}
		
		// Free the PIDL allocated by SHBrowseForFolder.
		pMalloc->Free(pidl);
	}
	// Release the shell's allocator.
	pMalloc->Release();
}

void BrowseForFile(HWND hwndDlg)
{
	OPENFILENAME ofn ={0};
	char szFileName[MAX_PATH]="";
	char szPath[MAX_PATH]="";

	GetDlgItemText(hwndDlg, IDC_DIRECTORY, szPath, MAX_PATH);
	
	ofn.lStructSize   = sizeof(OPENFILENAME); 
	ofn.hwndOwner     = hwndDlg; 
	ofn.hInstance     = (HINSTANCE) g_hInstance; 
	ofn.lpstrFilter   = IDP_FILTER;
	ofn.nFilterIndex  = 1L; 
	ofn.lpstrFileTitle= (LPSTR) szFileName; 
	ofn.nMaxFileTitle = MAX_PATH; 
	ofn.lpstrTitle    = 0; 
	ofn.lpstrInitialDir = szPath; 
	ofn.Flags = OFN_HIDEREADONLY | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;

	if (GetOpenFileName(&ofn))
		SetDlgItemText(hwndDlg, IDC_FILE, szFileName);
}

BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL bRet=TRUE;

	switch(uMsg)
	{
		case WM_COMMAND:
		{	
			switch(wParam)
			{	
				case IDC_BROWSED:
					BrowseForFolder(hwndDlg);
					break;
				
				case IDC_BROWSEF:
					BrowseForFile(hwndDlg);
					break;
				
				case ID_ABOUT :
					ShellExecute(0, "open", "http://www.soft-central.net", 0, 0, SW_SHOW);
					break;	
				
				default:
					bRet=FALSE;
			}
			break;
		}
		case WM_NOTIFY:
    		switch (((NMHDR FAR *) lParam)->code) 
    		{
  				case PSN_APPLY:
				{
					// check if a thread is running
					if ( g_hReplaceThread )
					{
						if (WaitForSingleObject(g_hReplaceThread, 0)==WAIT_TIMEOUT)
							SendMessage(GetParent(hwndDlg), PSM_SETCURSELID, 0, (LPARAM)IDD_DLG_RESULTS);
						else
						{
							CloseHandle(g_hReplaceThread);
							g_hReplaceThread=0;
						}

					}
					
					PREPLACE_INFO pInfo = new REPLACE_INFO;
					ZeroMemory(pInfo, sizeof(pInfo));
					char szBuf[MAX_PATH];

					try
					{
						// boolean flags
						if (SendMessage(GetDlgItem(hwndDlg, IDC_BRENAME), BM_GETCHECK, 0, 0L))
							pInfo->iFlags |= FLAGS_RENAME;
						if (SendMessage(GetDlgItem(hwndDlg, IDC_BREPLACE), BM_GETCHECK, 0, 0L))
							pInfo->iFlags |= FLAGS_REPLACE;
						if (SendMessage(GetDlgItem(hwndDlg, IDC_BSUBDIRS), BM_GETCHECK, 0, 0L))
							pInfo->iFlags |= FLAGS_SUBDIRS;
						if (SendMessage(GetDlgItem(hwndDlg, IDC_BCASESENSITIVE), BM_GETCHECK, 0, 0L))
							pInfo->iFlags |= FLAGS_MATCHCASE;
						if (SendMessage(GetDlgItem(hwndDlg, IDC_BNOCHANGES), BM_GETCHECK, 0, 0L))
							pInfo->iFlags |= FLAGS_NOCHANGES;

						// path
						GetDlgItemText(hwndDlg, IDC_DIRECTORY, szBuf, MAX_PATH);
						if (!strlen(szBuf))
							throw 1;
						pInfo->Path = szBuf;
						
						// file(s)
						GetDlgItemText(hwndDlg, IDC_FILE, szBuf, MAX_PATH);
						if (!strlen(szBuf))
							throw 2;
						pInfo->File = szBuf;

						// text to replace
						GetDlgItemText(hwndDlg, IDC_REPLACE, szBuf, MAX_PATH);
						if (!strlen(szBuf))
							throw 3;
						pInfo->Replace = szBuf;
						
						// replace with
						GetDlgItemText(hwndDlg, IDC_REPLACEWITH, szBuf, MAX_PATH);
						// null length string is OK
						pInfo->ReplaceWith = szBuf;

						if ( pInfo->iFlags & FLAGS_MATCHCASE
							&& 0 == strcmp(pInfo->Replace, pInfo->ReplaceWith)
							|| !(pInfo->iFlags & FLAGS_MATCHCASE )
							 && 0 == _stricmp(pInfo->Replace, pInfo->ReplaceWith) )
							throw 5;					
					}
					catch (int i)
					{
						delete pInfo;
												
						switch (i)
						{
							case 5: 
								AFXMSGBOX( hwndDlg, IDP_NOCHANGES, MB_ICONINFORMATION );
								break;
							default:
								AFXMSGBOX(hwndDlg, "Please fill in all fields", MB_ICONQUESTION);
						}
						SetWindowLong(hwndDlg, DWL_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
						break;
					}

					// store parameters into registry
					RegSetting("Path", (LPTSTR)(LPCTSTR)pInfo->Path, TRUE);
					RegSetting("File", (LPTSTR)(LPCTSTR)pInfo->File, TRUE);
					RegSetting("Replace", (LPTSTR)(LPCTSTR)pInfo->Replace, TRUE);
					RegSetting("ReplaceWith", (LPTSTR)(LPCTSTR)pInfo->ReplaceWith, TRUE);
					RegSetting("Flags", _itoa(pInfo->iFlags, szBuf, 10), TRUE);

					// create 'continue' event
					if (g_hReplaceContinue)
						CloseHandle(g_hReplaceContinue);
					g_hReplaceContinue = pInfo->hContinue =
						CreateEvent(NULL, FALSE, FALSE, IDP_SCREPLACECONTINUE);

					// activate resultspage
					SendMessage(GetParent(hwndDlg), PSM_SETCURSELID
						, 0, (LPARAM)IDD_DLG_RESULTS);

					// acquire HWNDS of results resources
					pInfo->hResultsDlg = (HWND) SendMessage( GetParent(hwndDlg)
						, PSM_GETCURRENTPAGEHWND, 0, 0L);
					
					// prevent the closure of the propertysheet
					SetWindowLong(hwndDlg, DWL_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
					
					// let's get to work
					DWORD dwID;
					g_hReplaceThread = CreateThread( NULL, NULL
						, (LPTHREAD_START_ROUTINE)&ReplaceThread
						,  (LPVOID)pInfo, NULL, &dwID);
		
					break;
				}
				default:
					bRet=FALSE;
			}
			break;

		case WM_INITDIALOG:
		{	
			HWND hParent = GetParent(hwndDlg);

			// Set Default Pushbutton to nothing
			::PostMessage(hwndDlg, DM_SETDEFID, 59955, 0L);

			// set browse bitmap
			if (g_hBrowse)
			{
				SendMessage(GetDlgItem(hwndDlg, IDC_BROWSED), BM_SETIMAGE
					, IMAGE_BITMAP, (LPARAM)g_hBrowse);

				SendMessage(GetDlgItem(hwndDlg, IDC_BROWSEF), BM_SETIMAGE
					, IMAGE_BITMAP, (LPARAM)g_hBrowse);
			}
			
			// set checkbox-checks
			char szBuf[256]="";
			if (RegSetting("Flags", szBuf, FALSE))
			{
				SendMessage(GetDlgItem(hwndDlg, IDC_BREPLACE), BM_SETCHECK
					, atoi(szBuf)&FLAGS_REPLACE, 0L);
				SendMessage(GetDlgItem(hwndDlg, IDC_BRENAME), BM_SETCHECK
					, atoi(szBuf)&FLAGS_RENAME, 0L);
				SendMessage(GetDlgItem(hwndDlg, IDC_BSUBDIRS), BM_SETCHECK
					, atoi(szBuf)&FLAGS_SUBDIRS, 0L);
				SendMessage(GetDlgItem(hwndDlg, IDC_BCASESENSITIVE), BM_SETCHECK
					, atoi(szBuf)&FLAGS_MATCHCASE, 0L);
				SendMessage(GetDlgItem(hwndDlg, IDC_BNOCHANGES), BM_SETCHECK
					, atoi(szBuf)&FLAGS_NOCHANGES, 0L);
			}
			else
			{
				// set default checkboxes
				SendMessage(GetDlgItem(hwndDlg, IDC_BREPLACE), BM_SETCHECK
					, TRUE, 0L);
				SendMessage(GetDlgItem(hwndDlg, IDC_BSUBDIRS), BM_SETCHECK
					, TRUE, 0L);
			}
			// check for directory cmdline option		
			char *psz = strstr(GetCommandLine(),"-d");
			if (psz)
				strcpy(szBuf, psz+3);
			else			
				RegSetting("Path", szBuf, FALSE);
			SetDlgItemText(hwndDlg, IDC_DIRECTORY, szBuf);

			if (RegSetting("File", szBuf, FALSE))
				SetDlgItemText(hwndDlg, IDC_FILE, szBuf);

			if (RegSetting("Replace", szBuf, FALSE))
			SetDlgItemText(hwndDlg, IDC_REPLACE, szBuf);

			if (RegSetting("ReplaceWith", szBuf, FALSE))
				SetDlgItemText(hwndDlg, IDC_REPLACEWITH, szBuf);
			
			// change text of buttons
			SetDlgItemText(hParent, IDOK, "Replace");
			SetDlgItemText(hParent, IDCANCEL, "Close");
			
			// set icon
			HICON hIcon = LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_MAIN));
			SendMessage(hParent, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
			
			bRet=FALSE;
			break;
		}
		default:
			bRet=FALSE;
	}

	return bRet;
}

BOOL CALLBACK DialogProcResults(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	BOOL bRet=TRUE;
	switch (uMsg)
	{
		case WM_COMMAND :
		 
			switch (wParam)
			{
				case IDC_RESULTSCANCEL:
				{
					if (g_hReplaceThread && g_hReplaceContinue
						&& WaitForSingleObject(g_hReplaceThread, 0)==WAIT_TIMEOUT)
					{
						SetEvent(g_hReplaceContinue);
						CloseHandle(g_hReplaceContinue);
						CloseHandle(g_hReplaceThread);
						g_hReplaceContinue=g_hReplaceThread=0;

						EnableWindow(GetDlgItem(hwndDlg, IDC_RESULTSCANCEL), FALSE);
					}
					break;
				}

				default:
					bRet=FALSE;
			}
					
			break;
		case WM_REPLACEACTIVE:
			EnableWindow(GetDlgItem(hwndDlg, IDC_RESULTSCANCEL), wParam);
			break;
				
		case WM_INITDIALOG:
		{
			HWND hList = GetDlgItem(hwndDlg, IDC_RESULTS);
	
			RECT rc;
			GetClientRect(hwndDlg, &rc);

			LVCOLUMN lvc={0};
			lvc.mask = LVCF_TEXT;
			
			lvc.pszText = "File";
			ListView_InsertColumn(hList, 0, &lvc);
			
			lvc.pszText = "Updated text";
			ListView_InsertColumn(hList, 1, &lvc);

			ListView_SetColumnWidth(hList, 0, rc.right/6-16);
			ListView_SetColumnWidth(hList, 1, ((rc.right)/6)*5-28);

			ListView_SetExtendedListViewStyle(hList
				, LVS_EX_FLATSB | LVS_EX_FULLROWSELECT | 
				  LVS_EX_TWOCLICKACTIVATE);

			g_hResults = hwndDlg;

			// Set Default Pushbutton to nothing
			::PostMessage(hwndDlg, DM_SETDEFID, 59955, 0L);
		}
		
		default:
			bRet=FALSE;
	}
	
	return bRet;
}

BOOL CALLBACK DialogProcAbout( HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	if (uMsg==WM_INITDIALOG)
	{
		// Set Default Pushbutton to nothing
		::PostMessage(hwndDlg, DM_SETDEFID, 59955, 0L);
	}
	else if (uMsg==WM_COMMAND&&(wParam==IDCANCEL))
	{
		EndDialog(hwndDlg, wParam);
		return TRUE;
	}

	return FALSE;
}

long RegCreateFullKeyPath(const HKEY hRootKey
	, LPCTSTR szPath, REGSAM samDesired, HKEY* phKeyResult)
{
	HKEY hCurKey = hRootKey;
	char szCurSubKey[256];
	const char* pszCur=szPath;
	long lRet=0L;
	int iPos;
		
	while (pszCur < szPath+strlen(szPath) )
	{
		iPos = 0;
		
		// get next subkey
		while(*pszCur != '\\'&&*pszCur!='\0')
			szCurSubKey[iPos++] = *pszCur++;
		szCurSubKey[iPos] = '\0';
		
		lRet = RegCreateKeyEx(hCurKey, szCurSubKey, 0
			, NULL, NULL
			, samDesired
			, NULL, phKeyResult
			, NULL);
		RegCloseKey(hCurKey);

		if (ERROR_SUCCESS != lRet)
		{
			*phKeyResult=0;
			return lRet;
		}

		hCurKey = *phKeyResult;

		pszCur++;
	}

	return lRet;
}


void RegisterForExplorer()
{
	HKEY hKey=0;
	char szPath[312]=IDP_SHELLCMD;
	strcat(szPath, IDP_SHELLCMD2);

	long lRet = RegCreateFullKeyPath( HKEY_CLASSES_ROOT
		, szPath
		, KEY_QUERY_VALUE|KEY_WRITE
		, &hKey);
	
	if (ERROR_SUCCESS == lRet && hKey)
	{				
		
		if (GetModuleFileName(NULL, szPath, MAX_PATH))
		{
			strcat(szPath, " -d %1");

			RegSetValueEx(hKey
				, NULL
				, 0, REG_SZ
				, (const LPBYTE) (LPCTSTR)szPath
				, strlen(szPath));
		}
		else
		{
			strcpy(szPath, IDP_SHELLCMD);
			strcat(szPath, IDP_SHELLCMD2);
			RegDeleteKey(HKEY_CLASSES_ROOT, szPath);
			RegDeleteKey(HKEY_CLASSES_ROOT,IDP_SHELLCMD);
		}
	}
}

BOOL RegSetting(LPCTSTR szName, LPTSTR szValue, BOOL bStore)
{
	if (!szValue)
		return FALSE;

	HKEY hKey=0;
	long lRet=RegCreateFullKeyPath(HKEY_LOCAL_MACHINE, "Software\\SoftCentral\\SCR"
		, (bStore) ? KEY_WRITE : KEY_READ, &hKey);
	if (ERROR_SUCCESS==lRet)
	{
		if (bStore)
			RegSetValueEx(hKey, szName, 0, REG_SZ, (LPBYTE)szValue, strlen(szValue)+1);
		else
		{
			DWORD dw=0, dwcb=256;
			lRet = RegQueryValueEx(hKey, szName, 0, &dw, (LPBYTE)szValue, &dwcb);
		}

		RegCloseKey(hKey);
	}

	return (ERROR_SUCCESS==lRet);
}
