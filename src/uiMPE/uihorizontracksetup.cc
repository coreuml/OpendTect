/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        K. Tingdahl
 Date:          Dec 2005
________________________________________________________________________

-*/

#include "uihorizontracksetup.h"

#include "draw.h"
#include "emhorizon2d.h"
#include "emhorizon3d.h"
#include "emmanager.h"
#include "emsurfaceauxdata.h"
#include "emundo.h"
#include "executor.h"
#include "horizonadjuster.h"
#include "horizon2dseedpicker.h"
#include "horizon3dseedpicker.h"
#include "horizon2dtracker.h"
#include "horizon3dtracker.h"
#include "mpeengine.h"
#include "ptrman.h"
#include "randcolor.h"
#include "sectiontracker.h"
#include "seisdatapack.h"
#include "seispreload.h"
#include "survinfo.h"

#include "uibutton.h"
#include "uibuttongroup.h"
#include "uicolor.h"
#include "uiflatviewer.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uimpecorrelationgrp.h"
#include "uimpeeventgrp.h"
#include "uimpepartserv.h"
#include "uimsg.h"
#include "uiseissel.h"
#include "uiseparator.h"
#include "uislider.h"
#include "uitabstack.h"
#include "uitaskrunner.h"
#include "uitoolbar.h"
#include "uitoolbutton.h"
#include "od_helpids.h"


#define mErrRet(s) { uiMSG().error( s ); return false; }

namespace MPE
{

void uiBaseHorizonSetupGroup::initClass()
{
    uiMPE().setupgrpfact.addFactory( uiBaseHorizonSetupGroup::create,
				     Horizon2DTracker::keyword() );
    uiMPE().setupgrpfact.addFactory( uiBaseHorizonSetupGroup::create,
				     Horizon3DTracker::keyword() );
}


uiSetupGroup* uiBaseHorizonSetupGroup::create( uiParent* p, const char* typestr)
{
    const FixedString type( typestr );
    if ( type != EM::Horizon3D::typeStr() && type != EM::Horizon2D::typeStr() )
	return 0;

    return new uiBaseHorizonSetupGroup( p, typestr );
}


uiBaseHorizonSetupGroup::uiBaseHorizonSetupGroup( uiParent* p,
						  const char* typestr )
    : uiHorizonSetupGroup( p, typestr )
{}


uiHorizonSetupGroup::uiHorizonSetupGroup( uiParent* p, const char* typestr )
    : uiSetupGroup(p,"")
    , trackmgr_(0)
    , sectiontracker_(0)
    , horadj_(0)
    , is2d_(FixedString(typestr)==EM::Horizon2D::typeStr())
    , modeChanged_(this)
    , varianceChanged_(this)
    , propertyChanged_(this)
    , mps_(0)
    , mode_(EMSeedPicker::TrackFromSeeds)
{
    tabgrp_ = new uiTabStack( this, "TabStack" );
    uiGroup* modegrp = createModeGroup();
    tabgrp_->addTab( modegrp, uiStrings::sMode() );

    eventgrp_ = new uiEventGroup( tabgrp_->tabGroup(), is2d_ );
    tabgrp_->addTab( eventgrp_, uiStrings::sEvent() );

    correlationgrp_ = new uiCorrelationGroup( tabgrp_->tabGroup(), is2d_ );
    tabgrp_->addTab( correlationgrp_, uiStrings::sCorrelation() );

    uiGroup* propertiesgrp = createPropertyGroup();
    tabgrp_->addTab( propertiesgrp, uiStrings::sProperties() );

    mDynamicCastGet(uiDialog*,dlg,p)
    toolbar_ = new uiToolBar( dlg, tr("Tracking tools"), uiToolBar::Left );
    initToolBar();

    engine().actionCalled.notify( mCB(this,uiHorizonSetupGroup,mpeActionCB) );
}


uiHorizonSetupGroup::~uiHorizonSetupGroup()
{
    engine().actionCalled.remove( mCB(this,uiHorizonSetupGroup,mpeActionCB) );
}


void uiHorizonSetupGroup::initToolBar()
{
    trackbutid_ = -1;

    startbutid_ = toolbar_->addButton("autotrack",tr("Start Auto Tracking [K]"),
				mCB(this,uiHorizonSetupGroup,startCB) );
    toolbar_->setShortcut( startbutid_, "k" );

    stopbutid_ = toolbar_->addButton( "stop", tr("Stop Auto Tracking [S]"),
				mCB(this,uiHorizonSetupGroup,stopCB) );
    toolbar_->setShortcut( stopbutid_, "s" );

    savebutid_ = toolbar_->addButton( "save", uiStrings::phrSave(
			  toUiString("%1 [Ctrl+S]").arg(uiStrings::sHorizon())),
			  mCB(this,uiHorizonSetupGroup,saveCB) );
    toolbar_->setShortcut( savebutid_, "ctrl+s" );

    retrackbutid_ = toolbar_->addButton( "retrackhorizon", tr("Retrack All"),
				mCB(this,uiHorizonSetupGroup,retrackCB) );

    undobutid_ = toolbar_->addButton( "undo", toUiString("%1 [Ctrl+Z]")
					.arg(uiStrings::sUndo()),
					mCB(this,uiHorizonSetupGroup,undoCB) );
    toolbar_->setShortcut( undobutid_, "ctrl+z" );

    redobutid_ = toolbar_->addButton( "redo", toUiString("%1 [Ctrl+Y]")
					.arg(uiStrings::sRedo()),
					mCB(this,uiHorizonSetupGroup,redoCB) );
    toolbar_->setShortcut( redobutid_, "ctrl+y" );

    updateButtonSensitivity();
}


void uiHorizonSetupGroup::enableTracking( bool yn )
{
    toolbar_->setSensitive( yn );
    if ( yn )
	updateButtonSensitivity();
}


void uiHorizonSetupGroup::setMPEPartServer( uiMPEPartServer* mps )
{
    mps_ = mps;
}


void uiHorizonSetupGroup::mpeActionCB( CallBacker* )
{
    mEnsureExecutedInMainThread( uiHorizonSetupGroup::mpeActionCB );

    updateButtonSensitivity();
    if ( !mps_ ) return;

    const MPE::Engine::TrackState state = engine().getState();
    if ( state==MPE::Engine::Started || state==MPE::Engine::Stopped )
	mps_->sendMPEEvent( uiMPEPartServer::evHorizonTracking() );

}


void uiHorizonSetupGroup::updateButtonSensitivity()
{
    const bool stopped = engine().getState() == MPE::Engine::Stopped;
    const bool usedata = mode_ != EMSeedPicker::DrawBetweenSeeds;
    const bool doauto = mode_ == EMSeedPicker::TrackFromSeeds ||
			mode_ == EMSeedPicker::TrackBetweenSeeds;
    const bool invol = !is2d_ && doauto;

//    betweenseedsfld_->setSensitive( modeselgrp_->selectedId()==0 );
    betweenseedsfld_->setSensitive( false ); // for time being
    snapfld_->setSensitive( modeselgrp_->selectedId()==1 );
    if ( failfld_ ) failfld_->setSensitive( modeselgrp_->selectedId()==0 );

    methodfld_->setSensitive( doauto );
    eventgrp_->updateSensitivity( doauto );
    tabgrp_->setTabEnabled( eventgrp_, usedata );
    tabgrp_->setTabEnabled( correlationgrp_, doauto );

    seedtypefld_->setSensitive( doauto );
    seedcolselfld_->setSensitive( doauto );
    seedsliderfld_->setSensitive( doauto );

    parentcolfld_->setSensitive( !is2d_ );
    lockcolfld_->setSensitive( !is2d_ );

    toolbar_->setSensitive( startbutid_, invol && stopped );
    toolbar_->setSensitive( stopbutid_, invol && !stopped );
    toolbar_->setSensitive( savebutid_, stopped );
    toolbar_->setSensitive( retrackbutid_, invol && stopped );

    toolbar_->setSensitive( undobutid_, false );
    toolbar_->setSensitive( redobutid_, false );

    EMTracker* tracker = engine().getActiveTracker();
    if ( trackbutid_ != -1 )
	toolbar_->turnOn( trackbutid_, tracker ? tracker->isEnabled() : false );

    if ( tracker )
    {
	EMSeedPicker* seedpicker = tracker->getSeedPicker( false );
	const bool canautotrack = seedpicker && seedpicker->nrSeeds();
	toolbar_->setSensitive( startbutid_, canautotrack &&
					     invol && stopped );
	const EM::Object* obj = tracker->emObject();
	if ( obj )
	{
	    toolbar_->setSensitive(
		undobutid_,EM::MGR().undo(obj->id()).canUnDo());
	    toolbar_->setSensitive(
		redobutid_,EM::MGR().undo(obj->id()).canReDo());
	}
    }
}


void uiHorizonSetupGroup::enabTrackCB( CallBacker* )
{
    engine().enableTracking( toolbar_->isOn(trackbutid_) );
}


void uiHorizonSetupGroup::startCB( CallBacker* )
{
    setFocus();
    uiString errmsg;
    if ( !engine().startTracking(errmsg) && !errmsg.isEmpty() )
	uiMSG().error( errmsg );
}


void uiHorizonSetupGroup::stopCB( CallBacker* )
{
    setFocus();
    engine().stopTracking();
}


void uiHorizonSetupGroup::saveCB( CallBacker* )
{
    setFocus();
    if ( !mps_ ) return;

    mps_->sendMPEEvent( uiMPEPartServer::evStoreEMObject() );
}


void uiHorizonSetupGroup::retrackCB( CallBacker* )
{
    setFocus();
    uiString errmsg;
    if ( !engine().startRetrack(errmsg) && !errmsg.isEmpty() )
	uiMSG().error( errmsg );
}


void uiHorizonSetupGroup::undoCB( CallBacker* )
{
    uiUserShowWait( this, tr("Undoing changes") );
    uiString errmsg;
    engine().undo( errmsg );
    if ( !errmsg.isEmpty() )
	uiMSG().error( errmsg );

    updateButtonSensitivity();
}


void uiHorizonSetupGroup::redoCB( CallBacker* )
{
    uiUserShowWait( this, tr("Re-doing changes") );
    uiString errmsg;
    engine().redo( errmsg );
    if ( !errmsg.isEmpty() )
	uiMSG().error( errmsg );

    updateButtonSensitivity();
}


uiGroup* uiHorizonSetupGroup::createModeGroup()
{
    uiGroup* grp = new uiGroup( tabgrp_->tabGroup(), "Mode" );

    modeselgrp_ = new uiButtonGroup( grp, "ModeSel", OD::Vertical );
    modeselgrp_->setExclusive( true );
    grp->setHAlignObj( modeselgrp_ );

    uiRadioButton* butptr = new uiRadioButton( modeselgrp_,
	tr("Section Auto-track") );
    butptr->activated.notify(
			mCB(this,uiHorizonSetupGroup,seedModeChange) );
    butptr = new uiRadioButton( modeselgrp_, tr("Manual Draw") );
    butptr->activated.notify(
			mCB(this,uiHorizonSetupGroup,seedModeChange) );

    uiGroup* optiongrp = new uiGroup( grp, "Options");
    betweenseedsfld_ = new uiCheckBox( optiongrp,
			tr("Between Seeds") );
    betweenseedsfld_->display( false );
    betweenseedsfld_->activated.notify(
			mCB(this,uiHorizonSetupGroup,seedModeChange) );
    snapfld_ = new uiCheckBox( optiongrp, tr("Snap to Event") );
    snapfld_->activated.notify(
			mCB(this,uiHorizonSetupGroup,seedModeChange) );
    snapfld_->attach( alignedBelow, betweenseedsfld_ );
    optiongrp->attach( rightTo, modeselgrp_ );

    uiSeparator* sep = new uiSeparator( grp );
    sep->attach( stretchedBelow, modeselgrp_ );
    uiStringSet strs; strs.add( tr("Seed Trace") ).add( tr("Adjacent Parent") );
    methodfld_ = new uiGenInput( grp, uiStrings::sMethod(),
				 StringListInpSpec(strs) );
    methodfld_->valuechanged.notify(
			mCB(this,uiHorizonSetupGroup,seedModeChange) );
    methodfld_->attach( alignedBelow, modeselgrp_ );
    methodfld_->attach( ensureBelow, sep );

    if ( is2d_ )
    {
	failfld_ = new uiGenInput( grp, tr("If tracking fails"),
		BoolInpSpec(true,uiStrings::sExtrapolate(),uiStrings::sStop()));
	failfld_->attach( alignedBelow, methodfld_ );
	failfld_->valuechanged.notify(
		mCB(this,uiHorizonSetupGroup,seedModeChange) );
    }
    else
	failfld_ = 0;

    return grp;
}


uiGroup* uiHorizonSetupGroup::createVarianceGroup()
{
    uiGroup* grp = new uiGroup( tabgrp_->tabGroup(), "Variance" );

    usevarfld_ = new uiGenInput( grp, tr("Use Variance"), BoolInpSpec(false) );
    usevarfld_->valuechanged.notify(
	    mCB(this,uiHorizonSetupGroup,selUseVariance) );
    usevarfld_->valuechanged.notify(
	    mCB(this,uiHorizonSetupGroup,varianceChangeCB) );

    const IOObjContext ctxt =
	uiSeisSel::ioContext( is2d_ ? Seis::Line : Seis::Vol, true );
    uiSeisSel::Setup ss( is2d_, false );
    variancefld_ = new uiSeisSel( grp, ctxt, ss );
    variancefld_->attach( alignedBelow, usevarfld_ );

    varthresholdfld_ =
	new uiGenInput( grp, tr("Variance threshold"), FloatInpSpec() );
    varthresholdfld_->attach( alignedBelow, variancefld_ );
    varthresholdfld_->valuechanged.notify(
	    mCB(this,uiHorizonSetupGroup,varianceChangeCB) );

    grp->setHAlignObj( usevarfld_ );
    return grp;
}


uiGroup* uiHorizonSetupGroup::createPropertyGroup()
{
    uiGroup* grp = new uiGroup( tabgrp_->tabGroup(), "Properties" );
    colorfld_ = new uiColorInput( grp,
				uiColorInput::Setup(Color::Green())
				.withdesc(false).lbltxt(tr("Horizon Color")) );
    colorfld_->colorChanged.notify(
			mCB(this,uiHorizonSetupGroup,colorChangeCB) );
    grp->setHAlignObj( colorfld_ );

    linewidthfld_ = new uiSlider( grp, uiSlider::Setup(tr("Line Width"))
					.withedit(true),
				  "Line Width" );
    linewidthfld_->setInterval( 1, 15, 1 );
    linewidthfld_->valueChanged.notify(
			mCB(this,uiHorizonSetupGroup,colorChangeCB) );
    linewidthfld_->attach( alignedBelow, colorfld_ );

    seedtypefld_ = new uiGenInput( grp, tr("Seed Shape/Color"),
			StringListInpSpec(OD::MarkerStyle3D::TypeDef()) );
    seedtypefld_->valuechanged.notify(
			mCB(this,uiHorizonSetupGroup,seedTypeSel) );
    seedtypefld_->attach( alignedBelow, linewidthfld_ );

    seedcolselfld_ = new uiColorInput( grp,
				uiColorInput::Setup(Color::DgbColor())
				.withdesc(false) );
    seedcolselfld_->attach( rightTo, seedtypefld_ );
    seedcolselfld_->colorChanged.notify(
				mCB(this,uiHorizonSetupGroup,seedColSel) );

    seedsliderfld_ = new uiSlider( grp,
				uiSlider::Setup(tr("Seed Size")).
				withedit(true),	"Seed Size" );
    seedsliderfld_->setInterval( 1, 15 );
    seedsliderfld_->valueChanged.notify(
			mCB(this,uiHorizonSetupGroup,seedSliderMove) );
    seedsliderfld_->attach( alignedBelow, seedtypefld_ );

    parentcolfld_ = new uiColorInput( grp,
			uiColorInput::Setup(Color::Yellow())
			.withdesc(false).lbltxt(uiStrings::sParent(mPlural)) );
    parentcolfld_->colorChanged.notify(
			mCB(this,uiHorizonSetupGroup,specColorChangeCB) );
    parentcolfld_->attach( alignedBelow, seedsliderfld_ );

    selectioncolfld_ = new uiColorInput( grp,
	   uiColorInput::Setup(EM::Horizon3D::sDefaultSelectionColor())
		    .withdesc(false).lbltxt(uiStrings::sSelection(mPlural)) );
    selectioncolfld_->colorChanged.notify(
			mCB(this,uiHorizonSetupGroup,specColorChangeCB) );
    selectioncolfld_->attach( rightTo, parentcolfld_ );

    lockcolfld_ = new uiColorInput(
	grp,uiColorInput::Setup(EM::Horizon3D::sDefaultLockColor())
				.withdesc(false).lbltxt(uiStrings::sLocked()) );
    lockcolfld_->colorChanged.notify(
			mCB(this,uiHorizonSetupGroup,specColorChangeCB) );
    lockcolfld_->attach( alignedBelow, parentcolfld_ );

    return grp;
}


NotifierAccess* uiHorizonSetupGroup::eventChangeNotifier()
{ return eventgrp_->changeNotifier(); }


NotifierAccess*	uiHorizonSetupGroup::correlationChangeNotifier()
{ return correlationgrp_->changeNotifier(); }


void uiHorizonSetupGroup::selUseVariance( CallBacker* )
{
    const bool usevar = usevarfld_->getBoolValue();
    variancefld_->setSensitive( usevar );
    varthresholdfld_->setSensitive( usevar );
}


void uiHorizonSetupGroup::seedModeChange( CallBacker* )
{
    const int modeidx = modeselgrp_->selectedId();
    if ( modeidx==0 )
    {
	mode_ = betweenseedsfld_->isChecked() ? EMSeedPicker::TrackBetweenSeeds
					      : EMSeedPicker::TrackFromSeeds;
    }
    else
    {
	mode_ = snapfld_->isChecked() ? EMSeedPicker::DrawAndSnap
				      : EMSeedPicker::DrawBetweenSeeds;
    }

    modeChanged_.trigger();
    updateButtonSensitivity();
}


void uiHorizonSetupGroup::varianceChangeCB( CallBacker* )
{ varianceChanged_.trigger(); }


void uiHorizonSetupGroup::specColorChangeCB( CallBacker* cb )
{
    if ( !sectiontracker_ ) return;

    EM::Object* emobj = &sectiontracker_->emObject();
    if ( !emobj ) return;

    mDynamicCastGet(EM::Horizon3D*,hor3d,emobj)

    if ( hor3d && cb == parentcolfld_ )
	hor3d->setParentColor( parentcolfld_->color() );
    else if ( cb==selectioncolfld_ )
	emobj->setSelectionColor( selectioncolfld_->color() );
    else if ( hor3d && cb==lockcolfld_ )
	hor3d->setLockColor( lockcolfld_->color() );
    else if ( hor3d && cb==0 )
    {
	// set initial color to horizon
	hor3d->setParentColor( parentcolfld_->color() );
	hor3d->setSelectionColor( selectioncolfld_->color() );
	hor3d->setLockColor( lockcolfld_->color() );
    }
}


void uiHorizonSetupGroup::colorChangeCB( CallBacker* )
{
    propertyChanged_.trigger();
}


void uiHorizonSetupGroup::seedTypeSel( CallBacker* )
{
    const OD::MarkerStyle3D::Type newtype =
	(OD::MarkerStyle3D::Type)
		(OD::MarkerStyle3D::None+seedtypefld_->getIntValue());
    if ( markerstyle_.type_ == newtype )
	return;

    markerstyle_.type_ = newtype;
    propertyChanged_.trigger();
}


void uiHorizonSetupGroup::seedSliderMove( CallBacker* )
{
    const float sldrval = seedsliderfld_->getFValue();
    const int newsize = mNINT32(sldrval);
    if ( markerstyle_.size_ == newsize )
	return;

    markerstyle_.size_ = newsize;
    propertyChanged_.trigger();
}


void uiHorizonSetupGroup::seedColSel( CallBacker* )
{
    const Color newcolor = seedcolselfld_->color();
    if ( markerstyle_.color_ == newcolor )
	return;

    markerstyle_.color_ = newcolor;
    propertyChanged_.trigger();
}


void uiHorizonSetupGroup::setSectionTracker( SectionTracker* st )
{
    sectiontracker_ = st;
    mDynamicCastGet(HorizonAdjuster*,horadj,
		    sectiontracker_ ? sectiontracker_->adjuster() : 0)
    horadj_ = horadj;
    if ( horadj_ )
	initStuff();

    correlationgrp_->setSectionTracker( st );
    eventgrp_->setSectionTracker( st );
}


void uiHorizonSetupGroup::initModeGroup()
{
    const int modeidx = mode_==EMSeedPicker::TrackFromSeeds ||
			mode_==EMSeedPicker::TrackBetweenSeeds ? 0 : 1;
    modeselgrp_->selectButton( modeidx );

    betweenseedsfld_->setChecked( mode_==EMSeedPicker::TrackBetweenSeeds );
    snapfld_->setChecked( mode_==EMSeedPicker::DrawAndSnap );

    if ( horadj_ )
    {
	methodfld_->setValue(
		horadj_->getCompareMethod()==EventTracker::SeedTrace ? 0 : 1 );
	if ( failfld_ )
	    failfld_->setValue( !horadj_->removesOnFailure() );
    }

    updateButtonSensitivity();
}


void uiHorizonSetupGroup::initStuff()
{
    initModeGroup();
//    initVarianceGroup();
//    selUseVariance(0);
    initPropertyGroup();
}


void uiHorizonSetupGroup::initVarianceGroup()
{
}


void uiHorizonSetupGroup::initPropertyGroup()
{
    seedsliderfld_->setValue( markerstyle_.size_ );
    seedcolselfld_->setColor( markerstyle_.color_ );
    seedtypefld_->setValue( markerstyle_.type_-OD::MarkerStyle3D::None );

    if ( !sectiontracker_ )
	return;

    const EM::Object& emobj = sectiontracker_->emObject();
    lockcolfld_->setColor( emobj.getLockColor() );
    selectioncolfld_->setColor( emobj.selectionColor() );

    mDynamicCastGet(const EM::Horizon3D*,hor3d,&emobj)
    if ( hor3d )
	parentcolfld_->setColor( hor3d->getParentColor() );

}


void uiHorizonSetupGroup::setMode( EMSeedPicker::TrackMode mode )
{
    mode_ = mode;
    initModeGroup();
}


EMSeedPicker::TrackMode uiHorizonSetupGroup::getMode() const
{ return mode_; }


void uiHorizonSetupGroup::setTrackingMethod( EventTracker::CompareMethod cm )
{
    if ( horadj_ ) horadj_->setCompareMethod( cm );
    methodfld_->setValue( cm==EventTracker::SeedTrace ? 0 : 1 );
}


EventTracker::CompareMethod uiHorizonSetupGroup::getTrackingMethod() const
{
    return methodfld_->getIntValue()==0 ? EventTracker::SeedTrace
					: EventTracker::AdjacentParent;
}


void uiHorizonSetupGroup::setSeedPos( const TrcKeyValue& tkv )
{
    eventgrp_->setSeedPos( tkv );
    correlationgrp_->setSeedPos( tkv );
    updateButtonSensitivity();
}


void uiHorizonSetupGroup::setColor( const Color& col )
{ colorfld_->setColor( col ); }

const Color& uiHorizonSetupGroup::getColor()
{ return colorfld_->color(); }

void uiHorizonSetupGroup::setLineWidth( int w )
{ linewidthfld_->setValue( w ); }

int uiHorizonSetupGroup::getLineWidth() const
{ return linewidthfld_->getIntValue(); }


void uiHorizonSetupGroup::setMarkerStyle( const OD::MarkerStyle3D& markerstyle )
{
    markerstyle_ = markerstyle;
    initPropertyGroup();
}


const OD::MarkerStyle3D& uiHorizonSetupGroup::getMarkerStyle()
{
    return markerstyle_;
}


bool uiHorizonSetupGroup::commitToTracker( bool& fieldchange ) const
{
    if ( !sectiontracker_ || !horadj_ )
	return false;

    fieldchange = false;
    correlationgrp_->commitToTracker( fieldchange );
    eventgrp_->commitToTracker( fieldchange );

    if ( !horadj_ || horadj_->getNrAttributes()<1 )
    {
	uiMSG().warning( tr("Unable to apply tracking setup") );
	return true;
    }

    horadj_->setCompareMethod( getTrackingMethod() );
    horadj_->removeOnFailure( failfld_ ? !failfld_->getBoolValue() : true );

    return true;
}


void uiHorizonSetupGroup::showGroupOnTop( const char* grpnm )
{
    tabgrp_->setCurrentPage( grpnm );
    mDynamicCastGet(uiDialog*,dlg,parent())
    if ( dlg && !dlg->isHidden() )
    {
	 dlg->showNormal();
	 dlg->raise();
    }
}


} //namespace MPE
