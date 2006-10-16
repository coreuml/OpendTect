#ifndef uiodapplmgr_h
#define uiodapplmgr_h
/*+
________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        A.H. Bril
 Date:          May 2001
 RCS:           $Id: uiodapplmgr.h,v 1.41 2006-10-16 14:58:29 cvsbert Exp $
________________________________________________________________________

-*/

#include "uiodmain.h"

class uidTectMan;
class uiApplPartServer;
class uiApplService;
class uiAttribPartServer;
class uiEMAttribPartServer;
class uiEMPartServer;
class uiMPEPartServer;
class uiNLAPartServer;
class uiODApplService;
class uiPopupMenu;
class uiPickPartServer;
class uiSeisPartServer;
class uiSoViewer;
class uiVisPartServer;
class uiWellAttribPartServer;
class uiWellPartServer;

class Color;
class Coord;
class MultiID;
class ODSession;
namespace Pick { class Set; }

/*!\brief Application level manager - ties part servers together

  The uiODApplMgr instance can be accessed like:
  ODMainWin()->applMgr()
  For plugins it may be interesting to pop up one of the OpendTect standard
  dialogs at any point in time. The best way to do that is by calling one
  of the uiODApplMgr methods.

  A big part of this class is dedicated to handling the events from the various
  part servers.
 
 */

class uiODApplMgr : public CallBacker
{
public:

    uiPickPartServer*	pickServer()		{ return pickserv; }
    uiVisPartServer*	visServer()		{ return visserv; }
    uiSeisPartServer*	seisServer()		{ return seisserv; }
    uiAttribPartServer*	attrServer()		{ return attrserv; }
    uiEMPartServer*	EMServer() 		{ return emserv; }
    uiEMAttribPartServer* EMAttribServer()	{ return emattrserv; }
    uiWellPartServer*	wellServer()		{ return wellserv; }
    uiWellAttribPartServer* wellAttribServer()	{ return wellattrserv; }
    uiMPEPartServer*	mpeServer()		{ return mpeserv; }
    uiNLAPartServer*	nlaServer()		{ return nlaserv; }
    void		setNlaServer( uiNLAPartServer* s )
    			{ nlaserv = s; }
    uiApplService&	applService()
			{ return (uiApplService&)applservice; }


    // File menu operations
    int			manageSurvey();
    enum ObjType	{ Seis, Hor, Flt, Wll, Attr, NLA, Pick, Sess, Wvlt };
    enum ActType	{ Imp, Exp, Man };
    void		doOperation(ObjType,ActType,int opt=0);
    			//!< Not all combinations are available ...!
    void		importLMKFault();

    // Processing menu operations
    void		editAttribSet();
    bool		editNLA();
    void		createVol();
    void		createHorOutput(int);
    void		reStartProc();

    // View menu operations
    void		setWorkingArea();
    void		setZScale();
    void		setStereoOffset();

    // Utility menu operations
    void		batchProgs();
    void		pluginMan();
    void		crDevEnv();
    void		setFonts();
    void		manageShortcuts();

    // Tree menu services
    // Selections
    void		selectWells(ObjectSet<MultiID>&);
    void		selectHorizon(MultiID&);
    void		selectFault(MultiID&);
    void		selectStickSet(MultiID&);
    bool		selectAttrib(int id,int attrib);

    // PickSets
    bool		storePickSets();
    bool		storePickSet(const Pick::Set&);
    bool		storePickSetAs(const Pick::Set&);
    bool		setPickSetDirs(Pick::Set&);
    bool		pickSetsStored() const;

    // Work. Don't use unless expert.
    bool		getNewData(int visid,int attrib);
    bool		evaluateAttribute(int visid,int attrib);
    void		pageUpDownPressed(bool up);
    void		resetServers();
    void		modifyColorTable(int visid, int attrib );
    NotifierAccess*	colorTableSeqChange();
    void		manSurvCB(CallBacker*)	{ manageSurvey(); }
    void		manAttrCB(CallBacker*)	{ editAttribSet(); }
    void		outVolCB(CallBacker*)	{ createVol(); }

    void		enableMenusAndToolBars(bool);
    void		enableTree(bool);
    void		enableSceneManipulation(bool);
    			/*!<Turns on/off viewMode and enables/disables
			    the possibility to go to actMode. */

    CNotifier<uiODApplMgr,int> getOtherFormatData;
    			/*!<Is triggered with the visid when the
			    vispartserver wants data, but the format
			    (as reported by
			    uiVisPartServer::getAttributeFormat() ) is
			    uiVisPartServer::OtherFormat. If you manage
			    a visobject with OtherFormat, you can
			    connect here to be notified when the object
			    needs data. */

protected:

				uiODApplMgr(uiODMain&);
				~uiODApplMgr();

    uiODMain&		appl;
    uiODApplService&	applservice;

    uiPickPartServer*	pickserv;
    uiVisPartServer*	visserv;
    uiNLAPartServer*	nlaserv;
    uiAttribPartServer*	attrserv;
    uiSeisPartServer*	seisserv;
    uiEMPartServer*	emserv;
    uiEMAttribPartServer* emattrserv;
    uiWellPartServer*	wellserv;
    uiWellAttribPartServer*  wellattrserv;
    uiMPEPartServer*	mpeserv;

    bool		handleEvent(const uiApplPartServer*,int);
    void*		deliverObject(const uiApplPartServer*,int);

    bool		handleMPEServEv(int);
    bool		handleWellServEv(int);
    bool		handleEMServEv(int);
    bool		handlePickServEv(int);
    bool		handleVisServEv(int);
    bool		handleNLAServEv(int);
    bool		handleAttribServEv(int);

    void		coltabChg(CallBacker*);
    void		setHistogram(int visid,int attrib);
    void		setupRdmLinePreview(const TypeSet<Coord>&);
    void		cleanPreview();

    friend class	uiODApplService;

    inline uiODSceneMgr& sceneMgr()	{ return appl.sceneMgr(); }
    inline uiODMenuMgr&	 menuMgr()	{ return appl.menuMgr(); }

    friend class	uiODMain;
};


#endif
