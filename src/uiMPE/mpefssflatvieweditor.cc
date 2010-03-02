
/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Umesh Sinha
 Date:		Jan 2010
 RCS:           $Id: mpefssflatvieweditor.cc,v 1.2 2010-03-02 06:51:06 cvsumesh Exp $
________________________________________________________________________

-*/

#include "mpefssflatvieweditor.h"

#include "emeditor.h"
#include "emfaultstickset.h"
#include "emfaultstickpainter.h"
#include "emmanager.h"
#include "emposid.h"
#include "faultstickseteditor.h"
#include "flatauxdataeditor.h"
#include "flatposdata.h"
#include "mouseevent.h"
#include "mousecursor.h"
#include "mpeengine.h"
#include "survinfo.h"

#include "uiworld2ui.h"


namespace MPE
{

FaultStickSetFlatViewEditor::FaultStickSetFlatViewEditor(
				FlatView::AuxDataEditor* ed )
    : EM::FaultStickSetFlatViewEditor(ed)
    , editor_(ed)
    , meh_(0)
    , activestickid_(-1)
    , seedhasmoved_(false)
    , mousepid_(-1)
{
    ed->movementStarted.notify(
	    mCB(this,FaultStickSetFlatViewEditor,seedMovementStartedCB) );
    ed->movementFinished.notify(
	    mCB(this,FaultStickSetFlatViewEditor,seedMovementFinishedCB) );
    MPE::engine().activefsschanged.notify(
	    mCB(this,FaultStickSetFlatViewEditor,activeFSSChgCB) );
    editor_->viewer().appearance().annot_.editable_ = true;
    fsspainter_->abouttorepaint_.notify(
	    mCB(this,FaultStickSetFlatViewEditor,fssRepaintATSCB) );
    fsspainter_->repaintdone_.notify( 
	    mCB(this,FaultStickSetFlatViewEditor,fssRepaintedCB) );
}


FaultStickSetFlatViewEditor::~FaultStickSetFlatViewEditor()
{
    editor_->movementStarted.remove(
	    mCB(this,FaultStickSetFlatViewEditor,seedMovementStartedCB) );
    editor_->movementFinished.notify(
	    mCB(this,FaultStickSetFlatViewEditor,seedMovementFinishedCB) );
    setMouseEventHandler( 0 );
}


void FaultStickSetFlatViewEditor::setMouseEventHandler( MouseEventHandler* meh )
{
    if ( meh_ )
    {
	meh_->movement.remove(
		mCB(this,FaultStickSetFlatViewEditor,mouseMoveCB) );
	meh_->buttonPressed.remove(
		mCB(this,FaultStickSetFlatViewEditor,mousePressCB) );
	meh_->buttonReleased.remove(
		mCB(this,FaultStickSetFlatViewEditor,mouseReleaseCB) );
    }

    meh_ = meh;

    if ( meh_ )
    {
	meh_->movement.notify(
		mCB(this,FaultStickSetFlatViewEditor,mouseMoveCB) );
	meh_->buttonPressed.notify(
		mCB(this,FaultStickSetFlatViewEditor,mousePressCB) );
	meh_->buttonReleased.notify(
		mCB(this,FaultStickSetFlatViewEditor,mouseReleaseCB) );
    }
}


void FaultStickSetFlatViewEditor::updateActStkContainer()
{
    cleanActStkContainer();
    if ( MPE::engine().getActiveFSSObjID() != -1 )
	fillActStkContainer( MPE::engine().getActiveFSSObjID() );
}


void FaultStickSetFlatViewEditor::cleanActStkContainer()
{
    for ( int idx=0; idx<markeridinfo_.size(); idx++ )
	editor_->removeAuxData( markeridinfo_[idx]->merkerid_ );

    if ( markeridinfo_.size() )
	deepErase( markeridinfo_ );
}


void FaultStickSetFlatViewEditor::fillActStkContainer(const EM::ObjectID oid)
{
    ObjectSet<EM::FaultStickPainter::StkMarkerInfo> dispedstkmrkinfos;
    fsspainter_->getDisplayedSticks( oid, dispedstkmrkinfos );

    for ( int idx=0; idx<dispedstkmrkinfos.size(); idx++ )
    {
	StkMarkerIdInfo* merkeridinfo = new  StkMarkerIdInfo;
	merkeridinfo->merkerid_ = editor_->addAuxData(
			    dispedstkmrkinfos[idx]->marker_, true );
	merkeridinfo->stickid_ = dispedstkmrkinfos[idx]->stickid_;
	editor_->enableEdit( merkeridinfo->merkerid_, false, true, false );

	markeridinfo_ += merkeridinfo;
    }
}


void FaultStickSetFlatViewEditor::activeFSSChgCB( CallBacker* )
{
    fsspainter_->setActiveFSS( MPE::engine().getActiveFSSObjID() );
    cleanActStkContainer();
    if ( MPE::engine().getActiveFSSObjID() != -1 )
	fillActStkContainer( MPE::engine().getActiveFSSObjID() );
}


void FaultStickSetFlatViewEditor::fssRepaintATSCB( CallBacker* )
{
    cleanActStkContainer();
}


void FaultStickSetFlatViewEditor::fssRepaintedCB( CallBacker* )
{
    if ( MPE::engine().getActiveFSSObjID() != -1 )
	fillActStkContainer( MPE::engine().getActiveFSSObjID() );
    activestickid_ = -1;
}


void FaultStickSetFlatViewEditor::seedMovementStartedCB( CallBacker* cb )
{
    int edidauxdataid = editor_->getSelPtDataID();
    int knotid = -1;
    if ( editor_->getSelPtIdx().size() > 0 )
	knotid = editor_->getSelPtIdx()[0];

    if ( (edidauxdataid==-1) || (knotid==-1) )
	return;

    int selstickid = -1;

    for ( int idx=0; idx<markeridinfo_.size(); idx++ )
    {
	if ( markeridinfo_[idx]->merkerid_ == edidauxdataid )
	{ 
	    selstickid = markeridinfo_[idx]->stickid_;
	    break;
	}
    }

    if ( selstickid == -1 )
	return;

    if ( selstickid == fsspainter_->getActiveStickId() )
	return;

    const Geom::Point2D<double> pos = editor_->getSelPtPos();

    EM::ObjectID emid = MPE::engine().getActiveFSSObjID();
    if ( emid == -1 ) return; 

    RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );
    mDynamicCastGet(EM::FaultStickSet*,emfss,emobject.ptr());
    if ( !emfss )
	return;

    RefMan<MPE::ObjectEditor> editor = MPE::engine().getEditor( emid, true );
    mDynamicCastGet( MPE::FaultStickSetEditor*, fsseditor, editor.ptr() );
    if ( !fsseditor )
	return;
}


void FaultStickSetFlatViewEditor::seedMovementFinishedCB( CallBacker* cb )
{
    int edidauxdataid = editor_->getSelPtDataID();
    int displayedknotid = -1;
    if ( editor_->getSelPtIdx().size() > 0 )
	displayedknotid = editor_->getSelPtIdx()[0];

    if ( (edidauxdataid==-1) || (displayedknotid==-1) )
	return;

    const Geom::Point2D<double> pos = editor_->getSelPtPos();
    const CubeSampling& cs = fsspainter_->getCubeSampling();
    
    Coord3 coord3;
    if ( !cs.isEmpty() ) // if empty 2D
    {
	if ( cs.nrZ() == 1 )
	{
	    const BinID bid( cs.hrg.inlRange().snap(pos.x), cs.hrg.crlRange().snap(pos.y) ); 
	    coord3.coord() = SI().transform(bid);
	    coord3.z = cs.zrg.start;
	}
	else
	{
	    const bool isinl = (cs.nrInl()==1);
	    const BinID bid = isinl
		? BinID(cs.hrg.start.inl,cs.hrg.crlRange().snap(pos.x) )
		: BinID(cs.hrg.inlRange().snap(pos.x),cs.hrg.start.crl);

	    coord3.coord() = SI().transform(bid);
	    coord3.z = pos.y ;
	}
    }
    else
    {
	//TODO for 2D
    }

    EM::ObjectID emid = MPE::engine().getActiveFSSObjID();
    if ( emid == -1 ) return; 

    RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );
    mDynamicCastGet(EM::FaultStickSet*,emfss,emobject.ptr());
    if ( !emfss )
	return;

    int sid = emfss->sectionID( 0 );
    mDynamicCastGet( const Geometry::FaultStickSet*, fss, 
		     emfss->sectionGeometry( sid ) );

    StepInterval<int> colrg = fss->colRange( fsspainter_->getActiveStickId() );
    const int knotid = colrg.start + displayedknotid*colrg.step;

    RefMan<MPE::ObjectEditor> editor = MPE::engine().getEditor( emid, true );
    mDynamicCastGet( MPE::FaultStickSetEditor*, fsseditor, editor.ptr() );
    if ( !fsseditor )
	return;

    const RowCol knotrc( fsspainter_->getActiveStickId(), knotid );

    EM::PosID pid( emid,0,knotrc.getSerialized() );

    emfss->setPos(pid,coord3,true);
    seedhasmoved_ = true;
}


void FaultStickSetFlatViewEditor::mouseMoveCB( CallBacker* cb )
{
    EM::ObjectID emid = MPE::engine().getActiveFSSObjID();
    if ( emid == -1 ) return; 

    RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );
    mDynamicCastGet(EM::FaultStickSet*,emfss,emobject.ptr());
    if ( !emfss )
	return;

    if ( emfss->isEmpty() )
	return;

    RefMan<MPE::ObjectEditor> editor = MPE::engine().getEditor( emid, true );
    mDynamicCastGet( MPE::FaultStickSetEditor*, fsseditor, editor.ptr() );
    if ( !fsseditor )
	return;

    const FlatDataPack* dp = editor_->viewer().pack( false );

    const MouseEvent& mouseevent = meh_->event();
    const uiRect datarect( editor_->getMouseArea() );
    if ( !datarect.isInside(mouseevent.pos()) ) return;

    const uiWorld2Ui w2u( datarect.size(), editor_->getWorldRect(mUdf(int)) );
    const uiWorldPoint wp = w2u.transform( mouseevent.pos()-datarect.topLeft());

    const FlatPosData& pd = dp->posData();
    const IndexInfo ix = pd.indexInfo( true, wp.x );
    const IndexInfo iy = pd.indexInfo( false, wp.y );
    Coord3 pos = dp->getCoord( ix.nearest_, iy.nearest_ );
    pos.z = (cs_.nrZ() == 1) ? cs_.zrg.start : wp.y;

    EM::PosID pid;
    fsseditor->getInteractionInfo( pid, 0, 0, pos, SI().zScale() );

    if ( pid.isUdf() )
	return; 

    const int sticknr = pid.isUdf() ? mUdf(int) : RowCol(pid.subID()).row;

    if ( activestickid_ != sticknr )
	activestickid_ = sticknr;

    if( fsspainter_->hasDiffActiveStick(&pid) )
	fsspainter_->setActiveStick( pid );
}


void FaultStickSetFlatViewEditor::mousePressCB( CallBacker* cb )
{
    mousepid_.setObjectID( -1 );
    int edidauxdataid = editor_->getSelPtDataID();
    int displayedknotid = -1;
    if ( editor_->getSelPtIdx().size() > 0 )
	displayedknotid = editor_->getSelPtIdx()[0];

    if ( (edidauxdataid==-1) || (displayedknotid==-1) )
	return;

    EM::ObjectID emid = MPE::engine().getActiveFSSObjID();
    if ( emid == -1 ) return;

    int stickid = -1;

    for ( int idx=0; idx<markeridinfo_.size(); idx++ )
    {
	if ( markeridinfo_[idx]->merkerid_ == edidauxdataid )
	{ 
	    stickid = markeridinfo_[idx]->stickid_;
	    break;
	}
    }

    if ( stickid == -1 ) return;

    RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );

    mDynamicCastGet(EM::FaultStickSet*,emfss,emobject.ptr());
    if ( !emfss ) return;

    int sid = emfss->sectionID( 0 );
    mDynamicCastGet( const Geometry::FaultStickSet*, fss, 
		     emfss->sectionGeometry( sid ) );

    RowCol rc;
    rc.row = stickid;
    int knotid = -1;

    StepInterval<int> colrg = fss->colRange( rc.row );
    knotid = colrg.start + displayedknotid*colrg.step;

    RefMan<MPE::ObjectEditor> editor = MPE::engine().getEditor( emid, true );
    mDynamicCastGet( MPE::FaultStickSetEditor*, fsseditor, editor.ptr() );
    if ( !fsseditor )
	return;

    EM::PosID mousepid( emid, 0, RowCol(stickid,knotid).getSerialized() );
    fsseditor->setLastClicked( mousepid );
    activestickid_ = stickid;
    fsspainter_->setActiveStick( mousepid );
    mousepid_ = mousepid;
}


void FaultStickSetFlatViewEditor::mouseReleaseCB( CallBacker* cb )
{
    if ( seedhasmoved_ )
    {
	seedhasmoved_ = false;
	return;
    }
     EM::ObjectID emid = MPE::engine().getActiveFSSObjID();
    if ( emid == -1 ) return; 

    RefMan<EM::EMObject> emobject = EM::EMM().getObject( emid );
    mDynamicCastGet(EM::FaultStickSet*,emfss,emobject.ptr());
    if ( !emfss )
	return;

    RefMan<MPE::ObjectEditor> editor = MPE::engine().getEditor( emid, true );
    mDynamicCastGet( MPE::FaultStickSetEditor*, fsseditor, editor.ptr() );
    if ( !fsseditor )
	return;

    const FlatDataPack* dp = editor_->viewer().pack( false );

    const MouseEvent& mouseevent = meh_->event();
    const uiRect datarect( editor_->getMouseArea() );
    if ( !datarect.isInside(mouseevent.pos()) ) return;

    const uiWorld2Ui w2u( datarect.size(), editor_->getWorldRect(mUdf(int)) );
    const uiWorldPoint wp = w2u.transform( mouseevent.pos()-datarect.topLeft());

    const FlatPosData& pd = dp->posData();
    const IndexInfo ix = pd.indexInfo( true, wp.x );
    const IndexInfo iy = pd.indexInfo( false, wp.y );
    Coord3 pos = dp->getCoord( ix.nearest_, iy.nearest_ );
    pos.z = (cs_.nrZ() == 1) ? cs_.zrg.start : wp.y;

    EM::FaultStickSetGeometry& fssg = emfss->geometry();

    EM::PosID interactpid;
    fsseditor->getInteractionInfo( interactpid, 0, 0, pos, SI().zScale() );

    if ( !mousepid_.isUdf() && mouseevent.ctrlStatus() && !mouseevent.shiftStatus() )
    {
	//Remove knot/stick
	const int rmnr = RowCol(mousepid_.subID()).row;
	if ( fssg.nrKnots(mousepid_.sectionID(),rmnr) == 1 )
	{
	    fssg.removeStick( mousepid_.sectionID(), rmnr, true );
	    fsseditor->setLastClicked( EM::PosID::udf() );
	}
	else
	    fssg.removeKnot( mousepid_.sectionID(), mousepid_.subID(), true );
	return;
    }

    if ( mouseevent.shiftStatus() || interactpid.isUdf() )
    {
	Coord3 editnormal( 0, 0, 1 );

	if ( cs_.defaultDir()==CubeSampling::Inl )
	    editnormal = Coord3( SI().binID2Coord().rowDir(), 0 );
	else if ( cs_.defaultDir()==CubeSampling::Crl )
	    editnormal = Coord3( SI().binID2Coord().colDir(), 0 );

	const int sid = emfss->sectionID(0);
	Geometry::FaultStickSet* fss = fssg.sectionGeometry( sid );
	const int insertsticknr = !fss || fss->isEmpty() ? 0 : fss->rowRange().stop+1;

	fssg.insertStick( sid, insertsticknr, 0, pos, editnormal,
			  0, 0, true );
	const EM::SubID subid = RowCol(insertsticknr,0).getSerialized();
	fsseditor->setLastClicked( EM::PosID(emfss->id(),sid,subid) );
    }
    else
    {
	fssg.insertKnot( interactpid.sectionID(), interactpid.subID(), pos, true );
	fsseditor->setLastClicked( interactpid );
    }
}

} // namespace MPE
