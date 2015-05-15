#ifndef uiflatviewstdcontrol_h
#define uiflatviewstdcontrol_h
/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bert
 Date:          Mar 2007
 RCS:           $Id$
________________________________________________________________________

-*/

#include "uiflatviewmod.h"
#include "uiflatviewcontrol.h"
#include "uigroup.h"
#include "menuhandler.h"
#include "helpview.h"

class uiCheckBox;
class uiMenuHandler;
class uiToolButton;
class uiFlatViewColTabEd;
class uiGenInput;
class uiToolBar;


mExpClass(uiFlatView) uiFlatViewZoomLevelGrp : public uiGroup
{ mODTextTranslationClass(uiFlatViewZoomLevelGrp)
public:
    			uiFlatViewZoomLevelGrp(uiParent*,float&,float&,bool);
    void		commitInput();
    bool		saveGlobal() const;
protected:
    float&		x1pospercm_;
    float&		x2pospercm_;

    uiGenInput*		x1fld_;
    uiGenInput*		x2fld_;
    uiCheckBox*		saveglobalfld_;
};


/*!
\brief The standard tools to control uiFlatViewer(s).
*/

mExpClass(uiFlatView) uiFlatViewStdControl : public uiFlatViewControl
{ mODTextTranslationClass(uiFlatViewStdControl);
public:

    struct Setup
    {
			Setup( uiParent* p=0 )
			    : parent_(p)
			    , withcoltabed_(true)
			    , withedit_(false)
			    , withhanddrag_(true)
			    , withsnapshot_(true)
			    , withflip_(true)
			    , withrubber_(true)
			    , withzoombut_(true)
			    , isvertical_(false)
			    , withfixedaspectratio_(false)
			    , withhomebutton_(false)
			    , managescoltab_(true)
			    , initialx1pospercm_(mUdf(float))
			    , initialx2pospercm_(mUdf(float))
			    , intitialcentre_(uiWorldPoint::udf())
                            , tba_(-1)	{}	      	

	mDefSetupMemb(uiParent*,parent) //!< null => viewer's parent
	mDefSetupMemb(bool,withcoltabed)
	mDefSetupMemb(bool,withedit)
	mDefSetupMemb(bool,withhanddrag)
	mDefSetupMemb(int,tba)		//!< uiToolBar::ToolBarArea preference
	mDefSetupMemb(HelpKey,helpkey)
	mDefSetupMemb(bool,withflip)
	mDefSetupMemb(bool,withsnapshot)
	mDefSetupMemb(bool,withrubber)
	mDefSetupMemb(bool,withzoombut)
	mDefSetupMemb(bool,isvertical)
	mDefSetupMemb(bool,withfixedaspectratio)
	mDefSetupMemb(bool,managescoltab)
	mDefSetupMemb(bool,withhomebutton)
	mDefSetupMemb(float,initialx1pospercm)
	mDefSetupMemb(float,initialx2pospercm)
	mDefSetupMemb(uiWorldPoint,intitialcentre);
    };

    			uiFlatViewStdControl(uiFlatViewer&,const Setup&);
    			~uiFlatViewStdControl();
    virtual uiToolBar*	toolBar()		{ return tb_; }
    virtual uiFlatViewColTabEd* colTabEd()	{ return ctabed_; }
    void		setEditMode(bool yn);
    float		getCurrentPosPerCM(bool forx1) const;

    NotifierAccess*	editPushed();

protected:

    bool		mousepressed_;
    uiPoint		mousedownpt_;
    uiWorldRect		mousedownwr_;

    float		defx1pospercm_;
    float		defx2pospercm_;
    
    uiToolBar*		tb_;
    uiToolButton*	rubbandzoombut_;
    uiToolButton*	zoominbut_;
    uiToolButton*	zoomoutbut_;
    uiToolButton*	vertzoominbut_;
    uiToolButton*	vertzoomoutbut_;
    uiToolButton*	cancelzoombut_;
    uiToolButton*	sethomezoombut_;
    uiToolButton*	gotohomezoombut_;
    uiToolButton*	parsbut_;
    uiToolButton*	editbut_;

    uiFlatViewer&	vwr_;
    uiFlatViewColTabEd* ctabed_;

    const Setup		setup_;

    virtual void	finalPrepare();
    void		clearToolBar();
    void		updatePosButtonStates();
    void		doZoom(bool zoomin,bool onlyvertzoom,uiFlatViewer&);
    void		setViewToCustomZoomLevel(uiFlatViewer&);
    void		setGlobalZoomLevel(float x1pospercm,
					   float x2pospercm) const;

    virtual void	coltabChg(CallBacker*);
    virtual void	dispChgCB(CallBacker*);
    virtual void        zoomChgCB(CallBacker*);
    virtual void	rubBandUsedCB(CallBacker*);
    virtual void	dragModeCB(CallBacker*);
    void		flipCB(CallBacker*);
    void		helpCB(CallBacker*);
    void		handDragStarted(CallBacker*);
    void		handDragging(CallBacker*);
    void		handDragged(CallBacker*);
    void		aspectRatioCB(CallBacker*);
    void		keyPressCB(CallBacker*);
    void		homeZoomOptSelCB(CallBacker*);
    virtual void	parsCB(CallBacker*);
    virtual void	vwrAdded(CallBacker*) 	{}
    virtual void	wheelMoveCB(CallBacker*);
    virtual void	zoomCB(CallBacker*);
    virtual void	pinchZoomCB(CallBacker*);
    virtual void	cancelZoomCB(CallBacker*);
    virtual void	gotoHomeZoomCB(CallBacker*);

    virtual bool	handleUserClick(int vwridx);

    uiMenuHandler&      menu_;
    MenuItem           	propertiesmnuitem_;
    void                createMenuCB(CallBacker*);
    void                handleMenuCB(CallBacker*);

    HelpKey		helpkey_;
};

#endif

