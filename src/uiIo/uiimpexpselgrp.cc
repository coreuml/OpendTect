/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Satyaki Maitra
 Date:          Nov 2010
________________________________________________________________________

-*/

static const char* rcsID = "";

#include "uiimpexpselgrp.h"

#include "uiaxishandler.h"
#include "uibutton.h"
#include "uicombobox.h"
#include "uigeom.h"
#include "uigeninput.h"
#include "uifileinput.h"
#include "uilabel.h"
#include "uilistbox.h"
#include "uimsg.h"
#include "uitoolbutton.h"

#include "ascstream.h"
#include "bufstring.h"
#include "color.h"
#include "ctxtioobj.h"
#include "file.h"
#include "odver.h"
#include "safefileio.h"
#include "separstr.h"
#include "strmprov.h"
#include "strmoper.h"
#include "timefun.h"

static const char* filefilter = "Text (*.txt *.dat)";
static const char* sKeyFileType = "CrossPlot Selection";
static const char* sKeyNrSelGrps = "Nr of Selection Groups";
static const char* sKeySelGrp()		{ return "SelectionGrps"; }
static const char* sKeyIdxFileName() 	{ return "index.txt"; }


uiSGSelGrp::uiSGSelGrp( uiParent* p, bool forread )
    : uiGroup(p)
    , forread_(forread)
{
    uiLabeledListBox* llb =
	new uiLabeledListBox( this, "Select Crossplot Selection Groups", false,
			      uiLabeledListBox::AboveMid );
    listfld_ = llb->box();
    listfld_->selectionChanged.notify( mCB(this,uiSGSelGrp,selChangedCB) );

    if ( !forread )
    {
	nmfld_ = new uiGenInput( this, "Name" );
	nmfld_->attach( alignedBelow, llb );
	nmfld_->setElemSzPol( uiObject::SmallMax );
	nmfld_->setStretch( 2, 0 );
    }

    infobut_ = new uiToolButton( this, "info.png", "Info",
	    			mCB(this,uiSGSelGrp,showInfo) );
    infobut_->attach( rightTo, llb );

    delbut_ = new uiToolButton( this, "trashcan.png", "Delete Selection-Groups",
	    		        mCB(this,uiSGSelGrp,delSelGrps) );
    delbut_->attach( alignedBelow, infobut_ );

    renamebut_ = new uiToolButton( this, "renameobj.png",
	    			   "Rename Selection-Groups",
	    			   mCB(this,uiSGSelGrp,renameSelGrps) );
    renamebut_->attach( alignedBelow, delbut_ );

    fillListBox();
}


void uiSGSelGrp::selChangedCB( CallBacker* )
{
    if ( !forread_ )
	nmfld_->setText( listfld_->getText() );
}


void uiSGSelGrp::showInfo( CallBacker* )
{
    BufferString info;
    ObjectSet<SelectionGrp> selgrpset;
    getCurSelGrpSet( selgrpset );

    for ( int idx=0; idx<selgrpset.size(); idx++ )
	selgrpset[idx]->getInfo( info );

    deepErase( selgrpset );
    uiMSG().message( info );
}


void uiSGSelGrp::delSelGrps( CallBacker* )
{
    BufferStringSet nms;
    getSelGrpSetNames( nms );
    const int idx = nms.indexOf( listfld_->getText() );
    if ( mIsUdf(idx) || idx < 0 )
	return;

    nms.remove( idx );
    setSelGrpSetNames( nms );
    File::remove( getCurFileNm() );
    fillListBox();
}


mClass uiImpSG : public uiDialog
{
public:

uiImpSG( uiParent* p, ObjectSet<SelectionGrp>& selgrpset )
    : uiDialog(p,uiDialog::Setup("Import Selection Groups","",""))
    , selgrpset_(selgrpset)
{
    inpfld_ = new uiFileInput( this, "Select Selection Group Ascii" );
}

bool acceptOK( CallBacker* )
{
    if ( !inpfld_->fileName() )
    {
	uiMSG().error( "Select a valid file" );
	return false;
    }

    ObjectSet<SelectionGrp> selgrpset;
    SelGrpImporter imp( inpfld_->fileName() );
    selgrpset = imp.getSelections();
    if ( !imp.errMsg().isEmpty() )
    {
	uiMSG().error( imp.errMsg() );
	return false;
    }

    xname_ = imp.xName();
    yname_ = imp.yName();
    y2name_ = imp.y2Name();

    deepErase( selgrpset_ );
    selgrpset_ = selgrpset;
    return true;
}

    BufferString			xname_;
    BufferString			yname_;
    BufferString			y2name_;

    ObjectSet<SelectionGrp>&		selgrpset_;
    uiFileInput*			inpfld_;
};


mClass uiRenameDlg : public uiDialog
{
public:

uiRenameDlg( uiParent* p, const char* nm )
    : uiDialog(p,uiDialog::Setup("Rename Selection Group Set","","") )
{
    namefld_ = new uiGenInput( this, "Selection Group Set Name" );
    namefld_->setText( nm );
}

const char* getName()
{ return namefld_->text(); }

    uiGenInput*			namefld_;

};


void uiSGSelGrp::renameSelGrps( CallBacker* )
{
    BufferStringSet nms;
    getSelGrpSetNames( nms );
    uiRenameDlg dlg( this, listfld_->getText() );
    if ( dlg.go() )
    {
	const int idx = nms.indexOf( listfld_->getText() );
	if ( mIsUdf(idx) || idx < 0 || !dlg.getName() )
	    return;
	nms.get( idx ) = dlg.getName();
	BufferString newfp( basefp_.fullPath() );
	newfp += "/";
	newfp += dlg.getName();
	setSelGrpSetNames( nms );
	File::rename( getCurFileNm(), newfp.buf() );
	fillListBox();
    }
}


bool uiSGSelGrp::addSelGrpSet()
{
    if ( forread_ )
	return false;

    BufferStringSet nms;
    if ( !getSelGrpSetNames(nms) || nms.isPresent(nmfld_->text()) )
	return false;
    nms.add( nmfld_->text() );
    if ( !setSelGrpSetNames(nms) )
	return false;
    return true;
}


bool uiSGSelGrp::fillListBox()
{
    BufferStringSet selgrpsetnms;
    if ( !createBaseDir() || (!hasIdxFile() && !setSelGrpSetNames(selgrpsetnms))
	 || !getSelGrpSetNames(selgrpsetnms) )
	return false;

    listfld_->setEmpty();
    listfld_->addItems( selgrpsetnms );

    return true;
}


bool uiSGSelGrp::getCurSelGrpSet( ObjectSet<SelectionGrp>& selgrps ) 
{
    SelGrpImporter imp( getCurFileNm() );
    selgrps = imp.getSelections();

    xname_ = imp.xName();
    yname_ = imp.yName();
    y2name_ = imp.y2Name();

    if ( !imp.errMsg().isEmpty() )
    {
	uiMSG().error(imp.errMsg());
	return false;
    }

    return true;
}


const char* uiSGSelGrp::selGrpSetNm() const
{
    return forread_ ? listfld_->getText() : nmfld_->text();
}


BufferString uiSGSelGrp::getCurFileNm() const
{
    BufferString ret( basefp_.fullPath() );
    ret.add( "/" );
    ret.add( forread_ ? listfld_->getText() : nmfld_->text() );

    return ret;
}


bool uiSGSelGrp::hasIdxFile()
{
    FilePath fp( basefp_ );
    fp.add( sKeyIdxFileName() );

    return File::exists( fp.fullPath() );
}


bool uiSGSelGrp::createBaseDir()
{
    basefp_ = IOObjContext::getDataDirName( IOObjContext::Feat );
    basefp_.add( sKeySelGrp() );

    if ( !File::exists(basefp_.fullPath()) )
    {
	if  ( !File::createDir(basefp_.fullPath()) )
	{
	    BufferString msg( "Cannot create " );
	    msg += sKeySelGrp();
	    msg += "for crossplot selctions. Check write permissions";
	    uiMSG().error( msg );
	    return false;
	}
    }

    return true;
}


bool uiSGSelGrp::setSelGrpSetNames( const BufferStringSet& nms ) const
{
    BufferString fnm;
    FilePath fp( basefp_ );
    fp.add( sKeyIdxFileName() );
    SafeFileIO sfio( fp.fullPath(), true );
    if ( !sfio.open(false) )
    {
	uiMSG().error("Cannot open Crossplot Selection index.txt "
		      "file for write");
	return false;
    }

    ascostream astrm( sfio.ostrm() );
    astrm.putHeader( "Selection Group Set Names" );
    for ( int idx=0; idx<nms.size(); idx++ )
	astrm.put( nms.get(idx).buf() );
    astrm.newParagraph();

    if ( sfio.ostrm().good() )
	sfio.closeSuccess();
    else
    {
	sfio.closeFail();
	uiMSG().error( "Error during write to Crossplot Selection index file ."
		       "Check disk space." );
	return false;
    }

    return true;
}


bool uiSGSelGrp::getSelGrpSetNames( BufferStringSet& nms ) const
{
    FilePath fp( basefp_ );
    fp.add( sKeyIdxFileName() );
    SafeFileIO sfio( fp.fullPath(), true );
    if ( !sfio.open(true) )
    {
	uiMSG().error( "Index file for Crossplot Selection group not present" );
	return false;
    }

    ascistream astrm( sfio.istrm() );
    if ( atEndOfSection(astrm) )
	astrm.next();

    while ( !atEndOfSection(astrm) )
    {
	nms.add( astrm.keyWord() );
	astrm.next();
    }

    sfio.closeSuccess();
    return true;
}


SelGrpImporter::SelGrpImporter( const char* fnm )
{
    sd_ = StreamProvider( fnm ).makeIStream();
    if ( !sd_.usable() )
	{ errmsg_ = "Cannot open input file"; return; }
}


SelGrpImporter::~SelGrpImporter()
{
    sd_.close();
}


ObjectSet<SelectionGrp> SelGrpImporter::getSelections()
{
    ObjectSet<SelectionGrp> selgrpset;
    if ( !sd_.usable() )
    {
	errmsg_ = "Stream not usable";
	return selgrpset;
    }

    ascistream astrm( *sd_.istrm, true );

    if ( !astrm.isOfFileType(sKeyFileType) )
    {
	errmsg_ = "File type does not match with Crossplot Selection";
	return selgrpset;
    }

    int nrselgrps = 0;
    IOPar par( astrm );
    
    if ( par.hasKey(sKeyNrSelGrps) )
	par.get( sKeyNrSelGrps, nrselgrps );
    if ( par.hasKey(IOPar::compKey(sKey::Attribute,"X")) )
	par.get( IOPar::compKey(sKey::Attribute,"X"), xname_ );
    if ( par.hasKey(IOPar::compKey(sKey::Attribute,"Y")) )
	par.get( IOPar::compKey(sKey::Attribute,"Y"), yname_ );
    if ( par.hasKey(IOPar::compKey(sKey::Attribute,"Y2")) )
	par.get( IOPar::compKey(sKey::Attribute,"Y2"), y2name_ );

    for ( int selidx=0; selidx < nrselgrps; selidx++ )
    {
	PtrMan<IOPar> selgrppar = par.subselect( selidx );
	if ( !selgrppar ) continue;

	SelectionGrp* selgrp = new SelectionGrp();
	selgrp->usePar( *selgrppar );
	selgrpset += selgrp;
    }

    sd_.close();

    return selgrpset;
}


SelGrpExporter::SelGrpExporter( const char* fnm )
{
    sd_ = StreamProvider( fnm ).makeOStream();
    if ( !sd_.usable() )
	{ errmsg_ = "Cannot open output file"; return; }
}

SelGrpExporter::~SelGrpExporter()
{
    sd_.close();
}

bool SelGrpExporter::putSelections( const ObjectSet<SelectionGrp>& selgrps,
				    const char* xname, const char* yname,
				    const char* y2name )
{
    if ( !sd_.usable() ) return false;

    ascostream ostrm( *sd_.ostrm ); 
    std::ostream& strm = ostrm.stream();

    if ( !selgrps.size() )
    {
	errmsg_ = "No selections found";
	return false;
    }

    IOPar selectionpar;
    selectionpar.set( IOPar::compKey(sKey::Attribute,"X"), xname );
    selectionpar.set( IOPar::compKey(sKey::Attribute,"Y"), yname );
    if ( y2name )
	selectionpar.set( IOPar::compKey(sKey::Attribute,"Y2"), y2name );
    selectionpar.set( sKeyNrSelGrps, selgrps.size() );

    for ( int selidx=0; selidx<selgrps.size(); selidx++ )
    {
	const SelectionGrp* selgrp = selgrps[selidx];
	IOPar par,selgrppar;
	BufferString selstr;
	selstr.add( selidx );
	selgrp->fillPar( par );
	selgrppar.mergeComp( par, selstr.buf() );
	selectionpar.merge( selgrppar );
    }

    selectionpar.write( ostrm.stream(), sKeyFileType );
    const bool ret = strm.good();
    if ( !ret )
	errmsg_ = "Error during write";
    sd_.close();
    return ret;
}
mClass uiSGSelDlg : public uiDialog
{
public:

uiSGSelDlg( uiParent* p, bool forread )
    : uiDialog(p,uiDialog::Setup("Select Crossplot Selection Groups","",""))
    , forread_(forread)
{
    selgrp_ = new uiSGSelGrp( this, forread );
}


bool acceptOK( CallBacker* )
{
    bool ret = true;
    if ( forread_ )
	ret = selgrp_->getCurSelGrpSet( selgrpset_ );
    else
	selgrp_->addSelGrpSet();
    
    selgrpsetnm_ = selgrp_->selGrpSetNm();
    filenm_ =  selgrp_->getCurFileNm();
    xname_ = selgrp_->xName();
    yname_ = selgrp_->yName();
    y2name_ = selgrp_->y2Name();

    return ret;
}

const ObjectSet<SelectionGrp>& selGrpSet() const	{ return selgrpset_; }

const char* selGrSetNm() const 				{ return selgrpsetnm_; }
const BufferString& selGrpFileNm() const		{ return filenm_;}

const char* xName() const 				{ return xname_; }
const char* yName() const 				{ return yname_; }
const char* y2Name() const 				{ return y2name_; }

protected:

    ObjectSet<SelectionGrp>	selgrpset_;
    uiSGSelGrp* 		selgrp_;
    
    BufferString		filenm_;
    BufferString		xname_;
    BufferString		yname_;
    BufferString		y2name_;
    bool			forread_;

    BufferString		selgrpsetnm_;
};



uiSGSel::uiSGSel( uiParent* p, bool forread )
    : uiGroup(p)
    , forread_(forread)
    , selGrpSelected(this)
{
    inpfld_ = new uiGenInput( this, "Select Crossplot Selection Group" );
    selbut_ = new uiPushButton( this, "Select ..", mCB(this,uiSGSel,selectSGCB),
	    			false );
    selbut_->attach( rightTo, inpfld_ );
}


void uiSGSel::selectSGCB( CallBacker* )
{
    uiSGSelDlg dlg( this, forread_ );
    if ( dlg.go() )
    {
	inpfld_->setText( dlg.selGrSetNm() );
	selgrpfilenm_ = dlg.selGrpFileNm();
	selgrpset_ = dlg.selGrpSet();
	xname_ = dlg.xName();
	yname_ = dlg.yName();
	y2name_ = dlg.y2Name();
	selGrpSelected.trigger();
    }
}


bool uiSGSel::isOK() const
{
    return inpfld_->text() || !selgrpset_.isEmpty();
}


const char* uiSGSel::selGrpSetNm() const
{
    return inpfld_->text();
}


const char* uiSGSel::selGrpFileNm() const
{
    return selgrpfilenm_.buf();
}


uiReadSelGrp::uiReadSelGrp( uiParent* p, uiDataPointSetCrossPlotter& plotter )
    : uiDialog(p,uiDialog::Setup("Import Crossplot Selection", "","112.0.0"))
    , plotter_(plotter)
    , selgrpset_(plotter.selectionGrps())
    , y2selfld_(0)
{
    setCtrlStyle( DoAndStay );
    bool hasy2 = plotter.axisHandler(2);
    BufferStringSet nms;
    nms.add( plotter.axisHandler(0)->name() );
    nms.add( plotter.axisHandler(1)->name() );
    if ( hasy2 )
	nms.add( plotter.axisHandler(2)->name() );

    inpfld_ = new uiSGSel( this, true );
    inpfld_->selGrpSelected.notify( mCB(this,uiReadSelGrp,selectedCB) );

    uiLabeledComboBox* xselfld =
	new uiLabeledComboBox( this, plotter.axisHandler(0)->name() );
    xselfld_ = xselfld->box();
    xselfld->attach( alignedBelow, inpfld_ );
    xselfld_->display( false, false );
   
    uiLabeledComboBox* yselfld =
	new uiLabeledComboBox( this, plotter.axisHandler(1)->name() );
    yselfld_ = yselfld->box();
    yselfld->attach( alignedBelow, xselfld );
    yselfld_->display( false, false );
    
    ychkfld_ = new uiCheckBox( this, "Import Y1",
	    		       mCB(this,uiReadSelGrp,fldCheckedCB) );
    ychkfld_->attach( rightTo, yselfld );
    ychkfld_->display( false, false );
    
    if ( hasy2 )
    {
	uiLabeledComboBox* y2selfld =
	    new uiLabeledComboBox( this,plotter.axisHandler(2)->name() );
	y2selfld_ = y2selfld->box();
	y2selfld->attach( alignedBelow, yselfld );
	y2selfld_->display( false, false );
	y2chkfld_ = new uiCheckBox( this, "Import Y2",
				    mCB(this,uiReadSelGrp,fldCheckedCB) );
	y2chkfld_->attach( rightTo, y2selfld );
	y2chkfld_->display( false, false );
    }
}


void uiReadSelGrp::fldCheckedCB( CallBacker* cb )
{
    mDynamicCastGet(uiCheckBox*,chkbox,cb);
    if ( ychkfld_ == chkbox )
	yselfld_->setSensitive( ychkfld_->isChecked() );
    else if ( y2chkfld_ == chkbox )
	y2selfld_->setSensitive( y2chkfld_->isChecked() );
}


void uiReadSelGrp::selectedCB( CallBacker* )
{
    if ( !inpfld_->isOK() )
    {
	uiMSG().error( "Selected Selection-Group set is corrupted" );
	return;
    }

    selgrpset_ = inpfld_->selGrpSet();

    xname_ = inpfld_->xName();
    yname_ = inpfld_->yName();
    y2name_ = inpfld_->y2Name();

    BufferStringSet nms;
    nms.add( xname_ );
    nms.add( yname_ );
    if ( !y2name_.isEmpty() )
	nms.add( y2name_ );
    xselfld_->setEmpty();
    xselfld_->addItems( nms );
    xselfld_->display( true );
    yselfld_->setEmpty();
    yselfld_->addItems( nms );
    yselfld_->display( true );
    yselfld_->setSensitive( true );
    
    if ( y2selfld_ )
    {
	ychkfld_->display( true );
	yselfld_->setSensitive( false );
	y2selfld_->setEmpty();
	y2selfld_->addItems( nms );
	y2selfld_->display( true );
	y2selfld_->setSensitive( false );
	y2chkfld_->display( true );
    }
}


BufferStringSet uiReadSelGrp::getAvailableAxisNames() const
{
    BufferStringSet axisnm;
    axisnm.add( xselfld_->textOfItem(xselfld_->currentItem()) );
    if ( ychkfld_->isChecked() || !ychkfld_->isDisplayed() )
	axisnm.add( yselfld_->textOfItem(yselfld_->currentItem()) );
    if ( y2selfld_ && y2chkfld_->isChecked() )
	axisnm.add( y2selfld_->textOfItem(y2selfld_->currentItem()) );
    return axisnm;
}

#define mGetAxisVals \
    int xaxis = xselfld_->currentItem(); \
    int yaxis=-1; \
    int y2axis=-2; \
    if ( !ychkfld_->isDisplayed() || ychkfld_->isChecked() ) \
	yaxis = yselfld_->currentItem(); \
    if ( y2selfld_ ) \
    { \
	if ( y2chkfld_->isChecked() ) \
	    y2axis = y2selfld_->currentItem(); \
    }



bool uiReadSelGrp::checkSelectionArea( SelectionArea& actselarea,
				       const BufferStringSet& selaxisnm,
				       const BufferStringSet& axisnms,
				       bool hasalt )
{
    mGetAxisVals;

    if ( !selaxisnm.isPresent(axisnms.get(0)) )
	return false;

    if ( yaxis<0 && y2axis>=0 )
    {
	if ( (axisnms.validIdx(2) && !selaxisnm.isPresent(axisnms.get(2))) ||
   	     !selaxisnm.isPresent(axisnms.get(1))  )
	    return false;

	actselarea.axistype_ = SelectionArea::Y2;
    }
    else if ( yaxis>=0 && y2axis<0 )
    {
	if ( !selaxisnm.isPresent(axisnms.get(1)) )
	    return false;

	actselarea.axistype_ = SelectionArea::Y1;
    }
    else if ( yaxis>=0 && y2axis>=0 )
    {
	actselarea.axistype_ = SelectionArea::Both;
	if ( !hasalt )
	{
	    if ( !selaxisnm.isPresent(axisnms.get(1)) )
		actselarea.axistype_ = SelectionArea::Y2;
	    else if ( !selaxisnm.isPresent(axisnms.get(2)) )
		actselarea.axistype_ = SelectionArea::Y1;
	}
    }

    return true;
}


void uiReadSelGrp::fillRectangle( const SelectionArea& selarea,
				  SelectionArea& actselarea )
{
    mGetAxisVals;
    bool hasalt = !selarea.altyaxisnm_.isEmpty();
    if ( xaxis == 0 )
    {
	actselarea.worldrect_ =
	    ((yaxis == 2) && hasalt) ? selarea.altworldrect_
			 : selarea.worldrect_;
	actselarea.altworldrect_ =
	    ((yaxis == 2) && hasalt) ? selarea.worldrect_
			 : selarea.altworldrect_;
    }
    else 
    {
	const bool xis1 = xaxis == 1;
	const bool yis0 = yaxis == 0;
       uiWorldRect rect = selarea.worldrect_;
       uiWorldRect altrect = hasalt ? selarea.altworldrect_
				    : selarea.worldrect_;
       TypeSet<double> ltptval;
       ltptval += rect.topLeft().x;
       ltptval += rect.topLeft().y;
       ltptval += altrect.topLeft().y;
       
       TypeSet<double> rbptval;
       rbptval += rect.bottomRight().x;
       rbptval += rect.bottomRight().y;
       rbptval += altrect.bottomRight().y;

       const bool onlyy2 = actselarea.axistype_ == SelectionArea::Y2;
       const int yaxisnr = (yaxis<0 || onlyy2) ? y2axis : yaxis;

       actselarea.worldrect_ =
	   uiWorldRect( ltptval[xaxis], ltptval[yaxisnr],
		        rbptval[xaxis], rbptval[yaxisnr] );
       actselarea.worldrect_.checkCorners( true, false );
       if (hasalt && actselarea.axistype_==SelectionArea::Both)
       {
	   actselarea.altworldrect_ =
	       uiWorldRect( ltptval[xaxis], ltptval[y2axis],
			    rbptval[xaxis], rbptval[y2axis] );
	   actselarea.altworldrect_.checkCorners( true, false );
       }
    }
}


void uiReadSelGrp::fillPolygon( const SelectionArea& selarea,
			        SelectionArea& actselarea )
{
    mGetAxisVals;
    bool hasalt = !selarea.altyaxisnm_.isEmpty();
 
    if ( xaxis == 0 )
    {
	actselarea.worldpoly_ = ((yaxis) == 2 && hasalt)
	    ? selarea.altworldpoly_ : selarea.worldpoly_;
	actselarea.altworldpoly_ = ((yaxis == 2) && hasalt)
	    ? selarea.worldpoly_ : selarea.altworldpoly_;
    }
    else 
    {
	const bool xis1 = xaxis == 1;
	const bool yis0 = yaxis == 0;
       ODPolygon<double> worldpoly,altworldpoly;
       TypeSet< Geom::Point2D<double> > pts = selarea.worldpoly_.data();
       TypeSet< Geom::Point2D<double> > altpts =
				   hasalt ? selarea.altworldpoly_.data()
					  : selarea.worldpoly_.data();
       for ( int idx=0; idx<pts.size(); idx++ )
       {
	   TypeSet<double> ptval;
	   ptval += pts[idx].x; ptval += pts[idx].y;
	   ptval += altpts[idx].y;
	   const bool onlyy2 = actselarea.axistype_ == SelectionArea::Y2;
	   const int yaxisnr = (yaxis<0 || onlyy2) ? y2axis : yaxis;
	   
	   worldpoly.add( Geom::Point2D<double>(ptval[xaxis], ptval[yaxisnr]) );
	   
	   if (hasalt && actselarea.axistype_==SelectionArea::Both)
	       altworldpoly.add( Geom::Point2D<double>(ptval[xaxis],
				 ptval[y2axis]) );
       }

       actselarea.worldpoly_ = worldpoly;
       actselarea.altworldpoly_ = altworldpoly;
    }
}


bool uiReadSelGrp::adjustSelectionGrps()
{
    mGetAxisVals;
    if ( xaxis<0 || (yaxis<0 && y2axis<0) )
    {
	uiMSG().error( "Can't import selection group" );
	return false;
    }

    if ( xaxis==yaxis || yaxis==y2axis || y2axis==xaxis )
    {
	uiMSG().error( "Choose separate axis for different Axis" );
	return false;
    }

    BufferStringSet axisnms = getAvailableAxisNames();

    int selareaid = 0;
    bool selimportfailed = false;
    for ( int selidx=0; selidx<selgrpset_.size(); selidx++ )
    {
	SelectionGrp* selgrp = selgrpset_[selidx];

	SelectionGrp* newselgrp =
	    new SelectionGrp( selgrp->name(), selgrp->col_ );
	for ( int idx=0; idx<selgrp->size(); idx++ )
	{
	    const SelectionArea& selarea = selgrp->getSelectionArea( idx );
	    SelectionArea actselarea = SelectionArea( selarea.isrectangle_ );
	    bool hasalt = !selarea.altyaxisnm_.isEmpty();

	    if ( !checkSelectionArea(actselarea,selarea.getAxisNames(),
				     axisnms,hasalt) )
	    {
		selimportfailed = true;
		continue;
	    }
	   
	    if ( selarea.isrectangle_ )
		fillRectangle( selarea, actselarea );
	    else
		fillPolygon( selarea, actselarea );

	    if ( plotter_.checkSelArea(actselarea) )
	    {
		actselarea.id_ = selareaid;
		newselgrp->addSelection( actselarea );
		selareaid++;
	    }
	    else
		selimportfailed = true;
	}

	delete selgrpset_.replace( selgrpset_.indexOf(selgrp), newselgrp );
    }

    if ( selimportfailed )
	uiMSG().message( "Some selectionareas could not be imported \n"
			 "as they fall outside the value ranges of the plot" );

    return true;
}


void uiReadSelGrp::getInfo( const ObjectSet<SelectionGrp>& selgrps,
			    BufferString& msg )
{
    for ( int selgrpidx=0; selgrpidx<selgrps.size(); selgrpidx++ )
    {
	selgrps[selgrpidx]->getInfo( msg );
	msg += "\n";
    }
}


bool uiReadSelGrp::acceptOK( CallBacker* )
{
    if ( !adjustSelectionGrps() )
	return false;
    plotter_.reDrawSelections();
    return true;
}


uiExpSelectionArea::uiExpSelectionArea( uiParent* p,
					const ObjectSet<SelectionGrp>& selgrps,
					uiExpSelectionArea::Setup su )
    : uiDialog(p,uiDialog::Setup("Export Selection Area",
				 "Specify parameters",mTODOHelpID))
    , selgrps_(selgrps)
    , setup_(su)
{
    setCtrlStyle( DoAndStay );

    outfld_ = new uiSGSel( this, false );
}


bool uiExpSelectionArea::acceptOK( CallBacker* )
{
    if ( !outfld_->isOK() )
    {
	uiMSG().error( "Ouput not selected" );
	return false;
    }

    SelGrpExporter exp( outfld_->selGrpFileNm() );
    BufferString yaxisnm;
    if ( !exp.putSelections(selgrps_,setup_.xname_,
			    setup_.yname_,setup_.y2name_) )
	{ uiMSG().error(exp.errMsg()); return false; }

    uiMSG().message( "Output file created" );
    return false;
}
