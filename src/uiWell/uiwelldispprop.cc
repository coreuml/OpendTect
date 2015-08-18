/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Bruno
 Date:		Dec 2008
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "uiwelldispprop.h"

#include "uichecklist.h"
#include "uicolor.h"
#include "uicolortable.h"
#include "uigeninput.h"
#include "uilabel.h"
#include "uilistbox.h"
#include "uispinbox.h"
#include "uiseparator.h"
#include "uislider.h"
#include "uistrings.h"
#include "welllog.h"
#include "welllogset.h"

static int deflogwidth = 250;

uiWellDispProperties::uiWellDispProperties( uiParent* p,
				const uiWellDispProperties::Setup& su,
				Well::DisplayProperties::BasicProps& pr )
    : uiGroup(p,"Well display properties group")
    , props_(&pr)
    , propChanged(this)
    , setup_(su)
    , curwelllogproperty_(0)
{
    szfld_ = new uiLabeledSpinBox( this, su.mysztxt_ );
    szfld_->box()->setInterval( StepInterval<int>(0,100,1) );
    szfld_->box()->setValue( props().size_ );
    szfld_->box()->valueChanging.notify(mCB(this,uiWellDispProperties,propChg));
    uiColorInput::Setup csu( props().color_ );
    csu.lbltxt( su.mycoltxt_ );
    colfld_ = new uiColorInput( this, csu, su.mycoltxt_.getFullString() );
    colfld_->attach( alignedBelow, szfld_ );
    colfld_->colorChanged.notify( mCB(this,uiWellDispProperties,propChg) );

    setHAlignObj( colfld_ );
}


void uiWellDispProperties::propChg( CallBacker* )
{
    getFromScreen();
    propChanged.trigger();
}


void uiWellDispProperties::putToScreen()
{
    NotifyStopper ns1( szfld_->box()->valueChanging );
    NotifyStopper ns2( colfld_->colorChanged );
    colfld_->setColor( props().color_ );
    doPutToScreen();
    szfld_->box()->setValue( props().size_ );
}


void uiWellDispProperties::getFromScreen()
{
    props().size_ = szfld_->box()->getValue();
    props().color_ = colfld_->color();
    doGetFromScreen();
}


uiWellTrackDispProperties::uiWellTrackDispProperties( uiParent* p,
				const uiWellDispProperties::Setup& su,
				Well::DisplayProperties::Track& tp )
    : uiWellDispProperties(p,su,tp)
{
    dispabovefld_ = new uiCheckBox( this, tr("Above") );
    dispabovefld_->attach( alignedBelow, colfld_ );
    dispbelowfld_ = new uiCheckBox( this, tr("Below") );
    dispbelowfld_->attach( rightOf, dispabovefld_ );
    uiLabel* lbl = new uiLabel( this, tr("Display well name") , dispabovefld_ );
    lbl = new uiLabel( this, uiStrings::sTrack(true) );
    lbl->attach( rightOf, dispbelowfld_ );

    nmsizefld_ = new uiLabeledSpinBox( this, tr("Name size") );
    nmsizefld_->box()->setInterval( 0, 500, 2 );
    nmsizefld_->attach( alignedBelow, dispabovefld_  );

    uiStringSet fontstyles;
    fontstyles.add( uiStrings::sNormal() ); fontstyles.add(tr("Bold"));
    fontstyles.add(tr("Italic")); fontstyles.add(tr("Bold Italic"));

    nmstylefld_ = new uiComboBox( this, fontstyles, "Fontstyle" );
    nmstylefld_->attach( rightOf, nmsizefld_ );

    doPutToScreen();

    dispabovefld_->activated.notify(
		mCB(this,uiWellTrackDispProperties,propChg) );
    dispbelowfld_->activated.notify(
		mCB(this,uiWellTrackDispProperties,propChg) );
    nmsizefld_->box()->valueChanging.notify(
		mCB(this,uiWellTrackDispProperties,propChg) );
    nmstylefld_->selectionChanged.notify(
	    mCB(this,uiWellTrackDispProperties,propChg) );
}


void uiWellTrackDispProperties::resetProps( Well::DisplayProperties::Track& pp )
{
    props_ = &pp;
}


void uiWellTrackDispProperties::doPutToScreen()
{
    dispbelowfld_->setChecked( trackprops().dispbelow_ );
    dispabovefld_->setChecked( trackprops().dispabove_ );
    nmsizefld_->box()->setValue( trackprops().font_.pointSize());

    int style = trackprops().font_.weight()>FontData::Normal ? 1 : 0;
    if ( trackprops().font_.isItalic() )
	style += 2;

    nmstylefld_->setValue( style );
}


void uiWellTrackDispProperties::doGetFromScreen()
{
    trackprops().dispbelow_ = dispbelowfld_->isChecked();
    trackprops().dispabove_ = dispabovefld_->isChecked();
    trackprops().font_.setPointSize( nmsizefld_->box()->getValue() );
    const int fontstyle = nmstylefld_->getIntValue();
    const bool bold = fontstyle==1 || fontstyle==3;
    trackprops().font_.setWeight( bold ? FontData::Bold : FontData::Normal );
    trackprops().font_.setItalic( fontstyle==2 || fontstyle==3 );
}


uiWellMarkersDispProperties::uiWellMarkersDispProperties( uiParent* p,
				const uiWellDispProperties::Setup& su,
				Well::DisplayProperties::Markers& mp,
				const BufferStringSet& allmarkernms, bool is2d )
    : uiWellDispProperties(p,su,mp)
    , is2d_(is2d)
{
    uiStringSet shapes3d;
    uiStringSet shapes2d;
    shapes3d.add(tr("Cylinder")); shapes3d.add(tr("Square"));
    shapes3d.add(tr("Sphere"));
    shapes2d.add(tr("Dot"));shapes2d.add(tr("Solid"));shapes2d.add(tr("Dash"));


    shapefld_ = new uiLabeledComboBox( this, tr("Shape") );
    shapefld_->attach( alignedBelow, colfld_ );
    for ( int idx=0; idx<shapes3d.size(); idx++)
	shapefld_->box()->addItem(is2d ? shapes2d[idx] : shapes3d[idx]);

    cylinderheightfld_ = new uiLabeledSpinBox( this, tr("Height") );
    cylinderheightfld_->box()->setInterval( 0, 10, 1 );
    cylinderheightfld_->attach( rightOf, shapefld_ );
    cylinderheightfld_->display( !is2d );

    singlecolfld_ = new uiCheckBox( this, tr("use single color") );
    singlecolfld_->attach( rightOf, colfld_);
    colfld_->setSensitive( singlecolfld_->isChecked() );

    nmsizefld_ = new uiLabeledSpinBox( this, tr("Names size") );
    nmsizefld_->box()->setInterval( 0, 500, 1 );
    nmsizefld_->box()->setValue( 2 * mp.size_ );
    nmsizefld_->attach( alignedBelow, shapefld_ );

    uiStringSet styles;
    styles.add( uiStrings::sNormal() ); styles.add(tr("Bold"));
    styles.add(tr("Italic")); styles.add(tr("Bold Italic"));

    nmstylefld_ = new uiComboBox( this, styles, "Fontstyle" );
    nmstylefld_->attach( rightOf, nmsizefld_ );

    uiString dlgtxt = tr( "Names color" );
    uiColorInput::Setup csu( mrkprops().color_ ); csu.lbltxt( dlgtxt );
    nmcolfld_ = new uiColorInput( this, csu, dlgtxt.getFullString() );
    nmcolfld_->attach( alignedBelow, nmsizefld_ );

    samecolasmarkerfld_ = new uiCheckBox( this, tr("same as markers") );
    samecolasmarkerfld_->attach( rightOf, nmcolfld_);

    uiListBox::Setup msu( OD::ChooseZeroOrMore, tr("Display markers") );
    displaymarkersfld_ = new uiListBox( this, msu );
    displaymarkersfld_->attach( alignedBelow, nmcolfld_ );
    displaymarkersfld_->addItems( allmarkernms );

    doPutToScreen();
    markerFldsChged(0);

    cylinderheightfld_->box()->valueChanging.notify(
		mCB(this,uiWellMarkersDispProperties,propChg) );
    nmcolfld_->colorChanged.notify(
		mCB(this,uiWellMarkersDispProperties,propChg) );
    nmsizefld_->box()->valueChanging.notify(
		mCB(this,uiWellMarkersDispProperties,propChg) );
    nmstylefld_->selectionChanged.notify(
	    mCB(this,uiWellMarkersDispProperties,propChg) );
    samecolasmarkerfld_->activated.notify(
		mCB(this,uiWellMarkersDispProperties,propChg) );
    samecolasmarkerfld_->activated.notify(
		mCB(this,uiWellMarkersDispProperties,markerFldsChged));
    singlecolfld_->activated.notify(
		mCB(this,uiWellMarkersDispProperties,propChg) );
    singlecolfld_->activated.notify(
		mCB(this,uiWellMarkersDispProperties,markerFldsChged));
    shapefld_->box()->selectionChanged.notify(
		mCB(this,uiWellMarkersDispProperties,propChg) );
    shapefld_->box()->selectionChanged.notify(
		mCB(this,uiWellMarkersDispProperties,markerFldsChged));
    displaymarkersfld_->itemChosen.notify(
			mCB(this,uiWellMarkersDispProperties,propChg) );
    displaymarkersfld_->itemChosen.notify(
			mCB(this,uiWellMarkersDispProperties,markerFldsChged) );
}


void uiWellMarkersDispProperties::getSelNames()
{
    mrkprops().selmarkernms_.erase();
    for ( int idx=0; idx<displaymarkersfld_->size(); idx++ )
    {
	if ( displaymarkersfld_->isChosen( idx ) )
	    mrkprops().selmarkernms_.add( displaymarkersfld_->textOfItem(idx) );
    }
}


void uiWellMarkersDispProperties::setSelNames()
{
    NotifyStopper ns( displaymarkersfld_->itemChosen );
    displaymarkersfld_->setChosen( mrkprops().selmarkernms_ );
}


void uiWellMarkersDispProperties::setAllMarkerNames(
					const BufferStringSet& allmarkernms )
{
    displaymarkersfld_->setEmpty();
    displaymarkersfld_->addItems( allmarkernms );
    setSelNames();
}


void uiWellMarkersDispProperties::resetProps(
				Well::DisplayProperties::Markers& pp )
{
    props_ = &pp;
    setSelNames();
}


void uiWellMarkersDispProperties::markerFldsChged( CallBacker* cb )
{
    colfld_->setSensitive( singlecolfld_->isChecked() );
    nmcolfld_->setSensitive( !samecolasmarkerfld_->isChecked() );
    cylinderheightfld_->display( !shapefld_->box()->currentItem() && !is2d_ );
}


void uiWellMarkersDispProperties::doPutToScreen()
{
    NotifyStopper ns1( cylinderheightfld_->box()->valueChanging );

    shapefld_->box()->setCurrentItem( mrkprops().shapeint_ );
    cylinderheightfld_->box()->setValue( mrkprops().cylinderheight_ );
    singlecolfld_->setChecked( mrkprops().issinglecol_ );
    const int sz = mrkprops().font_.pointSize();
    if ( sz > 0 )
	nmsizefld_->box()->setValue( sz );

    int style = mrkprops().font_.weight()>FontData::Normal ? 1 : 0;
    if ( mrkprops().font_.isItalic() )
	style += 2;

    nmstylefld_->setValue( style );

    samecolasmarkerfld_->setChecked( mrkprops().samenmcol_ );
    nmcolfld_->setColor( mrkprops().nmcol_ );
    setSelNames();
}


void uiWellMarkersDispProperties::doGetFromScreen()
{
    mrkprops().shapeint_ = shapefld_->box()->currentItem();
    mrkprops().cylinderheight_ = cylinderheightfld_->box()->getValue();
    mrkprops().issinglecol_ = singlecolfld_->isChecked();
    mrkprops().font_.setPointSize( nmsizefld_->box()->getValue() );
    const int fontstyle = nmstylefld_->getIntValue();
    const bool bold = fontstyle==1 || fontstyle==3;
    mrkprops().font_.setWeight( bold ? FontData::Bold : FontData::Normal );
    mrkprops().font_.setItalic( fontstyle==2 || fontstyle==3 );
    mrkprops().samenmcol_ = samecolasmarkerfld_->isChecked();
    mrkprops().nmcol_ = nmcolfld_->color();
    getSelNames();
}


uiWellLogDispProperties::uiWellLogDispProperties( uiParent* p,
				const uiWellDispProperties::Setup& su,
				Well::DisplayProperties::Log& lp,
				const Well::LogSet* wl)
    : uiWellDispProperties(p,su,lp)
{

    stylefld_ = new uiCheckList( this, uiCheckList::OneOnly, OD::Horizontal );
    stylefld_->addItem( tr("Well log") )
	      .addItem( tr("Seismic") );
    if ( !setup_.onlyfor2ddisplay_ )
	stylefld_->addItem( tr("Log tube") );
    stylefld_->setLabel( tr("Style") );
    stylefld_->attach( alignedAbove, szfld_ );

    uiSeparator* sep1 = new uiSeparator( this, "Sep" );
    sep1->attach( stretchedAbove, stylefld_ );

    rangefld_ = new uiGenInput( this, tr("Log range (min/max)"),
			     FloatInpIntervalSpec()
			     .setName(BufferString(" range start"),0)
			     .setName(BufferString(" range stop"),1) );
    rangefld_->attach( alignedAbove, stylefld_ );
    sep1->attach( stretchedBelow, rangefld_ );

    uiStringSet choice;
    choice.add( tr( "clip rate" ) );  choice.add( tr( "data range" ) );

    cliprangefld_ = new uiGenInput( this, tr("Specify"),
	StringListInpSpec(choice));
    cliprangefld_->attach( alignedAbove, rangefld_ );

    clipratefld_ = new uiGenInput( this, tr("Clip rate"), StringInpSpec() );
    clipratefld_->setElemSzPol( uiObject::Small );
    clipratefld_->attach( alignedBelow, cliprangefld_ );

    logarithmfld_ = new uiCheckBox( this, tr("Logarithmic") );
    logarithmfld_->attach( rightOf, rangefld_ );

    revertlogfld_ = new uiCheckBox( this, tr("Flip") );
    revertlogfld_->attach( rightOf, cliprangefld_ );

    lblr_ = new uiLabeledSpinBox( this, tr("Repeat") );
    repeatfld_ = lblr_ ->box();
    repeatfld_->setInterval( 1, 20, 1 );
    lblr_->attach( alignedBelow, colfld_ );

    logsfld_ = new uiLabeledComboBox( this, tr("Select log") );
    logsfld_->box()->setHSzPol( uiObject::Wide );
    logsfld_->attach( alignedAbove, cliprangefld_ );

    logfilltypefld_ = new uiLabeledComboBox( this, tr("Fill"));
    logfilltypefld_->box()->addItem( uiStrings::sNone() );
    logfilltypefld_->box()->addItem( tr("Left of log") );
    logfilltypefld_->box()->addItem( tr("Right of log") );
    logfilltypefld_->box()->addItem( tr("Full panel") );
    logfilltypefld_->attach( alignedBelow, colfld_ );

    filllogsfld_ = new uiLabeledComboBox( this, tr( "Fill with ") );
    filllogsfld_->attach( alignedBelow, logfilltypefld_ );

    singlfillcolfld_ = new uiCheckBox( this, tr("single color") );
    singlfillcolfld_->attach( rightOf, logfilltypefld_ );

    setLogSet( wl );

    coltablistfld_ = new uiColorTableSel( this, "Table selection" );
    coltablistfld_->attach( alignedBelow, filllogsfld_ );

    colorrangefld_ = new uiGenInput( this, uiString::emptyString(),
			     FloatInpIntervalSpec()
			     .setName(BufferString(" range start"),0)
			     .setName(BufferString(" range stop"),1) );
    colorrangefld_->attach( rightOf, coltablistfld_ );

    flipcoltabfld_ = new uiCheckBox( this, tr("flip color table") );
    flipcoltabfld_->attach( rightOf, filllogsfld_ );

    uiSeparator* sep2 = new uiSeparator( this, "Sep" );
    sep2->attach( stretchedBelow, coltablistfld_ );

    const uiString lbl =
       tr("Log display width %1").arg(getDistUnitString(SI().xyInFeet(),true));
    logwidthslider_ = new uiSlider( this, uiSlider::Setup(lbl).withedit(true) );
    logwidthslider_->attach( alignedBelow, coltablistfld_ );
    logwidthslider_->attach( ensureBelow, sep2 );

    logwidthslider_->setMinValue( 0.f );
    logwidthslider_->setMaxValue( 10000.0f );
    logwidthslider_->setStep( 250.0f );

    seiscolorfld_ = new uiColorInput( this,
		                 uiColorInput::Setup(logprops().seiscolor_)
			        .lbltxt(tr("Filling color")) );
    seiscolorfld_->attach( alignedBelow, lblr_ );
    seiscolorfld_->display(false);

    lblo_ = new uiLabeledSpinBox( this, tr("Overlap") );
    ovlapfld_ = lblo_->box();
    ovlapfld_->setInterval( 0, 100, 20 );
    lblo_->attach( rightOf, lblr_ );

    fillcolorfld_ = new uiColorInput( this,
		                 uiColorInput::Setup(logprops().seiscolor_)
			        .lbltxt(tr("Filling color")) );
    fillcolorfld_->attach( alignedBelow, logfilltypefld_ );
    fillcolorfld_->display(false);

    putToScreen();

    CallBack propchgcb = mCB(this,uiWellLogDispProperties,propChg);
    CallBack choiceselcb = mCB(this,uiWellLogDispProperties,choiceSel);
    cliprangefld_->valuechanged.notify( choiceselcb );
    clipratefld_->valuechanged.notify( choiceselcb );
    colorrangefld_->valuechanged.notify( choiceselcb );
    rangefld_->valuechanged.notify( choiceselcb );
    clipratefld_->valuechanged.notify( propchgcb );
    coltablistfld_->selectionChanged.notify( propchgcb );
    colorrangefld_->valuechanged.notify( propchgcb );
    fillcolorfld_->colorChanged.notify( propchgcb );
    logwidthslider_->valueChanged.notify(propchgcb);

    logarithmfld_->activated.notify( propchgcb );
    ovlapfld_->valueChanging.notify( propchgcb );
    rangefld_->valuechanged.notify( propchgcb );
    repeatfld_->valueChanging.notify( propchgcb );
    revertlogfld_->activated.notify( propchgcb );
    seiscolorfld_->colorChanged.notify( propchgcb );
    singlfillcolfld_->activated.notify( propchgcb );
    stylefld_->changed.notify( propchgcb );
    logfilltypefld_->box()->selectionChanged.notify( propchgcb );
    flipcoltabfld_->activated.notify( propchgcb );

    filllogsfld_->box()->selectionChanged.notify(
		mCB(this,uiWellLogDispProperties,updateFillRange) );
    logsfld_->box()->selectionChanged.notify(
		mCB(this,uiWellLogDispProperties,logSel) );
    logsfld_->box()->selectionChanged.notify(
		mCB(this,uiWellLogDispProperties,updateRange) );
    logsfld_->box()->selectionChanged.notify(
		mCB(this,uiWellLogDispProperties,updateFillRange) );
    logfilltypefld_->box()->selectionChanged.notify(
		mCB(this,uiWellLogDispProperties,isFilledSel));
    singlfillcolfld_->activated.notify(
		mCB(this,uiWellLogDispProperties,isFilledSel) );

    stylefld_->changed.notify(
	mCB(this,uiWellLogDispProperties,isStyleChanged) );
}


void uiWellLogDispProperties::resetProps( Well::DisplayProperties::Log& pp )
{
    props_ = &pp;
}


#define mSetSwapFillIdx( fidx )\
        if ( logprops().islogreverted_ )\
	{ if ( fidx == 2 ) fidx = 1; else if ( fidx == 1 ) fidx =2; }

void uiWellLogDispProperties::doPutToScreen()
{
    NotifyStopper nssfc( singlfillcolfld_->activated );
    NotifyStopper nso( ovlapfld_->valueChanging );
    NotifyStopper nsr( repeatfld_->valueChanging );
    NotifyStopper nsl( logarithmfld_->activated );
    NotifyStopper nsrev( revertlogfld_->activated );
    NotifyStopper nsslf( logfilltypefld_->box()->selectionChanged );
    NotifyStopper nsstylefld( stylefld_->changed );
    NotifyStopper nssliderfld( logwidthslider_->valueChanged );

    revertlogfld_->setChecked( logprops().islogreverted_ );
    logsfld_->box()->setText( logprops().name_ );
    rangefld_->setValue( logprops().range_ );
    colorrangefld_->setValue( logprops().fillrange_ );
    filllogsfld_->box()-> setText( logprops().fillname_ );

    stylefld_->setChecked( logprops().style_, true );
    logarithmfld_->setChecked( logprops().islogarithmic_ );
    coltablistfld_->setCurrent( logprops().seqname_ );
    flipcoltabfld_->setChecked( logprops().iscoltabflipped_ );
    ovlapfld_->setValue( logprops().repeatovlap_ );
    repeatfld_->setValue( logprops().repeat_ );
    int fidx = logprops().isleftfill_ ? logprops().isrightfill_ ? 3 : 1
				      : logprops().isrightfill_ ? 2 : 0;
    mSetSwapFillIdx( fidx )

    logfilltypefld_->box()->setCurrentItem( fidx );
    singlfillcolfld_->setChecked( logprops().issinglecol_ );
    clipratefld_->setValue( logprops().cliprate_ );
    cliprangefld_->setValue( logprops().isdatarange_ );
    if ( mIsUdf( logprops().cliprate_) || logprops().cliprate_ > 100  )
    {
	cliprangefld_->setValue( true );
	clipratefld_->setValue( 0.0 );
    }

    logwidthslider_->setValue( logprops().logwidth_ );

    if (logprops().style_ != 1 )
	fillcolorfld_->setColor( logprops().seiscolor_ );
    else
	seiscolorfld_->setColor( logprops().seiscolor_ );

    logSel( 0 );
    isStyleChanged( 0 );
    choiceSel( 0 );
}


void uiWellLogDispProperties::doGetFromScreen()
{
    logprops().style_ = stylefld_->firstChecked();
    logprops().isdatarange_ = cliprangefld_->getBoolValue();
    logprops().cliprate_ = clipratefld_->getfValue();
    if ( mIsUdf( logprops().cliprate_) || logprops().cliprate_ > 100 )
    {
	logprops().cliprate_= 0.0;
        logprops().isdatarange_ = true;
    }
    logprops().range_ = rangefld_->getFInterval();
    bool isreverted = revertlogfld_->isChecked();
    if ( !logprops().range_.isRev() && isreverted )
	logprops().range_.sort( false );
    if ( logprops().range_.isRev() && !isreverted )
	logprops().range_.sort( true );

    logprops().fillrange_ = colorrangefld_->getFInterval();
    logprops().islogreverted_ = revertlogfld_->isChecked();
    logprops().islogarithmic_ = logarithmfld_->isChecked();
    logprops().issinglecol_ = singlfillcolfld_->isChecked();
    int fillidx = logfilltypefld_->box()->currentItem();
    mSetSwapFillIdx( fillidx )
    logprops().isleftfill_ = ( fillidx == 1 || fillidx == 3 );
    logprops().isrightfill_ = ( fillidx == 2 || fillidx == 3 );
    logprops().seqname_ = coltablistfld_->text();
    logprops().iscoltabflipped_ = flipcoltabfld_->isChecked();
    logprops().repeat_ = stylefld_->isChecked( 1 ) ? repeatfld_->getValue() : 1;
    logprops().repeatovlap_ = mCast( float, ovlapfld_->getValue() );
    logprops().seiscolor_ = logprops().style_ == 1 ? seiscolorfld_->color()
						 : fillcolorfld_->color();
    logprops().name_ = logsfld_->box()->text();
    logprops().fillname_ = filllogsfld_->box()->text();

    deflogwidth = logprops().logwidth_  = (int)logwidthslider_->getValue();

    if ( !setup_.onlyfor2ddisplay_ && curwelllogproperty_ &&
	    curwelllogproperty_!=this )
    {
	if ( curwelllogproperty_->logprops().style_ == 2 ||
	     ( curwelllogproperty_->logprops().style_ != 2 &&
	       logprops().style_ == 2 ) )
	{
	    logprops().name_  = "None";
	}
    }
}


void uiWellLogDispProperties::isFilledSel( CallBacker* )
{
    const bool iswelllogortube = !stylefld_->isChecked( 1 );
    const bool issinglecol = singlfillcolfld_->isChecked();
    const int fillidx = logfilltypefld_->box()->currentItem();
    const bool isleftfilled_ = fillidx == 1 || fillidx == 3;
    const bool isrightfilled_ = fillidx == 2 || fillidx == 3;
    const bool isfilled = isrightfilled_ || isleftfilled_;
    singlfillcolfld_->display( isfilled && iswelllogortube );
    coltablistfld_->display( iswelllogortube &&  isfilled && !issinglecol );
    seiscolorfld_->display( !iswelllogortube );
    fillcolorfld_->display( iswelllogortube && issinglecol && isfilled );
    filllogsfld_->display( iswelllogortube &&  isfilled && !issinglecol );
    colorrangefld_->display( iswelllogortube &&  isfilled && !issinglecol );
    flipcoltabfld_->display( isfilled && iswelllogortube && !issinglecol );
}


void uiWellLogDispProperties::disableLogDisplays()
{
    singlfillcolfld_->display( false );
    coltablistfld_->display( false);
    seiscolorfld_->display( false );
    fillcolorfld_->display( false );
    filllogsfld_->display( false );
    colorrangefld_->display( false );
    flipcoltabfld_->display( false );
    lblr_->display( false );
    lblo_->display( false );
    singlfillcolfld_->display( false );
    logwidthslider_->display( false );
    szfld_->display( false );
    colfld_->display( false );
    flipcoltabfld_->display( false );
    revertlogfld_->display( false );
    logfilltypefld_->display( false );
}


void uiWellLogDispProperties::setSeismicSel()
{
    setStyleSensitive( true );
    lblr_->display( !setup_.onlyfor2ddisplay_ );
    lblo_->display( !setup_.onlyfor2ddisplay_ );
    colfld_->display( true );
    szfld_->display( true );
    logwidthslider_->display( !setup_.onlyfor2ddisplay_ );
    isFilledSel(0);
}


void uiWellLogDispProperties::setWellLogSel()
{
    BufferString sel = logsfld_->box()->text();
    if ( sel == "None" || sel == "none" )
	setStyleSensitive( false );
    else
	setStyleSensitive( true );
    singlfillcolfld_->display( true );
    coltablistfld_->display( true );
    logfilltypefld_->display( true );
    filllogsfld_->display( true );
    colorrangefld_->display( true );
    flipcoltabfld_->display( true );
    logwidthslider_->display( !setup_.onlyfor2ddisplay_ );
    colfld_->display( true );
    szfld_->display( true );
    isFilledSel(0);
}


void uiWellLogDispProperties::setTubeSel()
{
    setWellLogSel();
}


void uiWellLogDispProperties::setStyleSensitive( bool yn )
{
    colfld_->setSensitive( yn );
    szfld_->setSensitive( yn );
    singlfillcolfld_->setSensitive( yn );
    logfilltypefld_->setSensitive( yn );
}


void uiWellLogDispProperties::isStyleChanged( CallBacker* )
{
    disableLogDisplays();

    const int style = logprops().style_;

    if ( style == 0 )
	setWellLogSel();
    else if ( style == 1 )
	setSeismicSel();
    else
	setTubeSel();
}


void uiWellLogDispProperties::setLogSet( const Well::LogSet* wls )
{
    wl_ = wls;
    BufferStringSet lognames;
    for ( int idx=0; idx< wl_->size(); idx++ )
	lognames.addIfNew( wl_->getLog(idx).name() );
    lognames.sort();
    logsfld_->box()->setEmpty();
    logsfld_->box()->addItem(uiStrings::sNone());
    logsfld_->box()->addItems( lognames );
    filllogsfld_->box()->setEmpty();
    filllogsfld_->box()->addItems( lognames );
}


void uiWellLogDispProperties::logSel( CallBacker* cb )
{
    setFieldVals();
    if ( cb )
	filllogsfld_->box()->setText( logsfld_->box()->text() );
}


void uiWellLogDispProperties::selNone()
{
    rangefld_->setValue( Interval<float>(0,0) );
    colorrangefld_->setValue( Interval<float>(0,0) );
    colfld_->setColor( logprops().color_ );
    seiscolorfld_->setColor( logprops().seiscolor_ );
    fillcolorfld_->setColor( logprops().seiscolor_ );
    stylefld_->setChecked( 0, true );
    setFldSensitive( false );
    cliprangefld_->setValue( true );
    clipratefld_->setValue( 0.0 );
    repeatfld_->setValue( 0 );
    ovlapfld_->setValue( 0 );
    singlfillcolfld_->setChecked( false );
    coltablistfld_->setCurrent( logprops().seqname_ );
    logwidthslider_->setValue( deflogwidth );
}



void uiWellLogDispProperties::setFldSensitive( bool yn )
{
    rangefld_->setSensitive( yn );
    colorrangefld_->setSensitive( yn );
    cliprangefld_->setSensitive( yn );
    revertlogfld_->setSensitive( yn );
    colfld_->setSensitive( yn );
    seiscolorfld_->setSensitive( yn );
    fillcolorfld_->setSensitive( yn );
    stylefld_->setSensitive( yn );
    clipratefld_->setSensitive( yn );
    lblr_->setSensitive( yn );
    szfld_->setSensitive( yn );
    singlfillcolfld_->setSensitive( yn );
    coltablistfld_->setSensitive( yn );
    filllogsfld_->setSensitive(yn);
    flipcoltabfld_->setSensitive( yn );
    logarithmfld_->setSensitive(yn);
    logwidthslider_->setSensitive(yn);
    logfilltypefld_->setSensitive(yn);
}


void uiWellLogDispProperties::choiceSel( CallBacker* )
{
    const int isdatarange = cliprangefld_->getBoolValue();
    rangefld_->display( isdatarange );
    clipratefld_->display( !isdatarange );
}


void uiWellLogDispProperties::setFieldVals()
{
    BufferString sel = logsfld_->box()->text();
    if ( sel == "None" || sel == "none" )
    {
	selNone();
	return;
    }
    setFldSensitive( true );
}


void uiWellLogDispProperties::updateRange( CallBacker* )
{
    const char* lognm = logsfld_->box()->textOfItem(
		        logsfld_->box()->currentItem() );
    const int logno = wl_->indexOf( lognm );
    if ( logno<0 ) return;

    rangefld_->setValue( wl_->getLog(logno).valueRange() );
    propChanged.trigger();
}


void uiWellLogDispProperties::updateFillRange( CallBacker* )
{
    const char* lognm = filllogsfld_->box()->textOfItem(
			filllogsfld_->box()->currentItem() );
    const int logno = wl_->indexOf( lognm );
    if ( logno<0 ) return;

    colorrangefld_->setValue( wl_->getLog(logno).valueRange() );
    propChanged.trigger();
}



void uiWellLogDispProperties::calcRange( const char* lognm,
					 Interval<float>& valr )
{
    valr.set( mUdf(float), -mUdf(float) );
    for ( int idy=0; idy<wl_->size(); idy++ )
    {
	if ( wl_->getLog(idy).name() == lognm )
	{
	    const int logno = wl_->indexOf( lognm );
	    Interval<float> range = wl_->getLog(logno).valueRange();
	    if ( valr.start > range.start )
		valr.start = range.start;
	    if ( valr.stop < range.stop )
		valr.stop = range.stop;
	}
    }
}

