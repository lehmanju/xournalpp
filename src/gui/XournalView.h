/*
 * Xournal++
 *
 * The widget wich displays the PDF and the drawings
 *
 * @author Xournal++ Team
 * https://github.com/xournalpp/xournalpp
 *
 * @license GNU GPLv2 or later
 */

#pragma once

#define PAGE_SPACING 10
//TODO move PAGE_SPACING in a setting

#include "control/zoom/ZoomListener.h"
#include "control/zoom/ZoomGesture.h"
#include "model/DocumentListener.h"
#include "model/PageRef.h"
#include "gui/widgets/XournalWidget.h.old"

#include <Arrayiterator.h>

#include <gtk/gtk.h>
#include <cairo.h>

class Control;
class XournalppCursor;
class Document;
class EditSelection;
class Layout;
class PagePositionHandler;
class PageView;
class PdfCache;
class Rectangle;
class RepaintHandler;
class TextEditor;
class HandRecognition;

class XournalView : public DocumentListener, public ZoomListener
{
public:
	XournalView(GtkScrolledWindow* parent, Control* control, ZoomGesture* zoomGesture);
	~XournalView() override;

public:
	void layoutPages();

	void scrollTo(size_t pageNo, double y = 0);
	
	//Relative navigation in current layout:
	void pageRelativeXY(int offCol, int offRow );

	size_t getCurrentPage();

	void clearSelection();

	void layerChanged(size_t page);

	void requestFocus();

	void forceUpdatePagenumbers();

	PageView* getViewFor(size_t pageNr);

	bool searchTextOnPage(string text, size_t p, int* occures, double* top);

	bool cut();
	bool copy();
	bool paste();

	void getPasteTarget(double& x, double& y);

	bool actionDelete();

	void endTextAllPages(PageView* except = NULL);

	void resetShapeRecognizer();

	bool isPageVisible(size_t page, int* visibleHeight);

	void ensureRectIsVisible(int x, int y, int width, int height);

	void deleteSelection(EditSelection* sel = NULL);
	void repaintSelection(bool evenWithoutSelection = false);
	EditSelection* getSelection();
	void setSelection(EditSelection* sel);

	TextEditor* getTextEditor();
	ArrayIterator<PageView*> pageViewIterator();
	Control* getControl();
	double getZoom();
	int getDpiScaleFactor();
	Document* getDocument();
	PdfCache* getCache();
	RepaintHandler* getRepaintHandler();
	XournalppCursor* getCursor();

	Rectangle* getVisibleRect(PageView* redrawable);

	/**
	 * @return Helper class for Touch specific fixes
	 */
	HandRecognition* getHandRecognition();

	/**
	 * Get the handler for the zoom gesture
	 * @return The handler
	 */
	ZoomGesture* getZoomGestureHandler();

	GtkAdjustment* getHorizontalAdjustment();
	GtkAdjustment* getVerticalAdjustment();

	GtkClipboard* getGtkClipboard();

public:
	// ZoomListener interface
	void zoomChanged() override;

public:
	// DocumentListener interface
	void pageSelected(size_t page) override;
	void pageSizeChanged(size_t page) override;
	void pageChanged(size_t page) override;
	void pageInserted(size_t page) override;
	void pageDeleted(size_t page) override;
	void documentChanged(DocumentChangeType type) override;

public:
	bool onKeyPressEvent(GdkEventKey* event);
	bool onKeyReleaseEvent(GdkEventKey* event);

	static void onRealized(GtkWidget* widget, XournalView* view);
	static bool onDraw(GtkWidget* widget, cairo_t* cr, XournalView* view);
	static void onAllocate(GtkWidget* widget, GdkRectangle* rectangle, XournalView* view);

private:

	Rectangle* getVisibleRect(size_t page);

	static gboolean clearMemoryTimer(XournalView* widget);

private:
	ZoomGesture* zoomGesture;
	ZoomControl* zoomControl;

	GtkWidget* widget = nullptr;
	Layout* layout = nullptr;

	GtkAdjustment* horizontal = nullptr;
	GtkAdjustment* vertical = nullptr;

	PageView** viewPages = NULL;
	size_t viewPagesLen = 0;

	Control* control = NULL;

	size_t currentPage = 0;
	size_t lastSelectedPage = -1;

	PdfCache* cache = NULL;

	/**
	 * Handler for rerendering pages / repainting pages
	 */
	RepaintHandler* repaintHandler = NULL;

	/**
	 * Memory cleanup timeout
	 */
	int cleanupTimeout = -1;

	/**
	 * Helper class for Touch specific fixes
	 */
	HandRecognition* handRecognition = NULL;

	EditSelection* selection = nullptr;

	friend class Layout;
};
