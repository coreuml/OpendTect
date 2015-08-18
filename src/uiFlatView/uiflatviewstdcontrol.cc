/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Mar 2007
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "uiflatviewstdcontrol.h"

#include "uicolortable.h"
#include "uiflatviewcoltabed.h"
#include "uiflatviewer.h"
#include "uigraphicsscene.h"
#include "uigeninput.h"
#include "uimainwin.h"
#include "uimain.h"
#include "uimenu.h"
#include "uimenuhandler.h"
#include "uirgbarraycanvas.h"
#include "uistrings.h"
#include "uitoolbar.h"
#include "uitoolbutton.h"

#include "keyboardevent.h"
#include "mousecursor.h"
#include "mouseevent.h"
#include "settings.h"
#include "texttranslator.h"

#define mDefBut(but,fnm,cbnm,tt) \
    but = new uiToolButton(tb_,fnm,tt,mCB(this,uiFlatViewStdControl,cbnm) ); \
    tb_->addButton( but );

#define sLocalHZIdx	0
#define sGlobalHZIdx	1
#define sManHZIdx	2

static const char* sKeyVW2DTrcsPerCM()	{ return "Viewer2D.TrcsPerCM"; }
static const char* sKeyVW2DZPerCM()	{ return "Viewer2D.ZSamplesPerCM"; }

uiFlatViewZoomLevelDlg::uiFlatViewZoomLevelDlg( uiParent* p,
			float& x1pospercm, float& x2pospercm, bool isvertical )
    : uiDialog(p,uiDialog::Setup(tr("Set zoom level"),uiString::emptyString(),
				 mNoHelpKey))
    , x1pospercm_(x1pospercm)
    , x2pospercm_(x2pospercm)
    , x2fld_(0)
{
    x1fld_ = new uiGenInput( this, tr("Traces per cm"), FloatInpSpec() );
    x1fld_->setValue( x1pospercm_ );
    if ( isvertical )
    {
	x2fld_ = new uiGenInput( this, tr("Z Samples per cm"), FloatInpSpec() );
	x2fld_->setValue( x2pospercm_ );
	x2fld_->attach( alignedBelow, x1fld_ );
    }

    saveglobalfld_ = new uiCheckBox( this, tr( "Save globally" ) );
    saveglobalfld_->attach( alignedBelow, isvertical ? x2fld_ : x1fld_ );
}


bool uiFlatViewZoomLevelDlg::acceptOK( CallBacker* )
{
    x1pospercm_ = x1fld_->getFValue();
    x2pospercm_ = x2fld_ ? x2fld_->getFValue() : x1pospercm_;
    if ( saveglobalfld_->isChecked() )
	uiFlatViewStdControl::setGlobalZoomLevel(
		x1pospercm_, x2pospercm_, x2fld_ );
    return true;
}


uiFlatViewStdControl::uiFlatViewStdControl( uiFlatViewer& vwr,
					    const Setup& setup )
    : uiFlatViewControl(vwr,setup.parent_,setup.withrubber_)
    , vwr_(vwr)
    , setup_(setup)
    , ctabed_(0)
    , mousepressed_(false)
    , menu_(*new uiMenuHandler(0,-1))
    , propertiesmnuitem_(tr("Properties..."),100)
    , defx1pospercm_(mUdf(float))
    , defx2pospercm_(mUdf(float))
    , editbut_(0)
    , rubbandzoombut_(0)
    , zoominbut_(0)
    , zoomoutbut_(0)
    , vertzoominbut_(0)
    , vertzoomoutbut_(0)
    , cancelzoombut_(0)
    , sethomezoombut_(0)
    , gotohomezoombut_(0)
{
    if ( setup_.withfixedaspectratio_ )
	getGlobalZoomLevel( defx1pospercm_, defx2pospercm_, setup.isvertical_ );

    uiToolBar::ToolBarArea tba( setup.withcoltabed_ ? uiToolBar::Left
						    : uiToolBar::Top );
    if ( setup.tba_ > 0 )
	tba = (uiToolBar::ToolBarArea)setup.tba_;
    tb_ = new uiToolBar( mainwin(), tr("Flat Viewer Tools"), tba );

    if ( setup.withzoombut_ || setup.isvertical_ )
    {
	mDefBut(rubbandzoombut_,"rubbandzoom",dragModeCB,tr("Rubberband zoom"));
	rubbandzoombut_->setToggleButton( true );
    }

    if ( setup.withzoombut_ )
    {
	mDefBut(zoominbut_,"zoomforward",zoomCB,tr("Zoom in"));
	mDefBut(zoomoutbut_,"zoombackward",zoomCB,tr("Zoom out"));
    }

    if ( setup.isvertical_ )
    {
	mDefBut(vertzoominbut_,"vertzoomin",zoomCB,tr("Vertical zoom in"));
	mDefBut(vertzoomoutbut_,"vertzoomout",zoomCB,tr("Vertical zoom out"));
    }

    if ( setup.withzoombut_ || setup.isvertical_ )
    {
	mDefBut(cancelzoombut_,"cancelzoom",cancelZoomCB,tr("Cancel zoom"));
    }

    if ( setup.withhomebutton_ )
    {
	mDefBut(sethomezoombut_,"set_homezoom",homeZoomOptSelCB,
		tr("Set home zoom"));
	const CallBack optcb = mCB(this,uiFlatViewStdControl,homeZoomOptSelCB);
	uiMenu* mnu = new uiMenu( tb_, tr("Zoom level options") );
	mnu->insertAction( new uiAction(tr("Set local home zoom"),
					   optcb,"set_homezoom"), sLocalHZIdx );
	mnu->insertAction( new uiAction(tr("Set global home zoom"),
					optcb,"set_ghomezoom"), sGlobalHZIdx );
	mnu->insertAction( new uiAction(tr("Manually set home zoom"),
					optcb,"man_homezoom"), sManHZIdx );
	sethomezoombut_->setMenu( mnu );
	mDefBut(gotohomezoombut_,"homezoom",gotoHomeZoomCB,
		tr("Go to home zoom"));
	gotohomezoombut_->setSensitive( !mIsUdf(defx1pospercm_) &&
					!mIsUdf(defx2pospercm_) );
    }

    if ( setup.withflip_ )
    {
	uiToolButton* mDefBut(fliplrbut,"flip_lr",flipCB,tr("Flip left/right"));
    }

    if ( setup.withsnapshot_ )
    {
	vwr_.rgbCanvas().enableImageSave();
	tb_->addObject( vwr_.rgbCanvas().getSaveImageButton(tb_) );
	tb_->addObject( vwr_.rgbCanvas().getPrintImageButton(tb_) );
    }

    tb_->addSeparator();
    mDefBut(parsbut_,"2ddisppars",parsCB,tr("Set display parameters"));

    if ( setup.withcoltabed_ )
    {
	uiColorTableToolBar* coltabtb = new uiColorTableToolBar( mainwin() );
	ctabed_ = new uiFlatViewColTabEd( *coltabtb );
	coltabtb->display( vwr.rgbCanvas().prefHNrPics()>=400 );
	if ( setup.managecoltab_ )
	{
	    mAttachCB( ctabed_->colTabChgd, uiFlatViewStdControl::coltabChg );
	    mAttachCB( vwr.dispParsChanged, uiFlatViewStdControl::dispChgCB );
	}
    }

    if ( !setup.helpkey_.isEmpty() )
    {
	uiToolButton* mDefBut(helpbut,"contexthelp",helpCB,uiStrings::sHelp());
	helpkey_ = setup.helpkey_;
    }

    if ( setup.withedit_ )
    {
	mDefBut(editbut_,"seedpickmode",dragModeCB,tr("Edit mode"));
	editbut_->setToggleButton( true );
    }

    menu_.ref();
    mAttachCB( menu_.createnotifier, uiFlatViewStdControl::createMenuCB );
    mAttachCB( menu_.handlenotifier, uiFlatViewStdControl::handleMenuCB );
    mAttachCB( zoomChanged, uiFlatViewStdControl::zoomChgCB );
    mAttachCB( rubberBandUsed, uiFlatViewStdControl::rubBandUsedCB );
}


uiFlatViewStdControl::~uiFlatViewStdControl()
{
    detachAllNotifiers();
    menu_.unRef();
    delete ctabed_; ctabed_ = 0;
}


void uiFlatViewStdControl::finalPrepare()
{
    for ( int idx=0; idx<vwrs_.size(); idx++ )
    {
	uiGraphicsView& view = vwrs_[idx]->rgbCanvas();
	if ( setup_.withfixedaspectratio_ )
	{
	    vwrs_[idx]->updateBitmapsOnResize( false );
	    mAttachCBIfNotAttached(
		    view.reSize, uiFlatViewStdControl::aspectRatioCB );
	}

	MouseEventHandler& mevh = view.getNavigationMouseEventHandler();
	mAttachCBIfNotAttached(
		mevh.wheelMove, uiFlatViewStdControl::wheelMoveCB );

	if ( setup_.withhanddrag_ )
	{
	    mAttachCBIfNotAttached(
		    mevh.buttonPressed, uiFlatViewStdControl::handDragStarted );
	    mAttachCBIfNotAttached(
		    mevh.buttonReleased, uiFlatViewStdControl::handDragged );
	    mAttachCBIfNotAttached(
		    mevh.movement, uiFlatViewStdControl::handDragging );
	}

	mAttachCBIfNotAttached( view.gestureEventHandler().pinchnotifier,
				uiFlatViewStdControl::pinchZoomCB );
    }

    updatePosButtonStates();
}


void uiFlatViewStdControl::clearToolBar()
{
    delete mainwin()->removeToolBar( tb_ );
    tb_ = 0;
    zoominbut_ = zoomoutbut_ = rubbandzoombut_ = parsbut_ = editbut_ = 0;
    vertzoominbut_ = vertzoomoutbut_ = cancelzoombut_ = 0;
}


void uiFlatViewStdControl::updatePosButtonStates()
{
    const bool yn = !zoommgr_.atStart();
    if ( zoomoutbut_ ) zoomoutbut_->setSensitive( yn );
    if ( vertzoomoutbut_ ) vertzoomoutbut_->setSensitive( yn );
    if ( cancelzoombut_ ) cancelzoombut_->setSensitive( yn );
}


void uiFlatViewStdControl::dispChgCB( CallBacker* )
{
    if ( ctabed_ ) ctabed_->setColTab( vwr_.appearance().ddpars_.vd_ );
}


void uiFlatViewStdControl::zoomChgCB( CallBacker* )
{
    updatePosButtonStates();
}


void uiFlatViewStdControl::rubBandUsedCB( CallBacker* )
{
    if ( rubbandzoombut_ && rubbandzoombut_->isOn() )
    {
	rubbandzoombut_->setOn( false );
	dragModeCB( rubbandzoombut_ );
    }
}


void uiFlatViewStdControl::aspectRatioCB( CallBacker* cb )
{
    mCBCapsuleGet(uiSize,caps,cb);
    mDynamicCastGet(uiGraphicsView*,view,caps->caller);
    int vwridx = -1;
    for ( int idx=0; idx<vwrs_.size(); idx++ )
	if ( &vwrs_[idx]->rgbCanvas() == view )
	    { vwridx = idx; break; }
    if ( vwridx == -1 ) return;

    uiFlatViewer& vwr = *vwrs_[vwridx];
    if ( !view->mainwin()->poppedUp() )
	{ setViewToCustomZoomLevel( vwr ); return; }

    const uiWorldRect bb = vwr.boundingBox();
    const uiWorld2Ui& w2ui = vwr.getWorld2Ui();
    const uiWorldRect wr = w2ui.transform( vwr.getViewRect(false) );
    vwr.setBoundingRect( w2ui.transform(bb) );
    vwr.setView( getZoomOrPanRect(wr.centre(),wr.size(),wr,bb) );
    updateZoomManager();
}


void uiFlatViewStdControl::wheelMoveCB( CallBacker* cb )
{
    mDynamicCastGet( const MouseEventHandler*, meh, cb );
    if ( !meh || !meh->hasEvent() ) return;

    const MouseEvent& ev = meh->event();
    if ( mIsZero(ev.angle(),0.01) )
	return;

    uiToolButton* but = ev.angle()>0 ?
	(ev.shiftStatus() ? vertzoominbut_ : zoominbut_) :
	(ev.shiftStatus() ? vertzoomoutbut_ : zoomoutbut_);

    zoomCB( but );
}


void uiFlatViewStdControl::pinchZoomCB( CallBacker* cb )
{
    mDynamicCastGet(const GestureEventHandler*,evh,cb);
    if ( !evh || evh->isHandled() || vwrs_.isEmpty() )
	return;

    const GestureEvent* gevent = evh->getPinchEventInfo();
    if ( !gevent )
	return;

    uiFlatViewer& vwr = *vwrs_[0];
    const Geom::Size2D<double> cursz = vwr.curView().size();
    const float scalefac = gevent->scale();
    Geom::Size2D<double> newsz( cursz.width() * (1/scalefac),
				cursz.height() * (1/scalefac) );
    Geom::Point2D<double> pos = vwr.getWorld2Ui().transform( gevent->pos() );

    const uiWorldRect wr = getZoomOrPanRect( pos, newsz, vwr.curView(),
					     vwr.boundingBox() );
    vwr.setView( wr );

    if ( gevent->getState() == GestureEvent::Finished )
	updateZoomManager();
}


void uiFlatViewStdControl::zoomCB( CallBacker* but )
{
    if ( !but ) return;
    const bool zoomin = but==zoominbut_ || but==vertzoominbut_;
    const bool onlyvertzoom = but==vertzoominbut_ || but==vertzoomoutbut_;
    doZoom( zoomin, onlyvertzoom, *vwrs_[0] );
}


void uiFlatViewStdControl::doZoom( bool zoomin, bool onlyvertzoom,
				   uiFlatViewer& vwr )
{
    const int vwridx = vwrs_.indexOf( &vwr );
    if ( vwridx < 0 ) return;

    const MouseEventHandler& meh =
	vwr.rgbCanvas().getNavigationMouseEventHandler();
    const bool hasmouseevent = meh.hasEvent();

    if ( zoomin )
	zoommgr_.forward( vwridx, onlyvertzoom, true );
    else
    {
	if ( zoommgr_.atStart(vwridx) )
	    return;
	zoommgr_.back( vwridx, onlyvertzoom, hasmouseevent );
    }

    Geom::Point2D<double> mousepos = hasmouseevent ?
	vwr.getWorld2Ui().transform(meh.event().pos()) : vwr.curView().centre();
    Geom::Size2D<double> newsz = zoommgr_.current( vwridx );

    setNewView( mousepos, newsz );
}


void uiFlatViewStdControl::cancelZoomCB( CallBacker* )
{
    reInitZooms();
}


void uiFlatViewStdControl::homeZoomOptSelCB( CallBacker* cb )
{
    float x1pospercm = getCurrentPosPerCM( true );
    float x2pospercm = getCurrentPosPerCM( false );

    mDynamicCastGet(uiAction*,itm,cb)
    const int itmid = itm ? itm->getID() : sLocalHZIdx;
    if ( itmid == sLocalHZIdx )
    {
	defx1pospercm_ = x1pospercm;
	defx2pospercm_ = x2pospercm;
	gotohomezoombut_->setSensitive( true );
    }
    else if ( itmid == sGlobalHZIdx )
	setGlobalZoomLevel( x1pospercm, x2pospercm, setup_.isvertical_ );
    else
    {
	uiFlatViewZoomLevelDlg zoomlvldlg( this, x1pospercm, x2pospercm,
					   setup_.isvertical_ );
	if ( zoomlvldlg.go() )
	{
	    defx1pospercm_ = x1pospercm;
	    defx2pospercm_ = x2pospercm;
	    setViewToCustomZoomLevel( *vwrs_[0] );
	    gotohomezoombut_->setSensitive( true );
	}
    }
}


#define sInchToCMFac 2.54f

float uiFlatViewStdControl::getCurrentPosPerCM( bool forx1 ) const
{
    const uiFlatViewer& vwr = *vwrs_[0];
    const uiRect bbrect = vwr.getWorld2Ui().transform(vwr.boundingBox());
    const int nrpixels = forx1 ? bbrect.hNrPics() : bbrect.vNrPics();
    const float nrcms = (mCast(float,nrpixels)/uiMain::getDPI()) * sInchToCMFac;
    const int extrastep = forx1 ? 1 : 0;
    //<-- For x2 all points are not considered as flatviewer does not expand
    //<-- boundingbox by extfac_(0.5) along x2 as wiggles are not drawn in
    //<-- extended area.
    return (vwr.posRange(forx1).nrSteps() + extrastep) / nrcms;
}


void uiFlatViewStdControl::setGlobalZoomLevel(
		float x1pospercm, float x2pospercm, bool isvertical )
{
    Settings::common().set( sKeyVW2DTrcsPerCM(), x1pospercm );
    if ( isvertical )
	Settings::common().set( sKeyVW2DZPerCM(), x2pospercm );

    mSettWrite();
}


void uiFlatViewStdControl::getGlobalZoomLevel(
		float& x1pospercm, float& x2pospercm, bool isvertical )
{
    Settings::common().get( sKeyVW2DTrcsPerCM(), x1pospercm );
    if ( isvertical )
	Settings::common().get( sKeyVW2DZPerCM(), x2pospercm );
    else
	x2pospercm = x1pospercm;
}


void uiFlatViewStdControl::gotoHomeZoomCB( CallBacker* )
{
    setViewToCustomZoomLevel( *vwrs_[0] );
}


void uiFlatViewStdControl::setViewToCustomZoomLevel( uiFlatViewer& vwr )
{
    const bool ispoppedup = vwr.rgbCanvas().mainwin()->poppedUp();
    const float x1pospercm = (ispoppedup || mIsUdf(setup_.initialx1pospercm_))
			   ? defx1pospercm_ : setup_.initialx1pospercm_;
    const float x2pospercm = (ispoppedup || mIsUdf(setup_.initialx2pospercm_))
			   ? defx2pospercm_ : setup_.initialx2pospercm_;
    if ( mIsUdf(x1pospercm) || mIsUdf(x2pospercm) || mIsZero(x1pospercm,0.01) ||
	 mIsZero(x2pospercm,0.01) )
    {
	if ( !ispoppedup ) vwr.setViewToBoundingBox();
	return;
    }

    const uiRect viewrect = vwr.getViewRect( false );
    const int screendpi = uiMain::getDPI();
    const float cmwidth = ((float)viewrect.width()/screendpi) * sInchToCMFac;
    const float cmheight = ((float)viewrect.height()/screendpi) * sInchToCMFac;
    const double hwdth = vwr.posRange(true).step * cmwidth * x1pospercm / 2;
    const double hhght = vwr.posRange(false).step * cmheight * x2pospercm / 2;

    const uiWorldRect bb = vwr.boundingBox();
    uiWorldPoint wp(!ispoppedup? setup_.initialcentre_:vwr.curView().centre());
    if ( wp == uiWorldPoint::udf() ) wp = bb.centre();

    const uiWorldRect wr( wp.x-hwdth, wp.y-hhght, wp.x+hwdth, wp.y+hhght );
    vwr.setBoundingRect( uiWorld2Ui(viewrect.size(),wr).transform(bb) );
    vwr.setView( getZoomOrPanRect(wp,wr.size(),wr,bb) );
    updateZoomManager();
}


void uiFlatViewStdControl::handDragStarted( CallBacker* cb )
{
    mDynamicCastGet( const MouseEventHandler*, meh, cb );
    if ( !meh || meh->event().rightButton() ) return;

    const int vwridx = getViewerIdx( meh, false );
    if ( vwridx<0 ) return;
    uiFlatViewer* vwr = vwrs_[vwridx];
    if ( vwr->rgbCanvas().dragMode() != uiGraphicsViewBase::ScrollHandDrag )
	return;

    mousedownpt_ = meh->event().pos();
    mousepressed_ = true;
}


void uiFlatViewStdControl::handDragging( CallBacker* cb )
{
    mDynamicCastGet( const MouseEventHandler*, meh, cb );
    if ( !meh || !mousepressed_ ) return;

    const int vwridx = getViewerIdx( meh, false );
    if ( vwridx<0 ) return;
    uiFlatViewer* vwr = vwrs_[vwridx];

    const uiWorld2Ui& w2ui = vwr->getWorld2Ui();
    const uiWorldPoint startwpt = w2ui.transform( mousedownpt_ );
    const uiWorldPoint curwpt = w2ui.transform( meh->event().pos() );
    mousedownpt_ = meh->event().pos();

    uiWorldRect newwr( vwr->curView() );
    newwr.translate( startwpt-curwpt );

    newwr = getZoomOrPanRect( newwr.centre(), newwr.size(), newwr,
			      vwr->boundingBox() );
    vwr->setView( newwr );
}


void uiFlatViewStdControl::handDragged( CallBacker* cb )
{
    handDragging( cb );
    mousepressed_ = false;
}


void uiFlatViewStdControl::flipCB( CallBacker* )
{
    flip( true );
}


void uiFlatViewStdControl::parsCB( CallBacker* )
{
    doPropertiesDialog();
}


void uiFlatViewStdControl::setEditMode( bool yn )
{
    if ( editbut_ )
	editbut_->setOn( yn );
    dragModeCB( editbut_ );
}


void uiFlatViewStdControl::dragModeCB( CallBacker* cb )
{
    mDynamicCastGet(uiToolButton*,but,cb);
    const bool iseditbut = but==editbut_;
    const bool iseditmode = editbut_ && editbut_->isOn();
    const bool iszoommode = rubbandzoombut_ && rubbandzoombut_->isOn();

    uiGraphicsViewBase::ODDragMode mode( uiGraphicsViewBase::ScrollHandDrag );
    MouseCursor cursor( MouseCursor::OpenHand );
    if ( iszoommode || iseditmode )
    {
	mode = iszoommode ? uiGraphicsViewBase::RubberBandDrag
			  : uiGraphicsViewBase::NoDrag;
	cursor = iszoommode ? MouseCursor::Arrow : MouseCursor::Cross;
    }

    for ( int idx=0; idx<vwrs_.size(); idx++ )
    {
	vwrs_[idx]->rgbCanvas().setDragMode( mode );
	vwrs_[idx]->setCursor( cursor );
	vwrs_[idx]->appearance().annot_.editable_ = iseditmode &&
					!vwrs_[idx]->hasZAxisTransform();
	// TODO: Change while enabling tracking in Z-transformed 2D Viewers.
    }

    if ( iseditbut && iseditmode && rubbandzoombut_ )
	rubbandzoombut_->setOn( false );
}


void uiFlatViewStdControl::helpCB( CallBacker* )
{
    HelpProvider::provideHelp( helpkey_ );
}


bool uiFlatViewStdControl::handleUserClick( int vwridx )
{
    const MouseEvent& ev = mouseEventHandler(vwridx,true).event();
    if ( ev.rightButton() && !ev.ctrlStatus() && !ev.shiftStatus() &&
	  !ev.altStatus() )
    {
	menu_.executeMenu(0);
	return true;
    }
    return false;
}


void uiFlatViewStdControl::createMenuCB( CallBacker* cb )
{
    mDynamicCastGet(uiMenuHandler*,menu,cb);
    if ( !menu ) return;

    mAddMenuItem( menu, &propertiesmnuitem_, true, false );
}


void uiFlatViewStdControl::handleMenuCB( CallBacker* cb )
{
    mCBCapsuleUnpackWithCaller( int, mnuid, caller, cb );
    mDynamicCastGet( MenuHandler*, menu, caller );
    if ( mnuid==-1 || menu->isHandled() )
	return;

    const bool ishandled = mnuid==propertiesmnuitem_.id ;
    if ( ishandled )
	doPropertiesDialog();

    menu->setIsHandled( ishandled );
}


void uiFlatViewStdControl::coltabChg( CallBacker* )
{
    vwr_.appearance().ddpars_.vd_ = ctabed_->getDisplayPars();
    vwr_.handleChange( FlatView::Viewer::DisplayPars );
}


void uiFlatViewStdControl::keyPressCB( CallBacker* cb )
{
    mDynamicCastGet( const KeyboardEventHandler*, keh, cb );
    if ( !keh || !keh->hasEvent() ) return;

    if ( keh->event().key_==OD::Escape && rubbandzoombut_ )
    {
	rubbandzoombut_->setOn( !rubbandzoombut_->isOn() );
	dragModeCB( rubbandzoombut_ );
    }
}


NotifierAccess* uiFlatViewStdControl::editPushed()
{ return editbut_ ? &editbut_->activated : 0; }

