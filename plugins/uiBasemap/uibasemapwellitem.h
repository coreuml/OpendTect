#ifndef uibasemapwellitem_h
#define uibasemapwellitem_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Nanne Hemstra
 Date:		January 2014
 RCS:		$Id$
________________________________________________________________________

-*/

#include "uibasemapmod.h"
#include "uibasemapitem.h"

mExpClass(uiBasemap) uiBasemapWellGroup : public uiBasemapIOObjGroup
{
public:
			uiBasemapWellGroup(uiParent*,bool isadd);
			~uiBasemapWellGroup();

    bool		acceptOK();
    bool		fillPar(IOPar&) const;
    bool		usePar(const IOPar&);

protected:

};



mExpClass(uiBasemap) uiBasemapWellParentTreeItem :
					public uiBasemapParentTreeItem
{
public:
			uiBasemapWellParentTreeItem(int id)
			    : uiBasemapParentTreeItem("Well",id)	{}

protected:
    const char*		iconName() const;
};



mExpClass(uiBasemap) uiBasemapWellTreeItem : public uiBasemapTreeItem
{
public:
			uiBasemapWellTreeItem(const char*);
			~uiBasemapWellTreeItem();

    bool		usePar(const IOPar&);

protected:

    bool		showSubMenu();
    bool		handleSubMenu(int);
    const char*		parentType() const
			{ return typeid(uiBasemapWellParentTreeItem).name(); }
};



mExpClass(uiBasemap) uiBasemapWellItem : public uiBasemapItem
{
public:
			mDefaultFactoryInstantiation(
				uiBasemapItem,
				uiBasemapWellItem,
				"Wells",
				sFactoryKeyword())

    int			defaultZValue() const;
    uiBasemapGroup*	createGroup(uiParent*,bool isadd);
    uiBasemapParentTreeItem* createParentTreeItem();
    uiBasemapTreeItem*	createTreeItem(const char*);
};

#endif
