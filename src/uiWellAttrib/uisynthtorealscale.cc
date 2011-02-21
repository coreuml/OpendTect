/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Bert
 * DATE     : Feb 2010
-*/

static const char* rcsID = "$Id: uisynthtorealscale.cc,v 1.7 2011-02-21 05:47:18 cvsranojay Exp $";

#include "uisynthtorealscale.h"

#include "emhorizon3d.h"
#include "emhorizon2d.h"
#include "emmanager.h"
#include "emsurfacetr.h"
#include "emsurfaceposprov.h"
#include "binidvalset.h"
#include "survinfo.h"
#include "polygon.h"
#include "position.h"
#include "seistrc.h"
#include "seistrcprop.h"
#include "seisbuf.h"
#include "seisread.h"
#include "seisselectionimpl.h"
#include "stratlevel.h"
#include "statruncalc.h"
#include "picksettr.h"
#include "wavelet.h"

#include "uistratseisevent.h"
#include "uiseissel.h"
#include "uiseparator.h"
#include "uihistogramdisplay.h"
#include "uiaxishandler.h"
#include "uigeninput.h"
#include "uibutton.h"
#include "uilabel.h"
#include "uimsg.h"
#include "uitaskrunner.h"


#define mErrRetBool(s)\
{ uiMSG().error(s); return false; }\

#define mErrRet(s)\
{ uiMSG().error(s); return; }\

class uiSynthToRealScaleStatsDisp : public uiGroup
{
public:

uiSynthToRealScaleStatsDisp( uiParent* p, const char* nm, bool left )
    : uiGroup(p,nm)
    , usrval_(mUdf(float))
    , usrValChanged(this)
{
    uiFunctionDisplay::Setup su;
    su.annoty( false ).noyaxis( true ).noy2axis( true ).drawgridlines( false );
    dispfld_ = new uiHistogramDisplay( this, su );
    dispfld_->xAxis()->setName( "" );
    dispfld_->setPrefWidth( 260 );
    dispfld_->setPrefHeight( GetGoldenMinor(260) );
    avgfld_ = new uiGenInput( this, "", FloatInpSpec() );
    if ( left )
	avgfld_->attach( rightAlignedBelow, dispfld_ );
    else
	avgfld_->attach( ensureBelow, dispfld_ );
    avgfld_->valuechanged.notify(mCB(this,uiSynthToRealScaleStatsDisp,avgChg));
    setHAlignObj( dispfld_ );
}

void avgChg( CallBacker* )
{
    usrval_ = avgfld_->getfValue();
    dispfld_->setMarkValue( usrval_, true );
    usrValChanged.trigger();
}

    float		usrval_;

    uiHistogramDisplay*	dispfld_;
    uiGenInput*		avgfld_;

    Notifier<uiSynthToRealScaleStatsDisp>	usrValChanged;

};


uiSynthToRealScale::uiSynthToRealScale( uiParent* p, bool is2d, SeisTrcBuf& tb,
					const MultiID& wid, const char* lvlnm )
    : uiDialog(p,Setup("Scale synthetics","Determine scaling for synthetics",
			mTODOHelpID))
    , is2d_(is2d)
    , synth_(tb)
    , wvltid_(wid)
{
    uiSeisSel::Setup sssu( is2d_, false );
    seisfld_ = new uiSeisSel( this, uiSeisSel::ioContext(sssu.geom_,true),
	    		      sssu );

    const IOObjContext horctxt( is2d_ ? mIOObjContext(EMHorizon2D)
	    			      : mIOObjContext(EMHorizon3D) );
    uiIOObjSel::Setup horsu( BufferString("Horizon for '",lvlnm,"'") );
    horfld_ = new uiIOObjSel( this, horctxt, horsu );
    horfld_->attach( alignedBelow, seisfld_ );

    IOObjContext polyctxt( mIOObjContext(PickSet) );
    polyctxt.toselect.require_.set( sKey::Type, sKey::Polygon );
    uiIOObjSel::Setup polysu( "Within Polygon" ); polysu.optional( true );
    polyfld_ = new uiIOObjSel( this, polyctxt, polysu );
    polyfld_->attach( alignedBelow, horfld_ );

    uiStratSeisEvent::Setup ssesu( true );
    ssesu.fixedlevel( Strat::LVLS().get(lvlnm) );
    evfld_ = new uiStratSeisEvent( this, ssesu );
    evfld_->attach( alignedBelow, polyfld_ );

    uiPushButton* gobut = new uiPushButton( this, "Extract values",
	    			mCB(this,uiSynthToRealScale,goPush), true );
    gobut->attach( alignedBelow, evfld_ );

    uiSeparator* sep = new uiSeparator( this, "separator" );
    sep->attach( stretchedBelow, gobut );

    uiGroup* statsgrp = new uiGroup( this, "Stats displays" );

    synthstatsfld_ = new uiSynthToRealScaleStatsDisp( statsgrp, "Synthetics",
	    					      true );
    realstatsfld_ = new uiSynthToRealScaleStatsDisp( statsgrp, "Real Seismics",
	    					     false );
    realstatsfld_->attach( rightOf, synthstatsfld_ );
    const CallBack setsclcb( mCB(this,uiSynthToRealScale,setScaleFld) );
    synthstatsfld_->usrValChanged.notify( setsclcb );
    realstatsfld_->usrValChanged.notify( setsclcb );
    statsgrp->attach( ensureBelow, sep );
    statsgrp->setHAlignObj( realstatsfld_ );

    finalscalefld_ = new uiGenInput( this, "", FloatInpSpec() );
    finalscalefld_->attach( centeredBelow, statsgrp );
    new uiLabel( this, "Scaling factor", finalscalefld_ );

    IOObjContext wvltctxt( mIOObjContext(Wavelet) );
    wvltctxt.forread = false;
    uiIOObjSel::Setup wvltsu( "Save scaled Wavelet" ); wvltsu.optional( true );
    wvltfld_ = new uiIOObjSel( this, wvltctxt, wvltsu );
    wvltfld_->attach( alignedBelow, finalscalefld_ );

    finaliseDone.notify( mCB(this,uiSynthToRealScale,initWin) );
}


void uiSynthToRealScale::initWin( CallBacker* )
{
    updSynthStats();
}


void uiSynthToRealScale::setScaleFld( CallBacker* )
{
    const float synthval = synthstatsfld_->usrval_;
    const float realval = realstatsfld_->usrval_;
    if ( mIsUdf(synthval) || mIsUdf(realval) || synthval == 0 )
	finalscalefld_->setValue( mUdf(float) );
    else
	finalscalefld_->setValue( realval / synthval );
}


bool uiSynthToRealScale::acceptOK( CallBacker* )
{
    if ( !evfld_->getFromScreen() )
	return false;

    SeisTrc& trc = *synth_.get( 0 );
    SeisTrcPropChg tpc( *synth_.get(synth_.size()/2) );
    tpc.scale( 3 );
    return true;
}


void uiSynthToRealScale::updSynthStats()
{
    TypeSet<float> vals;
    if ( evfld_->getFromScreen() )
    {
	const Strat::SeisEvent& ssev = evfld_->event();
	const int nrtms = ssev.extrwin_.nrSteps() + 1;
	for ( int idx=0; idx<synth_.size(); idx++ )
	{
	    const SeisTrc& trc = *synth_.get( idx );
	    const float reftm = ssev.snappedTime( trc );
	    if ( mIsUdf(reftm) ) continue;

	    if ( nrtms < 2 )
		vals += trc.getValue( reftm + ssev.extrwin_.center(), 0 );
	    else
	    {
		const int lastsamp = trc.size() - 1;
		const Interval<float> trg( trc.startPos(),
					   trc.samplePos(lastsamp) );
		float sumsq = 0;
		for ( int itm=0; itm<nrtms; itm++ )
		{
		    float extrtm = reftm + ssev.extrwin_.atIndex(itm);
		    float val;
		    if ( extrtm <= trg.start )
			val = trc.get( 0, 0 );
		    if ( extrtm >= trg.stop )
			val = trc.get( lastsamp, 0 );
		    else
			val = trc.getValue( extrtm, 0 );
		    sumsq += val * val;
		}
		vals += sqrt( sumsq / nrtms ); // RMS of selected samples
	    }
	}
    }
    synthstatsfld_->dispfld_->setData( vals.arr(), vals.size() );
    synthstatsfld_->avgfld_->setValue(
	    	synthstatsfld_->dispfld_->getRunCalc().average() );
}


bool uiSynthToRealScale::getPolygon( DataSelection& ds ) const
{
    BufferString errmsg;
    const IOObj* polyioobj = polyfld_->isChecked() ? polyfld_->ioobj() : 0;
    ds.polygon_ = polyioobj ?
	PickSetTranslator::getPolygon( *polyioobj, errmsg ) : 0;
    if ( polyioobj && !ds.polygon_ )
	mErrRetBool( errmsg );

    if ( polyioobj )
    {
	TypeSet<Coord3> coords;
	PickSetTranslator::getCoordSet( polyioobj->key().buf(), coords );
	for ( int idx=0; idx<coords.size(); idx++ )
	{
	    const BinID bid = SI().transform( coords[idx] );
	    ds.polyhs_.include( bid );
	}
    }

    return true;
}


bool uiSynthToRealScale::getHorizon( DataSelection& ds, TaskRunner* tr ) const
{
    const MultiID mid = horfld_->key();
    EM::ObjectID emid = EM::EMM().getObjectID( mid );
    PtrMan<Executor> exec = 0;
    if ( emid < 0 )
    {
	EM::SurfaceIOData sd;
	EM::SurfaceIODataSelection sels( sd );
	if ( ds.polyhs_.isDefined() )
	    sels.rg = ds.polyhs_;
	exec = EM::EMM().objectLoader( mid, &sels );
	if ( !exec )
	    mErrRetBool( "Cannot create horizon loader" );
	const bool res = tr ? tr->execute( *exec ) : exec->execute();
	if ( !res )
	    mErrRetBool( "Error loading horizon" );

	emid = EM::EMM().getObjectID( mid ); 
    }

    EM::EMObject* emobj = EM::EMM().getObject( emid );
    if ( emobj ) emobj->ref();
    mDynamicCastGet(EM::Horizon*,hor,emobj);
    ds.setHorizon( hor );
    if ( emobj ) emobj->unRef();
    return hor;
}


bool uiSynthToRealScale::getBinIDs( BinIDValueSet& bvs, 
				    const DataSelection& ds ) const
{
    const EM::SectionID sid = ds.horizon_->sectionID( 0 );
    CubeSampling horcs;
    if ( ds.polygon_ )
	horcs.hrg = ds.polyhs_;

    PtrMan<EM::EMObjectIterator> emiter = 
	    ds.horizon_->createIterator( sid, &horcs );
    while ( true )
    {
	const EM::PosID posid = emiter->next();
	if ( posid.isUdf() ) break;

	const Coord3 crd = ds.horizon_->getPos( posid );
	const BinID bid = SI().transform( crd );
	if ( ds.polygon_ )
	{
	    const Geom::Point2D<float> point( bid.inl, bid.crl );
	    if ( ds.polygon_->isInside( point, true, 0 ) )
		bvs.add( bid, crd.z );
	}
	else
	    bvs.add( bid, crd.z );
    }

    bvs.randomSubselect( synth_.size() );
    return true;
}


void uiSynthToRealScale::updRealStats()
{
    // TODO: Handle is2d_

    uiTaskRunner tr( this );
    DataSelection ds;
    if ( !getPolygon(ds) || !getHorizon(ds,&tr) )
	return;

    MouseCursorChanger cursorchanger( MouseCursor::Wait );
    
    BinIDValueSet bvs( 1, false );
    if ( !getBinIDs(bvs,ds) )
	return;

    const StepInterval<float> window = evfld_->event().extrwin_;
    Seis::SelData* ssd = new Seis::TableSelData( bvs, &window );
    SeisTrcReader rdr( seisfld_->ioobj() );
    rdr.setSelData( ssd );
    if ( !rdr.prepareWork() )
	mErrRet( "Error reading seismic traces" );

    SeisTrcBuf trcbuf( true );
    SeisBufReader bufrdr( rdr, trcbuf );
    if ( !bufrdr.execute() )
	return;
    
    const int windowsz = window.nrSteps();
    TypeSet<float> vals;
    const EM::SectionID sid = ds.horizon_->sectionID( 0 );
    for ( int idx=0; idx<trcbuf.size(); idx++ )
    {
	const SeisTrc& trc = *trcbuf.get( idx );
	float sumsq = 0;
	int nrterms = 0;
	for ( int trcidx=0; trcidx<windowsz; trcidx++ )
	{
	    const BinID bid = trc.info().binid;
	    const float refz = ds.horizon_->getPos( sid, bid.toInt64() ).z;
	    const float val = trc.getValue( refz+window.atIndex(trcidx), 0 );
	    sumsq += val * val;
	    nrterms++;
	}

	if ( nrterms )
	    vals += sqrt( sumsq / nrterms );
    }

    uiHistogramDisplay* histfld = realstatsfld_->dispfld_;
    histfld->setData( vals.arr(), vals.size() );
    histfld->putN();
    realstatsfld_->avgfld_->setValue( histfld->getRunCalc().average() );
}


// uiSynthToRealScale::DataSelection
void uiSynthToRealScale::DataSelection::setHorizon( EM::Horizon* hor )
{
    if ( horizon_ ) horizon_->unRef();
    horizon_ = hor;
    if ( horizon_ ) horizon_->ref();
}


uiSynthToRealScale::DataSelection::~DataSelection()
{
    delete polygon_;
    if ( horizon_ ) horizon_->unRef();
}

