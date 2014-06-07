// JHCRect.h : added features for the WTL CRect class to support window placement
//
////////////////////////////////////////////////////////////////////////////////////

#pragma once

#include <atlmisc.h>

#define POS_CENTER (0)
#define POS_CENTER_TOP (1)
#define POS_RIGHT_TOP (2)
#define POS_RIGHT_CENTER (3)
#define POS_RIGHT_BOTTOM (4)
#define POS_CENTER_BOTTOM (5)
#define POS_LEFT_BOTTOM (6)
#define POS_LEFT_CENTER (7)
#define POS_LEFT_TOP (8)

class JHCRect : public CRect
{
public:
	JHCRect(): CRect(){}

	JHCRect(int l, int t, int r, int b): CRect(l,t,r,b){}

	JHCRect(const RECT& srcRect): CRect(srcRect){}

	JHCRect(LPCRECT lpSrcRect): CRect(lpSrcRect){}

	JHCRect(POINT point, SIZE size): CRect(point,size){}

	JHCRect(POINT topLeft, POINT bottomRight): CRect(topLeft,bottomRight){}

	// Shrink size as necessary to fit in container rect, then position per 'pos'
	// 1..8 run clockwise from TopCenter; default or any other value Center.
	void place(CRect container, int pos = 0) {
		int x; int y;
		x = container.Width();
		if (Width() > x) right = left + x;
		x = container.Height();
		if (Height() > x) bottom = top + x;
		switch(pos) {
		case POS_CENTER_TOP:
		case POS_RIGHT_TOP:
		case POS_LEFT_TOP:
			y = container.top;
			break;
		case POS_RIGHT_BOTTOM:
		case POS_CENTER_BOTTOM:
		case POS_LEFT_BOTTOM:
			y = container.bottom - Height();
			break;
		default:
			y = container.top + (container.Height() - Height()) / 2;
			break;
		}     
		switch(pos) {
		case POS_LEFT_TOP:
		case POS_LEFT_CENTER:
		case POS_LEFT_BOTTOM:
			x = container.left;
			break;
		case POS_RIGHT_TOP:
		case POS_RIGHT_CENTER:
		case POS_RIGHT_BOTTOM:
			x = container.right - Width();
			break;
		default:
			x = container.left + (container.Width() - Width()) / 2;
			break;
		}
		MoveToXY(x, y);
	}
};
