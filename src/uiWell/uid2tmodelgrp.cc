/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		August 2006
________________________________________________________________________

-*/

#include "uid2tmodelgrp.h"
#include "uitblimpexpdatasel.h"

#include "uiconstvel.h"
#include "uifilesel.h"
#include "uimsg.h"

#include "ioobjctxt.h"
#include "helpview.h"
#include "od_istream.h"
#include "survinfo.h"
#include "veldesc.h"
#include "unitofmeasure.h"
#include "welld2tmodel.h"
#include "wellimpasc.h"
#include "welldata.h"
#include "welltrack.h"
#include "wellinfo.h"
#include "od_helpids.h"

uiString uiD2TModelGroup::sKeyTemporaryVel()
{ return tr("Temporary model velocity"); }

float uiD2TModelGroup::getDefaultTemporaryVelocity()
{ return Vel::getGUIDefaultVelocity() * 1.25f; }


uiD2TModelGroup::uiD2TModelGroup( uiParent* p, const Setup& su )
    : uiGroup(p,"D2TModel group")
    , velfld_(0)
    , csfld_(0)
    , dataselfld_(0)
    , setup_(su)
    , fd_( *Well::D2TModelAscIO::getDesc(setup_.withunitfld_) )
{
    uiFileSel::Setup fssu;
    fssu.withexamine( true ).checkable( setup_.fileoptional_ );
    filefld_ = new uiFileSel( this, toUiString(setup_.filefldlbl_), fssu );
    if ( setup_.fileoptional_ )
    {
	filefld_->setChecked( true );
	filefld_->checked.notify( mCB(this,uiD2TModelGroup,fileFldChecked) );
	const uiString vellbl( sKeyTemporaryVel() );
	velfld_ = new uiConstantVel( this, getDefaultTemporaryVelocity(),
				     vellbl );
	velfld_->attach( alignedBelow, filefld_ );
    }

    dataselfld_ = new uiTableImpDataSel( this, fd_,
                                        mODHelpKey(mD2TModelGroupHelpID) );
    if ( setup_.fileoptional_ )
	dataselfld_->attach( alignedBelow, velfld_ );
    else
	dataselfld_->attach( alignedBelow, filefld_ );

    if ( setup_.asksetcsmdl_ )
    {
	csfld_ = new uiGenInput( this, tr("Is this checkshot data?"),
				 BoolInpSpec(false) );
	csfld_->attach( alignedBelow, dataselfld_ );
    }

    setHAlignObj( filefld_ );
    postFinalise().notify( mCB(this,uiD2TModelGroup,fileFldChecked) );
}



void uiD2TModelGroup::fileFldChecked( CallBacker* )
{
    const bool havefile = setup_.fileoptional_ ? filefld_->isChecked() : true;
    if ( csfld_ ) csfld_->display( havefile );
    if ( velfld_ ) velfld_->display( !havefile );
    dataselfld_->display( havefile );
    dataselfld_->updateSummary();
}


#define mErrRet(s) { errmsg_ = s; return false; }
bool uiD2TModelGroup::getD2T( Well::Data& wd, bool cksh ) const
{
    if ( setup_.fileoptional_ && !filefld_->isChecked() )
    {
	if ( velfld_->isUndef() )
	    mErrRet( uiStrings::phrEnter(tr("the velocity for "
			"generating the D2T model")) )
    }

    if ( wd.track().isEmpty() )
	mErrRet( tr("Cannot generate D2Time model without track") )

    if ( cksh )
	wd.checkShotModel().setEmpty();
    else
	wd.d2TModel().setEmpty();

    Well::D2TModel& d2t = cksh ? wd.checkShotModel() : wd.d2TModel();

    if ( filefld_->isCheckable() && !filefld_->isChecked() )
    {
	const float vel = velfld_->getInternalVelocity();
	d2t.makeFromTrack( wd.track(), vel, wd.info().replacementVelocity() );
    }
    else
    {
	const char* fname = filefld_->fileName();
	od_istream strm( fname );
	if ( !strm.isOK() )
	    mErrRet(uiStrings::phrCannotOpenInpFile())

	if ( !dataselfld_->commit() )
	    mErrRet( uiStrings::phrSpecify(tr("data format")) )

	d2t.setName( fname );
	Well::D2TModelAscIO aio( fd_ );
	if ( !aio.get(strm,d2t,wd) )
	{
	    errmsg_ = tr("ASCII TD model import failed for well %1\n%2\n\n"
			 "Change your format definition or edit your data." )
			.arg( wd.name() )
			.arg( aio.errMsg() );
	    return false;
	}

	warnmsg_ = aio.warnMsg();
    }

    if ( wd.track().zRange().stop < SI().seismicReferenceDatum() )
	return true;

    if ( d2t.size() < 2 )
	mErrRet( tr("Cannot import time-depth model") )

    d2t.deInterpolate();
    return true;
}


bool uiD2TModelGroup::wantAsCSModel() const
{
    return csfld_ && csfld_->getBoolValue() && filefld_->isChecked();
}


BufferString uiD2TModelGroup::dataSourceName() const
{
    BufferString ret;
    if ( !filefld_->isCheckable() || filefld_->isChecked() )
	ret.set( filefld_->fileName() );
    else
    {
	ret.set( "[V=" ).add( velfld_->getFValue() ).add( " " );
	ret.add( toString(VelocityDesc::getVelUnit(true)) ).add( "]" );
    }
    return ret;
}
