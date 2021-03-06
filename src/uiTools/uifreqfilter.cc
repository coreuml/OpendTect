/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:        Bruno
 Date:          April 2012
________________________________________________________________________

-*/



#include "uifreqfilter.h"
#include "uigeninput.h"


uiFreqFilterSelFreq::uiFreqFilterSelFreq( uiParent* p)
    : uiGroup( p, "Frequency Filter Selection")
    , parchanged(this)  
{
    typefld_ = new uiGenInput(this, tr("Filter type"), 
			      StringListInpSpec(FFTFilter::TypeDef()));
    typefld_->valuechanged.notify( mCB(this,uiFreqFilterSelFreq,getFromScreen));
    typefld_->valuechanged.notify( mCB(this,uiFreqFilterSelFreq,typeSel) );
    typefld_->valuechanged.notify( mCB(this,uiFreqFilterSelFreq,parChgCB) );

    freqfld_ = new uiGenInput( this, sMinMax(), 
	    FloatInpSpec().setName("Min frequency"),
	    FloatInpSpec().setName("Max frequency") );
    freqfld_->setElemSzPol( uiObject::Small );
    freqfld_->attach( alignedBelow, typefld_ );
    freqfld_->valuechanged.notify(mCB(this,uiFreqFilterSelFreq,getFromScreen));
    freqfld_->valuechanged.notify( mCB(this,uiFreqFilterSelFreq,parChgCB) );

    setHAlignObj( freqfld_ );

    set( 15, 50, FFTFilter::LowPass );

    postFinalise().notify( mCB(this,uiFreqFilterSelFreq,typeSel) );
}


void uiFreqFilterSelFreq::parChgCB( CallBacker* )
{
    parchanged.trigger();
}


const uiString uiFreqFilterSelFreq::sMinMax()
{ 
    return tr("Min/max %1(Hz)").arg(uiStrings::sFrequency(true)); 
}


void uiFreqFilterSelFreq::typeSel( CallBacker* )
{
    const int type = typefld_->getIntValue();
    const bool hasmin = type==1 || type==2;
    const bool hasmax = !type || type==2;
    freqfld_->setSensitive( hasmin, 0, 0 );
    freqfld_->setSensitive( hasmax, 0, 1 );
}


void uiFreqFilterSelFreq::putToScreen()
{
    typefld_->setValue( filtertype_ );
    freqfld_->setValue( freqrg_.start, 0 );
    freqfld_->setValue( freqrg_.stop, 1 );
    typeSel(0);
}



void uiFreqFilterSelFreq::getFromScreen( CallBacker* )
{
    filtertype_ = (FFTFilter::Type)typefld_->getIntValue();
    freqrg_ = freqfld_->getFInterval();
}


void uiFreqFilterSelFreq::set( float minf, float maxf, FFTFilter::Type tp )
{
    freqrg_.start = minf;
    freqrg_.stop = maxf;
    filtertype_ = tp;
    putToScreen();
}
