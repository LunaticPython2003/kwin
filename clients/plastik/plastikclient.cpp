/* Plastik KWin window decoration
  Copyright (C) 2003 Sandro Giessl <ceebx@users.sourceforge.net>

  based on the window decoration "Web":
  Copyright (C) 2001 Rik Hemsley (rikkus) <rik@kde.org>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; see the file COPYING.  If not, write to
  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
  Boston, MA 02111-1307, USA.
 */

#include <klocale.h>
#include <kpixmap.h>
#include <kpixmapeffect.h>

#include <qbitmap.h>
#include <qdatetime.h>
#include <qfontmetrics.h>
#include <qimage.h>
#include <qlabel.h>
#include <qlayout.h>
#include <qpainter.h>
#include <qpixmap.h>

#include "plastikclient.h"
#include "plastikclient.moc"
#include "plastikbutton.h"
#include "misc.h"
#include "shadow.h"

// global constants
static const int TOPMARGIN       = 4; // do not change
static const int DECOHEIGHT      = 2; // do not change
static const int SIDETITLEMARGIN = 6;

// Default button layout
const char default_left[]  = "M";
const char default_right[] = "HIAX";

namespace KWinPlastik
{

PlastikClient::PlastikClient(KDecorationBridge* bridge, KDecorationFactory* factory)
    : KDecoration (bridge, factory),
    mainLayout_(0),
    topSpacer_(0), titleSpacer_(0), leftTitleSpacer_(0), rightTitleSpacer_(0),
    decoSpacer_(0), leftSpacer_(0), rightSpacer_(0), bottomSpacer_(0),
    aCaptionBuffer(0), iCaptionBuffer(0),
    aTitleBarTile(0), iTitleBarTile(0), aTitleBarTopTile(0), iTitleBarTopTile(0),
    pixmaps_created(false),
    captionBufferDirty(true),
    closing(false),
    s_titleHeight(0),
    s_titleFont(QFont() )
{ }

PlastikClient::~PlastikClient()
{
    delete_pixmaps();

    delete aCaptionBuffer;
    delete iCaptionBuffer;

    for (int n=0; n<NumButtons; n++) {
        if (m_button[n]) delete m_button[n];
    }
}

void PlastikClient::init()
{
    s_titleHeight = isTool() ?
            PlastikHandler::titleHeightTool()
            : PlastikHandler::titleHeight();
    s_titleFont = isTool() ?
            PlastikHandler::titleFontTool()
            : PlastikHandler::titleFont();

    createMainWidget(WNoAutoErase);

    widget()->installEventFilter( this );

    // for flicker-free redraws
    widget()->setBackgroundMode(NoBackground);

    _resetLayout();

    create_pixmaps();

    aCaptionBuffer = new QPixmap();
    iCaptionBuffer = new QPixmap();
    captionBufferDirty = true;
    widget()->update(titleSpacer_->geometry());
}

bool PlastikClient::isTool()
{
    NET::WindowType type = windowType(NET::NormalMask|NET::ToolbarMask|
                                      NET::UtilityMask|NET::MenuMask);
    return ((type==NET::Toolbar)||(type==NET::NET::Utility)||(type==NET::Menu));
}

void PlastikClient::resizeEvent()
{
    doShape();
    widget()->update();
}

void PlastikClient::paintEvent(QPaintEvent*)
{
    if (!PlastikHandler::initialized()) return;

    if (captionBufferDirty)
        update_captionBuffer();

    bool active = isActive();

    QPainter painter(widget() );

    QRegion mask;

    // colors...
    const QColor windowContour = PlastikHandler::getColor(WindowContour, active);
    const QColor deco = PlastikHandler::getColor(TitleGradientTo, active);
    const QColor border = PlastikHandler::getColor(Border, active);
    const QColor highlightTop = PlastikHandler::getColor(TitleHighlightTop, active);
    const QColor highlightTitleLeft = alphaBlendColors(deco,
            PlastikHandler::getColor(SideHighlightLeft, active), 150);
    const QColor highlightTitleRight = alphaBlendColors(deco,
            PlastikHandler::getColor(SideHighlightRight, active), 150);
    const QColor highlightLeft = alphaBlendColors(border,
            PlastikHandler::getColor(SideHighlightLeft, active), 150);
    const QColor highlightRight = alphaBlendColors(border,
            PlastikHandler::getColor(SideHighlightRight, active), 150);
    const QColor highlightBottom = alphaBlendColors(border,
            PlastikHandler::getColor(SideHighlightBottom, active), 150);

    QRect Rtop(topSpacer_->geometry());
    QRect Rtitle(titleSpacer_->geometry());
    QRect Rltitle(leftTitleSpacer_->geometry());
    QRect Rrtitle(rightTitleSpacer_->geometry());
    QRect Rdeco(decoSpacer_->geometry());
    QRect Rleft(leftSpacer_->geometry());
    QRect Rright(rightSpacer_->geometry());
    QRect Rbottom(bottomSpacer_->geometry());
    QRect tempRect;

    // topSpacer
    if(Rtop.height() > 0)
    {
        painter.setPen(windowContour );
        painter.drawLine(Rtop.left()+2, Rtop.top(), Rtop.right()-2, Rtop.top() );
        painter.drawPoint(Rtop.left()+1, Rtop.top()+1 );
        painter.drawPoint(Rtop.right()-1, Rtop.top()+1 );
        painter.drawLine(Rtop.left(), Rtop.top()+2, Rtop.left(), Rtop.bottom() );
        painter.drawLine(Rtop.right(), Rtop.top()+2, Rtop.right(), Rtop.bottom() );
        painter.setPen(highlightTop );
        painter.drawLine(Rtop.left()+3, Rtop.top()+1, Rtop.right()-3, Rtop.top()+1 );
        // a bit anti-aliasing for the window contour...
        painter.setPen(alphaBlendColors(highlightTop, windowContour, 150) );
        painter.drawPoint(Rtop.left()+2, Rtop.top()+1);
        painter.drawPoint(Rtop.right()-2, Rtop.top()+1);
        painter.setPen(alphaBlendColors(highlightTitleLeft, windowContour, 150) );
        painter.drawPoint(Rtop.left()+1, Rtop.top()+2);
        painter.setPen(alphaBlendColors(highlightTitleRight, windowContour, 150) );
        painter.drawPoint(Rtop.right()-1, Rtop.top()+2);
        // highlight...
        painter.setPen(highlightTitleLeft);
        painter.drawLine(Rtop.left()+1, Rtop.top()+3, Rtop.left()+1, Rtop.bottom() );
        painter.setPen(highlightTitleRight);
        painter.drawLine(Rtop.right()-1, Rtop.top()+3, Rtop.right()-1, Rtop.bottom() );

        painter.drawTiledPixmap(Rtop.left()+2, Rtop.top()+2,
                                Rtop.width()-2*2, Rtop.height()-2,
                                active ? *aTitleBarTopTile : *iTitleBarTopTile );
    }

    // leftTitleSpacer
    if(Rltitle.width() > 0)
    {
        painter.setPen(windowContour );
        painter.drawLine(Rltitle.left(), Rltitle.top(),
                         Rltitle.left(), Rltitle.bottom() );
        painter.setPen(highlightTitleLeft);
        painter.drawLine(Rltitle.left()+1, Rltitle.top(),
                         Rltitle.left()+1, Rltitle.bottom() );

        painter.drawTiledPixmap(Rltitle.left()+2, Rltitle.top(), Rltitle.width()-2, Rltitle.height(),
                active ? *aTitleBarTile : *iTitleBarTile );
    }

    // Space under the left button group
    painter.drawTiledPixmap(Rltitle.right()+1, Rtitle.top(),
                            (Rtitle.left()-1)-Rltitle.right(), Rtitle.height(),
                            active ? *aTitleBarTile : *iTitleBarTile );


    // rightTitleSpacer
    if(Rrtitle.width() > 0)
    {
        painter.setPen(windowContour );
        painter.drawLine(Rrtitle.right(), Rrtitle.top(),
                         Rrtitle.right(), Rrtitle.bottom() );
        painter.setPen(highlightTitleRight);
        painter.drawLine(Rrtitle.right()-1, Rrtitle.top(),
                         Rrtitle.right()-1, Rrtitle.bottom() );

        painter.drawTiledPixmap(Rrtitle.left(), Rrtitle.top(), Rrtitle.width()-2, Rrtitle.height(),
                active ? *aTitleBarTile : *iTitleBarTile );
    }

    // Space under the right button group
    painter.drawTiledPixmap(Rtitle.right()+1, Rtitle.top(),
                            (Rrtitle.left()-1)-Rtitle.right(), Rtitle.height(),
                            active ? *aTitleBarTile : *iTitleBarTile );

    // titleSpacer
    QPixmap *titleBfrPtr = active ? aCaptionBuffer : iCaptionBuffer;
    if(Rtitle.width() > 0 && titleBfrPtr != 0)
    {
        const int titleMargin = 5; // 5 px betwee title and buttons

        int tX, tW;
        switch (PlastikHandler::titleAlign())
        {
        // AlignCenter
        case Qt::AlignHCenter:
            tX = (titleBfrPtr->width() > Rtitle.width()-2*titleMargin) ?
                    (Rtitle.left()+titleMargin)
                    : Rtitle.left()+(Rtitle.width()- titleBfrPtr->width() )/2;
            tW = (titleBfrPtr->width() > Rtitle.width()-2*titleMargin) ?
                    (Rtitle.width()-2*titleMargin)
                    : titleBfrPtr->width();
            break;
        // AlignRight
        case Qt::AlignRight:
            tX = (titleBfrPtr->width() > Rtitle.width()-2*titleMargin) ?
                    (Rtitle.left()+titleMargin)
                    : Rtitle.right()-titleMargin-titleBfrPtr->width();
            tW = (titleBfrPtr->width() > Rtitle.width()-2*titleMargin) ?
                    (Rtitle.width()-2*titleMargin)
                    : titleBfrPtr->width();
            break;
        // AlignLeft
        default:
            tX = (Rtitle.left()+titleMargin);
            tW = (titleBfrPtr->width() > Rtitle.width()-2*titleMargin) ?
                    (Rtitle.width()-2*titleMargin)
                    : titleBfrPtr->width();
        }

        if(tW > 0)
        {
            painter.drawTiledPixmap(tX, Rtitle.top(),
                                    tW, Rtitle.height(),
                                    *titleBfrPtr);
        }

        painter.drawTiledPixmap(Rtitle.left(), Rtitle.top(),
                                tX-Rtitle.left(), Rtitle.height(),
                                active ? *aTitleBarTile : *iTitleBarTile);

        painter.drawTiledPixmap(tX+tW, Rtitle.top(),
                                Rtitle.right()-(tX+tW)+1, Rtitle.height(),
                                active ? *aTitleBarTile : *iTitleBarTile);
    }
    titleBfrPtr = 0;

    // decoSpacer
    if(Rdeco.height() > 0)
    {
        int l;
        if(Rleft.width() != 0)
            l = Rdeco.left()+2;
        else
            l = Rdeco.left();
        int r;
        if(Rright.width() != 0)
            r = Rdeco.right()-2;
        else
            r = Rdeco.right();

        painter.setPen(deco );
        painter.drawLine(l, Rdeco.bottom(), r, Rdeco.bottom() );
        painter.drawLine(l, Rdeco.top(), r, Rdeco.top() );
        if(Rleft.width() != 0) {
            painter.setPen(windowContour );
            painter.drawLine(Rdeco.left(), Rdeco.top(), Rdeco.left(), Rdeco.bottom() );
            painter.setPen(highlightTitleLeft);
            painter.drawLine(Rdeco.left()+1, Rdeco.top(),
                            Rdeco.left()+1, Rdeco.bottom() );
        }
        if(Rright.width() != 0) {
        painter.setPen(windowContour );
        painter.drawLine(Rdeco.right(), Rdeco.top(), Rdeco.right(), Rdeco.bottom() );
        painter.setPen(highlightTitleRight);
        painter.drawLine(Rdeco.right()-1, Rdeco.top(),
                         Rdeco.right()-1, Rdeco.bottom() );
        }
    }

    if(Rleft.height() == 1 && Rright.height() == 1) // the window is shaded... hopefully
    {
        // let's orient on the bottomSpacer which should have the prper x coords...
        painter.setPen(windowContour );
        painter.drawPoint(Rbottom.left(), Rleft.top() );
        painter.drawPoint(Rbottom.right(), Rright.top() );
        painter.setPen(border );
        painter.drawLine(Rbottom.left()+1, Rleft.top(), Rbottom.right()-1, Rright.top() );

    }
    else
    {
        // leftSpacer
        if(Rleft.width() > 0 && Rleft.height() > 0)
        {
            painter.setPen(windowContour );
            painter.drawLine(Rleft.left(), Rleft.top(),
                             Rleft.left(), Rleft.bottom() );
            painter.setPen(highlightLeft );
            painter.drawLine(Rleft.left()+1, Rleft.top(),
                             Rleft.left()+1, Rleft.bottom() );
            if(Rleft.width() > 2) {
                tempRect.setCoords(Rleft.left()+2, Rleft.top(),
                                Rleft.right(), Rleft.bottom() );
                painter.fillRect(tempRect, border );
            }
        }

        // rightSpacer
        if(Rright.width() > 0 && Rright.height() > 0)
        {
            painter.setPen(windowContour );
            painter.drawLine(Rright.right(), Rright.top(),
                             Rright.right(), Rright.bottom() );
            painter.setPen(highlightRight );
            painter.drawLine(Rright.right()-1, Rright.top(),
                             Rright.right()-1, Rright.bottom() );
            if(Rright.width() > 2) {
                tempRect.setCoords(Rright.left(), Rright.top(),
                                Rright.right()-2, Rright.bottom() );
                painter.fillRect(tempRect, border );
            }
        }
    }

    // bottomSpacer
    if(Rbottom.height() > 0)
    {
        painter.setPen(windowContour );
        painter.drawLine(Rbottom.left()+1, Rbottom.bottom(),
                         Rbottom.right()-1, Rbottom.bottom() );

        if(Rleft.width() != 0) {
            painter.setPen(windowContour );
            painter.drawLine(Rbottom.left(), Rbottom.top(),
                            Rbottom.left(), Rbottom.bottom()-1 );
            painter.setPen(highlightLeft );
            painter.drawLine(Rbottom.left()+1, Rbottom.top(),
                            Rbottom.left()+1, Rbottom.bottom()-2 );
            // anti-alias for the window contour...
            painter.setPen(alphaBlendColors(border, windowContour, 110) );
            painter.drawPoint(Rbottom.left()+1, Rbottom.bottom()-1);
        }
        if(Rright.width() != 0) {
            painter.setPen(windowContour );
            painter.drawLine(Rbottom.right(), Rbottom.top(),
                            Rbottom.right(), Rbottom.bottom()-1 );
            painter.setPen(highlightRight );
            painter.drawLine(Rbottom.right()-1, Rbottom.top(),
                            Rbottom.right()-1, Rbottom.bottom()-2 );
            // anti-alias for the window contour...
            painter.setPen(alphaBlendColors(border, windowContour, 110) );
            painter.drawPoint(Rbottom.right()-1, Rbottom.bottom()-1);
        }

        int l;
        if(Rleft.width() != 0)
            l = Rbottom.left()+2;
        else
            l = Rbottom.left();
        int r;
        if(Rright.width() != 0)
            r = Rbottom.right()-2;
        else
            r = Rbottom.right();

        painter.setPen(highlightBottom );
        painter.drawLine(l, Rbottom.bottom()-1,
                         r, Rbottom.bottom()-1 );

        tempRect.setCoords(l, Rbottom.top(), r, Rbottom.bottom()-2);
        painter.fillRect(tempRect, border );
    }
}

void PlastikClient::showEvent(QShowEvent *)
{
    doShape();
    widget()->update();
}


void PlastikClient::windowWrapperShowEvent(QShowEvent *)
{
    doShape();
    widget()->update();
}

void PlastikClient::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (titleSpacer_->geometry().contains(e->pos()))
           titlebarDblClickOperation();
}

void PlastikClient::doShape()
{
    int w = widget()->width();
    int h = widget()->height();
    int r(w);
    int b(h);

    QRegion mask(0, 0, w, h);

    if(topSpacer_->geometry().height() > 0)
    {
        // Remove top-left corner.
        if(leftTitleSpacer_->geometry().width() > 0)
        {
            mask -= QRegion(0, 0, 1, 2);
            mask -= QRegion(1, 0, 1, 1);
        }
        // Remove top-right corner.
        if(rightTitleSpacer_->geometry().width() > 0)
        {
            mask -= QRegion(r-1, 0, 1, 2);
            mask -= QRegion(r-2, 0, 1, 1);
        }
    }

    // Remove bottom-left corner and bottom-right corner.
    if(bottomSpacer_->geometry().height() > 0)
    {
        mask -= QRegion(0, b-1, 1, 1);
        mask -= QRegion(r-1, b-1, 1, 1);
    }

    setMask( mask );
//     widget()->setMask(mask);
}

void PlastikClient::_resetLayout()
{
    // basic layout:
    //  _______________________________________________________________
    // |                         topSpacer                             |
    // |_______________________________________________________________|
    // | leftTitleSpacer | btns | titleSpacer | bts | rightTitleSpacer |
    // |_________________|______|_____________|_____|__________________|
    // |                         decoSpacer                            |
    // |_______________________________________________________________|
    // | |                                                           | |
    // | |                      contentsFake                         | |
    // | |                                                           | |
    // |leftSpacer                                          rightSpacer|
    // |_|___________________________________________________________|_|
    // |                           bottomSpacer                        |
    // |_______________________________________________________________|
    //

    if (!PlastikHandler::initialized()) return;

    delete mainLayout_;

    delete topSpacer_;
    delete titleSpacer_;
    delete leftTitleSpacer_;
    delete rightTitleSpacer_;
    delete decoSpacer_;
    delete leftSpacer_;
    delete rightSpacer_;
    delete bottomSpacer_;

    mainLayout_ = new QVBoxLayout(widget(), 0, 0);

    topSpacer_        = new QSpacerItem(1, TOPMARGIN, QSizePolicy::Expanding, QSizePolicy::Fixed);
    titleSpacer_      = new QSpacerItem(1, s_titleHeight,
                                        QSizePolicy::Expanding, QSizePolicy::Fixed);
    leftTitleSpacer_  = new QSpacerItem(SIDETITLEMARGIN, s_titleHeight,
                                        QSizePolicy::Fixed, QSizePolicy::Fixed);
    rightTitleSpacer_ = new QSpacerItem(SIDETITLEMARGIN, s_titleHeight,
                                        QSizePolicy::Fixed, QSizePolicy::Fixed);
    decoSpacer_       = new QSpacerItem(1, DECOHEIGHT, QSizePolicy::Expanding, QSizePolicy::Fixed);
    leftSpacer_       = new QSpacerItem(PlastikHandler::borderSize(), 1,
                                        QSizePolicy::Fixed, QSizePolicy::Expanding);
    rightSpacer_      = new QSpacerItem(PlastikHandler::borderSize(), 1,
                                        QSizePolicy::Fixed, QSizePolicy::Expanding);
    bottomSpacer_     = new QSpacerItem(1, PlastikHandler::borderSize(),
                                        QSizePolicy::Expanding, QSizePolicy::Fixed);

    // top
    mainLayout_->addItem(topSpacer_);

    // title
    QHBoxLayout *titleLayout_ = new QHBoxLayout(mainLayout_, 0, 0);

    // sizeof(...) is calculated at compile time
    memset(m_button, 0, sizeof(PlastikButton *) * NumButtons);

    titleLayout_->addItem(PlastikHandler::reverseLayout()?rightTitleSpacer_:leftTitleSpacer_);
    addButtons(titleLayout_,
               options()->customButtonPositions() ? options()->titleButtonsLeft() : QString(default_left),
               s_titleHeight-1);
    titleLayout_->addItem(titleSpacer_);
    addButtons(titleLayout_,
               options()->customButtonPositions() ? options()->titleButtonsRight() : QString(default_right),
               s_titleHeight-1);
    titleLayout_->addItem(PlastikHandler::reverseLayout()?leftTitleSpacer_:rightTitleSpacer_);

    // deco
    mainLayout_->addItem(decoSpacer_);

    //Mid
    QHBoxLayout * midLayout   = new QHBoxLayout(mainLayout_, 0, 0);
    midLayout->addItem(PlastikHandler::reverseLayout()?rightSpacer_:leftSpacer_);
    if( isPreview())
        midLayout->addWidget(new QLabel( i18n( "<center><b>Plastik</b></center>" ), widget()) );
    else
        midLayout->addItem( new QSpacerItem( 0, 0 ));
    midLayout->addItem(PlastikHandler::reverseLayout()?leftSpacer_:rightSpacer_);

    //Bottom
    mainLayout_->addItem(bottomSpacer_);


}

void PlastikClient::addButtons(QBoxLayout *layout, const QString& s, int buttonSize)
{
    if (s.length() > 0) {
        for (unsigned n=0; n < s.length(); n++) {
            switch (s[n]) {
              case 'M': // Menu button
                  if (!m_button[MenuButton]){
                      m_button[MenuButton] = new PlastikButton(this, "menu", i18n("Menu"), MenuButton, buttonSize);
                      connect(m_button[MenuButton], SIGNAL(pressed()), SLOT(menuButtonPressed()));
                      connect(m_button[MenuButton], SIGNAL(released()), this, SLOT(menuButtonReleased()));
                      layout->addWidget(m_button[MenuButton], 0, Qt::AlignHCenter | Qt::AlignTop);
                  }
                  break;
              case 'S': // OnAllDesktops button
                  if (!m_button[OnAllDesktopsButton]){
                      m_button[OnAllDesktopsButton] = new PlastikButton(this, "on_all_desktops", i18n("On All Desktops"), OnAllDesktopsButton, buttonSize);
                      connect(m_button[OnAllDesktopsButton], SIGNAL(clicked()), SLOT(toggleOnAllDesktops()));
                      layout->addWidget(m_button[OnAllDesktopsButton], 0, Qt::AlignHCenter | Qt::AlignTop);
                  }
                  break;
              case 'H': // Help button
                  if ((!m_button[HelpButton]) && providesContextHelp()){
                      m_button[HelpButton] = new PlastikButton(this, "help", i18n("Help"), HelpButton, buttonSize);
                      connect(m_button[HelpButton], SIGNAL(clicked()), SLOT(showContextHelp()));
                      layout->addWidget(m_button[HelpButton], 0, Qt::AlignHCenter | Qt::AlignTop);
                  }
                  break;
              case 'I': // Minimize button
                  if ((!m_button[MinButton]) && isMinimizable()){
                      m_button[MinButton] = new PlastikButton(this, "minimize", i18n("Minimize"), MinButton, buttonSize);
                      connect(m_button[MinButton], SIGNAL(clicked()), SLOT(minimize()));
                      layout->addWidget(m_button[MinButton], 0, Qt::AlignHCenter | Qt::AlignTop);
                  }
                  break;
              case 'A': // Maximize button
                  if ((!m_button[MaxButton]) && isMaximizable()){
                      m_button[MaxButton] = new PlastikButton(this, "maximize", i18n("Maximize"), MaxButton, buttonSize);
                      connect(m_button[MaxButton], SIGNAL(clicked()), SLOT(slotMaximize()));
                      layout->addWidget(m_button[MaxButton], 0, Qt::AlignHCenter | Qt::AlignTop);
                  }
                  break;
              case 'X': // Close button
                  if ((!m_button[CloseButton]) && isCloseable()){
                      m_button[CloseButton] = new PlastikButton(this, "close", i18n("Close"), CloseButton, buttonSize);
                      connect(m_button[CloseButton], SIGNAL(clicked()), SLOT(closeWindow()));
                      layout->addWidget(m_button[CloseButton], 0, Qt::AlignHCenter | Qt::AlignTop);
                  }
                  break;
              case '_': // Spacer item
                  layout->addSpacing(3); // add a 3 px spacing...
            }

             // add 2 px spacing between buttons
            if(n < (s.length()-1) ) // and only between them!
                layout->addSpacing (1);
        }
    }
}

void PlastikClient::captionChange()
{
    captionBufferDirty = true;
    widget()->update(titleSpacer_->geometry());
}

void PlastikClient::reset( unsigned long changed )
{
    if (changed & SettingColors)
    {
        // repaint the whole thing
        delete_pixmaps();
        create_pixmaps();
        captionBufferDirty = true;
        widget()->update();
        for (int n=0; n<NumButtons; n++) {
            if (m_button[n]) m_button[n]->update();
        }
    } else if (changed & SettingFont) {
        // font has changed -- update title height and font
        s_titleHeight = isTool() ?
                PlastikHandler::titleHeightTool()
                : PlastikHandler::titleHeight();
        s_titleFont = isTool() ?
                PlastikHandler::titleFontTool()
                : PlastikHandler::titleFont();
        // update buttons
        for (int n=0; n<NumButtons; n++) {
            if (m_button[n]) m_button[n]->setSize(s_titleHeight-1);
        }
        // update the spacer
        titleSpacer_->changeSize(1, s_titleHeight,
                QSizePolicy::Expanding, QSizePolicy::Fixed);
        // then repaint
        delete_pixmaps();
        create_pixmaps();
        captionBufferDirty = true;
        widget()->update();
    }
}

PlastikClient::MousePosition PlastikClient::mousePosition(const QPoint &point) const
{
    const int corner = 18+3*PlastikHandler::borderSize()/2;
    MousePosition pos = Center;

    // often needed coords..
    QRect topRect(topSpacer_->geometry());
    QRect decoRect(decoSpacer_->geometry());
    QRect leftRect(leftSpacer_->geometry());
    QRect leftTitleRect(leftTitleSpacer_->geometry());
    QRect rightRect(rightSpacer_->geometry());
    QRect rightTitleRect(rightTitleSpacer_->geometry());
    QRect bottomRect(bottomSpacer_->geometry());

    if(bottomRect.contains(point)) {
        if      (point.x() <= bottomRect.left()+corner)  pos = KDecorationDefines::BottomLeft2;
        else if (point.x() >= bottomRect.right()-corner) pos = KDecorationDefines::BottomRight2;
        else                                             pos = KDecorationDefines::Bottom;
    }
    else if(leftRect.contains(point)) {
        if      (point.y() <= topRect.top()+corner)       pos = KDecorationDefines::TopLeft2;
        else if (point.y() >= bottomRect.bottom()-corner) pos = KDecorationDefines::BottomLeft2;
        else                                              pos = KDecorationDefines::Left;
    }
    else if(leftTitleRect.contains(point)) {
        if      (point.y() <= topRect.top()+corner)       pos = KDecorationDefines::TopLeft2;
        else                                              pos = KDecorationDefines::Left;
    }
    else if(rightRect.contains(point)) {
        if      (point.y() <= topRect.top()+corner)       pos = KDecorationDefines::TopRight2;
        else if (point.y() >= bottomRect.bottom()-corner) pos = KDecorationDefines::BottomRight2;
        else                                              pos = KDecorationDefines::Right;
    }
    else if(rightTitleRect.contains(point)) {
        if      (point.y() <= topRect.top()+corner)       pos = KDecorationDefines::TopRight2;
        else                                              pos = KDecorationDefines::Right;
    }
    else if(topRect.contains(point)) {
        if      (point.x() <= topRect.left()+corner)      pos = KDecorationDefines::TopLeft2;
        else if (point.x() >= topRect.right()-corner)     pos = KDecorationDefines::TopRight2;
        else                                              pos = KDecorationDefines::Top;
    }
    else if(decoRect.contains(point)) {
        if(point.x() <= leftTitleRect.right()) {
            if(point.y() <= topRect.top()+corner)         pos = KDecorationDefines::TopLeft2;
            else                                          pos = KDecorationDefines::Left;
        }
        else if(point.x() >= rightTitleRect.left()) {
            if(point.y() <= topRect.top()+corner)         pos = KDecorationDefines::TopRight2;
            else                                          pos = KDecorationDefines::Right;
        }
    }

    return pos;
}

void PlastikClient::iconChange()
{
    if (m_button[MenuButton])
    {
        m_button[MenuButton]->update();
    }
}

void PlastikClient::activeChange()
{
    for (int n=0; n<NumButtons; n++)
        if (m_button[n]) m_button[n]->update();
    widget()->update();
}

void PlastikClient::maximizeChange()
{
    if (!PlastikHandler::initialized()) return;


}

void PlastikClient::desktopChange()
{
    if ( m_button[OnAllDesktopsButton] ) {
        m_button[OnAllDesktopsButton]->setOnAllDesktops( isOnAllDesktops() );
        m_button[OnAllDesktopsButton]->setTipText( isOnAllDesktops() ?
                i18n("Not On All Desktops")
                : i18n("On All Desktops"));
        m_button[OnAllDesktopsButton]->setDeco(); // update icon/deco
    }
}

void PlastikClient::slotMaximize()
{
    if (m_button[MaxButton])
    {
        switch (m_button[MaxButton]->lastMousePress())
        {
          case MidButton:
              maximize(maximizeMode() ^ MaximizeVertical );
              maximizeChange();
              break;
          case RightButton:
              maximize(maximizeMode() ^ MaximizeHorizontal );
              maximizeChange();
              break;
          default:
              maximize(maximizeMode() == MaximizeFull ? MaximizeRestore : MaximizeFull );
              maximizeChange();
        }

        m_button[MaxButton]->setMaximized( maximizeMode()!=MaximizeRestore);
        m_button[MaxButton]->setTipText( (maximizeMode()==MaximizeRestore) ?
                i18n("Maximize")
                : i18n("Restore"));
        m_button[MaxButton]->setDeco(); // update the button icon...
        doShape();
    }
}

void PlastikClient::menuButtonPressed()
{
    static QTime* t = NULL;
    static PlastikClient* lastClient = NULL;
    if (t == NULL)
        t = new QTime;
    bool dbl = (lastClient==this && t->elapsed() <= QApplication::doubleClickInterval());
    lastClient = this;
    t->start();
    if (!dbl || !PlastikHandler::menuClose()) {
        QPoint pos = m_button[MenuButton]->mapToGlobal(m_button[MenuButton]->rect().bottomLeft() );
        KDecorationFactory* f = factory();
        showWindowMenu( pos );
        if( !f->exists( this )) // 'this' was deleted
            return;
        m_button[MenuButton]->setDown(false);
    }
    else
        closing = true;
}

void PlastikClient::menuButtonReleased()
{
    if(closing)
        closeWindow();
}

void PlastikClient::create_pixmaps()
{
    if(pixmaps_created)
        return;

    KPixmap tempPixmap;
    QPainter painter;

    // aTitleBarTopTile
    tempPixmap.resize(1, TOPMARGIN-1-1 );
    KPixmapEffect::gradient(tempPixmap,
                            PlastikHandler::getColor(TitleGradientToTop, true),
                            PlastikHandler::getColor(TitleGradientFrom, true),
                            KPixmapEffect::VerticalGradient);
    aTitleBarTopTile = new QPixmap(1, TOPMARGIN-1-1 );
    painter.begin(aTitleBarTopTile);
    painter.drawPixmap(0, 0, tempPixmap);
    painter.end();

    // aTitleBarTile
    tempPixmap.resize(1, s_titleHeight );
    KPixmapEffect::gradient(tempPixmap,
                            PlastikHandler::getColor(TitleGradientFrom, true),
                            PlastikHandler::getColor(TitleGradientTo, true),
                            KPixmapEffect::VerticalGradient);
    aTitleBarTile = new QPixmap(1, s_titleHeight );
    painter.begin(aTitleBarTile);
    painter.drawPixmap(0, 0, tempPixmap);
    painter.end();

    // iTitleBarTopTile
    tempPixmap.resize(1, TOPMARGIN-1-1 );
    KPixmapEffect::gradient(tempPixmap,
                            PlastikHandler::getColor(TitleGradientToTop, false),
                            PlastikHandler::getColor(TitleGradientFrom, false),
                            KPixmapEffect::VerticalGradient);
    iTitleBarTopTile = new QPixmap(1, TOPMARGIN-1-1 );
    painter.begin(iTitleBarTopTile);
    painter.drawPixmap(0, 0, tempPixmap);
    painter.end();

    // iTitleBarTile
    tempPixmap.resize(1, s_titleHeight );
    KPixmapEffect::gradient(tempPixmap,
                            PlastikHandler::getColor(TitleGradientFrom, false),
                            PlastikHandler::getColor(TitleGradientTo, false),
                            KPixmapEffect::VerticalGradient);
    iTitleBarTile = new QPixmap(1, s_titleHeight );
    painter.begin(iTitleBarTile);
    painter.drawPixmap(0, 0, tempPixmap);
    painter.end();

    pixmaps_created = true;
}

void PlastikClient::delete_pixmaps()
{
    delete aTitleBarTopTile;
    aTitleBarTopTile = 0;

    delete iTitleBarTopTile;
    iTitleBarTopTile = 0;

    delete aTitleBarTile;
    aTitleBarTile = 0;

    delete iTitleBarTile;
    iTitleBarTile = 0;

    pixmaps_created = false;
}

void PlastikClient::update_captionBuffer()
{
    if (!PlastikHandler::initialized()) return;

    const uint maxCaptionLength = 300; // truncate captions longer than this!
    QString c(caption() );
    if (c.length() > maxCaptionLength) {
        c.truncate(maxCaptionLength);
        c.append(" [...]");
    }

    QFontMetrics fm(s_titleFont);
    int captionWidth  = fm.width(c);

    QPixmap textPixmap;
    QPainter painter;
    if(PlastikHandler::titleShadow())
    {
        // prepare the shadow
        textPixmap = QPixmap(captionWidth+2*2, s_titleHeight ); // 2*2 px shadow space
        textPixmap.fill(QColor(0,0,0));
        textPixmap.setMask( textPixmap.createHeuristicMask(TRUE) );
        painter.begin(&textPixmap);
        painter.setFont(s_titleFont);
        painter.setPen(white);
        painter.drawText(textPixmap.rect(), AlignCenter, c );
        painter.end();
    }

    QImage shadow;
    ShadowEngine se;

    // active
    aCaptionBuffer->resize(captionWidth+4, s_titleHeight ); // 4 px shadow
    painter.begin(aCaptionBuffer);
    painter.drawTiledPixmap(aCaptionBuffer->rect(), *aTitleBarTile);
    if(PlastikHandler::titleShadow())
    {
        shadow = se.makeShadow(textPixmap, QColor(0, 0, 0));
        painter.drawImage(1, 1, shadow);
    }
    painter.setFont(s_titleFont);
    painter.setPen(PlastikHandler::getColor(TitleFont,true));
    painter.drawText(aCaptionBuffer->rect(), AlignCenter, c );
    painter.end();


    // inactive
    iCaptionBuffer->resize(captionWidth+4, s_titleHeight );
    painter.begin(iCaptionBuffer);
    painter.drawTiledPixmap(iCaptionBuffer->rect(), *iTitleBarTile);
    if(PlastikHandler::titleShadow())
    {
        painter.drawImage(1, 1, shadow);
    }
    painter.setFont(s_titleFont);
    painter.setPen(PlastikHandler::getColor(TitleFont,false));
    painter.drawText(iCaptionBuffer->rect(), AlignCenter, c );
    painter.end();

    captionBufferDirty = false;
}

void PlastikClient::borders( int& left, int& right, int& top, int& bottom ) const
{
    if ((maximizeMode()==MaximizeFull) && !options()->moveResizeMaximizedWindows()) {
        left = right = bottom = 0;
        top = s_titleHeight + DECOHEIGHT;

        // update layout etc.
        topSpacer_->changeSize(1, 0, QSizePolicy::Expanding, QSizePolicy::Fixed);
        leftSpacer_->changeSize(left, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
        leftTitleSpacer_->changeSize(left, s_titleHeight, QSizePolicy::Fixed, QSizePolicy::Fixed);
        rightSpacer_->changeSize(right, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
        rightTitleSpacer_->changeSize(right, s_titleHeight, QSizePolicy::Fixed, QSizePolicy::Fixed);
        bottomSpacer_->changeSize(1, bottom, QSizePolicy::Expanding, QSizePolicy::Fixed);
    } else {
        left = right = bottom = PlastikHandler::borderSize();
        top = s_titleHeight + DECOHEIGHT + TOPMARGIN;

        // update layout etc.
        topSpacer_->changeSize(1, TOPMARGIN, QSizePolicy::Expanding, QSizePolicy::Fixed);
        leftSpacer_->changeSize(left, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
        leftTitleSpacer_->changeSize(SIDETITLEMARGIN, s_titleHeight,
                QSizePolicy::Fixed, QSizePolicy::Fixed);
        rightSpacer_->changeSize(right, 1, QSizePolicy::Fixed, QSizePolicy::Expanding);
        rightTitleSpacer_->changeSize(SIDETITLEMARGIN, s_titleHeight,
                QSizePolicy::Fixed, QSizePolicy::Fixed);
        bottomSpacer_->changeSize(1, bottom, QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    // activate updated layout
    widget()->layout()->activate();
}

QSize PlastikClient::minimumSize() const
{
    return widget()->minimumSize();
}

void PlastikClient::show()
{
    widget()->show();
}

void PlastikClient::resize( const QSize& s )
{
    widget()->resize( s );
}

bool PlastikClient::eventFilter( QObject* o, QEvent* e )
{
    if( o != widget())
    return false;
    switch( e->type())
    {
    case QEvent::Resize:
        resizeEvent();
        return true;
    case QEvent::Paint:
        paintEvent( static_cast< QPaintEvent* >( e ));
        return true;
    case QEvent::MouseButtonDblClick:
        mouseDoubleClickEvent( static_cast< QMouseEvent* >( e ));
        return true;
    case QEvent::MouseButtonPress:
        processMousePressEvent( static_cast< QMouseEvent* >( e ));
        return true;
    default:
        break;
    }
    return false;
}

} // KWinPlastik
