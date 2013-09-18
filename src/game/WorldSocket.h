/*
 * This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef WORLD_SOCKET_H
#define WORLD_SOCKET_H

#include <ace/Time_Value.h>
#include <boost/thread/recursive_mutex.hpp>
#include <boost/thread/lock_guard.hpp>
#include "Common.h"
#include "Auth/AuthCrypt.h"
#include "Auth/BigNumber.h"
#include "Network/Socket.h"

#if !defined (ACE_LACKS_PRAGMA_ONCE)
#pragma once
#endif /* ACE_LACKS_PRAGMA_ONCE */

class WorldPacket;
class WorldSession;
class NetworkThread;
class WorldSocketMgr;

/**
 * WorldSocket.
 *
 * This class is responsible for the communication with
 * remote clients.
 * Most methods return -1 on failure.
 * The class uses reference counting.
 *
 * For output the class uses one buffer (64K usually) and
 * a queue where it stores packet if there is no place on
 * the queue. The reason this is done, is because the server
 * does really a lot of small-size writes to it, and it doesn't
 * scale well to allocate memory for every. When something is
 * written to the output buffer the socket is not immediately
 * activated for output (again for the same reason), there
 * is 10ms celling (thats why there is Update() override method).
 * This concept is similar to TCP_CORK, but TCP_CORK
 * uses 200ms celling. As result overhead generated by
 * sending packets from "producer" threads is minimal,
 * and doing a lot of writes with small size is tolerated.
 *
 * The calls to Update () method are managed by WorldSocketMgr
 * and ReactorRunnable.
 *
 * For input ,the class uses one 1024 bytes buffer on stack
 * to which it does recv() calls. And then received data is
 * distributed where its needed. 1024 matches pretty well the
 * traffic generated by client for now.
 *
 * The input/output do speculative reads/writes (AKA it tryes
 * to read all data available in the kernel buffer or tryes to
 * write everything available in userspace buffer),
 * which is ok for using with Level and Edge Triggered IO
 * notification.
 *
 */
class WorldSocket : public Socket
{
public:
    virtual void CloseSocket(void) override;

    /// Send A packet on the socket, this function is reentrant.
    /// @param pct packet to send
    /// @return false of failure
    bool SendPacket(const WorldPacket& pct);

    /// Return the session key
    BigNumber& GetSessionKey() { return m_s; }

    WorldSocket(NetworkManager& manager, NetworkThread& owner);

    virtual ~WorldSocket(void);

protected:
    virtual bool Open() override;
    virtual bool ProcessIncomingData() override;

private:
    /// Helper functions for processing incoming data.
    int handle_input_header(void);
    int handle_input_payload(void);

    /// process one incoming packet.
    /// @param new_pct received packet ,note that you need to delete it.
    int ProcessIncoming(WorldPacket* new_pct);

    /// Called by ProcessIncoming() on CMSG_AUTH_SESSION.
    int HandleAuthSession(WorldPacket& recvPacket);

    /// Called by ProcessIncoming() on CMSG_PING.
    int HandlePing(WorldPacket& recvPacket);

    /// Time in which the last ping was received
    ACE_Time_Value m_LastPingTime;

    /// Keep track of over-speed pings ,to prevent ping flood.
    uint32 m_OverSpeedPings;

    /// Class used for managing encryption of the headers
    AuthCrypt m_Crypt;

    /// Mutex lock to protect m_Session
    LockType m_SessionLock;

    /// Session to which received packets are routed
    WorldSession* m_Session;

    /// here are stored the fragments of the received data
    WorldPacket* m_RecvWPct;

    /// This block actually refers to m_RecvWPct contents,
    /// which allows easy and safe writing to it.
    /// It wont free memory when its deleted. m_RecvWPct takes care of freeing.
    NetworkBuffer m_RecvPct;

    /// Fragment of the received header.
    NetworkBuffer m_Header;

    uint32 m_Seed;

    BigNumber m_s;
};

typedef boost::shared_ptr<WorldSocket> WorldSocketPtr;

#endif // WORLD_SOCKET_H