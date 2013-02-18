/**
 * @file sys_network.h
 * Low-level network socket routines. @ingroup network
 *
 * @authors Copyright © 2003-2013 Jaakko Keränen <jaakko.keranen@iki.fi>
 * @authors Copyright © 2009-2013 Daniel Swanson <danij@dengine.net>
 *
 * @par License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * <small>This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version. This program is distributed in the hope that it
 * will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details. You should have received a copy of the GNU
 * General Public License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA</small>
 */

#ifndef LIBDENG_SYSTEM_NETWORK_H
#define LIBDENG_SYSTEM_NETWORK_H

#include "dd_share.h"
#include "net_buf.h"
#include "monitor.h"

#define DEFAULT_TCP_PORT    13209
#define DEFAULT_UDP_PORT    13209

typedef void (*expectedresponder_t)(int, const byte*, int);

// If a master action fails, the action queue is emptied.
typedef enum {
    MAC_REQUEST, // Retrieve the list of servers from the master.
    MAC_WAIT, // Wait for the server list to arrive.
    MAC_LIST // Print the server list in the console.
} masteraction_t;

#ifdef __cplusplus
extern "C" {
#endif

extern boolean  allowSending;
extern int      maxQueuePackets;

extern char    *nptIPAddress;
extern int      nptIPPort;

extern char    *serverName, *serverInfo, *playerName;
extern int      serverData[];

extern char    *masterAddress;
extern int      masterPort;
extern char    *masterPath;

void            N_Register(void);
void            N_SystemInit(void);
void            N_SystemShutdown(void);
void            N_PrintInfo(void);
void            N_Listen(void);
void            N_ListenNodes(void);

#ifdef __CLIENT__
boolean         N_LookForHosts(const char *address, int port, expectedresponder_t responder);
void            N_ClientHandleResponseToInfoQuery(int nodeId, const byte *data, int size);
boolean         N_Connect(void);
boolean         N_Disconnect(void);
#endif

#ifdef __SERVER__
boolean         N_ServerOpen(void);
boolean         N_ServerClose(void);
#endif

void            N_TerminateNode(nodeid_t id);

int             N_GetNodeSocket(nodeid_t id);
boolean         N_HasNodeJoined(nodeid_t id);
//boolean         N_GetNodeName(nodeid_t id, char *name);

int             N_GetHostCount(void);
boolean         N_GetHostInfo(int index, struct serverinfo_s *info);

void            N_PrintNetworkStatus(void);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* LIBDENG_SYSTEM_NETWORK_H */
