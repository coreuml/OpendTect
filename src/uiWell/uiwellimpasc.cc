/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        N. Hemstra
 Date:          August 2003
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "uiwellimpasc.h"

#include "file.h"
#include "filepath.h"
#include "ioman.h"
#include "ioobj.h"
#include "iopar.h"
#include "keystrs.h"
#include "od_istream.h"
#include "ptrman.h"
#include "separstr.h"
#include "survinfo.h"
#include "tabledef.h"
#include "unitofmeasure.h"
#include "welldata.h"
#include "wellimpasc.h"
#include "welltrack.h"
#include "wellwriter.h"

#include "uibutton.h"
#include "uid2tmodelgrp.h"
#include "uifileinput.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uimsg.h"
#include "uiseparator.h"
#include "uiselsurvranges.h"
#include "uitblimpexpdatasel.h"
#include "uiwellsel.h"
#include "od_helpids.h"


uiWellImportAsc::uiWellImportAsc( uiParent* p )
    : uiDialog(p,uiDialog::Setup(tr("Import Well Track"),mNoDlgTitle,
				 mODHelpKey(mWellImportAscHelpID)).modal(false))
    , fd_(*Well::TrackAscIO::getDesc())
    , wd_(*new Well::Data)
    , zun_(UnitOfMeasure::surveyDefDepthUnit())
    , importReady(this)
{
    enableSaveButton( tr("Display after import") );
    setOkCancelText( uiStrings::sImport(), uiStrings::sClose() );

    havetrckbox_ = new uiCheckBox( this, uiStrings::sEmptyString() );
    havetrckbox_->setChecked( true );
    havetrckbox_->activated.notify( mCB(this,uiWellImportAsc,haveTrckSel) );

    trckinpfld_ = new uiFileInput( this, "Well Track File",
				   uiFileInput::Setup().withexamine(true) );
    trckinpfld_->valuechanged.notify( mCB(this,uiWellImportAsc,inputChgd) );
    trckinpfld_->attach( rightOf, havetrckbox_ );

    vertwelllbl_ = new uiLabel( this, tr("-> Vertical well") );
    vertwelllbl_->attach( rightTo, havetrckbox_ );
    vertwelllbl_->attach( alignedWith, trckinpfld_ );

    dataselfld_ = new uiTableImpDataSel( this, fd_,
                      mODHelpKey(mWellImportAscDataSelHelpID) );
    dataselfld_->attach( alignedBelow, trckinpfld_ );
    dataselfld_->descChanged.notify( mCB(this,uiWellImportAsc,trckFmtChg) );

    BufferString coordunitslbl( "Coordinate " );
    coordunitslbl += SI().getXYUnitString();
    coordfld_ = new uiGenInput( this, coordunitslbl,
			PositionInpSpec(PositionInpSpec::Setup(true)) );
    coordfld_->attach( alignedBelow, trckinpfld_ );

    const uiString zunit = UnitOfMeasure::surveyDefDepthUnitAnnot( true, true );
    uiString kblbl = tr( "%1 %2" )
		     .arg(Well::Info::sKeykbelev()).arg( zunit );
    kbelevfld_ = new uiGenInput( this, kblbl, FloatInpSpec(0) );
    kbelevfld_->setWithCheck();
    kbelevfld_->setChecked( false );
    kbelevfld_->attach( alignedBelow, dataselfld_ );

    uiString tdlbl = tr( "%1 %2" )
		     .arg(Well::Info::sKeyTD()).arg( zunit );
    tdfld_ = new uiGenInput( this, tdlbl, FloatInpSpec() );
    tdfld_->setWithCheck();
    tdfld_->setChecked( false );
    tdfld_->attach( alignedBelow, kbelevfld_ );

    uiSeparator* sep = new uiSeparator( this, "H sep" );
    sep->attach( stretchedBelow, tdfld_ );

    wd_.ref();
    float dispval = wd_.info().replvel;
    if ( !mIsUdf(dispval) )
	wd_.info().replvel = dispval;

    const bool zistime = SI().zIsTime();
    if ( zistime )
    {
	uiD2TModelGroup::Setup su; su.asksetcsmdl( true );
	d2tgrp_ = new uiD2TModelGroup( this, su );
	d2tgrp_->attach( alignedBelow, dataselfld_ );
	d2tgrp_->attach( ensureBelow, sep );
	sep = new uiSeparator( this, "H sep 2" );
	sep->attach( stretchedBelow, d2tgrp_ );
    }

    uiButton* but = new uiPushButton( this, tr("Advanced/Optional"),
					mCB(this,uiWellImportAsc,doAdvOpt),
					false );
    but->attach( alignedBelow, zistime ? (uiObject*)d2tgrp_
				       : (uiObject*)dataselfld_ );
    but->attach( ensureBelow, sep );

    outfld_ = new uiWellSel( this, false );
    outfld_->attach( alignedBelow, but );

    postFinalise().notify( mCB(this,uiWellImportAsc,haveTrckSel) );
}


uiWellImportAsc::~uiWellImportAsc()
{
    delete &fd_;
    wd_.unRef();
}


MultiID uiWellImportAsc::getWellID() const
{ return outfld_->key(); }


void uiWellImportAsc::haveTrckSel( CallBacker* )
{
    const bool havetrck = havetrckbox_->isChecked();
    trckinpfld_->display( havetrck );
    dataselfld_->display( havetrck );
    vertwelllbl_->display( !havetrck );
    coordfld_->display( !havetrck );
    kbelevfld_->setChecked( !havetrck );
    tdfld_->setChecked( !havetrck );
}


void uiWellImportAsc::inputChgd( CallBacker* )
{
    FilePath fnmfp( trckinpfld_->fileName() );
    outfld_->setInputText( fnmfp.baseName() );
    kbelevfld_->setValue(0);
    tdfld_->setChecked( false );
    tdfld_->setValue( mUdf(float) );
}


void uiWellImportAsc::trckFmtChg( CallBacker* )
{
    const Table::FormatDesc& fd = dataselfld_->desc();
    if ( !fd.isGood() )
	return;

    bool havez = false;
    bool havemd = false;
    for ( int idx=0; idx<fd.bodyinfos_.size(); idx++ )
    {
	const Table::TargetInfo& ti = *fd.bodyinfos_[idx];
	if ( ti.name() == "Z" && ti.selection_.isInFile(0) )
	    havez = true;

	if ( ti.name() == "MD" && ti.selection_.isInFile(0) )
	    havemd = true;
    }

    if ( !havez && !havemd )
    {
	uiMSG().error( tr("The format you defined has neither Z nor MD."
			  "\nYou should define at least one."
			  "\nAs it is now, the track will not load.") );
    }

    kbelevfld_->setChecked( !havez || !havemd );
}



class uiWellImportAscOptDlg : public uiDialog
{ mODTextTranslationClass(uiWellImportAscOptDlg);
public:

uiWellImportAscOptDlg( uiWellImportAsc* p )
    : uiDialog(p,uiDialog::Setup(tr("Import well: Advanced/Optional"),
				 tr("Advanced and Optional"),
                                 mODHelpKey(mWellImpPptDlgHelpID)))
    , uwia_(p)
    , zun_(UnitOfMeasure::surveyDefDepthUnit())
{
    const Well::Info& info = uwia_->wd_.info();

    PositionInpSpec::Setup possu( true );
    if ( !mIsZero(info.surfacecoord.x,0.1) )
	possu.coord_ = info.surfacecoord;
    coordfld = new uiGenInput( this,
	tr("Surface coordinate (if different from first "
	   "coordinate in track file)"),
	PositionInpSpec(possu).setName( "X", 0 ).setName( "Y", 1 ) );

    const bool zinfeet = SI().depthsInFeet();

    float dispval = info.replvel;
    if ( zinfeet && zun_ )
	dispval = zun_->userValue( info.replvel );

    uiString lbl = tr("%1 %2")
		   .arg( Well::Info::sKeyreplvel() )
		   .arg( UnitOfMeasure::surveyDefVelUnitAnnot(true,true) );
    replvelfld = new uiGenInput( this, lbl, FloatInpSpec(dispval) );
    replvelfld->attach( alignedBelow, coordfld );

    dispval = info.groundelev;
    if ( zinfeet && zun_ ) dispval = zun_->userValue( info.groundelev );
    lbl = tr("%1 %2")
		  .arg( Well::Info::sKeygroundelev() )
	      .arg( UnitOfMeasure::surveyDefDepthUnitAnnot(true,true) );
    gdelevfld = new uiGenInput( this, lbl, FloatInpSpec(dispval) );
    gdelevfld->attach( alignedBelow, replvelfld );

    uiSeparator* horsep = new uiSeparator( this );
    horsep->attach( stretchedBelow, gdelevfld );

    idfld = new uiGenInput( this, Well::Info::sKeyuwid(),
			    StringInpSpec(info.uwid) );
    idfld->attach( alignedBelow, gdelevfld );

    operfld = new uiGenInput( this, Well::Info::sKeyoper(),
			      StringInpSpec(info.oper) );
    operfld->attach( alignedBelow, idfld );

    statefld = new uiGenInput( this, Well::Info::sKeystate(),
			       StringInpSpec(info.state) );
    statefld->attach( alignedBelow, operfld );

    countyfld = new uiGenInput( this, Well::Info::sKeycounty(),
				StringInpSpec(info.county) );
    countyfld->attach( rightTo, statefld );
}


bool acceptOK( CallBacker* )
{
    Well::Info& info = uwia_->wd_.info();

    if ( *coordfld->text() )
	info.surfacecoord = coordfld->getCoord();

    if ( *replvelfld->text() )
    {
	const float replvel = replvelfld->getfValue();
	if ( !mIsUdf(replvel) && zun_ )
	    info.replvel = zun_->internalValue( replvel );
    }

    if ( *gdelevfld->text() )
    {
	const float gdevel = gdelevfld->getfValue();
	if ( !mIsUdf(gdevel)  && zun_ )
	    info.groundelev = zun_->internalValue( gdevel );
    }

    info.uwid = idfld->text();
    info.oper = operfld->text();
    info.state = statefld->text();
    info.county = countyfld->text();

    return true;
}

    const UnitOfMeasure* zun_;
    uiWellImportAsc*	uwia_;
    uiGenInput*		coordfld;
    uiGenInput*		elevfld;
    uiGenInput*		replvelfld;
    uiGenInput*		gdelevfld;
    uiGenInput*		idfld;
    uiGenInput*		operfld;
    uiGenInput*		statefld;
    uiGenInput*		countyfld;
    uiCheckBox*		zinftbox;
};


void uiWellImportAsc::doAdvOpt( CallBacker* )
{
    uiWellImportAscOptDlg dlg( this );
    dlg.go();
}


bool uiWellImportAsc::acceptOK( CallBacker* )
{
    if ( checkInpFlds() )
    {
	doWork();
	wd_.info().surfacecoord.x = wd_.info().surfacecoord.y = 0;
	wd_.info().groundelev = mUdf(float);
    }
    return false;
}


#define mErrRet(s) { if ( (s).isSet() ) uiMSG().error(s); return false; }

bool uiWellImportAsc::doWork()
{
    wd_.setEmpty();
    wd_.info().setName( outfld_->getInput() );
    const IOObj* outioobj = outfld_->ioobj();
    if ( !outioobj )
	return false;

    float kbelev = kbelevfld_->getfValue();
    if ( mIsUdf(kbelev) )
	kbelev = 0;
    else if ( SI().depthsInFeet() && zun_ )
	kbelev = zun_->internalValue( kbelev );

    float td = tdfld_->getfValue();
    if ( !mIsUdf(td) && SI().depthsInFeet() && zun_ )
	td = zun_->internalValue( td ) ;

    BufferStringSet datasrcnms;
    if ( havetrckbox_->isChecked() )
    {
	const BufferString fnm( trckinpfld_->fileName() );
	datasrcnms.add( fnm );
	od_istream strm( fnm );
	if ( !strm.isOK() )
	    mErrRet( tr("Cannot open track file") )

	Well::TrackAscIO wellascio( fd_, strm );
	if ( !kbelevfld_->isChecked() )
	    kbelev = mUdf(float);

	if ( !tdfld_->isChecked() )
	    td = mUdf(float);

	if ( !wellascio.getData(wd_,kbelev,td) )
	{
	    uiString msg = tr( "The track file cannot be loaded:\n%1" )
				.arg( wellascio.errMsg() );
	    mErrRet( msg );
	}

	if ( wellascio.warnMsg().isSet() )
	{
	    uiString msg = tr( "The track file loading issued a warning:\n%1" )
				.arg( wellascio.warnMsg() );
	    uiMSG().warning( msg );
	}
    }
    else
    {
	datasrcnms.add( "[Vertical]" );

	if ( mIsUdf(td) || td < 1e-6 )
	{
	    float survzstop = SI().zRange(false).stop;
	    if ( SI().zIsTime() )
		survzstop *= 2000;
	    td = survzstop - kbelev;
	}

	Interval<float> zrg( -kbelev, td-kbelev );
	const Coord c( coordfld_->getCoord() );
	wd_.track().addPoint( c, zrg.start, 0 );
	wd_.track().addPoint( c, zrg.stop, td );
    }

    if ( SI().zIsTime() )
    {
	const bool validd2t = d2tgrp_->getD2T( wd_, false );
	if ( !validd2t )
	{
	    uiStringSet msgs;

	    if ( d2tgrp_->errMsg().isSet() )
		msgs += d2tgrp_->errMsg();

	    msgs += tr( "Alternatively, swith off the use of a"
			" Depth to Time model file" );
	    uiMSG().errorWithDetails( msgs );
	    return false;
	}

	if ( d2tgrp_->warnMsg().isSet() )
	{
	    uiString msg = tr( "The Time-Depth Model file loading issued"
			       " a warning:\n%1" )
			       .arg( d2tgrp_->warnMsg() );
	    uiMSG().warning( msg );
	}

	datasrcnms.add( d2tgrp_->dataSourceName() );
	if ( d2tgrp_->wantAsCSModel() )
	    wd_.setCheckShotModel( new Well::D2TModel( *wd_.d2TModel() ) );
    }

    Well::Writer wwr( *outioobj, wd_ );
    if ( !wwr.put() )
	mErrRet( tr(wwr.errMsg()) );

    outioobj->pars().update( sKey::CrFrom(), datasrcnms.cat("`") );
    outioobj->updateCreationPars();
    IOM().commitChanges( *outioobj );

    uiMSG().message( tr("Well import successful") );
    if ( saveButtonChecked() )
	importReady.trigger();

    return false;
}


bool uiWellImportAsc::checkInpFlds()
{
    if ( havetrckbox_->isChecked() )
    {
	if ( !*trckinpfld_->fileName() )
	    mErrRet(tr("Please specify a well track file"))

	if ( !dataselfld_->commit() )
	    return false;
    }
    else
    {
	if ( !SI().isReasonable(coordfld_->getCoord()) )
	{
	    if ( !uiMSG().askGoOn(
			tr("Well coordinate seems to be far outside the survey."
			"\nIs this correct?")) )
		return false;
	}

	if ( !kbelevfld_->isChecked() )
	    mErrRet(tr("Please specify a %1").arg(Well::Info::sKeykbelev()))

	if ( !tdfld_->isChecked() )
	    mErrRet(tr("Please specify a %1").arg(Well::Info::sKeyTD()))
    }

    if ( !outfld_->commitInput() )
	mErrRet( outfld_->isEmpty()
		? tr("Please enter a name for the well")
		: uiString::emptyString() )

    return true;
}
