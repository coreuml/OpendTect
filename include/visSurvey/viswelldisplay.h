#ifndef vissurvwell_h
#define vissurvwell_h

/*+
________________________________________________________________________

 CopyRight:	(C) de Groot-Bril Earth Sciences B.V.
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
 RCS:		$Id: viswelldisplay.h,v 1.10 2003-10-22 15:27:58 nanne Exp $
________________________________________________________________________


-*/

#include "visobject.h"
#include "vissurvobj.h"
#include "multiid.h"

class LineStyle;
template <class T> class Interval;

namespace visBase { class Well; class Transformation; };


namespace visSurvey
{
class Scene;

/*!\brief 


*/

class WellDisplay :	public visBase::VisualObjectImpl,
			public visSurvey::SurveyObject
{
public:
    static WellDisplay*		create()
				mCreateDataObj(WellDisplay);

    bool			setWellId(const MultiID&);
    const MultiID&		wellId() const 		{ return wellid; }

    const LineStyle&		lineStyle() const;
    void			setLineStyle(const LineStyle&);

    void			showWellName(bool);
    bool			wellNameShown() const;

    void			addMarkers();
    void			showMarkers(bool);
    bool			markersShown() const;
    void			showMarkerName(bool);
    bool			markerNameShown() const;
    void			setMarkerSize(int);
    int				markerSize() const;

    void			displayLog(int idx,int nr,
	    				   const Interval<float>&);
    				//!< idx: idx in Well::LogSet
    				//!< nr==1: left log; nr==2: right log
    const Color&		logColor(int) const;
    void			setLogColor(const Color&,int);
    void			showLogs(bool);
    bool			logsShown() const;
    void			showLogName(bool);
    bool			logNameShown() const;

    virtual void                fillPar(IOPar&,TypeSet<int>&) const;
    virtual int                 usePar(const IOPar&);

    void			setTransformation(visBase::Transformation*);
    visBase::Transformation*	getTransformation();

protected:
    virtual			~WellDisplay();

    visBase::Well*		well;

    MultiID			wellid;
    const bool			zistime;

    static const char*		ioobjidstr;
    static const char*		earthmodelidstr;
};

};


#endif
