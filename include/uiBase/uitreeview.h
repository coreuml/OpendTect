#ifndef uitreeview_h
#define uitreeview_h

/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        A.H. Lammertink
 Date:          29/01/2002
 RCS:           $Id$
________________________________________________________________________

-*/

#include "uibasemod.h"
#include "uiobj.h"
#include "bufstringset.h"
#include "keyenum.h"
#include "draw.h"
#include "uistring.h"
#include "uistrings.h"

mFDQtclass(QTreeWidget)
mFDQtclass(QTreeWidgetItem)
mFDQtclass(QString)
mFDQtclass(QStringList)

class uiPixmap;
class uiTreeViewBody;
class uiTreeViewItem;
namespace ColTab { class Sequence; }

mExpClass(uiBase) uiTreeView : public uiObject
{ mODTextTranslationClass(uiTreeView);
public:
			uiTreeView(uiParent* parnt,
				   const char* nm="uiTreeView",
				   int preferredNrLines=0,
				   bool rootdecorated=true);
    virtual		~uiTreeView();

    void		setEmpty();
    void		setNrLines(int);

    bool		rootDecorated() const;
    void		setRootDecorated(bool yn);
    void		showHeader(bool yn); // default is true

    void		addColumns(const uiStringSet&);
    void		addColumns(const BufferStringSet&);
    void		setNrColumns(int); // when columns have no name
    int			nrColumns() const;

    void		removeColumn(int index);
    void		setColumnText(int column,const uiString& label);
    uiString		getColumnText(int column) const;
    void		setColumnWidth(int column,int width);
    void		setFixedColumnWidth(int column,int width);
    int			columnWidth(int column) const;

    enum		WidthMode { Manual, Fixed, Stretch, ResizeToContents,
				    Custom };
    void		setColumnWidthMode(WidthMode);
    void		setColumnWidthMode(int column,WidthMode);
    WidthMode		columnWidthMode(int column) const;

    void		setColumnAlignment(Alignment::HPos);
    void		setColumnAlignment(int,Alignment::HPos);
    Alignment::HPos	columnAlignment(int) const;

    enum		ScrollMode { Auto, AlwaysOff, AlwaysOn };
    void		setHScrollBarMode(ScrollMode);
    void		setVScrollBarMode(ScrollMode);

    enum		SelectionMode
			{ NoSelection=0, Single, Multi, Extended, Contiguous };
    enum		SelectionBehavior
			{ SelectItems, SelectRows, SelectColumns };
    void		setSelectionMode(SelectionMode);
    SelectionMode	selectionMode() const;
    void		setSelectionBehavior(SelectionBehavior);
    SelectionBehavior	selectionBehavior() const;

    void		clearSelection();
    void		setSelected(uiTreeViewItem*,bool);
    bool		isSelected(const uiTreeViewItem*) const;
    uiTreeViewItem*	selectedItem() const;
    int			nrSelected() const;
    void		getSelectedItems(ObjectSet<uiTreeViewItem>&) const;
    void		removeSelectedItems();

    int			nrItems() const;
    void		setCurrentItem(uiTreeViewItem*,int column=0);
    uiTreeViewItem*	currentItem() const;
    int			currentColumn() const;
    int			indexOfItem(uiTreeViewItem*) const;
    uiTreeViewItem*	getItem(int) const;
    uiTreeViewItem*	firstItem() const;
    uiTreeViewItem*	lastItem() const;
    uiTreeViewItem*	findItem(const char*,int,bool) const;

    void		setItemMargin(int);
    int			itemMargin() const;

    void		setShowToolTips(bool);
    bool		showToolTips() const;

    void		clear();
    void		invertSelection();
    void		selectAll();

    void		expandAll();
    void		collapseAll();
    void		expandTo(int treedepth);
    void		ensureItemVisible(const uiTreeViewItem*);

			// For MOVING an item to another point in the tree
    void		takeItem(uiTreeViewItem*);
    void		insertItem(int,uiTreeViewItem*);

    uiParent*		parent()		{ return parent_; }
    void		translateText();
    bool		handleLongTabletPress();

			//! re-draws at next X-loop
    void		triggerUpdate();

			//! item last notified. See notifiers below
    uiTreeViewItem*	itemNotified()		{ return lastitemnotified_; }
    int			columnNotified()	{ return column_; }
    void		unNotify()		{ lastitemnotified_ = 0; }

    void		setNotifiedItem(mQtclass(QTreeWidgetItem*));
    void		setNotifiedColumn(int col)	{ column_ = col; }

    Notifier<uiTreeView> selectionChanged;
    Notifier<uiTreeView> currentChanged;
    Notifier<uiTreeView> itemChanged;
    Notifier<uiTreeView> returnPressed;
    Notifier<uiTreeView> rightButtonClicked;
    Notifier<uiTreeView> rightButtonPressed;
    Notifier<uiTreeView> leftButtonClicked;
    Notifier<uiTreeView> leftButtonPressed;
    Notifier<uiTreeView> mouseButtonPressed;
    Notifier<uiTreeView> mouseButtonClicked;
    Notifier<uiTreeView> contextMenuRequested;
    Notifier<uiTreeView> doubleClicked;
    Notifier<uiTreeView> itemRenamed;
    Notifier<uiTreeView> expanded;
    Notifier<uiTreeView> collapsed;
    Notifier<uiTreeView> unusedKey;

protected:

    mutable BufferString rettxt;
    uiTreeViewItem*	lastitemnotified_;
    uiParent*		parent_;
    int			column_;
    OD::ButtonState     buttonstate_;

    void		cursorSelectionChanged(CallBacker*);
    void		itemChangedCB(CallBacker*);
    void		updateCheckStatus(uiTreeViewItem*);

    uiTreeViewBody*		lvbody()	{ return body_; }
    const uiTreeViewBody*	lvbody() const	{ return body_; }

private:

    friend class		i_treeVwMessenger;
    friend class		uiTreeViewBody;
    friend class		uiTreeViewItem;

    uiTreeViewBody*		body_;

    uiStringSet		labels_;

			// 0: use nr itms in tree
    uiTreeViewBody&	mkbody(uiParent*,const char*,int);

public:

    const char*		columnText(int column) const;
				//Commandline driver usage only

};


/*!

*/

mExpClass(uiBase) uiTreeViewItem : public CallBacker
{ mODTextTranslationClass(uiTreeViewItem);
public:

    enum			Type { Standard, CheckBox };

    mExpClass(uiBase) Setup
    { mODTextTranslationClass(Setup);
    public:
			Setup( const uiString& txt=uiStrings::sEmptyString(),
				       uiTreeViewItem::Type tp=
						uiTreeViewItem::Standard,
				       bool setchecked=true )
				: type_(tp)
				, after_(0)
				, setcheck_(setchecked)
				{ label( txt ); }

	mDefSetupMemb(uiTreeViewItem::Type,type)
	mDefSetupMemb(uiTreeViewItem*,after)
	mDefSetupMemb(BufferString,iconname)
	mDefSetupMemb(bool,setcheck)
	uiStringSet		labels_;

	Setup&			label( const uiString& txt )
				{
				    if ( !txt.isEmpty() )
					labels_ += txt;
				    return *this;
				}
    };

			uiTreeViewItem(uiTreeViewItem* parent,const Setup&);
			uiTreeViewItem(uiTreeView* parent,const Setup&);
    virtual		~uiTreeViewItem();

    mQtclass(QTreeWidgetItem*)	qItem()			{ return qtreeitem_; }
    const mQtclass(QTreeWidgetItem*) qItem() const	{ return qtreeitem_; }

    int			nrChildren() const;

    void		setBGColor(int column,const Color&);

    void		setCheckable(bool);
    bool		isCheckable() const;
    void		setChecked(bool,bool trigger=false);
			//!< does nothing if not checkable
    bool		isChecked(bool qtstatus=true) const;
			//!< returns false if not checkable

    void		setToolTip(int column,const uiString&);

    void		translateText();

    static void		updateToolTips();

    void		insertItem(int,uiTreeViewItem*);
    void		takeItem(uiTreeViewItem*);
    void		removeItem(uiTreeViewItem*);
    void		moveItem(uiTreeViewItem* after);
    int			siblingIndex() const;
			/*!<\returns this items index of it's siblings. */

    void		setText( const uiString&, 
				 int column=0 );
    void		setText( int i, int column=0 )
			{ setText( mkUiString(toString(i)), column ); }
    void		setText( float f, int column=0 )
			{ setText( mkUiString(toString(f)), column ); }
    void		setText( double d, int column=0 )
			{ setText( mkUiString(toString(d)), column ); }

    const char*		text( int column=0 ) const;

    void		setIcon(int column,const char* iconname);
    void		setPixmap(int column,const uiPixmap&);
    void		setPixmap(int column,const Color&,
				  int width=16,int height=10);
    void		setPixmap(int column,const ColTab::Sequence&,
				  int width=16,int height=10);

    virtual const char* key(int,bool) const		{ return 0; }
    virtual int		compare( uiTreeViewItem*,int column,bool) const
							{ return mUdf(int); }

    void		setOpen(bool yn=true);
    bool		isOpen() const;

    void		setSelected(bool yn);
    bool		isSelected() const;

    uiTreeViewItem*	getChild(int) const;
    uiTreeViewItem*	firstChild() const;
    uiTreeViewItem*	lastChild() const;
    uiTreeViewItem*	nextSibling() const;
    uiTreeViewItem*	prevSibling() const;
    uiTreeViewItem*	parent() const;

    uiTreeViewItem*	itemAbove();
    uiTreeViewItem*	itemBelow();

    uiTreeView*		treeView() const;

    void		setSelectable(bool yn);
    bool		isSelectable() const;

    void		setDragEnabled(bool);
    void		setDropEnabled(bool);
    bool		dragEnabled() const;
    bool		dropEnabled() const;

    void		setVisible(bool yn);
    bool		isVisible() const;

    void		setRenameEnabled(int column,bool);
    bool		renameEnabled(int column) const;

    void		setEnabled(bool);
    bool		isEnabled() const;


    Notifier<uiTreeViewItem> stateChanged; //!< only works for CheckBox type
    Notifier<uiTreeViewItem> keyPressed;
			//!< passes CBCapsule<const char*>* cb
			//!< If you handle it, set cb->data = 0;

    static mQtclass(QTreeWidgetItem*)	 qitemFor(uiTreeViewItem*);
    static const mQtclass(QTreeWidgetItem*)  qitemFor(const uiTreeViewItem*);

    static uiTreeViewItem*	 itemFor(mQtclass(QTreeWidgetItem*));
    static const uiTreeViewItem* itemFor(const mQtclass(QTreeWidgetItem*));

protected:

    mQtclass(QTreeWidgetItem*)	qtreeitem_;

    void			init(const Setup&);
    void			updateFlags();
    bool			updateToolTip(int column);

    uiStringSet			texts_;
    uiStringSet			tooltips_;

    bool			isselectable_;
    bool			iseditable_;
    bool			isdragenabled_;
    bool			isdropenabled_;
    bool			ischeckable_;
    bool			isenabled_;
    bool			checked_;
};

#endif

