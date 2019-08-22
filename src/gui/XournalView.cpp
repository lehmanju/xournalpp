#include "XournalView.h"

#include "Layout.h"
#include "PageView.h"
#include "RepaintHandler.h"
#include "Shadow.h"
#include "XournalppCursor.h"

#include "control/Control.h"
#include "control/PdfCache.h"
#include "control/settings/MetadataManager.h"
#include "gui/inputdevices/HandRecognition.h"
#include "model/Document.h"
#include "model/Stroke.h"
#include "undo/DeleteUndoAction.h"
#include "gui/widgets/XournalWidget.h.old"

#include "Rectangle.h"
#include "Util.h"
#include "util/cpp14memory.h"

#include <gdk/gdk.h>

#include <cmath>
#include <tuple>

XournalView::XournalView(GtkScrolledWindow* parent, Control* control, ZoomGesture* zoomGesture)
 : control(control)
 , zoomGesture(zoomGesture)
{
	this->cache = new PdfCache(control->getSettings()->getPdfPageCacheSize());
	registerListener(control);

	this->horizontal = gtk_scrolled_window_get_hadjustment(parent);
	this->vertical = gtk_scrolled_window_get_vadjustment(parent);

	auto inputContext = new InputContext(this);
	this->widget = gtk_drawing_area_new();

	gtk_container_add(GTK_CONTAINER(parent), this->widget);
	gtk_widget_show(this->widget);

	g_signal_connect(this->widget, "realize", G_CALLBACK(onRealized), this);
	g_signal_connect(this->widget, "size-allocate", G_CALLBACK(onAllocate), this);
	g_signal_connect(this->widget, "draw", G_CALLBACK(onDraw), this);

	this->repaintHandler = new RepaintHandler(this);
	this->handRecognition = new HandRecognition(this->widget, inputContext, control->getSettings());


	gtk_widget_set_can_default(this->widget, true);
	gtk_widget_grab_default(this->widget);

	gtk_widget_grab_focus(this->widget);

	this->cleanupTimeout = g_timeout_add_seconds(5, (GSourceFunc) clearMemoryTimer, this);

	layout = new Layout(this);
}

XournalView::~XournalView()
{
	g_source_remove(this->cleanupTimeout);

	for (size_t i = 0; i < this->viewPagesLen; i++)
	{
		delete this->viewPages[i];
	}
	delete[] this->viewPages;
	this->viewPagesLen = 0;
	this->viewPages = nullptr;

	delete this->cache;
	this->cache = nullptr;
	delete this->repaintHandler;
	this->repaintHandler = nullptr;


	gtk_widget_destroy(this->widget);
	this->widget = nullptr;

	delete this->handRecognition;
	this->handRecognition = nullptr;
}

gint pageViewCmpSize(PageView* a, PageView* b)
{
	return a->getLastVisibleTime() - b->getLastVisibleTime();
}

gboolean XournalView::clearMemoryTimer(XournalView* widget)
{
	GList* list = nullptr;

	for (size_t i = 0; i < widget->viewPagesLen; i++)
	{
		PageView* v = widget->viewPages[i];
		if (v->getLastVisibleTime() > 0)
		{
			list = g_list_insert_sorted(list, v, (GCompareFunc) pageViewCmpSize);
		}
	}

	int pixel = 2884560;
	int firstPages = 4;

	int i = 0;

	for (GList* l = list; l != nullptr; l = l->next)
	{
		if (firstPages)
		{
			firstPages--;
		}
		else
		{
			PageView* v = (PageView*) l->data;

			if (pixel <= 0)
			{
				v->deleteViewBuffer();
			}
			else
			{
				pixel -= v->getBufferPixels();
			}
		}
		i++;
	}

	g_list_free(list);

	// call again
	return true;
}

size_t XournalView::getCurrentPage()
{
	return currentPage;
}

const int scrollKeySize = 30;

//TODO move to input?
bool XournalView::onKeyPressEvent(GdkEventKey* event)
{
	size_t p = getCurrentPage();
	if (p != npos && p < this->viewPagesLen)
	{
		PageView* v = this->viewPages[p];
		if (v->onKeyPressEvent(event))
		{
			return true;
		}
	}

	// Esc leaves fullscreen mode
	if (event->keyval == GDK_KEY_Escape || event->keyval == GDK_KEY_F11)
	{
		if (control->isFullscreen())
		{
			control->setFullscreen(false);
			return true;
		}
	}

	// F5 starts presentation modus
	if (event->keyval == GDK_KEY_F5)
	{
		if (!control->isFullscreen())
		{
			control->setViewPresentationMode(true);
			control->setFullscreen(true);
			return true;
		}
	}

	guint state = event->state & gtk_accelerator_get_default_mod_mask();

	if (state & GDK_SHIFT_MASK)
	{
		GtkAllocation alloc = {0};
		gtk_widget_get_allocation(gtk_widget_get_parent(this->widget), &alloc);
		int windowHeight = alloc.height - scrollKeySize;

		if (event->keyval == GDK_KEY_Page_Down)
		{
			layout->scrollRelative(0, windowHeight);
			return false;
		}
		if (event->keyval == GDK_KEY_Page_Up || event->keyval == GDK_KEY_space)
		{
			layout->scrollRelative(0, -windowHeight);
			return true;
		}
	}
	else
	{
		if (event->keyval == GDK_KEY_Page_Down || event->keyval == GDK_KEY_KP_Page_Down)
		{
			control->getScrollHandler()->goToNextPage();
			return true;
		}
		if (event->keyval == GDK_KEY_Page_Up || event->keyval == GDK_KEY_KP_Page_Up)
		{
			control->getScrollHandler()->goToPreviousPage();
			return true;
		}
	}

	if (event->keyval == GDK_KEY_space) {
		GtkAllocation alloc = {0};
		gtk_widget_get_allocation(gtk_widget_get_parent(this->widget), &alloc);
		int windowHeight = alloc.height - scrollKeySize;

		layout->scrollRelative(0, windowHeight);
		return true;
	}

	// Numeric keypad always navigates by page
	if (event->keyval == GDK_KEY_KP_Up)
	{
		this->pageRelativeXY(0, -1);
		return true;
	}

	if (event->keyval == GDK_KEY_KP_Down)
	{
		this->pageRelativeXY(0, 1);
		return true;
	}

	if (event->keyval == GDK_KEY_KP_Left)
	{
		this->pageRelativeXY(-1, 0);
		return true;
	}

	if (event->keyval == GDK_KEY_KP_Right)
	{
		this->pageRelativeXY(1, 0);
		return true;
	}


	if (event->keyval == GDK_KEY_Up)
	{
		if (control->getSettings()->isPresentationMode())
		{
			control->getScrollHandler()->goToPreviousPage();
			return true;
		}
		else
		{
			if (state & GDK_SHIFT_MASK)
			{
				this->pageRelativeXY(0, -1);
			}
			else
			{
				layout->scrollRelative(0, -scrollKeySize);
			}
			return true;
		}
	}

	if (event->keyval == GDK_KEY_Down)
	{
		if (control->getSettings()->isPresentationMode())
		{
			control->getScrollHandler()->goToNextPage();
			return true;
		}
		else
		{
			if (state & GDK_SHIFT_MASK)
			{
				this->pageRelativeXY(0, 1);
			}
			else
			{
				layout->scrollRelative(0, scrollKeySize);
			}
			return true;
		}
	}

	if (event->keyval == GDK_KEY_Left)
	{
		if (state & GDK_SHIFT_MASK)
		{
			this->pageRelativeXY(-1, 0);
		}
		else
		{
			if (control->getSettings()->isPresentationMode())
			{
				control->getScrollHandler()->goToPreviousPage();
			}
			else
			{
				layout->scrollRelative(-scrollKeySize, 0);
			}
		}
		return true;
	}

	if (event->keyval == GDK_KEY_Right)
	{
		if (state & GDK_SHIFT_MASK)
		{
			this->pageRelativeXY(1, 0);
		}
		else
		{
			if (control->getSettings()->isPresentationMode())
			{
				control->getScrollHandler()->goToNextPage();
			}
			else
			{
				layout->scrollRelative(scrollKeySize, 0);
			}
		}
		return true;
	}

	if (event->keyval == GDK_KEY_End || event->keyval == GDK_KEY_KP_End)
	{
		control->getScrollHandler()->goToLastPage();
		return true;
	}

	if (event->keyval == GDK_KEY_Home || event->keyval == GDK_KEY_KP_Home)
	{
		control->getScrollHandler()->goToFirstPage();
		return true;
	}

	// vim like scrolling
	if (event->keyval == GDK_KEY_j)
	{
		layout->scrollRelative(0, 60);
		return true;
	}
	if (event->keyval == GDK_KEY_k)
	{
		layout->scrollRelative(0, -60);
		return true;
	}
	if (event->keyval == GDK_KEY_h)
	{
		layout->scrollRelative(-60, 0);
		return true;
	}
	if (event->keyval == GDK_KEY_l)
	{
		layout->scrollRelative(60, 0);
		return true;
	}

	return false;
}

RepaintHandler* XournalView::getRepaintHandler()
{
	return this->repaintHandler;
}

bool XournalView::onKeyReleaseEvent(GdkEventKey* event)
{
	size_t p = getCurrentPage();
	if (p != npos && p < this->viewPagesLen)
	{
		PageView* v = this->viewPages[p];
		if (v->onKeyReleaseEvent(event))
		{
			return true;
		}
	}

	return false;
}

// send the focus back to the appropriate widget
void XournalView::requestFocus()
{
	gtk_widget_grab_focus(this->widget);
}

bool XournalView::searchTextOnPage(string text, size_t p, int* occures, double* top)
{
	if (p == npos || p >= this->viewPagesLen)
	{
		return false;
	}
	PageView* v = this->viewPages[p];

	return v->searchTextOnPage(text, occures, top);
}

void XournalView::forceUpdatePagenumbers()
{
	size_t p = this->currentPage;
	this->currentPage = npos;

	control->firePageSelected(p);
}

PageView* XournalView::getViewFor(size_t pageNr)
{
	if (pageNr == npos || pageNr >= this->viewPagesLen)
	{
		return nullptr;
	}
	return this->viewPages[pageNr];
}

void XournalView::pageSelected(size_t page)
{
	if (this->currentPage == page && this->lastSelectedPage == page)
	{
		return;
	}

	Document* doc = control->getDocument();
	doc->lock();
	Path file = doc->getEvMetadataFilename();
	doc->unlock();

	control->getMetadataManager()->storeMetadata(file.str(), page, getZoom());

	if (this->lastSelectedPage != npos && this->lastSelectedPage < this->viewPagesLen)
	{
		this->viewPages[this->lastSelectedPage]->setSelected(false);
	}

	this->currentPage = page;

	size_t pdfPage = npos;

	if (page != npos && page < viewPagesLen)
	{
		PageView* vp = viewPages[page];
		vp->setSelected(true);
		lastSelectedPage = page;
		pdfPage = vp->getPage()->getPdfPageNr();
	}

	control->updatePageNumbers(currentPage, pdfPage);

	control->updateBackgroundSizeButton();
}

Control* XournalView::getControl()
{
	return control;
}

void XournalView::scrollTo(size_t pageNo, double yDocument)
{
	if (pageNo >= this->viewPagesLen)
	{
		return;
	}

	PageView* v = this->viewPages[pageNo];

	// Make sure it is visible

	int x = v->getX();
	int y = v->getY() + yDocument;
	int width = v->getDisplayWidth();
	int height = v->getDisplayHeight();

	this->layout->ensureRectIsVisible(x, y, width, height);

	// Select the page
	control->firePageSelected(pageNo);
}


void XournalView::pageRelativeXY(int offCol, int offRow)
{
	int currPage = getCurrentPage();

	PageView* view = getViewFor(currPage);
	int row = view->getMappedRow();
	int col = view->getMappedCol();

	int page = this->layout->getIndexAtGridMap(row + offRow, col + offCol);
	if (page >= 0)
	{
		this->scrollTo(page, 0);
	}
}


void XournalView::endTextAllPages(PageView* except)
{
	for (size_t i = 0; i < this->viewPagesLen; i++)
	{
		PageView* v = this->viewPages[i];
		if (except != v)
		{
			v->endText();
		}
	}
}

void XournalView::layerChanged(size_t page)
{
	if (page != npos && page < this->viewPagesLen)
	{
		this->viewPages[page]->rerenderPage();
	}
}

void XournalView::getPasteTarget(double& x, double& y)
{
	size_t pageNo = getCurrentPage();
	if (pageNo == npos)
	{
		return;
	}

	Rectangle* rect = getVisibleRect(pageNo);

	if (rect)
	{
		x = rect->x + rect->width / 2;
		y = rect->y + rect->height / 2;
		delete rect;
	}
}

/**
 * Return the rectangle which is visible on screen, in document cooordinates
 *
 * Or nullptr if the page is not visible
 */
Rectangle* XournalView::getVisibleRect(size_t page)
{
	if (page == npos || page >= this->viewPagesLen)
	{
		return nullptr;
	}
	PageView* p = this->viewPages[page];

	return getVisibleRect(p);
}

Rectangle* XournalView::getVisibleRect(PageView* redrawable)
{
	return gtk_xournal_get_visible_area(this->widget, redrawable);
}

/**
 * @return Helper class for Touch specific fixes
 */
HandRecognition* XournalView::getHandRecognition()
{
	return handRecognition;
}

ZoomGesture* XournalView::getZoomGestureHandler()
{
	return zoomGesture;
}

void XournalView::ensureRectIsVisible(int x, int y, int width, int height)
{
	this->layout->ensureRectIsVisible(x, y, width, height);
}

void XournalView::zoomChanged()
{
	int currPage = this->getCurrentPage();
	PageView* view = getViewFor(currPage);
	ZoomControl* zoom = control->getZoomControl();

	if (!view)
	{
		return;
	}

	// move this somewhere else maybe
	this->layout->layoutPages();

	if (zoom->isZoomPresentationMode() || zoom->isZoomFitMode())
	{
		scrollTo(currPage);
	}
	else
	{
		std::tuple<double, double> pos = zoom->getScrollPositionAfterZoom();
		if (std::get<0>(pos) != -1 && std::get<1>(pos) != -1)
		{
			layout->scrollAbs(std::get<0>(pos), std::get<1>(pos));
		}
	}

	Document* doc = control->getDocument();
	doc->lock();
	Path file = doc->getEvMetadataFilename();
	doc->unlock();

	control->getMetadataManager()->storeMetadata(file.str(), getCurrentPage(), zoom->getZoomReal());

	// Updates the Eraser's cursor icon in order to make it as big as the erasing area
	control->getCursor()->updateCursor();

	this->control->getScheduler()->blockRerenderZoom();
}

void XournalView::pageChanged(size_t page)
{
	if (page != npos && page < this->viewPagesLen)
	{
		this->viewPages[page]->rerenderPage();
	}
}

void XournalView::pageDeleted(size_t page)
{
	size_t currPage = control->getCurrentPageNo();

	delete this->viewPages[page];

	for (size_t i = page; i < this->viewPagesLen - 1; i++)
	{
		this->viewPages[i] = this->viewPages[i + 1];
	}

	this->viewPagesLen--;
	this->viewPages[this->viewPagesLen] = nullptr;

	if (currPage >= page)
	{
		currPage--;
	}

	layoutPages();
	control->getScrollHandler()->scrollToPage(currPage);
}

TextEditor* XournalView::getTextEditor()
{
	for (size_t i = 0; i < this->viewPagesLen; i++)
	{
		PageView* v = this->viewPages[i];
		if (v->getTextEditor())
		{
			return v->getTextEditor();
		}
	}

	return nullptr;
}

void XournalView::resetShapeRecognizer()
{
	for (size_t i = 0; i < this->viewPagesLen; i++)
	{
		PageView* v = this->viewPages[i];
		v->resetShapeRecognizer();
	}
}

PdfCache* XournalView::getCache()
{
	return this->cache;
}

void XournalView::pageInserted(size_t page)
{
	PageView** lastViewPages = this->viewPages;

	this->viewPages = new PageView*[this->viewPagesLen + 1];

	for (size_t i = 0; i < page; i++)
	{
		this->viewPages[i] = lastViewPages[i];

		// unselect to prevent problems...
		this->viewPages[i]->setSelected(false);
	}

	for (size_t i = page; i < this->viewPagesLen; i++)
	{
		this->viewPages[i + 1] = lastViewPages[i];

		// unselect to prevent problems...
		this->viewPages[i + 1]->setSelected(false);
	}

	this->lastSelectedPage = -1;

	this->viewPagesLen++;

	delete[] lastViewPages;

	Document* doc = control->getDocument();
	doc->lock();
	PageView* pageView = new PageView(this, doc->getPage(page));
	doc->unlock();

	this->viewPages[page] = pageView;

	this->layout->layoutPages();
	this->layout->updateVisibility();
}

double XournalView::getZoom()
{
	return control->getZoomControl()->getZoom();
}

int XournalView::getDpiScaleFactor()
{
	return gtk_widget_get_scale_factor(widget);
}

void XournalView::clearSelection()
{
	EditSelection* sel = this->selection;
	this->selection = nullptr;
	delete sel;

	control->setClipboardHandlerSelection(getSelection());

	getCursor()->setMouseSelectionType(CURSOR_SELECTION_NONE);
	control->getToolHandler()->setSelectionEditTools(false, false, false);
}

void XournalView::deleteSelection(EditSelection* sel)
{
	if (sel == nullptr)
	{
		sel = getSelection();
	}

	if (sel)
	{
		PageView* view = sel->getView();
		auto undo = mem::make_unique<DeleteUndoAction>(sel->getSourcePage(), false);
		sel->fillUndoItem(undo.get());
		control->getUndoRedoHandler()->addUndoAction(std::move(undo));

		clearSelection();

		view->rerenderPage();
		repaintSelection(true);
	}
}

void XournalView::setSelection(EditSelection* sel)
{
	clearSelection();
	this->selection = sel;

	control->setClipboardHandlerSelection(getSelection());

	bool canChangeSize = false;
	bool canChangeColor = false;
	bool canChangeFill = false;

	for (Element* e: *sel->getElements())
	{
		if (e->getType() == ELEMENT_TEXT)
		{
			canChangeColor = true;
		}
		else if (e->getType() == ELEMENT_STROKE)
		{
			auto* s = (Stroke*) e;
			if (s->getToolType() != STROKE_TOOL_ERASER)
			{
				canChangeColor = true;
				canChangeFill = true;
			}
			canChangeSize = true;
		}

		if (canChangeColor && canChangeSize && canChangeFill)
		{
			break;
		}
	}

	control->getToolHandler()->setSelectionEditTools(canChangeColor, canChangeSize, canChangeFill);

	repaintSelection();
}

void XournalView::repaintSelection(bool evenWithoutSelection)
{
	if (evenWithoutSelection)
	{
		gtk_widget_queue_draw(this->widget);
		return;
	}

	if (this->selection == nullptr)
	{
		return;
	}

	// repaint always the whole widget
	gtk_widget_queue_draw(this->widget);
}

void XournalView::layoutPages()
{
	this->layout->layoutPages();
}

bool XournalView::isPageVisible(size_t page, int* visibleHeight)
{
	Rectangle* rect = getVisibleRect(page);
	if (rect)
	{
		if (visibleHeight)
		{
			*visibleHeight = rect->height;
		}

		delete rect;
		return true;
	}
	if (visibleHeight)
	{
		*visibleHeight = 0;
	}

	return false;
}

void XournalView::documentChanged(DocumentChangeType type)
{
	if (type != DOCUMENT_CHANGE_CLEARED && type != DOCUMENT_CHANGE_COMPLETE)
	{
		return;
	}

	XournalScheduler* scheduler = this->control->getScheduler();
	scheduler->lock();
	scheduler->removeAllJobs();

	clearSelection();

	for (size_t i = 0; i < this->viewPagesLen; i++)
	{
		delete this->viewPages[i];
	}
	delete[] this->viewPages;

	Document* doc = control->getDocument();
	doc->lock();

	this->viewPagesLen = doc->getPageCount();
	this->viewPages = new PageView*[viewPagesLen];

	for (size_t i = 0; i < viewPagesLen; i++)
	{
		PageView* pageView = new PageView(this, doc->getPage(i));
		viewPages[i] = pageView;
	}

	doc->unlock();

	layoutPages();
	scrollTo(0, 0);

	scheduler->unlock();
}

bool XournalView::cut()
{
	size_t p = getCurrentPage();
	if (p == npos || p >= viewPagesLen)
	{
		return false;
	}

	PageView* page = viewPages[p];
	return page->cut();
}

bool XournalView::copy()
{
	size_t p = getCurrentPage();
	if (p == npos || p >= viewPagesLen)
	{
		return false;
	}

	PageView* page = viewPages[p];
	return page->copy();
}

bool XournalView::paste()
{
	size_t p = getCurrentPage();
	if (p == npos || p >= viewPagesLen)
	{
		return false;
	}

	PageView* page = viewPages[p];
	return page->paste();
}

bool XournalView::actionDelete()
{
	size_t p = getCurrentPage();
	if (p == npos || p >= viewPagesLen)
	{
		return false;
	}

	PageView* page = viewPages[p];
	return page->actionDelete();
}

Document* XournalView::getDocument()
{
	return control->getDocument();
}

ArrayIterator<PageView*> XournalView::pageViewIterator()
{
	return ArrayIterator<PageView*>(viewPages, viewPagesLen);
}


XournalppCursor* XournalView::getCursor()
{
	return control->getCursor();
}

EditSelection* XournalView::getSelection()
{
	g_return_val_if_fail(this->widget != nullptr, nullptr);

	return this->selection;
}

GtkAdjustment* XournalView::getHorizontalAdjustment()
{
	return this->horizontal;
}

GtkAdjustment* XournalView::getVerticalAdjustment()
{
	return this->vertical;
}

bool XournalView::onDraw(GtkWidget *widget, cairo_t *cr, XournalView *view)
{
	ArrayIterator<PageView*> it = view->pageViewIterator();

	double x1, x2, y1, y2;

    cairo_clip_extents(cr, &x1, &y1, &x2, &y2);

    // Draw background in clipped area
    Settings* settings = view->getControl()->getSettings();
    Util::cairo_set_source_rgbi(cr, settings->getBackgroundColor());
    cairo_rectangle(cr, x1, y1, x2 - x1, y2 - y1);
    cairo_fill(cr);

    // determine which pages need to be rendered, therefore we extend the clipped area by PAGE_SPACING
    Rectangle clippingRect(x1 - PAGE_SPACING, y1 - PAGE_SPACING, x2 - x1 + 2*PAGE_SPACING, y2 - y1 + 2*PAGE_SPACING);

    while (it.hasNext())
    {
		PageView* pv = it.next();

		int px = pv->getX();
        int py = pv->getY();
		int pw = pv->getDisplayWidth();
		int ph = pv->getDisplayHeight();

		if (!clippingRect.intersects(pv->getRect()))
        {
            continue;
        }

		if (pv->isSelected())
		{
			Shadow::drawShadow(cr, px - 2, py - 2, pw + 4, ph + 4);

			// Draw border
			Util::cairo_set_source_rgbi(cr, settings->getBorderColor());
			cairo_set_line_width(cr, 4.0);
			cairo_set_line_cap(cr, CAIRO_LINE_CAP_BUTT);
			cairo_set_line_join(cr, CAIRO_LINE_JOIN_BEVEL);

			cairo_move_to(cr, px, py);
			cairo_line_to(cr, px, py + ph);
			cairo_line_to(cr, px + pw, py + ph);
			cairo_line_to(cr, px + pw, py);
			cairo_close_path(cr);

			cairo_stroke(cr);
		}
		else
		{
			Shadow::drawShadow(cr, px, py, pw, ph);
		}

		cairo_save(cr);
        cairo_translate(cr, px, py);

        pv->paintPage(cr, nullptr);
        cairo_restore(cr);
    }

    return true;
}

void XournalView::onAllocate(GtkWidget *widget, GdkRectangle *rectangle, XournalView *view)
{
    gtk_widget_set_allocation(widget, rectangle);
	view->layout->setSize(rectangle->width, rectangle->height);
}

void XournalView::onRealized(GtkWidget* widget, XournalView* view)
{
	gtk_widget_set_realized(widget, true);

	gtk_widget_set_hexpand(widget, true);
	gtk_widget_set_vexpand(widget, true);

	GtkAllocation allocation;
	gtk_widget_get_allocation(widget, &allocation);
	gtk_widget_set_window(widget, gtk_widget_get_parent_window(widget));

	// Disable event compression?
}

GtkClipboard* XournalView::getGtkClipboard()
{
	return gtk_widget_get_clipboard(this->widget, GDK_SELECTION_CLIPBOARD);
}

void XournalView::pageSizeChanged(size_t page)
{
	this->layoutPages();
}
void XournalView::queueRedraw()
{
	gtk_widget_queue_draw(this->widget);
}
Layout* XournalView::getLayout()
{
	return this->layout;
}
