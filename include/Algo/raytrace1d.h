#ifndef raytrace1d_h
#define raytrace1d_h
/*
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	K. Tingdahl
 Date:		Jan 2011
 RCS:		$Id: raytrace1d.h,v 1.14 2011-02-18 09:10:03 cvsbruno Exp $
________________________________________________________________________

*/

#include "ailayer.h"
#include "fixedstring.h"
#include "task.h"

template <class T> class Array2DImpl;
class IOPar;

mClass RayTracer1D : public ParallelTask
{
public:
    mClass Setup
    {
    public:
			Setup() 
			    : pdown_( true )
			    , pup_( true )
			    , sourcedepth_( 0 )
			    , receiverdepth_( 0 )
			{}

				mDefSetupMemb(bool,pdown);
				mDefSetupMemb(bool,pup);
				mDefSetupMemb(float,sourcedepth);
				mDefSetupMemb(float,receiverdepth);

	virtual void		fillPar(IOPar&) const;
	virtual bool		usePar(const IOPar&);

	static const char*	sKeyPWave()	{ return "Wavetypes"; }
	static const char*	sKeySRDepth()	{ return "SR Depths"; }
    };


				RayTracer1D(const Setup&);
    virtual			~RayTracer1D();			
    virtual const Setup&	setup() const		{ return setup_; }
    virtual void		setSetup(const Setup&);

    void		setModel(bool pmodel,const TypeSet<AILayer>&);
    			/*!<Note, if both p-model and s-model are set,
			    they should be identical with regards to their sizes
			    and the layers' depths. */
    void		setOffsets(const TypeSet<float>& offsets);

    const char*		errMsg() const { return errmsg_.str(); }

    			//Available after execution
    float		getSinAngle(int layeridx,int offsetidx) const;
    float*		getSinAngleData() const;

    float 		getTWT(int layeridx,int offsetidx) const;

protected:
    friend class	OffsetFromRayParam;

    od_int64		nrIterations() const;
    virtual bool	doPrepare(int);
    virtual bool	doWork(od_int64,od_int64,int);
    static int		findLayer(const TypeSet<AILayer>& model,
	    			  float targetdepth);

    			//Setup variables
    TypeSet<AILayer>	pmodel_;
    TypeSet<AILayer>	smodel_;
    TypeSet<float>	offsets_;
    Setup		setup_;

    				//Runtime variables
    int			sourcelayer_;
    int			receiverlayer_;
    int			firstlayer_;
    TypeSet<int>	offsetpermutation_;
    FixedString		errmsg_;

			//Results
    Array2DImpl<float>*	sini_;
    Array2DImpl<float>* twt_;
};


#endif
