/*+
________________________________________________________________________

 (C) dGB Beheer B.V.; (LICENSE) http://opendtect.org/OpendTect_license.txt
 Author:	Kristofer / Bert
 Date:		Aug 2014
________________________________________________________________________

-*/
static const char* rcsID mUsedVar = "$Id$";

#include "netreqpacket.h"
#include "atomic.h"
#include "settings.h"
#include "ptrman.h"


static Threads::Atomic<int> curreqid_;

static const char* sKeySizeLimit = "Network.Limit Packet Size";

od_int32 Network::RequestPacket::systemSizeLimit()
{
    const char* res = Settings::common().find( sKeySizeLimit );
    return toInt( res );
}

void Network::RequestPacket::setSystemSizeLimit( od_int32 val )
{
    const int curval = systemSizeLimit();
    if ( val < 0 ) val = 0;
    if ( curval == val )
	return;

    Settings& setts = Settings::common();
    setts.set( sKeySizeLimit, val );
    setts.write();
}


#define mRequestID_	(header_.int32s_[1])
#define mPayloadSize_	(header_.int32s_[0])
#define mSubID_		(header_.int16s_[4])


Network::RequestPacket::RequestPacket( od_int32 payloadsize )
    : payload_( 0 )
{
    setPayload( allocPayload(payloadsize), payloadsize );
    setSubID( cMoreSubID() );
    setRequestID( -1 );
}


Network::RequestPacket::~RequestPacket()
{ deleteAndZeroArrPtr( payload_ ); }


bool Network::RequestPacket::isOK() const
{
    return payloadSize()>=0 && requestID()>=0;
}


od_int32 Network::RequestPacket::getPayloadSize( const void* buf )
{
    return buf ? ((Header*)buf)->int32s_[0] : 0;
}


int Network::RequestPacket::setIsNewRequest()
{
    const int reqid = ++curreqid_;
    setRequestID( reqid );
    setSubID( cBeginSubID() );
    return reqid;
}


od_int32 Network::RequestPacket::payloadSize() const
{
    return mPayloadSize_;
}


od_int32 Network::RequestPacket::requestID() const
{
    return mRequestID_;
}


void Network::RequestPacket::setRequestID( od_int32 id )
{
    mRequestID_ = id;
}


od_int16 Network::RequestPacket::subID() const
{
    return mSubID_;
}


void Network::RequestPacket::setSubID( od_int16 sid )
{
    mSubID_ = sid;
}


OD::JSON::Object Network::RequestPacket::getDefaultJsonHeader( bool fortxt,
							       od_int32 paysz )
{
    OD::JSON::Object extendedhdr;
    extendedhdr.set( "byteorder", "little" );
    extendedhdr.set( "source", "OpendTect" );
    extendedhdr.set( "content-type", fortxt ? "text/json" : "binary" );
    if ( fortxt )
	extendedhdr.set( "content-encoding", "utf-8" );

    extendedhdr.set( "content-length", paysz );
    return extendedhdr;
}


Network::PacketFiller* Network::RequestPacket::finalize(
    const OD::JSON::Object& extendedhdr )
{
    BufferString hdrstr;
    extendedhdr.dumpJSon( hdrstr );
    const od_int64 paysz = extendedhdr.getIntValue("content-length");

    const od_int32 payloadsz =
    mCast(od_int32, PacketFiller::sizeFor(hdrstr) + paysz);
    void* payloadptr = allocPayload( payloadsz );
    if ( !payloadptr )
	return nullptr;

    setPayload( payloadptr, payloadsz );

    PacketFiller* filler = new PacketFiller( *this );
    filler->put( hdrstr );

    return filler;
}


void* Network::RequestPacket::allocPayload( od_int32 size )
{
    if ( size < 1 )
	return 0;

    mDeclareAndTryAlloc( char*, pl, char[size] );
    return pl;
}


void Network::RequestPacket::setPayload( void* ptr, od_int32 size )
{
    deleteAndZeroArrPtr( payload_ );

    payload_ = (char*)ptr;
    mPayloadSize_ = payload_ ? size : 0;
}


void Network::RequestPacket::setStringPayload( const char* str )
{
    const int sz = FixedString( str ).size();
    if ( sz < 1 )
	setPayload( 0, 0 );
    else
    {
	int newplsz = sz + sizeof(int);
	void* newpl = allocPayload( newplsz );
	if ( !newpl )
	    newplsz = 0;
	else
	{
	    *((int*)newpl) = sz;
	    OD::memCopy( ((char*)newpl)+sizeof(int), str, sz );
	}

	setPayload( newpl, newplsz );
    }
}


bool Network::RequestPacket::setPayload( const OD::JSON::Object& req )
{
    BufferString reqstr;
    req.dumpJSon( reqstr );

    OD::JSON::Object hdr = getDefaultJsonHeader( true,
						 PacketFiller::sizeFor(reqstr));
    hdr.set( "content-type", "text/json" );
    PtrMan<PacketFiller> filler = finalize( hdr );
    if ( !filler )
	return false;

    filler->put( reqstr );
    return true;
}


const void* Network::RequestPacket::payload() const
{
    return payload_;
}


void* Network::RequestPacket::payload( bool takeover )
{
    void* res = payload_;
    if ( takeover )
	payload_ = 0;
    return res;
}


void Network::RequestPacket::getStringPayload( BufferString& str ) const
{
    str.setEmpty();
    if ( !payload_ )
	return;

    const int plsz = mPayloadSize_;
    if ( plsz <= sizeof(int) )
	return;

    int nrchars = *((int*)payload_);
    if ( nrchars > plsz-sizeof(int) )
    {
	pErrMsg("Payload not consistent with string");
	nrchars = plsz-sizeof(int);
    }

    if ( nrchars < 1 )
	return;

    str.setBufSize( nrchars+1 );
    char* cstr = str.getCStr();
    if ( cstr )
    {
	OD::memCopy( cstr, payload_ + sizeof(int), nrchars );
	*(cstr+nrchars) = '\0';
    }
}


Network::PacketInterpreter* Network::RequestPacket::readJsonHeader(
    OD::JSON::Object& hdr,
    uiRetVal& uirv ) const
    {
	Network::PacketInterpreter* interp =
				    new Network::PacketInterpreter(*this);
	BufferString jsonhdrstr( interp->getString() );
	uirv = hdr.parseJSon( jsonhdrstr.getCStr(), jsonhdrstr.size() );
	return interp;
    }


uiRetVal Network::RequestPacket::getPayload( OD::JSON::Object& json ) const
{
    OD::JSON::Object hdr;
    uiRetVal uirv;
    PtrMan<PacketInterpreter> interp = readJsonHeader( hdr, uirv );
    if ( !interp || !uirv.isOK() )
	return uirv;

    const BufferString type( hdr.getStringValue("content-type") );
    if ( type != "text/json" )
    {
	uirv = tr("Incorrect return type");
	uirv.add( tr("Expected: %1").arg("text/json") );
	uirv.add( tr("Received: %1").arg(type) );
	return uirv;
    }

    BufferString jsonstr( interp->getString() );
    return json.parseJSon( jsonstr.getCStr(), jsonstr.size() );
}


void Network::RequestPacket::addErrMsg( BufferString& msg ) const
{
    if ( !isError() )
    {
	pErrMsg( "addErrMsg for non-error packet requested" );
	return;
    }

    BufferString mymsg; getStringPayload( mymsg );
    if ( mymsg.isEmpty() )
    {
	pErrMsg( "No error message in error packet" );
	mymsg = "Unknown network error occurred";
    }
    msg.add( ":\n" ).add( mymsg );

}
