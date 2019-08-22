/*
 * Xournal++
 *
 * Class for render and repaint pages
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <XournalType.h>

class PageView;
class XournalView;

class RepaintHandler
{
public:
	RepaintHandler(XournalView* xournal);
	virtual ~RepaintHandler();

public:
	/**
	 * Repaint a page
	 */
	void repaintPage(PageView* view);

	/**
	 * Repaint a page area, coordinates are in view coordinates
	 */
	void repaintPageArea(PageView* view, int x1, int y1, int x2, int y2);

	/**
	 * Repaints the page border (at least)
	 */
	void repaintPageBorder(PageView* view);

private:
	XOJ_TYPE_ATTRIB;

	XournalView* xournal;
};
