#pragma once

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Kristofer Tingdahl
 Date:		24-01-2003
________________________________________________________________________


-*/

#include "uivismod.h"
#include "menuhandler.h"

class uiVisPartServer;

/*!Adds a menu-item to a visual object's right-click menu. Menu item
will be added to all visual objects of a certain type.

Usage example:
\code
    uiVisMenuItemHandler( visSurvey::WellDisplay::getStaticClassName(),
		  vispartserver_, tr("My menu text"),
		  mCB(this,MyClass,myFuncCB) );
\endcode
*/


mExpClass(uiVis) uiVisMenuItemHandler : public MenuItemHandler
{
public:
    		uiVisMenuItemHandler(const char* classnm,uiVisPartServer&,
				const uiString& mnutext,const CallBack& cb,
				const char* parenttext=0,int placement=-1);

    int		getDisplayID() const;
    		/*!<Does only give a valid answer if called from cb. */

protected:
    bool		shouldAddMenu() const;

    const char*		classnm_;
    uiVisPartServer&	visserv_;
};


/*!brief MenuItemHandler for PickSet/Polygons
    By default the menu will be added to both. This can be controlled with the
    functions addWhenPickSet and addWhenPolygon
*/

mExpClass(uiVis) uiPickSetPolygonMenuItemHandler : public MenuItemHandler
{
public:
		uiPickSetPolygonMenuItemHandler(uiVisPartServer&,
				const uiString& mnutext,const CallBack&,
				const char* parenttext=0,int placement=-1);

    void	addWhenPickSet(bool);
    void	addWhenPolygon(bool);

    int		getDisplayID() const;
    		/*!<Does only give a valid answer if called from cb. */
protected:
    bool		shouldAddMenu() const;

    uiVisPartServer&	visserv_;
    bool		addwhenpolygon_;
    bool		addwhenpickset_;
};
