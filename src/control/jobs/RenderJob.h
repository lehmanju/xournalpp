/*
 * Xournal++
 *
 * A job which redraws a page or a page region
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#include "Job.h"

#include <XournalType.h>

#include <gtk/gtk.h>

class Rectangle;
class PageView;

class RenderJob : public Job
{
public:
	RenderJob(PageView* view);

protected:
	virtual ~RenderJob();

public:
	virtual JobType getType();

	void* getSource();

	void run();

private:
	/**
	 * Repaint the widget in UI Thread
	 */
	void repaintWidget(GtkWidget* widget);

	void rerenderRectangle(Rectangle* rect);

private:
	XOJ_TYPE_ATTRIB;

	PageView* view;
};
