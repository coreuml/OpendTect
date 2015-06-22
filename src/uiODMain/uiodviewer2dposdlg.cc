/*+
________________________________________________________________________

(C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
Author:        Satyaki
Date:	       March 2015
________________________________________________________________________

-*/

static const char* rcsID mUsedVar = "$Id$";

#include "uiodviewer2dposdlg.h"

#include "uiattribpartserv.h"
#include "uibutton.h"
#include "uiflatviewstdcontrol.h"
#include "uiodviewer2dmgr.h"
#include "uiodviewer2dposgrp.h"
#include "uiodmain.h"
#include "uiodapplmgr.h"
#include "randomlinegeom.h"


uiODViewer2DPosDlg::uiODViewer2DPosDlg( uiODMain& appl )
    : uiDialog(&appl,uiDialog::Setup(tr("2D Viewer Launcher"),
				     tr("Select Position and Data"),
				     mODHelpKey(mODViewer2DPosDlgHelpID)))
    , odappl_(appl)
    , initialx1pospercm_(mUdf(float))
    , initialx2pospercm_(mUdf(float))
{
    uiFlatViewStdControl::getGlobalZoomLevel(
	    initialx1pospercm_, initialx2pospercm_, true );

    posgrp_  = new uiODViewer2DPosGrp( this, new Viewer2DPosDataSel(), false );

    uiPushButton* zoomlevelbut = new uiPushButton( this, tr("Advanced..."),
	    		mCB(this,uiODViewer2DPosDlg,zoomLevelCB), true );
    zoomlevelbut->attach( alignedBelow, posgrp_ );
}


void uiODViewer2DPosDlg::zoomLevelCB( CallBacker* )
{
    uiFlatViewZoomLevelDlg zoomlvldlg(
	    this, initialx1pospercm_, initialx2pospercm_, true );
    zoomlvldlg.go();
}


bool uiODViewer2DPosDlg::acceptOK( CallBacker* )
{
    if ( !posgrp_->commitSel( true ) )
	return false;

    IOPar seldatapar;
    posgrp_->fillPar( seldatapar );
    Viewer2DPosDataSel posdatasel;
    posdatasel.usePar( seldatapar );
    DataPack::ID dpid = DataPack::cNoID();
    uiAttribPartServer* attrserv = odappl_.applMgr().attrServer();
    attrserv->setTargetSelSpec( posdatasel.selspec_ );
    const bool isrl = !posdatasel.rdmlineid_.isUdf();
    if ( isrl )
    {
	TypeSet<BinID> knots, path;
	Geometry::RandomLineSet::getGeometry(
		posdatasel.rdmlineid_, knots, &posdatasel.tkzs_.zsamp_ );
	Geometry::RandomLine::getPathBids( knots, path );
	dpid = attrserv->createRdmTrcsOutput(
				posdatasel.tkzs_.zsamp_, &path, &knots );
    }
    else
	dpid = attrserv->createOutput( posdatasel.tkzs_, DataPack::cNoID() );
    
    odappl_.viewer2DMgr().displayIn2DViewer( dpid, posdatasel.selspec_,
					     false, initialx1pospercm_,
	   				     initialx2pospercm_ );
    return true;
}
