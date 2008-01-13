#include "desktopview.h"
#include <backdropfactory.h>
#include <QGLWidget>


namespace PlexyDesk
{
class  DesktopView::Private
{
    public:
    Private(){}
    ~Private(){}
    BackdropInterface * bIface;
    BackdropFactory * bgfact;
};

DesktopView::DesktopView ( QGraphicsScene * scene, QWidget * parent ):QGraphicsView(scene,parent),d(new Private)
{
       setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
       setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

       //setCacheMode(QGraphicsView::CacheBackground);
       setViewportUpdateMode(QGraphicsView::MinimalViewportUpdate);

      // setRenderHints(QPainter::HighQualityAntialiasing);
      // setOptimizationFlag(QGraphicsView::DontClipPainter,false);
      // setOptimizationFlag(QGraphicsView::DontSavePainterState,true);
       setAlignment(Qt::AlignLeft | Qt::AlignTop);
       d->bIface  = 0;
       d->bgfact = new BackdropFactory(0);
       d->bIface = d->bgfact->instance();
    //   setInteractive(true);
}

DesktopView::~DesktopView()
{
    delete d;
}


void DesktopView::drawBackground ( QPainter * painter, const QRectF & rect )
{
    qDebug()<<rect<<endl;
    painter->save();
    painter->setClipRect(rect);
    d->bIface->render(painter,QRectF(rect.x(),rect.y(),rect.width(),rect.height()));
    painter->restore();
}


}
#include "desktopview.moc"