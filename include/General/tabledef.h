#ifndef tabledef_h
#define tabledef_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	A.H.Bril
 Date:		Oct 2006
 RCS:		$Id: tabledef.h,v 1.5 2006-11-03 13:18:49 cvsbert Exp $
________________________________________________________________________

-*/

#include "namedobj.h"
#include "sets.h"
#include "rowcol.h"
#include "bufstringset.h"
#include <iostream>

namespace Table
{

    enum ReqSpec	{ Optional=0, Required=1 };

/*!\brief Logical piece of information, present in tables.

 In most simple situations, you need to know the column of some data, or the
 row/col of a header. Then you just describe it as:
 FormatInfo fi( Table::Optional, "Sample rate" );

 In some cases, data can be present or offered in various ways. For example, a
 position in the survey can be given as inline/crossline or X and Y. This would
 be specified as follows:

 FormatInfo fi( "Position", Table::Required );
 fi.add( new BufferStringSet( {"Inline","Xline"}, 2 ) );
 fi.add( new BufferStringSet( {"X-coord","Y-coord"}, 2 ) );

*/

class FormatInfo : public NamedObject
{
public:

    			FormatInfo( ReqSpec rs, const char* elemnm )
			    : NamedObject((const char*)0)
			    , req_(rs)		{ init(elemnm); }
    			FormatInfo( const char* nm, ReqSpec rs,
				    const char* elemnm=0 )
			    : NamedObject(nm)
			    , req_(rs)		{ init(elemnm); }
			~FormatInfo()		{ deepErase( elements_ ); }

    void		add( BufferStringSet* bss ) { elements_ += bss; }
    bool		isOptional() const	{ return req_ == Optional; }

    /*!\brief Selected element/positioning */
    struct Selection
    {
			Selection() : elem_(0)	{}

	int		elem_;
	TypeSet<RowCol>	pos_;

	BufferStringSet	vals_;	//!< when !havePos(ifld)

	bool		havePos( int ifld ) const
	    		{ return ifld < pos_.size() && pos_[ifld].c() >= 0; }
	const char*	getVal( int ifld ) const
	    		{ return ifld >= vals_.size() ? "" :
				 vals_.get(ifld).buf(); }

    };

    ObjectSet<BufferStringSet>	elements_;
    ReqSpec		req_;
    mutable Selection	selection_;

protected:

    void		init( const char* elemnm )
    			{
			    if ( elemnm && *elemnm )
			    {
				BufferStringSet* s = new BufferStringSet;
				s->add( elemnm );
				add( s );
			    }
			}

};


/*!\brief description of input our output table format */

class FormatDesc : public NamedObject
{
public:
    			FormatDesc( const char* nm )
			    : NamedObject(nm)
			    , nrhdrlines_(0)
			    , tokencol_(-1)		{}
			~FormatDesc()
			{
			    deepErase( headerinfos_ );
			    deepErase( bodyinfos_ );
			}

    ObjectSet<FormatInfo> headerinfos_;
    ObjectSet<FormatInfo> bodyinfos_;

    int			nrhdrlines_;	//!< if < 0 token will be used
    BufferString	token_;
    int			tokencol_;	//!< if < 0 token can be in any col

};

}; // namespace Table


#endif
