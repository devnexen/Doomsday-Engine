/*
 * The Doomsday Engine Project -- libdeng2
 *
 * Copyright (c) 2009, 2010 Jaakko Keränen <jaakko.keranen@iki.fi>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIBDENG2_LINK_H
#define LIBDENG2_LINK_H

#include "../Transceiver"
#include "../Observers"

#include <QFlags>
#include <QAbstractSocket>

namespace de
{   
    class Address;
    class Packet;
    class Socket;
    
    /**
     * Network communications link.
     *
     * @ingroup net
     */
    class LIBDENG2_API Link : public QObject, public Transceiver
    {
        Q_OBJECT

    public:
        /// The remote end has closed the link. @ingroup errors
        DEFINE_ERROR(DisconnectedError);
        
        enum Flag
        {
            /// Sending on channel 1 instead of the default 0.
            Channel1 = 0x1
        };
        Q_DECLARE_FLAGS(Flags, Flag);
               
    public:
        /**
         * Constructs a new communications link. A new socket is created for the link.
         *
         * @param address  Address to connect to.
         */
        Link(const Address& address);
        
        /**
         * Constructs a new communications link.
         *
         * @param socket  Socket for the link. Link gets ownership.
         */
        Link(Socket* socket);
        
        virtual ~Link();

        /**
         * Checks if any incoming data has been received.
         */
        bool hasIncoming() const;
        
        /**
         * Wait until all data has been sent.
         */
        void flush();

        /**
         * Returns the socket over which the Link communicates.
         *
         * @return  Socket.
         */ 
        Socket& socket() { return *_socket; }
        
        /**
         * Returns the address of the remote end of the link.
         */ 
        Address peerAddress() const;
        
        // Implements Transceiver.
        void send(const IByteArray& data);
        Message* receive();

    signals:
        void messagesReady();

    protected slots:
        void socketDisconnected();
        void socketError(QAbstractSocket::SocketError error);

    protected:
        void initialize();

    public:
        DEFINE_AUDIENCE(Deletion, void linkBeingDeleted(Link& link));

        /// Mode flags.
        Flags mode;
    
    private:
        /// Socket over which the link communicates.
        Socket* _socket; 
    };
}

Q_DECLARE_OPERATORS_FOR_FLAGS(de::Link::Flags);

#endif /* LIBDENG2_LINK_H */
