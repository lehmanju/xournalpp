/*
 * Xournal++
 *
 * Image Tool handler
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include <XournalType.h>

#include <gtk/gtk.h>

class Control;
class PageView;

class ImageHandler
{
public:
	ImageHandler(Control* control, PageView* view);
	virtual ~ImageHandler();

public:
	bool insertImage(double x, double y);
	bool insertImage(GFile* file, double x, double y);

private:
	XOJ_TYPE_ATTRIB;

	Control* control;
	PageView* view;
};
