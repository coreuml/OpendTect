/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        A.H. Lammertink
 Date:          30/05/2001
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "uitoolbar.h"

#include "uiaction.h"
#include "uimainwin.h"
#include "uiparentbody.h"
#include "uistrings.h"
#include "uitoolbutton.h"

#include "bufstringset.h"
#include "menuhandler.h"
#include "separstr.h"
#include "hiddenparam.h"

#include <QToolBar>
#include <QToolButton>
#include "i_qtoolbar.h"

mUseQtnamespace

static HiddenParam<uiToolBar,uiString*> dispnm_(0);


ObjectSet<uiToolBar>& uiToolBar::toolBars()
{
    mDefineStaticLocalObject( ObjectSet<uiToolBar>, ret, );
    return ret;
}



uiToolBar::uiToolBar( uiParent* parnt, const uiString& nm, ToolBarArea tba,
		      bool newline )
    : uiParent(nm.getFullString(),0)
    , parent_(parnt)
    , tbarea_(tba)
    , buttonClicked(this)
    , orientationChanged(this)
    , toolbarmenuaction_(0)
    , qtoolbar_(new QToolBar(nm.getQString(), parnt ? parnt->getWidget() : 0))
{
    dispnm_.setParam( this, new uiString(nm) );
    qtoolbar_->setObjectName( nm.getQString() );
    msgr_ = new i_ToolBarMessenger( qtoolbar_, this );

    mDynamicCastGet(uiMainWin*,uimw,parnt)
    if ( uimw )
    {
	if ( newline ) uimw->addToolBarBreak();
	uimw->addToolBar( this );
    }

    toolBars() += this;
}


uiToolBar::~uiToolBar()
{
    removeAllActions();
    deepErase( addedobjects_ );

    delete qtoolbar_;
    delete msgr_;
    dispnm_.removeAndDeleteParam(this);
    toolBars() -= this;
}


uiString uiToolBar::getDispNm()
{
    return *dispnm_.getParam(this);
}

int uiToolBar::addButton( const char* fnm, const uiString& tt,
			  const CallBack& cb, bool toggle, int id )
{
    uiAction* action = new uiAction( uiStrings::sEmptyString(), cb, fnm );
    action->setToolTip( tt );
    action->setCheckable( toggle );
    return insertAction( action, id );
}


int uiToolBar::addButton( const uiToolButtonSetup& su )
{
    uiAction* action = new uiAction( su.name_, su.cb_, su.icid_ );
    action->setToolTip( su.tooltip_ );
    action->setCheckable( su.istoggle_ );
    action->setShortcut( su.shortcut_ );
    action->setChecked( su.ison_ );
    if ( su.arrowtype_!=uiToolButton::NoArrow )
    {
	pErrMsg("Not implemented yet");
    }

    return insertAction( action );
}


int uiToolBar::addButton( const MenuItem& itm )
{
    return insertAction( new uiAction(itm), itm.id );
}


void uiToolBar::addButton( uiButton* button )
{
    addObject( button );
}


void uiToolBar::addObject( uiObject* obj )
{
    QWidget* qw = obj && obj->body() ? obj->body()->qwidget() : 0;
    if ( qw )
    {
	qtoolbar_->addWidget( qw );
	mDynamicCastGet(uiToolButton*,button,obj)
	if ( !button ) qw->setMaximumHeight( uiObject::iconSize() );
	addedobjects_ += obj;
    }
    else
    {
	pErrMsg("Not a valid object");
    }
}


void uiToolBar::setLabel( const uiString& lbl )
{
    label_ = lbl;
    qtoolbar_->setWindowTitle( lbl.getQString() );
    setName( lbl.getFullString() );
}

#define mGetAction( conststatement, erraction ) \
    conststatement uiAction* action = \
			const_cast<uiToolBar*>(this)->findAction( id ); \
    if ( !action ) \
    { \
	pErrMsg("Action not found"); \
	erraction; \
    }


void uiToolBar::turnOn( int id, bool yn )
{
    mGetAction( ,return );
    action->setChecked( yn );
}

bool uiToolBar::isOn( int id ) const
{
    mGetAction( const, return false );
    return action->isChecked();
}


void uiToolBar::setSensitive( int id, bool yn )
{
    mGetAction( , return );

    action->setEnabled( yn );
}


void uiToolBar::setSensitive( bool yn )
{
    qtoolbar_->setEnabled( yn );
}

bool uiToolBar::isSensitive() const
{ return qtoolbar_->isEnabled(); }


void uiToolBar::setToolTip( int id, const uiString& tip )
{
    mGetAction( , return );
    action->setToolTip( tip );
}

void uiToolBar::setShortcut( int id, const char* sc )
{
    mGetAction( , return );
    action->setShortcut( sc );
}


void uiToolBar::setToggle( int id, bool yn )
{
    mGetAction( , return );
    action->setCheckable( yn );
}


void uiToolBar::setIcon( int id, const char* fnm )
{
    mGetAction( , return );
    action->setIcon( fnm );
}


void uiToolBar::setIcon( int id, const uiIcon& icon )
{
    mGetAction( , return );
    action->setIcon( icon );
}


void uiToolBar::setButtonMenu( int id, uiMenu* mnu,
			       uiToolButton::PopupMode mode )
{
    mGetAction( , return );
    action->setMenu( mnu );
    QWidget* qw = qtoolbar_->widgetForAction( action->qaction() );
    mDynamicCastGet(QToolButton*,qtb,qw)
    if ( qtb )
	qtb->setPopupMode( (QToolButton::ToolButtonPopupMode)mode );
}


void uiToolBar::display( bool yn, bool, bool )
{
    if ( yn )
	qtoolbar_->show();
    else
	qtoolbar_->hide();

    if ( toolbarmenuaction_ )
	toolbarmenuaction_->setChecked( yn );
}


bool uiToolBar::isHidden() const
{ return qtoolbar_->isHidden(); }


bool uiToolBar::isVisible() const
{ return qtoolbar_->isVisible(); }


OD::Orientation uiToolBar::getOrientation() const
{
    return qtoolbar_->orientation()==Qt::Horizontal ?
		OD::Horizontal : OD::Vertical;
}


void uiToolBar::setToolBarMenuAction( uiAction* action )
{
    toolbarmenuaction_ = action;
    if ( toolbarmenuaction_ )
	toolbarmenuaction_->setChecked( !isHidden() );
}


void uiToolBar::handleFinalise( bool pre )
{
    if ( pre )
	preFinalise().trigger();

    for ( int idx=0; idx<addedobjects_.size(); idx++ )
    {
	uiObject& uiobj = *addedobjects_[idx];
	if ( pre )
	    uiobj.preFinalise().trigger();
	else
	    uiobj.finalise();
    }

    if ( !pre )
	postFinalise().trigger();
}


void uiToolBar::clear()
{
    removeAllActions();
    deepErase( addedobjects_ );
}


void uiToolBar::translateText()
{
    if ( !label_.isEmpty() )
	qtoolbar_->setWindowTitle( label_.getQString() );

    for ( int idx=0; idx<addedobjects_.size(); idx++ )
	addedobjects_[idx]->translateText();
}


void uiToolBar::doInsertMenu(mQtclass(QMenu)* menu,
			  mQtclass(QAction)* before)
{
    pErrMsg("Not implemented. Should not be called");
}


void uiToolBar::doInsertAction(mQtclass(QAction)* action,
			       mQtclass(QAction)* before)
{
    qtoolbar_->insertAction( before, action );
}


void uiToolBar::doInsertSeparator(QAction* before)
{
    qtoolbar_->insertSeparator( before );
}


void uiToolBar::doClear()
{
    qtoolbar_->clear();
}


void uiToolBar::doRemoveAction( mQtclass(QAction)* action )
{
    qtoolbar_->removeAction( action );
}


void uiToolBar::getEntityList( ObjectSet<const CallBacker>& entities ) const
{
    entities.erase();

    for ( int actidx=0; actidx<qtoolbar_->actions().size(); actidx++ )
    {
	QAction* qaction = qtoolbar_->actions()[actidx];
	const int id = getID( qaction );
	const uiAction* action = const_cast<uiToolBar*>(this)->findAction( id );

	if ( !action )
	{
	    const QWidget* qw = qtoolbar_->widgetForAction( qaction );
	    for ( int objidx=0; objidx<addedobjects_.size(); objidx++ )
	    {
		if ( qw==addedobjects_[objidx]->qwidget() )
		{
		    entities += addedobjects_[objidx];
		    break;
		}
	    }
	}
	else
	    entities += action;
    }
}
