/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	A. Huck
 Date:		May 2019
________________________________________________________________________

-*/

#include "pythonaccess.h"

#include "ascstream.h"
#include "bufstringset.h"
#include "dirlist.h"
#include "envvars.h"
#include "file.h"
#include "genc.h"
#include "keystrs.h"
#include "oddirs.h"
#include "oscommand.h"
#include "separstr.h"
#include "settings.h"
#include "string2.h"
#include "timefun.h"
#include "timer.h"
#include "uistrings.h"
#include "odplatform.h"

const char* OD::PythonAccess::sKeyPythonSrc() { return "Python Source"; }
const char* OD::PythonAccess::sKeyEnviron() { return "Environment"; }

#define mFileRetentionTimeInMilliSec	60000
#define mDelCycleTym			mFileRetentionTimeInMilliSec*5

OD::PythonAccess& OD::PythA()
{
    static PtrMan<OD::PythonAccess> theinst = nullptr;
    return *theinst.createIfNull();
}


const char* OD::PythonAccess::sPythonExecNm( bool v3, bool v2 )
{
#ifdef __win__
    return "python.exe";
#else
    if (v3)
	return "python3";
    else if (v2)
	return "python2";
    else
	return "python";
#endif
}


mDefineNameSpaceEnumUtils(OD,PythonSource,"Python Source")
{
    "Internal", "System", "Custom", 0
};
template <>
void EnumDefImpl<OD::PythonSource>::init()
{
    uistrings_ += tr("Internal");
    uistrings_ += tr("System");
    uistrings_ += tr("Custom");
}



OD::PythonAccess::PythonAccess()
    : envChange(this)
{
}



OD::PythonAccess::~PythonAccess()
{
    detachAllNotifiers();
    delete activatefp_;
}


BufferString OD::PythonAccess::pyVersion() const
{
    if ( pythversion_.isEmpty() )
    {
	PythonAccess& acc = const_cast<PythonAccess&>( *this );
	acc.retrievePythonVersionStr();
    }

    return pythversion_;
}


uiString OD::PythonAccess::pySummary() const
{
    BufferString pythonstr( sKey::Python() ); pythonstr.toLower();
    const IOPar& pythonsetts = Settings::fetch( pythonstr );
    PythonSource source;
    if (!PythonSourceDef().parse(pythonsetts,sKeyPythonSrc(),source))
	source = System;

    uiStringSet result;
    result += uiStrings::sUsing();
    result += uiString(tr("%1 %2").arg(source).arg(pyVersion()));

    if ( source == Custom )
    {
	BufferString virtenvloc;
	if ( pythonsetts.get(sKeyEnviron(),virtenvloc) )
	{
	    if ( virtenvnm_.isEmpty() )
		result += uiString(tr("from %1").arg(virtenvloc));
	    else
		result += uiString(tr("environment %1 in %2").arg(virtenvnm_)
							    .arg(virtenvloc));
	}
    }

    return result.cat( uiString::Space, uiString::OnSameLine );
}


bool OD::PythonAccess::isUsable( bool force, const char* scriptstr,
				 const char* scriptexpectedout ) const
{
    if ( !force )
	return isusable_;

    OD::PythonAccess& pytha = const_cast<OD::PythonAccess&>( *this );
    return pytha.isUsable( force, scriptstr, scriptexpectedout );
}


bool OD::PythonAccess::isUsable( bool force, const char* scriptstr,
				 const char* scriptexpectedout )
{
    static bool force_external = GetEnvVarYN( "OD_FORCE_PYTHON_ENV_OK" );
    if ( force_external )
	return (isusable_ = istested_ = true);
    if ( !force )
	return isusable_;

    istested_ = true;
    isusable_ = false;
    BufferString pythonstr( sKey::Python() ); pythonstr.toLower();
    const IOPar& pythonsetts = Settings::fetch( pythonstr );
    PythonSource source;
    const bool usesystem =
	!PythonSourceDef().parse(pythonsetts,sKeyPythonSrc(),source) ||
	source == System;

    if ( usesystem )
	return isEnvUsable(nullptr,nullptr,scriptstr,scriptexpectedout);

    PtrMan<File::Path> externalroot = nullptr;
    PtrMan<BufferString> virtenvnm;
    if ( source == Custom )
    {
	BufferString virtenvloc;
	if ( pythonsetts.get(sKeyEnviron(),virtenvloc) )
	    externalroot = new File::Path( virtenvloc );
	virtenvnm = new BufferString();
	pythonsetts.get( sKey::Name(), *virtenvnm );
    }

    ManagedObjectSet<File::Path> pythonenvsfp;
    BufferStringSet envnms;
    if ( !getSortedVirtualEnvironmentLoc(pythonenvsfp,envnms,virtenvnm,
					 externalroot) )
	return false;

    for ( int idx=0; idx<pythonenvsfp.size(); idx++ )
    {
	const File::Path* pythonenvfp = pythonenvsfp.get( idx );
	const BufferString& envnm = envnms.get( idx );
	if ( isEnvUsable(pythonenvfp,envnm.buf(),scriptstr,scriptexpectedout) )
	    return true;
    }

    return false;
}


File::Path* OD::PythonAccess::getActivateScript( const File::Path& rootfp )
{
    File::Path ret( rootfp.fullPath(), "bin" );
    if ( !ret.exists() )
    {
	ret.set( rootfp.fullPath() ).add( "condabin" );
	if ( !ret.exists() )
	    return nullptr;
    }
    ret.add( "activate" );
#ifdef __win__
    ret.setExtension( "bat" );
#endif

    return ret.exists() ? new File::Path( ret ) : nullptr;
}


bool OD::PythonAccess::isEnvUsable( const File::Path* pythonenvfp,
				    const char* envnm,
				    const char* scriptstr,
				    const char* scriptexpectedout )
{
    PtrMan<File::Path> activatefp;
    BufferString venvnm( envnm );
    if ( pythonenvfp )
    {
	if ( !pythonenvfp->exists() )
	    return false;

	activatefp = getActivateScript( File::Path(pythonenvfp->fullPath()) );
	if ( !activatefp )
	    return false;
    }

    OS::MachineCommand cmd( sPythonExecNm(true) );
    const bool doscript = scriptstr && *scriptstr;
    if ( doscript )
	cmd.addArg( "-c" ).addArg( scriptstr );
    else
	cmd.addFlag( "version" );

    bool res = doExecute( cmd, nullptr, nullptr, activatefp.ptr(), venvnm );
    if ( !res )
	return false;

    const bool testscriptout = scriptexpectedout && *scriptexpectedout;
    res = doscript ?  (testscriptout ? laststdout_ ==
				       FixedString(scriptexpectedout)
				     : res)
		   : !laststdout_.isEmpty() || !laststderr_.isEmpty();
    if ( !res )
	return false;

    bool notrigger;
    if ( pythonenvfp )
    {
	notrigger = activatefp_ &&
		    activatefp_->fullPath() == activatefp->fullPath() &&
		    virtenvnm_ == venvnm;
	delete activatefp_;
	activatefp_ = new File::Path( *activatefp );
	virtenvnm_.set( venvnm );
    }
    else
    {
	notrigger = !activatefp_ && virtenvnm_.isEmpty() && venvnm.isEmpty();
	deleteAndZeroPtr( activatefp_ );
	virtenvnm_.setEmpty();
    }

    msg_.setEmpty();
    isusable_ = true;
    if ( !notrigger )
	envChangeCB(nullptr);

    return isusable_;
}


bool OD::PythonAccess::execute( const OS::MachineCommand& cmd,
				bool wait4finish ) const
{
    OS::CommandExecPars execpars( wait4finish ? OS::Wait4Finish : OS::RunInBG );
    execpars.createstreams_ = true;
    execpars.prioritylevel_ = 0.f;
    return execute( cmd, execpars );
}


bool OD::PythonAccess::execute( const OS::MachineCommand& cmd,
				BufferString& stdoutstr,
				BufferString* stderrstr,
				uiString* errmsg ) const
{
    if ( !isUsable(!istested_) )
	return false;

    const bool res = doExecute( cmd, nullptr, nullptr, activatefp_,
				virtenvnm_.buf() );
    if ( &stdoutstr != &laststdout_ )
	stdoutstr = laststdout_;
    if ( stderrstr && &laststderr_ != stderrstr )
	stderrstr->set( laststderr_ );
    if ( errmsg )
	errmsg->set( msg_ );

    return res;
}


bool OD::PythonAccess::execute( const OS::MachineCommand& cmd,
				const OS::CommandExecPars& pars,
				int* pid, uiString* errmsg ) const
{
    if ( !isUsable(!istested_) )
	return false;

    const bool res = doExecute( cmd, &pars, pid, activatefp_, virtenvnm_.buf());
    if ( errmsg )
	errmsg->set( msg_ );

    return res;
}


BufferString OD::PythonAccess::lastOutput( bool stderrout, uiString* msg ) const
{
    if ( cl_.ptr() )
    {
	OS::CommandLauncher* cl = cl_.ptr();
	if ( msg && !cl->errorMsg().isEmpty() )
	    msg->set( cl->errorMsg() );

	BufferString ret;
	if ( stderrout )
	{
	    if ( cl->getStdError() )
	    {
		cl->getStdError()->getAll( ret );
		if ( ret.isEmpty() && !laststderr_.isEmpty() )
		    ret = laststderr_;
	    }
	    else
		ret = laststderr_;
	}
	else
	{
	    if ( cl->getStdOutput() )
	    {
		cl->getStdOutput()->getAll( ret );
		if ( ret.isEmpty() && !laststdout_.isEmpty() )
		    ret = laststdout_;
	    }
	    else
		ret = laststdout_;
	}

	return ret;
    }

    if ( msg )
	msg->set( msg_ );
    return stderrout ? laststderr_ : laststdout_;
}


bool OD::PythonAccess::isModuleUsable( const char* nm ) const
{
    OS::MachineCommand cmd( sPythonExecNm(true) );
    cmd.addArg( "-c" ).addArg( BufferString("\"import ",nm,"\"") );
    return execute( cmd ) && lastOutput(true,nullptr).isEmpty();
}


BufferString OD::PythonAccess::getDataTypeStr( OD::DataRepType typ )
{
    BufferString ret;
    if ( typ == OD::F32 )
	ret.set( "float32" );
    else if ( typ == OD::F64 )
	ret.set( "float64" );
    else if ( typ == OD::SI8 )
	ret.set( "int8" );
    else if ( typ == OD::UI8 )
	ret.set( "uint8" );
    else if ( typ == OD::SI16 )
	ret.set( "int16" );
    else if ( typ == OD::UI16 )
	ret.set( "uint16" );
    else if ( typ == OD::SI32 )
	ret.set( "int32" );
    else if ( typ == OD::UI32 )
	ret.set( "uint32" );
    else if ( typ == OD::SI64 )
	ret.set( "int64" );
/*    else if ( typ == OD::UI64 )
	ret.set( "uint64" );*/

    return ret;
}


OD::DataRepType OD::PythonAccess::getDataType( const char* str )
{
    OD::DataRepType ret = OD::AutoDataRep;
    const FixedString typestr( str );
    if ( typestr == "float32" )
	ret = OD::F32;
    else if ( typestr == "float64" )
	ret = OD::F64;
    else if ( typestr == "int8" )
	ret = OD::SI8;
    else if ( typestr == "uint8" )
	ret = OD::UI8;
    else if ( typestr == "int16" )
	ret = OD::SI16;
    else if ( typestr == "uint16" )
	ret = OD::UI16;
    else if ( typestr == "int32" )
	ret = OD::SI32;
    else if ( typestr == "uint32" )
	ret = OD::UI32;
    else if ( typestr == "int64" )
	ret = OD::SI64;
    else if ( typestr == "uint64" )
	ret = OD::SI64;

    return ret;
}


static BufferString getPIDFilePathStr( const File::Path& scriptfp )
{
    File::Path ret( scriptfp );
    ret.setExtension( "pid" );
    return ret.fullPath();
}


File::Path* OD::PythonAccess::getCommand( OS::MachineCommand& cmd,
					  bool background,
					  const File::Path* activatefp,
					  const char* envnm )
{
    if ( !activatefp || !envnm )
    {
	const OS::MachineCommand cmdret( cmd, true );
	cmd = cmdret;
	return nullptr;
    }

    File::Path* ret = new File::Path(
			  File::Path::getTempFullPath("runpython",nullptr) );
    if ( !ret )
	return nullptr;
#ifdef __win__
    ret->setExtension( "bat" );
#else
    ret->setExtension( "sh" );
#endif
    od_ostream strm( *ret );
    if ( !strm.isOK() )
    {
	delete ret;
	return nullptr;
    }

#ifdef __win__
    strm.add( "@SETLOCAL" ).add( od_newline );
    strm.add( "@ECHO OFF" ).add( od_newline ).add( od_newline );
    if ( background )
    {
	strm.add( "SET procnm=%~n0" ).add( od_newline );
	strm.add( "SET proctitle=%procnm%_children" ).add( od_newline );
	strm.add( "SET pidfile=\"%~dpn0.pid\"" ).add( od_newline ).add( od_newline);
    }
    strm.add( "@CALL \"" );
#else
    strm.add( "#!/bin/bash" ).add( od_newline ).add( od_newline );
    strm.add( "source " );
#endif
    strm.add( activatefp->fullPath() );
#ifdef __win__
    strm.add( "\"" );
#endif
    if ( envnm )
	strm.add( " " ).add( envnm ).add( od_newline );
#ifdef __win__
    if ( background )
	strm.add( "Start \"%proctitle%\" /MIN " );
#endif
    strm.add( cmd.program() ).add( " " )
	.add( cmd.args().cat(" ") );
    if ( background )
    {
#ifdef __win__
	strm.add( od_newline );
	strm.add( "timeout 2 > nul" ).add( od_newline );
	strm.add( "FOR /F \"tokens=* USEBACKQ\" %%g IN (`tasklist /FI" );
	strm.add( " \"WINDOWTITLE eq %proctitle%\" /FO CSV /NH`) DO " );
	strm.add( "(SET \"PROCRET=%%g\")" ).add( od_newline );
	strm.add( "FOR /F \"tokens=2 delims=,\" %%g IN (\"%PROCRET%\") DO (" );
	strm.add( "SET \"PIDRET=%%g\")" ).add( od_newline );
	strm.add( "ECHO %PIDRET:\"=% > %PIDFILE%" );
#else
	strm.add( " 2>/dev/null" );
	strm.add( " &" ).add( od_newline );
	strm.add( "echo $! > " ).add( getPIDFilePathStr(*ret) );
#endif
    }
    strm.add( od_newline );

    strm.close();
#ifdef __unix__
    File::makeExecutable( ret->fullPath(), true );
#endif
    cmd = OS::MachineCommand( ret->fullPath(), true );

    return ret;
}


OS::CommandLauncher* OD::PythonAccess::getLauncher(
						const OS::MachineCommand& mc,
						bool background,
						const File::Path* activatefp,
						const char* envnm,
						File::Path& scriptfpret )
{
    OS::MachineCommand scriptcmd( mc );
    PtrMan<File::Path> scriptfp = getCommand( scriptcmd, background,
					      activatefp, envnm );
    if ( scriptfp )
	scriptfpret = *scriptfp;
    else
	scriptfpret.set( nullptr );

    OS::CommandLauncher* cl = new OS::CommandLauncher( scriptcmd );
    return cl;
}


void OD::PythonAccess::getPIDFromFile( const char* pidfnm, int* pid )
{
    double waittm = 10., waitstep = 0.1;
    while ( waittm > 0. )
    {
	waittm -= waitstep;
	if ( !File::exists(pidfnm) )
	    { Threads::sleep( waitstep ); continue; }

	BufferString pidstr;
	if ( !File::getContent(pidfnm,pidstr) || pidstr.isEmpty() )
	    { Threads::sleep( waitstep ); continue; }

	const int localpid = pidstr.toInt();
	if ( !isProcessAlive(localpid) )
	    { Threads::sleep( waitstep ); continue; }

	if ( pid )
	    *pid = localpid;
	File::remove( pidfnm );
	return;
    }
}


bool OD::PythonAccess::doExecute( const OS::MachineCommand& cmd,
				  const OS::CommandExecPars* execpars, int* pid,
				  const File::Path* activatefp,
				  const char* envnm ) const
{
    laststdout_.setEmpty();
    laststderr_.setEmpty();
    msg_.setEmpty();

    File::Path scriptfp;
    const bool background = execpars && execpars->launchtype_ == OS::RunInBG;
    cl_ = getLauncher( cmd, background, activatefp, envnm, scriptfp );
    if ( !cl_.ptr() )
    {
	msg_ = tr("Cannot create launcher for command '%1'")
		    .arg( cmd.getSingleStringRep() );
	return false;
    }

    const bool res = execpars ? cl_->execute( *execpars )
			      : cl_->execute( laststdout_, &laststderr_ );
    if ( pid )
	*pid = cl_->processID();

    if ( !scriptfp.isEmpty() )
    {
	const BufferString pidfnm( getPIDFilePathStr(scriptfp) );
	if ( res && background )
	    getPIDFromFile( pidfnm, pid );
	File::remove( scriptfp.fullPath() );
    }

    if ( !res )
    {
	if ( cl_->errorMsg().isEmpty() )
	    msg_.set( uiStrings::phrCannotStart(cmd.program()) );
	else
	    msg_.set( cl_->errorMsg() );
    }

    return res;
}


namespace OD
{
    static const char* sKeyPriorityGlobExpr()	{ return "Priority.*"; }
} // namespace OD


bool OD::PythonAccess::getSortedVirtualEnvironmentLoc(
					ObjectSet<File::Path>& pythonenvfp,
					BufferStringSet& envnms,
					const BufferString* envnm,
					const File::Path* externalroot )
{
    File::Path envsfp;
    if ( externalroot )
	envsfp = *externalroot;
    else
    {
	if ( !hasInternalEnvironment() ||
	     (hasInternalEnvironment() &&
	      !getInternalEnvironmentLocation(envsfp,true)) )
	    return false;
    }

    if ( envnm )
    {
	if ( !envnm->isEmpty() )
	{
	    const DirList dl( File::Path(envsfp,"envs").fullPath().str(),
			      File::DirsInDir );
	    if ( !dl.isPresent(envnm->str()) )
		return false;
	}

	pythonenvfp.add( new File::Path(envsfp) );
	envnms.add( *envnm );
	return true;
    }

    const DirList dl( File::Path(envsfp,"envs").fullPath().str(),
		      File::DirsInDir );
    BufferStringSet prioritydirs;
    TypeSet<int> prioritylist;
    for ( int idx=0; idx<dl.size(); idx++ )
    {
	const BufferString envpath( dl.fullPath(idx) );
	const DirList priorityfiles( envpath, File::FilesInDir,
				     sKeyPriorityGlobExpr() );
	if ( !priorityfiles.isEmpty() )
	{
	    const File::Path priofp( priorityfiles.fullPath(0) );
	    prioritydirs.add( envpath );
	    prioritylist += toInt( priofp.extension() );
	}
    }
    if ( prioritylist.isEmpty() )
    {
	for ( int idx=0; idx<dl.size(); idx++ )
	{
	    pythonenvfp.add( new File::Path(envsfp) );
	    const File::Path virtenvpath( dl.fullPath(idx) );
	    envnms.add( virtenvpath.baseName() );
	}
    }
    else
    {
	while( !prioritylist.isEmpty() )
	{
	    int highestprio = -1, prioidx = -1;
	    for ( int idx=0; idx<prioritylist.size(); idx++ )
	    {
		if ( prioritylist[idx] > highestprio )
		{
		    highestprio = prioritylist[idx];
		    prioidx = idx;
		}
	    }
	    if ( prioidx == -1 )
		break;

	    pythonenvfp.add( new File::Path(envsfp) );
	    const File::Path virtenvpath( prioritydirs.get(prioidx) );
	    envnms.add( virtenvpath.baseName() );
	    prioritydirs.removeSingle( prioidx );
	    prioritylist.removeSingle( prioidx );
	}
    }

    return !pythonenvfp.isEmpty();
}


bool OD::PythonAccess::validInternalEnvironment( const File::Path& fp )
{
    if ( !fp.exists() )
	return false;

    const BufferString relinfostr( "relinfo" );
    const File::Path relinfofp( fp, relinfostr );
    if ( !relinfofp.exists() )
	return false;

    PtrMan<File::Path> activatefp = getActivateScript( fp );
    if ( !activatefp )
	return false;

    const DirList dl( File::Path(fp,"envs").fullPath().str(), File::DirsInDir );
    for ( int idx=0; idx<dl.size(); idx++)
    {
	File::Path envfp( dl.fullPath(idx) );
#ifdef __unix__
	envfp.add( "bin" );
#endif
	envfp.add( sPythonExecNm() );
	if ( envfp.exists() )
	    return true;
    }

    return false;
}


File::Path OD::PythonAccess::getInternalEnvPath( bool userdef )
{
    File::Path fp;
    getInternalEnvironmentLocation( fp, userdef );
    return fp;
}


bool OD::PythonAccess::getInternalEnvironmentLocation( File::Path& fp,
						       bool userdef )
{
    const BufferString envloc( GetEnvVar("DTECT_PYTHON_INTERNAL") );
    if ( !envloc.isEmpty() )
    {
	fp.set( envloc );
	if ( validInternalEnvironment(fp) )
	    return true;
    }

    fp.set( GetSetupDataFileName(ODSetupLoc_ApplSetupPref,"Python_envs.txt",
				 false) );
    BufferString pythonloc;
    if ( fp.exists() )
    {
	od_istream strm( fp.fullPath() );
	ascistream astrm( strm );
	IOPar par;
	par.getFrom( astrm );
	if ( par.get(sKey::FileName(),pythonloc) && !pythonloc.isEmpty() )
	{
	    fp.set( pythonloc );
	    if ( validInternalEnvironment(fp) )
		return true;
	}
    }

    if ( !userdef )
	return false;

    BufferString pythonstr( sKey::Python() ); pythonstr.toLower();
    const IOPar& pythonsetts = Settings::fetch( pythonstr );
    PythonSource source;
    if ( !PythonSourceDef().parse(pythonsetts,sKeyPythonSrc(),source) ||
	 source != Internal || !pythonsetts.get(sKeyEnviron(),pythonloc) )
	return false;

    fp.set( pythonloc );
    return validInternalEnvironment( fp );
}


bool OD::PythonAccess::hasInternalEnvironment( bool userdef )
{
    const File::Path fp( getInternalEnvPath( userdef ) );
    return fp.exists();
}


bool OD::PythonAccess::retrievePythonVersionStr()
{
    if ( !isUsable(!istested_) )
	return false;

    OS::MachineCommand cmd( sPythonExecNm(true) );
    cmd.addFlag( "version" );
    BufferString laststdout, laststderr;
    const bool res = execute( cmd, laststdout, &laststderr );
    if ( res )
    {
	if ( !laststdout.isEmpty() )
	    pythversion_ = laststdout;
	else if ( !laststderr.isEmpty() )
	    pythversion_ = laststderr;
    }

    return !pythversion_.isEmpty();
}


void OD::PythonAccess::envChangeCB( CallBacker* )
{
    retrievePythonVersionStr();
    const uiRetVal uirv = updateModuleInfo( nullptr );
    if ( !uirv.isOK() )
	msg_.append( uirv );

    envChange.trigger();
}


uiRetVal OD::PythonAccess::verifyEnvironment( const char* piname )
{
    if ( !isUsable(!istested_) )
	return uiRetVal( tr("Could not detect a valid Python installation.") );

    if ( !msg_.isEmpty() )
	return uiRetVal( msg_ );

    File::Path fp( mGetSWDirDataDir() );
    fp.add( "Python" );
    BufferString genericName( piname );
    genericName.add( "_requirements" );
    BufferString platSpecificName( piname );
    platSpecificName.add( "_requirements_" )
		    .add(OD::Platform::local().shortName());

    fp.add( platSpecificName ).setExtension( "txt" );
    if ( !fp.exists() ) {
	fp.setFileName( genericName ).setExtension( "txt" );
	if ( !fp.exists() )
	    return uiRetVal( uiStrings::phrFileDoesNotExist(fp.fullPath() ) );
    }
    od_istream strm( fp.fullPath() );
    if ( !strm.isOK() )
	return uiRetVal( tr("Can't open requirements file: %1").arg(
			    fp.fullPath() ) );

    BufferString line;
    bool newlinefound = true;
    int lnum = 1;
    uiRetVal retval;
    while ( strm.isOK() )
    {
	strm.getLine( line, &newlinefound );
	if ( !newlinefound )
	    break;
	BufferStringSet modulestr;
	modulestr.unCat( line, "==" );
	BufferString modname = modulestr.get(0).trimBlanks().toLower();
	if ( modulestr.size() == 1 )
	    retval.add( hasModule( modname ) );
	else if (modulestr.size() >= 2 )
	{
	    BufferString ver = modulestr.get( 1 ).trimBlanks();
	    retval.add( hasModule( modname, ver ) );
	} else
	    retval.add( tr("Python requirements file: %1 error at line: %2"
			    ).arg( fp.fullPath() ).arg( lnum ) );
	lnum++;
    };

    return retval;
}


BufferString OD::PythonAccess::getPacmanExecNm() const
{
    if ( activatefp_ )
    {
	File::Path packmanexe( *activatefp_ );
	packmanexe.setFileName( "conda" );
#ifdef __win__
	packmanexe.setExtension( "bat" );
#endif
	if ( packmanexe.exists() )
	    return packmanexe.baseName();

#ifdef __win__
	packmanexe.setFileName( nullptr ).setFileName( nullptr )
		  .add( "Scripts" );
#endif
	packmanexe.setFileName( "pip" );
#ifdef __win__
	packmanexe.setExtension( "exe" );
#endif
	if ( packmanexe.exists() )
	    return packmanexe.baseName();

	packmanexe.setFileName( "pip3" );
	if ( packmanexe.exists() )
	    return packmanexe.baseName();

	return BufferString( "pip" ); //Fallback
    }

#ifdef __win__
    return BufferString( "pip" );
#else
    return BufferString( "pip3" );
#endif
}


uiRetVal OD::PythonAccess::hasModule( const char* modname,
				      const char* minversion ) const
{
    uiString msg;
    if ( minversion )
	msg = tr("Package: %1 Version: %2 or higher required").arg( modname )
		.arg( minversion );
    else
	msg = tr("Package: %1 required").arg( modname );

    for ( auto module : moduleinfos_ )
    {
	if ( module->name() == modname )
	{
	    if ( minversion ) {
		const SeparString actverstr( module->versionstr_, '.' );
		const SeparString reqverstr( minversion, '.' );
		for ( int ver=0; ver<reqverstr.size(); ver++ )
		{
		    if ( actverstr.getUI16Value(ver)<
			 reqverstr.getUI16Value(ver) )
			return uiRetVal( tr("%1, but installed Version: %2")
					    .arg( msg )
			.arg( module->versionstr_ ) );
		    else if ( actverstr.getUI16Value(ver)>reqverstr
				.getUI16Value(ver))
			break;
		}
	    }
	    return uiRetVal::OK();
	}
    }

    return uiRetVal( tr("%1, but module not found").arg( msg ) );
}


uiRetVal OD::PythonAccess::updateModuleInfo( const char* cmd )
{
    BufferString modulecmd( cmd );
    if ( modulecmd.isEmpty() )
	modulecmd.set( getPacmanExecNm() ).addSpace().add( "list" );

    moduleinfos_.setEmpty();

    BufferStringSet cmdstrs;
    cmdstrs.unCat( modulecmd, " " );
    if ( cmdstrs.isEmpty() )
	return uiRetVal( tr("Invalid command: %1").arg(modulecmd) );

    const BufferString prognm( cmdstrs.first()->str() );
    cmdstrs.removeSingle(0);
    OS::MachineCommand mc( prognm, cmdstrs );
    BufferString laststdout, laststderr;
    bool res = execute( mc, laststdout, &laststderr );
#ifdef __unix__
    if ( !res && prognm == FixedString("pip") )
    {
	mc.setProgram( "pip3" );
	res = execute( mc, laststdout, &laststderr );
    }
#endif
    if ( !res )
    {
	return uiRetVal( tr("Cannot generate a list of python modules:\n%1")
			    .arg(laststderr) );
    }

    BufferStringSet modstrs;
    modstrs.unCat( laststdout );
    for ( auto modstr : modstrs )
    {
	if ( modstr->startsWith("#") ||
	     modstr->startsWith("Package") || modstr->startsWith("----") )
	    continue;
	moduleinfos_.add( new ModuleInfo( modstr->trimBlanks().toLower() ) );
    }

    return uiRetVal::OK();
}


uiRetVal OD::PythonAccess::getModules( ManagedObjectSet<ModuleInfo>& mods )
{
    uiRetVal retval;
    if ( moduleinfos_.isEmpty() )
    {
	retval = updateModuleInfo( nullptr );
	if ( !retval.isOK() )
	    return retval;
    }

    mods.setEmpty();
    for ( auto module : moduleinfos_ )
    {
	ModuleInfo* minfo = new ModuleInfo( module->name() );
	minfo->versionstr_ = module->versionstr_;
	mods.add( minfo );
    }

    return retval;
}



OD::PythonAccess::ModuleInfo::ModuleInfo( const char* modulestr )
    : NamedObject("")
{
    BufferStringSet moduledata;
    moduledata.addWordsFrom( modulestr );
    if ( moduledata.isEmpty() )
	return;

    setName( moduledata.first()->buf() );
    if ( moduledata.size() > 1 )
	versionstr_.set( moduledata.get(1).buf() );
}


BufferString OD::PythonAccess::ModuleInfo::displayStr( bool withver ) const
{
    BufferString ret( name() );
    if ( withver )
	ret.add( " " ).add( versionstr_ );
    return ret;
}


namespace OD
{

static const char* sKeyNvidia() { return "NVIDIA"; }

static bool usesNvidiaCard( BufferString* glversionstr )
{
    bool ret = false;
    OS::MachineCommand cmd( "od_glxinfo" );
    BufferString stdoutstr;
    if ( !cmd.execute(stdoutstr) || stdoutstr.isEmpty() )
	return false;

    BufferStringSet glxinfostrs;
    glxinfostrs.unCat( stdoutstr.str() );
    for ( const auto line : glxinfostrs )
    {
	if ( !line->startsWith("OpenGL") )
	    continue;

	if ( line->contains("vendor string:") )
	    ret = line->contains( sKeyNvidia() );
	else if ( line->contains("version string:") && glversionstr )
	{
	    glversionstr->set( line->find( ':' )+1 );
	    glversionstr->trimBlanks();
	}
    }

    return ret;
}


//from https://docs.nvidia.com/cuda/cuda-toolkit-release-notes/index.html
static const char* cudastrs[] = { "10.1.105", "10.0.130", "9.2.148 update 1",
    "9.2.88", "9.1.85", "9.0.76", "8.0.61 GA2", "8.0.44", "7.5.16", "7.0.28",
0 };
#ifdef __win__
static const float nvidiavers[] = { 418.96f, 411.31f, 398.26f, 397.44f, 391.29f,
    385.54f, 376.51f, 369.30f, 353.66f, 347.62f };
#else
static const float nvidiavers[] = { 418.39f, 410.48f, 396.37f, 396.26f, 390.46f,
    384.81f, 375.26f, 367.48f, 352.31f, 346.46f };
#endif

static bool cudaCapable( const char* glstr, BufferString* maxcudaversionstr )
{
    BufferStringSet openglstrset;
    openglstrset.addWordsFrom( glstr );
    if ( openglstrset.size() < 2 || !openglstrset.isPresent(sKeyNvidia()) )
	return false;

    const float version = toFloat(openglstrset.last()->buf());
    int idx = 0;
    BufferString tmpcudastr;
    BufferString& maxcudaversion = maxcudaversionstr ? *maxcudaversionstr
						     : tmpcudastr;
    while( true )
    {
	const float nvidiaver = nvidiavers[idx];
	const char* cudastr = cudastrs[idx++];
	if ( !cudastr )
	    break;

	if ( version >= nvidiaver )
	{
	    maxcudaversion.set( cudastr );
	    break;
	}
    }

    return !maxcudaversion.isEmpty();
}


bool canDoCUDA( BufferString& maxverstr )
{
    BufferString glversion;
    const bool hasnv = usesNvidiaCard( &glversion );
    return hasnv && cudaCapable( glversion, &maxverstr );
}

};

