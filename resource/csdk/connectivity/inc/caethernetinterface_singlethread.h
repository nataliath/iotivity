/******************************************************************
*
* Copyright 2014 Samsung Electronics All Rights Reserved.
*
*
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*
******************************************************************/

/**
 * @file caethernetinterface_singlethread.h
 * @brief This file provides APIs ethernet client/server/network monitor modules
 */

#ifndef _CA_ETHERNET_INTERFACE_SINGLETHREAD_H_
#define _CA_ETHERNET_INTERFACE_SINGLETHREAD_H_

#include <stdbool.h>

#include "cacommon.h"

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @enum CAAdapterServerType_t
 * @brief Enum for defining different server types.
 */
typedef enum
{
    CA_UNICAST_SERVER = 0,      /**< Unicast Server */
    CA_MULTICAST_SERVER,        /**< Multicast Server */
    CA_SECURED_UNICAST_SERVER   /**< Secured Unicast Server */
} CAAdapterServerType_t;

/**
 * @brief Callback to be notified on reception of any data from remote OIC devices.
 * @param  ipAddress    [IN] IP address of remote OIC device.
 * @param  port         [IN] Port number on which data is received.
 * @param  data         [IN] Data received from remote OIC device.
 * @param  dataLength   [IN] Length of data in bytes.
 * @return NONE
 * @pre  Callback must be registered using CAEthernetSetPacketReceiveCallback()
 */
typedef void (*CAEthernetPacketReceivedCallback)(const char *ipAddress, uint32_t port,
                                                 const void *data, uint32_t dataLength);

/**
 * @brief  Callback to be notified when exception occures on multicast/unicast server.
 * @param  type  [IN] Type of server(#CAAdapterServerType_t)
 * @return NONE
 * @pre  Callback must be registered using CAEthernetSetExceptionCallback()
 */
typedef void (*CAEthernetExceptionCallback)(CAAdapterServerType_t type);

/**
 * @brief  Initialize Ethernet server
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_INVALID_PARAM Invalid input data
 * @retval  #CA_STATUS_FAILED Initialization failed
 */
CAResult_t CAEthernetInitializeServer(void);

/**
 * @brief  Terminate Ethernet server
 * @return NONE
 */
void CAEthernetTerminateServer(void);

/**
 * @brief  Start multicast server for specified multicast address and port
 *
 * @param   localAddress        [IN]      Local adapter address to which server to be binded.
 * @param   multicastAddress    [IN]      Multicast group address.
 * @param   multicastPort       [IN,OUT]  Port number on which server will be running. If binding
                                          the port failed, server starts in the next available port.
 * @param   serverFD            [OUT]     Multicast server socket FD.
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_INVALID_PARAM Invalid input data
 * @retval  #CA_SERVER_STARTED_ALREADY Multicast server is already started and running.
 * @retval  #CA_STATUS_FAILED Operation failed
 */
CAResult_t CAEthernetStartMulticastServer(const char *localAddress, const char *multicastAddress,
                                          uint16_t multicastPort, int *serverFD);

/**
 * @brief  Start unicast server for specified local address and port
 *
 * @param  localAddress [IN]      Local adapter address to which server to be binded.
 * @param  port         [IN,OUT]  Port number on which server will be running. If binding
                                  the port failed, server starts in the next available port.
 * @param  forceStart   [IN]      Indicate whether to start server forcesfully on specified port
 *                                or not.
 * @param  serverFD     [OUT]     Unicast server socket FD.
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_INVALID_PARAM Invalid input data
 * @retval  #CA_SERVER_STARTED_ALREADY Unicast server is already started and running.
 * @retval  #CA_STATUS_FAILED Operation failed
 */
CAResult_t CAEthernetStartUnicastServer(const char *localAddress, uint16_t *port,
                                        bool forceStart, int *serverFD);

/**
 * @brief  Stop multicast server.
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_FAILED Operation failed
 */
CAResult_t CAEthernetStopMulticastServer(void);

/**
 * @brief  Stop unicast server.
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_FAILED Operation failed
 */
CAResult_t CAEthernetStopUnicastServer();

/**
 * @brief  Stop secured unicast server.
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_FAILED Operation failed
 */
CAResult_t CAEthernetStopSecureUnicastServer();

/**
 * @brief  Get the Unicast Server Information if it is started
 * @param  ipAddress    [OUT] IP address on which server is binded and running.
 * @param  port         [OUT]Port number on which server is running
 * @param  serverFD     [OUT]Server socket fd.
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_INVALID_PARAM Invalid input data
 * @retval  #CA_STATUS_FAILED Operation failed
 * @remarks  ipAddress must be freed using free().
 */
CAResult_t CAEthernetGetUnicastServerInfo(char **ipAddress, int16_t *port, int *serverFD);

/**
 * @brief  Set this callback for receiving data packets from peer devices.
 * @param  callback   [IN] Callback to be notified on reception of unicast/multicast data packets.
 *
 * @return  NONE
 */
void CAEthernetSetPacketReceiveCallback(CAEthernetPacketReceivedCallback callback);

/**
 * @brief  Pull the Received Data
 * @return NONE
 */
void CAEthernetPullData();

/**
 * @brief  Set this callback for receiving exception notifications.
 *
 * @param  callback [IN] Callback to be notified on occurance of exception on running servers.
 *
 * @return  NONE
 */
void CAEthernetSetExceptionCallback(CAEthernetExceptionCallback callback);

/**
 * @brief  Set socket description for sending unicast UDP data. Once the Unicast server is started,
 *         the same socket descriptor is used for sending the Unicast UDP data.
 *
 * @param  socketFD [IN]  Socket descriptor used for sending UDP data.
 * @return  NONE
 */
void CAEthernetSetUnicastSocket(int socketFD);

/**
 * @brief  Set the port number for sending unicast UDP data
 * @param  port [IN] Port number used for sending UDP data.
 * @return NONE
 */
void CAEthernetSetUnicastPort(uint16_t port);

/**
 * @brief  Set socket description for sending secured (encrypted) unicast UDP data
 *
 * @param socketFD [IN] Socket descriptor used for sending secured (encrypted) UDP data.
 * @return  NONE
 */
void CAEthernetSetSecureUnicastSocket(int socketFD);

/**
 * @brief  API to send unicast UDP data
 *
 * @param  remoteAddress    [IN] IP address to which data needs to be sent.
 * @param  port             [IN] Port to which data needs to be send.
 * @param  buf              [IN] Data to be send.
 * @param  bufLen           [IN] Length of data in bytes
 * @param  isMulticast      [IN] Whether data needs to be sent to multicast ip
 *
 * @return  The number of bytes sent on the network. Returns 0 on error.
 */
uint32_t CAEthernetSendData(const char *remoteAddress, uint16_t port,
                            const char *buf, uint32_t bufLen, bool isMulticast);

/**
 * @brief  Callback to be notified when ethernet adapter connection state changes.
 *
 * @param  ipAddress    [IN] IP address of remote OIC device.
 * @param  status       [IN] Connection status either #CA_INTERFACE_UP or #CA_INTERFACE_DOWN.
 * @return  NONE
 * @pre  Callback must be registered using CAEthernetSetConnectionStateChangeCallback()
 */
typedef void (*CAEthernetConnectionStateChangeCallback)(const char *ipAddress,
                                                        CANetworkStatus_t status);

/**
 * @brief Initialize Ethernet network monitor
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_INVALID_PARAM Invalid input data
 * @retval  #CA_STATUS_FAILED Initialization failed
 */
CAResult_t CAEthernetInitializeNetworkMonitor(void);

/**
 * @brief Terminate Ethernet network monitor
 * @return  NONE
 */
void CAEthernetTerminateNetworkMonitor(void);

/**
 * @brief  Start network monitoring process.
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_FAILED Operation failed
 */
CAResult_t CAEthernetStartNetworkMonitor(void);

/**
 * @brief  Stop network monitoring process.
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_FAILED Operation failed
 */
CAResult_t CAEthernetStopNetworkMonitor(void);

/**
 * @brief  Get local adapter network information.
 *
 * @param  interfaceName [OUT] Local adapter interface name
 * @param  ipAddress     [OUT] IP address
 *
 * @return  #CA_STATUS_OK or Appropriate error code
 * @retval  #CA_STATUS_OK  Successful
 * @retval  #CA_STATUS_INVALID_PARAM Invalid input data
 * @retval  #CA_STATUS_FAILED Operation failed
 * @remarks  interfaceName and ipAddress must be freed using free().
 */
CAResult_t CAEthernetGetInterfaceInfo(char **interfaceName, char **ipAddress);

/**
 * @brief  Get Ethernet adapter connection state.
 *
 * @return  True if Ethernet adapter is connected, otherwise false
 */
bool CAEthernetIsConnected(void);

/**
 * @brief  Set callback for receiving local ethernet adapter connection status.
 *
 * @param  callback [IN] Callback to be notified when local Ethernet adapter connection state
 *                       changes.
 * @return NONE
 */
void CAEthernetSetConnectionStateChangeCallback
                (CAEthernetConnectionStateChangeCallback callback);

#ifdef __cplusplus
}
#endif

#endif //_CA_ETHERNET_INTERFACE_SINGLETHREAD_H_

