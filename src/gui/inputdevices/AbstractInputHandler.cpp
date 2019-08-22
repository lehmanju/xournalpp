//
// Created by ulrich on 06.04.19.
//

#include "AbstractInputHandler.h"
#include "InputContext.h"
#include <gui/XournalppCursor.h>

AbstractInputHandler::AbstractInputHandler(InputContext* inputContext)
{
	XOJ_INIT_TYPE(AbstractInputHandler);

	this->inputContext = inputContext;
}

AbstractInputHandler::~AbstractInputHandler()
{
	XOJ_CHECK_TYPE(AbstractInputHandler);

	XOJ_RELEASE_TYPE(AbstractInputHandler);
}

void AbstractInputHandler::block(bool block)
{
	XOJ_CHECK_TYPE(AbstractInputHandler);

	this->blocked = block;
	this->onBlock();
}

bool AbstractInputHandler::isBlocked()
{
	XOJ_CHECK_TYPE(AbstractInputHandler);

	return this->blocked;
}

bool AbstractInputHandler::handle(InputEvent* event)
{
	XOJ_CHECK_TYPE(AbstractInputHandler);

	if (!this->blocked)
	{
		this->inputContext->getXournal()->view->getCursor()->setInputDeviceClass(event->deviceClass);
		return this->handleImpl(event);
	} else {
		return true;
	}
}

/**
 * Get Page at current position
 *
 * @return page or NULL if none
 */
PageView* AbstractInputHandler::getPageAtCurrentPosition(InputEvent* event)
{
	XOJ_CHECK_TYPE(AbstractInputHandler);

	if (event == nullptr)
	{
		return nullptr;
	}

	gdouble eventX = event->relativeX;
	gdouble eventY = event->relativeY;

	GtkXournal* xournal = this->inputContext->getXournal();

	double x = eventX + xournal->x;
	double y = eventY + xournal->y;

	return xournal->layout->getViewAt(x,y);
}

/**
 * Get input data relative to current input page
 */
PositionInputData AbstractInputHandler::getInputDataRelativeToCurrentPage(PageView* page, InputEvent* event)
{
	XOJ_CHECK_TYPE(AbstractInputHandler);

	GtkXournal* xournal = inputContext->getXournal();

	gdouble eventX = event->relativeX;
	gdouble eventY = event->relativeY;

	PositionInputData pos = {};
	pos.x = eventX - page->getX() - xournal->x;
	pos.y = eventY - page->getY() - xournal->y;
	pos.pressure = Point::NO_PRESSURE;

	if (this->inputContext->getSettings()->isPressureSensitivity())
	{
		pos.pressure = event->pressure;
	}

	pos.state = this->inputContext->getModifierState();
	pos.timestamp = event->timestamp;

	return pos;
}

void AbstractInputHandler::onBlock()
{
	XOJ_CHECK_TYPE(AbstractInputHandler);
}
