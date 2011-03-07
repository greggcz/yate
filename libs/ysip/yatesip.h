/*
 * yatesip.h
 * Yet Another SIP Stack
 * This file is part of the YATE Project http://YATE.null.ro
 *
 * Yet Another Telephony Engine - a fully featured software PBX and IVR
 * Copyright (C) 2004-2006 Null Team
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

#ifndef __YATESIP_H
#define __YATESIP_H

#include <yateclass.h>
#include <yatemime.h>

#ifdef _WINDOWS

#ifdef LIBYSIP_EXPORTS
#define YSIP_API __declspec(dllexport)
#else
#ifndef LIBYSIP_STATIC
#define YSIP_API __declspec(dllimport)
#endif
#endif

#endif /* _WINDOWS */

#ifndef YSIP_API
#define YSIP_API
#endif

/** 
 * Holds all Telephony Engine related classes.
 */
namespace TelEngine {

/**
 * Token table containing default human readable responses for answer codes
 */
extern YSIP_API TokenDict* SIPResponses;

class SIPEngine;
class SIPEvent;

class YSIP_API SIPParty : public RefObject
{
public:
    SIPParty();
    SIPParty(bool reliable);
    virtual ~SIPParty();
    virtual void transmit(SIPEvent* event) = 0;
    virtual const char* getProtoName() const = 0;
    virtual bool setParty(const URI& uri) = 0;
    inline const String& getLocalAddr() const
	{ return m_local; }
    inline const String& getPartyAddr() const
	{ return m_party; }
    inline int getLocalPort() const
	{ return m_localPort; }
    inline int getPartyPort() const
	{ return m_partyPort; }
    inline bool isReliable() const
	{ return m_reliable; }
protected:
    bool m_reliable;
    bool m_init;
    String m_local;
    String m_party;
    int m_localPort;
    int m_partyPort;
};

/**
 * An object that holds the sip message parsed into this library model.
 * This class can be used to parse a sip message from a text buffer, or it
 * can be used to create a text buffer from a sip message.
 * @short A container and parser for SIP messages
 */
class YSIP_API SIPMessage : public RefObject
{
public:
    /**
     * Various message flags
     */
    enum Flags {
	Defaults          =      0,
	NotReqRport       = 0x0001,
	NotAddAllow       = 0x0002,
	NotAddAgent       = 0x0004,
	RportAfterBranch  = 0x0008,
	NotSetRport       = 0x0010,
	NotSetReceived    = 0x0020,
    };

    /**
     * Copy constructor
     */
    SIPMessage(const SIPMessage& original);

    /**
     * Creates a new, empty, outgoing SIPMessage.
     */
    SIPMessage(const char* _method, const char* _uri, const char* _version = "SIP/2.0");

    /**
     * Creates a new SIPMessage from parsing a text buffer.
     */
    SIPMessage(SIPParty* ep, const char* buf, int len = -1);

    /**
     * Creates a new SIPMessage as answer to another message.
     */
    SIPMessage(const SIPMessage* message, int _code, const char* _reason = 0);

    /**
     * Creates an ACK message from an original message and a response.
     */
    SIPMessage(const SIPMessage* original, const SIPMessage* answer);

    /**
     * Destroy the message and all
     */
    virtual ~SIPMessage();

    /**
     * Construct a new SIP message by parsing a text buffer
     * @return A pointer to a valid new message or NULL
     */
    static SIPMessage* fromParsing(SIPParty* ep, const char* buf, int len = -1);

    /**
     * Complete missing fields with defaults taken from a SIP engine
     * @param engine Pointer to the SIP engine to use for extra parameters
     * @param user Username to set in the From header instead of that in rURI
     * @param domain Domain to use in From instead of the local IP address
     * @param dlgTag Value of dialog tag parameter to set in To header
     * @param flags Miscellaneous completion flags, -1 to take them from engine
     */
    void complete(SIPEngine* engine, const char* user = 0, const char* domain = 0, const char* dlgTag = 0, int flags = -1);

    /**
     * Copy an entire header line (including all parameters) from another message
     * @param message Pointer to the message to copy the header from
     * @param name Name of the header to copy
     * @param newName New name to force in headers, NULL to just copy
     * @return True if the header was found and copied
     */
    bool copyHeader(const SIPMessage* message, const char* name, const char* newName = 0);

    /**
     * Copy multiple header lines (including all parameters) from another message
     * @param message Pointer to the message to copy the header from
     * @param name Name of the headers to copy
     * @param newName New name to force in headers, NULL to just copy
     * @return Number of headers found and copied
     */
    int copyAllHeaders(const SIPMessage* message, const char* name, const char* newName = 0);

    /**
     * Get the endpoint this message uses
     * @return Pointer to the endpoint of this message
     */
    inline SIPParty* getParty() const
	{ return m_ep; }

    /**
     * Set the endpoint this message uses
     * @param ep Pointer to the endpoint of this message
     */
    void setParty(SIPParty* ep = 0);

    /**
     * Check if this message is valid as result of the parsing
     */
    inline bool isValid() const
	{ return m_valid; }

    /**
     * Check if this message is an answer or a request
     */
    inline bool isAnswer() const
	{ return m_answer; }

    /**
     * Check if this message is an outgoing message
     * @return True if this message should be sent to remote
     */
    inline bool isOutgoing() const
	{ return m_outgoing; }

    /**
     * Check if this message is an ACK message
     * @return True if this message has an ACK method
     */
    inline bool isACK() const
	{ return m_ack; }

    /**
     * Check if this message is handled by a reliable protocol
     * @return True if a reliable protocol (TCP, SCTP) is used
     */
    inline bool isReliable() const
	{ return m_ep ? m_ep->isReliable() : false; }

    /**
     * Get the Command Sequence number from this message
     * @return Number part of CSEQ in this message
     */
    inline int getCSeq() const
	{ return m_cseq; }

    /**
     * Get the last flags used by this message
     * @return Flags last used, ORed together
     */
    inline int getFlags() const
	{ return m_flags; }

    /**
     * Find a header line by name
     * @param name Name of the header to locate
     * @return A pointer to the first matching header line or 0 if not found
     */
    const MimeHeaderLine* getHeader(const char* name) const;

    /**
     * Find the last header line that matches a given name name
     * @param name Name of the header to locate
     * @return A pointer to the last matching header line or 0 if not found
     */
    const MimeHeaderLine* getLastHeader(const char* name) const;

    /**
     * Count the header lines matching a specific name
     * @param name Name of the header to locate
     * @return Number of matching header lines
     */
    int countHeaders(const char* name) const;

    /**
     * Find a header parameter by name
     * @param name Name of the header to locate
     * @param param Name of the parameter to locate in the tag
     * @param last Find the last header with that name instead of first
     * @return A pointer to the first matching header line or 0 if not found
     */
    const NamedString* getParam(const char* name, const char* param, bool last = false) const;

    /**
     * Get a string value (without parameters) from a header line
     * @param name Name of the header to locate
     * @param last Find the last header with that name instead of first
     * @return The value hold in the header or an empty String
     */
    const String& getHeaderValue(const char* name, bool last = false) const;

    /**
     * Get a string value from a parameter in a header line
     * @param name Name of the header to locate
     * @param param Name of the parameter to locate in the tag
     * @param last Find the last header with that name instead of first
     * @return The value hold in the parameter or an empty String
     */
    const String& getParamValue(const char* name, const char* param, bool last = false) const;

    /**
     * Append a new header line constructed from name and content
     * @param name Name of the header to add
     * @param value Content of the new header line
     */
    inline void addHeader(const char* name, const char* value = 0)
	{ header.append(new MimeHeaderLine(name,value)); }

    /**
     * Append an already constructed header line
     * @param line Header line to add
     */
    inline void addHeader(MimeHeaderLine* line)
	{ header.append(line); }

    /**
     * Clear all header lines that match a name
     * @param name Name of the header to clear
     */
    void clearHeaders(const char* name);

    /**
     * Set a header line constructed from name and content
     */
    inline void setHeader(const char* name, const char* value = 0)
	{ clearHeaders(name); addHeader(name,value); }

    /**
     * Construct a new authorization line based on credentials and challenge
     * @param username User account name
     * @param password Clear text password for the account
     * @param meth Method to include in the authorization digest
     * @param uri URI to include in the authorization digest
     * @param proxy Set to true to authenticate to a proxy, false to a server
     * @return A new authorization line to be used in a new transaction
     */
    MimeAuthLine* buildAuth(const String& username, const String& password,
	const String& meth, const String& uri, bool proxy = false) const;

    /**
     * Construct a new authorization line based on this answer and original message
     * @param original Origianl outgoing message
     * @return A new authorization line to be used in a new transaction
     */
    MimeAuthLine* buildAuth(const SIPMessage& original) const;

    /**
     * Prepare the message for automatic client transaction authentication.
     * @param username Username for auto authentication
     * @param password Password for auto authentication
     */
    inline void setAutoAuth(const char* username = 0, const char* password = 0)
	{ m_authUser = username; m_authPass = password; }

    /**
     * Retrieve the username to be used for auto authentication
     * @return Username for auto authentication
     */
    inline const String& getAuthUsername() const
	{ return m_authUser; }

    /**
     * Retrieve the password to be used for auto authentication
     * @return Password for auto authentication
     */
    inline const String& getAuthPassword() const
	{ return m_authPass; }

    /**
     * Extract routes from Record-Route: headers
     * @return A list of MimeHeaderLine representing SIP routes
     */
    ObjList* getRoutes() const;

    /**
     * Add Route: headers to an outgoing message
     * @param routes List of MimeHeaderLine representing SIP routes
     */
    void addRoutes(const ObjList* routes);

    /**
     * Creates a binary buffer from a SIPMessage.
     */
    const DataBlock& getBuffer() const;

    /**
     * Creates a text buffer from the headers.
     */
    const String& getHeaders() const;

    /**
     * Set a new body for this message
     */
    void setBody(MimeBody* newbody = 0);

    /**
     * Sip Version
     */
    String version;

    /**
     * This holds the method name of the message.
     */
    String method;

    /**
     * URI of the request
     */
    String uri;

    /**
     * Status code
     */
    int code;

    /**
     * Reason Phrase
     */
    String reason;

    /**
     * All the headers should be in this list.
     */
    ObjList header;

    /**
     * All the body related things should be here, including the entire body and
     * the parsed body.
     */
    MimeBody* body;

protected:
    bool parse(const char* buf, int len);
    bool parseFirst(String& line);
    SIPParty* m_ep;
    bool m_valid;
    bool m_answer;
    bool m_outgoing;
    bool m_ack;
    int m_cseq;
    int m_flags;
    mutable String m_string;
    mutable DataBlock m_data;
    String m_authUser;
    String m_authPass;
private:
    SIPMessage(); // no, thanks
};

/**
 * A class to store information required to identify a dialog
 * @short SIP Dialog object
 */
class YSIP_API SIPDialog : public String
{
public:
    /**
     * Default constructor, build an empty SIP dialog
     */
    SIPDialog();

    /**
     * Copy constructor
     * @param original Original SIP dialog to copy
     */
    SIPDialog(const SIPDialog& original);

    /**
     * Constructor from a SIP message
     * @param message SIP message to copy the dialog information from
     */
    explicit SIPDialog(const SIPMessage& message);

    /**
     * Constructor from a Call ID, leaves URIs and tags empty
     * @param callid Call ID to insert in the dialog
     */
    inline explicit SIPDialog(const String& callid)
	: String(callid)
	{ }

    /**
     * Assignment from another dialog
     * @param original Original SIP dialog to copy
     * @return Reference to this SIP dialog
     */
    SIPDialog& operator=(const SIPDialog& original);

    /**
     * Assignment from a SIP message
     * @param message SIP message to copy the dialog information from
     * @return Reference to this SIP dialog
     */
    SIPDialog& operator=(const SIPMessage& message);

    /**
     * Assignment from a Call ID, URIs and tags are cleared
     * @param callid Call ID to copy to the dialog
     * @return Reference to this SIP dialog
     */
    SIPDialog& operator=(const String& callid);

    /**
     * SIP dialog matching check
     * @param other Other dialog to compare to
     * @param ignoreURIs True to ignore local and remote URIs when comparing
     * @return True if the two dialogs match
     */
    bool matches(const SIPDialog& other, bool ignoreURIs) const;

    /**
     * Dialog equality comparation, suitable for RFC 2543
     * @param other Other dialog to compare to
     * @return True if the two dialogs are equal
     */
    inline bool operator==(const SIPDialog& other) const
	{ return matches(other,false); }

    /**
     * Dialog inequality comparation, suitable for RFC 2543
     * @param other Other dialog to compare to
     * @return True if the two dialogs are different
     */
    inline bool operator!=(const SIPDialog& other) const
	{ return !matches(other,false); }

    /**
     * Dialog equality comparation, suitable for RFC 3261
     * @param other Other dialog to compare to
     * @return True if the two dialogs match (ignoring local and remote URIs)
     */
    inline bool operator&=(const SIPDialog& other) const
	{ return matches(other,true); }

    /**
     * Dialog inequality comparation, suitable for RFC 3261
     * @param other Other dialog to compare to
     * @return True if the two dialogs do not match (ignoring local and remote URIs)
     */
    inline bool operator|=(const SIPDialog& other) const
	{ return !matches(other,true); }

    /**
     * Get the From URI from the dialog
     * @param outgoing True if getting the URI for an outgoing transaction
     * @return Reference to the From URI in dialog
     */
    inline const String& fromURI(bool outgoing) const
	{ return outgoing ? localURI : remoteURI; }

    /**
     * Get the From tag from the dialog
     * @param outgoing True if getting the tag for an outgoing transaction
     * @return Reference to the From URI tag in dialog
     */
    inline const String& fromTag(bool outgoing) const
	{ return outgoing ? localTag : remoteTag; }

    /**
     * Get the To URI from the dialog
     * @param outgoing True if getting the URI for an outgoing transaction
     * @return Reference to the To URI in dialog
     */
    inline const String& toURI(bool outgoing) const
	{ return outgoing ? remoteURI : localURI; }

    /**
     * Get the To tag from the dialog
     * @param outgoing True if getting the tag for an outgoing transaction
     * @return Reference to the To URI tag in dialog
     */
    inline const String& toTag(bool outgoing) const
	{ return outgoing ? remoteTag : localTag; }

    /**
     * Local URI of the dialog
     */
    String localURI;

    /**
     * Tag parameter of the local URI
     */
    String localTag;

    /**
     * Remote URI of the dialog
     */
    String remoteURI;

    /**
     * Tag parameter of the remote URI
     */
    String remoteTag;
};

/**
 * All informaton related to a SIP transaction, starting with 1st message
 * @short A class holding one SIP transaction
 */
class YSIP_API SIPTransaction : public RefObject
{
public:
    /**
     * Current state of the transaction
     */
    enum State {
	// Invalid state - before constructor or after destructor
	Invalid,

	// Initial state - after the initial message was inserted
	Initial,

	// Trying state - got the message but no decision made yet
	Trying,

	// Process state - while locally processing the event
	Process,

	// Retrans state - retransmits latest message until getting ACK
	Retrans,

	// Finish state - transmits the last message on client retransmission
	Finish,

	// Cleared state - removed from engine, awaiting destruction
	Cleared
    };

    /**
     * Possible return values from @ref processMessage()
     */
    enum Processed {
	// Not matched at all
	NoMatch,

	// Belongs to another dialog - probably result of a fork
	NoDialog,

	// Matched to transaction/dialog and processed
	Matched
    };

    /**
     * Constructor from first message
     * @param message A pointer to the initial message, should not be used
     *  afterwards as the transaction takes ownership
     * @param engine A pointer to the SIP engine this transaction belongs
     * @param outgoing True if this transaction is for an outgoing request
     */
    SIPTransaction(SIPMessage* message, SIPEngine* engine, bool outgoing = true);

    /**
     * Copy constructor to be used with forked INVITEs
     * @param original Original transaction that is to be copied
     * @param tag Dialog tag for the new transaction
     */
    SIPTransaction(const SIPTransaction& original, const String& tag);

    /**
     * Destructor - clears all held objects
     */
    virtual ~SIPTransaction();

    /**
     * Get the name of a transaction state
     */
    static const char* stateName(int state);

    /**
     * The current state of the transaction
     * @return The current state as one of the State enums
     */
    inline int getState() const
	{ return m_state; }

    /**
     * Check if the transaction is active for the upper layer
     * @return True if the transaction is active, false if it finished
     */
    inline bool isActive() const
	{ return (Invalid < m_state) && (m_state < Finish); }

    /**
     * The first message that created this transaction
     */
    inline const SIPMessage* initialMessage() const
	{ return m_firstMessage; }

    /**
     * The last message (re)sent by this transaction
     */
    inline const SIPMessage* latestMessage() const
	{ return m_lastMessage; }

    /**
     * The most recent message handled by this transaction
     */
    inline const SIPMessage* recentMessage() const
	{ return m_lastMessage ? m_lastMessage : m_firstMessage; }

    /**
     * The SIPEngine this transaction belongs to
     */
    inline SIPEngine* getEngine() const
	{ return m_engine; }

    /**
     * Check if this transaction was initiated by the remote peer or locally
     * @return True if the transaction was created by an outgoing message
     */
    inline bool isOutgoing() const
	{ return m_outgoing; }

    /**
     * Check if this transaction was initiated locally or by the remote peer
     * @return True if the transaction was created by an incoming message
     */
    inline bool isIncoming() const
	{ return !m_outgoing; }

    /**
     * Check if this transaction is an INVITE transaction or not
     * @return True if the transaction is an INVITE
     */
    inline bool isInvite() const
	{ return m_invite; }

    /**
     * Check if this transaction is handled by a reliable protocol
     * @return True if a reliable protocol (TCP, SCTP) is used
     */
    inline bool isReliable() const
	{ return m_firstMessage ? m_firstMessage->isReliable() : false; }

    /**
     * The SIP method this transaction handles
     */
    inline const String& getMethod() const
	{ return m_firstMessage ? m_firstMessage->method : String::empty(); }

    /**
     * The SIP URI this transaction handles
     */
    inline const String& getURI() const
	{ return m_firstMessage ? m_firstMessage->uri : String::empty(); }

    /**
     * The Via branch that may uniquely identify this transaction
     * @return The branch parameter taken from the Via header
     */
    inline const String& getBranch() const
	{ return m_branch; }

    /**
     * The call ID may identify this transaction
     * @return The Call-ID parameter taken from the message
     */
    inline const String& getCallID() const
	{ return m_callid; }

    /**
     * The dialog tag that may identify this transaction
     * @return The dialog tag parameter
     */
    inline const String& getDialogTag() const
	{ return m_tag; }

    /**
     * Set a new dialog tag, optionally build a random one
     * @param tag New dialog tag, a null string will build a random tag
     */
    void setDialogTag(const char* tag = 0);

    /**
     * Set the (re)transmission flag that allows the latest outgoing message
     *  to be send over the wire
     */
    inline void setTransmit()
	{ m_transmit = true; }


    /**
     * Send back an authentication required response
     * @param realm Authentication realm to send in the answer
     * @param domain Domain for which it will authenticate
     * @param stale True if the previous password is valid but nonce is too old
     * @param proxy True to authenticate as proxy, false as user agent
     */
    void requestAuth(const String& realm, const String& domain, bool stale, bool proxy = false);

    /**
     * Detect the proper credentials for any user in the engine
     * @param user String to store the authenticated user name or user to
     *  look for (if not null on entry)
     * @param proxy True to authenticate as proxy, false as user agent
     * @param userData Pointer to an optional object that is passed back to @ref SIPEngine::checkUser
     * @return Age of the nonce if user matches, negative for a failure
     */
    int authUser(String& user, bool proxy = false, GenObject* userData = 0);

    /**
     * Check if a message belongs to this transaction and process it if so
     * @param message A pointer to the message to check, should not be used
     *  afterwards if this method returned Matched
     * @param branch The branch parameter extracted from first Via header
     * @return Matched if the message was handled by this transaction, in
     *  which case it takes ownership over the message
     */
    virtual Processed processMessage(SIPMessage* message, const String& branch);

    /**
     * Process a message belonging to this transaction
     * @param message A pointer to the message to process, the caller must
     *  make sure it belongs to this transaction
     */
    virtual void processMessage(SIPMessage* message);

    /**
     * Get an event for this transaction if any is available.
     * It provides default handling for invalid states, otherwise calls
     *  the more specific protected version.
     * You may override this method if you need processing of invalid states.
     * @param pendingOnly True to only return outgoing and pending events
     * @return A newly allocated event or NULL if none is needed
     */
    virtual SIPEvent* getEvent(bool pendingOnly = false);

    /**
     * Creates and transmits a final response message
     * @param code Response code to send
     * @param reason Human readable reason text (optional)
     * @return True if the message was queued for transmission
     */
    bool setResponse(int code, const char* reason = 0);

    /**
     * Transmits a final response message
     */
    void setResponse(SIPMessage* message);

    /**
     * Retrieve the latest response code
     * @return Code of most recent response, zero if none is known
     */
    inline int getResponseCode() const
	{ return m_response; }

    /**
     * Set an arbitrary pointer as user specific data
     */
    inline void setUserData(void* data)
	{ m_private = data; }

    /**
     * Return the opaque user data
     */
    inline void* getUserData() const
	{ return m_private; }

protected:
    /**
     * Constructor from previous auto authenticated transaction. This is used only internally
     * @param original Original transaction that failed authentication
     * @param answer SIP answer that creates the new transaction
     */
    SIPTransaction(SIPTransaction& original, SIPMessage* answer);

    /**
     * Pre-destruction notification used to clean up the transaction
     */
    virtual void destroyed();

    /**
     * Attempt to perform automatic client transaction authentication
     * @param answer SIP answer that creates the new transaction
     * @return True if current client processing must be abandoned
     */
    bool tryAutoAuth(SIPMessage* answer);

    /**
     * Get an event only for client transactions
     * @param state The current state of the transaction
     * @param timeout If timeout occured, number of remaining timeouts,
     *  otherwise -1
     * @return A newly allocated event or NULL if none is needed
     */
    virtual SIPEvent* getClientEvent(int state, int timeout);

    /**
     * Get an event only for server transactions.
     * @param state The current state of the transaction
     * @param timeout If timeout occured, number of remaining timeouts,
     *  otherwise -1
     * @return A newly allocated event or NULL if none is needed
     */
    virtual SIPEvent* getServerEvent(int state, int timeout);

    /**
     * Process only the messages for client transactions
     * @param message A pointer to the message to process, should not be used
     *  afterwards if this method returned True
     * @param state The current state of the transaction
     */
    virtual void processClientMessage(SIPMessage* message, int state);

    /**
     * Process only the messages for server transactions
     * @param message A pointer to the message to process, should not be used
     *  afterwards if this method returned True
     * @param state The current state of the transaction
     */
    virtual void processServerMessage(SIPMessage* message, int state);

    /**
     * Change the transaction state
     * @param newstate The desired new state
     * @return True if state change occured
     */
    bool changeState(int newstate);

    /**
     * Set the latest message sent by this transaction
     * @param message Pointer to the latest message
     */
    void setLatestMessage(SIPMessage* message = 0);

    /**
     * Store a pending event to be picked up at the next @ref getEvent() call
     * @param event Event to store
     * @param replace True to replace any existing pending event
     */
    void setPendingEvent(SIPEvent* event = 0, bool replace = false);

    /**
     * Check if there is a pending event waiting
     * @return True is there is a pending event
     */
    inline bool isPendingEvent() const
	{ return (m_pending != 0); }

    /**
     * Set a repetitive timeout
     * @param delay How often (in microseconds) to fire the timeout
     * @param count How many times to keep firing the timeout
     */
    void setTimeout(u_int64_t delay = 0, unsigned int count = 1);

    bool m_outgoing;
    bool m_invite;
    bool m_transmit;
    int m_state;
    int m_response;
    unsigned int m_timeouts;
    u_int64_t m_delay;
    u_int64_t m_timeout;
    SIPMessage* m_firstMessage;
    SIPMessage* m_lastMessage;
    SIPEvent* m_pending;
    SIPEngine* m_engine;
    String m_branch;
    String m_callid;
    String m_tag;
    void *m_private;
};

/**
 * This object is an event that will be taken from SIPEngine
 * @short A SIP event as retrieved from engine
 */ 
class YSIP_API SIPEvent
{
    friend class SIPTransaction;
public:

    SIPEvent()
	: m_message(0), m_transaction(0), m_state(SIPTransaction::Invalid)
	{ }

    SIPEvent(SIPMessage* message, SIPTransaction* transaction = 0);

    ~SIPEvent();

    /**
     * Get the SIP engine this event belongs to, if any
     * @return Pointer to owning SIP engine or NULL
     */
    inline SIPEngine* getEngine() const
	{ return m_transaction ? m_transaction->getEngine() : 0; }

    /**
     * Get the SIP message this event is supposed to handle
     * @return Pointer to SIP message causing the event
     */
    inline SIPMessage* getMessage() const
	{ return m_message; }

    /**
     * Get the SIP transaction that generated the event, if any
     * @return Pointer to owning SIP transaction or NULL
     */
    inline SIPTransaction* getTransaction() const
	{ return m_transaction; }

    /**
     * Check if the message is an outgoing message
     * @return True if the message should be sent to remote
     */
    inline bool isOutgoing() const
	{ return m_message && m_message->isOutgoing(); }

    /**
     * Check if the message is an incoming message
     * @return True if the message is coming from remote
     */
    inline bool isIncoming() const
	{ return m_message && !m_message->isOutgoing(); }

    /**
     * Get the pointer to the endpoint this event uses
     */
    inline SIPParty* getParty() const
	{ return m_message ? m_message->getParty() : 0; }

    /**
     * Return the opaque user data stored in the transaction
     */
    inline void* getUserData() const
	{ return m_transaction ? m_transaction->getUserData() : 0; }

    /**
     * The state of the transaction when the event was generated
     */
    inline int getState() const
	{ return m_state; }

    /**
     * Check if the transaction was active when the event was generated
     * @return True if the transaction was active, false if it finished
     */
    inline bool isActive() const
	{ return (SIPTransaction::Invalid < m_state) && (m_state < SIPTransaction::Finish); }

protected:
    SIPMessage* m_message;
    SIPTransaction* m_transaction;
    int m_state;
};

/**
 * The SIP engine holds common methods and the list of current transactions
 * @short The SIP engine and transaction list
 */
class YSIP_API SIPEngine : public DebugEnabler, public Mutex
{
public:
    /**
     * Create the SIP Engine
     */
    SIPEngine(const char* userAgent = 0);

    /**
     * Destroy the SIP Engine
     */
    virtual ~SIPEngine();

    /**
     * Build a new SIPParty for a message
     * @param message Pointer to the message to build the party
     * @return True on success, false if party could not be built
     */
    virtual bool buildParty(SIPMessage* message) = 0;

    /**
     * Check user credentials for validity
     * @param username User account name
     * @param realm Authentication realm
     * @param nonce Authentication opaque nonce generated by the server
     * @param method Method of the SIP message that is being authenticated
     * @param uri URI of the SIP message that is being authenticated
     * @param response Response computed by the authenticated entity
     * @param message Message that is to be authenticated
     * @param userData Pointer to an optional object passed from @ref authUser
     * @return True if valid user/password, false if verification failed
     */
    virtual bool checkUser(const String& username, const String& realm, const String& nonce,
	const String& method, const String& uri, const String& response,
	const SIPMessage* message, GenObject* userData);

    /**
     * Authenticate a message by other means than user credentials. By default
     *  it calls @ref checkUser with empty user credential fields
     * @param noUser No plausible user credentials were detected so far
     * @param message Message that is to be authenticated
     * @param userData Pointer to an optional object passed from @ref authUser
     * @return True if message is authenticated, false if verification failed
     */
    virtual bool checkAuth(bool noUser, const SIPMessage* message, GenObject* userData);

    /**
     * Detect the proper credentials for any user in the engine
     * @param message Pointer to the message to check
     * @param user String to store the authenticated user name or user to
     *  look for (if not null on entry)
     * @param proxy True to authenticate as proxy, false as user agent
     * @param userData Pointer to an optional object that is passed back to @ref checkUser
     * @return Age of the nonce if user matches, negative for a failure
     */
    int authUser(const SIPMessage* message, String& user, bool proxy = false, GenObject* userData = 0);

    /**
     * Add a message into the transaction list
     * @param ep Party of the received message
     * @param buf A buffer containing the SIP message text
     * @param len The length of the message or -1 to interpret as C string
     * @return Pointer to the transaction or NULL if message was invalid
     */
    SIPTransaction* addMessage(SIPParty* ep, const char* buf, int len = -1);

    /**
     * Add a message into the transaction list
     * This method is thread safe
     * @param message A parsed SIP message to add to the transactions
     * @return Pointer to the transaction or NULL if message was invalid
     */
    SIPTransaction* addMessage(SIPMessage* message);

    /**
     * Get a SIPEvent from the queue. 
     * This method mainly looks into the transaction list and get all kind of 
     * events, like an incoming request (INVITE, REGISTRATION), a timer, an
     * outgoing message.
     * This method is thread safe
     */
    SIPEvent *getEvent();

    /**
     * This method should be called very often to get the events from the list and 
     * to send them to processEvent method.
     * @return True if some events were processed this turn
     */
    bool process();

    /**
     * Default handling for events.
     * This method should be overriden for what you need and at the end you
     * should call this default one
     * This method is thread safe
     */
    virtual void processEvent(SIPEvent *event);

    /**
     * Handle answers that create new dialogs for an outgoing INVITE
     * @param answer The message that creates the INVITE fork
     * @param trans One of the transactions part of the same INVITE
     * @return Pointer to new transaction or NULL if message is ignored
     */
    virtual SIPTransaction* forkInvite(SIPMessage* answer, SIPTransaction* trans);

    /**
     * Get the timeout to be used for transactions involving human interaction.
     * The default implementation returns the proxy INVITE timeout (timer C = 3 minutes)
     *  minus the INVITE response retransmit interval (timer T2 = 4 seconds)
     * @return Duration of the timeout in microseconds
     */
    virtual u_int64_t getUserTimeout() const;

    /**
     * Get the length of a timer
     * @param which A one-character constant that selects which timer to return
     * @param reliable Whether we request the timer value for a reliable protocol
     * @return Duration of the selected timer or 0 if invalid
     */
    u_int64_t getTimer(char which, bool reliable = false) const;

    /**
     * Get the default value of the Max-Forwards header for this engine
     * @return The maximum number of hops the request is allowed to pass
     */
    inline unsigned int getMaxForwards() const
	{ return m_maxForwards; }

    /**
     * Get the User agent for this SIP engine
     */
    inline const String& getUserAgent() const
	{ return m_userAgent; }

    /**
     * Get a CSeq value suitable for use in a new request
     */
    inline int getNextCSeq()
	{ return ++m_cseq; }

    /**
     * Check if the engine is set up for lazy "100 Trying" messages
     * @return True if the first 100 message is to be skipped for non-INVITE
     */
    inline bool lazyTrying() const
	{ return m_lazyTrying; }

    /**
     * Set the lazy "100 Trying" messages flag
     * @param lazy100 True to not send the 1st 100 message for non-INVITE
     */
    inline void lazyTrying(bool lazy100)
	{ m_lazyTrying = lazy100; }

    /**
     * Retrieve various flags for this engine
     * @return Value of flags ORed together
     */
    inline int flags() const
	{ return m_flags; }

    /**
     * Get an authentication nonce
     * @param nonce String reference to fill with the current nonce
     */
    void nonceGet(String& nonce);

    /**
     * Get the age of an authentication nonce
     * @param nonce String nonce to check for validity and age
     * @return Age of the nonce in seconds, negative for invalid
     */
    long nonceAge(const String& nonce);

    /**
     * Build an authentication response
     * @param username User account name
     * @param realm Authentication realm
     * @param passwd Account password
     * @param nonce Authentication opaque nonce generated by the server
     * @param method Method of the SIP message that is being authenticated
     * @param uri URI of the SIP message that is being authenticated
     * @param response String to store the computed response
     */
    static void buildAuth(const String& username, const String& realm, const String& passwd,
	const String& nonce, const String& method, const String& uri, String& response);

    /**
     * Build an authentication response from already hashed components
     * @param hash_a1 MD5 digest of username:realm:password
     * @param nonce Authentication opaque nonce generated by the server
     * @param hash_a2 MD5 digest of method:uri
     * @param response String to store the computed response
     */
    static void buildAuth(const String& hash_a1, const String& nonce, const String& hash_a2,
	String& response);

    /**
     * Check if a method is in the allowed methods list
     * @param method Uppercase name of the method to check
     * @return True if the method should be allowed processing
     */
    bool isAllowed(const char* method) const;

    /**
     * Add a method to the allowed methods list
     * @param method Uppercase name of the method to add
     */
    void addAllowed(const char* method);

    /**
     * Get all the allowed methods
     * @return Comma separated list of allowed methods
     */
    inline const String& getAllowed() const
	{ return m_allowed; }

    /**
     * Remove a transaction from the list without dereferencing it
     * @param transaction Pointer to transaction to remove
     */
    inline void remove(SIPTransaction* transaction)
	{ lock(); m_transList.remove(transaction,false); unlock(); }

    /**
     * Append a transaction to the end of the list
     * @param transaction Pointer to transaction to append
     */
    inline void append(SIPTransaction* transaction)
	{ lock(); m_transList.append(transaction); unlock(); }

    /**
     * Insert a transaction at the start of the list
     * @param transaction Pointer to transaction to insert
     */
    inline void insert(SIPTransaction* transaction)
	{ lock(); m_transList.insert(transaction); unlock(); }

protected:
    /**
     * The list that holds all the SIP transactions.
     */
    ObjList m_transList;

    u_int64_t m_t1;
    u_int64_t m_t4;
    unsigned int m_maxForwards;
    int m_cseq;
    int m_flags;
    bool m_lazyTrying;
    String m_userAgent;
    String m_allowed;
    String m_nonce;
    String m_nonce_secret;
    u_int32_t m_nonce_time;
    Mutex m_nonce_mutex;
};

}

#endif /* __YATESIP_H */

/* vi: set ts=8 sw=4 sts=4 noet: */
