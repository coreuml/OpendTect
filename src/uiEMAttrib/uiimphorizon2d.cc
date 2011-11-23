/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Raman Singh
 Date:          May 2008
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uiimphorizon2d.cc,v 1.33 2011-11-23 11:35:55 cvsbert Exp $";

#include "uiimphorizon2d.h"

#include "uibutton.h"
#include "uicombobox.h"
#include "uiempartserv.h"
#include "uifileinput.h"
#include "uigeninputdlg.h"
#include "uilistbox.h"
#include "uimsg.h"
#include "uiseispartserv.h"
#include "uitaskrunner.h"
#include "uitblimpexpdatasel.h"

#include "binidvalset.h"
#include "emmanager.h"
#include "emsurfacetr.h"
#include "horizon2dscanner.h"
#include "randcolor.h"
#include "seis2dline.h"
#include "strmdata.h"
#include "strmprov.h"
#include "surfaceinfo.h"
#include "survinfo.h"
#include "surv2dgeom.h"
#include "tabledef.h"
#include "file.h"
#include "emhorizon2d.h"

#include <math.h>


class Horizon2DImporter : public Executor
{
public:

Horizon2DImporter( const BufferStringSet& lnms,
		   ObjectSet<EM::Horizon2D>& hors,
		   const MultiID& setid, const BinIDValueSet* valset )
    : Executor("2D Horizon Importer")
    , linenames_(lnms)
    , hors_(hors)
    , setid_(setid)
    , bvalset_(valset)
    , prevlineidx_(-1)
    , nrdone_(0)
{
    const char* lsnm = IOM().get( setid )->name();
    for ( int lineidx=0; lineidx<lnms.size(); lineidx++ )
	geomidset_ += S2DPOS().getGeomID( lsnm, lnms.get(lineidx).buf() );
}


const char* message() const
{ return "Horizon Import"; }

od_int64 totalNr() const
{ return bvalset_ ? bvalset_->totalSize() : 0; }

const char* nrDoneText() const
{ return "Positions written:"; }

od_int64 nrDone() const
{ return nrdone_; }

int nextStep()
{
    if ( !bvalset_ ) return Executor::ErrorOccurred();
    if ( !bvalset_->next(pos_) ) return Executor::Finished();

    BinID bid;
    const int nrvals = bvalset_->nrVals();
    mAllocVarLenArr( float, vals, nrvals )
    for ( int idx=0; idx<nrvals; idx++ )
	vals[idx] = mUdf(float);

    bvalset_->get( pos_, bid, vals );
    if ( bid.inl < 0 ) return Executor::ErrorOccurred();

    const PosInfo::GeomID& geomid = geomidset_[bid.inl];

    if ( bid.inl != prevlineidx_ )
    {
	prevlineidx_ = bid.inl;
	prevtrcnr_ = -1;
	linegeom_.setEmpty();
	PtrMan<IOObj> lsobj = IOM().get( setid_ );
	if ( !lsobj ) return Executor::ErrorOccurred();

	S2DPOS().setCurLineSet( lsobj->name() );
	BufferString linenm = S2DPOS().getLineName( geomid.lineid_ );
	linegeom_.setLineName( linenm );
	if ( !S2DPOS().getGeometry(linegeom_) )
	    return Executor::ErrorOccurred();

	for ( int hdx=0; hdx<hors_.size(); hdx++ )
	{
	    if ( !geomid.isOK() )
	    {
		hors_[hdx]->geometry().removeLine( geomid );
		continue;
	    }

	    hors_[hdx]->geometry().addLine( geomid );
	}
    }

    curtrcnr_ = bid.crl;
    mAllocVarLenArr( float, prevvals, nrvals )
    BinID prevbid;
    bool dointerpol = false;
    if ( prevtrcnr_ >= 0 && abs(curtrcnr_-prevtrcnr_) > 1 )
    {
	dointerpol = true;
	for ( int idx=0; idx<nrvals; idx++ )
	    prevvals[idx] = mUdf(float);

	BinIDValueSet::Pos prevpos = pos_;
	bvalset_->prev( prevpos );
	bvalset_->get( prevpos, prevbid, prevvals );
    }

    Coord coord( 0, 0 );
    for ( int validx=0; validx<nrvals; validx++ )
    {
	if ( validx >= hors_.size() || !hors_[validx] )
	    break;

	const float val = vals[validx];
	if ( mIsUdf(val) || !geomid.isOK() )
	    continue;

	EM::SectionID sid = hors_[validx]->sectionID(0);
	hors_[validx]->setPos( sid, geomid, curtrcnr_,val, false );
	if ( dointerpol && !mIsUdf(prevvals[validx]) )
	    interpolateAndSetVals( validx, geomid, curtrcnr_,
		    		   prevtrcnr_, val, prevvals[validx] );
    }

    prevtrcnr_ = curtrcnr_;
    nrdone_++;
    return Executor::MoreToDo();
}


void interpolateAndSetVals( int hidx, const PosInfo::GeomID& geomid,
			    int curtrcnr, int prevtrcnr,
			    float curval, float prevval )
{
    if ( linegeom_.isEmpty() ) return;

    const int nrpos = abs( curtrcnr - prevtrcnr ) - 1;
    const bool isrev = curtrcnr < prevtrcnr;
    PosInfo::Line2DPos curpos, prevpos;
    if ( !linegeom_.getPos(curtrcnr,curpos)
	|| !linegeom_.getPos(prevtrcnr,prevpos) )
	return;

    const Coord vec = curpos.coord_ - prevpos.coord_;
    for ( int idx=1; idx<=nrpos; idx++ )
    {
	const int trcnr = isrev ? prevtrcnr - idx : prevtrcnr + idx;
	PosInfo::Line2DPos pos;
	if ( !linegeom_.getPos(trcnr,pos) )
	    continue;

	const Coord newvec = pos.coord_ - prevpos.coord_;
	const float sq = vec.sqAbs();
	const float prod = vec.dot(newvec);
	const float factor = mIsZero(sq,mDefEps) ? 0 : prod / sq;
	const float val = prevval + factor * ( curval - prevval );
	hors_[hidx]->setPos( hors_[hidx]->sectionID(0), geomid,trcnr,val,false);
    }
}

protected:

    const BufferStringSet&	linenames_;
    ObjectSet<EM::Horizon2D>&	hors_;
    const MultiID&		setid_;
    const BinIDValueSet*	bvalset_;
    TypeSet<PosInfo::GeomID>	geomidset_;
    PosInfo::Line2DData		linegeom_;
    int				nrdone_;
    int				prevtrcnr_;
    int				curtrcnr_;
    int				prevlineidx_;
    BinIDValueSet::Pos		pos_;
};


uiImportHorizon2D::uiImportHorizon2D( uiParent* p ) 
    : uiDialog(p,uiDialog::Setup("Import 2D Horizon","Specify parameters",
				 "104.0.14"))
    , displayfld_(0)
    , dataselfld_(0)
    , scanner_(0)
    , linesetnms_(*new BufferStringSet)
    , fd_(*EM::Horizon2DAscIO::getDesc())
{
    setCtrlStyle( DoAndStay );

    inpfld_ = new uiFileInput( this, "Input ASCII File", uiFileInput::Setup()
					    .withexamine(true)
					    .forread(true) );
    inpfld_->setSelectMode( uiFileDialog::ExistingFiles );
    inpfld_->valuechanged.notify( mCB(this,uiImportHorizon2D,formatSel) );

    TypeSet<BufferStringSet> linenms;
    uiSeisPartServer::get2DLineInfo( linesetnms_, setids_, linenms );
    uiLabeledComboBox* lsetbox = new uiLabeledComboBox( this, "Select Line Set",
	    						"Line Set Selector" );
    lsetbox->attach( alignedBelow, inpfld_ );
    linesetfld_ = lsetbox->box();
    linesetfld_->addItems( linesetnms_ );
    linesetfld_->selectionChanged.notify( mCB(this,uiImportHorizon2D,setSel) );

    BufferStringSet hornms;
    uiEMPartServer::getAllSurfaceInfo( horinfos_, true );
    for ( int idx=0; idx<horinfos_.size(); idx++ )
	hornms.add( horinfos_[idx]->name );

    uiLabeledListBox* horbox = new uiLabeledListBox( this, hornms,
	   				"Select Horizons to import", true ); 
    horbox->attach( alignedBelow, lsetbox );
    horselfld_ = horbox->box();
    horselfld_->selectionChanged.notify(mCB(this,uiImportHorizon2D,formatSel));

    uiPushButton* addbut = new uiPushButton( this, "Add new",
	    			mCB(this,uiImportHorizon2D,addHor), false );
    addbut->attach( rightTo, horbox );

    dataselfld_ = new uiTableImpDataSel( this, fd_, "104.0.9" );
    dataselfld_->attach( alignedBelow, horbox );
    dataselfld_->descChanged.notify( mCB(this,uiImportHorizon2D,descChg) );

    scanbut_ = new uiPushButton( this, "Scan Input Files",
	    			 mCB(this,uiImportHorizon2D,scanPush), false );
    scanbut_->attach( alignedBelow, dataselfld_);

    postFinalise().notify( mCB(this,uiImportHorizon2D,formatSel) );
}


uiImportHorizon2D::~uiImportHorizon2D()
{
    delete &linesetnms_;
    deepErase( horinfos_ );
}


void uiImportHorizon2D::descChg( CallBacker* cb )
{
    delete scanner_;
    scanner_ = 0;
}


void uiImportHorizon2D::formatSel( CallBacker* cb )
{
    if ( !dataselfld_ ) return;

    BufferStringSet hornms;
    horselfld_->getSelectedItems( hornms );
    const int nrhors = hornms.size();
    EM::Horizon2DAscIO::updateDesc( fd_, hornms );
    dataselfld_->updateSummary();
    dataselfld_->setSensitive( nrhors );
    scanbut_->setSensitive( *inpfld_->fileName() && nrhors );
}


void uiImportHorizon2D::setSel( CallBacker* )
{
    delete scanner_;
    scanner_ = 0;
}


void uiImportHorizon2D::addHor( CallBacker* )
{
    uiGenInputDlg dlg( this, "Add Horizon", "Name", new StringInpSpec() );
    if ( !dlg.go() ) return;

    const char* hornm = dlg.text();
    IOM().to( MultiID(IOObjContext::getStdDirData(IOObjContext::Surf)->id) );
    if ( IOM().getLocal(hornm) )
    {
	uiMSG().error( "Failed to add: a surface already exists with name ",
			hornm );
	return;
    }

    horselfld_->addItem( hornm );
    const int idx = horselfld_->size() - 1;
    horselfld_->setSelected( idx, true );
}


void uiImportHorizon2D::scanPush( CallBacker* cb )
{
    if ( !horselfld_->nrSelected() )
    { uiMSG().error("Please select at least one horizon"); return; }

    if ( !dataselfld_->commit() ) return;

    BufferString msg;
    if ( !EM::Horizon2DAscIO::isFormatOK(fd_, msg) )
    {
	uiMSG().message( msg );
	return;
    }

    if ( scanner_ ) 
    {
	if ( cb )
	    scanner_->launchBrowser();

	return;
    }

    BufferStringSet filenms;
    if ( !getFileNames(filenms) ) return;

    const char* setnm = linesetfld_->text();
    const int setidx = linesetnms_.indexOf( setnm );
    scanner_ = new Horizon2DScanner( filenms, setids_[setidx], fd_ );
    uiTaskRunner taskrunner( this );
    taskrunner.execute( *scanner_ );
    if ( cb )
	scanner_->launchBrowser();
}


bool uiImportHorizon2D::doDisplay() const
{
    return displayfld_ && displayfld_->isChecked();
}


#define mErrRet(s) { uiMSG().error(s); return 0; }
#define mErrRetUnRef(s) { horizon->unRef(); mErrRet(s) }

bool uiImportHorizon2D::doImport()
{
    scanPush(0);
    if ( !scanner_ ) return false;

    const BinIDValueSet* valset = scanner_->getVals();
    if ( !valset || valset->totalSize() == 0 )
    {
	BufferString msg( "No valid positions found\n" );
	msg.add( "Please re-examine input files and format definition" );
	uiMSG().message( msg );
	return false;
    }

    BufferStringSet linenms;
    scanner_->getLineNames( linenms );
    BufferStringSet hornms;
    horselfld_->getSelectedItems( hornms );
    ObjectSet<EM::Horizon2D> horizons;
    IOM().to( MultiID(IOObjContext::getStdDirData(IOObjContext::Surf)->id) );
    EM::EMManager& em = EM::EMM();
    for ( int idx=0; idx<hornms.size(); idx++ )
    {
	BufferString nm = hornms.get( idx );
	PtrMan<IOObj> ioobj = IOM().getLocal( nm );
	PtrMan<Executor> exec = ioobj ? em.objectLoader( ioobj->key() ) : 0;
	EM::ObjectID id = -1;
	if ( !ioobj || !exec || !exec->execute() )
	{
	    id = em.createObject( EM::Horizon2D::typeStr(), nm );
	    mDynamicCastGet(EM::Horizon2D*,hor,em.getObject(id));
	    if ( ioobj ) 
		hor->setMultiID( ioobj->key() );

	    hor->ref();
	    horizons += hor;
	    continue;
	}

	id = em.getObjectID(ioobj->key());
	mDynamicCastGet(EM::Horizon2D*,hor,em.getObject(id));
	if ( !hor )
	    mErrRet("Could not load horizon") 

	for ( int ldx=0; ldx<linenms.size(); ldx++ )
	{
	    BufferString linenm = linenms.get( ldx );
	    if ( hor->geometry().lineIndex(linenm) < 0 )
		continue;

	    BufferString msg = "Horizon ";
	    msg += nm;
	    msg += " already exists for line ";
	    msg += linenm;
	    msg += ". Overwrite?";
	    if ( !uiMSG().askOverwrite(msg) )
		return false;
	}

	hor->ref();
	horizons += hor;
    }

    const char* setnm = linesetfld_->text();
    const int setidx = linesetnms_.indexOf( setnm );
    PtrMan<Horizon2DImporter> exec =
	new Horizon2DImporter( linenms, horizons, setids_[setidx], valset );
    uiTaskRunner impdlg( this );
    if ( !impdlg.execute(*exec) )
	return false;

    for ( int idx=0; idx<horizons.size(); idx++ )
    {
	PtrMan<Executor> saver = horizons[idx]->saver();
	saver->execute();
	horizons[idx]->unRef();
    }
	    					
    return true;
}


bool uiImportHorizon2D::acceptOK( CallBacker* )
{
    if ( !checkInpFlds() ) return false;

    const bool res = doImport();
    if ( res )
	uiMSG().message( "Horizon successfully imported" );

    return false;
}


bool uiImportHorizon2D::getFileNames( BufferStringSet& filenames ) const
{
    if ( !*inpfld_->fileName() )
	mErrRet( "Please select input file(s)" )
    
    inpfld_->getFileNames( filenames );
    for ( int idx=0; idx<filenames.size(); idx++ )
    {
	const char* fnm = filenames[idx]->buf();
	if ( !File::exists(fnm) )
	{
	    BufferString errmsg( "Cannot find input file:\n" );
	    errmsg += fnm;
	    deepErase( filenames );
	    mErrRet( errmsg );
	}
    }

    return true;
}


bool uiImportHorizon2D::checkInpFlds()
{
    BufferStringSet filenames;
    if ( !getFileNames(filenames) ) return false;

    if ( !horselfld_->nrSelected() )
	mErrRet("Please select at least one horizon") 

    if ( !dataselfld_->commit() )
	mErrRet( "Please define data format" );

    return true;
}


