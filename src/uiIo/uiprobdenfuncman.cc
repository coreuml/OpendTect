/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Nanne Hemstra
 Date:          January 2010
________________________________________________________________________

-*/

#include "uiprobdenfuncman.h"

#include "uiprobdenfuncgen.h"
#include "uieditpdf.h"

#include "uilistbox.h"
#include "uitextedit.h"
#include "uiioobjseldlg.h"
#include "uiioobjmanip.h"
#include "uimsg.h"

#include "bufstring.h"
#include "sampledprobdenfunc.h"
#include "simpnumer.h"
#include "probdenfunctr.h"
#include "od_helpids.h"

static const int cPrefWidth = 75;

uiProbDenFuncMan::uiProbDenFuncMan( uiParent* p )
: uiObjFileMan(p,uiDialog::Setup(
                uiStrings::phrManage( uiStrings::sProbDensFunc(false,mPlural)),
                 mNoDlgTitle,
                 mODHelpKey(mProbDenFuncManHelpID) )
			       .nrstatusflds(1).modal(false),
	           ProbDenFuncTranslatorGroup::ioContext())
{
    createDefaultUI();
    selgrp_->getListField()->doubleClicked.notify(
				mCB(this,uiProbDenFuncMan,browsePush) );

    uiIOObjManipGroup* manipgrp = selgrp_->getManipGroup();
    manipgrp->addButton( "editprdf",
			 tr("Browse/edit this Probability Density Function"),
			 mCB(this,uiProbDenFuncMan,browsePush) );
    manipgrp->addButton( "genprdf",
			 tr("Generate Probability Density Function"),
			 mCB(this,uiProbDenFuncMan,genPush) );

    selgrp_->setPrefWidthInChar( mCast(float,cPrefWidth) );
    infofld_->setPrefWidthInChar( cPrefWidth );
    selChg( this );
}



uiProbDenFuncMan::~uiProbDenFuncMan()
{
}


#define mGetPDF(pdf,ioobj) \
    PtrMan<ProbDenFunc> pdf = ProbDenFuncTranslator::read( ioobj )


void uiProbDenFuncMan::browsePush( CallBacker* )
{
    if ( !curioobj_ )
	return;
    mGetPDF(pdf,*curioobj_);
    if ( !pdf )
	return;

    uiEditProbDenFuncDlg dlg( this, *pdf, true );
    if ( dlg.go() && dlg.isChanged() )
    {
	const int choice = uiMSG().question( tr("PDF changed. Save?"),
		uiStrings::sYes(), m3Dots(tr("As new")), uiStrings::sNo() );
	if ( choice < 0 ) return;

	PtrMan<IOObj> saveioobj = curioobj_->clone();
	if ( choice == 0 )
	{
	    CtxtIOObj ctio( ctxt_ );
	    ctio.ctxt_.forread_ = false;
	    uiIOObjSelDlg seldlg( this, ctio, uiStrings::sSaveAs() );
	    if ( !seldlg.go() || !seldlg.ioObj() ) return;
	    saveioobj = seldlg.ioObj()->clone();
	}

	uiString emsg;
	if ( !ProbDenFuncTranslator::write(*pdf,*saveioobj,&emsg) )
	    uiMSG().error( emsg );
	else
	    selgrp_->fullUpdate( saveioobj->key() );
    }
}


void uiProbDenFuncMan::genPush( CallBacker* )
{
    uiProbDenFuncGen dlg( this );
    if ( dlg.go() )
	selgrp_->fullUpdate( dlg.newObjKey() );
}


bool uiProbDenFuncMan::gtItemInfo( const IOObj& ioobj, uiPhraseSet& inf ) const
{
    mGetPDF(pdf,ioobj);
    if ( !pdf )
	return false;

    addObjInfo( inf, uiStrings::sType(), pdf->getTypeStr() );
    for ( int idx=0; idx<pdf->nrDims(); idx++ )
    {
	uiWord txt = toUiString( "\t%1 %2" )
			    .arg( uiStrings::sDimension() ).arg( idx+1 );
	inf.add( txt.addMoreInfo( toUiString(pdf->dimName(idx)) ) );
    }

    return true;
}
