#ifndef wellimpasc_h
#define wellimpasc_h

/*+
________________________________________________________________________

 CopyRight:	(C) de Groot-Bril Earth Sciences B.V.
 Author:	Bert Bril
 Date:		Aug 2003
 RCS:		$Id: wellimpasc.h,v 1.2 2003-08-25 10:31:12 bert Exp $
________________________________________________________________________


-*/

#include "sets.h"
#include "bufstring.h"
#include "ranges.h"
#include "strmdata.h"
#include <iosfwd>

class MeasureUnit;


namespace Well
{
class Data;

class AscImporter
{
public:

			AscImporter( Data& d ) : wd(d)	{}
			~AscImporter();

    const char*		getTrack(const char*,bool first_is_surface);
    const char*		getD2T(const char*,bool istvd);

    class LasFileInfo
    {
    public:
				LasFileInfo()
				    : zrg(mUndefValue,-mUndefValue)
				    , undefval(-999.25)		{}

	ObjectSet<BufferString>	lognms;
	Interval<float>		zrg;
	float			undefval;

	BufferString		wellnm; //!< only info; not used by getLogs
    };

    const char*		getLogInfo(const char* lasfnm,LasFileInfo&) const;
    const char*		getLogs(const char* lasfnm,const LasFileInfo&,
	    			bool istvd=true);

protected:

    Data&		wd;

    mutable bool		wrap;
    mutable ObjectSet<MeasureUnit> convs;
    mutable StreamData		sd;
    void		parseHeader(char*,char*&,char*&,char*&) const;
    const char*		gtLogInfo(const char* lasfnm,LasFileInfo&,bool) const;

};

}; // namespace Well

#endif
