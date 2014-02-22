// JHCPathString.h : added features for the WTL CString class to support file paths
//
////////////////////////////////////////////////////////////////////////////////////

#pragma once

class CPathString : public CString
{
public:
	CPathString() : CString() {}
	CPathString(const CString& stringSrc) : CString(stringSrc) {}
	CPathString(TCHAR ch, int nRepeat = 1) : CString(ch, nRepeat) {}
	CPathString(LPCTSTR lpsz) : CString(lpsz) {}
#ifdef _UNICODE
	CPathString(LPCSTR lpsz) : CString(lpsz) {}
	CPathString(LPCSTR lpsz, int nLength) : CString(lpsz, nLength) {}
#else
	CPathString(LPCWSTR lpsz) : CString(lpsz) {}
	CPathString(LPCWSTR lpsz, int nLength) : CString(lpsz, nLength) {}
#endif
	CPathString(LPCTSTR lpch, int nLength) : CString(lpch, nLength) {}
	CPathString(const unsigned char* lpsz) : CString(lpsz) {}

	// Appends a backslash to the path if it does not already end with one
	BOOL PathAddBackslash(void) {BOOL r = (::PathAddBackslash(GetBufferSetLength(GetLength() + 1)) != NULL); ReleaseBuffer(); return r;}
	// Appends an extension to the path
	BOOL PathAddExtension(CString& e) {BOOL r = ::PathAddExtension(GetBufferSetLength(GetLength() + e.GetLength()), e); ReleaseBuffer(); return r;}
	// Appends a path (inserting a backslash if necessary)
	BOOL PathAppend(CString& e) {BOOL r = ::PathAppend(GetBufferSetLength(GetLength() + e.GetLength() + 1), e); ReleaseBuffer(); return r;}
	// Replaces the contents with a root directory path for the given drive number (0 = A)
	BOOL PathBuildRoot(int drive) {Empty(); ::PathBuildRoot(GetBufferSetLength(3), drive); ReleaseBuffer(); return (GetLength() > 0);}
	// Canonicalises the path (resolving relative path specifiers)
	BOOL PathCanonicalize(void) {CPathString t; BOOL r = ::PathCanonicalize(t.GetBufferSetLength(MAX_PATH), LockBuffer()); t.ReleaseBuffer(); ReleaseBuffer(); Empty(); *this = t.m_pchData; return r;}
	// Prefixes the given root path to the path
	BOOL PathCombine(CString& p) {CPathString t; LPTSTR r = ::PathCombine(t.GetBufferSetLength(MAX_PATH), p.LockBuffer(), LockBuffer()); t.ReleaseBuffer(); p.ReleaseBuffer(); ReleaseBuffer(); Empty(); *this = t.m_pchData; return (r != NULL);}
	// Returns the number of characters that comprise the common prefix between the given path and the path
	int PathCommonPrefix(CString& p) {int r = ::PathCommonPrefix(LockBuffer(), p.LockBuffer(), NULL); ReleaseBuffer(); p.ReleaseBuffer(); return r;}
	// Compacts the path for display in the specified character width
	BOOL PathCompactPath(UINT width) {CPathString t; BOOL r = ::PathCompactPathEx(t.GetBufferSetLength(MAX_PATH), LockBuffer(), width, 0); t.ReleaseBuffer(); ReleaseBuffer(); Empty(); *this = t.m_pchData; return r;}
	// Returns true if the path exists as a directory or file
	BOOL PathFileExists(void) {BOOL r = ::PathFileExists(LockBuffer()); ReleaseBuffer(); return r;}
	// Returns a new CString containing the file extension (or empty string)
	CPathString PathFindExtension(void) {CPathString r(::PathFindExtension(LockBuffer())); ReleaseBuffer(); return r;}
	// Returns a new CString containing the file name part (with any extension)
	CPathString PathFindFileName(void) {CPathString r(::PathFindFileName(LockBuffer())); ReleaseBuffer(); return r;}
	// Returns a new CString containing the path minus the first component and seperator
	CPathString PathFindNextComponent(void) {CPathString r(::PathFindNextComponent(LockBuffer())); ReleaseBuffer(); return r;}
	// Returns the drive number (0 = A) of the drive specified on the path, or -1
	int PathGetDriveNumber(void) {int r = ::PathGetDriveNumber(LockBuffer()); ReleaseBuffer(); return r;}
	// Returns true if the path represents an existing directory
	BOOL PathIsDirectory(void) {BOOL r = ::PathIsDirectory(LockBuffer()); ReleaseBuffer(); return r;}
	// Returns true if the path represents a valid and empty directory
	BOOL PathIsDirectoryEmpty(void) {BOOL r = ::PathIsDirectoryEmpty(LockBuffer()); ReleaseBuffer(); return r;}
	// Returns true if the path represents a file specification with no directory path
	BOOL PathIsFileSpec(void) {BOOL r = ::PathIsFileSpec(LockBuffer()); ReleaseBuffer(); return r;}
	// Returns true if the path is relative rather than absolute (complete)
	BOOL PathIsRelative(void) {BOOL r = ::PathIsRelative(LockBuffer()); ReleaseBuffer(); return r;}
	// Returns true if the path is a root directory (device or network)
	BOOL PathIsRoot(void) {BOOL r = ::PathIsRoot(LockBuffer()); ReleaseBuffer(); return r;}
	// Returns true if the wildcarded string matches the path
	BOOL PathMatchSpec(CString& m) {BOOL r = ::PathMatchSpec(LockBuffer(), m.LockBuffer()); ReleaseBuffer(); m.ReleaseBuffer(); return r;}
	// Removes any trailing backslash from the path
	void PathRemoveBackslash(void) {::PathRemoveBackslash(GetBuffer(0)); ReleaseBuffer();}
	// Removes any leading or trailing whitespace
	void PathRemoveBlanks(void) {::PathRemoveBlanks(GetBuffer(0)); ReleaseBuffer();}
	// Removes any file extension from the path
	void PathRemoveExtension(void) {::PathRemoveExtension(GetBuffer(0)); ReleaseBuffer();}
	// Removes any file name and the separating backslash
	void PathRemoveFileSpec(void) {::PathRemoveFileSpec(GetBuffer(0)); ReleaseBuffer();}
	// Removes the directory path leaving only any filename
	void PathStripPath(void) {::PathStripPath(GetBuffer(0)); ReleaseBuffer();}
	// Strips the path back to any root directory only
	void PathStripToRoot(void) {::PathStripToRoot(GetBuffer(0)); ReleaseBuffer();}
	// Converts URL to path in MSDOS format
	BOOL PathCreateFromUrl(void) {CPathString n; DWORD c = MAX_PATH; HRESULT r = ::PathCreateFromUrl(LockBuffer(), n.GetBufferSetLength(MAX_PATH), &c, NULL);
		ReleaseBuffer(); n.ReleaseBuffer(); if (r == S_OK) {Empty(); *this = n.m_pchData;}; return (r == S_OK);}
	// Converts path in MSDOS format to URL format
	BOOL UrlCreateFromPath(void) {CPathString n; DWORD c = MAX_PATH+10; HRESULT r = ::UrlCreateFromPath(LockBuffer(), n.GetBufferSetLength(MAX_PATH+10), &c, NULL);
		ReleaseBuffer(); n.ReleaseBuffer(); if (r == S_OK) {Empty(); *this = n.m_pchData;}; return (r == S_OK); }

	// Converts the string in-place to a multi-string by replacing occurances of cSep with 0 and ensuring 00 at the overall end.
	void MakeMultistring(TCHAR cSep = '|')
	{
		TCHAR sep = cSep;
		if (Right(1) != CString(sep)) ConcatInPlace(1, &sep);
		LPTSTR psz = GetBuffer(0);
		while (*psz != '\0') {if (*psz == sep) *psz++ = '\0'; else psz = CharNext(psz);}
	}

	// Makes the file name unique by adding a sequence number if necessary.
	BOOL MakeNameUnique(void)
	{
		if (!PathFileExists()) return TRUE;
		CPathString suf = CString(")") + PathFindExtension();
		CPathString pre(*this); pre.PathRemoveExtension(); pre += CString(" (");
		int v = 1;
		CPathString tst;
		while (v < 100000)
		{
			tst = pre; tst.Append(++v); tst += suf;
			if (!tst.PathFileExists())
			{
				Empty();
				*this = tst.m_pchData;
				return TRUE;
			}
		}
		return FALSE;
	}

};
