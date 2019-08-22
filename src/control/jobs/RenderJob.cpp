#include "RenderJob.h"

#include "control/Control.h"
#include "control/ToolHandler.h"
#include "gui/PageView.h"
#include "gui/XournalView.h"
#include "model/Document.h"
#include "view/DocumentView.h"
#include "view/PdfView.h"

#include <Rectangle.h>
#include <Util.h>
#include <config-features.h>

#include <list>

RenderJob::RenderJob(PageView* view)
 : view(view)
{
	XOJ_INIT_TYPE(RenderJob);
}

RenderJob::~RenderJob()
{
	XOJ_CHECK_TYPE(RenderJob);

	this->view = NULL;

	XOJ_RELEASE_TYPE(RenderJob);
}

void* RenderJob::getSource()
{
	XOJ_CHECK_TYPE(RenderJob);

	return this->view;
}

void RenderJob::rerenderRectangle(Rectangle* rect)
{
	XOJ_CHECK_TYPE(RenderJob);

	double zoom = view->xournal->getZoom();
	Document* doc = view->xournal->getDocument();

	doc->lock();
	double pageWidth = view->page->getWidth();
	double pageHeight = view->page->getHeight();
	doc->unlock();

	int x = rect->x * zoom;
	int y = rect->y * zoom;
	int width = rect->width * zoom;
	int height = rect->height * zoom;

	cairo_surface_t* rectBuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
	cairo_t* crRect = cairo_create(rectBuffer);
	cairo_translate(crRect, -x, -y);
	cairo_scale(crRect, zoom, zoom);

	DocumentView v;
	Control* control = view->getXournal()->getControl();
	v.setMarkAudioStroke(control->getToolHandler()->getToolType() == TOOL_PLAY_OBJECT);
	v.limitArea(rect->x, rect->y, rect->width, rect->height);

	bool backgroundVisible = view->page->isLayerVisible(0);
	if (backgroundVisible && view->page->getBackgroundType().isPdfPage())
	{
		int pgNo = view->page->getPdfPageNr();
		XojPdfPageSPtr popplerPage = doc->getPdfPage(pgNo);
		PdfCache* cache = view->xournal->getCache();
		PdfView::drawPage(cache, popplerPage, crRect, zoom, pageWidth, pageHeight);
	}

	doc->lock();
	v.drawPage(view->page, crRect, false);
	doc->unlock();

	cairo_destroy(crRect);

	g_mutex_lock(&view->drawingMutex);

	cairo_t * crPageBuffer = cairo_create(view->crBuffer);

	cairo_set_operator(crPageBuffer, CAIRO_OPERATOR_SOURCE);
	cairo_set_source_surface(crPageBuffer, rectBuffer, x, y);
	cairo_rectangle(crPageBuffer, x, y, width, height);
	cairo_fill(crPageBuffer);

	cairo_destroy(crPageBuffer);

	cairo_surface_destroy(rectBuffer);

	g_mutex_unlock(&view->drawingMutex);
}

void RenderJob::run()
{
	XOJ_CHECK_TYPE(RenderJob);

	double zoom = this->view->xournal->getZoom();

	g_mutex_lock(&this->view->repaintRectMutex);

	bool rerenderComplete = this->view->rerenderComplete;
	std::vector<Rectangle*> rerenderRects = this->view->rerenderRects;
	this->view->rerenderRects.clear();

	this->view->rerenderComplete = false;

	g_mutex_unlock(&this->view->repaintRectMutex);

	int dpiScaleFactor = this->view->xournal->getDpiScaleFactor();

	if (rerenderComplete || dpiScaleFactor > 1)
	{
		Document* doc = this->view->xournal->getDocument();

		int dispWidth = this->view->getDisplayWidth();
		int dispHeight = this->view->getDisplayHeight();

		dispWidth *= dpiScaleFactor;
		dispHeight *= dpiScaleFactor;
		zoom *= dpiScaleFactor;

		cairo_surface_t* crBuffer = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, dispWidth, dispHeight);
		cairo_t* cr2 = cairo_create(crBuffer);
		cairo_scale(cr2, zoom, zoom);

		XojPdfPageSPtr popplerPage;

		doc->lock();

		if (this->view->page->getBackgroundType().isPdfPage())
		{
			int pgNo = this->view->page->getPdfPageNr();
			popplerPage = doc->getPdfPage(pgNo);
		}

		Control* control = view->getXournal()->getControl();
		DocumentView documentView;
		documentView.setMarkAudioStroke(control->getToolHandler()->getToolType() == TOOL_PLAY_OBJECT);
		double width = this->view->page->getWidth();
		double height = this->view->page->getHeight();

		bool backgroundVisible = this->view->page->isLayerVisible(0);
		if (backgroundVisible)
		{
			PdfView::drawPage(this->view->xournal->getCache(), popplerPage, cr2, zoom, width, height);
		}
		documentView.drawPage(this->view->page, cr2, false);

		cairo_destroy(cr2);

		g_mutex_lock(&this->view->drawingMutex);

		if (this->view->crBuffer)
		{
			cairo_surface_destroy(this->view->crBuffer);
		}
		this->view->crBuffer = crBuffer;

		g_mutex_unlock(&this->view->drawingMutex);
		doc->unlock();
	}
	else
	{
		for (Rectangle* rect : rerenderRects)
		{
			rerenderRectangle(rect);
		}
	}

	// Schedule a repaint of the widget
	Util::execInUiThread([=]() { this->view->getXournal()->queueRedraw(); });

	// delete all rectangles
	for (Rectangle* rect : rerenderRects)
	{
		delete rect;
	}
	rerenderRects.clear();
}

JobType RenderJob::getType()
{
	XOJ_CHECK_TYPE(RenderJob);

	return JOB_TYPE_RENDER;
}
