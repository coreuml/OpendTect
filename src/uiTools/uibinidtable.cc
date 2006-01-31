/*+
 ________________________________________________________________________

 CopyRight:     (C) dGB Beheer B.V.
 Author:        N. Hemstra
 Date:          February 2003
 RCS:           $Id: uibinidtable.cc,v 1.10 2006-01-31 09:07:04 cvsnanne Exp $
 ________________________________________________________________________

-*/

#include "uibinidtable.h"
#include "uigeninput.h"
#include "uiseparator.h"
#include "uitable.h"
#include "uimsg.h"
#include "position.h"
#include "ranges.h"
#include "survinfo.h"


uiBinIDTable::uiBinIDTable( uiParent* p, bool withz )
    : uiGroup(p,"BinID table")
    , withz_(withz)
{
    table_ = new uiTable( this, uiTable::Setup().rowdesc("Node")
	    					.rowgrow(true)
						.defrowlbl(true) );
    table_->setNrCols( 2 );
    table_->setColumnLabel( 0, "Inline" );
    table_->setColumnLabel( 1, "Crossline" );
    table_->setNrRows( 5 );

    uiSeparator* hsep = new uiSeparator( this, "Separator" );
    hsep->attach( stretchedBelow, table_ );

    if ( withz_ )
    {
	BufferString lbl = "Z range "; lbl += SI().getZUnit();
	zfld_ = new uiGenInput( this, lbl, FloatInpIntervalSpec() );
	zfld_->attach( leftAlignedBelow, table_ );
	zfld_->attach( ensureBelow, hsep );
    }
}


void uiBinIDTable::setBinIDs( const TypeSet<BinID>& bids )
{
    const int nrbids = bids.size();
    table_->setNrRows( nrbids+5 );
    for ( int idx=0; idx<nrbids; idx++ )
    {
	const BinID bid = bids[idx];
	table_->setText( RowCol(idx,0), BufferString(bid.inl) );
	table_->setText( RowCol(idx,1), BufferString(bid.crl) );
    }
}


void uiBinIDTable::getBinIDs( TypeSet<BinID>& bids ) const
{
    int nrrows = table_->size().height();
    for ( int idx=0; idx<nrrows; idx++ )
    {
	BinID bid(0,0);
	BufferString inlstr = table_->text( RowCol(idx,0) );
	BufferString crlstr = table_->text( RowCol(idx,1) );
	if ( !(*inlstr) || !(*crlstr) )
	    continue;

	bid.inl = atoi(inlstr);
	bid.crl = atoi(crlstr);
	if ( !SI().isReasonable(bid) )
	{
	    Coord c( atof(inlstr), atof(crlstr) );
	    if ( SI().isReasonable(c) )
		bid = SI().transform(c);
	    else
	    {
		BufferString msg( "Position " );
		msg += bid.inl; msg += "/"; msg += bid.crl;
		msg += " is probably wrong.\nDo you wish to discard it?";
		if ( uiMSG().askGoOn(msg) )
		{
		    table_->removeRow( idx );
		    nrrows--; idx--;
		    continue;
		}
	    }
	}
	SI().snap( bid );
	bids += bid;
    }
}


void uiBinIDTable::setZRange( const Interval<float>& zrg )
{
    zfld_->setValue( zrg );
}


void uiBinIDTable::getZRange( Interval<float>& zrg ) const
{
    assign( zrg, withz_ ? zfld_->getFInterval() 
	    		: (Interval<float>)SI().zRange(false) );
}
