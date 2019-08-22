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

#include "control/zoom/ZoomListener.h"
#include "control/zoom/ZoomGesture.h"
#include "model/DocumentListener.h"
#include "model/PageRef.h"
#include "widgets/XournalWidget.h"

#include <Arrayiterator.h>

#include <gtk/gtk.h>

class Control;
class XournalppCursor;
class Document;
class EditSelection;
class Layout;
class PagePositionHandler;
class XojPageView;
class PdfCache;
class Rectangle;
class RepaintHandler;
class TextEditor;
class HandRecognition;

class XournalView : public DocumentListener, public ZoomListener
{
public:
	XournalView(GtkScrolledWindow* parent, Control* control, ZoomGesture* zoomGesture);
	virtual ~XournalView();

public:
	void zoomIn();
	void zoomOut();

	bool paint(GtkWidget* widget, GdkEventExpose* event);

	void requestPage(XojPageView* page);

	void layoutPages();

	void scrollTo(size_t pageNo, double y = 0);
	
	//Relative navigation in current layout:
	void pageRelativeXY(int offCol, int offRow );

	size_t getCurrentPage();

	void clearSelection();

	void layerChanged(size_t page);

	void requestFocus();

	void forceUpdatePagenumbers();

	XojPageView* getViewFor(size_t pageNr);

	bool searchTextOnPage(string text, size_t p, int* occures, double* top);

	bool cut();
	bool copy();
	bool paste();

	void getPasteTarget(double& x, double& y);

	bool actionDelete();

	void endTextAllPages(XojPageView* except = NULL);

	void resetShapeRecognizer();

	int getDisplayWidth() const;
	int getDisplayHeight() const;

	bool isPageVisible(size_t page, int* visibleHeight);

	void ensureRectIsVisible(int x, int y, int width, int height);

	void setSelection(EditSelection* selection);
	EditSelection* getSelection();
	void deleteSelection(EditSelection* sel = NULL);
	void repaintSelection(bool evenWithoutSelection = false);

	TextEditor* getTextEditor();
	ArrayIterator<XojPageView*> pageViewIterator();
	Control* getControl();
	double getZoom();
	int getDpiScaleFactor();
	Document* getDocument();
	PdfCache* getCache();
	RepaintHandler* getRepaintHandler();
	GtkWidget* getWidget();
	XournalppCursor* getCursor();

	Rectangle* getVisibleRect(int page);
	Rectangle* getVisibleRect(XojPageView* redrawable);

	/**
	 * A pen action was detected now, therefore ignore touch events
	 * for a short time
	 */
	void penActionDetected();

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

	void queueResize();

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

private:
	void fireZoomChanged();

	void addLoadPageToQue(PageRef page, int priority);

	Rectangle* getVisibleRect(size_t page);

	static gboolean clearMemoryTimer(XournalView* widget);

	static void staticLayoutPages(GtkWidget *widget, GtkAllocation* allocation, void* data);

private:
	XOJ_TYPE_ATTRIB;

	/**
	 * Scrollbars
	 */

	ZoomGesture* zoomGesture;

	GtkWidget* widget = NULL;

	GtkAdjustment* horizontal = nullptr;
	GtkAdjustment* vertical = nullptr;

	double margin = 75;

	XojPageView** viewPages = NULL;
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

	friend class Layout;
};
