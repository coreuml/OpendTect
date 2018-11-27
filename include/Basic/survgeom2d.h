#pragma once

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bert
 Date:		Aug 2010
________________________________________________________________________

-*/


#include "basicmod.h"
#include "survgeom.h"
class TrcKeyZSampling;
namespace PosInfo { class Line2DData; class Line2DPos; }


namespace Survey
{

class SubGeometry2D;


/*!\brief Geometry of a 2D Line.

  This object is not protected against MT updates. It's perfectly good for
  concurrent reading.

 */

mExpClass(Basic) Geometry2D : public Geometry
			    , public CallBacker
{
public:

    typedef PosInfo::Line2DData	Line2DData;
    typedef PosInfo::Line2DPos	Line2DPos;
    typedef float		spnr_type;
    typedef TypeSet<spnr_type>	SPNrSet;
    mUseType( SPNrSet,		size_type );

			Geometry2D(const char* lnm);
			Geometry2D(Line2DData*); //!<Line2DData becomes mine

    static bool		isPresent(GeomID);
    static const Geometry2D& get(const char* linenm);
    static const Geometry2D& get(GeomID);
    bool		isEmpty() const;
    bool		isDummy() const		    { return this == &dummy(); }

    GeomSystem		geomSystem() const override { return OD::LineBasedGeom;}
    const name_type&	name() const override;
    size_type		size() const;
    idx_type		indexOf(tracenr_type) const;
    tracenr_type	trcNr(idx_type) const;
    bool		includes(tracenr_type) const;
    Coord		getCoord(tracenr_type) const;
    spnr_type		getSPNr(tracenr_type) const;
    bool		findSP(spnr_type,tracenr_type&) const;
    void		getInfo(tracenr_type,Coord&,spnr_type&) const;

    dist_type		averageTrcDist() const	    { return avgtrcdist_; }
    dist_type		lineLength() const	    { return linelength_; }

    tracenr_type	nearestTracePosition(const Coord&,
					     dist_type* dist_to_line=0) const;
    tracenr_type	tracePosition(const Coord&,
				      dist_type maxdist=mUdf(dist_type)) const;

    Notifier<Geometry2D> objectChanged;

protected:

			Geometry2D();
			~Geometry2D();

    Line2DData&		data_;
    TypeSet<spnr_type>	spnrs_;
    mutable dist_type	avgtrcdist_			= mUdf(dist_type);
    mutable dist_type	linelength_			= mUdf(dist_type);

    Geometry3D*		gtAs3D() const	override	{ return 0; }
    Geometry2D*		gtAs2D() const	override
			{ return const_cast<Geometry2D*>(this); }

    friend class	SubGeometry2D;

    void		setFromLineData();

public:

    RelationType	compare(const Geometry2D&,bool usezrg) const;

    const Line2DData&	data() const		{ return data_; }
    Line2DData&		data()			{ return data_; }
    const SPNrSet&	spNrs() const		{ return spnrs_; }
    SPNrSet&		spNrs()			{ return spnrs_; }
    void		getSampling(TrcKeyZSampling&) const;

			// *you* should probably not be changing line geometries
    void		setEmpty() const;
    void		add(const Coord&,tracenr_type,spnr_type);
    void		commitChanges() const; //!< mandatory after any change

    static Geometry2D&	dummy();

};

} // namespace Survey
