// SCMem.cpp: implementation of the SCMem class.
//
//////////////////////////////////////////////////////////////////////

#include "stdafx.h"
#include "SCMem.h"

#include "malloc.h"
//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////
 
SCMem::SCMem()
{
	m_pszData=0;
}

SCMem::~SCMem()
{
	FreeMemory();
	
}

SCMem::SCMem(SCMem& sc)
{
	if (*this != sc)
	{
		m_pszData=0;
		if (sc.m_pszData && AllocateMemory(strlen(sc.m_pszData)))
			strcpy(m_pszData, sc.m_pszData);
	}
}
SCMem::SCMem(LPCTSTR sz)
{
	m_pszData=0;
	if (sz && AllocateMemory(strlen(sz)))
		strcpy(m_pszData, sz);
}


/////////////////////////////////////////////////////////////////
//*************************************************************//
//								Operators					   //
//*************************************************************//
/////////////////////////////////////////////////////////////////


SCMem& SCMem::operator=(LPCTSTR szNewValue)
{
	if (szNewValue && AllocateMemory(strlen(szNewValue)))
		strcpy(m_pszData, szNewValue);
	
	return *this;
}
SCMem&  SCMem::operator =(SCMem& sc)
{
	if (*this != sc)
	{
		if (sc.m_pszData && AllocateMemory(sc.SafeStrlen()) )
			strcpy(m_pszData, sc.m_pszData);
	}
	return *this;
}

void SCMem::operator +=(LPCTSTR sz)
{
	if (sz && AllocateMemory(_msize(m_pszData) + strlen(sz)))
		strcat(m_pszData, sz);
}

SCMem SCMem::operator+(LPCTSTR sz)
{
	SCMem scret;
	
	if (scret.AllocateMemory(this->SafeStrlen()+strlen(sz)))
	{
		strcpy(scret.m_pszData, this->m_pszData);
		strcat(scret.m_pszData, sz);
	}	
	
	return scret;
}

char SCMem::operator [](UINT iPos)
{
	return m_pszData[iPos];
}

/////////////////////////////////////////////////////////////////
//*************************************************************//
//								Functions					   //
//*************************************************************//
/////////////////////////////////////////////////////////////////


void SCMem::FreeMemory()
{
	if (0 != m_pszData)
	{
		free(m_pszData);
		m_pszData=0;
	}
}

BOOL SCMem::AllocateMemory(size_t NewSize)
{
	// alloc NewSize + 2 for null characters
	if (0 == m_pszData)
		m_pszData = static_cast<char*>(malloc(NewSize+16));
	else if (_msize(m_pszData) < NewSize)
		m_pszData = static_cast<char*>(realloc(m_pszData, NewSize+16));
	return (m_pszData!=NULL);
}

size_t SCMem::SafeStrlen()
{
	if (m_pszData)
		return strlen(m_pszData);
	return 0;
}

int SCMem::Replace(LPCTSTR sz, LPCTSTR szNew, BOOL bMatchCase)
{
	UINT iReplace=0, iSame=0, iNewLen=strlen(szNew), iOldLen=strlen(sz);
	int ic=0, icEnd=SafeStrlen(), iDiff = iNewLen-strlen(sz);
	
	// count instances of sz in m_pszData
	while (ic<icEnd)
	{
		while (SCMem::IsSameChar(m_pszData[ic++],sz[iSame++], bMatchCase))
		{
			if (iSame==iOldLen)
			{
				iReplace++;
				break;
			}
		}
		iSame=0;
	}
	if (iReplace > 0)
	{
		// get new last character position
		int icNewEnd = icEnd + iReplace*(iDiff);
	
		// allocatememory for current size + the number of required replacements *
		// the difference of stringlengths
		// AllocateMemory() only reallocates if our buffer is too small
		AllocateMemory(icNewEnd);

		ic=0;
		
		int iReplaced = 0;
		int iOldStringLen = strlen(m_pszData);

		while(ic<icNewEnd)
		{
			while (SCMem::IsSameChar(m_pszData[ic++],sz[iSame++], bMatchCase))
			{
				if(iSame==iOldLen)
				{	
					memmove(&m_pszData[ic+iDiff], &m_pszData[ic], sizeof(TCHAR)*(iOldStringLen-ic+(iReplaced*iDiff)));
					memcpy(&m_pszData[ic-iSame], szNew, sizeof(TCHAR)*iNewLen);

					ic += iDiff;
					iReplaced++;

					break;
				}
			}
			iSame=0;
		}

		// end with null
		m_pszData[icNewEnd]='\0';
	}
	
	return iReplace;
}

BOOL SCMem::IsSameChar(char c, char d, BOOL bMatchCase)
{
	return ( c==d || (!bMatchCase && ( c == d+32 || d == c+32))&&isalpha(c)&&isalpha(d));
}
