/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        K. Tingdahl
 Date:          May 2002
________________________________________________________________________

-*/
static const char* rcsID = "$Id: vishorizondisplay.cc,v 1.133 2010-04-14 10:50:45 cvsjaap Exp $";

#include "vishorizondisplay.h"

#include "attribsel.h"
#include "bidvsetarrayadapter.h"
#include "coltabmapper.h"
#include "coltabsequence.h"
#include "datapointset.h"
#include "datacoldef.h"
#include "emhorizon3d.h"
#include "emsurfaceauxdata.h"
#include "emsurfaceedgeline.h"
#include "isocontourtracer.h"
#include "survinfo.h"
#include "mpeengine.h"
#include "posvecdataset.h"

#include "viscolortab.h"
#include "viscoord.h"
#include "visdrawstyle.h"
#include "visevent.h"
#include "vishingeline.h"
#include "vismarker.h"
#include "vismaterial.h"
#include "vismpe.h"
#include "vishorizonsection.h"
#include "visplanedatadisplay.h"
#include "vispolyline.h"
#include "visrandomtrackdisplay.h"
#include "vistexturechannel2rgba.h"
#include "vistexturechannels.h"
#include "vismultiattribsurvobj.h"
#include "visseis2ddisplay.h"
#include "vistransform.h"
#include "zaxistransform.h"


mCreateFactoryEntry( visSurvey::HorizonDisplay );

namespace visSurvey
{

const char* HorizonDisplay::sKeyTexture()	{ return "Use texture"; }
const char* HorizonDisplay::sKeyShift()		{ return "Shift"; }
const char* HorizonDisplay::sKeyWireFrame()	{ return "WireFrame on"; }
const char* HorizonDisplay::sKeyResolution()	{ return "Resolution"; }
const char* HorizonDisplay::sKeyEdgeLineRadius(){ return "Edgeline radius"; }
const char* HorizonDisplay::sKeyRowRange()	{ return "Row range"; }
const char* HorizonDisplay::sKeyColRange()	{ return "Col range"; }
const char* HorizonDisplay::sKeyIntersectLineMaterialID() 
{ return "Intsectline material id"; }


HorizonDisplay::HorizonDisplay()
    : parrowrg_( -1, -1, -1 )
    , parcolrg_( -1, -1, -1 )
    , curtextureidx_( 0 )
    , usestexture_( false )
    , useswireframe_( false )
    , translation_( 0 )
    , edgelineradius_( 3.5 )
    , validtexture_( false )
    , resolution_( 0 )
    //, zaxistransform_( 0 )
    , allowshading_( true )					
    , intersectionlinematerial_( 0 )	
    , displayintersectionlines_( true )
    , enabletextureinterp_( true )    
{
    maxintersectionlinethickness_ = 0.02 *
	mMAX( SI().inlDistance() * SI().inlRange(true).width(),
	      SI().crlDistance() * SI().crlRange(true).width() );

    as_ += new Attrib::SelSpec;
    coltabmappersetups_ += ColTab::MapperSetup();
    coltabsequences_ += ColTab::Sequence(ColTab::defSeqName());
    
    TypeSet<float> shift;
    shift += 0.0;
    curshiftidx_ += 0;
    BufferStringSet* attrnms = new BufferStringSet();
    attrnms->allowNull();
    userrefs_ += attrnms;
    shifts_ += new TypeSet<float>;
    enabled_ += true;
    datapackids_ += -1;

    material_->setAmbience( 0.3 );

    RefMan<visBase::Material> linemat = visBase::Material::create();
    linemat->setFrom( *material_ );
    linemat->setColor( nontexturecol_ );
    linemat->setDiffIntensity( 1 );
    linemat->setAmbience( 1 );
    setIntersectLineMaterial( linemat );
}


HorizonDisplay::~HorizonDisplay()
{
    deepErase(as_);
    if ( intersectionlinematerial_ ) 
	intersectionlinematerial_->unRef();

    coltabmappersetups_.erase();
    coltabsequences_.erase();

    setSceneEventCatcher( 0 );
    curshiftidx_.erase();

    if ( translation_ )
    {
	removeChild( translation_->getInventorNode() );
	translation_->unRef();
	translation_ = 0;
    }

    removeEMStuff();
    if ( zaxistransform_ )
    {
	for ( int idx=0; idx<intersectionlinevoi_.size(); idx++ )
	    zaxistransform_->removeVolumeOfInterest(intersectionlinevoi_[idx]);

	zaxistransform_->unRef();
    }

    DataPackMgr& dpman = DPM( DataPackMgr::FlatID() );
    for ( int idx=0; idx<datapackids_.size(); idx++ )
	dpman.release( datapackids_[idx] );

    deepErase( shifts_ );
}


void HorizonDisplay::displayIntersectionLines( bool yn )
{
    displayintersectionlines_ = yn;

    hasmoved.trigger();
    changedisplay.trigger();
}


bool HorizonDisplay::displaysIntersectionLines() const
{
    return displayintersectionlines_;
}


void HorizonDisplay::setDisplayTransformation( mVisTrans* nt )
{
    EMObjectDisplay::setDisplayTransformation( nt );

    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->setDisplayTransformation(transformation_);

    for ( int idx=0; idx<edgelinedisplays_.size(); idx++ )
	edgelinedisplays_[idx]->setDisplayTransformation(transformation_);

    for ( int idx=0; idx<intersectionlines_.size(); idx++ )
	intersectionlines_[idx]->setDisplayTransformation(transformation_);
}


bool HorizonDisplay::setZAxisTransform( ZAxisTransform* nz, TaskRunner* tr )
{
    if ( zaxistransform_ ) zaxistransform_->unRef();

    zaxistransform_ = nz;
    if ( zaxistransform_ ) zaxistransform_->ref();

    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->setZAxisTransform( nz, tr );

    return true;
}


//const ZAxisTransform* HorizonDisplay::getZAxisTransform() const
//{ return zaxistransform_; }


bool HorizonDisplay::setChannels2RGBA( visBase::TextureChannel2RGBA* t )
{
    RefMan<visBase::TextureChannel2RGBA> dummy( t );
    if ( sections_.size()!=1 )
	return EMObjectDisplay::setChannels2RGBA( t );

    EMObjectDisplay::setChannels2RGBA( 0 );
    sections_[0]->setChannels2RGBA( t );
    sections_[0]->getChannels2RGBA()->enableInterpolation(enabletextureinterp_);

    return true;
}


visBase::TextureChannel2RGBA* HorizonDisplay::getChannels2RGBA()
{
    return sections_.size()
	? sections_[0]->getChannels2RGBA()
	: EMObjectDisplay::getChannels2RGBA();
}


void HorizonDisplay::setSceneEventCatcher(visBase::EventCatcher* ec)
{
    EMObjectDisplay::setSceneEventCatcher( ec );

    for ( int idx=0; idx<sections_.size(); idx++ )
    {
	sections_[idx]->setSceneEventCatcher( ec );
	if ( ec && scene_ )
	    sections_[idx]->setRightHandSystem( scene_->isRightHandSystem() );
    }

    for ( int idx=0; idx<edgelinedisplays_.size(); idx++ )
	edgelinedisplays_[idx]->setSceneEventCatcher( ec );

    for ( int idx=0; idx<intersectionlines_.size(); idx++ )
	intersectionlines_[idx]->setSceneEventCatcher( ec );

}


EM::PosID HorizonDisplay::findClosestNode( const Coord3& pickedpos ) const
{
    const mVisTrans* ztrans = scene_->getZScaleTransform();
    Coord3 newpos = ztrans->transformBack( pickedpos );
    if ( transformation_ )
	newpos = transformation_->transformBack( newpos );

    const BinID pickedbid = SI().transform( newpos );
    const EM::SubID pickedsubid = pickedbid.getSerialized();
    TypeSet<EM::PosID> closestnodes;
    for ( int idx=sids_.size()-1; idx>=0; idx-- )
    {
	if ( !emobject_->isDefined( sids_[idx], pickedsubid ) )
	    continue;
	closestnodes += EM::PosID( emobject_->id(), sids_[idx], pickedsubid );
    }

    if ( closestnodes.isEmpty() ) return EM::PosID( -1, -1, -1 );

    EM::PosID closestnode = closestnodes[0];
    float mindist = mUdf(float);
    for ( int idx=0; idx<closestnodes.size(); idx++ )
    {
	const Coord3 coord = emobject_->getPos( closestnodes[idx] );
	const Coord3 displaypos = ztrans->transform(
		transformation_ ? transformation_->transform(coord) : coord );

	const float dist = displaypos.distTo( pickedpos );
	if ( !idx || dist<mindist )
	{
	    closestnode = closestnodes[idx];
	    mindist = dist;
	}
    }

    return closestnode;
}


void HorizonDisplay::edgeLineRightClickCB( CallBacker* )
{}


void HorizonDisplay::removeEMStuff()
{
    for ( int idx=0; idx<sections_.size(); idx++ )
    {
	removeChild( sections_[idx]->getInventorNode() );
	sections_[idx]->unRef();
    }

    sections_.erase();
    sids_.erase();

    while ( intersectionlines_.size() )
    {
	intersectionlines_[0]->unRef();
	intersectionlines_.remove(0);
	intersectionlineids_.remove(0);
	if ( zaxistransform_ )
	    zaxistransform_->removeVolumeOfInterest( intersectionlinevoi_[0] );
	intersectionlinevoi_.remove(0);

    }

    mDynamicCastGet( EM::Horizon3D*, emhorizon, emobject_ );
    if ( emhorizon )
	emhorizon->edgelinesets.addremovenotify.remove(
			mCB(this,HorizonDisplay,emEdgeLineChangeCB ));

    while ( !edgelinedisplays_.isEmpty() )
    {
	EdgeLineSetDisplay* elsd = edgelinedisplays_[0];
	edgelinedisplays_ -= elsd;
	elsd->rightClicked()->remove(
		 mCB(this,HorizonDisplay,edgeLineRightClickCB) );
	removeChild( elsd->getInventorNode() );
	elsd->unRef();
    }

    EMObjectDisplay::removeEMStuff();
}


bool HorizonDisplay::setEMObject( const EM::ObjectID& newid, TaskRunner* tr )
{
    if ( !EMObjectDisplay::setEMObject( newid, tr ) )
	return false;

    mDynamicCastGet( EM::Horizon3D*, emhorizon, emobject_ );
    if ( emhorizon ) 
	emhorizon->edgelinesets.addremovenotify.notify(
		mCB(this,HorizonDisplay,emEdgeLineChangeCB) );

    return true;
}


StepInterval<int> HorizonDisplay::geometryRowRange() const
{
    mDynamicCastGet(const EM::Horizon3D*, surface, emobject_ );
    if ( !surface ) return parrowrg_;
    
    return surface->geometry().rowRange();
}


StepInterval<int> HorizonDisplay::geometryColRange() const
{
    mDynamicCastGet(const EM::Horizon3D*, surface, emobject_ );
    if ( !surface ) return parcolrg_;
    
    return surface->geometry().colRange();
}


bool HorizonDisplay::updateFromEM( TaskRunner* tr )
{ 
    if ( !EMObjectDisplay::updateFromEM( tr ) )
	return false;

    updateSingleColor();
    return true;
}


void HorizonDisplay::updateFromMPE()
{
    const bool hastracker = MPE::engine().getTrackerByObject(getObjectID())>=0;
	
    //if ( hastracker && !restoresessupdate_ )
    //{
	//useWireframe( true );
	//useTexture( false );
    //}

    if ( geometryRowRange().nrSteps()<=1 || geometryColRange().nrSteps()<=1 )
	setResolution( 0, 0 ); //Automatic resolution

    EMObjectDisplay::updateFromMPE();
}


bool HorizonDisplay::addEdgeLineDisplay( const EM::SectionID& sid )
{
    mDynamicCastGet( EM::Horizon3D*, emhorizon, emobject_ );
    EM::EdgeLineSet* els = emhorizon 
	? emhorizon->edgelinesets.getEdgeLineSet(sid,false) : 0;

    if ( els )
    {
	bool found = false;
	for ( int idx=0; idx<edgelinedisplays_.size(); idx++ )
	{
	    if ( edgelinedisplays_[idx]->getEdgeLineSet()==els )
	    {
		found = true;
		break;
	    }
	}
	
	if ( !found )
	{
	    visSurvey::EdgeLineSetDisplay* elsd =
		visSurvey::EdgeLineSetDisplay::create();
	    elsd->ref();
	    elsd->setConnect(true);
	    elsd->setEdgeLineSet(els);
	    elsd->setRadius(edgelineradius_);
	    addChild( elsd->getInventorNode() );
	    elsd->setDisplayTransformation(transformation_);
	    elsd->rightClicked()->notify(
		    mCB(this,HorizonDisplay,edgeLineRightClickCB));
	    edgelinedisplays_ += elsd;
	}
    }

    return true;
}


void HorizonDisplay::useTexture( bool yn, bool trigger )
{
    if ( yn && !validtexture_ )
    {
	for ( int idx=0; idx<nrAttribs(); idx++ )
	{
	    if ( as_[idx]->id()==Attrib::SelSpec::cNoAttrib() )
	    {
		usestexture_ = yn;
		setDepthAsAttrib(idx);
		return;
	    }
	}
    }

    usestexture_ = yn;

    updateSingleColor();
    
    if ( trigger )
	changedisplay.trigger();
}


bool HorizonDisplay::usesTexture() const
{ return usestexture_; }


bool HorizonDisplay::showingTexture() const
{
    return validtexture_ && usesTexture();
}


bool HorizonDisplay::shouldUseTexture() const
{
    if ( !validtexture_ || !usestexture_ )
	 return false;
 
    for ( int idx=nrAttribs()-1; idx>=0; idx-- )
    {
 	if ( isAttribEnabled( idx ) )
 	    return true;
    }
 
    return false;
}


bool HorizonDisplay::getOnlyAtSectionsDisplay() const
{ return displayonlyatsections_; }


bool HorizonDisplay::canHaveMultipleAttribs() const
{ return true; }


int HorizonDisplay::nrTextures( int channel ) const
{
    if ( channel<0 || channel>=nrAttribs() || !sections_.size() ) 
	return 0;

    return sections_[0]->nrVersions( channel );
}


void HorizonDisplay::selectTexture( int channel, int textureidx )
{
    curtextureidx_ = textureidx;
    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->selectActiveVersion( channel, textureidx );

    if ( !as_.validIdx(channel) || !userrefs_.validIdx(channel) ||
	 userrefs_[channel]->isEmpty() )
	return;

    if ( !strcmp("Section ID",userrefs_[channel]->get(0)) )
	textureidx++;

    BufferString usrref = userrefs_[channel]->validIdx(textureidx) ?
	userrefs_[channel]->get(textureidx) : "<No name>";
    as_[channel]->setUserRef( userrefs_[channel]->get(textureidx) );
}


int HorizonDisplay::selectedTexture( int channel ) const
{
    if ( channel<0 || channel>=nrAttribs() || !sections_.size() ) 
	return 0;

    return sections_[0]->activeVersion( channel );
}


SurveyObject::AttribFormat HorizonDisplay::getAttributeFormat( int ) const
{
    return sections_.size() ? SurveyObject::RandomPos : SurveyObject::None;
}


int HorizonDisplay::nrAttribs() const
{ return as_.size(); }


bool HorizonDisplay::canAddAttrib( int nr ) const
{
    if ( !sections_.size() )
	return false;

    const int maxnr =  sections_[0]->getChannels2RGBA()->maxNrChannels();
    if ( !maxnr ) return true;

    return nrAttribs()+nr<=maxnr;
}


bool HorizonDisplay::canRemoveAttrib() const
{
    if ( !sections_.size() )
	return false;

    const int newnrattribs = nrAttribs()-1;
    return newnrattribs>=sections_[0]->getChannels2RGBA()->minNrChannels();
}


void HorizonDisplay::setAttribShift( int channel, const TypeSet<float>& shifts )
{
    (*shifts_[channel]) = shifts;
}


bool HorizonDisplay::addAttrib()
{
    as_ += new Attrib::SelSpec;
    TypeSet<float> shift;
    shift += 0.0;
    curshiftidx_ += 0;
    BufferStringSet* attrnms = new BufferStringSet();
    attrnms->allowNull();
    userrefs_ += attrnms;
    enabled_ += true;
    shifts_ += new TypeSet<float>;
    datapackids_ += -1;
    coltabmappersetups_ += ColTab::MapperSetup();
    coltabsequences_ += ColTab::Sequence(ColTab::defSeqName());

    const int curchannel = coltabmappersetups_.size()-1;
    for ( int idx=0; idx<sections_.size(); idx++ )
    {
	sections_[idx]->addChannel();
    	sections_[idx]->setColTabSequence( curchannel,
		coltabsequences_[curchannel] );

	sections_[idx]->setColTabMapperSetup( curchannel,
		coltabmappersetups_[curchannel], 0 );
    }

    return true;
}


bool HorizonDisplay::removeAttrib( int channel )
{
    if ( channel<0 || channel>=nrAttribs() )
       return true;

    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->removeChannel( channel );

    curshiftidx_.remove( channel );
    userrefs_.remove( channel );
    enabled_.remove( channel );
    delete shifts_.remove( channel );
    DPM( DataPackMgr::FlatID() ).release( datapackids_[channel] );
    datapackids_.remove( channel );
    coltabmappersetups_.remove( channel );
    coltabsequences_.remove( channel );
    delete as_[channel];
    as_.remove( channel );

    for ( int chan=channel; chan<nrAttribs(); chan++ )
    {
	for ( int idx=0; idx<sections_.size(); idx++ )
	    sections_[idx]->setColTabSequence( chan, coltabsequences_[chan] );
    }

    return true;
}


bool HorizonDisplay::swapAttribs( int a0, int a1 )
{
    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->swapChannels( a0, a1 );

    coltabmappersetups_.swap( a0, a1 );
    coltabsequences_.swap( a0, a1 );

    as_.swap( a0, a1 );
    enabled_.swap( a0, a1 );
    shifts_.swap( a0, a1 );
    curshiftidx_.swap( a0, a1 );
    userrefs_.swap( a0, a1 );
    datapackids_.swap( a0, a1 );
    return true;
}


void HorizonDisplay::setAttribTransparency( int channel, unsigned char nt )
{
    if ( channel<0 || channel>=nrAttribs() )
       return;

    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->setTransparency( channel, nt );
}


unsigned char HorizonDisplay::getAttribTransparency( int channel ) const
{  
    if ( channel<0 || channel>=nrAttribs() )
       return 0;

    return sections_.size() ? sections_[0]->getTransparency(channel) : 0;
}


void HorizonDisplay::enableAttrib( int channelnr, bool yn )
{
    enabled_[channelnr] = yn;
    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->getChannels2RGBA()->setEnabled( channelnr, yn );

    updateSingleColor();
}


bool HorizonDisplay::isAttribEnabled( int channel ) const
{
    if ( channel<0 || channel>=nrAttribs() )
       return false;

    return enabled_[channel];
}


void HorizonDisplay::allowShading( bool yn )
{
    allowshading_ = yn;
    for ( int idx=sections_.size()-1; idx>=0; idx-- )
	sections_[idx]->getChannels2RGBA()->allowShading( yn );

}


const Attrib::SelSpec* HorizonDisplay::getSelSpec( int channel ) const
{ return as_[channel]; }


void HorizonDisplay::setSelSpec( int channel, const Attrib::SelSpec& as )
{ (*as_[channel]) = as; }


void HorizonDisplay::setDepthAsAttrib( int channel )
{
    as_[channel]->set( "Depth", Attrib::SelSpec::cNoAttrib(), false, "" );

    TypeSet<DataPointSet::DataRow> pts;
    ObjectSet<DataColDef> defs;
    DataColDef depthdef( "Depth" );
    defs += &depthdef;
    DataPointSet positions( pts, defs, false, true );
    getRandomPos( positions, 0 );

    if ( !positions.size() ) return;

    BinIDValueSet& bivs = positions.bivSet();
    if ( bivs.nrVals()!=3 )
    {
	pErrMsg( "Hmm" );
	return;
    }

    int depthcol = 
	positions.dataSet().findColDef( depthdef, PosVecDataSet::NameExact );
    if ( depthcol==-1 )
	depthcol = 1;

    BinIDValueSet::Pos pos;
    while ( bivs.next(pos,true) )
    {
	float* vals = bivs.getVals(pos);
	if ( zaxistransform_ )
	{
	    vals[depthcol] = zaxistransform_->transform(
		    BinIDValue( bivs.getBinID(pos), vals[0] ) );
	}
	else
	    vals[depthcol] = vals[0];
    }

    createAndDispDataPack( channel, &positions, 0 );
}


void HorizonDisplay::createAndDispDataPack( int channel,
					    const DataPointSet* positions,
					    TaskRunner* tr )
{
    if ( !positions ) return;

    BufferStringSet* attrnms = new BufferStringSet();
    for ( int idx=0; idx<positions->nrCols(); idx++ )
	attrnms->add( positions->colDef(idx).name_ );
    userrefs_.replace( channel, attrnms );

    const bool isz = attrnms->size()>=1 &&
		     !strcmp(attrnms->get(0).buf(),"Depth");
    mDeclareAndTryAlloc( BIDValSetArrAdapter*, bvsarr, 
	    		 BIDValSetArrAdapter(positions->bivSet(),isz?0:2) );
    const char* catnm = isz ? "Geometry" : "Surface Data";
    const char* dpnm = isz ? "Depth"
			   : (attrnms->size()>1 ? attrnms->get(1).buf() : "");
    mDeclareAndTryAlloc(MapDataPack*,newpack,MapDataPack(catnm,dpnm,bvsarr));

    StepInterval<double> inlrg( bvsarr->inlrg_.start, bvsarr->inlrg_.stop,
				SI().inlStep() );
    StepInterval<double> crlrg( bvsarr->crlrg_.start, bvsarr->crlrg_.stop,
				SI().crlStep() );
    BufferStringSet dimnames;
    dimnames.add("X").add("Y").add("In-Line").add("Cross-line");
    newpack->setProps( inlrg, crlrg, true, &dimnames );
    DataPackMgr& dpman = DPM( DataPackMgr::FlatID() );
    dpman.add( newpack );
    setDataPackID( channel, newpack->id(), tr );
    setRandomPosData( channel, positions, tr );
}


void HorizonDisplay::getRandomPos( DataPointSet& data, TaskRunner* tr ) const
{
    data.bivSet().allowDuplicateBids(false);
    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->getDataPositions( data, getTranslation().z, 
					  sids_[idx], tr );

    data.dataChanged();
}


void HorizonDisplay::getRandomPosCache( int channel, DataPointSet& data ) const
{
    if ( channel<0 || channel>=nrAttribs() )
       return;

    data.bivSet().empty();
    for ( int idx=0; idx<userrefs_[channel]->size(); idx++ )
	data.dataSet().add( new DataColDef(userrefs_[channel]->get(idx)) );

    for ( int idx=0; idx<sections_.size(); idx++ )
    {
	const BinIDValueSet* cache = sections_[idx]->getCache( channel );
	if ( cache )
	{
	    data.bivSet().setNrVals( cache->nrVals(), false );
	    data.bivSet().append( *cache );
	}
    }

    data.dataChanged();
}


void HorizonDisplay::updateSingleColor()
{
    const bool usesinglecol = !shouldUseTexture();
    const Color col = usesinglecol  ? nontexturecol_ : Color::White();
    material_->setColor( col );
    if ( intersectionlinematerial_ ) 
	intersectionlinematerial_->setColor( nontexturecol_ );

    for ( int idx=0; idx<sections_.size(); idx++ )
    {
	sections_[idx]->useChannel( !usesinglecol );
	sections_[idx]->setWireframeColor( col );
    }
}


void HorizonDisplay::setRandomPosData( int channel, const DataPointSet* data,
				       TaskRunner* tr )
{
    if ( channel<0 || channel>=nrAttribs() )
       return;

    if ( !data || !data->size() )
    {
	validtexture_ = false;
	usestexture_ = false;
	updateSingleColor();
	return;
    }

    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->setTextureData( channel, data, sids_[idx], tr );

    //We should really scale here, and then update sections. This
    //works for single sections though.
    if ( sections_.size() && sections_[0]->getColTabMapperSetup( channel ) )
    {
	coltabmappersetups_[channel] =
	    *sections_[0]->getColTabMapperSetup( channel );
	coltabmappersetups_[channel].triggerRangeChange();
    }

    validtexture_ = true;
    usestexture_ = true;
    updateSingleColor();
}


bool HorizonDisplay::hasStoredAttrib( int channel ) const
{
    const char* userref = as_[channel]->userRef();
    return as_[channel]->id()==Attrib::SelSpec::cOtherAttrib() &&
	   userref && *userref;
}


bool HorizonDisplay::hasDepth( int channel ) const
{ return as_[channel]->id()==Attrib::SelSpec::cNoAttrib(); }


Coord3 HorizonDisplay::getTranslation() const
{
    if ( !translation_ ) return Coord3(0,0,0);

    Coord3 shift = translation_->getTranslation();
    shift.z *= -1; 
    return shift;
}


void HorizonDisplay::setTranslation( const Coord3& nt )
{
    if ( !translation_ )
    {
	translation_ = visBase::Transformation::create();
	translation_->ref();
	insertChild( 0, translation_->getInventorNode() );
    }

    Coord3 shift( nt ); shift.z *= -1;
    translation_->setTranslation( shift );

    setOnlyAtSectionsDisplay( displayonlyatsections_ );		/* retrigger */
}


bool HorizonDisplay::usesWireframe() const { return useswireframe_; }


void HorizonDisplay::useWireframe( bool yn )
{
    useswireframe_ = yn;

    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->useWireframe( yn );
}


void HorizonDisplay::setEdgeLineRadius(float nr)
{
    edgelineradius_ = nr;
    for ( int idx=0; idx<edgelinedisplays_.size(); idx++ )
	edgelinedisplays_[idx]->setRadius(nr);
}


float HorizonDisplay::getEdgeLineRadius() const
{ return edgelineradius_; }


void HorizonDisplay::removeSectionDisplay( const EM::SectionID& sid )
{
    const int idx = sids_.indexOf( sid );
    if ( idx<0 ) return;

    removeChild( sections_[idx]->getInventorNode() );
    sections_[idx]->unRef();
    sections_.remove( idx );
    sids_.remove( idx );
};


bool HorizonDisplay::addSection( const EM::SectionID& sid, TaskRunner* tr )
{
    RefMan<visBase::HorizonSection> surf = visBase::HorizonSection::create();
    surf->ref();
    surf->setDisplayTransformation( transformation_ );
    surf->setZAxisTransform( zaxistransform_, tr );

    mDynamicCastGet( EM::Horizon3D*, horizon, emobject_ );
    surf->setSurface( horizon->geometry().sectionGeometry(sid), true, tr );
   
    while ( surf->nrChannels()<nrAttribs() ) 
	surf->addChannel();

    for ( int idx=0; idx<nrAttribs(); idx++ )
    {
	surf->setColTabMapperSetup( idx, coltabmappersetups_[idx], 0 );
	surf->setColTabSequence( idx, coltabsequences_[idx] );
	surf->getChannels2RGBA()->setEnabled( idx, enabled_[idx] );
    }

    if ( !sections_.size() && channel2rgba_ )
    {
	surf->setChannels2RGBA( channel2rgba_ );
	EMObjectDisplay::setChannels2RGBA( 0 );
    }

    surf->getChannels2RGBA()->allowShading( allowshading_ );
    surf->getChannels2RGBA()->enableInterpolation( enabletextureinterp_ );
    surf->useWireframe( useswireframe_ );
    surf->setResolution( resolution_-1, tr );

    surf->setMaterial( 0 );
    const int index = childIndex(drawstyle_->getInventorNode());
    insertChild( index, surf->getInventorNode() );
    surf->turnOn( !displayonlyatsections_ );

    sections_ += surf;
    sids_ += sid;
    hasmoved.trigger();

    return addEdgeLineDisplay( sid );
}


void HorizonDisplay::enableTextureInterpolation( bool yn )
{
    if ( enabletextureinterp_==yn )
	return;

    enabletextureinterp_ = yn;
    for ( int idx=0; idx<sections_.size(); idx++ )
    {
	if ( !sections_[idx] || !sections_[idx]->getChannels2RGBA() )
	    continue;
	
	sections_[idx]->getChannels2RGBA()->enableInterpolation( yn );

	//Crap, but does not work otherwise
	if ( !yn && sections_[idx]->getChannels2RGBA()->canUseShading() )
	    sections_[idx]->getChannels()->touchMappedData();
	//End of crap
    }
}


void HorizonDisplay::setOnlyAtSectionsDisplay( bool yn )
{
    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->turnOn(!yn);

    EMObjectDisplay::setOnlyAtSectionsDisplay( yn );

    for ( int idx=0; yn && idx<intersectionlines_.size(); idx++ )
	intersectionlines_[idx]->setMaterial( intersectionlinematerial_ );
}


visBase::Material* HorizonDisplay::getMaterial()
{
    return material_;
}


void HorizonDisplay::setIntersectLineMaterial( visBase::Material* nm )
{
    if ( intersectionlinematerial_ )
	intersectionlinematerial_->unRef();

    intersectionlinematerial_ = nm;
    if ( intersectionlinematerial_ )
	intersectionlinematerial_->ref();
}


void HorizonDisplay::emChangeCB( CallBacker* cb )
{
    EMObjectDisplay::emChangeCB( cb );
    mCBCapsuleUnpack(const EM::EMObjectCallbackData&,cbdata,cb);
    mDynamicCastGet(const EM::Horizon3D*,emhorizon,emobject_);
    if ( cbdata.event==EM::EMObjectCallbackData::PositionChange )
    {
	validtexture_ = false;
	const EM::SectionID sid = cbdata.pid0.sectionID();
	const int idx = sids_.indexOf( sid );
	if ( idx>=0 && idx<sections_.size() )
	    sections_[idx]->inValidateCache(-1);
    }
    else if ( cbdata.event==EM::EMObjectCallbackData::PrefColorChange )
	nontexturecol_ = emobject_->preferredColor();

    updateSingleColor();
}


void HorizonDisplay::emEdgeLineChangeCB(CallBacker* cb)
{
    mCBCapsuleUnpack( EM::SectionID, section, cb );

    mDynamicCastGet(const EM::Horizon3D*,emhorizon,emobject_);
    if ( !emhorizon ) return;

    if ( emhorizon->edgelinesets.getEdgeLineSet( section, false ) )
	 addEdgeLineDisplay(section);
    else
    {
	for ( int idx=0; idx<edgelinedisplays_.size(); idx++ )
	{
	    if (edgelinedisplays_[idx]->getEdgeLineSet()->getSection()==section)
  	    {
		EdgeLineSetDisplay* elsd = edgelinedisplays_[idx--];
		edgelinedisplays_ -= elsd;
		elsd->rightClicked()->remove(
			 mCB( this, HorizonDisplay, edgeLineRightClickCB ));
		removeChild( elsd->getInventorNode() );
		elsd->unRef();
		break;
	    }
	}
    }
}


int HorizonDisplay::nrResolutions() const
{ 
    return sections_.size() ? sections_[0]->nrResolutions()+1 : 1; 
}


BufferString HorizonDisplay::getResolutionName( int res ) const
{
    BufferString str;
    if ( !res ) str = "Automatic";
    else
    {
	res--;
	int val = 1;
	for ( int idx=0; idx<res; idx++ )
	    val *= 2;

	if ( val==2 ) 		str = "Half";
	else if ( val==1 ) 	str = "Full";
	else 			{ str = "1 / "; str += val; }
    }

    return str;
}


int HorizonDisplay::getResolution() const
{
    return sections_.size() ? sections_[0]->currentResolution()+1 : 0;
}


void HorizonDisplay::setResolution( int res, TaskRunner* tr )
{
    resolution_ = res;
    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->setResolution( res-1, tr );
}


const ColTab::Sequence* HorizonDisplay::getColTabSequence( int channel ) const
{
    if ( channel<0 || channel>=nrAttribs() )
       return 0;

    return sections_.size()
	? sections_[0]->getColTabSequence( channel )
	: &coltabsequences_[channel];
}


bool HorizonDisplay::canSetColTabSequence() const
{
    if ( !usesTexture() || !sections_.size() ||
	 !sections_[0]->getChannels2RGBA() )
	return false;

    return sections_[0]->getChannels2RGBA()->canSetSequence();
}


void HorizonDisplay::setColTabSequence( int chan, const ColTab::Sequence& seq,
       					TaskRunner* )
{
    if ( chan<0 || chan>=nrAttribs() )
       return;

    coltabsequences_[chan] = seq;

    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->setColTabSequence( chan, seq );
}


void HorizonDisplay::setColTabMapperSetup( int channel,
			   const ColTab::MapperSetup& ms, TaskRunner* tr )
{
    if ( channel<0 || channel>=nrAttribs() )
       return;

    for ( int idx=0; idx<sections_.size(); idx++ )
	sections_[idx]->setColTabMapperSetup( channel, ms, tr );

    //We should really scale here, and then update sections. This
    //works for single sections though.
    if ( sections_.size() && sections_[0]->getColTabMapperSetup( channel ) )
    {
	coltabmappersetups_[channel] =
	    *sections_[0]->getColTabMapperSetup( channel );
    }
}


const ColTab::MapperSetup* HorizonDisplay::getColTabMapperSetup( int ch ) const
{
    if ( ch<0 || ch>=nrAttribs() )
       return 0;

    return &coltabmappersetups_[ch];
}


const TypeSet<float>* HorizonDisplay::getHistogram( int attrib ) const
{
    if ( !sections_.size() )
	return 0;

    return sections_[0]->getHistogram( attrib );
}


float HorizonDisplay::calcDist( const Coord3& pickpos ) const
{
    if ( !emobject_ ) return mUdf(float);

    const mVisTrans* utm2display = scene_->getUTM2DisplayTransform();
    const Coord3 xytpos = utm2display->transformBack( pickpos );
    mDynamicCastGet(const EM::Horizon3D*,hor,emobject_)
    if ( hor )
    {
	const BinID bid = SI().transform( xytpos );
	const EM::SubID bidid = bid.getSerialized();
	TypeSet<Coord3> positions;
	for ( int idx=sids_.size()-1; idx>=0; idx-- )
	{
	    if ( !emobject_->isDefined( sids_[idx], bidid ) )
		continue;

	    positions += emobject_->getPos( sids_[idx], bidid );
	}

	float mindist = mUdf(float);
	for ( int idx=0; idx<positions.size(); idx++ )
	{
	    const float zfactor = scene_ ? scene_->getZScale(): SI().zScale();
	    const Coord3& pos = positions[idx] + getTranslation()/zfactor;
	    const float dist = fabs(xytpos.z-pos.z);
	    if ( dist < mindist ) mindist = dist;
	}

	return mindist;
    }

    return mUdf(float);
}


float HorizonDisplay::maxDist() const
{
    return SI().zRange(true).step;
}


visBase::HorizonSection* HorizonDisplay::getHorizonSection( 
	const EM::SectionID& sid )
{
    for ( int idx=0; idx<sids_.size(); idx++ )
    {
	if ( sids_[idx]==sid )
	    return sections_[idx];
    }

    return 0;
}


EM::SectionID HorizonDisplay::getSectionID( int visid ) const
{
    for ( int idx=0; idx<sections_.size(); idx++ )
    {
	if ( sections_[idx]->id()==visid )
		return sids_[idx];
    }

    return -1;
}


void HorizonDisplay::getMousePosInfo( const visBase::EventInfo& eventinfo,
				       Coord3& pos, BufferString& val,
				       BufferString& info ) const
{
    EMObjectDisplay::getMousePosInfo( eventinfo, pos, val, info );
    if ( !emobject_ || !usesTexture() ) return;

    const EM::SectionID sid =
	EMObjectDisplay::getSectionID(&eventinfo.pickedobjids);

    const int sectionidx = sids_.indexOf( sid );
    if ( sectionidx<0 || sectionidx>=sections_.size() ) 
	return;

    const BinID bid( SI().transform(pos) );

    for ( int idx=as_.size()-1; idx>=0; idx-- )
    {
	if ( as_[idx]->id().isUnselInvalid() )
	    return;

	if ( !sections_[sectionidx]->getChannels2RGBA()->isEnabled(idx) || 
	      sections_[sectionidx]->getTransparency(idx)==255 )
	    continue;

	const BinIDValueSet* bidvalset = sections_[sectionidx]->getCache( idx );
	if ( !bidvalset || bidvalset->nrVals()<2 ) continue;

	const BinIDValueSet::Pos setpos = bidvalset->findFirst( bid );
	if ( !setpos.valid() )
	    continue;

	const float* vals = bidvalset->getVals( setpos );
	int curtexture = selectedTexture(idx);
	if ( curtexture>bidvalset->nrVals()-2 ) curtexture = 0;

	const float fval = vals[curtexture+2];
	bool islowest = true;
	for ( int idy=idx-1; idy>=0; idy-- )
	{
	    if ( !sections_[sectionidx]->getCache(idy) || 
		 !isAttribEnabled(idy) ||
		 sections_[sectionidx]->getTransparency(idy)==255 )
		continue;
							                 
		islowest = false;
		break;
	}    

	if ( !islowest )
	{
	    const Color col = getColTabSequence(idx) ? 
		getColTabSequence(idx)->color(fval) : Color();
	    if ( col.t()==255 )
		continue;
	}
	    
	if ( !mIsUdf(fval) )
	{
	    val = fval;
	    float attribshift = 0;
	    const TypeSet<float>& attribshifts = *shifts_[idx];
	    const int version = selectedTexture( idx );
	    if ( attribshifts.validIdx(version) )
		attribshift = attribshifts[version] * SI().zFactor();

	    const float zshift = getTranslation().z*SI().zFactor();

	    const bool hasshift = !mIsZero(attribshift,0.1) ||
				  !mIsZero(zshift,0.1);
 	    if ( as_.size() > 1 || hasshift )
	    {
		BufferString channelstr = "(";
		bool first = true;
		if ( as_.size()>1 )
		{
		    channelstr += as_[idx]->userRef();
		    first = false;
		}

		if ( hasshift )
		{
		    if ( !first )
			channelstr += ", ";
		    channelstr += "shift=";
		    channelstr += attribshift;
		}

		channelstr += ")";
		val.replaceAt( cValNameOffset(), channelstr );
	    }
	}

	return;
    }
}


#define mEndLine \
{ \
    if ( cii && line->getCoordIndex(cii-1)!=-1 ) \
    { \
	if ( cii==1 || line->getCoordIndex(cii-2)==-1 ) \
	    line->getCoordinates()->removePos( line->getCoordIndex(--cii) ); \
	else \
	    line->setCoordIndex(cii++,-1); \
    } \
} 


#define mTraverseLine( linetype,startbid,faststop,faststep,slowdim,fastdim ) \
\
    const StepInterval<int> inlrg = horizon->geometry().rowRange(sid);\
    const StepInterval<int> crlrg = horizon->geometry().colRange(sid); \
\
    const int target##linetype = cs.hrg.start.linetype; \
    if ( !linetype##rg.includes(target##linetype) ) \
    { \
	mEndLine; \
	continue; \
    } \
 \
    const int rgindex = linetype##rg.getIndex(target##linetype); \
    const int prev##linetype = linetype##rg.atIndex(rgindex); \
    const int next##linetype = prev##linetype<target##linetype \
	? linetype##rg.atIndex(rgindex+1) \
	: prev##linetype; \
 \
    for ( BinID bid=startbid; bid[fastdim]<=faststop; bid[fastdim]+=faststep ) \
    { \
	if ( !cs.hrg.includes(bid) ) \
	{ \
	    mEndLine; \
	    continue; \
	} \
 \
	BinID prevbid( bid ); prevbid[slowdim] = prev##linetype; \
	BinID nextbid( bid ); nextbid[slowdim] = next##linetype; \
	Coord3 prevpos(horizon->getPos(sid,prevbid.getSerialized())); \
	if ( zaxistransform_ ) prevpos.z =zaxistransform_->transform(prevpos); \
	Coord3 pos = prevpos; \
	if ( nextbid!=prevbid && prevpos.isDefined() ) \
	{ \
	    Coord3 nextpos = \
		horizon->getPos(sid,nextbid.getSerialized()); \
	    if ( zaxistransform_ ) nextpos.z = \
	    	zaxistransform_->transform(nextpos); \
	    if ( nextpos.isDefined() ) \
	    { \
		const float frac = float( target##linetype - prev##linetype ) \
				   / linetype##rg.step; \
		pos = (1-frac)*prevpos + frac*nextpos; \
	    } \
	} \
 \
	if ( !pos.isDefined() || !cs.zrg.includes(pos.z) ) \
	{ \
	    mEndLine; \
	    continue; \
	} \
 \
	line->setCoordIndex(cii++, line->getCoordinates()->addPos(pos)); \
    } \
    mEndLine


static void drawHorizonOnRandomTrack( const TypeSet<Coord>& trclist, 
			const StepInterval<float>& zrg, 
			const EM::Horizon3D* hor, const EM::SectionID&  sid, 
			const ZAxisTransform* zaxistransform, 
			visBase::IndexedShape* line, int& cii )
{
    const StepInterval<int> inlrg = hor->geometry().rowRange( sid );
    const StepInterval<int> crlrg = hor->geometry().colRange( sid );

    int startidx; 
    int stopidx = 0; 
    int jumpstart = 0;

    while ( true )
    {
	startidx = stopidx;
	while ( startidx<trclist.size()-1 && 
		trclist[startidx]==trclist[startidx+1] ) startidx++;

	stopidx = startidx + 1;
	while ( stopidx<trclist.size()-1 && 
		trclist[stopidx]!=trclist[stopidx+1] ) stopidx++;

	if ( stopidx >= trclist.size() )
	    break;

	const Coord startcrd = SI().binID2Coord().transform(trclist[startidx]);
	const Coord stopcrd = SI().binID2Coord().transform(trclist[stopidx]);

	for ( int cidx=startidx+jumpstart; cidx<=stopidx; cidx++ )
	{
	    const int inlidx = inlrg.getIndex( trclist[cidx].x );
	    const int inl0 = inlrg.atIndex( inlidx );
	    const int inl1 = inl0<trclist[cidx].x ? 
			     inlrg.atIndex( inlidx+1 ) : inl0;

	    const int crlidx = crlrg.getIndex( trclist[cidx].y );
	    const int crl0 = crlrg.atIndex( crlidx );
	    const int crl1 = crl0<trclist[cidx].y ? 
			     crlrg.atIndex( crlidx+1 ) : crl0;

	    Coord3 p00 = hor->getPos( sid, BinID(inl0,crl0).getSerialized() );
	    Coord3 p01 = hor->getPos( sid, BinID(inl0,crl1).getSerialized() );
	    Coord3 p10 = hor->getPos( sid, BinID(inl1,crl0).getSerialized() );
	    Coord3 p11 = hor->getPos( sid, BinID(inl1,crl1).getSerialized() );

	    if ( zaxistransform )
	    {
		p00.z = zaxistransform->transform( p00 );
		p01.z = zaxistransform->transform( p01 );
		p10.z = zaxistransform->transform( p10 );
		p11.z = zaxistransform->transform( p11 );
	    }

	    if ( p00.isDefined() && p01.isDefined() && 
		 p10.isDefined() && p11.isDefined() )
	    {
		const float frac = float(cidx-startidx) / (stopidx-startidx);
		Coord3 pos = (1-frac) * Coord3(startcrd,0) +
		    		frac  * Coord3(stopcrd, 0);
	    
		const float ifrac = (trclist[cidx].x - inl0) / inlrg.step;
		const float cfrac = (trclist[cidx].y - crl0) / crlrg.step;
		pos.z = (1-ifrac)*( (1-cfrac)*p00.z + cfrac*p01.z ) +
			   ifrac *( (1-cfrac)*p10.z + cfrac*p11.z );

		if ( zrg.includes(pos.z) )
		{
		    line->setCoordIndex( cii++, 
			    		 line->getCoordinates()->addPos(pos) );
		    continue;
		}
	    }
	    mEndLine; 
	}
	
	jumpstart = 1;
    }
    mEndLine; 
}


static void drawHorizonOnTimeSlice( const CubeSampling& cs, float zshift,
			const EM::Horizon3D* hor, const EM::SectionID&  sid, 
			const ZAxisTransform* zaxistransform, 
			visBase::IndexedShape* line, int& cii )
{
    const Array2D<float>* field = 
			hor->geometry().sectionGeometry(sid)->getArray(); 
    if ( !field ) return;

    if ( zaxistransform )
	field = hor->createArray2D( sid, zaxistransform );

    IsoContourTracer ictracer( *field );
    ictracer.setSampling( hor->geometry().rowRange(sid),
	   		  hor->geometry().colRange(sid) );
    ictracer.selectRectROI( cs.hrg.inlRange(), cs.hrg.crlRange() );
    ObjectSet<ODPolygon<float> > isocontours;
    ictracer.getContours( isocontours, cs.zrg.start-zshift, false );
    
    for ( int cidx=0; cidx<isocontours.size(); cidx++ )
    {
	const ODPolygon<float>& ic = *isocontours[cidx];
	for ( int vidx=0; vidx<ic.size(); vidx++ )
	{
	    const Geom::Point2D<float> vertex = ic.getVertex( vidx );
	    Coord vrtxcoord( vertex.x, vertex.y );
	    vrtxcoord = SI().binID2Coord().transform( vrtxcoord );
	    const Coord3 pos( vrtxcoord, cs.zrg.start-zshift );
	    const int posidx = line->getCoordinates()->addPos( pos );
	    line->setCoordIndex( cii++, posidx ); 
	}
	
	if ( ic.isClosed() )
	{
	    const int posidx = line->getCoordIndex( cii-ic.size() );
	    line->setCoordIndex( cii++, posidx );
	}
	line->setCoordIndex( cii++, -1 );
    }

    if ( zaxistransform ) delete field;
    deepErase( isocontours );
}


void HorizonDisplay::updateIntersectionLines(
	    const ObjectSet<const SurveyObject>& objs, int whichobj )
{
    mDynamicCastGet(const EM::Horizon3D*,horizon,emobject_);
    if ( !horizon ) return;

    TypeSet<int> linestoupdate;
    BoolTypeSet lineshouldexist( intersectionlineids_.size(), false );

    if ( displayonlyatsections_ || displayintersectionlines_ )
    {
	for ( int idx=0; idx<objs.size(); idx++ )
	{
	    int objectid = -1;
	    mDynamicCastGet( const PlaneDataDisplay*, plane, objs[idx] );
	    if ( plane )
		objectid = plane->id();
	    
	    mDynamicCastGet( const MPEDisplay*, mped, objs[idx] );
	    if ( mped && mped->isDraggerShown() )
		objectid = mped->id();

	    mDynamicCastGet( const RandomTrackDisplay*, rtdisplay, objs[idx] );
	    if ( rtdisplay )
		objectid = rtdisplay->id();
	    
	    mDynamicCastGet( const Seis2DDisplay*, seis2ddisplay, objs[idx] );
	    if ( seis2ddisplay )
		objectid = seis2ddisplay->id();

	    if ( objectid==-1 )
		continue;

	    const int idy = intersectionlineids_.indexOf(objectid);
	    if ( idy==-1 )
	    {
		linestoupdate += objectid;
		lineshouldexist += true;
	    }
	    else
	    {
		if ( ( whichobj==objectid || whichobj==id() ) && 
		     linestoupdate.indexOf(whichobj)==-1 )
		{
		    linestoupdate += objectid;
		}

		lineshouldexist[idy] = true;
	    }
	}
    }

    for ( int idx=0; idx<intersectionlineids_.size(); idx++ )
    {
	if ( !lineshouldexist[idx] )
	{
	    removeChild( intersectionlines_[idx]->getInventorNode() );
	    intersectionlines_[idx]->unRef();

	    lineshouldexist.remove(idx);
	    intersectionlines_.remove(idx);
	    intersectionlineids_.remove(idx);
	    if ( zaxistransform_ )
	    {
		zaxistransform_->removeVolumeOfInterest(
			intersectionlinevoi_[idx] );
	    }

	    intersectionlinevoi_.remove(idx);
	    idx--;
	}
    }

    for ( int idx=0; idx<linestoupdate.size(); idx++ )
    {
	CubeSampling cs(false);
	mDynamicCastGet( PlaneDataDisplay*, plane,
			 visBase::DM().getObject(linestoupdate[idx]) );
	if ( plane )
	    cs = plane->getCubeSampling(true,true,-1);

	mDynamicCastGet( const MPEDisplay*, mped,
			 visBase::DM().getObject(linestoupdate[idx]) );
	if ( mped )
	    mped->getPlanePosition(cs);

	TypeSet<Coord> trclist;
	mDynamicCastGet( const RandomTrackDisplay*, rtdisplay,
			 visBase::DM().getObject(linestoupdate[idx]) );
	if ( rtdisplay )
	{
	    cs.zrg.setFrom( rtdisplay->getDataTraceRange() );
	    cs.zrg.step = SI().zStep();
	    TypeSet<BinID> tracebids;
	    rtdisplay->getDataTraceBids( tracebids );
	    for ( int bidx=0; bidx<tracebids.size(); bidx++ )
	    {
		cs.hrg.include( tracebids[bidx] );
		trclist += Coord( tracebids[bidx].inl, tracebids[bidx].crl );
	    }
	}

	mDynamicCastGet( const Seis2DDisplay*, seis2ddisplay,
			 visBase::DM().getObject(linestoupdate[idx]) );
	if ( seis2ddisplay )
	{
	    cs.zrg.setFrom( seis2ddisplay->getZRange(false) );
	    cs.zrg.step = SI().zStep();
	    const Interval<int>& trcnrrg = seis2ddisplay->getTraceNrRange();
	    for ( int trcnr=trcnrrg.start; trcnr<=trcnrrg.stop; trcnr++ )
	    {
		Coord crd = seis2ddisplay->getCoord( trcnr );
		crd = SI().binID2Coord().transformBackNoSnap( crd );
		if ( mIsUdf(crd.x) || mIsUdf(crd.y) )
		    continue;

		cs.hrg.include( BinID((int) floor(crd.x),(int) floor(crd.y)) );
		cs.hrg.include( BinID((int)  ceil(crd.x),(int)  ceil(crd.y)) );
		trclist += crd; trclist += crd;
	    }
	}
	
	int lineidx = intersectionlineids_.indexOf(linestoupdate[idx]);
	if ( lineidx==-1 )
	{
	    lineidx = intersectionlineids_.size();
	    intersectionlineids_ += linestoupdate[idx];
	    intersectionlinevoi_ += -2;

	    const bool do3d = lineStyle()->type_==LineStyle::Solid;

	    visBase::IndexedShape* newline = do3d
		? (visBase::IndexedShape*) visBase::IndexedPolyLine3D::create()
		: (visBase::IndexedShape*) visBase::IndexedPolyLine::create();

	    newline->ref();

	    if ( do3d )
	    {
		const float radius = ((float) lineStyle()->width_) / 2;
		((visBase::IndexedPolyLine3D* ) newline)->setRadius( radius,
		    true, maxintersectionlinethickness_ );
	    }

	    newline->setRightHandSystem( righthandsystem_ );
	    newline->setDisplayTransformation(transformation_);
	    if ( intersectionlinematerial_ ) 
		newline->setMaterial( intersectionlinematerial_ );
	    intersectionlines_ += newline;
	    addChild( newline->getInventorNode() );
	}

	if ( zaxistransform_ )
	{
	    if ( intersectionlinevoi_[lineidx]==-2 )
	    {
		intersectionlinevoi_[lineidx] =
		    zaxistransform_->addVolumeOfInterest(cs,true);
	    }
	    else
	    {
		zaxistransform_->setVolumeOfInterest(
			intersectionlinevoi_[lineidx],cs,true);
	    }

	    if ( intersectionlinevoi_[lineidx]>=0 )
	    {
		zaxistransform_->loadDataIfMissing(
			intersectionlinevoi_[lineidx] );
	    }
	}
	    
	visBase::IndexedShape* line = intersectionlines_[lineidx];
	line->getCoordinates()->removeAfter(-1);
	int cii = 0;

	for ( int sectionidx=0; sectionidx<horizon->nrSections(); sectionidx++ )
	{
	    const EM::SectionID sid = horizon->sectionID(sectionidx);

	    if ( rtdisplay || seis2ddisplay )
	    {
		drawHorizonOnRandomTrack( trclist, cs.zrg, horizon, sid, 
					  zaxistransform_, line, cii );
	    }
	    else if ( cs.hrg.start.inl==cs.hrg.stop.inl )
	    {
		mTraverseLine( inl, BinID(targetinl,crlrg.start),
			       crlrg.stop, crlrg.step, 0, 1 );
	    }
	    else if ( cs.hrg.start.crl==cs.hrg.stop.crl )
	    {
		mTraverseLine( crl, BinID(inlrg.start,targetcrl),
			       inlrg.stop, inlrg.step, 1, 0 );
	    }
	    else
	    {
		drawHorizonOnTimeSlice( cs, getTranslation().z, horizon, sid,  
				        zaxistransform_, line, cii );
	    }
	}

	line->removeCoordIndexAfter(cii-1);
    }
}


void HorizonDisplay::setLineStyle( const LineStyle& lst )
{
    if ( lst==*lineStyle() )
	return;

    const bool removelines =
	(lst.type_==LineStyle::Solid) != (lineStyle()->type_==LineStyle::Solid);

    EMObjectDisplay::setLineStyle( lst );

    if ( removelines )
    {
	for ( int idx=0; idx<intersectionlines_.size(); idx++ )
	{
	    visBase::IndexedShape* newline = lst.type_==LineStyle::Solid
		? (visBase::IndexedShape*) visBase::IndexedPolyLine3D::create()
		: (visBase::IndexedShape*) visBase::IndexedPolyLine::create();
	    newline->ref();
	    newline->setRightHandSystem( righthandsystem_ );
	    newline->setDisplayTransformation(transformation_);
	    newline->copyCoordIndicesFrom( *intersectionlines_[idx] );
	    newline->getCoordinates()->copyFrom(
		    *intersectionlines_[idx]->getCoordinates() );
	    if ( intersectionlinematerial_ ) 
		newline->setMaterial( intersectionlinematerial_ );

	    removeChild( intersectionlines_[idx]->getInventorNode() );
	    addChild( newline->getInventorNode() );

	    intersectionlines_.replace( idx, newline )->unRef();

	    if ( lst.type_==LineStyle::Solid )
	    {
		const float radius = ((float) lineStyle()->width_) / 2;
		((visBase::IndexedPolyLine3D* ) newline )->
		    setRadius( radius, true, maxintersectionlinethickness_ );
	    }
	}
    }
    else if ( lst.type_==LineStyle::Solid )
    {
	for ( int idx=0; idx<intersectionlines_.size(); idx++ )
	{
	    const float radius = ((float) lineStyle()->width_) / 2;
	    visBase::IndexedPolyLine3D* pl =
		(visBase::IndexedPolyLine3D*) intersectionlines_[idx];
	    pl->setRadius( radius, true, maxintersectionlinethickness_ );
	}
    }
}



void HorizonDisplay::updateSectionSeeds( 
	    const ObjectSet<const SurveyObject>& objs, int movedobj )
{
    bool refresh = movedobj==-1 || movedobj==id();
    TypeSet<int> planelist;

    for ( int idx=0; idx<objs.size(); idx++ )
    {
	mDynamicCastGet(const PlaneDataDisplay*,plane,objs[idx]);
	if ( plane && plane->getOrientation()!=PlaneDataDisplay::Timeslice )
	{
	    planelist += idx; 
	    if ( movedobj==plane->id() ) 
		refresh = true;
	}

	mDynamicCastGet( const MPEDisplay*, mped, objs[idx] );
	if ( mped && mped->isDraggerShown() )
	{
	    CubeSampling cs;
	    if ( mped->getPlanePosition(cs) && cs.nrZ()!=1 )
	    {
		planelist += idx; 
		if ( movedobj==mped->id() ) 
		    refresh = true;
	    }
	}
    }

    if ( !refresh ) return;

    for ( int idx=0; idx<posattribmarkers_.size(); idx++ )
    {
	visBase::DataObjectGroup* group = posattribmarkers_[idx];
	for ( int idy=0; idy<group->size(); idy++ )
	{
	    mDynamicCastGet(visBase::Marker*,marker,group->getObject(idy))
	    if ( !marker ) continue;
	
	    marker->turnOn( !displayonlyatsections_ );
	    Coord3 pos = marker->centerPos();
	    if ( transformation_ )
		pos = transformation_->transform( pos );

	    for ( int idz=0; idz<planelist.size(); idz++ )
	    {
		const float dist = objs[planelist[idz]]->calcDist(pos);
		if ( dist < objs[planelist[idz]]->maxDist() )
		{
		    marker->turnOn(true);
		    break;
		}
	    }
	}
    }
}


void HorizonDisplay::otherObjectsMoved(
	    const ObjectSet<const SurveyObject>& objs, int whichobj )
{ 
    updateIntersectionLines( objs, whichobj ); 
    updateSectionSeeds( objs, whichobj );
}


void HorizonDisplay::fillPar( IOPar& par, TypeSet<int>& saveids ) const
{
    visBase::VisualObjectImpl::fillPar( par, saveids );
    EMObjectDisplay::fillPar( par, saveids );

    if ( emobject_ && !emobject_->isFullyLoaded() )
    {
	par.set( sKeyRowRange(), geometryRowRange() );
	par.set( sKeyColRange(), geometryColRange() );
    }

    par.setYN( sKeyTexture(), usesTexture() );
    par.setYN( sKeyWireFrame(), usesWireframe() );
    par.set( sKeyShift(), getTranslation().z );
    par.set( sKeyResolution(), getResolution() );

    mDynamicCastGet( visBase::ColTabTextureChannel2RGBA*, cttc2rgba,
		     const_cast<HorizonDisplay*>(this)->getChannels2RGBA() );
    if ( !cttc2rgba )
    {
	const int ctid =
          const_cast<HorizonDisplay*>(this)->getChannels2RGBA()->id();
	par.set( MultiTextureSurveyObject::sKeyTC2RGBA(), ctid );
	saveids += ctid;
    }


    for ( int channel=as_.size()-1; channel>=0; channel-- )
    {
	IOPar channelpar;
	as_[channel]->fillPar( channelpar );

	getColTabMapperSetup(channel)->fillPar( channelpar );
	if ( getColTabSequence(channel) )
	    getColTabSequence(channel)->fillPar( channelpar );

	channelpar.setYN( sKeyIsOn(), isAttribEnabled(channel) );

	BufferString key = sKeyAttribs();
	key += channel;
	par.mergeComp( channelpar, key );
    }

    par.set( sKeyNrAttribs(), as_.size() );

    const int matid = 
	intersectionlinematerial_ ? intersectionlinematerial_->id() : -1;
    par.set( sKeyIntersectLineMaterialID(), matid );
    if ( matid!=-1 && saveids.indexOf(matid)==-1 )
	saveids += matid;
}


int HorizonDisplay::usePar( const IOPar& par )
{
    int res = EMObjectDisplay::usePar( par );
    if ( res!=1 ) return res;

    int tc2rgbaid;
    if ( par.get( MultiTextureSurveyObject::sKeyTC2RGBA(), tc2rgbaid ) )
    {
	RefMan<visBase::DataObject> dataobj =
		      visBase::DM().getObject( tc2rgbaid );
	if ( !dataobj )
	    return 0;

	mDynamicCastGet(visBase::TextureChannel2RGBA*, tc2rgba, dataobj.ptr() );
	if ( tc2rgba )
	setChannels2RGBA( tc2rgba );
    }

    if ( scene_ )
	setDisplayTransformation( scene_->getUTM2DisplayTransform() );

    if ( !par.get(sKeyEarthModelID,parmid_) )
	return -1;

    par.get( sKeyRowRange(), parrowrg_ );
    par.get( sKeyColRange(), parcolrg_ );

    if ( !par.getYN(sKeyTexture(),usestexture_) )
	usestexture_ = true;

    bool usewireframe = false;
    par.getYN( sKeyWireFrame(), usewireframe );
    useWireframe( usewireframe );
    
    int resolution = 0;
    par.get( sKeyResolution(), resolution );

    Coord3 shift( 0, 0, 0 );
    par.get( sKeyShift(), shift.z );
    setTranslation( shift );

    int nrchannels;
    if ( par.get(sKeyNrAttribs(),nrchannels) ) //Current format
    {
	//This portion is for 3.2-pars. Can be removed in version 3.6 or later
	ObjectSet<visBase::VisColorTab> ctabs;
	ctabs.allowNull( true );
	bool isversion32 = false;
	for ( int attrib=0; attrib<nrchannels; attrib++ )
	{
	    BufferString key = sKeyAttribs();
	    key += attrib;
	    PtrMan<const IOPar> attribpar = par.subselect( key );
	    if ( !attribpar )
		continue;

	    int coltabid;
	    if ( attribpar->get(sKeyColTabID(),coltabid) )
	    {
		isversion32 = true;
		RefMan<visBase::DataObject> dataobj =
			visBase::DM().getObject( coltabid );
		if ( !dataobj )
		{
		    deepUnRef( ctabs );
		    return 0;
		}

		mDynamicCastGet(visBase::VisColorTab*,coltab,dataobj.ptr());
		if ( coltab ) coltab->ref();
		ctabs += coltab;
	    }
	    else
		ctabs += 0;
	}

	if ( isversion32 )
	{
	    if ( !resolution )
		resolution = 6-resolution;
	}

	//End of 3.2 code

	ColTab::MapperSetup mappersetup;
	ColTab::Sequence coltabsequence( ColTab::defSeqName() );

	bool firstchannel = true;
	for ( int channel=0; channel<nrchannels; channel++ )
	{
	    BufferString key = sKeyAttribs();
	    key += channel;
	    PtrMan<const IOPar> channelpar = par.subselect( key );
	    if ( !channelpar )
		continue;

	    if ( !firstchannel )
		addAttrib();
	    else
		firstchannel = false;

	    bool ison = true;
	    channelpar->getYN( sKeyIsOn(), ison );
	    const int channelnr = as_.size()-1;
	    enabled_[channelnr] = ison;
	    as_[channelnr]->usePar( *channelpar );

	    if ( !mappersetup.usePar( *channelpar ) ||
		 !coltabsequence.usePar( *channelpar ) )
	    {
		if ( ctabs[channel] )
		{
		    mappersetup = ctabs[channel]->colorMapper().setup_;
		    coltabsequence = ctabs[channel]->colorSeq().colors();
		}
	    }

	    setColTabMapperSetup( channel, mappersetup, 0 );
	    setColTabSequence( channel, coltabsequence, 0 );
	}

	deepUnRef( ctabs );
    }
    else //old format
    {
	as_[0]->usePar( par );
	int coltabid = -1;
	par.get( sKeyColTabID(), coltabid );
	if ( coltabid>-1 )
	{
	    DataObject* dataobj = visBase::DM().getObject( coltabid );
	    if ( !dataobj ) return 0;
	    
	    mDynamicCastGet( visBase::VisColorTab*, coltab, dataobj );
	    if ( !coltab ) return -1;
	
	    setColTabSequence( 0, coltab->colorSeq().colors(), 0 );
    	    setColTabMapperSetup( 0, coltab->colorMapper().setup_, 0 );
	}
    }

    setResolution( resolution, 0 );

    int intersectlinematid;
    if ( par.get(sKeyIntersectLineMaterialID(),intersectlinematid) )
    {
	if ( intersectlinematid==-1 ) 
	    setIntersectLineMaterial( 0 );
	else
	{
	    DataObject* mat = visBase::DM().getObject( intersectlinematid );
	    if ( !mat ) return 0;
	    if ( typeid(*mat) != typeid(visBase::Material) ) return -1;

	    setIntersectLineMaterial( (visBase::Material*)mat );
	}
    }
    else
	setIntersectLineMaterial( 0 );

    return 1;
}


const ObjectSet<visBase::IndexedShape>&
HorizonDisplay::getIntersectionLines() const
{ return intersectionlines_; }


bool HorizonDisplay::setDataPackID( int channel, DataPack::ID dpid,
				    TaskRunner* tr)
{
    DataPackMgr& dpman = DPM( DataPackMgr::FlatID() );
    const DataPack* datapack = dpman.obtain( dpid );
    if ( !datapack ) return false;

    DataPack::ID oldid = datapackids_[channel];
    datapackids_[channel] = dpid;
    dpman.release( oldid );
    return true;
}


DataPack::ID HorizonDisplay::getDataPackID( int channel ) const
{
    return datapackids_[channel];
}

}; // namespace visSurvey
