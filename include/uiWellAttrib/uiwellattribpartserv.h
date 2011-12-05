#ifndef uiwellattribpartserv_h
#define uiwellattribpartserv_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Nanne Hemstra
 Date:          February 2004
 RCS:           $Id: uiwellattribpartserv.h,v 1.20 2011-12-05 09:05:44 cvssatyaki Exp $
________________________________________________________________________

-*/

#include "uiapplserv.h"

class DataPointSet;
class DataPointSetDisplayMgr;
class MultiID;
class NLAModel;
class uiWellAttribCrossPlot;

namespace Attrib { class DescSet; }
namespace WellTie { class uiTieWinMGRDlg; }

/*! \brief Part Server for Wells */

mClass uiWellAttribPartServer : public uiApplPartServer
{
public:
				uiWellAttribPartServer(uiApplService&);
				~uiWellAttribPartServer();

    void			setAttribSet(const Attrib::DescSet&);
    void			setNLAModel(const NLAModel*);
    const NLAModel*		getNLAModel()		{ return nlamodel;}
    void			importSEGYVSP();

    const char*			name() const		{ return "Wells"; }

    				// Services
    bool			createAttribLog(const MultiID&,int);
    bool			createLogCube(const MultiID&);
    void			doXPlot();

    void 			setDPSDispMgr(DataPointSetDisplayMgr* dispmgr )
				{ dpsdispmgr_ = dispmgr; }
    bool                        createD2TModel(const MultiID&);

protected:

    Attrib::DescSet*		attrset;
    const NLAModel*		nlamodel;
    bool			welltiedlgopened_;

    WellTie::uiTieWinMGRDlg*	welltiedlg_;
    uiWellAttribCrossPlot*	xplotwin2d_;
    uiWellAttribCrossPlot*	xplotwin3d_;
    DataPointSetDisplayMgr*	dpsdispmgr_;
    
    void                        closeWellTieDlg(CallBacker*);
    void                        surveyChangedCB(CallBacker*);
};

/*!\mainpage WellAttrib User Interface

  Here you will find all attribute handling regarding wells.
  The uiAttribWellPartServer delivers the services needed.

*/


#endif
