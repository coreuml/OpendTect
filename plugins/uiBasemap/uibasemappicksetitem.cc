/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2014
 RCS:		$Id$
________________________________________________________________________

-*/

#include "uibasemappicksetitem.h"

#include "uiaction.h"
#include "uiioobjselgrp.h"
#include "uimenu.h"
#include "uimsg.h"
#include "uiodscenemgr.h"
#include "uistrings.h"

#include "basemappickset.h"
#include "oduicommon.h"
#include "picksettr.h"


static IOObjContext getIOObjContext()
{
    IOObjContext ctxt = mIOObjContext( PickSet );
    ctxt.toselect.dontallow_.set( sKey::Type(), sKey::Polygon() );
    return ctxt;
}


// uiBasemapPickSetGroup
uiBasemapPickSetGroup::uiBasemapPickSetGroup( uiParent* p, bool isadd )
    : uiBasemapIOObjGroup(p,getIOObjContext(),isadd)
{
}


uiBasemapPickSetGroup::~uiBasemapPickSetGroup()
{
}


bool uiBasemapPickSetGroup::acceptOK()
{
    const bool res = uiBasemapIOObjGroup::acceptOK();
    return res;
}


bool uiBasemapPickSetGroup::fillPar( IOPar& par ) const
{
    const bool res = uiBasemapIOObjGroup::fillPar( par );
    return res;
}


bool uiBasemapPickSetGroup::usePar( const IOPar& par )
{
    const bool res = uiBasemapIOObjGroup::usePar( par );
    return res;
}



// uiBasemapPickSetParentTreeItem
const char* uiBasemapPickSetParentTreeItem::iconName() const
{ return "basemap-pickset"; }



// uiBasemapPickSetItem
int uiBasemapPickSetItem::defaultZValue() const
{ return 100; }

uiBasemapGroup* uiBasemapPickSetItem::createGroup( uiParent* p, bool isadd )
{ return new uiBasemapPickSetGroup( p, isadd ); }

uiBasemapParentTreeItem* uiBasemapPickSetItem::createParentTreeItem()
{ return new uiBasemapPickSetParentTreeItem(ID()); }

uiBasemapTreeItem* uiBasemapPickSetItem::createTreeItem( const char* nm )
{ return new uiBasemapPickSetTreeItem( nm ); }



// uiBasemapPickSetTreeItem
uiBasemapPickSetTreeItem::uiBasemapPickSetTreeItem( const char* nm )
    : uiBasemapTreeItem(nm)
{}


uiBasemapPickSetTreeItem::~uiBasemapPickSetTreeItem()
{}


bool uiBasemapPickSetTreeItem::usePar( const IOPar& par )
{
    uiBasemapTreeItem::usePar( par );

    MultiID mid;
    if ( !par.get(sKey::ID(),mid) )
	return false;

    if ( basemapobjs_.isEmpty() )
	addBasemapObject( *new Basemap::PickSetObject(mid) );

    mDynamicCastGet(Basemap::PickSetObject*,obj,basemapobjs_[0])
    if ( !obj ) return false;

    obj->setMultiID( mid );
    obj->updateGeometry();

    return true;
}


bool uiBasemapPickSetTreeItem::showSubMenu()
{
    uiMenu mnu( getUiParent(), uiStrings::sAction() );
    mnu.insertItem( new uiAction(uiStrings::sEdit(false)), sEditID() );
    mnu.insertItem( new uiAction("Show in 3D"), 1 );
    mnu.insertItem( new uiAction(uiStrings::sRemove(true)), sRemoveID() );
    const int mnuid = mnu.exec();
    return handleSubMenu( mnuid );
}


bool uiBasemapPickSetTreeItem::handleSubMenu( int mnuid )
{
    if ( uiBasemapTreeItem::handleSubMenu(mnuid) )
	return true;

    bool handled = true;
    if ( mnuid==1 )
    {
	const int nrobjs = basemapobjs_.size();
	for ( int idx=0; idx<nrobjs; idx++ )
	{
	    mDynamicCastGet(Basemap::PickSetObject*,obj,basemapobjs_[idx])
	    if ( !obj ) continue;

	    ODMainWin()->sceneMgr().addPickSetItem( obj->getMultiID() );
	}
    }
    else
	handled = false;

    return handled;
}


const char* uiBasemapPickSetTreeItem::parentType() const
{
    return typeid(uiBasemapPickSetParentTreeItem).name();
}
