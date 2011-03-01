#include "CMCanvasControllerDeclarative.h"

#include <QtCore/QTimer>
#include <QtGui/QGesture>
#include <QtGui/QVector2D>
#include <QtGui/QGraphicsWidget>
#include <QtGui/QGraphicsSceneMouseEvent>
#include <QtGui/QApplication>

#include <KDE/KActionCollection>

#include <KoCanvasBase.h>
#include <KoViewConverter.h>
#include <KoToolManager.h>
#include <KoZoomHandler.h>
#include <KoZoomController.h>

class CMCanvasControllerDeclarative::Private
{
public:
    Private(CMCanvasControllerDeclarative* qq)
        : q(qq),
        canvas(0), zoomHandler(0), zoomController(0),
        vastScrollingFactor(0.f),
        minX(0), minY(0), maxX(0), maxY(0),
        dragging(false), updateCanvas(true), zoom(1.0)
    { }
    ~Private() { }

    void handleMouseMoveEvent(QGraphicsSceneMouseEvent* event);
    void handleGesture(QGestureEvent* event);
    void updateMinMax();
    void updateCanvasSize();
    void checkBounce(const QPoint& offset);
    void frameChanged(int frame);

    CMCanvasControllerDeclarative* q;
    KoCanvasBase* canvas;
    KoZoomHandler* zoomHandler;
    KoZoomController* zoomController;

    QTimer* timer;

    qreal vastScrollingFactor;

    qreal minX;
    qreal minY;
    qreal maxX;
    qreal maxY;

    bool dragging;
    qreal distX;
    qreal distY;

    QGraphicsPixmapItem* scaleProxy;
    bool updateCanvas;

    qreal zoom;
    qreal zoomMax;
    qreal zoomMin;
    
    QVector2D force;
    QVector2D velocity;
    
    float mass;
    float dragCoeff;
    float timeStep;
    float springCoeff;
};

CMCanvasControllerDeclarative::CMCanvasControllerDeclarative(QDeclarativeItem* parent)
    : QDeclarativeItem(parent), KoCanvasController(0), d(new Private(this))
{
    setClip(true);
    setAcceptedMouseButtons(Qt::LeftButton);
    setFiltersChildEvents(true);
    setAcceptTouchEvents(true);
    grabGesture(Qt::PinchGesture);

    d->timer = new QTimer(this);
    d->timer->setInterval(40);
    connect(d->timer, SIGNAL(timeout()), this, SLOT(timerUpdate()));

    d->mass = 10.f;
    d->dragCoeff = 0.05f;
    d->springCoeff = 0.8f;
    d->timeStep = 1000.f / d->timer->interval();

    d->scaleProxy = new QGraphicsPixmapItem(QPixmap(), this);
    d->scaleProxy->setVisible(false);

    connect(this, SIGNAL(heightChanged()), this, SLOT(onHeightChanged()));
    connect(this, SIGNAL(widthChanged()), this, SLOT(onWidthChanged()));

    connect(proxyObject, SIGNAL(moveDocumentOffset(QPoint)), this, SLOT(documentOffsetMoved(QPoint)));
}

CMCanvasControllerDeclarative::~CMCanvasControllerDeclarative()
{
    KoToolManager::instance()->removeCanvasController(this);
}

void CMCanvasControllerDeclarative::setVastScrolling(qreal factor)
{
    d->vastScrollingFactor = factor;
    d->updateMinMax();
}

void CMCanvasControllerDeclarative::setZoomWithWheel(bool zoom)
{
    Q_UNUSED(zoom)
}

void CMCanvasControllerDeclarative::updateDocumentSize(const QSize& sz, bool recalculateCenter)
{
    Q_UNUSED(recalculateCenter)
    proxyObject->emitSizeChanged(sz);
    KoCanvasController::setDocumentSize(sz);
    d->updateMinMax();
}

void CMCanvasControllerDeclarative::resetDocumentOffset(const QPoint& offset)
{
    QPoint o = offset;
    setDocumentOffset(o);
    proxyObject->emitMoveDocumentOffset(offset);
    if(d->updateCanvas) {
        d->updateCanvasSize();
    }
}

void CMCanvasControllerDeclarative::setScrollBarValue(const QPoint& value)
{
    Q_UNUSED(value)
}

QPoint CMCanvasControllerDeclarative::scrollBarValue() const
{
    return documentOffset();
}

void CMCanvasControllerDeclarative::pan(const QPoint& distance)
{
    Q_UNUSED(distance)
}

QPoint CMCanvasControllerDeclarative::preferredCenter() const
{
    return QPoint();
}

void CMCanvasControllerDeclarative::setPreferredCenter(const QPoint& viewPoint)
{
    Q_UNUSED(viewPoint)
}

void CMCanvasControllerDeclarative::recenterPreferred()
{
}

void CMCanvasControllerDeclarative::zoomTo(const QRect& rect)
{
    Q_UNUSED(rect)
}

void CMCanvasControllerDeclarative::zoomBy(const QPoint& center, qreal zoom)
{
    qreal tempZoom = d->zoom * zoom;

    if(tempZoom > KoZoomMode::minimumZoom() && tempZoom < KoZoomMode::maximumZoom()) {
        proxyObject->emitZoomBy(zoom);
        d->zoom = tempZoom;
        
        QPointF offset = documentOffset();
        QPointF position;
        position.rx() = (zoom * -offset.x()) + (1 - zoom) * center.x();
        position.ry() = (zoom * -offset.y()) + (1 - zoom) * center.y();

        QPoint oNew = (-position).toPoint();

        d->updateMinMax();
        d->updateCanvasSize();
        resetDocumentOffset(oNew);
        d->timer->start();
    }
}

void CMCanvasControllerDeclarative::zoomOut(const QPoint& center)
{
    if(center.isNull()) {
        zoomBy(QPoint(width()/ 2, height() / 2), sqrt(0.5));
    } else {
        zoomBy(center, sqrt(0.5));
    }
}

void CMCanvasControllerDeclarative::zoomIn(const QPoint& center)
{
    if(center.isNull()) {
        zoomBy(QPoint(width()/ 2, height() / 2), sqrt(2.0));
    } else {
        zoomBy(center, sqrt(2.0));
    }
}

void CMCanvasControllerDeclarative::resetZoom()
{
    d->zoomController->setZoom(KoZoomMode::ZOOM_CONSTANT, 1.0);
    d->updateMinMax();
    d->updateCanvasSize();
}

void CMCanvasControllerDeclarative::ensureVisible(KoShape* shape)
{
    Q_UNUSED(shape)
}

void CMCanvasControllerDeclarative::ensureVisible(const QRectF& rect, bool smooth)
{
    Q_UNUSED(rect)
    Q_UNUSED(smooth)
}

int CMCanvasControllerDeclarative::canvasOffsetY() const
{
    return documentOffset().y();
}

int CMCanvasControllerDeclarative::canvasOffsetX() const
{
    return documentOffset().x();
}

int CMCanvasControllerDeclarative::visibleWidth() const
{
    return width();
}

int CMCanvasControllerDeclarative::visibleHeight() const
{
    return height();
}

KoCanvasBase* CMCanvasControllerDeclarative::canvas() const
{
    return d->canvas;
}

void CMCanvasControllerDeclarative::setCanvas(KoCanvasBase* canvas)
{
    Q_ASSERT(canvas);

    if (d->canvas) {
        proxyObject->emitCanvasRemoved(this);
        d->canvas->canvasItem()->hide();
        canvas->setCanvasController(0);
        canvas->canvasItem()->removeSceneEventFilter(this);
    }

    d->canvas = canvas;

    KoToolManager::instance()->addController(this);

    zoomController(canvas->viewConverter(), true);

    canvas->setCanvasController(this);
    canvas->canvasItem()->setParentItem(this);
    canvas->canvasItem()->installEventFilter(this);
    canvas->canvasItem()->setAcceptTouchEvents(true);
    canvas->canvasItem()->grabGesture(Qt::PinchGesture);
    canvas->canvasItem()->show();

    d->updateCanvasSize();
    d->updateMinMax();

    resetDocumentOffset(QPoint(d->minX, d->minY));
    setFocusProxy(d->canvas->canvasItem());
    proxyObject->emitCanvasSet(this);
    d->timer->start();
}

void CMCanvasControllerDeclarative::setDrawShadow(bool drawShadow)
{
    Q_UNUSED(drawShadow)
}

QSize CMCanvasControllerDeclarative::viewportSize() const
{
    return QSize(width(), height());
}

void CMCanvasControllerDeclarative::scrollContentsBy(int dx, int dy)
{
    if(!d->updateCanvas)
        return;

    if(d->minX == d->maxX || d->minY == d->maxY)
        d->updateMinMax();


    QPoint offset = documentOffset();
    offset.setX(offset.x() + dx);
    offset.setY(offset.y() + dy);

    resetDocumentOffset(offset);
}

bool CMCanvasControllerDeclarative::eventFilter(QObject* target , QEvent* event )
{
    if(target == this || target == d->canvas->canvasItem()) {
        if(event->type() == QEvent::GraphicsSceneMousePress) {
            d->velocity = QVector2D();
            d->timer->stop();
            return true;
        } else if(event->type() == QEvent::GraphicsSceneMouseMove) {
            if(d->updateCanvas)
                d->handleMouseMoveEvent(static_cast<QGraphicsSceneMouseEvent*>(event));
            return true;
        } else if(event->type() == QEvent::GraphicsSceneMouseRelease) {
            d->timer->start();
            return true;
        } else if(event->type() == QEvent::TouchBegin) {
            event->accept();
            return true;
        } else if(event->type() == QEvent::Gesture) {
            d->handleGesture(static_cast<QGestureEvent*>(event));
            return true;
        }
    }
    return QDeclarativeItem::eventFilter(target, event);
}

KoZoomController* CMCanvasControllerDeclarative::zoomController(KoViewConverter* viewConverter, bool recreate)
{
    if(recreate) {
        delete d->zoomController;
        d->zoomController = 0;
    }

    if(!d->zoomController) {
        d->zoomHandler = dynamic_cast<KoZoomHandler*>(viewConverter);
        if(!d->zoomHandler)
            d->zoomHandler = new KoZoomHandler();

        KActionCollection collection(this);
        d->zoomController = new KoZoomController(this, d->zoomHandler, &collection, 0, this);
    }
    return d->zoomController;
}

void CMCanvasControllerDeclarative::setMargin(int margin)
{
    KoCanvasController::setMargin(margin);
    d->updateMinMax();
}

void CMCanvasControllerDeclarative::onHeightChanged()
{
    d->updateCanvasSize();
}

void CMCanvasControllerDeclarative::onWidthChanged()
{
    d->updateCanvasSize();
}

void CMCanvasControllerDeclarative::Private::handleMouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
    QPointF prev = event->lastPos();
    QPointF cur = event->pos();

    force.setX(prev.x() - cur.x());
    force.setY(prev.y() - cur.y());

    q->scrollContentsBy(force.x(), force.y());
}

void CMCanvasControllerDeclarative::Private::handleGesture(QGestureEvent* event)
{
    QPinchGesture* pinch = qobject_cast<QPinchGesture*>(event->gesture(Qt::PinchGesture));

    if(!pinch)
        return;

    switch(pinch->state())
    {
        case Qt::GestureStarted:
        {
	    updateCanvas = false;
             
            qreal zoomX, zoomY;
            zoomHandler->zoom(&zoomX, &zoomY);
            
            zoomMax = 1.0 + (KoZoomMode::maximumZoom() - zoomX);
            zoomMin = KoZoomMode::minimumZoom() / zoomX;
  
            scaleProxy->setPixmap(QPixmap::grabWindow(QApplication::activeWindow()->winId(), q->x(), q->y(), q->width(), q->height()));
            scaleProxy->setScale(1.0);
            scaleProxy->setVisible(true);
            canvas->canvasItem()->setVisible(false);
        }
        case Qt::GestureUpdated:
        {
            scaleProxy->setTransformOriginPoint(pinch->centerPoint());
            qreal newScale = scaleProxy->scale() + (pinch->scaleFactor() - pinch->lastScaleFactor());
            if(newScale > scaleProxy->scale() && newScale > zoomMax) {
                break;
            } else if(newScale < scaleProxy->scale() && newScale < zoomMin) {
                break;
            }
            scaleProxy->setScale(scaleProxy->scale() + (pinch->scaleFactor() - pinch->lastScaleFactor()));
            break;
        }
        case Qt::GestureFinished:
            scaleProxy->setVisible(false);
            canvas->canvasItem()->setVisible(true);
            q->zoomBy(pinch->centerPoint().toPoint(), scaleProxy->scale());
	    updateCanvas = true;
            break;
    }
    
}

void CMCanvasControllerDeclarative::Private::updateMinMax()
{
    minX = -(q->width() * vastScrollingFactor) - q->margin();
    minY = -(q->height() * vastScrollingFactor) - q->margin();
    maxX = (q->documentSize().width() + (q->width() * vastScrollingFactor) ) - q->width() + q->margin();
    maxY = (q->documentSize().height() + (q->height() * vastScrollingFactor) ) - q->height() + q->margin();
}

void CMCanvasControllerDeclarative::Private::updateCanvasSize()
{
    if(canvas) {
        canvas->canvasItem()->setX(0);
        canvas->canvasItem()->setY(0);
        canvas->canvasItem()->setGeometry(0, 0, q->width(), q->height());
        canvas->updateCanvas(canvas->viewConverter()->viewToDocument(QRectF(0, 0, q->width(), q->height())));
        canvas->canvasItem()->update();
    }
}

void CMCanvasControllerDeclarative::documentOffsetMoved(const QPoint& point)
{
    Q_UNUSED(point);
    d->updateMinMax();
}

void CMCanvasControllerDeclarative::timerUpdate()
{
    QVector2D position = QVector2D(documentOffset());
    QVector2D totalForce = d->force - (d->dragCoeff * d->velocity);
    d->force = QVector2D();

    QVector2D accel = totalForce / d->mass;

    position += d->velocity;
    if(position.x() < d->minX) {
        position.setX(position.x() * d->springCoeff);
    }
    if(position.y() < d->minY) {
        position.setY(position.y() * d->springCoeff);
    }

    if(position.x() > d->maxX) {
        float diff = position.x() - d->maxX;
        position.setX(d->maxX + diff * 0.9);
    }
    if(position.y() > d->maxY) {
        float diff = position.y() - d->maxY;
        position.setY(d->maxY + diff * 0.9);
    }

    d->velocity += accel * d->timeStep;

    resetDocumentOffset(QPoint(position.x(), position.y()));
}

