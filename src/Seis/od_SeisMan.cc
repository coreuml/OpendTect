/*+
 * (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 * AUTHOR   : Bert
 * DATE     : Nov 2018
-*/


#include "serverprogtool.h"
#include "ascbinstream.h"
#include "commandlineparser.h"
#include "ctxtioobj.h"
#include "dbman.h"
#include "dbdir.h"
#include "keystrs.h"
#include "od_iostream.h"
#include "prog.h"
#include "segydirectdef.h"
#include "segyfiledata.h"
#include "segyfiledef.h"
#include "seisprovider.h"
#include "seisstorer.h"
#include "seistrc.h"
#include "survinfo.h"
#include "transl.h"


static const int cProtocolNr = 1;

static const char* sListCubesCmd	= "list-cubes";
static const char* sListLinesCmd	= "list-lines";
static const char* sListPS3DCmd		= "list-ps3d";
static const char* sListPS2DCmd		= "list-ps2d";
static const char* sListAttribs2DCmd	= "list-2d-attributes";
static const char* sWriteCubeCmd	= "write-cube";
static const char* sWriteLineCmd	= "write-line";
static const char* sWritePS3DCmd	= "write-ps3d";
static const char* sWritePS2DCmd	= "write-ps2d";
static const char* sWriteSEGYDefCmd	= "write-segydef";
static const char* sAsciiCmd		= "ascii";
static const char* sTypeCmd		= "type";
static const char* sFormatCmd		= "format";
static const char* sEncodingCmd		= "encoding";
static const char* sGeometryCmd		= "geometry";
static const char* sNrFilesCmd		= "nrfiles";
static const char* sFileNameCmd		= "filename";
static const char* sFileNamesCmd	= "filenames";


class SeisServerTool : public ServerProgTool
{
public:

    mUseType( Pos,	GeomID );
    mUseType( Seis,	GeomType );
    mUseType( Seis,	Provider );
    mUseType( Seis,	Storer );

			SeisServerTool(int,char**);

    void		listCubes();
    void		listLines(const char* attr);
    void		listPS2D();
    void		listPS3D();
    void		listAttribs2D();
    void		writeCube(const char*);
    void		writeLine(const char*);
    void		writePS3D(const char*);
    void		writePS2D(const char*);
    void		writeSEGYDef(const char*);

protected:

    BufferString	getSpecificUsage() const override;

    IOObjContext*	ctxt_	    = nullptr;
    Storer*		storer_	    = nullptr;
    bool		ascii_	    = false;
    BufferString	translname_;
    BufferString	typetag_;
    int			encoding_   = 0;
    GeomID		geomid_;

    void		getCtxt(GeomType,bool forread=true);
    void		listObjs(GeomType);
    BufferString	getObjName(const char*);
    void		writeObj(GeomType,const char*);
    void		finishWrite();
    void		writeData(GeomType);
    int			getTranslIdx(const char*) const;
    GeomType		getGeomTypeFromCL() const;
    bool		getFileNamesFromCL(BufferStringSet&) const;
    void		slurpSEGYIndexingData(GeomType,SEGY::FileDataSet&);

};


SeisServerTool::SeisServerTool( int argc, char** argv )
    : ServerProgTool(argc,argv,"Seis")
{
    initParsing( cProtocolNr );
    ascii_ = clp().hasKey( sAsciiCmd );
}


void SeisServerTool::getCtxt( GeomType gt, bool forread )
{
    ctxt_ = Seis::getIOObjContext( gt, forread );
}


BufferString SeisServerTool::getObjName( const char* ky )
{
    return getKeyedArgStr( ky, true );
}


void SeisServerTool::listObjs( GeomType gt )
{
    getCtxt( gt );

    DBKeySet ids;
    BufferStringSet nms;
    BufferStringSet types;
    DBDirEntryList del( *ctxt_ );

    for ( int idx=0; idx<del.size(); idx++ )
    {
	ids.add( del.key(idx) );
	nms.add( del.name(idx) );
	types.add( del.ioobj(idx).pars().find(sKey::Type()) );
    }

    setSize( ids.size() );
    set( sKey::ID(mPlural), ids );
    set( sKey::Name(mPlural), nms );
    set( sKey::Type(mPlural), types );
    respondInfo( true );
}


void SeisServerTool::listCubes()
{
    listObjs( Seis::Vol );
}


void SeisServerTool::listLines( const char* attr )
{
    respondError( "TODO" );
}


void SeisServerTool::listPS3D()
{
    listObjs( Seis::VolPS );
}


void SeisServerTool::listPS2D()
{
    listObjs( Seis::LinePS );
}


void SeisServerTool::listAttribs2D()
{
    listObjs( Seis::Line );
}


int SeisServerTool::getTranslIdx( const char* trnm ) const
{
    int ret = -1;
    const Translator* transl = ctxt_->trgroup_->getTemplate( trnm, true );
    if ( transl )
	ret = ctxt_->trgroup_->templates().indexOf( transl );
    return ret;
}


void SeisServerTool::writeObj( GeomType gt, const char* cmd )
{
    getCtxt( gt, false );

    int translidx = -1;
    bool issegy = false;
    if ( clp().hasKey(sFormatCmd) )
    {
	clp().setKeyHasValue( sFormatCmd );
	BufferString trnm;
	clp().getVal( sFormatCmd, trnm );
	issegy = trnm.startsWith( "SEG" );
	translidx = getTranslIdx( trnm );
    }

    ctxt_->setName( getObjName(cmd) );
    CtxtIOObj ctio( *ctxt_ );
    auto res = ctio.fillObj( false, translidx );
    if ( !res )
	respondError( "Cannot create entry in Data Store" );

    bool ioobjchgd = false;
    if ( clp().hasKey(sTypeCmd) )
    {
	clp().setKeyHasValue( sTypeCmd );
	BufferString typetag;
	clp().getVal( sTypeCmd, typetag );
	if ( !typetag.isEmpty() )
	{
	    ctio.ioobj_->pars().set( sKey::Type(), typetag );
	    ioobjchgd = true;
	}
    }

    if ( clp().hasKey(sEncodingCmd) )
    {
	clp().setKeyHasValue( sEncodingCmd );
	int encnr = 0;
	clp().getVal( sEncodingCmd, encnr );
	if ( encnr > 0 )
	{
	    if ( issegy )
		ctio.ioobj_->pars().set( "Number format", encnr );
	    else
		DataCharacteristics::putUserTypeToPar( ctio.ioobj_->pars(),
				(DataCharacteristics::UserType)encnr );
	    ioobjchgd = true;
	}
    }

    if ( ioobjchgd )
	DBM().setEntry( *ctio.ioobj_ );

    storer_ = new Storer( *ctio.ioobj_ );
    ctio.setObj( nullptr );

    writeData( gt );
}


void SeisServerTool::finishWrite()
{
    const uiRetVal uirv = storer_->close();
    if ( !uirv.isOK() )
	respondError( toString(uirv) );
    else
	respondInfo( true );
}


void SeisServerTool::writeData( GeomType gt )
{
    respondInfo( true, false );

    ascbinistream strm( inStream(), !ascii_ );
    SeisTrc trc; SeisTrcInfo& ti = trc.info();
    int nrsamps;
    strm.get( ti.sampling_.start ).get( ti.sampling_.step ).get( nrsamps );
    if ( !strm.isOK() )
	respondError( "Failed to initialise seimic data slurp" );
    if ( SI().zIsTime() )
    {
	ti.sampling_.start *= 0.001f;
	ti.sampling_.step *= 0.001f;
    }

    trc.setSize( nrsamps );
    const bool is2d = Seis::is2D( gt );
    const bool isps = Seis::isPS( gt );
    int inl=0, crl=0, trcnr=0;

    while ( true )
    {
	if ( is2d )
	    strm.get( trcnr );
	else
	    strm.get( inl );

	if ( (is2d && trcnr==0) || (!is2d && inl==0) )
	    finishWrite();

	if ( !is2d )
	    strm.get( crl );
	else if ( !isps )
	    strm.get( ti.refnr_ ).get( ti.coord_.x_ ).get( ti.coord_.y_ );

	if ( isps )
	    strm.get( ti.offset_ ).get( ti.azimuth_ );

	if ( is2d )
	    ti.setPos( geomid_, trcnr );
	else
	    ti.setPos( BinID(inl,crl) );

	strm.getArr( trc.data().arr<float>(), nrsamps );
	if ( strm.isBad() )
	    finishWrite();

	const uiRetVal uirv = storer_->put( trc );
	if ( !uirv.isOK() )
	    respondError( toString(uirv) );
    }
}


void SeisServerTool::writeCube( const char* cmd )
{
    writeObj( Seis::Vol, cmd );
}


void SeisServerTool::writeLine( const char* cmd )
{
    // create entry in geometry manager first, obtaining geomid_
    respondError( "TODO" );
}


void SeisServerTool::writePS3D( const char* cmd )
{
    writeObj( Seis::VolPS, cmd );
}


void SeisServerTool::writePS2D( const char* cmd )
{
    writeObj( Seis::LinePS, cmd );
}


Seis::GeomType SeisServerTool::getGeomTypeFromCL() const
{
    clp().setKeyHasValue( sGeometryCmd );
    BufferString gtstr;
    clp().getVal( sGeometryCmd, gtstr );
    if ( gtstr.isEqual("2d",CaseInsensitive) )
	return Seis::Line;
    else if ( gtstr.isEqual("ps3d",CaseInsensitive) )
	return Seis::VolPS;
    else if ( gtstr.isEqual("ps2d",CaseInsensitive) )
	return Seis::LinePS;
    return Seis::Vol;
}


bool SeisServerTool::getFileNamesFromCL( BufferStringSet& fnms ) const
{
    clp().setKeyHasValue( sNrFilesCmd );
    BufferString fnm;
    if ( clp().hasKey(sFileNameCmd) )
    {
	clp().setKeyHasValue( sFileNameCmd );
	clp().getVal( sFileNameCmd, fnm );
	fnms.add( fnm );
    }
    else if ( clp().hasKey(sFileNamesCmd) )
    {
	clp().setKeyHasValue( sNrFilesCmd );
	int nrfiles = 1;
	clp().getVal( sFileNameCmd, nrfiles );
	clp().setKeyHasValue( sFileNamesCmd, nrfiles );
	for ( int idx=0; idx<nrfiles; idx++ )
	{
	    clp().getVal( sFileNamesCmd, fnm, false, idx );
	    fnms.add( fnm );
	}
    }
    return !fnms.isEmpty();
}


void SeisServerTool::writeSEGYDef( const char* cmd )
{
    const GeomType gt = getGeomTypeFromCL();
    getCtxt( gt, false );
    ctxt_->setName( getObjName(cmd) );
    CtxtIOObj ctio( *ctxt_ );
    const int translidx = getTranslIdx( "SEGYDirect" );
    auto res = ctio.fillObj( false, translidx );
    if ( !res )
	respondError( "Cannot create SEGYDirect entry in Data Store" );

    IOPar segypars;
    if ( clp().hasKey("format") )
    {
	int fmt = 1;
	clp().setKeyHasValue( "format" );
	clp().getVal( "format", fmt );
	segypars.set( SEGY::FilePars::sKeyNumberFormat(), fmt );
    }

    SEGY::FileDataSet fds( segypars );
    BufferStringSet fnms;
    if ( !getFileNamesFromCL(fnms) )
	respondError( "Please specify the SEG-Y file name(s)" );
    for ( int idx=0; idx<fnms.size(); idx++ )
	fds.addFile( fnms.get(idx) );

    // ns // z0 // dz

    SEGY::DirectDef def;
    def.setData( fds );
    const BufferString fnm( ctio.ioobj_->mainFileName() );
    if ( !def.writeHeadersToFile(fnm) )
	respondError( BufferString("Cannot open SEGYDirect data file ",fnm) );

    fds.setOutputStream( *def.getOutputStream() );
    respondInfo( true, false );
    slurpSEGYIndexingData( gt, fds );

    if ( !def.writeFootersToFile() )
	respondError( BufferString("Cannot finish SEGYDirect data file ",fnm) );

    respondInfo( true );
}


void SeisServerTool::slurpSEGYIndexingData( GeomType gt, SEGY::FileDataSet& fds)
{
    ascbinistream strm( inStream(), !ascii_ );

    int fileidx;
    od_int64 fileoffs;
    Seis::PosKey pk;
    Coord coord;
    const bool is2d = Seis::is2D( gt );
    const bool isps = Seis::isPS( gt );

    while ( true )
    {
	strm.get( fileidx );
	if ( fileidx < 0 )
	    return;

	strm.get( fileoffs );
	if ( is2d )
	    strm.get( pk.trcNr() );
	else
	    strm.get( pk.binID().inl() ).get( pk.binID().crl() );

	if ( isps )
	    strm.get( pk.offset() );

	const bool res = fds.addTrace( fileidx, pk, coord, true );
	if ( !res )
	    respondError( "Error during data transfer" );
    }
}


BufferString SeisServerTool::getSpecificUsage() const
{
    BufferString ret;
    addToUsageStr( ret, sListCubesCmd, "" );
    addToUsageStr( ret, sListLinesCmd, "[attrib_name]" );
    addToUsageStr( ret, sListPS3DCmd, "" );
    addToUsageStr( ret, sListPS2DCmd, "" );
    addToUsageStr( ret, sListAttribs2DCmd, "" );
#   define mAddWriteCmdToUsage( cmd ) \
	addToUsageStr( ret, cmd, \
	    "name [--ascii] [--format fmt] [--encoding nr] [--type tag]" )
    mAddWriteCmdToUsage( sWriteCubeCmd );
    mAddWriteCmdToUsage( sWriteLineCmd );
    mAddWriteCmdToUsage( sWritePS3DCmd );
    mAddWriteCmdToUsage( sWritePS2DCmd );
    addToUsageStr( ret, sWriteSEGYDefCmd,
		    "name --geometry 3D|2D|PS3D|PS3D more_args" );
    return ret;
}


int main( int argc, char** argv )
{
    SeisServerTool st( argc, argv );
    auto& clp = st.clp();

    if ( clp.hasKey(sListCubesCmd) )
	st.listCubes();
    else if ( clp.hasKey(sListPS3DCmd) )
	st.listPS3D();
    else if ( clp.hasKey(sListPS2DCmd) )
	st.listPS2D();
    else if ( clp.hasKey(sListAttribs2DCmd) )
	st.listAttribs2D();
    else if ( clp.hasKey(sListLinesCmd) )
    {
	BufferStringSet normargs;
	clp.getNormalArguments( normargs );
	BufferString attrnm( "Seis" );
	if ( !normargs.isEmpty() )
	    attrnm.set( normargs.get(0) );
	st.listLines( attrnm );
    }
    else if ( clp.hasKey(sWriteCubeCmd) )
	st.writeCube( sWriteCubeCmd );
    else if ( clp.hasKey(sWriteLineCmd) )
	st.writeLine( sWriteLineCmd );
    else if ( clp.hasKey(sWritePS3DCmd) )
	st.writePS3D( sWritePS3DCmd );
    else if ( clp.hasKey(sWritePS2DCmd) )
	st.writePS2D( sWritePS2DCmd );
    else if ( clp.hasKey(sWriteSEGYDefCmd) )
	st.writeSEGYDef( sWriteSEGYDefCmd );

    pFreeFnErrMsg( "Should not reach" );
    return ExitProgram( 0 );
}
