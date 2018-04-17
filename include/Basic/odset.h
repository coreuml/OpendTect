#pragma once

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Feb 2009
________________________________________________________________________

-*/

# include "gendefs.h"


namespace OD
{

/*!
\brief Base class for all sets used in OpendTect.

Guaranteed are also:

* typedef of indices and sizes: size_type
* typedef of object contained: object_type
* a member 'size() const'
* a member 'removeRange(size_type start,size_type stop)'

*/

mExpClass(Basic) Set
{
public:

    virtual		~Set()					{}

    virtual od_int64	nrItems() const				= 0;
    virtual bool	validIdx(od_int64) const		= 0;
    virtual void	swapItems(od_int64,od_int64)		= 0;
    virtual void	erase()					= 0;

    inline bool		isEmpty() const		{ return nrItems() <= 0; }
    inline void		setEmpty()		{ erase(); }

    static inline od_int32	maxIdx32()	{ return 2147483647; }
    static inline od_int64	maxIdx64()	{ return 9223372036854775807LL; }
};

} // namespace



/*!\brief Removes a range from the set. */

template <class ODSET,class size_type>
inline void removeRange( ODSET& inst, size_type start, size_type stop )
{
    inst.removeRange( start, stop );
}


/*!\brief Adds all names from a set to another set with an add() function
	(typically a BufferStringSet) */

template <class ODSET,class SET>
inline void addNames( const ODSET& inp, SET& setwithadd )
{
    for ( auto obj : inp )
	if ( obj )
	    setwithadd.add( obj->name() );
}
