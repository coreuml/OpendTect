/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        K. Tingdahl
 Date:          October 2002
________________________________________________________________________

-*/
static const char* rcsID = "$Id: uiprintscenedlg.cc,v 1.43 2009-03-10 06:35:42 cvssatyaki Exp $";

#include "uiprintscenedlg.h"

#include "uibutton.h"
#include "uicombobox.h"
#include "mousecursor.h"
#include "uifileinput.h"
#include "uilabel.h"
#include "uimsg.h"
#include "uiobj.h"
#include "uisoviewer.h"
#include "uispinbox.h"

#include "filegen.h"
#include "filepath.h"
#include "ioman.h"
#include "iopar.h"
#include "keystrs.h"
#include "oddirs.h"
#include "ptrman.h"
#include "settings.h"
#include "visscene.h"

#include <Inventor/actions/SoToVRML2Action.h>
#include <Inventor/actions/SoWriteAction.h>
#include <Inventor/VRMLnodes/SoVRMLGroup.h>
#include <Inventor/SoOffscreenRenderer.h>
#include <Inventor/SoOutput.h>

static const char* sKeySnapshot = "snapshot";
static bool prevsavestate = true;
BufferString uiSaveImageDlg::dirname_ = "";
static StepInterval<float> sPixelSizeRange(1,9999,1);

#define mAttachToAbove( fld ) \
	if ( fldabove ) fld->attach( alignedBelow, fldabove ); \
	fldabove = fld

uiPrintSceneDlg::uiPrintSceneDlg( uiParent* p,
				  const ObjectSet<uiSoViewer>& vwrs )
    : uiSaveImageDlg(p)
    , viewers_(vwrs)
    , scenefld_(0)
    , dovrmlfld_(0)
{
    screendpi_ = SoOffscreenRenderer::getScreenPixelsPerInch();
    SbViewportRegion vp;
    SoOffscreenRenderer sor( vp );
    const int nrfiletypes = sor.getNumWriteFiletypes();

    bool showvrml = false;
    Settings::common().getYN( IOPar::compKey("dTect","Enable VRML"), showvrml );
    if ( nrfiletypes==0 && !showvrml )
    {
	new uiLabel( this,
	    "No output file types found.\n"
	    "Probably, 'libsimage.so' is not installed or invalid." );
	setCtrlStyle( LeaveOnly );
	return;
    }

    SbVec2s maxres = SoOffscreenRenderer::getMaximumResolution();
    sPixelSizeRange.stop = mMIN(maxres[0],maxres[1]);

    uiParent* fldabove = 0;
    if ( viewers_.size() > 1 )
    {
	BufferStringSet scenenms;
	for ( int idx=0; idx<viewers_.size(); idx++ )
	    scenenms.add( viewers_[idx]->getScene()->name() );

	scenefld_ = new uiLabeledComboBox( this, scenenms, "Make snapshot of" );
	scenefld_->box()->selectionChanged.notify( 
					mCB(this,uiPrintSceneDlg,sceneSel) );
	mAttachToAbove( scenefld_ );
    }

    if ( showvrml && nrfiletypes )
    {
	dovrmlfld_ = new uiGenInput( this, "Type of snapshot",
				     BoolInpSpec(true,"Scene","Image") );
	dovrmlfld_->valuechanged.notify( mCB(this,uiPrintSceneDlg,typeSel) );
	dovrmlfld_->setValue( false );
	mAttachToAbove( dovrmlfld_ );
    }

    if ( nrfiletypes>0 )
    {
	if ( showvrml )
	    createGeomInpFlds( dovrmlfld_ );
	else if ( scenefld_ )
	    createGeomInpFlds( scenefld_ );
	else
	    createGeomInpFlds( useparsfld_ );
    }
    fileinputfld_->attach( alignedBelow, dpifld_ );

    sceneSel(0);
    PtrMan<IOPar> ctiopar;
    getSettingsPar( ctiopar, BufferString("3D") );
    if ( ctiopar.ptr() )
	usePar( *ctiopar );
    updateFilter();
}


void uiPrintSceneDlg::getSupportedFormats( const char** imagefrmt,
					   const char** frmtdesc,
					   BufferString& filters )
{
    const bool vrml = dovrmlfld_ && dovrmlfld_->getBoolValue();
    if ( dovrmlfld_ && dovrmlfld_->getBoolValue() )
	filters = "VRML (*.wrl)";
    else
    {
	SbViewportRegion vp;
	SoOffscreenRenderer sor( vp );
	int idx = 0;
	while ( imagefrmt[idx] )
	{
	    if ( sor.isWriteSupported(imagefrmt[idx]) )
	    {
		if ( !filters.isEmpty() ) filters += ";;";
		filters += frmtdesc[idx];
	    }
	    idx++;
	}
    }
}


void uiPrintSceneDlg::setFldVals( CallBacker* )
{
    if ( useparsfld_->getBoolValue() )
    {
	lockfld_->setChecked( false );
	lockfld_->setSensitive( true );
	PtrMan<IOPar> ctiopar;
	getSettingsPar( ctiopar, BufferString("3D") );
	if ( ctiopar.ptr() )
	    usePar( *ctiopar );
    }
    else
    {
	//dpifld_->setValue( (int)screendpi_ );
	sceneSel( 0 );
	lockfld_->setChecked( true );
	lockfld_->setSensitive( false );
    }
}


void uiPrintSceneDlg::typeSel( CallBacker* )
{
    const bool dovrml = dovrmlfld_->getBoolValue();
    if ( heightfld_ )	heightfld_->display( !dovrml );
    if ( widthfld_ )	widthfld_->display( !dovrml );
    if ( unitfld_ )	unitfld_->display( !dovrml );
    if ( lockfld_ )	lockfld_->display( !dovrml );
    if ( dpifld_ )	dpifld_->display( !dovrml );
    updateFilter();
}


void uiPrintSceneDlg::sceneSel( CallBacker* )
{
    const int vwridx = scenefld_ ? scenefld_->box()->currentItem() : 0;
    const uiSoViewer* vwr = viewers_[vwridx];
    const SbVec2s& winsz = vwr->getViewportSizePixels();
    aspectratio_ = (float)winsz[0] / winsz[1];
    
    if ( useparsfld_->getBoolValue() )
	return;
    setSizeInPix( (int)winsz[0], (int)winsz[1] );
}


bool uiPrintSceneDlg::acceptOK( CallBacker* )
{
    if ( !filenameOK() ) return false;

    const int vwridx = scenefld_ ? scenefld_->box()->currentItem() : 0;
    const uiSoViewer* vwr = viewers_[vwridx];
    FilePath filepath( fileinputfld_->fileName() );
    dirname_ = filepath.pathOnly();

    MouseCursorChanger cursorchanger( MouseCursor::Wait );

    if ( dovrmlfld_ && dovrmlfld_->getBoolValue() )
    {
	if ( !uiMSG().askGoOn("The VRML output in in pre apha testing "
		    	      "status,\nis not officially supported and is \n"
			      "known to be very unstable.\n\n"
			      "Do you want to continue?") )
	{
	    return false;
	}

	SoToVRML2Action tovrml2;
	SoNode* root = vwr->getSceneGraph();
	root->ref();
	tovrml2.apply( root );
	SoVRMLGroup* newroot = tovrml2.getVRML2SceneGraph();
	newroot->ref();
	root->unref();

	SoOutput out;
	if ( !out.openFile( filepath.fullPath().buf() ) )
	{
	    BufferString msg =  "Cannot open file ";
	    msg += filepath.fullPath();
	    msg += ".";
	    
	    uiMSG().error( msg );
	    return false;
	}

	out.setHeaderString("#VRML V2.0 utf8");
	SoWriteAction wra(&out);
	wra.apply(newroot);
        out.closeFile();

	newroot->unref();
	return true;
    }


    if ( !widthfld_ ) return true;

    SbViewportRegion viewport;
    viewport.setWindowSize( mNINT(sizepix_.width()), mNINT(sizepix_.height()) );
    viewport.setPixelsPerInch( dpifld_->getfValue() );

    prevsavestate = saveButtonChecked();
    if ( prevsavestate )
	writeToSettings();
	
    PtrMan<SoOffscreenRenderer> sor = new SoOffscreenRenderer(viewport);

#define col2f(rgb) float(col.rgb())/255
    const Color col = vwr->getBackgroundColor();
    sor->setBackgroundColor( SbColor(col2f(r),col2f(g),col2f(b)) );

    SoNode* scenegraph = vwr->getSceneGraph();
    if ( !sor->render(scenegraph) )
    {
	uiMSG().error( "Cannot render scene" );
	return false;
    }

    const char* extension = getExtension();
    if ( !sor->writeToFile(filepath.fullPath().buf(),extension) )
    {
	uiMSG().error( "Couldn't write to specified file" );
	return false;
    }

    return true;
}


void uiPrintSceneDlg::writeToSettings()
{
    PtrMan<IOPar> ctiopar;
    getSettingsPar( ctiopar, BufferString("3D") );
    if ( ctiopar )
	fillPar( *ctiopar, false );
    settings_.mergeComp( *ctiopar, getStringFromInt(1) );
    if ( !settings_.write() )
	uiMSG().error( "Cannot write settings" );
}


const char* uiPrintSceneDlg::getExtension()
{
    if ( dovrmlfld_ && dovrmlfld_->getBoolValue() )
	return "wrl";

    return uiSaveImageDlg::getExtension();
}
