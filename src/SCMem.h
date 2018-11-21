// SCMem.h: interface for the SCMem class.
//
//////////////////////////////////////////////////////////////////////

#if !defined(AFX_SCMEM_H__DA2F0DE6_1BB9_11D6_8343_0001029EB3A0__INCLUDED_)
#define AFX_SCMEM_H__DA2F0DE6_1BB9_11D6_8343_0001029EB3A0__INCLUDED_

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

class SCMem
{
public:
	int Replace(LPCTSTR sz, LPCTSTR szNew, BOOL bMatchCase);
	size_t	SafeStrlen();
	
	SCMem();
	SCMem(SCMem& scmem);
	SCMem(LPCTSTR sz);

	virtual ~SCMem();
	
	SCMem&		operator =(LPCTSTR sz);
	SCMem&		operator =(SCMem&);
	void		operator +=(LPCTSTR sz);
	SCMem		operator +(LPCTSTR sz);
	char		operator [](UINT iPos);		

				operator LPCTSTR()	{	return m_pszData; }

	static BOOL IsSameChar(char c, char d, BOOL bMatchCase);
private:
	BOOL AllocateMemory(size_t NewSize);
	void	FreeMemory();

	char* m_pszData;
};

#endif // !defined(AFX_SCMEM_H__DA2F0DE6_1BB9_11D6_8343_0001029EB3A0__INCLUDED_)
