/*+
 * COPYRIGHT: (C) dGB Beheer B.V.
 * AUTHOR   : A.H. Bril
 * DATE     : Apr 2002
-*/

static const char* rcsID = "$Id: seisjobexecprov.cc,v 1.18 2005-08-26 18:19:28 cvsbert Exp $";

#include "seisjobexecprov.h"
#include "seistrctr.h"
#include "seis2dline.h"
#include "seissingtrcproc.h"
#include "jobdescprov.h"
#include "jobrunner.h"
#include "ctxtioobj.h"
#include "cbvsreader.h"
#include "ioman.h"
#include "iostrm.h"
#include "iopar.h"
#include "iodirentry.h"
#include "oddirs.h"
#include "hostdata.h"
#include "filegen.h"
#include "filepath.h"
#include "keystrs.h"
#include "strmprov.h"
#include "ptrman.h"
#include "survinfo.h"
#include "cubesampling.h"
#include <iostream>
#include <sstream>

const char* SeisJobExecProv::sKeySeisOutIDKey = "Output Seismics Key";
const char* SeisJobExecProv::sKeyOutputLS = "Output Line Set";
const char* SeisJobExecProv::sKeyWorkLS = "Work Line Set";

static const char* sKeyProcIs2D = "Processing is 2D";
#define mOutKey(s) IOPar::compKey("Output.1",s)


SeisJobExecProv::SeisJobExecProv( const char* prognm, const IOPar& iniop )
	: progname_(prognm)
    	, iopar_(*new IOPar(iniop))
    	, outioobjpars_(*new IOPar)
    	, ctio_(*new CtxtIOObj(SeisTrcTranslatorGroup::ioContext()) )
    	, nrrunners_(0)
    	, is2d_(false)
    	, outls_(0)
{
    ctio_.ctxt.trglobexpr = "CBVS";
    seisoutkey_ = outputKey( iopar_ );

    const char* res = iopar_.find( seisoutkey_ );
    IOObj* outioobj = IOM().get( res );
    if ( !outioobj )
	errmsg_ = "Cannot find specified output seismic ID";
    else
    {
	seisoutid_ = outioobj->key();
	outioobjpars_ = outioobj->pars();
	is2d_ = SeisTrcTranslator::is2D(*outioobj);
	delete outioobj;
    }
}


SeisJobExecProv::~SeisJobExecProv()
{
    delete outls_;
    delete &iopar_;
    delete &outioobjpars_;
    delete &ctio_;
}


const char* SeisJobExecProv::outputKey( const IOPar& iopar )
{
    static BufferString res;
    res = iopar.find( sKeySeisOutIDKey );
    if ( res == "" ) res = mOutKey("Seismic ID");
    return res.buf();
}


JobDescProv* SeisJobExecProv::mk2DJobProv()
{
    const char* restkey = iopar_.find( sKeyProcIs2D );
    const bool isrestart = restkey && *restkey == 'Y';
    iopar_.set( sKeyProcIs2D, "Yes" );

    BufferStringSet nms;
    const char* lskey = iopar_.find( "LineSet Key" );
    if ( !lskey ) lskey = "Input Seismics.ID";
    lskey = iopar_.find( lskey );
    if ( !lskey ) lskey = iopar_.find( "Attributes.0.Definition" );
    IOObj* ioobj = IOM().get( lskey );
    if ( ioobj && SeisTrcTranslator::is2D(*ioobj) )
    {
	Seis2DLineSet* inpls = new Seis2DLineSet( ioobj->fullUserExpr(true) );
	for ( int idx=0; idx<inpls->nrLines(); idx++ )
	    nms.addIfNew( inpls->lineName(idx) );
	const BufferString attrnm = iopar_.find( "Target value" );

	if ( isrestart )
	{
	    for ( int idx=0; idx<nms.size(); idx++ )
	    {
		LineKey lk( nms.get(idx) );
		lk.setAttrName( attrnm );
		const int lidx = inpls->indexOf( lk );
		if ( lidx >= 0 )
		{
		    Line2DGeometry geom;
		    if ( inpls->getGeometry(lidx,geom) && geom.posns.size()>0 )
		    {
			nms.remove( idx );
			idx--;
		    }
		}
	    }
	    if ( nms.size() < 1 )
	    {
		// Hmm - all already done. Then probably (s)he wants to
		// re-process, possibly with new attrib definition
		for ( int idx=0; idx<inpls->nrLines(); idx++ )
		    nms.addIfNew( inpls->lineName(idx) );
	    }
	}

	// Because we may be going drastically concurrent, we'd better
	// ensure we have the line set ready.
	// This is crucial in the war against NFS attribute caching
	lskey = iopar_.find( "Output.1.Seismic ID" );
	delete outls_; outls_ = inpls;
	if ( lskey )
	{
	    IOObj* outioobj = IOM().get( lskey );
	    if ( outioobj && outioobj->key() != ioobj->key() )
	    {
		outls_ = new Seis2DLineSet( outioobj->fullUserExpr(true) );
		delete outioobj;
	    }
	}
	inpls->addLineKeys( *outls_, attrnm );
	if ( inpls != outls_ )
	    delete inpls;
    }
    delete ioobj;

    BufferString parkey( mOutKey("Line key") );
    KeyReplaceJobDescProv* ret
	= new KeyReplaceJobDescProv( iopar_, parkey, nms );
    ret->objtyp_ = "Line";
    return ret;
}


bool SeisJobExecProv::emitLSFile( const char* fnm ) const
{
    if ( !outls_ ) return false;

    StreamData sd = StreamProvider(fnm).makeOStream();
    if ( !sd.usable() )
	return false;

    outls_->putTo( *sd.ostrm );
    sd.close();
    return !File_isEmpty( fnm );
}


void SeisJobExecProv::preparePreSet( IOPar& iop, const char* reallskey ) const
{
    if ( outls_ )
	outls_->preparePreSet( iop, reallskey );
}


bool SeisJobExecProv::isRestart() const
{
    const char* res = iopar_.find( sKey::TmpStor );
    if ( !res )
	return iopar_.find( sKeyProcIs2D );

    return File_isDirectory(res);
}


JobDescProv* SeisJobExecProv::mk3DJobProv()
{
    const char* res = iopar_.find( sKey::TmpStor );
    if ( !res )
    {
	iopar_.set( sKey::TmpStor, getDefTempStorDir() );
	res = iopar_.find( sKey::TmpStor );
    }
    const bool havetempdir = File_isDirectory(res);

    TypeSet<int> inlnrs;
    TypeSet<int>* ptrnrs = 0;
    BufferString rgkey = iopar_.find( "Inline Range Key" );
    if ( rgkey == "" ) rgkey = mOutKey("In-line range");
    InlineSplitJobDescProv jdp( iopar_, rgkey );
    jdp.getRange( todoinls_ );

    if ( havetempdir )
    {
	getMissingLines( inlnrs, rgkey );
	ptrnrs = &inlnrs;
    }
    else if ( !File_createDir(res,0) )
    {
	errmsg_ = "Cannot create data directory in Temporary storage dir";
	return 0;
    }

    tmpstorid_ = tempStorID();
    if ( tmpstorid_ == "" )
	return 0;

    IOPar jpiopar( iopar_ );
    jpiopar.set( seisoutkey_, tmpstorid_ );
    jpiopar.setYN( mOutKey(sKey::BinIDSel), true );

    return ptrnrs ? new InlineSplitJobDescProv( jpiopar, *ptrnrs, rgkey )
		  : new InlineSplitJobDescProv( jpiopar, rgkey );
}


JobRunner* SeisJobExecProv::getRunner()
{
    if ( is2d_ && nrrunners_ > 0 ) return 0;

    JobDescProv* jdp = is2d_ ? mk2DJobProv() : mk3DJobProv();
    if ( jdp && jdp->nrJobs() == 0 )
    {
	delete jdp; jdp = 0;
	errmsg_ = "No lines to process";
    }

    if ( jdp )
    {
	nrrunners_++;
	return new JobRunner( jdp, progname_ );
    }

    return 0;
}


BufferString SeisJobExecProv::getDefTempStorDir( const char* pth )
{
    const bool havepth = pth && *pth;
    FilePath fp( havepth ? pth : GetDataDir() );
    if ( !havepth )
	fp.add( "Seismics" );

    BufferString stordir = "Proc_";
    stordir += HostData::localHostName();
    stordir += "_";
    stordir += GetPID();

    fp.add( stordir );
    return fp.fullPath();
}


void SeisJobExecProv::getMissingLines( TypeSet<int>& inlnrs,
					 const char* rgkey ) const
{
    FilePath basefp( iopar_.find(sKey::TmpStor) );

    int lastgood = todoinls_.start - todoinls_.step;
    for ( int inl=todoinls_.start; inl<=todoinls_.stop; inl+=todoinls_.step )
    {
	FilePath fp( basefp );
	BufferString fnm( "i." ); fnm += inl;
	fp.add( fnm ); fnm = fp.fullPath();
	StreamData sd = StreamProvider( fnm ).makeIStream();
	bool isok = sd.usable();
	if ( isok )
	{
	    CBVSReader rdr( sd.istrm, false ); // stream closed by reader
	    isok = !rdr.errMsg() || !*rdr.errMsg();
	    if ( isok )
		isok = rdr.info().geom.start.crl || rdr.info().geom.start.crl;
	}
	if ( !isok )
	    inlnrs += inl;
    }
}


MultiID SeisJobExecProv::tempStorID() const
{
    FilePath fp( iopar_.find(sKey::TmpStor) );

    // Is there already an entry?
    IOM().to( ctio_.ctxt.stdSelKey() );
    IODirEntryList el( IOM().dirPtr(), ctio_.ctxt );
    const BufferString fnm( fp.fullPath() );
    for ( int idx=0; idx<el.size(); idx++ )
    {
	const IOObj* ioobj = el[idx]->ioobj;
	if ( !ioobj ) continue;
	mDynamicCastGet(const IOStream*,iostrm,ioobj)
	if ( !iostrm || !iostrm->isMulti() ) continue;

	if ( fnm == iostrm->fileName() )
	    return iostrm->key();
    }

    MultiID ret;
    BufferString objnm( "~" );
    objnm += fp.fileName();
    ctio_.setName( objnm );
    IOM().getEntry( ctio_ );
    if ( !ctio_.ioobj )
	errmsg_ = "Cannot create temporary object for seismics";
    else
    {
	ret = ctio_.ioobj->key();
	ctio_.ioobj->pars() = outioobjpars_;
	mDynamicCastGet(IOStream*,iostrm,ctio_.ioobj)
	fp.add( "i.*" );
	if ( todoinls_.start != todoinls_.stop || todoinls_.start != 0 )
	    iostrm->fileNumbers() = todoinls_;
	else
	{
	    // That cannot be right.
	    StepInterval<int> fnrs;
	    fnrs.start = SI().sampling(false).hrg.start.inl;
	    fnrs.stop = SI().sampling(false).hrg.stop.inl;
	    fnrs.step = SI().sampling(false).hrg.step.inl;
	    iostrm->fileNumbers() = fnrs;
	}
	iostrm->setFileName( fp.fullPath() );
	IOM().commitChanges( *iostrm );
	ctio_.setObj(0);
    }

    return ret;
}


Executor* SeisJobExecProv::getPostProcessor()
{
    if ( is2d_ ) return 0;

    PtrMan<IOObj> inioobj = IOM().get( tmpstorid_ );
    PtrMan<IOObj> outioobj = IOM().get( seisoutid_ );
    return new SeisSingleTraceProc( inioobj, outioobj,
				    "Data transfer", &iopar_,
				    "Writing results to output cube" );
}


bool SeisJobExecProv::removeTempSeis()
{
    if ( is2d_ ) return true;

    PtrMan<IOObj> ioobj = IOM().get( tmpstorid_ );
    if ( !ioobj ) return true;

    FilePath fp( ioobj->fullUserExpr(true) );
    IOM().permRemove( tmpstorid_ );

    if ( fp.fileName() == "i.*" )
	fp.setFileName(0);
    return File_remove(fp.fullPath().buf(),YES);
}
