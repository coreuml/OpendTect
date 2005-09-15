#ifndef trackplane_h
#define trackplane_h

/*+
________________________________________________________________________

 CopyRight:	(C) dGB Beheer B.V.
 Author:	Kristofer Tingdahl
 Date:		4-11-2002
 RCS:		$Id: trackplane.h,v 1.5 2005-09-15 08:18:33 cvskris Exp $
________________________________________________________________________


-*/

#include "cubesampling.h"
#include "mathfunc.h"

class Plane3;


namespace MPE
{

/*!\brief

*/

class TrackPlane
{
public:
    			TrackPlane( const BinID& start,
			       const BinID& stop,
			       float time );

   			TrackPlane( const BinID& start,
			       const BinID& stop,
			       float starttime,
			       float stoptime );
			TrackPlane() {}

    bool		isVertical() const;
    const CubeSampling&	boundingBox() const { return cubesampling; }
    CubeSampling&	boundingBox() { return cubesampling; }

    Coord3		normal(const FloatMathFunction* t2d=0) const;
    float		distance(const Coord3&,
	    			 const FloatMathFunction* t2d=0) const;
    			/*!<\note does not check the plane's boundaries */
    const BinIDValue&	motion() const { return motion_; }
    void		setMotion( int inl, int crl, float z );

    void		computePlane(Plane3&) const;

    enum TrackMode	{ None, Extend, ReTrack, Erase, Move };
    void		setTrackMode(TrackMode tm)	{ trackmode = tm; }
    TrackMode		getTrackMode() const		{ return trackmode; }

protected:

    CubeSampling	cubesampling;
    BinIDValue		motion_;

    TrackMode		trackmode;
};

}; // Namespace

#endif
