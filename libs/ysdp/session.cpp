/**
 * session.cpp
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * SDP media handling
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2004-2009 Null Team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <yatesdp.h>

namespace TelEngine {

/*
 * SDPSession
 */
SDPSession::SDPSession(SDPParser* parser)
    : m_parser(parser), m_mediaStatus(MediaMissing),
      m_rtpForward(false), m_sdpForward(false), m_rtpMedia(0),
      m_sdpSession(0), m_sdpVersion(0),
      m_secure(m_parser->m_secure), m_rfc2833(m_parser->m_rfc2833)
{
}

SDPSession::SDPSession(SDPParser* parser, NamedList& params)
    : m_parser(parser), m_mediaStatus(MediaMissing),
      m_rtpForward(false), m_sdpForward(false), m_rtpMedia(0),
      m_sdpSession(0), m_sdpVersion(0)
{
    m_rtpForward = params.getBoolValue("rtp_forward");
    m_secure = params.getBoolValue("secure",parser->m_secure);
    m_rfc2833 = params.getBoolValue("rfc2833",parser->m_rfc2833);
}

SDPSession::~SDPSession()
{
    resetSdp();
}

// Set new media list. Return true if changed
bool SDPSession::setMedia(ObjList* media)
{
    if (media == m_rtpMedia)
	return false;
    DDebug(m_parser,DebugAll,"SDPSession::setMedia(%p) [%p]",media,this);
    ObjList* tmp = m_rtpMedia;
    m_rtpMedia = media;
    bool chg = m_rtpMedia != 0;
    if (tmp) {
	chg = false;
	for (ObjList* o = tmp->skipNull(); o; o = o->skipNext()) {
	    SDPMedia* m = static_cast<SDPMedia*>(o->get());
	    if (media && m->sameAs(static_cast<SDPMedia*>((*media)[*m]),m_parser->ignorePort()))
		continue;
	    chg = true;
	    mediaChanged(*m);
	}
	TelEngine::destruct(tmp);
    }
    return chg;
}

// Put the list of net media in a parameter list
void SDPSession::putMedia(NamedList& msg, ObjList* mList, bool putPort)
{
    if (!mList)
	return;
    for (mList = mList->skipNull(); mList; mList = mList->skipNext()) {
	SDPMedia* m = static_cast<SDPMedia*>(mList->get());
	m->putMedia(msg,putPort);
    }
}

// Build and dispatch a chan.rtp message for a given media. Update media on success
bool SDPSession::dispatchRtp(SDPMedia* media, const char* addr, bool start,
    bool pick, RefObject* context)
{
    DDebug(m_parser,DebugAll,"SDPSession::dispatchRtp(%p,%s,%u,%u,%p) [%p]",
	media,addr,start,pick,context,this);
    Message* m = buildChanRtp(media,addr,start,context);
    if (!(m && Engine::dispatch(m))) {
	TelEngine::destruct(m);
	return false;
    }
    media->update(*m,start);
    if (!pick) {
	TelEngine::destruct(m);
	return true;
    }
    m_rtpForward = false;
    m_rtpLocalAddr = m->getValue("localip",m_rtpLocalAddr);
    m_mediaStatus = MediaStarted;
    const char* sdpPrefix = m->getValue("osdp-prefix","osdp");
    if (sdpPrefix) {
	unsigned int n = m->length();
	for (unsigned int j = 0; j < n; j++) {
	    const NamedString* param = m->getParam(j);
	    if (!param)
		continue;
	    String tmp = param->name();
	    if (tmp.startSkip(sdpPrefix,false) && tmp.startSkip("_",false) && tmp)
	        media->parameter(tmp,*param,false);
	}
    }
    if (m_secure) {
	int tag = m->getIntValue("crypto_tag",1);
	tag = m->getIntValue("ocrypto_tag",tag);
	const String* suite = m->getParam("ocrypto_suite");
	const String* key = m->getParam("ocrypto_key");
	const String* params = m->getParam("ocrypto_params");
	if (suite && key && (tag > 0)) {
	    String sdes(tag);
	    sdes << " " << *suite << " " << *key;
	    if (params)
		sdes << " " << *params;
	    media->crypto(sdes,false);
	}
    }
    TelEngine::destruct(m);
    return true;
}

// Repeatedly calls dispatchRtp() for each media in the list
// Update it on success. Remove it on failure
bool SDPSession::dispatchRtp(const char* addr, bool start, RefObject* context)
{
    if (!m_rtpMedia)
	return false;
    DDebug(m_parser,DebugAll,"SDPSession::dispatchRtp(%s,%u,%p) [%p]",
	addr,start,context,this);
    bool ok = false;
    ObjList* o = m_rtpMedia->skipNull();
    while (o) {
	SDPMedia* m = static_cast<SDPMedia*>(o->get());
	if (dispatchRtp(m,addr,start,true,context)) {
	    ok = true;
	    o = o->skipNext();
	}
	else {
	    Debug(m_parser,DebugMild,
		"Removing failed SDP media '%s' format '%s' from offer [%p]",
		m->c_str(),m->format().safe(),this);
	    o->remove();
	    o = o->skipNull();
	}
    }
    return ok;
}

// Try to start RTP for all media
bool SDPSession::startRtp(RefObject* context)
{
    if (m_rtpForward || !m_rtpMedia || (m_mediaStatus != MediaStarted))
	return false;
    DDebug(m_parser,DebugAll,"SDPSession::startRtp(%p) [%p]",context,this);
    bool ok = false;
    for (ObjList* o = m_rtpMedia->skipNull(); o; o = o->skipNext()) {
	SDPMedia* m = static_cast<SDPMedia*>(o->get());
	ok = dispatchRtp(m,m_rtpAddr,true,false,context) || ok;
    }
    return ok;
}

// Update from parameters. Build a default SDP if no media is found in params
bool SDPSession::updateSDP(const NamedList& params)
{
    DDebug(m_parser,DebugAll,"SDPSession::updateSdp('%s') [%p]",params.c_str(),this);
    bool defaults = true;
    const char* sdpPrefix = params.getValue("osdp-prefix","osdp");
    ObjList* lst = 0;
    unsigned int n = params.length();
    String defFormats;
    m_parser->getAudioFormats(defFormats);
    for (unsigned int i = 0; i < n; i++) {
	const NamedString* p = params.getParam(i);
	if (!p)
	    continue;
	// search for rtp_port or rtp_port_MEDIANAME parameters
	String tmp(p->name());
	if (!tmp.startSkip("media",false))
	    continue;
	if (tmp && (tmp[0] != '_'))
	    continue;
	// since we found at least one media declaration disable defaults
	defaults = false;
	// now tmp holds the suffix for the media, null for audio
	bool audio = tmp.null();
	// check if media is supported, default only for audio
	if (!p->toBoolean(audio))
	    continue;
	String fmts = params.getValue("formats" + tmp);
	if (audio && fmts.null())
	    fmts = defFormats;
	if (fmts.null())
	    continue;
	String trans = params.getValue("transport" + tmp,"RTP/AVP");
	String crypto;
	if (m_secure)
	    crypto = params.getValue("crypto" + tmp);
	if (audio)
	    tmp = "audio";
	else
	    tmp >> "_";
	SDPMedia* rtp = 0;
	// try to take the media descriptor from the old list
	if (m_rtpMedia) {
	    ObjList* om = m_rtpMedia->find(tmp);
	    if (om)
		rtp = static_cast<SDPMedia*>(om->remove(false));
	}
	bool append = false;
	if (rtp)
	    rtp->update(fmts);
	else {
	    rtp = new SDPMedia(tmp,trans,fmts);
	    append = true;
	}
	rtp->crypto(crypto,false);
	if (sdpPrefix) {
	    for (unsigned int j = 0; j < n; j++) {
		const NamedString* param = params.getParam(j);
		if (!param)
		    continue;
		tmp = param->name();
		if (tmp.startSkip(sdpPrefix + rtp->suffix() + "_",false) && (tmp.find('_') < 0))
		    rtp->parameter(tmp,*param,append);
	    }
	}
	if (!lst)
	    lst = new ObjList;
	lst->append(rtp);
    }
    if (defaults && !lst) {
	lst = new ObjList;
	lst->append(new SDPMedia("audio","RTP/AVP",params.getValue("formats",defFormats)));
    }
    return setMedia(lst);
}


// Update RTP/SDP data from parameters
// Return true if media changed
bool SDPSession::updateRtpSDP(const NamedList& params)
{
    DDebug(m_parser,DebugAll,"SDPSession::updateRtpSDP(%s) [%p]",params.c_str(),this);
    String addr;
    ObjList* tmp = updateRtpSDP(params,addr,m_rtpMedia);
    if (tmp) {
	bool chg = (m_rtpLocalAddr != addr);
	m_rtpLocalAddr = addr;
	return setMedia(tmp) || chg;
    }
    return false;
}

// Creates a SDP body from transport address and list of media descriptors
// Use own list if given media list is 0
MimeSdpBody* SDPSession::createSDP(const char* addr, ObjList* mediaList)
{
    DDebug(m_parser,DebugAll,"SDPSession::createSDP('%s',%p) [%p]",addr,mediaList,this);
    if (!mediaList)
	mediaList = m_rtpMedia;
    // if we got no media descriptors we simply create no SDP
    if (!mediaList)
	return 0;
    if (m_sdpSession)
	++m_sdpVersion;
    else
	m_sdpVersion = m_sdpSession = Time::secNow();

    // no address means on hold or muted
    String origin;
    origin << "yate " << m_sdpSession << " " << m_sdpVersion;
    origin << " IN IP4 " << (addr ? addr : m_host.safe());
    String conn;
    conn << "IN IP4 " << (addr ? addr : "0.0.0.0");

    MimeSdpBody* sdp = new MimeSdpBody;
    sdp->addLine("v","0");
    sdp->addLine("o",origin);
    sdp->addLine("s",m_parser->m_sessionName);
    sdp->addLine("c",conn);
    sdp->addLine("t","0 0");

    Lock lock(m_parser);
    bool defcodecs = m_parser->m_codecs.getBoolValue("default",true);
    for (ObjList* ml = mediaList->skipNull(); ml; ml = ml->skipNext()) {
	SDPMedia* m = static_cast<SDPMedia*>(ml->get());
	String mline(m->fmtList());
	ObjList* l = mline.split(',',false);
	mline = *m;
	mline << " " << (m->localPort() ? m->localPort().c_str() : "0") << " " << m->transport();
	ObjList* map = m->mappings().split(',',false);
	ObjList rtpmap;
	String frm;
	int ptime = 0;
	ObjList* f = l;
	for (; f; f = f->next()) {
	    String* s = static_cast<String*>(f->get());
	    if (s) {
		int mode = 0;
		if (*s == "ilbc20")
		    ptime = mode = 20;
		else if (*s == "ilbc30")
		    ptime = mode = 30;
		else if (*s == "g729b")
		    continue;
		int payload = s->toInteger(SDPParser::s_payloads,-1);
		int defcode = payload;
		String tmp = *s;
		tmp << "=";
		for (ObjList* pl = map; pl; pl = pl->next()) {
		    String* mapping = static_cast<String*>(pl->get());
		    if (!mapping)
			continue;
		    if (mapping->startsWith(tmp)) {
			payload = -1;
			tmp = *mapping;
			tmp >> "=" >> payload;
			XDebug(m_parser,DebugAll,"RTP mapped payload %d for '%s' [%p]",
			    payload,s->c_str(),this);
			break;
		    }
		}
		if (payload >= 0) {
		    if (defcode < 0)
			defcode = payload;
		    const char* map = lookup(defcode,SDPParser::s_rtpmap);
		    if (map && m_parser->m_codecs.getBoolValue(*s,defcodecs && DataTranslator::canConvert(*s))) {
			frm << " " << payload;
			String* temp = new String("rtpmap:");
			*temp << payload << " " << map;
			rtpmap.append(temp);
			if (mode) {
			    temp = new String("fmtp:");
			    *temp << payload << " mode=" << mode;
			    rtpmap.append(temp);
			}
			if (*s == "g729") {
			    temp = new String("fmtp:");
			    *temp << payload << " annexb=" <<
				((0 != l->find("g729b")) ? "yes" : "no");
			    rtpmap.append(temp);
			}
			else if (*s == "amr") {
			    temp = new String("fmtp:");
			    *temp << payload << " octet-align=0";
			    rtpmap.append(temp);
			}
			else if (*s == "amr-o") {
			    temp = new String("fmtp:");
			    *temp << payload << " octet-align=1";
			    rtpmap.append(temp);
			}
		    }
		}
	    }
	}
	TelEngine::destruct(l);
	TelEngine::destruct(map);

	if (m_rfc2833 && frm && m->isAudio()) {
	    int rfc2833 = m->rfc2833().toInteger(-1);
	    if (rfc2833 < 0)
		rfc2833 = 101;
	    // claim to support telephone events
	    frm << " " << rfc2833;
	    String* s = new String;
	    *s << "rtpmap:" << rfc2833 << " telephone-event/8000";
	    rtpmap.append(s);
	}

	if (frm.null()) {
	    if (m->isAudio() || !m->fmtList()) {
		Debug(m_parser,DebugMild,"No formats for '%s', excluding from SDP [%p]",
		    m->c_str(),this);
		continue;
	    }
	    Debug(m_parser,DebugInfo,"Assuming formats '%s' for media '%s' [%p]",
		m->fmtList(),m->c_str(),this);
	    frm << " " << m->fmtList();
	    // brutal but effective
	    for (char* p = const_cast<char*>(frm.c_str()); *p; p++) {
		if (*p == ',')
		    *p = ' ';
	    }
	}

	if (ptime) {
	    String* temp = new String("ptime:");
	    *temp << ptime;
	    rtpmap.append(temp);
	}

	sdp->addLine("m",mline + frm);
	bool enc = false;
	if (m->isModified()) {
	    unsigned int n = m->length();
	    for (unsigned int i = 0; i < n; i++) {
		const NamedString* param = m->getParam(i);
		if (param) {
		    String tmp = param->name();
		    if (*param)
			tmp << ":" << *param;
		    sdp->addLine("a",tmp);
		    enc = enc || (param->name() == "encryption");
		}
	    }
	}
	for (f = rtpmap.skipNull(); f; f = f->skipNext()) {
	    String* s = static_cast<String*>(f->get());
	    if (s)
		sdp->addLine("a",*s);
	}
	if (addr && m->localCrypto()) {
	    sdp->addLine("a","crypto:" + m->localCrypto());
	    if (!enc)
		sdp->addLine("a","encryption:optional");
	}
    }

    return sdp;
}

// Creates a SDP body for the current media status
MimeSdpBody* SDPSession::createSDP()
{
    switch (m_mediaStatus) {
	case MediaStarted:
	    return createSDP(getRtpAddr());
	case MediaMuted:
	    return createSDP(0);
	default:
	    return 0;
    }
}

// Creates a SDP from RTP address data present in message
MimeSdpBody* SDPSession::createPasstroughSDP(NamedList& msg, bool update)
{
    String tmp = msg.getValue("rtp_forward");
    msg.clearParam("rtp_forward");
    if (!(m_rtpForward && tmp.toBoolean()))
	return 0;
    String* raw = msg.getParam("sdp_raw");
    if (raw) {
	m_sdpForward = m_sdpForward || m_parser->sdpForward();
	if (m_sdpForward) {
	    msg.setParam("rtp_forward","accepted");
	    return new MimeSdpBody("application/sdp",raw->safe(),raw->length());
	}
    }
    String addr;
    ObjList* lst = updateRtpSDP(msg,addr,update ? m_rtpMedia : 0);
    if (!lst)
	return 0;
    MimeSdpBody* sdp = createSDP(addr,lst);
    if (update) {
	m_rtpLocalAddr = addr;
	setMedia(lst);
    }
    else
	TelEngine::destruct(lst);
    if (sdp)
	msg.setParam("rtp_forward","accepted");
    return sdp;
}

// Update media format lists from parameters
void SDPSession::updateFormats(const NamedList& msg)
{
    if (!m_rtpMedia)
	return;
    unsigned int n = msg.length();
    for (unsigned int i = 0; i < n; i++) {
	const NamedString* p = msg.getParam(i);
	if (!p)
	    continue;
	// search for formats_MEDIANAME parameters
	String tmp = p->name();
	if (!tmp.startSkip("formats",false))
	    continue;
	if (tmp && (tmp[0] != '_'))
	    continue;
	if (tmp.null())
	    tmp = "audio";
	else
	    tmp = tmp.substr(1);
	SDPMedia* rtp = static_cast<SDPMedia*>(m_rtpMedia->operator[](tmp));
	if (rtp && rtp->update(*p))
	    Debug(m_parser,DebugNote,"Formats for '%s' changed to '%s' [%p]",
		tmp.c_str(),p->c_str(),this);
    }
}

// Add raw SDP forwarding parameter
bool SDPSession::addSdpParams(NamedList& msg, const MimeBody* body)
{
    if (!(m_sdpForward && body))
	return false;
    const MimeSdpBody* sdp =
	static_cast<const MimeSdpBody*>(body->isSDP() ? body : body->getFirst("application/sdp"));
    if (!sdp)
	return false;
    const DataBlock& raw = sdp->getBody();
    String tmp((const char*)raw.data(),raw.length());
    return addSdpParams(msg,tmp);
}

// Add raw SDP forwarding parameter
bool SDPSession::addSdpParams(NamedList& msg, const String& rawSdp)
{
    if (!m_sdpForward)
	return false;
    msg.setParam("rtp_forward","yes");
    msg.addParam("sdp_raw",rawSdp);
    return true;
}

// Add RTP forwarding parameters to a message
bool SDPSession::addRtpParams(NamedList& msg, const String& natAddr,
    const MimeBody* body, bool force)
{
    if (!(m_rtpMedia && m_rtpAddr))
	return false;
    putMedia(msg,false);
    if (force || (!startRtp() && m_rtpForward)) {
	if (natAddr)
	    msg.addParam("rtp_nat_addr",natAddr);
	msg.addParam("rtp_forward","yes");
	msg.addParam("rtp_addr",m_rtpAddr);
	for (ObjList* o = m_rtpMedia->skipNull(); o; o = o->skipNext()) {
	    SDPMedia* m = static_cast<SDPMedia*>(o->get());
	    msg.addParam("rtp_port" + m->suffix(),m->remotePort());
	    if (m->isAudio())
		msg.addParam("rtp_rfc2833",m->rfc2833());
	}
	addSdpParams(msg,body);
	return true;
    }
    return false;
}

// Reset this object to default values
void SDPSession::resetSdp()
{
    m_mediaStatus = MediaMissing;
    TelEngine::destruct(m_rtpMedia);
    m_rtpForward = false;
    m_sdpForward = false;
    m_externalAddr.clear();
    m_rtpAddr.clear();
    m_rtpLocalAddr.clear();
    m_sdpSession = 0;
    m_sdpVersion = 0;
    m_host.clear();
    m_secure = m_parser->secure();
    m_rfc2833 = m_parser->rfc2833();
}

// Build a populated chan.rtp message
Message* SDPSession::buildChanRtp(SDPMedia* media, const char* addr, bool start, RefObject* context)
{
    if (!(media && addr))
	return 0;
    Message* m = buildChanRtp(context);
    if (!m)
	return 0;
    m->addParam("media",*media);
    m->addParam("transport",media->transport());
    m->addParam("direction","bidir");
    if (m_rtpLocalAddr)
	m->addParam("localip",m_rtpLocalAddr);
    m->addParam("remoteip",addr);
    if (start) {
	m->addParam("remoteport",media->remotePort());
	m->addParam("format",media->format());
	String tmp = media->format();
	tmp << "=";
	ObjList* mappings = media->mappings().split(',',false);
	for (ObjList* pl = mappings; pl; pl = pl->next()) {
	    String* mapping = static_cast<String*>(pl->get());
	    if (!mapping)
		continue;
	    if (mapping->startsWith(tmp)) {
		tmp = *mapping;
		tmp >> "=";
		m->addParam("payload",tmp);
		break;
	    }
	}
	m->addParam("evpayload",media->rfc2833());
	TelEngine::destruct(mappings);
    }
    if (m_secure) {
	if (media->remoteCrypto()) {
	    String sdes = media->remoteCrypto();
	    Regexp r("^\\([0-9]\\+\\) \\+\\([^ ]\\+\\) \\+\\([^ ]\\+\\) *\\(.*\\)$");
	    if (sdes.matches(r)) {
		m->addParam("secure",String::boolText(true));
		m->addParam("crypto_tag",sdes.matchString(1));
		m->addParam("crypto_suite",sdes.matchString(2));
		m->addParam("crypto_key",sdes.matchString(3));
		if (sdes.matchLength(4))
		    m->addParam("crypto_params",sdes.matchString(4));
	    }
	    else
		Debug(m_parser,DebugWarn,"Invalid SDES: '%s' [%p]",sdes.c_str(),this);
	}
	else if (media->securable())
	    m->addParam("secure",String::boolText(true));
    }
    else
	media->crypto(0,true);
    unsigned int n = media->length();
    for (unsigned int i = 0; i < n; i++) {
	const NamedString* param = media->getParam(i);
	if (!param)
	    continue;
	m->addParam("sdp_" + param->name(),*param);
    }
    return m;
}

// Check if local RTP data changed for at least one media
bool SDPSession::localRtpChanged() const
{
    if (!m_rtpMedia)
	return false;
    for (ObjList* o = m_rtpMedia->skipNull(); o; o = o->skipNext()) {
	SDPMedia* m = static_cast<SDPMedia*>(o->get());
	if (m->localChanged())
	    return true;
    }
    return false;
}

// Set or reset the local RTP data changed flag for all media
void SDPSession::setLocalRtpChanged(bool chg)
{
    if (!m_rtpMedia)
	return;
    for (ObjList* o = m_rtpMedia->skipNull(); o; o = o->skipNext())
	(static_cast<SDPMedia*>(o->get()))->setLocalChanged(chg);
}

// Update RTP/SDP data from parameters
ObjList* SDPSession::updateRtpSDP(const NamedList& params, String& rtpAddr, ObjList* oldList)
{
    DDebug(DebugAll,"SDPSession::updateRtpSDP(%s,%s,%p)",params.c_str(),rtpAddr.c_str(),oldList);
    rtpAddr = params.getValue("rtp_addr");
    if (!rtpAddr)
	return 0;
    const char* sdpPrefix = params.getValue("osdp-prefix","osdp");
    ObjList* lst = 0;
    unsigned int n = params.length();
    for (unsigned int i = 0; i < n; i++) {
	const NamedString* p = params.getParam(i);
	if (!p)
	    continue;
	// search for rtp_port or rtp_port_MEDIANAME parameters
	String tmp = p->name();
	if (!tmp.startSkip("rtp_port",false))
	    continue;
	if (tmp && (tmp[0] != '_'))
	    continue;
	// now tmp holds the suffix for the media, null for audio
	bool audio = tmp.null();
	// check if media is supported, default only for audio
	if (!params.getBoolValue("media" + tmp,audio))
	    continue;
	int port = p->toInteger();
	if (!port)
	    continue;
	const char* fmts = params.getValue("formats" + tmp);
	if (!fmts)
	    continue;
	String trans = params.getValue("transport" + tmp,"RTP/AVP");
	if (audio)
	    tmp = "audio";
	else
	    tmp >> "_";
	SDPMedia* rtp = 0;
	// try to take the media descriptor from the old list
	if (oldList) {
	    ObjList* om = oldList->find(tmp);
	    if (om)
		rtp = static_cast<SDPMedia*>(om->remove(false));
	}
	bool append = false;
	if (rtp)
	    rtp->update(fmts,-1,port);
	else {
	    rtp = new SDPMedia(tmp,trans,fmts,-1,port);
	    append = true;
	}
	if (sdpPrefix) {
	    for (unsigned int j = 0; j < n; j++) {
		const NamedString* param = params.getParam(j);
		if (!param)
		    continue;
		tmp = param->name();
		if (tmp.startSkip(sdpPrefix + rtp->suffix() + "_",false) && (tmp.find('_') < 0))
		    rtp->parameter(tmp,*param,append);
	    }
	}
	rtp->mappings(params.getValue("rtp_mapping" + rtp->suffix()));
	if (audio)
	    rtp->rfc2833(params.getIntValue("rtp_rfc2833",-1));
	rtp->crypto(params.getValue("crypto" + rtp->suffix()),false);
	if (!lst)
	    lst = new ObjList;
	lst->append(rtp);
    }
    return lst;
}

// Media changed notification.
void SDPSession::mediaChanged(const String& name)
{
    XDebug(m_parser,DebugAll,"SDPSession::mediaChanged(%s) [%p]",name.c_str(),this);
}

};   // namespace TelEngine

/* vi: set ts=8 sw=4 sts=4 noet: */