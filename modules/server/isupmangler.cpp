/**
 * isupmangler.cpp
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * ISUP parameter mangling in a STP
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2010 Null Team
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

#include <yatephone.h>
#include <yatesig.h>

using namespace TelEngine;
namespace { // anonymous

class IsupIntercept : public SS7ISUP
{
    friend class IsupMangler;
    YCLASS(IsupIntercept,SS7ISUP)
public:
    inline IsupIntercept(const NamedList& params)
	: SignallingComponent(params,&params), SS7ISUP(params),
	  m_used(true), m_symmetric(false)
	{ m_symmetric = params.getBoolValue("symmetric",m_symmetric); }
    virtual bool initialize(const NamedList* config);
    void dispatched(SS7MsgISUP& isup, const Message& msg, const SS7Label& label, int sls, bool accepted);
protected:
    virtual HandledMSU receivedMSU(const SS7MSU& msu, const SS7Label& label,
	SS7Layer3* network, int sls);
    virtual bool processMSU(SS7MsgISUP::Type type, unsigned int cic,
        const unsigned char* paramPtr, unsigned int paramLen,
	const SS7Label& label, SS7Layer3* network, int sls);
private:
    bool m_used;
    bool m_symmetric;
};

class IsupMessage : public Message
{
public:
    inline IsupMessage(const char* name, IsupIntercept* isup, SS7MsgISUP* msg,
	const SS7Label& label, int sls)
	: Message(name),
	  m_isup(isup), m_msg(msg), m_lbl(label), m_sls(sls), m_accepted(false)
	{ }
    inline ~IsupMessage()
	{ if (m_isup && m_msg) m_isup->dispatched(*m_msg,*this,m_lbl,m_sls,m_accepted); }
protected:
    virtual void dispatched(bool accepted)
	{ m_accepted = accepted; }
private:
    RefPointer<IsupIntercept> m_isup;
    RefPointer<SS7MsgISUP> m_msg;
    SS7Label m_lbl;
    int m_sls;
    bool m_accepted;
};

class IsupMangler : public Plugin
{
public:
    inline IsupMangler()
	: Plugin("isupmangler")
	{ Output("Loaded module ISUP Mangler"); }
    inline ~IsupMangler()
	{ Output("Unloading module ISUP Mangler"); }
    virtual void initialize();
};

static ObjList s_manglers;

INIT_PLUGIN(IsupMangler);


bool IsupIntercept::initialize(const NamedList* config)
{
    if (!config)
	return false;
    SS7ISUP::initialize(config);
    m_symmetric = config->getBoolValue("symmetric",m_symmetric);
    Debug(this,DebugAll,"Added %u Point Codes",setPointCode(*config));
    return true;
}

HandledMSU IsupIntercept::receivedMSU(const SS7MSU& msu, const SS7Label& label, SS7Layer3* network, int sls)
{
    if (msu.getSIF() != sif())
	return HandledMSU::Rejected;
    if (!hasPointCode(label.dpc()) || !handlesRemotePC(label.opc())) {
	if (!m_symmetric || !hasPointCode(label.opc()) || !handlesRemotePC(label.dpc()))
	    return HandledMSU::Rejected;
    }
    // we should have at least 2 bytes CIC and 1 byte message type
    const unsigned char* s = msu.getData(label.length()+1,3);
    if (!s) {
	Debug(this,DebugNote,"Got short MSU");
	return HandledMSU::Rejected;
    }
    unsigned int len = msu.length()-label.length()-1;
    unsigned int cic = s[0] | (s[1] << 8);
    SS7MsgISUP::Type type = (SS7MsgISUP::Type)s[2];
    String name = SS7MsgISUP::lookup(type);
    if (!name) {
        String tmp;
	tmp.hexify((void*)s,len,' ');
	Debug(this,DebugMild,"Received unknown ISUP type 0x%02x, cic=%u, length %u: %s",
	    type,cic,len,tmp.c_str());
	name = (int)type;
    }
    switch (type) {
	case SS7MsgISUP::IAM:
	    return processMSU(type,cic,s+3,len-3,label,network,sls) ?
		HandledMSU::Accepted : HandledMSU::Rejected;
	default:
	    // let the message pass through
	    return HandledMSU::Rejected;
    }
}

bool IsupIntercept::processMSU(SS7MsgISUP::Type type, unsigned int cic,
    const unsigned char* paramPtr, unsigned int paramLen,
    const SS7Label& label, SS7Layer3* network, int sls)
{
    XDebug(this,DebugAll,"IsupIntercept::processMSU(%u,%u,%p,%u,%p,%p,%d) [%p]",
	type,cic,paramPtr,paramLen,&label,network,sls,this);

    SS7MsgISUP* msg = new SS7MsgISUP(type,cic);
    if (!SS7MsgISUP::lookup(type)) {
	String tmp;
	tmp.hexify(&type,1);
	msg->params().assign("Message_" + tmp);
    }
    if (!decodeMessage(msg->params(),type,label.type(),paramPtr,paramLen)) {
	TelEngine::destruct(msg);
	return false;
    }

    if (debugAt(DebugAll)) {
	String tmp;
	tmp << label;
	Debug(this,DebugAll,"Received message '%s' cic=%u label=%s",
	    msg->name(),msg->cic(),tmp.c_str());
    }
    IsupMessage* m = new IsupMessage("isup.mangle",this,msg,label,sls);
    String addr;
    addr << toString() << "/" << cic;
    m->addParam("address",addr);
    m->addParam("sls",String(sls));
    m->copyParams(msg->params());
    TelEngine::destruct(msg);
    return Engine::enqueue(m);
}

void IsupIntercept::dispatched(SS7MsgISUP& isup, const Message& msg, const SS7Label& label, int sls, bool accepted)
{
    SS7MSU* msu = createMSU(isup.type(),ssf(),label,isup.cic(),&msg);
    if (!msu || (transmitMSU(*msu,label,sls) < 0))
	Debug(this,DebugWarn,"Failed to forward mangled %s (%u) [%p]",
	    SS7MsgISUP::lookup(isup.type()),isup.cic(),this);
    TelEngine::destruct(msu);
}


void IsupMangler::initialize()
{
    Output("Initializing module ISUP Mangler");
    SignallingEngine* engine = SignallingEngine::self();
    if (!engine) {
	Debug(DebugWarn,"SignallingEngine not yet created, cannot install ISUP manglers [%p]",this);
	return;
    }
    unsigned int n = s_manglers.length();
    unsigned int i;
    for (i = 0; i < n; i++) {
	IsupIntercept* isup = YOBJECT(IsupIntercept,s_manglers[i]);
	if (isup)
	    isup->m_used = false;
    }
    Configuration cfg(Engine::configFile("isupmangler"));
    n = cfg.sections();
    for (i = 0; i < n; i++) {
	NamedList* sect = cfg.getSection(i);
	if (TelEngine::null(sect))
	    continue;
	if (!sect->getBoolValue("enable",true))
	    continue;
	IsupIntercept* isup = YOBJECT(IsupIntercept,s_manglers[*sect]);
	if (!isup) {
	    isup = new IsupIntercept(*sect);
	    engine->insert(isup);
	    s_manglers.append(isup);
	}
	isup->m_used = true;
	isup->initialize(sect);
    }
    ListIterator iter(s_manglers);
    while (IsupIntercept* isup = YOBJECT(IsupIntercept,iter.get())) {
	if (!isup->m_used)
	    s_manglers.remove(isup);
    }
}

UNLOAD_PLUGIN(unloadNow)
{
    if (unloadNow)
	s_manglers.clear();
    return true;
}

}; // anonymous namespace

/* vi: set ts=8 sw=4 sts=4 noet: */