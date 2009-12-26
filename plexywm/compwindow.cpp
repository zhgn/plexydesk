/*******************************************************************************
* This file is part of PlexyDesk.
*  Maintained by : Siraj Razick <siraj@kde.org>
*  Authored By  :
*
*  PlexyDesk is free software: you can redistribute it and/or modify
*  it under the terms of the GNU Lesser General Public License as published by
*  the Free Software Foundation, either version 3 of the License, or
*  (at your option) any later version.
*
*  PlexyDesk is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU Lesser General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with PlexyDesk. If not, see <http://www.gnu.org/licenses/lgpl.html>
*******************************************************************************/

/*special credit goes to Pyrodesk project, the logic of plexydeskwm is based on
  pyrodesk's compzilla windows manager */

#include "compwindow.h"
#include "XAtoms.h"

extern "C" {
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/shape.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/extensions/Xdamage.h>

}
#include <QX11Info>
#include <QGraphicsView>
#include "plexywindows.h"
//#include <qq.h>

//plexy
#include <plexy.h>
#include <baserender.h>
//#include <desktopview.h>
#include <pluginloader.h>
#include <fakemime.h>
#include <datainterface.h>
#include <canvas.h>
#include <plexyconfig.h>
#include <netwm.h>

class CompWindow::Private
{
public:
    Private() {}
    ~Private() {}
    Display* mDisplay;
    Window  mRootWindow;

    Window mMainWin;
    Window mMainwinParent;
    Window mOverlay;
    Window mManagerWindow;

    QGraphicsView* canvasview;
    PlexyDesk::Canvas * scene;

    bool mCompositing;
    bool mManging;
    XAtoms atoms;
    QMap<Window, PlexyWindows*> windowMap;
    //variables required for even mapping.. etc
    int opcode;
    int composite_event, composite_error, xfixes_event, xfixes_error;
    int shape_event, shape_error, damage_event, damage_error;
    int composite_major, composite_minor;

    //variables
};

CompWindow::CompWindow(int & argc, char ** argv):QApplication(argc, argv), d(new Private)
{
    d->mDisplay =  QX11Info::display();
    d->mRootWindow = QApplication::desktop()->winId();
    
    d->scene = new PlexyDesk::Canvas();
    d->scene->setBackgroundBrush(Qt::NoBrush);
    d->scene->setItemIndexMethod(QGraphicsScene::NoIndex);
    d->scene->setSceneRect(QDesktopWidget().geometry());//TODO Resolution changes ?

    d->canvasview= new QGraphicsView(d->scene);

    d->canvasview->setWindowFlags(Qt::X11BypassWindowManagerHint);
  //  d->canvasview->setAttribute(Qt::WA_NoSystemBackground);
  //  d->canvasview->setUpdatesEnabled(false);
   // d->canvasview->setAutoFillBackground(false);
   // d->canvasview->setBackgroundBrush(Qt::NoBrush);
   // d->canvasview->setForegroundBrush(Qt::NoBrush);

    //d->canvasview->enableOpenGL(false);
    QRect r = QDesktopWidget().geometry();
    d->canvasview->resize(QDesktopWidget().availableGeometry().size());

    d->canvasview->show();
    QStringList list = PlexyDesk::Config::getInstance()->widgetList;

    init();
}


CompWindow::~CompWindow()
{
    delete d;
}

void CompWindow::addWindow(Window window)
{
    XWindowAttributes attrs;
    if (!XGetWindowAttributes(d->mDisplay, window, &attrs)) {
        qDebug()<<"Error adding windows, getting window attributes failed"<<endl;
        return;
    }

    if (attrs.c_class == InputOnly) {
        return;
    }

    PlexyWindows *  _window  = new PlexyWindows(d->mDisplay, window, &attrs);
    d->windowMap[window] = _window;
    d->canvasview->scene()->addItem(_window);
    _window->show();
    qDebug() << Q_FUNC_INFO << endl;
}


//utility

bool CompWindow::isWmRunning()
{
    Atom wmAtom;
    wmAtom  = XInternAtom(d->mDisplay, "WM_S0",false);
    bool hasWm =  XGetSelectionOwner(d->mDisplay, wmAtom) != None;
    return  hasWm;
}

void CompWindow::init()
{
    qDebug()<<Q_FUNC_INFO<<endl;

    if (!isWmRunning()) {
        XSetWindowAttributes attrs;
        attrs.override_redirect = True;
        attrs.event_mask = PropertyChangeMask;

        d->mManagerWindow = XCreateWindow (d->mDisplay,
                                           d->mRootWindow,
                                           -100, -100, 1, 1,
                                           0,
                                           CopyFromParent,
                                           CopyFromParent,
                                           (Visual *)CopyFromParent,
                                           CWOverrideRedirect | CWEventMask,
                                           &attrs);
        Atom wmAtom = XInternAtom(d->mDisplay, "WM_S0", false);
        if (!registerWindowManager(d->mManagerWindow, wmAtom)) {
            qDebug()<<"Register window failed"<<endl;
        }
        Xutf8SetWMProperties (d->mDisplay, d->mManagerWindow, "plexydeskwm",
                              "plexydeskwm", NULL, 0, NULL, NULL, NULL);
        Atom cmAtom = XInternAtom(d->mDisplay, "_NET_WM_CM_S0", false);
        if (!registerWindowManager(d->mManagerWindow, cmAtom)) {
            qDebug()<<"Register Composite manager failed"<<endl;
        }
        registerAtoms();
        if (!checkExtensions()) {
            qDebug()<<"Some or all extensions are missing or out dated, upgrade and check again, thanks ";
        }


        startOverlay();
        setupWindows();

        Cursor normal = XCreateFontCursor(d->mDisplay, XC_left_ptr);
        XDefineCursor(d->mDisplay, d->mRootWindow, normal);

    } else {
        qDebug()<<"Another Window manager already running.. "<<endl;
        qApp->quit();
    }
}

void CompWindow::registerAtoms()
{
    bool result = XInternAtoms (d->mDisplay,
                                atom_names, sizeof (atom_names) / sizeof (atom_names[0]),
                                false,
                                d->atoms.a);
    if (!result)
        qDebug()<<"Registration of atoms:" <<atom_names<<"Failed"<<endl;
}
bool CompWindow::registerWindowManager(Window getOwner, Atom wmAtom)
{

    Window  owner = XGetSelectionOwner(d->mDisplay, wmAtom);

    if (owner != None)
        XSelectInput (d->mDisplay, owner, StructureNotifyMask);

    XSetSelectionOwner(d->mDisplay, wmAtom, getOwner, CurrentTime);

    if (XGetSelectionOwner (d->mDisplay, wmAtom) != getOwner) {
        qDebug() << "Error registering Window Manager"<<endl;
        return false;
    }

    XClientMessageEvent cm;
    cm.window = d->mRootWindow;
    cm.message_type = XInternAtom(d->mDisplay, "MANAGER", false);
    cm.type =  ClientMessage;
    cm.format = 32;
    cm.data.l[0] = CurrentTime;
    cm.data.l[1] = wmAtom;

    XSendEvent(d->mDisplay, d->mRootWindow, false, StructureNotifyMask, (XEvent*)&cm);
    if (owner != None) {
        XEvent event;
        do {
            XWindowEvent (d->mDisplay, owner, StructureNotifyMask, &event);
            qDebug()<<"Waiting for current owner .."<<endl;
        } while (event.type != DestroyNotify);
    }
    return true;
}


bool CompWindow::registerCompositeManager()
{
    Atom wmAtom;
    XSetWindowAttributes attrs;
    attrs.override_redirect = True;
    attrs.event_mask = PropertyChangeMask;

    wmAtom = XInternAtom(d->mDisplay, "WM_S0", false);
    Window  owner = XGetSelectionOwner(d->mDisplay, wmAtom);
    Window  getOwner  = XCreateWindow(d->mDisplay, d->mRootWindow, -100, -100, 1, 1, 0, CopyFromParent,
                                      CopyFromParent, (Visual*) CopyFromParent, CWOverrideRedirect | CWEventMask, & attrs);

    if (owner != None)
        XSelectInput (d->mDisplay, owner, StructureNotifyMask);

    XSetSelectionOwner(d->mDisplay, wmAtom, getOwner, CurrentTime);

    if (XGetSelectionOwner (d->mDisplay, wmAtom) != getOwner) {
        qDebug() << "Error registering Window Manager"<<endl;
        return false;
    }

    XClientMessageEvent cm;
    cm.window = d->mRootWindow;
    cm.message_type = XInternAtom(d->mDisplay, "MANAGER", false);
    cm.type =  ClientMessage;
    cm.format = 32;
    cm.data.l[0] = CurrentTime;
    cm.data.l[1] = wmAtom;

    XSendEvent(d->mDisplay, d->mRootWindow, false, StructureNotifyMask, (XEvent*)&cm);
    if (owner != None) {
        XEvent event;
        do {
            XWindowEvent (d->mDisplay, owner, StructureNotifyMask, &event);
            qDebug()<<"Waiting for current owner .."<<endl;
        } while (event.type != DestroyNotify);
    }
    return true;
}


bool CompWindow::checkExtensions()
{
    int opcode;
    int composite_event, composite_error, xfixes_event, xfixes_error;
    int shape_event, shape_error, damage_event, damage_error;
    int composite_major, composite_minor;

    if (!XQueryExtension (d->mDisplay, COMPOSITE_NAME, &opcode,
                          &composite_event, &composite_error)) {
        qDebug()<<"missing composite extension\n";
        return false;
    }
    XCompositeQueryVersion (d->mDisplay, &d->composite_major, &composite_minor);
    if (composite_major == 0 && composite_minor < 2) {
        qDebug()<<"libXcomposite too old, upgrade your package";
        return false;
    }

    if (!XDamageQueryExtension (d->mDisplay, &d->damage_event, &d->damage_error)) {
        qDebug()<<"Missing damage extension";
        return false;
    }

    if (!XFixesQueryExtension (d->mDisplay, &d->xfixes_event, &d->xfixes_error)) {
        qDebug()<<"Missing XFixes extension";
        return false;
    }
#if HAVE_XSHAPE
    if (!XShapeQueryExtension (d->mDisplay, &d->shape_event, &d->shape_error)) {
        qDebug()<<"Missing Shaped window extension";
        return false;
    }
#endif

    return true;
}



bool CompWindow::startOverlay()
{
    d->mOverlay = XCompositeGetOverlayWindow (d->mDisplay, d->mRootWindow);
    if (!d->mOverlay) {
        qDebug()<<"Overly window can not start"<<endl;
    }

    d->mMainWin = d->canvasview->winId();
    XReparentWindow (d->mDisplay, d->canvasview->winId(), d->mOverlay, 0, 0);
    input(d->mOverlay);
    input(d->canvasview->winId());
   
}

void CompWindow::input(Window w)
{
    XserverRegion region;
    XRectangle rect = { 0, 0, DisplayWidth(d->mDisplay, 0), DisplayHeight(d->mDisplay, 0) };
    region = XFixesCreateRegion(d->mDisplay, &rect, 1);
    XFixesSetWindowShapeRegion(d->mDisplay, w, ShapeBounding, 0, 0, 0);
    XFixesSetWindowShapeRegion(d->mDisplay, w, ShapeInput, 0, 0, 0);
    XFixesDestroyRegion(d->mDisplay, region);
}

void CompWindow::setupWindows()
{
    XGrabServer (d->mDisplay);

    long ev_mask = (SubstructureRedirectMask |
                    SubstructureNotifyMask |
                    StructureNotifyMask |
                    ResizeRedirectMask |
                    PropertyChangeMask
                    |
                    FocusChangeMask);
    XSelectInput (d->mDisplay, d->mRootWindow, ev_mask);

    Window root_notused, parent_notused;
    Window *children;
    unsigned int nchildren;
    XWindowAttributes attr;

    XQueryTree (d->mDisplay,
                d->mRootWindow,
                &root_notused,
                &parent_notused,
                &children,
                &nchildren);

    for (int i = 0; i < nchildren; i++) {
        qDebug() << Q_FUNC_INFO ;

        XGetWindowAttributes(QX11Info::display(), children[i], &attr);
        if (attr.map_state == IsViewable && children[i] != d->canvasview->winId() && attr.width > 1 &&
         attr.height > 1) {
            addWindow(children[i]);
        }

    }
    XFree (children);
    XUngrabServer (d->mDisplay);
}


Window CompWindow::GetEventXWindow (XEvent *xev)
{
    switch (xev->type) {
    case ClientMessage:
        return xev->xclient.window;
    case CreateNotify:
        return xev->xcreatewindow.window;
    case DestroyNotify:
        return xev->xdestroywindow.window;
    case ConfigureNotify:
        return xev->xconfigure.window;
    case ConfigureRequest:
        return xev->xconfigurerequest.window;
    case ReparentNotify:
        return xev->xreparent.window;
    case MapRequest:
        return xev->xmaprequest.window;
    case MapNotify:
        return xev->xmap.window;
    case UnmapNotify:
        return xev->xunmap.window;
    case PropertyNotify:
        return xev->xproperty.window;
    case FocusIn:
    case FocusOut:
        return xev->xfocus.window;
    default:
        if (xev->type == d->damage_event + XDamageNotify) {
            XDamageNotifyEvent *damage_ev = (XDamageNotifyEvent *) xev;
            return damage_ev->drawable;
        }
        else if (xev->type == d->xfixes_event + XFixesCursorNotify) {
            XFixesCursorNotifyEvent *cursor_ev = (XFixesCursorNotifyEvent *) xev;
            return cursor_ev->window;
        }
        else if (xev->type == d->shape_event + ShapeNotify) {
            XShapeEvent *shape_ev = (XShapeEvent *) xev;
            return shape_ev->window;
        }
    }

    return None;
}

bool CompWindow::x11EventFilter( XEvent* event)
{
    XEvent * xev = (XEvent*) event;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];


         XVisibilityEvent desk_notify;
        desk_notify.type       = VisibilityNotify;
        desk_notify.send_event = True;
        desk_notify.window     = xwin;
        desk_notify.state      = VisibilityUnobscured;
        XSendEvent( QX11Info::display(), xwin, true, VisibilityChangeMask, (XEvent*)&desk_notify);
   if (xev->type == Expose || xev->type == VisibilityNotify) {
        return false;
    }


    d->damage_event = d->damage_event + XDamageNotify;
    if (event->type == d->damage_event ) {
        XDamageNotifyEvent *damage_ev = (XDamageNotifyEvent *) xev;
        do {
            win->Damaged (&damage_ev->area);
        } while (XCheckTypedEvent (d->mDisplay, d->damage_event + XDamageNotify, xev));

        return true;
    }//done
    switch (event->type) {
    case ClientMessage:
        clientMsgNotify(event);
        break;
    case CreateNotify:
        createNotify(event);
        break;
    case DestroyNotify:
        destroyNotify(event);
    case ConfigureNotify:
        break;
    case ConfigureRequest:
        configureRequest(event);
        break;
    case ReparentNotify:
        reparentNotify(event);
        break;
    case MapRequest:
        mapRequest(event);
        break;
    case MapNotify:
        mapNotify(event);
        break;
    case UnmapNotify:
        unmapNotify(event);
        break;
    case PropertyNotify:
        propertyNotify(event);
        break;
    case FocusIn:
    case FocusOut:
    default:
        return false;
    };


    return false;
}

void CompWindow::destroyNotify(XEvent* e)
{
    XEvent * xev = (XEvent*) e;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];
    d->windowMap.remove(xwin);
    if (win)
        delete win;

    qDebug() << Q_FUNC_INFO << endl;
}

void CompWindow::configureRequest(XEvent* e)
{
    XEvent * xev = (XEvent*) e;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];
    if (xev->xconfigurerequest.parent == d->mRootWindow) {
    }
    qDebug() << Q_FUNC_INFO <<endl;
}

void CompWindow::configureNotify(XEvent* e)
{
    XEvent * xev = (XEvent*) e;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];
    if (xwin == d->mOverlay) {
        return;
    }

    if (e->xconfigure.override_redirect == True) {
        return;
    }

    win->Configured(true,
                    xev->xconfigure.x,
                    xev->xconfigure.y,
                    xev->xconfigure.width,
                    xev->xconfigure.height,
                    xev->xconfigure.border_width,
                    0,
                    xev->xconfigure.override_redirect);

    qDebug() << Q_FUNC_INFO <<endl;
}

void CompWindow::mapRequest(XEvent* e)
{
    XEvent * xev = (XEvent*) e;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];

    if (xwin == d->mOverlay) {
        return;
    }

    if (xev->xmaprequest.parent == d->mRootWindow) {
        if (!XCheckTypedWindowEvent (d->mDisplay, xwin, UnmapNotify, xev)) {
            XMapWindow (d->mDisplay, xwin);
            qDebug() << Q_FUNC_INFO  << "XMaping" <<endl;
        }
    }

    qDebug() << Q_FUNC_INFO <<endl;
}

void CompWindow::getWindowType(Window w)
{
    Atom actual;
    int format;
    unsigned long n, left;
    unsigned char *data;

//XGetWindowProperty(QX11Info::display(), w, XAtoms["_NET_WM_WINDOW_TYPE"], 0L, 1L, False, XA_ATOM, &actual,
  //          &format,&n, &left, &data);

}
void CompWindow::createNotify(XEvent* e)
{
    XEvent * xev = (XEvent*) e;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];

    /*
    if (d->mMainWin == xwin) {
        return;
    }
*/
    if (xwin == d->mOverlay) {
        return;
    }

    if (xwin == d->canvasview->winId()) {
        return;
    }

    if ( xwin == d->canvasview->viewport()->winId()) {
        return;

    }

    if (xev->xcreatewindow.parent == d->mRootWindow) {
        if (!win) {


            if (!XCheckTypedWindowEvent (d->mDisplay, xwin, DestroyNotify, xev) &&
                    !XCheckTypedWindowEvent (d->mDisplay, xwin, ReparentNotify, xev)) {
                addWindow (xwin);
            }
        }
    }
    qDebug() << Q_FUNC_INFO <<endl;
}

void CompWindow::clientMsgNotify(XEvent* e)
{
    XEvent * xev = (XEvent*) e;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];

    qDebug() << Q_FUNC_INFO <<endl;
}

void CompWindow::reparentNotify(XEvent* e)
{
    qDebug() << Q_FUNC_INFO <<endl;
}

void CompWindow::mapNotify(XEvent* e)
{
    XEvent * xev = (XEvent*) e;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];

    if (win) {
        if (!XCheckTypedWindowEvent (d->mDisplay, xwin, UnmapNotify, xev)) {
            win->Mapped (xev->xmap.override_redirect);
        }
    }
    qDebug() << Q_FUNC_INFO <<endl;
}

void CompWindow::unmapNotify(XEvent* e)
{
    qDebug() << Q_FUNC_INFO <<endl;
    XEvent * xev = (XEvent*) e;
    Window  xwin = GetEventXWindow(xev);
    PlexyWindows * win  = d->windowMap[xwin];

    if (xwin == d->mOverlay) {
        return;
    }
}


void CompWindow::propertyNotify(XEvent* e)
{
    qDebug() << Q_FUNC_INFO <<endl;
}
