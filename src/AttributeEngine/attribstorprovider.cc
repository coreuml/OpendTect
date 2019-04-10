/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Bert
 * DATE     : Sep 2003
-*/



#include "attribstorprovider.h"

#include "attribdesc.h"
#include "attribfactory.h"
#include "attribparam.h"
#include "attribdataholder.h"
#include "datainpspec.h"
#include "dbman.h"
#include "ioobj.h"
#include "linesubsel.h"
#include "linesetposinfo.h"
#include "seis2ddata.h"
#include "seisbounds.h"
#include "seisbufadapters.h"
#include "seiscbvs.h"
#include "seiscbvs2d.h"
#include "seismscprov.h"
#include "seisdatapack.h"
#include "seisioobjinfo.h"
#include "seispacketinfo.h"
#include "seisprovider.h"
#include "seistrc.h"
#include "seisrangeseldata.h"
#include "survinfo.h"
#include "survgeom2d.h"
#include "posinfo2dsurv.h"


namespace Attrib
{

mAttrDefCreateInstance(StorageProvider)

void StorageProvider::initClass()
{
    mAttrStartInitClassWithUpdate

    desc->addParam( new SeisStorageRefParam(keyStr()) );
    desc->addOutputDataType( Seis::UnknownData );

    mAttrEndInitClass
}


void StorageProvider::updateDesc( Desc& desc )
{
    updateDescAndGetCompNms( desc, 0 );
}


void StorageProvider::updateDescAndGetCompNms( Desc& desc,
					       BufferStringSet* compnms )
{
    if ( compnms )
	compnms->erase();

    const StringPair strpair( desc.getValParam(keyStr())->getStringValue(0) );

    const BufferString storstr = strpair.first();
    if (  storstr.firstChar() == '#' )
    {
	DataPack::FullID fid = DataPack::FullID::getFromString(
							storstr.buf()+1 );
	if ( !DPM(fid).isPresent( fid ) )
	    desc.setErrMsg( uiStrings::phrCannotFind(tr("data in memory")) );
	return;
    }

    const DBKey key = DBKey::getFromStr( storstr );
    uiRetVal uirv;
    Seis::Provider* prov = Seis::Provider::create( key, &uirv );
    if ( !prov )
	{ desc.setErrMsg( uirv ); return; }

    BufferStringSet provcompnms; DataType dtyp;
    prov->getComponentInfo( provcompnms, &dtyp );

    if ( compnms )
	*compnms = provcompnms;

    for ( int idx=0; idx<desc.nrOutputs(); idx++ )
	desc.changeOutputDataType( idx, dtyp );

    //safety, might be removed afterwards but we still use old surveys/cubes
    if ( desc.isSteering() )
    {
	desc.changeOutputDataType( 0, Seis::Dip );
	desc.changeOutputDataType( 1, Seis::Dip );
	if ( compnms && compnms->size()>=2 )
	{
	    compnms->set( 0, new BufferString( "Inline Dip" ) );
	    compnms->set( 1, new BufferString( "Crossline Dip" ) );
	}
    }
}


StorageProvider::StorageProvider( Desc& desc )
    : Provider( desc )
    , mscprov_(0)
    , status_( None )
    , stepoutstep_(-1,0)
    , isondisc_(true)
    , useintertrcdist_(false)
    , ls2ddata_(0)
{
    const StringPair strpair( desc.getValParam(keyStr())->getStringValue(0) );
    const BufferString storstr = strpair.first();
    if ( storstr.firstChar() == '#' )
    {
	DataPack::FullID fid = DataPack::FullID::getFromString(
							storstr.buf()+1 );
	isondisc_ = !DPM(fid).isPresent( fid );
    }
}


StorageProvider::~StorageProvider()
{
    if ( mscprov_ ) delete mscprov_;
    if ( ls2ddata_ ) delete ls2ddata_;
}


#undef mErrRet
#define mErrRet(s) { errmsg_ = s; delete mscprov_; mscprov_= 0; return false; }

bool StorageProvider::checkInpAndParsAtStart()
{
    if ( status_!=None ) return false;

    if ( !isondisc_ )
    {
	storedvolume_.zsamp_.start = 0;	//cover up for synthetics
	DataPack::FullID fid( getDPID() );
	auto stbdtp = DPM(fid).get<SeisTrcBufDataPack>(fid.packID());
	if ( !stbdtp || stbdtp->trcBuf().isEmpty() )
	    return false;

	SeisPacketInfo si;
	stbdtp->trcBuf().fill( si );
	storedvolume_.hsamp_.setIs2D( stbdtp->trcBuf().get(0)->info().is2D() );
	storedvolume_.hsamp_.setInlRange( si.inlrg );
	storedvolume_.hsamp_.setCrlRange( si.crlrg );
	storedvolume_.zsamp_ = si.zrg;
	return true;
    }

    const StringPair strpair( desc_.getValParam(keyStr())->getStringValue(0) );
    const DBKey mid = DBKey::getFromStr( strpair.first() );
    if ( !isOK() )
	return false;

    mscprov_ = new Seis::MSCProvider( mid );
    if ( !initMSCProvider() )
	mErrRet( errmsg_ )

    const bool is2d = mscprov_->is2D();
    desc_.setIs2D( is2d );
    if ( !is2d )
    {
	SeisTrcTranslator::getRanges( mid, storedvolume_, 0 );
	storedvolume_.hsamp_.setIs2D( false );
    }
    else
    {
	storedvolume_.set2DDef();
	storedvolume_.hsamp_.start_.inl() = 0;
	storedvolume_.hsamp_.stop_.inl() = 1;
	storedvolume_.hsamp_.step_.inl() = 1;
	storedvolume_.hsamp_.include( BinID( 0,0 ) );
	storedvolume_.hsamp_.include( BinID( 0, SI().maxNrTraces() ) );
	storedvolume_.hsamp_.step_.crl() = 1; // what else?

	const auto& prov = *mscprov_->provider()->as2D();
	bool isfirst = false;
	for ( int iln=0; iln<prov.nrLines(); iln++ )
	{
	    const auto& lss = prov.lineSubSel( iln );
	    const auto trcrg = lss.trcNrRange();
	    const auto zrg = lss.zRange();

	    const Pos::GeomID geomid = prov.geomID( iln );
	    const auto lnr = geomid.lineNr();

	    if ( !isfirst )
	    {
		storedvolume_.hsamp_.include( BinID(lnr,trcrg.start) );
		storedvolume_.hsamp_.include( BinID(lnr,trcrg.stop) );
		storedvolume_.zsamp_.include( zrg );
	    }
	    else
	    {
		storedvolume_.hsamp_.setLineRange(StepInterval<int>(lnr,lnr,1));
		storedvolume_.hsamp_.setTrcRange( trcrg );
		storedvolume_.zsamp_ = zrg;
		isfirst = false;
	    }
	}
    }

    status_ = StorageOpened;
    return true;
}


int StorageProvider::moveToNextTrace( BinID startpos, bool firstcheck )
{
    if ( alreadymoved_ )
	return 1;

    if ( status_==None && isondisc_ )
	return -1;

    if ( status_==StorageOpened )
    {
	if ( !setMSCProvSelData() )
	    return -1;

	status_ = Ready;
    }

    if ( !useshortcuts_ )
    {
	if ( getDesc().is2D() )
	    prevtrcnr_ = currentbid_.crl();

	bool validstartpos = startpos != BinID(-1,-1);
	if ( validstartpos && curtrcinfo_ && curtrcinfo_->binID() == startpos )
	{
	    alreadymoved_ = true;
	    return 1;
	}
    }

    bool advancefurther = true;
    while ( advancefurther )
    {
	if ( isondisc_ )
	{
	    Seis::MSCProvider::AdvanceState res = mscprov_ ? mscprov_->advance()
					     : Seis::MSCProvider::EndReached;
	    switch ( res )
	    {
		case Seis::MSCProvider::Error:	{ errmsg_ = mscprov_->errMsg();
						      return -1; }
		case Seis::MSCProvider::EndReached:	return 0;
		case Seis::MSCProvider::Buffering:	continue;
						//TODO return 'no new position'

		case Seis::MSCProvider::NewPosition:
		{
		    if ( useshortcuts_ )
			{ advancefurther = false; continue; }

		    SeisTrc* trc = mscprov_->get( 0, 0 );
		    if ( !trc )
			{ pErrMsg("should not happen"); continue; }

		    registerNewPosInfo( trc, startpos, firstcheck,
					advancefurther );
		}
	    }
	}
	else
	{
	    SeisTrc* trc = getTrcFromPack( BinID(0,0), 1 );
	    if ( !trc )
		return 0;

	    status_ = Ready;
	    registerNewPosInfo( trc, startpos, firstcheck, advancefurther );
	}
    }

    setCurrentPosition( currentbid_ );
    alreadymoved_ = true;
    return 1;
}


void StorageProvider::registerNewPosInfo( SeisTrc* trc, const BinID& startpos,
					  bool firstcheck, bool& advancefurther)
{
    for ( int idx=0; idx<trc->nrComponents(); idx++ )
    {
	if ( datachar_.size()<=idx )
	    datachar_ +=
		trc->data().getInterpreter()->dataChar();
	else
	    datachar_[idx] =
		trc->data().getInterpreter()->dataChar();
    }

    curtrcinfo_ = 0;
    const SeisTrcInfo& newti = trc->info();
    currentbid_ = trcinfobid_ = newti.binID();
    if ( firstcheck || startpos == BinID(-1,-1) || currentbid_ == startpos
	    || newti.binID() == startpos )
    {
	advancefurther = false;
	curtrcinfo_ = &trc->info();
    }
}


bool StorageProvider::getLine2DStoredVolume()
{
    if ( mIsUdfGeomID(geomid_) && desiredvolume_->is2D() )
	geomid_ = desiredvolume_->hsamp_.getGeomID();

    if ( mIsUdfGeomID(geomid_) || !mscprov_ || !mscprov_->is2D() )
	return true;

    storedvolume_.hsamp_.setGeomID( geomid_ );

    const auto& prov = *mscprov_->provider()->as2D();
    const auto& lss = prov.lineSubSel( prov.lineNr(geomid_) );
    const auto trcrg = lss.trcNrRange();
    const auto zrg = lss.zRange();

    const auto lnr = geomid_.lineNr();
    storedvolume_.hsamp_.setLineRange( StepInterval<int>(lnr,lnr,1) );
    storedvolume_.hsamp_.setTrcRange( trcrg );
    storedvolume_.zsamp_ = zrg;
    return true;
}


bool StorageProvider::getPossibleVolume( int, TrcKeyZSampling& globpv )
{
    const bool is2d = mscprov_ && mscprov_->is2D();
    if ( !possiblevolume_ )
    {
	if ( is2d && geomid_.isValid() )
	    possiblevolume_ = new TrcKeyZSampling( geomid_ );
	else
	    possiblevolume_ = new TrcKeyZSampling;
    }

    if ( is2d && !getLine2DStoredVolume() )
	return false;

    *possiblevolume_ = storedvolume_;
    if ( is2d == globpv.is2D() )
	globpv.limitTo( *possiblevolume_ );

    const bool issynthetic = possiblevolume_->hsamp_.isSynthetic();
    if ( issynthetic )
	globpv = *possiblevolume_;

    if ( globpv.isEmpty() && !issynthetic )
    {
	setDataUnavailableFlag( true );
	errmsg_ = tr("Stored cube %1 is not available in the desired range")
		  .arg(mscprov_->name());
	return false;
    }

    return true;
}


bool StorageProvider::initMSCProvider()
{
    if ( !mscprov_ || !mscprov_->errMsg().isEmpty() )
    {
	if ( !mscprov_ )
	    errmsg_ = mINTERNAL( "mscprov_ null" );
	else
	    errmsg_ = mscprov_->errMsg();
	return false;
    }

    updateStorageReqs();
    return true;
}


#define setBufStepout( prefix ) \
{ \
    if ( ns.inl() <= prefix##bufferstepout_.inl() \
	    && ns.crl() <= prefix##bufferstepout_.crl() ) \
	return; \
\
    if ( ns.inl() > prefix##bufferstepout_.inl() ) \
	prefix##bufferstepout_.inl() = ns.inl(); \
    if ( ns.crl() > prefix##bufferstepout_.crl() ) \
	prefix##bufferstepout_.crl() = ns.crl();\
}


void StorageProvider::setReqBufStepout( const BinID& ns, bool wait )
{
    setBufStepout(req);
    if ( !wait )
	updateStorageReqs();
}


void StorageProvider::setDesBufStepout( const BinID& ns, bool wait )
{
    setBufStepout(des);
    if ( !wait )
	updateStorageReqs();
}


void StorageProvider::updateStorageReqs( bool )
{
    if ( !mscprov_ ) return;

    mscprov_->setStepout( desbufferstepout_.inl(), desbufferstepout_.crl(),
			  false );
    mscprov_->setStepout( reqbufferstepout_.inl(), reqbufferstepout_.crl(),
			  true );
}


Seis::MSCProvider* StorageProvider::getMSCProvider( bool& needmscprov) const
{
    needmscprov = isondisc_;
    return mscprov_;
}


bool StorageProvider::setMSCProvSelData()
{
    if ( !mscprov_ )
	return false;

    Seis::Provider* prov = mscprov_->provider();
    if ( !prov || prov->isPS() )
	return false;

    const bool is2d = prov->is2D();
    const bool haveseldata = seldata_ && !seldata_->isAll();

    if ( haveseldata && seldata_->type() == Seis::Table )
	return setTableSelData();

    if ( is2d )
	return set2DRangeSelData();

    if ( !desiredvolume_ )
    {
	for ( int idp=0; idp<parents_.size(); idp++ )
	{
	    if ( !parents_[idp] ) continue;

	    if ( parents_[idp]->getDesiredVolume() )
	    {
		setDesiredVolume( *parents_[idp]->getDesiredVolume() );
		break;
	    }
	}
	if ( !desiredvolume_ )
	    return true;
    }

    if ( !checkDesiredVolumeOK() )
	return false;

    TrcKeyZSampling cs;
    cs.hsamp_.start_.inl() =
	desiredvolume_->hsamp_.start_.inl()<storedvolume_.hsamp_.start_.inl() ?
	storedvolume_.hsamp_.start_.inl() : desiredvolume_->hsamp_.start_.inl();
    cs.hsamp_.stop_.inl() =
	desiredvolume_->hsamp_.stop_.inl() > storedvolume_.hsamp_.stop_.inl() ?
	storedvolume_.hsamp_.stop_.inl() : desiredvolume_->hsamp_.stop_.inl();
    cs.hsamp_.stop_.crl() =
	desiredvolume_->hsamp_.stop_.crl() > storedvolume_.hsamp_.stop_.crl() ?
	storedvolume_.hsamp_.stop_.crl() : desiredvolume_->hsamp_.stop_.crl();
    cs.hsamp_.start_.crl() =
	desiredvolume_->hsamp_.start_.crl()<storedvolume_.hsamp_.start_.crl() ?
	storedvolume_.hsamp_.start_.crl() : desiredvolume_->hsamp_.start_.crl();
    cs.zsamp_.start = desiredvolume_->zsamp_.start < storedvolume_.zsamp_.start
		? storedvolume_.zsamp_.start : desiredvolume_->zsamp_.start;
    cs.zsamp_.stop = desiredvolume_->zsamp_.stop > storedvolume_.zsamp_.stop ?
		     storedvolume_.zsamp_.stop : desiredvolume_->zsamp_.stop;

    prov->setSelData( haveseldata ? seldata_->clone()
				   : new Seis::RangeSelData(cs) );

    TypeSet<int> selcomps;
    for ( int idx=0; idx<outputinterest_.size(); idx++ )
	if ( outputinterest_[idx] )
	    selcomps += idx;
    prov->selectComponents( selcomps );

    return true;
}


bool StorageProvider::setTableSelData()
{
    if ( !isondisc_ )
	return false;	//in this case we might not use a table
    if ( !mscprov_ )
	return false;

    Seis::SelData* seldata = seldata_->clone();
    seldata->extendZ( extraz_ );
    Seis::Provider& prov = *mscprov_->provider();
    if ( prov.is2D() && seldata->isRange() )
	seldata->asRange()->setGeomID( geomid_ );

    prov.setSelData( seldata );

    TypeSet<int> selcomps;
    for ( int idx=0; idx<outputinterest_.size(); idx++ )
	if ( outputinterest_[idx] )
	    selcomps += idx;
    prov.selectComponents( selcomps );

    return true;
}


bool StorageProvider::set2DRangeSelData()
{
    if ( !isondisc_ )
	return false;

    mDynamicCastGet(const Seis::RangeSelData*,rsd,seldata_)
    Seis::RangeSelData* seldata = rsd ? rsd->clone()->asRange()
				      : new Seis::RangeSelData;

    Seis::Provider& prov = *mscprov_->provider();
    if ( !prov.is2D() )
	{ pErrMsg("shld be 2D"); return false; }
    const auto& prov2d = *prov.as2D();

    if ( geomid_.isValid() && geomid_.is2D() )
    {
	TrcKeyZSampling tkzs; tkzs.set2DDef();
	seldata->setGeomID( geomid_ );
	const auto& lss = prov2d.lineSubSel( prov2d.lineNr(geomid_) );
	const auto trcrg = lss.trcNrRange();
	const auto dszrg = lss.zRange();

	if ( !checkDesiredTrcRgOK(trcrg,dszrg) )
	    return false;

	StepInterval<int> rg( geomid_.lineNr(), geomid_.lineNr(), 1 );
	tkzs.hsamp_.setLineRange( rg );
	rg.start = desiredvolume_->hsamp_.start_.crl() < trcrg.start?
		    trcrg.start : desiredvolume_->hsamp_.start_.crl();
	rg.stop = desiredvolume_->hsamp_.stop_.crl() > trcrg.stop ?
		    trcrg.stop : desiredvolume_->hsamp_.stop_.crl();
	rg.step = desiredvolume_->hsamp_.step_.crl() > trcrg.step ?
		    desiredvolume_->hsamp_.step_.crl() : trcrg.step;
	tkzs.hsamp_.setTrcRange( rg );
	tkzs.zsamp_.start = desiredvolume_->zsamp_.start < dszrg.start ?
		    dszrg.start : desiredvolume_->zsamp_.start;
	tkzs.zsamp_.stop = desiredvolume_->zsamp_.stop > dszrg.stop ?
		    dszrg.stop : desiredvolume_->zsamp_.stop;

	seldata->set( LineSubSel(tkzs) );
	prov.setSelData( seldata );
    }
    else if ( !rsd && seldata )
	delete seldata;

    return true;
}


bool StorageProvider::checkDesiredVolumeOK()
{
    if ( !desiredvolume_ )
	return true;

    const bool inlwrong =
	desiredvolume_->hsamp_.start_.inl() > storedvolume_.hsamp_.stop_.inl()
     || desiredvolume_->hsamp_.stop_.inl() < storedvolume_.hsamp_.start_.inl();
    const bool crlwrong =
	desiredvolume_->hsamp_.start_.crl() > storedvolume_.hsamp_.stop_.crl()
     || desiredvolume_->hsamp_.stop_.crl() < storedvolume_.hsamp_.start_.crl();
    const float zepsilon = 1e-06f;
    const bool zwrong =
	desiredvolume_->zsamp_.start > storedvolume_.zsamp_.stop+zepsilon ||
	desiredvolume_->zsamp_.stop < storedvolume_.zsamp_.start-zepsilon;
    const float zstepratio =
		desiredvolume_->zsamp_.step / storedvolume_.zsamp_.step;
    const bool zstepwrong = zstepratio > 100 || zstepratio < 0.01;

    if ( !inlwrong && !crlwrong && !zwrong && !zstepwrong )
	return true;

    errmsg_ = tr("'%1' contains no data in selected area:\n")
		.arg( desc_.userRef() );

    if ( inlwrong )
	errmsg_.appendPhrase( tr("Inline range is: %1-%2 [%3]\n")
		      .arg( storedvolume_.hsamp_.start_.inl() )
		      .arg( storedvolume_.hsamp_.stop_.inl() )
		      .arg( storedvolume_.hsamp_.step_.inl() ) );
    if ( crlwrong )
	errmsg_.appendPhrase( tr("Crossline range is: %1-%2 [%3]\n")
		      .arg( storedvolume_.hsamp_.start_.crl() )
		      .arg( storedvolume_.hsamp_.stop_.crl() )
		      .arg( storedvolume_.hsamp_.step_.crl() ) );
    if ( zwrong )
	errmsg_.appendPhrase( tr("Z range is: %1-%2\n")
		      .arg( storedvolume_.zsamp_.start )
		      .arg( storedvolume_.zsamp_.stop ) );
    if ( inlwrong || crlwrong || zwrong )
    {
	setDataUnavailableFlag( true );
	return false;
    }

    if ( zstepwrong )
	errmsg_ = tr("Z-Step is not correct. The maximum resampling "
		     "allowed is a factor 100.\nProbably the data belongs to a"
		     " different Z-Domain");
    return false;
}


bool StorageProvider::checkDesiredTrcRgOK( StepInterval<int> trcrg,
				   StepInterval<float>zrg )
{
    if ( !desiredvolume_ )
    {
	errmsg_ = mINTERNAL("'%1' has no desired volume").arg(desc_.userRef());
	return false;
    }

    const bool trcrgwrong =
	desiredvolume_->hsamp_.start_.crl() > trcrg.stop
     || desiredvolume_->hsamp_.stop_.crl() < trcrg.start;
    const bool zwrong =
	desiredvolume_->zsamp_.start > zrg.stop
     || desiredvolume_->zsamp_.stop < zrg.start;

    if ( !trcrgwrong && !zwrong )
	return true;

    setDataUnavailableFlag( true );
    errmsg_ = tr("'%1' contains no data in selected area:\n")
		.arg( desc_.userRef() );
    if ( trcrgwrong )
	errmsg_.appendPhrase( tr("Trace range is: %1-%2\n")
		      .arg( trcrg.start ).arg( trcrg.stop ) );
    if ( zwrong )
	errmsg_.appendPhrase( tr("Z range is: %1-%2\n")
		      .arg( zrg.start ).arg( zrg.stop ) );
    return false;
}


bool StorageProvider::computeData( const DataHolder& output,
				   const BinID& relpos,
				   int z0, int nrsamples, int threadid ) const
{
    const BinID bidstep = getStepoutStep();
    const SeisTrc* trc = 0;

    if ( isondisc_)
	trc = mscprov_->get( relpos.inl()/bidstep.inl(),
			     relpos.crl()/bidstep.crl() );
    else
	trc = getTrcFromPack( relpos, 0 );

    if ( !trc || !trc->size() )
	return false;

    if ( desc_.is2D() && seldata_ && seldata_->type() == Seis::Table )
    {
	Interval<float> deszrg = desiredvolume_->zsamp_;
	Interval<float> poszrg = possiblevolume_->zsamp_;
	const float desonlyzrgstart = deszrg.start - poszrg.start;
	const float desonlyzrgstop = deszrg.stop - poszrg.stop;
	Interval<float> trcrange = trc->info().sampling_.interval(trc->size());
	const float diffstart = z0*refstep_ - trcrange.start;
	const float diffstop = (z0+nrsamples-1)*refstep_ - trcrange.stop;
	bool isdiffacceptable =
	    ( (mIsEqual(diffstart,0,refstep_/100) || diffstart>0)
	      || diffstart >= desonlyzrgstart )
	 && ( (mIsEqual(diffstop,0,refstep_/100) || diffstop<0 )
	      || diffstop<=desonlyzrgstop );
	if ( !isdiffacceptable )
	    return false;
    }

    return fillDataHolderWithTrc( trc, output );
}


DataPack::FullID StorageProvider::getDPID() const
{
    const StringPair strpair( desc_.getValParam(keyStr())->getStringValue(0) );

    const BufferString storstr = strpair.first();
    if ( storstr.firstChar() != '#' )
	return DataPack::FullID::getInvalid();

    DataPack::FullID fid = DataPack::FullID::getFromString( storstr.buf()+1 );
    return fid;
}


SeisTrc* StorageProvider::getTrcFromPack( const BinID& relpos, int relidx) const
{
    DataPack::FullID fid( getDPID() );
    auto stbdtp = DPM( fid ).get<SeisTrcBufDataPack>( fid.packID() );

    if ( !stbdtp )
	return 0;

    int trcidx = stbdtp->trcBuf().find(currentbid_+relpos, desc_.is2D());
    if ( trcidx+relidx >= stbdtp->trcBuf().size() || trcidx+relidx<0 )
	return 0;

    return stbdtp->trcBuf().get( trcidx + relidx );
}


bool StorageProvider::fillDataHolderWithTrc( const SeisTrc* trc,
					     const DataHolder& data ) const
{
    if ( !trc || data.isEmpty() )
	return false;

    for ( int idx=0; idx<outputinterest_.size(); idx++ )
    {
        if ( outputinterest_[idx] && !data.series(idx) )
	    return false;
    }

    const int z0 = data.z0_;
    float extrazfromsamppos = 0;
    BoolTypeSet isclass( outputinterest_.size(), true );
    if ( needinterp_ )
    {
	ValueSeriesInterpolator<float>& intpol =
	    const_cast<ValueSeriesInterpolator<float>&>(trc->interpolator());
	intpol.udfval_ = mUdf(float);
	checkClassType( trc, isclass );
	extrazfromsamppos = getExtraZFromSampInterval( z0, data.nrsamples_ );
	const_cast<DataHolder&>(data).extrazfromsamppos_ = extrazfromsamppos;
    }

    Interval<float> trcrange = trc->info().sampling_.interval(trc->size());
    trcrange.widen( 0.001f * trc->info().sampling_.step );
    int compidx = -1;
    for ( int idx=0; idx<outputinterest_.size(); idx++ )
    {
	if ( !outputinterest_[idx] )
	    continue;

	ValueSeries<float>* series = const_cast<DataHolder&>(data).series(idx);
	const bool isclss = isclass[idx];
	compidx++;

	const int compnr = desc_.is2D() ? idx : compidx;
	for ( int sampidx=0; sampidx<data.nrsamples_; sampidx++ )
	{
	    const float curt = (float)(z0+sampidx)*refstep_ + extrazfromsamppos;
	    const float val = trcrange.includes(curt,false) ?
		(isclss ? trc->get(trc->nearestSample(curt),compnr)
		  : trc->getValue(curt,compnr)) : mUdf(float);

	    series->setValue( sampidx, val );
	}
    }

    return true;
}


BinDataDesc StorageProvider::getOutputFormat( int output ) const
{
    if ( output>=datachar_.size() )
	return Provider::getOutputFormat( output );

    return datachar_[output];
}


BinID StorageProvider::getStepoutStep() const
{
    if ( stepoutstep_.inl() >= 0 )
	return stepoutstep_;

    BinID& sos = const_cast<StorageProvider*>(this)->stepoutstep_;
    if ( !mscprov_ )
	sos.inl() = sos.crl() = 1;
    else
    {
	const Seis::Provider& prov = *mscprov_->provider();
	if ( prov.is2D() )
	{
	    const auto& prov2d = *prov.as2D();
	    int lnr = mIsUdfGeomID(geomid_) ? 0 : prov2d.lineNr( geomid_ );
	    const auto& lss = prov2d.lineSubSel( lnr );
	    const auto trcrg = lss.trcNrRange();
	    sos.crl() = trcrg.step;
	}
	else
	{
	    const TrcKeyZSampling cs( prov.as3D()->cubeSubSel() );
	    sos = cs.hsamp_.step_;
	}
    }

    return stepoutstep_;
}


void StorageProvider::adjust2DLineStoredVolume()
{
    if ( !isondisc_ || !mscprov_ ) return;

    const Seis::Provider& prov = *mscprov_->provider();
    if ( !prov.is2D() )
	return;

    const auto& prov2d = *prov.as2D();
    int lnr = mIsUdfGeomID(geomid_) ? 0 : prov2d.lineNr( geomid_ );
    const auto& lss = prov2d.lineSubSel( lnr );
    storedvolume_.hsamp_.setTrcRange( lss.trcNrRange() );
    storedvolume_.zsamp_ = lss.zRange();
}


Pos::GeomID StorageProvider::getGeomID() const
{
    return geomid_;
}


void StorageProvider::fillDataPackWithTrc( RegularSeisDataPack* dp ) const
{
    if ( !mscprov_ ) return;
    const SeisTrc* trc = mscprov_->get(0,0);
    if ( !trc ) return;

    Interval<float> trcrange = trc->info().sampling_.interval(trc->size());
    trcrange.widen( 0.001f * trc->info().sampling_.step );
    const BinID bid = trc->info().binID();
    if ( !dp->sampling().hsamp_.includes(bid) )
	return;

    const TrcKeyZSampling& sampling = dp->sampling();
    const int inlidx = sampling.hsamp_.lineRange().nearestIndex( bid.inl() );
    const int crlidx = sampling.hsamp_.trcRange().nearestIndex( bid.crl() );
    int cubeidx = -1;
    for ( int idx=0; idx<outputinterest_.size(); idx++ )
    {
	if ( !outputinterest_[idx] )
	    continue;

	cubeidx++;
	if ( cubeidx>=dp->nrComponents() &&
		!dp->addComponent(OD::EmptyString(),true) )
	    continue;

	const int compnr = desc_.is2D() ? idx : cubeidx;
	for ( int zidx=0; zidx<sampling.nrZ(); zidx++ )
	{
	    const float curt = sampling.zsamp_.atIndex( zidx );
	    if ( !trcrange.includes(curt,false) )
		continue;

	    //the component index inthe trace is depending on outputinterest_,
	    //thus is the same as cubeidx
	    const float val = trc->getValue( curt, compnr );
	    dp->data(cubeidx).set( inlidx, crlidx, zidx, val );
	}
    }
}


void StorageProvider::checkClassType( const SeisTrc* trc,
				      BoolTypeSet& isclass ) const
{
    int idx = 0;
    bool foundneed = true;
    while ( idx<trc->size() && foundneed )
    {
	foundneed = false;
	int compidx = -1;
	for ( int ido=0; ido<outputinterest_.size(); ido++ )
	{
	    if ( outputinterest_[ido] )
	    {
		compidx++;
		if ( isclass[ido] )
		{
		    foundneed = true;
		    const float val  = trc->get( idx, compidx );
		    if ( !holdsClassValue( val) )
			isclass[ido] = false;
		}
	    }
	}
	idx++;
    }
}


bool StorageProvider::compDistBetwTrcsStats( bool force )
{
    if ( !mscprov_ )
	return false;
    if ( ls2ddata_ && ls2ddata_->areStatsComputed() )
	return true;

    const Seis::Provider& prov = *mscprov_->provider();
    if ( !prov.is2D() )
	return false;

    const auto& prov2d = *prov.as2D();

    if ( ls2ddata_ )
	delete ls2ddata_;
    ls2ddata_ = new PosInfo::LineSet2DData();
    for ( int idx=0; idx<prov2d.nrLines(); idx++ )
    {
	const BufferString linenm( prov2d.lineName(idx) );
	PosInfo::Line2DData& linegeom = ls2ddata_->addLine( linenm );
	const auto& geom2d = Survey::Geometry::get2D( prov2d.geomID(idx) );
	if ( geom2d.isEmpty() )
	    continue;

	linegeom = geom2d.data();
	if ( linegeom.positions().isEmpty() )
	{
	    ls2ddata_->removeLine( linenm );
	    return false;
	}
    }

    ls2ddata_->compDistBetwTrcsStats();
    return true;
}


void StorageProvider::getCompNames( BufferStringSet& nms ) const
{
    updateDescAndGetCompNms( desc_, &nms );
}


bool StorageProvider::useInterTrcDist() const
{
    if ( useintertrcdist_ )
	return true;

    const Desc& dsc = getDesc();
    if ( dsc.is2D() && nrOutputs() >=2 )
    {
	if ( dsc.dataType(0) == Seis::Dip && dsc.dataType(1) == Seis::Dip )
	{
	    const_cast<Attrib::StorageProvider*>(this)->useintertrcdist_ = true;
	    return useintertrcdist_;
	}
    }

    return false;
}


float StorageProvider::getDistBetwTrcs( bool ismax, const char* linenm ) const
{
    if ( !ls2ddata_ )
	const_cast<StorageProvider*>(this)->compDistBetwTrcsStats( true );

    return ls2ddata_ ? ls2ddata_->getDistBetwTrcs( ismax, linenm )
		     : mUdf(float);
}

}; // namespace Attrib
