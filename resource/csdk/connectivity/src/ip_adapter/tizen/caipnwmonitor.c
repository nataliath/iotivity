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

#include "caipinterface.h"

#include <sys/types.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <wifi.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include "caipnwmonitor.h"
#include "caadapterutils.h"
#include "logger.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include <coap/utlist.h>

#define TAG "IP_MONITOR"
#define NETLINK_MESSAGE_LENGTH  (4096)
#define IFC_LABEL_LOOP          "lo"
#define IFC_ADDR_LOOP_IPV4      "127.0.0.1"
#define IFC_ADDR_LOOP_IPV6      "::1"

/**
 * Used to storing adapter changes callback interface.
 */
static struct CAIPCBData_t *g_adapterCallbackList = NULL;

/**
 * Create new interface item.
 */
static CAInterface_t *CANewInterfaceItem(int index, char *name, int family,
                                         const char *addr, int flags);

/**
 * Add new network interface in list.
 */
static CAResult_t CAAddInterfaceItem(u_arraylist_t *iflist, int index,
                                     char *name, int family, const char *addr, int flags);

/**
 * Pass the changed network status through the stored callback.
 */
static void CAIPPassNetworkChangesToAdapter(CANetworkStatus_t status);

/**
 * Callback function to received connection state changes.
 */
static void CAWIFIConnectionStateChangedCb(wifi_connection_state_e state, wifi_ap_h ap,
                                           void *userData);

/**
 * Callback function to received device state changes.
 */
static void CAWIFIDeviceStateChangedCb(wifi_device_state_e state, void *userData);

int CAGetPollingInterval(int interval)
{
    return interval;
}

static void CAIPPassNetworkChangesToAdapter(CANetworkStatus_t status)
{
    CAIPCBData_t *cbitem = NULL;
    LL_FOREACH(g_adapterCallbackList, cbitem)
    {
        if (cbitem && cbitem->adapter)
        {
            cbitem->callback(cbitem->adapter, status);
        }
    }
}

CAResult_t CAIPSetNetworkMonitorCallback(CAIPAdapterStateChangeCallback callback,
                                         CATransportAdapter_t adapter)
{
    if (!callback)
    {
        OIC_LOG(ERROR, TAG, "callback is null");
        return CA_STATUS_INVALID_PARAM;
    }

    CAIPCBData_t *cbitem = NULL;
    LL_FOREACH(g_adapterCallbackList, cbitem)
    {
        if (cbitem && adapter == cbitem->adapter && callback == cbitem->callback)
        {
            OIC_LOG(DEBUG, TAG, "this callback is already added");
            return CA_STATUS_OK;
        }
    }

    cbitem = (CAIPCBData_t *)OICCalloc(1, sizeof(*cbitem));
    if (!cbitem)
    {
        OIC_LOG(ERROR, TAG, "Malloc failed");
        return CA_STATUS_FAILED;
    }

    cbitem->adapter = adapter;
    cbitem->callback = callback;
    LL_APPEND(g_adapterCallbackList, cbitem);

    return CA_STATUS_OK;
}

CAResult_t CAIPUnSetNetworkMonitorCallback(CATransportAdapter_t adapter)
{
    CAIPCBData_t *cbitem = NULL;
    CAIPCBData_t *tmpCbitem = NULL;
    LL_FOREACH_SAFE(g_adapterCallbackList, cbitem, tmpCbitem)
    {
        if (cbitem && adapter == cbitem->adapter)
        {
            OIC_LOG(DEBUG, TAG, "remove specific callback");
            LL_DELETE(g_adapterCallbackList, cbitem);
            OICFree(cbitem);
            return CA_STATUS_OK;
        }
    }
    return CA_STATUS_OK;
}

u_arraylist_t *CAFindInterfaceChange()
{
    u_arraylist_t *iflist = NULL;
    char buf[NETLINK_MESSAGE_LENGTH] = { 0 };
    struct sockaddr_nl sa = { 0 };
    struct iovec iov = { .iov_base = buf,
                         .iov_len = sizeof (buf) };
    struct msghdr msg = { .msg_name = (void *)&sa,
                          .msg_namelen = sizeof (sa),
                          .msg_iov = &iov,
                          .msg_iovlen = 1 };

    ssize_t len = recvmsg(caglobals.ip.netlinkFd, &msg, 0);

    for (struct nlmsghdr *nh = (struct nlmsghdr *)buf; NLMSG_OK(nh, len); nh = NLMSG_NEXT(nh, len))
    {
        if (nh != NULL && nh->nlmsg_type != RTM_NEWLINK)
        {
            continue;
        }
        struct ifinfomsg *ifi = (struct ifinfomsg *)NLMSG_DATA(nh);

        if ((!ifi || (ifi->ifi_flags & IFF_LOOPBACK) || !(ifi->ifi_flags & IFF_RUNNING)))
        {
            continue;
        }

        int ifiIndex = ifi->ifi_index;

        iflist = CAIPGetInterfaceInformation(ifiIndex);

        if (!iflist)
        {
            OIC_LOG_V(ERROR, TAG, "get interface info failed: %s", strerror(errno));
            return NULL;
        }
    }
    return iflist;
}

CAResult_t CAIPStartNetworkMonitor(CAIPAdapterStateChangeCallback callback,
                                   CATransportAdapter_t adapter)
{
    OIC_LOG(DEBUG, TAG, "IN");

    if (!g_adapterCallbackList)
    {
        // Initialize Wifi service
       wifi_error_e ret = wifi_initialize();
       if (WIFI_ERROR_NONE != ret)
       {
           OIC_LOG(ERROR, TAG, "wifi_initialize failed");
           return CA_STATUS_FAILED;
       }

       // Set callback for receiving state changes
       ret = wifi_set_device_state_changed_cb(CAWIFIDeviceStateChangedCb, NULL);
       if (WIFI_ERROR_NONE != ret)
       {
           OIC_LOG(ERROR, TAG, "wifi_set_device_state_changed_cb failed");
           return CA_STATUS_FAILED;
       }

       // Set callback for receiving connection state changes
       ret = wifi_set_connection_state_changed_cb(CAWIFIConnectionStateChangedCb, NULL);
       if (WIFI_ERROR_NONE != ret)
       {
           OIC_LOG(ERROR, TAG, "wifi_set_connection_state_changed_cb failed");
           return CA_STATUS_FAILED;
       }
    }

    return CAIPSetNetworkMonitorCallback(callback, adapter);
}

CAResult_t CAIPStopNetworkMonitor(CATransportAdapter_t adapter)
{
    OIC_LOG(DEBUG, TAG, "IN");

    CAIPUnSetNetworkMonitorCallback(adapter);
    if (!g_adapterCallbackList)
    {
        // Reset callback for receiving state changes
       wifi_error_e ret = wifi_unset_device_state_changed_cb();
       if (WIFI_ERROR_NONE != ret)
       {
           OIC_LOG(ERROR, TAG, "wifi_unset_device_state_changed_cb failed");
       }

       // Reset callback for receiving connection state changes
       ret = wifi_unset_connection_state_changed_cb();
       if (WIFI_ERROR_NONE != ret)
       {
           OIC_LOG(ERROR, TAG, "wifi_unset_connection_state_changed_cb failed");
       }

       // Deinitialize Wifi service
       ret = wifi_deinitialize();
       if (WIFI_ERROR_NONE != ret)
       {
           OIC_LOG(ERROR, TAG, "wifi_deinitialize failed");
       }
    }

    return CA_STATUS_OK;
}

/**
 * Used to send netlink query to kernel and recv response from kernel.
 *
 * @param[in]   idx       desired network interface index, 0 means all interfaces.
 * @param[out]  iflist    linked list.
 *
 */
static bool CAIPGetAddrInfo(int idx, u_arraylist_t *iflist)
{
    if ((idx < 0) || (iflist == NULL))
    {
        return false;
    }

    struct ifaddrs *ifp = NULL;
    if (-1 == getifaddrs(&ifp))
    {
        OIC_LOG_V(ERROR, TAG, "Failed to get ifaddrs: %s", strerror(errno));
        return false;
    }

    struct ifaddrs *ifa = NULL;
    for (ifa = ifp; ifa; ifa = ifa->ifa_next)
    {
        if (!ifa->ifa_addr)
        {
            continue;
        }

        int family = ifa->ifa_addr->sa_family;
        if ((ifa->ifa_flags & IFF_LOOPBACK) || (AF_INET != family && AF_INET6 != family))
        {
            continue;
        }

        int ifindex = if_nametoindex(ifa->ifa_name);
        if (idx && (ifindex != idx))
        {
            continue;
        }

        char ipaddr[MAX_ADDR_STR_SIZE_CA] = {0};
        if (family == AF_INET6)
        {
            struct sockaddr_in6 *in6 = (struct sockaddr_in6*) ifa->ifa_addr;
            inet_ntop(family, (void *)&(in6->sin6_addr), ipaddr, sizeof(ipaddr));
        }
        else if (family == AF_INET)
        {
            struct sockaddr_in *in = (struct sockaddr_in*) ifa->ifa_addr;
            inet_ntop(family, (void *)&(in->sin_addr), ipaddr, sizeof(ipaddr));
        }

        if ((strcmp(ipaddr, IFC_ADDR_LOOP_IPV4) == 0) ||
            (strcmp(ipaddr, IFC_ADDR_LOOP_IPV6) == 0) ||
            (strcmp(ifa->ifa_name, IFC_LABEL_LOOP) == 0))
        {
            OIC_LOG(DEBUG, TAG, "LOOPBACK continue!!!");
            continue;
        }

        CAResult_t result = CAAddInterfaceItem(iflist, ifindex,
                                               ifa->ifa_name, family,
                                               ipaddr, ifa->ifa_flags);
        if (CA_STATUS_OK != result)
        {
            OIC_LOG(ERROR, TAG, "CAAddInterfaceItem fail");
            goto exit;
        }
    }
    freeifaddrs(ifp);
    return true;

exit:
    freeifaddrs(ifp);
    return false;
}

u_arraylist_t *CAIPGetInterfaceInformation(int desiredIndex)
{
    u_arraylist_t *iflist = u_arraylist_create();
    if (!iflist)
    {
        OIC_LOG_V(ERROR, TAG, "Failed to create iflist: %s", strerror(errno));
        return NULL;
    }

    if (!CAIPGetAddrInfo(desiredIndex, iflist))
    {
        goto exit;
    }

    return iflist;

exit:
    u_arraylist_destroy(iflist);
    return NULL;
}

static CAResult_t CAAddInterfaceItem(u_arraylist_t *iflist, int index,
                                     char *name, int family, const char *addr, int flags)
{
    CAInterface_t *ifitem = CANewInterfaceItem(index, name, family, addr, flags);
    if (!ifitem)
    {
        return CA_STATUS_FAILED;
    }
    bool result = u_arraylist_add(iflist, ifitem);
    if (!result)
    {
        OIC_LOG(ERROR, TAG, "u_arraylist_add failed.");
        OICFree(ifitem);
        return CA_STATUS_FAILED;
    }

    return CA_STATUS_OK;
}

static CAInterface_t *CANewInterfaceItem(int index, char *name, int family,
                                         const char *addr, int flags)
{
    CAInterface_t *ifitem = (CAInterface_t *)OICCalloc(1, sizeof (CAInterface_t));
    if (!ifitem)
    {
        OIC_LOG(ERROR, TAG, "Malloc failed");
        return NULL;
    }

    OICStrcpy(ifitem->name, INTERFACE_NAME_MAX, name);
    ifitem->index = index;
    ifitem->family = family;
    OICStrcpy(ifitem->addr, sizeof(ifitem->addr), addr);
    ifitem->flags = flags;

    return ifitem;
}

void CAWIFIConnectionStateChangedCb(wifi_connection_state_e state, wifi_ap_h ap,
                                    void *userData)
{
    OIC_LOG(DEBUG, TAG, "IN");

    if (WIFI_CONNECTION_STATE_ASSOCIATION == state
        || WIFI_CONNECTION_STATE_CONFIGURATION == state)
    {
        OIC_LOG(DEBUG, TAG, "Connection is in Association State");
        return;
    }

    if (WIFI_CONNECTION_STATE_CONNECTED == state)
    {
        CAIPPassNetworkChangesToAdapter(CA_INTERFACE_UP);
    }
    else
    {
        CAIPPassNetworkChangesToAdapter(CA_INTERFACE_DOWN);
    }

    OIC_LOG(DEBUG, TAG, "OUT");
}

void CAWIFIDeviceStateChangedCb(wifi_device_state_e state, void *userData)
{
    OIC_LOG(DEBUG, TAG, "IN");

    if (WIFI_DEVICE_STATE_ACTIVATED == state)
    {
        OIC_LOG(DEBUG, TAG, "Wifi is in Activated State");
    }
    else
    {
        CAWIFIConnectionStateChangedCb(WIFI_CONNECTION_STATE_DISCONNECTED, NULL, NULL);
        OIC_LOG(DEBUG, TAG, "Wifi is in Deactivated State");
    }

    OIC_LOG(DEBUG, TAG, "OUT");
}
