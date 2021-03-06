/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bruno
 Date:          Jul 2010
________________________________________________________________________

-*/


#include "wellhorpos.h"

#include "emhorizon2d.h"
#include "emhorizon3d.h"
#include "emmanager.h"
#include "survinfo.h"
#include "welld2tmodel.h"
#include "welltrack.h"


WellHorIntersectFinder::WellHorIntersectFinder( const Well::Track& tr,
						const Well::D2TModel* d2t )
    : track_(tr)
    , d2t_(d2t)
{
    if ( d2t_ && d2t_->isEmpty() )
	d2t_ = 0;
}


void WellHorIntersectFinder::setHorizon( const DBKey& emid )
{
    hor2d_ = 0; hor3d_ = 0;
    const EM::Object* emobj = EM::MGR().getObject( emid );
    mDynamicCastGet(const EM::Horizon2D*,hor2d,emobj)
    mDynamicCastGet(const EM::Horizon3D*,hor3d,emobj)
    hor3d_ = hor3d; hor2d_ = hor2d;
}


float WellHorIntersectFinder::findZIntersection() const
{
    const float zstep = SI().zStep();
    const Interval<float>& dahrg = track_.dahRange();
    float zstart = d2t_ ? d2t_->getTime( dahrg.start, track_ ) : dahrg.start;
    float zstop = d2t_ ? d2t_->getTime( dahrg.stop, track_ ) : dahrg.stop;
    zstart = mMAX( SI().zRange().start, zstart );
    zstop = mMIN( SI().zRange().stop, zstop );

    float zval = zstart;
    bool isabove = true;
    bool firstvalidzfound = false;

    while ( zval < zstop )
    {
	const float dah = d2t_ ? d2t_->getDah( zval, track_ ) : zval;
	const Coord3& crd = track_.getPos( dah );
	const float horz = intersectPosHor( crd );

	if ( mIsUdf( horz ) )
	{
	    zval += zstep;
	    continue;
	}

	if ( !firstvalidzfound )
	{
	    isabove = zval >= horz;
	    firstvalidzfound = true;
	}

	if ( ( isabove && horz >= zval ) || ( !isabove && horz <= zval ) )
	    return horz;

	zval += zstep;
    }
    return mUdf( float );
}


float WellHorIntersectFinder::intersectPosHor( const Coord3& pos ) const
{
    const BinID& bid = SI().transform( pos.getXY() );
    if ( !SI().includes(bid) )
       return mUdf( float );

    if ( hor3d_ )
    {
	const EM::PosID subid = EM::PosID::getFromRowCol( bid );
	const Coord3& horpos = hor3d_->getPos( subid );
	const BinID horbid = SI().transform( horpos.getXY() );
	if ( bid == horbid )
	    return (float)horpos.z_;
    }
    else if ( hor2d_ )
	return hor2d_->getZValue( pos.getXY() );

    return  mUdf( float );
}
