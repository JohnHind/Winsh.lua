// JHCCMenu.h - implementation of the CCMenu class
// ===============================================
// Add-in template to pimp and add features to menus. May be added to CFrameWindowImpl
// class or any CWindowImpl class with a context menu. In former case, pimps main menu
// and any task-bar menu; in the latter, any context menus. Two steps are needed to use:
// 1-Add this template to the inheritance list of the class which implements the menus;
// 2-Add CHAIN_MSG_MAP(CCMenu<xxx>) to the class message map.
//
// Menus created from resources are converted automatically. Items with id = 0
// and MF_DISABLED are rendered as headings - horizontal unless the NEXT item
// has MF_MENUBARBREAK in which case a vertical sidebar. Icons from resources are
// used if they have a numeric id which matches the command id (similar to the way
// help string resources are matched to menu items).
//
// Menus created programatically have more flexibility. Use AppendCCMenu which takes
// the same parameters as AppendMenu but adds hIcon and uExtras parameters. You can
// specify the icon (or NULL). Extras flags are MFT_EX_HTITLE (= 1); MFT_EX_VTITLE
// (= 2) and MFT_EX_KEEP (= 4). The first two specify horizontal and vertical
// titles, the last prevents automatic clean-up when the menu closes. Use MFT_EX_KEEP
// if you create the menu once and then re-use it; you must then call DestroyCCMenu
// for each menu created (must be called for sub-menus too). If you create the menu
// anew each time in the WM_INITMENUPOPUP handler, leave this flag out and cleanup
// will be automatic (including destroying the icon if any).
//
// References:
// http://www.codeproject.com/KB/wtl/WTLOwnerDrawCtxtMenu.aspx
// http://www.codeproject.com/KB/wtl/sidebarmenu.aspx
// http://dvinogradov.blogspot.com/2007_01_01_archive.html
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "stdafx.h"

#define MAX_MENU_ITEM_TEXT_LENGTH       (100)
#define IMGPADDING                      (6)
#define TEXTPADDING                     (8)

#define SIDEBAR_FONT					(_T("TREBUCHET"))
#define TITLE_BG						(::GetSysColor(COLOR_HIGHLIGHT))
#define TITLE_TX						(::GetSysColor(COLOR_HIGHLIGHTTEXT))
#define MENU_BG							(HLS_TRANSFORM(::GetSysColor(COLOR_3DFACE), -4, 60))
#define ICON_BG							(HLS_TRANSFORM(::GetSysColor(COLOR_3DFACE), +20, 60))
#define MENU_TX							(::GetSysColor(COLOR_MENUTEXT))
#define MENU_DT							(::GetSysColor(COLOR_GRAYTEXT))
#define MENU_CK							(RGB(255,0,0))
#define SELECT_BG						(HLS_TRANSFORM(::GetSysColor(COLOR_HIGHLIGHT), +70, -57))
#define SELECT_BR						(::GetSysColor(COLOR_HIGHLIGHT))
#define SEPARATOR_PN					(::GetSysColor(COLOR_HIGHLIGHT))

#ifndef OBM_CHECK
#define OBM_CHECK                       32760
#endif

#ifndef SPI_GETFLATMENU
	const UINT SPI_GETFLATMENU = 0x1022;
#endif

// Bit flags for "fExtra":
#define MFT_EX_HTITLE					(0x00000001L)
#define MFT_EX_VTITLE					(0x00000002L)
#define MFT_EX_KEEP						(0x00000004L)

//template <class T>
class CCMenu
{
private:
    int m_cxExtraSpacing;
    COLORREF m_clrMask;
    SIZE m_szBitmap;
    SIZE m_szButton;
    CFont m_fontMenu;
    bool m_bFlatMenus;
	UINT m_itemHeight;
	bool m_bEnabled;
	bool m_bGutter;

protected:
	struct MenuItemData	
	{
		LPTSTR lpstrText;
		UINT fType;
		UINT fState;
		UINT fExtra;
        HICON hIcon;
	};

	// Transforms an RGB colour by increasing or reducing its luminance and/or saturation in HLS space.
	COLORREF HLS_TRANSFORM(COLORREF rgb, int percent_L, int percent_S)
	{
		WORD h, l, s;
		ColorRGBToHLS(rgb, &h, &l, &s);
		if ( percent_L > 0 )
		{
			l = WORD(l + ((240 - l) * percent_L) / 100);
		}
		else if ( percent_L < 0 )
		{
			l = WORD((l * (100 + percent_L)) / 100);
		}
		if ( percent_S > 0 )
		{
			s = WORD(s + ((240 - s) * percent_S) / 100);
		}
		else if ( percent_S < 0 )
		{
			s = WORD((s * (100 + percent_S)) / 100);
		}
		if (l > 240) l = 240; if (l < 0) l = 0;
		if (s > 240) s = 240; if (s < 0) s = 0;
		return ColorHLSToRGB(h, l, s);
	}

	// Handles the WM_MEASUREITEM message.
	void MeasureItem(LPMEASUREITEMSTRUCT lpMeasureItemStruct)
	{
		MenuItemData* pmd = (MenuItemData*)lpMeasureItemStruct->itemData;

		if (m_fontMenu.IsNull())
		{	// First time only, cache the font and some drawing metrics
			NONCLIENTMETRICS info = { sizeof(NONCLIENTMETRICS), 0 };
			#if (WINVER >= 0x0600)
			OSVERSIONINFO osvi = { sizeof(OSVERSIONINFO), 0 };
			GetVersionEx(&osvi);
			if (osvi.dwMajorVersion < 6) info.cbSize -= sizeof(int);
			#endif
			::SystemParametersInfo(SPI_GETNONCLIENTMETRICS, info.cbSize, &info, 0);
			HFONT hFontMenu = ::CreateFontIndirect(&info.lfMenuFont);
			ATLASSERT(hFontMenu != NULL);
			m_fontMenu.Attach(hFontMenu);
			CWindowDC dc(NULL);
			HFONT hFontOld = dc.SelectFont(m_fontMenu);
			RECT rcText = { 0, 0, 0, 0 };
			dc.DrawText(_T("\t"), -1, &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_CALCRECT);
			if ((rcText.right - rcText.left) < 4)
			{
				::SetRectEmpty(&rcText);
				dc.DrawText(_T("x"), -1, &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_CALCRECT);
				m_cxExtraSpacing = rcText.right - rcText.left;
			}
			else
			{
				m_cxExtraSpacing = 0;
			}
			m_itemHeight = abs(info.lfMenuFont.lfHeight) + 10;
			dc.SelectFont(hFontOld);
		}

		if (pmd->fType & MFT_SEPARATOR)
		{   // separator - use quarter system height and leave width for system to calculate
			lpMeasureItemStruct->itemHeight = ::GetSystemMetrics(SM_CYMENU) / 4;
			lpMeasureItemStruct->itemWidth  = 0;
		}
		else if (pmd->fExtra & MFT_EX_VTITLE)
		{	// Sidebar - set width to height of normal item and leave height for system to calculate
			lpMeasureItemStruct->itemWidth = ::GetSystemMetrics(SM_CYMENU) - (::GetSystemMetrics(SM_CXMENUCHECK) - 1);
			lpMeasureItemStruct->itemHeight  = 0;
		}
		else
		{	// Compute size of text - use DrawText with DT_CALCRECT
			CWindowDC dc(NULL);
			CFont fontBold;
			HFONT hOldFont = NULL;
			if ((pmd->fState & MFS_DEFAULT) || (pmd->fExtra & MFT_EX_HTITLE))
			{	// Need bold version of font
				LOGFONT lf = { 0 };
				m_fontMenu.GetLogFont(lf);
				lf.lfWeight += 200;
				fontBold.CreateFontIndirect(&lf);
				ATLASSERT(fontBold.m_hFont != NULL);
				hOldFont = dc.SelectFont(fontBold);
                fontBold.DeleteObject();
			}
			else
			{	// Standard font
				hOldFont = dc.SelectFont(m_fontMenu);
			}
			RECT rcText = { 0, 0, 0, 0 };
			dc.DrawText(pmd->lpstrText, -1, &rcText, DT_SINGLELINE | DT_LEFT | DT_VCENTER | DT_CALCRECT);
            int cx = rcText.right - rcText.left;
			dc.SelectFont(hOldFont);
			if (m_bGutter) cx += m_szButton.cx + IMGPADDING;
			cx += m_cxExtraSpacing + ( 2 * TEXTPADDING) + 4;
			lpMeasureItemStruct->itemWidth = cx;
			lpMeasureItemStruct->itemHeight = m_itemHeight;
		}
	}

	// Draw the icon in greyscale within the given rectangle in the device context.
	void DrawDisabledIcon(HDC DC, CRect& Rect, HICON Icon)
	{
		WTL::CDC MemDC(CreateCompatibleDC(DC));
		BITMAPINFO bmi;
		ZeroMemory(&bmi, sizeof(BITMAPINFO));
		bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmi.bmiHeader.biWidth = Rect.Width();
		bmi.bmiHeader.biHeight = Rect.Height();
		bmi.bmiHeader.biPlanes = 1;
		bmi.bmiHeader.biBitCount = 32;
		bmi.bmiHeader.biCompression = BI_RGB;
		bmi.bmiHeader.biSizeImage = bmi.bmiHeader.biWidth * bmi.bmiHeader.biHeight * 4;
		VOID *pvBits;
		WTL::CBitmap Bitmap(::CreateDIBSection(MemDC, &bmi, DIB_RGB_COLORS, &pvBits, NULL, 0));
		WTL::CBitmapHandle PrevBitmap(MemDC.SelectBitmap(Bitmap));
		MemDC.DrawIconEx(0, 0, Icon, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight);
		for (unsigned char *p = (unsigned char*)pvBits, *end = p + bmi.bmiHeader.biSizeImage; p < end; p += 4)
		{	// Gray = 0.3*R + 0.59*G + 0.11*B
			p[0] = p[1] = p[2] = (static_cast<unsigned int>(p[2]) *  77 + static_cast<unsigned int>(p[1]) * 151 +
				static_cast<unsigned int>(p[0]) *  28) >> 8;
		}
		BLENDFUNCTION BlendFunction;
		BlendFunction.BlendOp = AC_SRC_OVER;
		BlendFunction.BlendFlags = 0;
		BlendFunction.SourceConstantAlpha = 0x60;  // half transparent
		BlendFunction.AlphaFormat = AC_SRC_ALPHA;  // use bitmap alpha
		AlphaBlend(DC, Rect.left, Rect.top, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight,
			MemDC, 0, 0, bmi.bmiHeader.biWidth, bmi.bmiHeader.biHeight, BlendFunction);
		MemDC.SelectBitmap(PrevBitmap);
	}

	// Draw a vertical (sidebar) title item.
	void DrawVTitle(LPDRAWITEMSTRUCT lpDrawItemStruct)
	{
		MenuItemData* pmd = (MenuItemData*)lpDrawItemStruct->itemData;
		CDCHandle dc = lpDrawItemStruct->hDC;
        CRect rc(lpDrawItemStruct->rcItem);
        CString sCaption = pmd->lpstrText;
		RECT rct; dc.GetClipBox(&rct);
		rc.bottom = rct.bottom - rct.top;
        dc.FillSolidRect(rc, TITLE_BG);
		rc.DeflateRect(2,2);
		HFONT hFont = CreateFont(rc.right - rc.left, 0, 900, 900, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
			OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, PROOF_QUALITY, DEFAULT_PITCH | FF_DONTCARE, SIDEBAR_FONT);
		rc.InflateRect(2,2);
		int oldbkm = dc.SetBkMode(TRANSPARENT);
		COLORREF oldcolor = dc.SetTextColor(TITLE_TX);
		HFONT fontold = dc.SelectFont(hFont);
		dc.DrawText(sCaption, sCaption.GetLength(), rc, DT_SINGLELINE|DT_LEFT|DT_BOTTOM);
		dc.SelectFont(fontold);
		dc.SetTextColor(oldcolor);
		dc.SetBkMode(oldbkm);
	}

	// Draw a horizontal title item.
	void DrawHTitle(LPDRAWITEMSTRUCT lpDrawItemStruct)
	{
		MenuItemData* pmd = (MenuItemData*)lpDrawItemStruct->itemData;
		CDCHandle dc = lpDrawItemStruct->hDC;
        CRect rc(lpDrawItemStruct->rcItem);
		CRect rcx(rc);
		if (m_bGutter)
		{
			rcx.right = rc.left + m_szBitmap.cx + IMGPADDING;
			dc.FillSolidRect(rcx, ICON_BG);
			rcx.left = rcx.right;
			rcx.right = rc.right;
		}
        dc.FillSolidRect(rcx, TITLE_BG);
		dc.SetTextColor(TITLE_TX);
		dc.SetBkMode(TRANSPARENT);
        CString sCaption = pmd->lpstrText;
		HFONT m_hDefFont = NULL;
		LOGFONT lf;
		CFont m_fontBold;
		dc.GetCurrentFont().GetLogFont(&lf);
		lf.lfWeight = FW_BOLD;
		m_fontBold.CreateFontIndirect(&lf);
		m_hDefFont = dc.SelectFont(m_fontBold);
        dc.DrawText(sCaption, sCaption.GetLength(), rcx, DT_SINGLELINE|DT_VCENTER|DT_CENTER);
		dc.SelectFont(m_hDefFont);
	}

	// Draw a horizontal separator bar.
	void DrawSeparator(LPDRAWITEMSTRUCT lpDrawItemStruct)
	{
		MenuItemData* pmd = (MenuItemData*)lpDrawItemStruct->itemData;
		CDCHandle dc = lpDrawItemStruct->hDC;
		CRect rc(lpDrawItemStruct->rcItem);
		CRect rcx(rc);
		if (m_bGutter)
		{
			rcx.right = rc.left + m_szBitmap.cx + IMGPADDING;
			dc.FillSolidRect(rcx, ICON_BG);
			rcx.left = rcx.right;
			rcx.right = rc.right;
		}
        dc.FillSolidRect(rcx, MENU_BG);
		rcx.top += (rc.bottom - rc.top) / 2;
		rcx.left += 2; rcx.right -= 2;
		CPen newpen;
		HPEN oldpen;
		newpen.CreatePen(PS_SOLID, 1, SEPARATOR_PN);
		oldpen = dc.SelectPen(newpen);
		dc.MoveTo(CPoint(rcx.left, rcx.top));
		dc.LineTo(CPoint(rcx.right, rcx.top));
		dc.SelectPen(oldpen);
	}

	// Draw a normal menu item.
	void DrawActiveItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
	{
		MenuItemData* pmd = (MenuItemData*)lpDrawItemStruct->itemData;
		CDCHandle dc = lpDrawItemStruct->hDC;
		CRect rc(lpDrawItemStruct->rcItem);
		BOOL bDisabled = lpDrawItemStruct->itemState & ODS_GRAYED;
 		BOOL bChecked = lpDrawItemStruct->itemState & ODS_CHECKED;
		BOOL bSelected = lpDrawItemStruct->itemState & ODS_SELECTED;
		BOOL bDefault = pmd->fState & MFS_DEFAULT;

        if (bSelected && !bDisabled)
        {	// Draw the selected item background
			HPEN oldpen;
			CPen newpen;
			newpen.CreatePen(PS_SOLID, 1, SELECT_BR);
			oldpen = dc.SelectPen(newpen);
			HBRUSH oldbrush;
			CBrush newbrush;
			newbrush.CreateSolidBrush(SELECT_BG);
			oldbrush = (HBRUSH)::SelectObject(dc, newbrush);
            dc.Rectangle(rc);
			dc.SelectBrush(oldbrush);
			dc.SelectPen(oldpen);
        }
        else
        {   // Draw the unselected item background
            CRect rcx(rc);
			if (m_bGutter)
			{
				rcx.right = rc.left + m_szBitmap.cx + IMGPADDING;
				dc.FillSolidRect(rcx, ICON_BG);
				rcx.left = rcx.right;
				rcx.right = rc.right;
			}
            dc.FillSolidRect(rcx, MENU_BG);
        }

        // Draw the text
        CRect rcx(rc);
        CString sCaption = pmd->lpstrText;
        int nTab = sCaption.Find('\t');
        if (nTab >= 0) sCaption = sCaption.Left (nTab);
		if ((!m_bGutter) && (bChecked))
			dc.SetTextColor(bDisabled ? MENU_DT : MENU_CK);
		else
			dc.SetTextColor(bDisabled ? MENU_DT : MENU_TX);
		dc.SetBkMode(TRANSPARENT);
		HFONT m_hDefFont = NULL;
		if ((bDefault) || ((bChecked) && (!m_bGutter)))
		{	// Bold font required
			LOGFONT lf;
			CFont m_fontBold;
			CFontHandle ((HFONT)::GetCurrentObject (dc, OBJ_FONT)).GetLogFont (&lf);
			lf.lfWeight = FW_BOLD;
			m_fontBold.CreateFontIndirect (&lf);
			m_hDefFont = (HFONT)::SelectObject(dc, m_fontBold);
		}
        if (m_bGutter) rcx.left += m_szBitmap.cx + IMGPADDING;
		rcx.left += TEXTPADDING;
		dc.DrawText(sCaption, sCaption.GetLength(), rcx, DT_SINGLELINE|DT_VCENTER|DT_LEFT);
        if (nTab >= 0)
        {    
            rcx.right -= TEXTPADDING + 4;
            dc.DrawText(pmd->lpstrText + nTab + 1, _tcslen(pmd->lpstrText + nTab + 1), rcx, DT_SINGLELINE|DT_VCENTER|DT_RIGHT);
        }
		if (m_hDefFont != NULL) dc.SelectFont(m_hDefFont);

		if ((bChecked) && (m_bGutter))
        {	// Draw the check mark:
			HPEN oldpen;
			CPen newpen;
			newpen.CreatePen(PS_SOLID, 1, SELECT_BR);
			oldpen = dc.SelectPen(newpen);
			HBRUSH oldbrush;
			CBrush newbrush;
			newbrush.CreateSolidBrush(bDisabled ? ICON_BG : (bSelected ? SELECT_BG : ICON_BG));
			oldbrush = (HBRUSH)::SelectObject(dc, newbrush);
            dc.Rectangle(CRect(rc.left + 1, rc.top + 1, rc.left + m_szButton.cx - 2, rc.bottom - 1));
			dc.SelectBrush(oldbrush);
			dc.SelectPen(oldpen);
			CRect rcx(rc);
            rcx.left  = rc.left + 2;
            rcx.right = rc.left + m_szBitmap.cx + IMGPADDING;
            dc.SetBkColor(bSelected? SELECT_BG : ICON_BG);
			rcx.top += 3; rcx.left += 2;
            HBITMAP hBmp = LoadBitmap(NULL, MAKEINTRESOURCE(OBM_CHECK));
            BOOL bRet = dc.DrawState(rcx.TopLeft(), rcx.Size(), hBmp, DSS_NORMAL, (HBRUSH)NULL);
            DeleteObject(hBmp);
        }
        else if ((pmd->hIcon != NULL) && (m_bGutter))
        {	// Draw the icon:
            if (bDisabled)
			{
				DrawDisabledIcon(dc, CRect(rc.left + 3, rc.top + 4, rc.left + 3 + m_szBitmap.cx, rc.top + 4 + m_szBitmap.cx), pmd->hIcon);
			}
			else
            {
				dc.DrawIconEx(CPoint(rc.left + 3, rc.top + 4), pmd->hIcon, CSize(m_szBitmap.cx, m_szBitmap.cx));
            }
        }
	}

	// Determine which type of item is to be drawn.
    void DrawItem(LPDRAWITEMSTRUCT lpDrawItemStruct)
	{
		MenuItemData* pmd = (MenuItemData*)lpDrawItemStruct->itemData;
        if (pmd->fType & MFT_SEPARATOR)
        {
			DrawSeparator(lpDrawItemStruct);
		}
		else if (pmd->fExtra & MFT_EX_VTITLE)
		{
			DrawVTitle(lpDrawItemStruct);
		}
		else if (pmd->fExtra & MFT_EX_HTITLE)
		{
			DrawHTitle(lpDrawItemStruct);
		}
		else
		{
			DrawActiveItem(lpDrawItemStruct);
		}
    }

	// Handle WM_INITMENUPOPUP by automatically converting non OWNERDRAW items.
    LRESULT InitMenuPopupHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {        
		if ((BOOL)HIWORD(lParam))
		{	// System menu, do nothing
			bHandled = FALSE;
			return 1;
		}
		CMenuHandle menuPopup = (HMENU)wParam;
		ATLASSERT(menuPopup.m_hMenu != NULL);
        TCHAR szString[MAX_MENU_ITEM_TEXT_LENGTH];
		MenuItemData* pMIlast = NULL;
        for (int i = 0; i < menuPopup.GetMenuItemCount(); i++)
        {	// Process every menuitem in menu
            CMenuItemInfo mii;
            mii.cch = MAX_MENU_ITEM_TEXT_LENGTH;
            mii.fMask = MIIM_CHECKMARKS | MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_SUBMENU | MIIM_TYPE;
            mii.dwTypeData = szString;
			menuPopup.GetMenuItemInfo(i, TRUE, &mii);
            if (!(mii.fType & MFT_OWNERDRAW))
            {   // Not already an ownerdraw item
                MenuItemData* pMI = new MenuItemData;
                ATLASSERT(pMI != NULL);
 				pMI->fExtra = 0;
				pMI->hIcon = NULL;
				pMI->fType = mii.fType;
				pMI->fState = mii.fState;
				if (pMIlast)
				{	// If last item was a title and this has MENUBARBREAK, last should be vertical
					if (pMI->fType & MFT_MENUBARBREAK) pMIlast->fExtra = MFT_EX_VTITLE;
					pMIlast = NULL;
				}
				if ((mii.wID == 0) && (mii.fState & MF_DISABLED))
				{	// If ID is zero and it's disabled, it is a title, establish if vertical next item
					pMIlast = pMI;
					pMI->fExtra = MFT_EX_HTITLE;
				}
				pMI->lpstrText = new TCHAR[lstrlen(szString) + 1];
                ATLASSERT(pMI->lpstrText != NULL);
				lstrcpy(pMI->lpstrText, szString);
                mii.dwItemData = (ULONG_PTR)pMI;
                mii.fType |= MFT_OWNERDRAW;
				pMI->hIcon = (HICON)LoadImage(_Module.GetResourceInstance(), MAKEINTRESOURCE(mii.wID), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
				menuPopup.SetMenuItemInfo(i, TRUE, &mii);
             }
        }
        return 0;
    }

	// For WM_MEASUREITEM check it is an owner-draw menu and if so handle it.
	LRESULT MeasureItemHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
	{
		LPMEASUREITEMSTRUCT lpMeasureItemStruct = (LPMEASUREITEMSTRUCT)lParam;
		MenuItemData* pmd = (MenuItemData*)lpMeasureItemStruct->itemData;
		if (lpMeasureItemStruct->CtlType == ODT_MENU && pmd != NULL)
		{
		    MeasureItem(lpMeasureItemStruct);
		}
		else
		{
			bHandled = FALSE;
		}
		return 1;
	}

	// For WM_DRAWITEM check it is an owner-draw menu and if so handle it.
    LRESULT DrawItemHandler(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
        LPDRAWITEMSTRUCT lpDrawItemStruct = (LPDRAWITEMSTRUCT)lParam;
        MenuItemData* pMI = (MenuItemData*)lpDrawItemStruct->itemData;
		if (lpDrawItemStruct->CtlType == ODT_MENU && pMI != NULL)
		{
            DrawItem(lpDrawItemStruct);
		}
		else
		{
			bHandled = FALSE;
		}
        return 1;
    }

public:
    CCMenu()
    {
		m_bEnabled = false;
		m_bGutter = true;
        m_cxExtraSpacing = 0;
        m_clrMask = RGB(192, 192, 192);
        m_szBitmap.cx = 16;
        m_szBitmap.cy = 15;
    	m_szButton.cx = m_szBitmap.cx + 6;
		m_szButton.cy = m_szBitmap.cy + 6;

		// Query flat menu mode (Windows XP or later)
		OSVERSIONINFO ovi = { sizeof(OSVERSIONINFO) };
		::GetVersionEx(&ovi);
		if (((ovi.dwMajorVersion == 5) && (ovi.dwMinorVersion >= 1)) || (ovi.dwMajorVersion > 5))
		{
			BOOL bRetVal = FALSE;
			BOOL bRet = ::SystemParametersInfo(SPI_GETFLATMENU, 0, &bRetVal, 0);
			m_bFlatMenus = (bRet && bRetVal);
		}

		// Mark as enabled to process messages
		m_bEnabled = true;
	}

    ~CCMenu()
    {
        if (!m_fontMenu.IsNull()) m_fontMenu.DeleteObject();
    }

	// Note: do not forget to put CHAIN_MSG_MAP in your message map.
	BEGIN_MSG_MAP(CCMenu)
        MESSAGE_HANDLER(WM_INITMENUPOPUP, OnInitMenuPopup)
		MESSAGE_HANDLER(WM_UNINITMENUPOPUP, OnUninitMenuPopup)
        MESSAGE_HANDLER(WM_MEASUREITEM, OnMeasureItem)
        MESSAGE_HANDLER(WM_DRAWITEM, OnDrawItem)
	END_MSG_MAP()

	// For programatic menu construction, works as AppendMenu with added optional hIcon and uExtras.
	BOOL AppendCCMenu(HMENU hMenu, UINT uFlags, UINT_PTR uIDNewItem = 0, LPCTSTR lpNewItem = NULL, HICON hIcon = NULL, UINT uExtras = 0)
	{
		if (m_bEnabled)
		{
			MenuItemData* pMI = new MenuItemData;
			pMI->lpstrText = new TCHAR[lstrlen(lpNewItem) + 1];
			ATLASSERT(pMI->lpstrText != NULL);
			if (lpNewItem == NULL)
				pMI->lpstrText[0] = 0;
			else
				lstrcpy(pMI->lpstrText, lpNewItem);
			pMI->fExtra = uExtras;
			pMI->hIcon = hIcon;
			pMI->fType = uFlags & (MF_BITMAP|MF_MENUBARBREAK|MF_MENUBREAK|MF_RIGHTJUSTIFY|MF_SEPARATOR|MF_STRING);
			pMI->fState = uFlags & (MF_CHECKED|MF_DEFAULT|MF_DISABLED|MF_GRAYED|MF_HILITE);
			return ::AppendMenu(hMenu, MF_OWNERDRAW | uFlags, uIDNewItem, (LPCWSTR)pMI);
		}
		else
		{
			return ::AppendMenu(hMenu, uFlags, uIDNewItem, lpNewItem);
		}
	}

	// Turn icon/check gutter on or off. If gutter is off, icons are not drawn and checked items are shown
	// in bold red.
	void SetCCMenuGutter(bool on) {
		m_bGutter = on;
	}

	// Call this explicitly (defaulting the second parameter) if you use AppendCCmenu with the MFT_EX_KEEP
	// flag when you are done with the menu.
	void DestroyCCMenu(CMenuHandle menu, BOOL always = TRUE)
	{
        ATLASSERT(menu.m_hMenu != NULL);
        for (int i = 0; i < menu.GetMenuItemCount(); i++)
        {
			CMenuItemInfo mii;
            mii.fMask = MIIM_DATA | MIIM_TYPE;
            menu.GetMenuItemInfo(i, TRUE, &mii);
            MenuItemData* pMI = (MenuItemData*)mii.dwItemData;
            if (pMI != NULL)
            {
				if (!always) if (pMI->fExtra & MFT_EX_KEEP) return;
                mii.fMask = MIIM_DATA | MIIM_TYPE | MIIM_STATE;
                mii.fType = pMI->fType;
				mii.fState = pMI->fState;
                mii.dwTypeData = pMI->lpstrText;
                mii.cch = lstrlen(pMI->lpstrText);
                mii.dwItemData = NULL;
                menu.SetMenuItemInfo(i, TRUE, &mii);
				if (pMI->hIcon != NULL) DestroyIcon(pMI->hIcon);
                delete [] pMI->lpstrText;
                delete pMI;
            }
        }
	}

    LRESULT OnInitMenuPopup(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
		if (!m_bEnabled) {bHandled = FALSE; return 1;}
        return InitMenuPopupHandler(uMsg, wParam, lParam, bHandled);
    }

    LRESULT OnUninitMenuPopup(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
		if (!m_bEnabled) {bHandled = FALSE; return 0;}
		HMENU hMenu = (HMENU)wParam;
        CMenuHandle menuPopup = hMenu;
		DestroyCCMenu(menuPopup, FALSE);
		bHandled = FALSE;
		return 1;
    }

    LRESULT OnMeasureItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
		if (!m_bEnabled) {bHandled = FALSE; return 0;}
        return MeasureItemHandler(uMsg, wParam, lParam, bHandled);
    }

    LRESULT OnDrawItem(UINT uMsg, WPARAM wParam, LPARAM lParam, BOOL& bHandled)
    {
		if (!m_bEnabled) {bHandled = FALSE; return 0;}
        return DrawItemHandler(uMsg, wParam, lParam, bHandled);
    }

};
