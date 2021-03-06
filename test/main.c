/*
 *  Network Client Library
 *
* Copyright 2012  Samsung Electronics Co., Ltd

* Licensed under the Flora License, Version 1.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at

* http://www.tizenopensource.org/license

* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
 *
 */

 
#ifdef HAVE_CONFIG_H 
#include <config.h> 
#endif 
 
#include <stdio.h> 
#include <errno.h> 
#include <stdlib.h> 
#include <string.h> 
#include <glib.h>

#include <dbus/dbus.h> 

#include <network-pm-config.h>
#include <network-cm-intf.h>
#include <network-pm-intf.h>
#include <network-wifi-intf.h>

#define debug_print(format, args...) printf("[%s][Ln: %d] " format, __FILE__, __LINE__, ##args)

#define Convert_time2double(timevar) ((double)timevar.tv_sec+(double)timevar.tv_usec/1000000.0)

#define	MAIN_MEMFREE(x)	do {if (x != NULL) free(x); x = NULL;} while(0)

typedef enum {
	PROFILE_FULL_INFO = 0x01,
	PROFILE_PARTIAL_INFO,
	PROFILE_BASIC_INFO
} profile_print_type_t;

double start_time = 0;
double finish_time = 0;
double async_time = 0;
struct timeval timevar;

GMainLoop *mainloop = NULL;

static void __print_profile_list(int num_of_profile,
		net_profile_info_t* profile_table, profile_print_type_t print_type);
static void __network_print_profile(net_profile_info_t* ProfInfo, profile_print_type_t print_type);
static void __network_print_ipaddress(net_addr_t* ip_address);

static void __network_evt_cb (net_event_info_t*  event_cb, void* user_data);

static void __network_print_ipaddress(net_addr_t* ip_address)
{
	unsigned char *ipaddr;
	ipaddr = (unsigned char *)&ip_address->Data.Ipv4.s_addr;
	debug_print("Profile IP Address = [%d.%d.%d.%d]\n",
			ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
}

static void __network_print_profile(net_profile_info_t* ProfInfo, profile_print_type_t print_type)
{
	net_pdp_profile_info_t *pdp_info = &ProfInfo->ProfileInfo.Pdp;
	net_wifi_profile_info_t *wlan_info = &ProfInfo->ProfileInfo.Wlan;
	net_eth_profile_info_t *eth_info = &ProfInfo->ProfileInfo.Ethernet;

	int di = 0;
	unsigned char *ipaddr;
	unsigned char *netmaskaddr;
	unsigned char *gwaddr;
	net_dev_info_t *net_info;

	if (ProfInfo->profile_type == NET_DEVICE_WIFI) {
		ipaddr = (unsigned char *)&wlan_info->net_info.IpAddr.Data.Ipv4.s_addr;
		netmaskaddr = (unsigned char *)&wlan_info->net_info.SubnetMask.Data.Ipv4.s_addr;
		gwaddr = (unsigned char *)&wlan_info->net_info.GatewayAddr.Data.Ipv4.s_addr;
		net_info = &(wlan_info->net_info);
	} else if (ProfInfo->profile_type == NET_DEVICE_CELLULAR) {
		ipaddr = (unsigned char *)&pdp_info->net_info.IpAddr.Data.Ipv4.s_addr;
		netmaskaddr = (unsigned char *)&pdp_info->net_info.SubnetMask.Data.Ipv4.s_addr;
		gwaddr = (unsigned char *)&pdp_info->net_info.GatewayAddr.Data.Ipv4.s_addr;
		net_info = &(pdp_info->net_info);
	} else if (ProfInfo->profile_type == NET_DEVICE_ETHERNET) {
		ipaddr = (unsigned char *)&eth_info->net_info.IpAddr.Data.Ipv4.s_addr;
		netmaskaddr = (unsigned char *)&eth_info->net_info.SubnetMask.Data.Ipv4.s_addr;
		gwaddr = (unsigned char *)&eth_info->net_info.GatewayAddr.Data.Ipv4.s_addr;
		net_info = &(eth_info->net_info);
	} else {
		debug_print("Error!!! Invalid profile type\n");
		return ;
	}

	debug_print("Profile Name = [%s]\n", ProfInfo->ProfileName);
	
	if (ProfInfo->ProfileState == NET_STATE_TYPE_IDLE)
		debug_print("Profile State = [idle]\n");
	else if (ProfInfo->ProfileState == NET_STATE_TYPE_FAILURE)
		debug_print("Profile State = [failure]\n");
	else if (ProfInfo->ProfileState == NET_STATE_TYPE_ASSOCIATION)
		debug_print("Profile State = [association]\n");
	else if (ProfInfo->ProfileState == NET_STATE_TYPE_CONFIGURATION)
		debug_print("Profile State = [configuration]\n");
	else if (ProfInfo->ProfileState == NET_STATE_TYPE_READY)
		debug_print("Profile State = [ready]\n");
	else if (ProfInfo->ProfileState == NET_STATE_TYPE_ONLINE)
		debug_print("Profile State = [online]\n");
	else if (ProfInfo->ProfileState == NET_STATE_TYPE_DISCONNECT)
		debug_print("Profile State = [disconnect]\n");
	else 
		debug_print("Profile State = [unknown]\n");

	if (ProfInfo->profile_type == NET_DEVICE_WIFI) {
		debug_print("Profile Type = [wifi]\n");
		debug_print("Profile ESSID = [%s]\n", wlan_info->essid);
		debug_print("Profile BSSID = [%s]\n", wlan_info->bssid);

		if (print_type == PROFILE_PARTIAL_INFO ||
		    print_type == PROFILE_FULL_INFO) {
			debug_print("Profile Strength = [%d]\n", (int)wlan_info->Strength);
			debug_print("Profile Frequency = [%d]\n", (int)wlan_info->frequency);
			debug_print("Profile Max Rate = [%d]\n", (int)wlan_info->max_rate);
			debug_print("Profile Passphrase Required = [%d]\n",
					(int)wlan_info->PassphraseRequired);

			if (wlan_info->wlan_mode == NETPM_WLAN_CONNMODE_INFRA)
				debug_print("Profile Wlan mode = [NETPM_WLAN_CONNMODE_INFRA]\n");
			else if (wlan_info->wlan_mode == NETPM_WLAN_CONNMODE_ADHOC)
				debug_print("Profile Wlan mode = [NETPM_WLAN_CONNMODE_ADHOC]\n");
			else
				debug_print("Profile Wlan mode = [NETPM_WLAN_CONNMODE_AUTO]\n");

			debug_print("Profile Security mode = [%d]\n",
					(int)wlan_info->security_info.sec_mode);
			debug_print("Profile Encryption mode = [%d]\n",
					(int)wlan_info->security_info.enc_mode);
			debug_print("Profile Security key = [%s]\n",
					wlan_info->security_info.authentication.psk.pskKey);
			debug_print("Profile WPS support = [%d]\n",
					(int)wlan_info->security_info.wps_support);
		}
	} else if (ProfInfo->profile_type == NET_DEVICE_CELLULAR) {
		debug_print("Profile Type = [pdp]\n");
		
		if (print_type == PROFILE_PARTIAL_INFO ||
		    print_type == PROFILE_FULL_INFO) {

			if (pdp_info->ProtocolType == NET_PDP_TYPE_GPRS)
				debug_print("Profile Protocol Type = [GPRS]\n");
			else if (pdp_info->ProtocolType == NET_PDP_TYPE_EDGE)
				debug_print("Profile Protocol Type = [EDGE]\n");
			else if (pdp_info->ProtocolType == NET_PDP_TYPE_UMTS)
				debug_print("Profile Protocol Type = [UMTS]\n");
			else
				debug_print("Profile Protocol Type = [NONE]\n");

			if (pdp_info->ServiceType == NET_SERVICE_INTERNET)
				debug_print("Profile Service Type = [Internet]\n");
			else if (pdp_info->ServiceType == NET_SERVICE_MMS)
				debug_print("Profile Service Type = [MMS]\n");
			else if (pdp_info->ServiceType == NET_SERVICE_WAP)
				debug_print("Profile Service Type = [WAP]\n");
			else if (pdp_info->ServiceType == NET_SERVICE_PREPAID_INTERNET)
				debug_print("Profile Service Type = [Prepaid Internet]\n");
			else if (pdp_info->ServiceType == NET_SERVICE_PREPAID_MMS)
				debug_print("Profile Service Type = [Prepaid MMS]\n");
			else
				debug_print("Profile Service Type = [Unknown]\n");

			debug_print("Profile APN = [%s]\n", pdp_info->Apn);

			if (pdp_info->AuthInfo.AuthType == NET_PDP_AUTH_PAP)
				debug_print("Profile Auth Type = [PAP]\n");
			else if (pdp_info->AuthInfo.AuthType == NET_PDP_AUTH_CHAP)
				debug_print("Profile Auth Type = [CHAP]\n");
			else
				debug_print("Profile Auth Type = [NONE]\n");

			debug_print("Profile Auth UserName = [%s]\n", pdp_info->AuthInfo.UserName);
			debug_print("Profile Auth Password = [%s]\n", pdp_info->AuthInfo.Password);

			debug_print("Profile Home URL = [%s]\n", pdp_info->HomeURL);
			debug_print("Profile MCC = [%s]\n", pdp_info->Mcc);
			debug_print("Profile MNC = [%s]\n", pdp_info->Mnc);
			debug_print("Profile Roaming = [%d]\n", (int)pdp_info->Roaming);
			debug_print("Profile Setup Required = [%d]\n",
					(int)pdp_info->SetupRequired);
		}
	} else if (ProfInfo->profile_type == NET_DEVICE_ETHERNET) {
		debug_print("Profile Type = [ethernet]\n");
	}

	if (print_type == PROFILE_FULL_INFO) {
		unsigned char *dns = (unsigned char *)&net_info->DnsAddr[di].Data.Ipv4.s_addr;

		debug_print("Profile Favourite = [%d]\n", (int)ProfInfo->Favourite);
		debug_print("Profile Device Name = [%s]\n", net_info->DevName);
		debug_print("Profile DNS Count = [%d]\n", net_info->DnsCount);

		for (di = 0;di < net_info->DnsCount;di++)
			debug_print("Profile DNS Address %d = [%d.%d.%d.%d]\n",
					di+1, dns[0], dns[1], dns[2], dns[3]);

		if (net_info->IpConfigType == NET_IP_CONFIG_TYPE_DYNAMIC)
			debug_print("Profile IPv4 Method = [NET_IP_CONFIG_TYPE_DYNAMIC]\n");
		else if (net_info->IpConfigType == NET_IP_CONFIG_TYPE_STATIC)
			debug_print("Profile IPv4 Method = [NET_IP_CONFIG_TYPE_STATIC]\n");
		else if (net_info->IpConfigType == NET_IP_CONFIG_TYPE_FIXED)
			debug_print("Profile IPv4 Method = [NET_IP_CONFIG_TYPE_FIXED]\n");
		else if (net_info->IpConfigType == NET_IP_CONFIG_TYPE_OFF)
			debug_print("Profile IPv4 Method = [NET_IP_CONFIG_TYPE_OFF]\n");
		else
			debug_print("Profile IPv4 Method = [UNKNOWN]\n");

		debug_print("Profile IP Address = [%d.%d.%d.%d]\n",
				ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
		debug_print("Profile Netmask = [%d.%d.%d.%d]\n",
				netmaskaddr[0], netmaskaddr[1], netmaskaddr[2], netmaskaddr[3]);
		debug_print("Profile Gateway = [%d.%d.%d.%d]\n",
				gwaddr[0], gwaddr[1], gwaddr[2], gwaddr[3]);

		if (net_info->ProxyMethod == NET_PROXY_TYPE_DIRECT)
			debug_print("Proxy Method = [direct]\n");
		else if (net_info->ProxyMethod == NET_PROXY_TYPE_AUTO)
			debug_print("Proxy Method = [auto]\n");
		else if (net_info->ProxyMethod == NET_PROXY_TYPE_MANUAL)
			debug_print("Proxy Method = [manual]\n");
		else
			debug_print("Proxy Method = [unknown]\n");

		debug_print("Profile Proxy = [%s]\n", net_info->ProxyAddr);
		debug_print("Profile MAC = [%s]\n", net_info->MacAddr);
	}
}

static void __print_profile_list(int num_of_profile,
		net_profile_info_t* profile_table, profile_print_type_t print_type)
{
	debug_print("num_of_profile [%d], Profiles [%p]\n",
			num_of_profile, profile_table);

	if (num_of_profile > 0 && profile_table != NULL) {
		int ci = 0;
		for (ci = 0;ci < num_of_profile;ci++)
			__network_print_profile(profile_table + ci, print_type);
	}
}

static void __network_evt_cb (net_event_info_t*  event_cb, void* user_data)
{
	net_profile_info_t *prof_info = NULL;

	debug_print("==CM Event callback==\n");

	switch (event_cb->Event)	{
	case NET_EVENT_WIFI_SCAN_IND:
		if (event_cb->Error != NET_ERR_NONE) {
			debug_print("Got NET_EVENT_WIFI_SCAN_IND. Scan failed!, Error [%d]\n",
					event_cb->Error);
			break;
		}

		debug_print("Got NET_EVENT_WIFI_SCAN_IND\n");
		break;

	case NET_EVENT_WIFI_SCAN_RSP:
		if (event_cb->Error != NET_ERR_NONE) {
			debug_print("Got NET_EVENT_WIFI_SCAN_RSP. Scan failed!, Error [%d]\n",
					event_cb->Error);
			break;
		}

		debug_print("Got NET_EVENT_WIFI_SCAN_RSP\n");

		int num_of_profile = 0;
		int Error;
		net_profile_info_t* Profiles = NULL;

		Error = net_get_profile_list(NET_DEVICE_WIFI, &Profiles, &num_of_profile);
		if (Error != NET_ERR_NONE) {
			debug_print("Error!!! failed to get service(profile) list. Error [%d]\n",
					Error);
		} else {
			debug_print("network_dbus_get_scan_result() successfull\n");
			debug_print("........................Scan Table.......................\n");
			__print_profile_list(num_of_profile, Profiles, PROFILE_BASIC_INFO);
			debug_print(".........................................................\n");
		}

		MAIN_MEMFREE(Profiles);

		gettimeofday(&timevar, NULL);
		async_time = Convert_time2double(timevar);
		debug_print("Async Resp total time taken = [%f]\n", async_time - finish_time);

		break;

	case NET_EVENT_IP_CHANGE_IND:
		debug_print("Got IP Change Indication.\n");
		break;

	case NET_EVENT_OPEN_IND:
	case NET_EVENT_OPEN_RSP:
		if (event_cb->Event == NET_EVENT_OPEN_RSP)
			debug_print("Got Open RSP\n");
		else
			debug_print("Got Open Indication. Auto Joined\n");

		gettimeofday(&timevar, NULL);
		async_time = Convert_time2double(timevar);
		debug_print("Async Resp total time taken = [%f]\n", async_time - finish_time);

		debug_print("Received ACTIVATION response: %d \n", event_cb->Error);

		switch (event_cb->Error) {
		case NET_ERR_NONE:
			/* Successful PDP Activation */
			if (event_cb->Datalength != sizeof(net_profile_info_t)) {
				debug_print("Activation succeeded, but can't get profile info\n");
			} else {
				prof_info = (net_profile_info_t*)event_cb->Data;
				__network_print_profile(prof_info, PROFILE_FULL_INFO);
			}
			break;
		case NET_ERR_TIME_OUT:
			debug_print("Request time out!\n");
			break;
		case NET_ERR_IN_PROGRESS:
			debug_print("Connction is in progress!\n");
			break;
		case NET_ERR_ACTIVE_CONNECTION_EXISTS:
			/* Successful PDP Activation(share the existing connection) */
			if (event_cb->Datalength != sizeof(net_profile_info_t)) {
				debug_print("Activation succeeded, but can't get profile info\n");
			} else {
				prof_info = (net_profile_info_t*)event_cb->Data;
				__network_print_profile(prof_info, PROFILE_FULL_INFO);
			}
			break;
		case NET_ERR_OPERATION_ABORTED:
			debug_print("Connction is aborted!\n");
			break;
		case NET_ERR_UNKNOWN_METHOD:
			debug_print("Service not found!\n");
			break;
		case NET_ERR_UNKNOWN:
			debug_print("Activation Failed!\n");
			break;
		default:
			debug_print("Unknown Error!\n");
			break;
		}

		break;

	case NET_EVENT_CLOSE_RSP:
		debug_print("Got Close RSP\n");

		gettimeofday(&timevar, NULL);
		async_time = Convert_time2double(timevar);
		debug_print("Async Resp total time taken = [%f]\n", async_time - finish_time);

		switch (event_cb->Error) {
		case NET_ERR_NONE:
			/* Successful PDP Deactivation */
			debug_print("Deactivation succeeded!\n");
			break;
		case NET_ERR_TIME_OUT:
			debug_print("Request time out!\n");
			break;
		case NET_ERR_IN_PROGRESS:
			debug_print("Disconncting is in progress!\n");
			break;
		case NET_ERR_OPERATION_ABORTED:
			debug_print("Disconnction is aborted!\n");
			break;
		case NET_ERR_UNKNOWN_METHOD:
			debug_print("Service not found!\n");
			break;
		case NET_ERR_UNKNOWN:
			debug_print("Deactivation Failed!\n");
			break;
		default:
			debug_print("Unknown Error!\n");
			break;
		}

		break;

	case NET_EVENT_CLOSE_IND:
		debug_print("Got Close IND\n");
		break;

	case NET_EVENT_WIFI_POWER_IND:
	case NET_EVENT_WIFI_POWER_RSP:
		if (event_cb->Event == NET_EVENT_WIFI_POWER_RSP)
			debug_print("Got Wi-Fi Power RSP\n");
		else
			debug_print("Got Wi-Fi Power IND\n");

		net_wifi_state_t *wifi_state = (net_wifi_state_t*)event_cb->Data;

		if (event_cb->Error == NET_ERR_NONE &&
		    event_cb->Datalength == sizeof(net_wifi_state_t)) {
			if (*wifi_state == WIFI_ON)
				debug_print("Wi-Fi State : Power ON\n");
			else if (*wifi_state == WIFI_OFF)
				debug_print("Wi-Fi State : Power OFF\n");
			else
				debug_print("Wi-Fi State : Unknown\n");
		} else
			debug_print("Wi-Fi Power on/off request failed! Error [%d]\n",
					event_cb->Error);

		break;

	case NET_EVENT_NET_STATE_IND:
		debug_print("Got State changed IND\n");
		net_state_type_t *profile_state = (net_state_type_t*)event_cb->Data;

		if (event_cb->Error == NET_ERR_NONE &&
		    event_cb->Datalength == sizeof(net_state_type_t)) {
			switch (*profile_state) {
			case NET_STATE_TYPE_IDLE:
				debug_print("Wi-Fi State : Idle, profile name : %s\n",
						event_cb->ProfileName);
				break;
			case NET_STATE_TYPE_FAILURE:
				debug_print("Wi-Fi State : Failure, profile name : %s\n",
						event_cb->ProfileName);
				break;
			case NET_STATE_TYPE_ASSOCIATION:
				debug_print("Wi-Fi State : Association, profile name : %s\n",
						event_cb->ProfileName);
				break;
			case NET_STATE_TYPE_CONFIGURATION:
				debug_print("Wi-Fi State : Configuration, profile name : %s\n",
						event_cb->ProfileName);
				break;
			case NET_STATE_TYPE_READY:
				debug_print("Wi-Fi State : Ready, profile name : %s\n",
						event_cb->ProfileName);
				break;
			case NET_STATE_TYPE_ONLINE:
				debug_print("Wi-Fi State : Online, profile name : %s\n",
						event_cb->ProfileName);
				break;
			case NET_STATE_TYPE_DISCONNECT:
				debug_print("Wi-Fi State : Disconnect, profile name : %s\n",
						event_cb->ProfileName);
				break;
			default :
				debug_print("Wi-Fi State : unknown\n");
				break;
			}
		}
		break;

	case NET_EVENT_WIFI_WPS_RSP:
		debug_print("Got WPS Rsp\n");
		break;

	default :
		debug_print("Error! Unknown Event\n\n");
		break;
	}

}

int __network_modify_profile_info(net_profile_info_t *profile_info)
{
	net_dev_info_t *net_info2 = &profile_info->ProfileInfo.Wlan.net_info;
	wlan_security_info_t *security_info2 =
			&profile_info->ProfileInfo.Wlan.security_info;
	net_pdp_profile_info_t *pdp_info = &profile_info->ProfileInfo.Pdp;
	char input_str[100] = {0,};
	int ei = 0;

	if (profile_info->profile_type == NET_DEVICE_WIFI) {
		debug_print("\nInput Passphrase(Enter for skip) :\n");

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		input_str[strlen(input_str) - 1] = '\0';

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
			g_strlcpy(security_info2->authentication.psk.pskKey,
					input_str, NETPM_WLAN_MAX_PSK_PASSPHRASE_LEN + 1);

		debug_print("\nInput Proxy Type(1:direct, 2:auto, 3:manual - current:%d)"
				" - (Enter for skip) :\n", net_info2->ProxyMethod);

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		input_str[strlen(input_str) - 1] = '\0';

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r') {
			int proxyType = 0;
			proxyType = atoi(input_str);

			if (proxyType == NET_PROXY_TYPE_DIRECT)
				net_info2->ProxyMethod = NET_PROXY_TYPE_DIRECT;
			else if (proxyType == NET_PROXY_TYPE_AUTO)
				net_info2->ProxyMethod = NET_PROXY_TYPE_AUTO;
			else if (proxyType == NET_PROXY_TYPE_MANUAL)
				net_info2->ProxyMethod = NET_PROXY_TYPE_MANUAL;

			if (net_info2->ProxyMethod == NET_PROXY_TYPE_AUTO ||
			    net_info2->ProxyMethod == NET_PROXY_TYPE_MANUAL) {

				if (net_info2->ProxyMethod == NET_PROXY_TYPE_AUTO)
					debug_print("\nInput auto Proxy URL - "
							"(Enter for DHCP/WPAD auto-discover) :\n");
				else
					debug_print("\nInput manual Proxy address - "
							"(Enter for skip) :\n");

				memset(input_str, '\0', 100);
				read(0, input_str, 100);

				if (input_str[0] != '\0' &&
				    *input_str != '\n' &&
				    *input_str != '\r') {
					input_str[strlen(input_str)-1] = '\0';
					g_strlcpy(net_info2->ProxyAddr,
							input_str, NET_PROXY_LEN_MAX + 1);
				} else {
					net_info2->ProxyAddr[0] = '\0';
				}
			}
		}

		debug_print("\nInput IPv4 Address Type dhcp/manual (Enter for skip) :\n");

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		input_str[strlen(input_str) - 1] = '\0';

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r') {
			if (strcmp(input_str, "dhcp") == 0) {
				net_info2->IpConfigType = NET_IP_CONFIG_TYPE_DYNAMIC;
			} else if (strcmp(input_str, "manual") == 0) {
				net_info2->IpConfigType = NET_IP_CONFIG_TYPE_STATIC;

				debug_print("\nInput IP Address (Enter for skip) :\n");

				memset(input_str, '\0', 100);
				read(0, input_str, 100);

				input_str[strlen(input_str) - 1] = '\0';

				if ((input_str[0] != '\0' &&
				     *input_str != '\n' &&
				     *input_str != '\r') &&
				    strlen(input_str) >= NETPM_IPV4_STR_LEN_MIN) {
					inet_aton(input_str, &(net_info2->IpAddr.Data.Ipv4));
				}

				debug_print("\nInput Netmask (Enter for skip) :\n");

				memset(input_str, '\0', 100);
				read(0, input_str, 100);

				input_str[strlen(input_str) - 1] = '\0';

				if ((input_str[0] != '\0' &&
				     *input_str != '\n' &&
				     *input_str != '\r') &&
				    strlen(input_str) >= NETPM_IPV4_STR_LEN_MIN) {
					inet_aton(input_str, &(net_info2->SubnetMask.Data.Ipv4));
				}

				debug_print("\nInput Gateway (Enter for skip) :\n");

				memset(input_str, '\0', 100);
				read(0, input_str, 100);

				input_str[strlen(input_str)-1] = '\0';

				if ((input_str[0] != '\0' &&
				     *input_str != '\n' &&
				     *input_str != '\r') &&
				    strlen(input_str) >= NETPM_IPV4_STR_LEN_MIN) {
					inet_aton(input_str, &(net_info2->GatewayAddr.Data.Ipv4));
				}
			}
		}

		if (net_info2->IpConfigType == NET_IP_CONFIG_TYPE_STATIC) {
			for (ei = 0;ei < NET_DNS_ADDR_MAX;ei++) {
				debug_print("\nInput DNS %d Address(Enter for skip) :\n", ei);

				memset(input_str, '\0', 100);
				read(0, input_str, 100);

				input_str[strlen(input_str)-1] = '\0';

				if (input_str[0] != '\0' &&
				    *input_str != '\n' &&
				    *input_str != '\r') {
					inet_aton(input_str, &(net_info2->DnsAddr[ei].Data.Ipv4));
					net_info2->DnsCount = ei+1;
				} else {
					return FALSE;
				}
			}
		}
	} else if (profile_info->profile_type == NET_DEVICE_CELLULAR) {
		debug_print("\nInput Apn(current:%s) - (Enter for skip) :\n", pdp_info->Apn);

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		input_str[strlen(input_str)-1] = '\0';

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
			g_strlcpy(pdp_info->Apn, input_str, NET_PDP_APN_LEN_MAX+1);

		debug_print("\nInput Proxy(current:%s) - (Enter for skip) :\n",
				pdp_info->net_info.ProxyAddr);

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		input_str[strlen(input_str)-1] = '\0';

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
			g_strlcpy(pdp_info->net_info.ProxyAddr, input_str, NET_PROXY_LEN_MAX+1);

		debug_print("\nInput HomeURL(current:%s) - (Enter for skip) :\n",
				pdp_info->HomeURL);

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		input_str[strlen(input_str)-1] = '\0';

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
			g_strlcpy(pdp_info->HomeURL, input_str, NET_HOME_URL_LEN_MAX+1);

		debug_print("\nInput AuthType(0:None, 1:PAP, 2:CHAP - current:%d)"
				" - (Enter for skip) :\n", pdp_info->AuthInfo.AuthType);

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r') {
			int typeValue = 0;
			typeValue = atoi(input_str);

			if(typeValue == NET_PDP_AUTH_PAP)
				pdp_info->AuthInfo.AuthType = NET_PDP_AUTH_PAP;
			else if(typeValue == NET_PDP_AUTH_CHAP)
				pdp_info->AuthInfo.AuthType = NET_PDP_AUTH_CHAP;
			else
				pdp_info->AuthInfo.AuthType = NET_PDP_AUTH_NONE;
		}

		if (pdp_info->AuthInfo.AuthType == NET_PDP_AUTH_PAP ||
		    pdp_info->AuthInfo.AuthType == NET_PDP_AUTH_CHAP) {
			debug_print("\nInput AuthId(current:%s) - (Enter for skip) :\n",
					pdp_info->AuthInfo.UserName);

			memset(input_str, '\0', 100);
			read(0, input_str, 100);

			input_str[strlen(input_str)-1] = '\0';

			if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
				g_strlcpy(pdp_info->AuthInfo.UserName,
						input_str, NET_PDP_AUTH_USERNAME_LEN_MAX+1);

			debug_print("\nInput AuthPwd(current:%s) - (Enter for skip) :\n",
					pdp_info->AuthInfo.Password);

			memset(input_str, '\0', 100);
			read(0, input_str, 100);

			input_str[strlen(input_str)-1] = '\0';

			if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
				g_strlcpy(pdp_info->AuthInfo.Password,
						input_str, NET_PDP_AUTH_PASSWORD_LEN_MAX+1);
		}
	}
	return TRUE;
}

int __network_add_profile_info(net_profile_info_t *profile_info)
{
	net_pdp_profile_info_t *pdp_info = &profile_info->ProfileInfo.Pdp;
	char input_str[100] = {0,};

	debug_print("\nInput Apn - (Enter for skip) :\n");

	memset(input_str, '\0', 100);
	read(0, input_str, 100);

	input_str[strlen(input_str)-1] = '\0';

	if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
		g_strlcpy(pdp_info->Apn, input_str, NET_PDP_APN_LEN_MAX+1);
	else
		pdp_info->Apn[0] = '\0';

	debug_print("\nInput Proxy - (Enter for skip) :\n");

	memset(input_str, '\0', 100);
	read(0, input_str, 100);

	input_str[strlen(input_str)-1] = '\0';

	if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
		g_strlcpy(pdp_info->net_info.ProxyAddr, input_str, NET_PROXY_LEN_MAX+1);
	else
		pdp_info->net_info.ProxyAddr[0] = '\0';

	debug_print("\nInput HomeURL - (Enter for skip) :\n");

	memset(input_str, '\0', 100);
	read(0, input_str, 100);

	input_str[strlen(input_str)-1] = '\0';

	if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
		g_strlcpy(pdp_info->HomeURL, input_str, NET_HOME_URL_LEN_MAX+1);
	else
		pdp_info->HomeURL[0] = '\0';

	debug_print("\nInput AuthType(0:NONE 1:PAP 2:CHAP) - (Enter for skip) :\n");

	memset(input_str, '\0', 100);
	read(0, input_str, 100);

	if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r') {
		int typeValue = 0;
		typeValue = atoi(input_str);

		if (typeValue == NET_PDP_AUTH_PAP)
			pdp_info->AuthInfo.AuthType = NET_PDP_AUTH_PAP;
		else if (typeValue == NET_PDP_AUTH_CHAP)
			pdp_info->AuthInfo.AuthType = NET_PDP_AUTH_CHAP;
		else
			pdp_info->AuthInfo.AuthType = NET_PDP_AUTH_NONE;
	} else {
		pdp_info->AuthInfo.AuthType = NET_PDP_AUTH_NONE;
	}

	if (pdp_info->AuthInfo.AuthType == NET_PDP_AUTH_PAP ||
	    pdp_info->AuthInfo.AuthType == NET_PDP_AUTH_CHAP) {
		debug_print("\nInput AuthId - (Enter for skip) :\n");

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		input_str[strlen(input_str)-1] = '\0';

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
			g_strlcpy(pdp_info->AuthInfo.UserName,
					input_str, NET_PDP_AUTH_USERNAME_LEN_MAX+1);
		else
			pdp_info->AuthInfo.UserName[0] = '\0';

		debug_print("\nInput AuthPwd - (Enter for skip) :\n");

		memset(input_str, '\0', 100);
		read(0, input_str, 100);

		input_str[strlen(input_str)-1] = '\0';

		if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r')
			g_strlcpy(pdp_info->AuthInfo.Password,
					input_str, NET_PDP_AUTH_PASSWORD_LEN_MAX+1);
		else
			pdp_info->AuthInfo.Password[0] = '\0';
	}

	return TRUE;
}

static gpointer network_main_gthread(gpointer data)
{
	char ProfileName[NET_PROFILE_NAME_LEN_MAX+1] = {0,};
	int input_int = 0;
	char input_str[100] = {0,};
	int net_error = 0;
	net_profile_info_t profile_info;

	if (net_register_client ((net_event_cb_t) __network_evt_cb, NULL) != NET_ERR_NONE) {
		debug_print("Error!! net_register_client() failed.\n");
		return NULL;
	}

	debug_print( "net_register_client() successfull\n");

	while (TRUE) {
		fd_set rfds;

		int retval;
		FD_ZERO(&rfds);
		FD_SET(0, &rfds);

		retval = select((0+1), &rfds, NULL, NULL, NULL);

		if (retval <= 0)
			continue;

		if (!FD_ISSET(0, &rfds))
			continue;

		debug_print("Event received from stdin \n");
		char user_input[100];

		memset(user_input, '\0', 100);
		read(0, user_input, 100);

		if (user_input[0] == '9')
			exit(1);

		if (*user_input == '\n' || *user_input == '\r') {
			debug_print("\nDatanetworking Test .......\n\n");
			debug_print("Options..\n");
			debug_print("0	- WLAN Power On request\n");
			debug_print("1	- WLAN Power Off request\n");
			debug_print("2	- WLAN Start scan request\n");
			debug_print("3	- WLAN Set BG Scan Mode\n");
			debug_print("4	- Open Connection with service type\n");
			debug_print("5	- Open Connection with profile name\n");
			debug_print("6	- Close Connection\n");
			debug_print("7	- Get WiFi State\n");
			debug_print("a	- Check whether connected?\n");
			debug_print("b	- Get Network Status\n");
			debug_print("c	- Get Active(default) profile information\n");
			debug_print("d	- Get Profile list\n");
			debug_print("e	- Get Profile info\n");
			debug_print("f	- Modify Profile info\n");
			debug_print("g	- Delete Profile(PDP:delete, WiFi:forgot) \n");
			debug_print("h	- Add Profile(PDP only) \n");
			debug_print("i	- Connect to Specific SSID\n");
			debug_print("k	- Enroll WPS PBC \n");
			debug_print("l	- Connect with EAP\n");
			debug_print("z 	- Exit \n");

			debug_print("ENTER 	- Show options menu.......\n");
		}

		switch (user_input[0]) {
		case '0':
			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_wifi_power_on() != NET_ERR_NONE) {
				debug_print("Error!! net_wifi_power_on() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("net_wifi_power_on() success\n");
			break;

		case '1':
			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_wifi_power_off() != NET_ERR_NONE ) {
				debug_print("Error!! net_wifi_power_off() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("net_wifi_power_off() success\n");
			break;

		case '2':
			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_scan_wifi() != NET_ERR_NONE ) {
				debug_print("Error!! net_scan_wifi() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("net_scan_wifi() success\n");
			break;

		case '3':
			debug_print("Enter BG Scan Mode(0:default, 1:periodic, 2:exponential):");
			scanf("%d", &input_int);

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_wifi_set_background_scan_mode(input_int) != NET_ERR_NONE) {
				debug_print(
					"Error!! network_wifi_set_scan_interval() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("network_wifi_set_background_scan_mode() success\n");
			break;

		case '4':
			debug_print( "Enter Service Type(1:Internet, 2:MMS, 3:WAP): \n");
			scanf("%d", &input_int);

			net_service_type_t service_type = NET_SERVICE_UNKNOWN;

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			switch (input_int) {
			case 1:
				service_type = NET_SERVICE_INTERNET;
				break;
			case 2:
				service_type = NET_SERVICE_MMS;
				break;
			case 3:
				service_type = NET_SERVICE_WAP;
				break;
			}

			if (service_type != NET_SERVICE_UNKNOWN) {
				net_error = net_open_connection_with_preference(service_type);
				if (net_error != NET_ERR_NONE) {
					debug_print(
					  "Error!! net_open_connection_with_profile() failed.\n");
					break;
				}
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("net_open_connection_with_profile() success\n");
			break;

		case '5':
			debug_print( "Enter Profile Name: \n");
			scanf("%s", ProfileName);

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_open_connection_with_profile(ProfileName) != NET_ERR_NONE) {
				debug_print(
					"Error!! net_open_connection_with_profile() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("net_open_connection_with_profile() success\n");
			break;

		case '6':
			debug_print( "Enter Profile Name: \n");
			scanf("%s", ProfileName);

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_close_connection(ProfileName) != NET_ERR_NONE) {
				debug_print(
					"Error!! net_close_connection() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("net_close_connection() success\n");
			break;

		case '7':
			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			net_wifi_state_t wlanstate = 0;
			net_profile_name_t profile_name;

			if (net_get_wifi_state(&wlanstate, &profile_name) != NET_ERR_NONE) {
				debug_print("Error!! net_get_wifi_state() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("net_get_wifi_state() success. wlanstate [%d]\n", wlanstate);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			switch (wlanstate) {
			case WIFI_OFF:
				debug_print("wlanstate : WIFI_OFF\n");
				break;
			case WIFI_ON:
				debug_print("wlanstate : WIFI_ON\n");
				break;
			case WIFI_CONNECTING:
				debug_print("wlanstate : WIFI_CONNECTING, profile name : %s\n",
						profile_name.ProfileName);
				break;
			case WIFI_CONNECTED:
				debug_print("wlanstate : WIFI_CONNECTED, profile name : %s\n",
						profile_name.ProfileName);
				break;
			case WIFI_DISCONNECTING:
				debug_print("wlanstate : WIFI_DISCONNECTING, profile name : %s\n",
						profile_name.ProfileName);
				break;
			default :
				debug_print("wlanstate : Unknown\n");
			}
			break;

		case 'a':
			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			int flag = FALSE;

			flag = net_is_connected();
			debug_print("[%s]\n", (flag == TRUE)? "connected":"not connected");

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			break;

		case 'b':
			debug_print("Enter network type (wifi/pdp): \n");
			scanf("%s", input_str);

			net_device_t device_type = NET_DEVICE_UNKNOWN;
			net_cm_network_status_t NetworkStatus;

			if (strcmp(input_str, "wifi") == 0)
				device_type = NET_DEVICE_WIFI;
			else
				device_type = NET_DEVICE_CELLULAR;

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_get_network_status(device_type, &NetworkStatus) != NET_ERR_NONE) {
				debug_print("Error!!! net_get_network_status() failed\n");
				break;
			}

			debug_print("[%s]\n", (NET_STATUS_AVAILABLE == NetworkStatus) ?
					"NET_STATUS_AVAILABLE":"NET_STATUS_UNAVAILABLE");

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			break;

		case 'c':
			debug_print("Enter info Type(1:Full, 2:ip, 3:netmask, 4:gateway,"
						   " 5:DNS, 6:ESSID, 7:Proxy):\n");
			scanf("%d", &input_int);

			memset(&profile_info, 0, sizeof(net_profile_info_t));

			net_addr_t ip_address;
			net_essid_t essid;
			net_proxy_t proxy;

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			switch (input_int) {
			case 1:
				if (net_get_active_net_info(&profile_info) != NET_ERR_NONE)
					debug_print("Error!!! net_get_active_net_info() failed\n");
				else
					__network_print_profile(&profile_info, PROFILE_FULL_INFO);
				break;
			case 2:
				if (net_get_active_ipaddress(&ip_address) != NET_ERR_NONE)
					debug_print("Error!!! net_get_active_ipaddress() failed\n");
				else
					__network_print_ipaddress(&ip_address);
				break;
			case 3:
				if (net_get_active_netmask(&ip_address) != NET_ERR_NONE)
					debug_print("Error!!! net_get_active_netmask() failed\n");
				else
					__network_print_ipaddress(&ip_address);
				break;
			case 4:
				if (net_get_active_gateway(&ip_address) != NET_ERR_NONE)
					debug_print("Error!!! net_get_active_gateway() failed\n");
				else
					__network_print_ipaddress(&ip_address);
				break;
			case 5:
				if (net_get_active_dns(&ip_address) != NET_ERR_NONE)
					debug_print("Error!!! net_get_active_dns() failed\n");
				else
					__network_print_ipaddress(&ip_address);
				break;
			case 6:
				if (net_get_active_essid(&essid) != NET_ERR_NONE)
					debug_print("Error!!! net_get_active_essid() failed\n");
				else
					debug_print("Profile ESSID = [%s]\n", essid.essid);
				break;
			case 7:
				if (net_get_active_proxy(&proxy) != NET_ERR_NONE)
					debug_print("Error!!! net_get_active_proxy() failed\n");
				else
					debug_print("Profile Proxy = [%s]\n", proxy.proxy_addr);
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);
			break;

		case 'd':
			debug_print("\nInput profile type - 1:wifi, 2:mobile 3:ethernet(Enter for skip):\n");
			memset(input_str, 0, 100);
			read(0, input_str, 100);

			net_device_t deviceType = NET_DEVICE_UNKNOWN;
			int profListCount = 0;
			net_profile_info_t *profList;

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r') {
				input_str[strlen(input_str)-1] = '\0';

				if (strcmp(input_str, "1") == 0)
					deviceType = NET_DEVICE_WIFI;
				else if (strcmp(input_str, "2") == 0)
					deviceType = NET_DEVICE_CELLULAR;
				else if (strcmp(input_str, "3") == 0)
					deviceType = NET_DEVICE_ETHERNET;

				net_error = net_get_profile_list(deviceType,
						&profList, &profListCount);

				if (net_error != NET_ERR_NONE) {
					debug_print("Error!!! net_get_profile_list() failed\n");
					break;
				}

				__print_profile_list(profListCount, profList, PROFILE_BASIC_INFO);
				MAIN_MEMFREE(profList);
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			break;

		case 'e':
			debug_print("\nInput profile Name((Enter for skip) :\n");
			memset(ProfileName, 0, NET_PROFILE_NAME_LEN_MAX);
			read(0, ProfileName, NET_PROFILE_NAME_LEN_MAX);

			if (ProfileName[0] == '\0' || *ProfileName == '\n' || *ProfileName == '\r')
				debug_print("\nCanceled!\n\n");

			ProfileName[strlen(ProfileName)-1] = '\0';

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_get_profile_info(ProfileName, &profile_info) != NET_ERR_NONE) {
				debug_print("Error!!! net_get_profile_info() failed\n");
				break;
			}

			__network_print_profile(&profile_info, PROFILE_FULL_INFO);

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			break;

		case 'f':
			debug_print("\nInput profile Name(Enter for skip) :\n");
			memset(ProfileName, '\0', NET_PROFILE_NAME_LEN_MAX);
			read(0, ProfileName, NET_PROFILE_NAME_LEN_MAX);

			if (ProfileName[0] != '\0' &&
			    *ProfileName != '\n' &&
			    *ProfileName != '\r') {
				ProfileName[strlen(ProfileName) - 1] = '\0';

				net_error = net_get_profile_info(ProfileName, &profile_info);
				if (net_error != NET_ERR_NONE) {
					debug_print("Error!!! net_get_profile_info() failed\n");
					break;
				}
			} else {
				debug_print("\nCanceled!\n\n");
				break;
			}

			if (!__network_modify_profile_info(&profile_info))
				continue;

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_modify_profile(ProfileName, &profile_info) != NET_ERR_NONE) {
				debug_print("Error!!! net_modify_profile() failed\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			break;

		case 'g':
			debug_print("\nInput profile Name(Enter for skip) :\n");

			memset(ProfileName, '\0', NET_PROFILE_NAME_LEN_MAX);
			read(0, ProfileName, NET_PROFILE_NAME_LEN_MAX);

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (ProfileName[0] == '\0' || *ProfileName == '\n' || *ProfileName == '\r')
				debug_print("\nCanceled!\n\n");

			ProfileName[strlen(ProfileName)-1] = '\0';

			if (net_delete_profile(ProfileName) != NET_ERR_NONE) {
				debug_print("Error!!! net_delete_profile() failed\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			break;

		case 'h':
			debug_print("\nInput Network Type(internet:1, MMS:2, WAP:3)"
					" - (Enter for skip) :\n");

			memset(input_str, '\0', 100);
			read(0, input_str, 100);

			net_service_type_t network_type = NET_SERVICE_INTERNET;
			memset(&profile_info, 0, sizeof(net_profile_info_t));

			if (input_str[0] != '\0' && *input_str != '\n' && *input_str != '\r') {
				int typeValue = 0;
				typeValue = atoi(input_str);

				if (typeValue > NET_SERVICE_UNKNOWN &&
				    typeValue <= NET_SERVICE_PREPAID_MMS)
					network_type = typeValue;
				else
					break;
			} else {
				break;
			}

			__network_add_profile_info(&profile_info);

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			if (net_add_profile(network_type, &profile_info) != NET_ERR_NONE) {
				debug_print("Error!!! net_add_profile() failed\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			break;

		case 'i': {
			net_wifi_connection_info_t wifi_info = {{0,}, };

			debug_print("Enter essid:\n");
			scanf("%s", wifi_info.essid);

			wifi_info.wlan_mode = NETPM_WLAN_CONNMODE_INFRA;
			wifi_info.security_info.sec_mode = WLAN_SEC_MODE_WPA_PSK;

			debug_print("Enter psk key:\n");
			scanf("%s", wifi_info.security_info.authentication.psk.pskKey);

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);

			net_error = net_open_connection_with_wifi_info(&wifi_info);

			if (net_error != NET_ERR_NONE) {
				debug_print("Error!!! "
					    "net_open_connection_with_wifi_info() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("net_open_connection_with_wifi_info() success\n");
		}

			break;

		case 'k': {
			debug_print( "Enter Profile Name: \n");
			scanf("%s", ProfileName);

			gettimeofday(&timevar, NULL);
			start_time = Convert_time2double(timevar);
			net_wifi_wps_info_t wps_info;
			memset(&wps_info, 0, sizeof(net_wifi_wps_info_t));

			wps_info.type = WIFI_WPS_PBC;

			if (net_wifi_enroll_wps(ProfileName, &wps_info) != NET_ERR_NONE) {
				debug_print("Error!! net_wifi_enroll_wps() failed.\n");
				break;
			}

			gettimeofday(&timevar, NULL);
			finish_time = Convert_time2double(timevar);
			debug_print("Total time taken = [%f]\n", finish_time - start_time);

			debug_print("net_wifi_enroll_wps() success\n");
		}
			break;

		case 'l': {
			int eap_type = 0;
			int eap_auth = 0;
			net_wifi_connection_info_t info;
			memset(&info, 0, sizeof(net_wifi_connection_info_t));

			info.wlan_mode = NETPM_WLAN_CONNMODE_INFRA;
			info.security_info.sec_mode = WLAN_SEC_MODE_IEEE8021X;

			debug_print("Enter essid:\n");
			scanf("%s", info.essid);

			debug_print("Enter EAP type PEAP 1, TLS 2, TTLS 3, SIM 4, AKA 5:\n");
			scanf("%d", &eap_type);
			info.security_info.authentication.eap.eap_type = (wlan_eap_type_t) eap_type;

			debug_print("Enter EAP auth None 1, PAP 2, MSCHAP 3, MSCHAPV2 4, GTC 5, MD5 6:\n");
			scanf("%d", &eap_auth);
			info.security_info.authentication.eap.eap_auth = (wlan_eap_auth_type_t) eap_auth;

			debug_print("Enter user name:\n");
			scanf("%s", info.security_info.authentication.eap.username);

			debug_print("Enter password:\n");
			scanf("%s", info.security_info.authentication.eap.password);

			debug_print("Enter CA Cert filename:\n");
			scanf("%s", info.security_info.authentication.eap.ca_cert_filename);

			debug_print("Enter Client Cert filename:\n");
			scanf("%s", info.security_info.authentication.eap.client_cert_filename);

			debug_print("Enter private key filename:\n");
			scanf("%s", info.security_info.authentication.eap.private_key_filename);

			debug_print("Enter private key password:\n");
			scanf("%s", info.security_info.authentication.eap.private_key_passwd);

			net_open_connection_with_wifi_info(&info);
		}
			break;

		case 'z':
			debug_print( "Exiting...!!!\n");

			if (net_deregister_client () != NET_ERR_NONE) {
				debug_print("Error!! net_deregister_client() failed.\n");
				return NULL;
			}

			debug_print( "net_deregister_client() success\n");

			exit(0);

		default:
			debug_print( "default...!!!\n");
			break;
		}
	}

	if (net_deregister_client () != NET_ERR_NONE) {
		debug_print("Error!! net_deregister_client() failed.\n");
		return NULL;
	}

	debug_print( "net_deregister_client() success\n");

	return NULL;
}


int main(int argc, char *argv[])
{
	GMainLoop* mainloop = NULL;
	GThread * main_gthread_ptr = NULL;

	/* First thing initialze the thread system */
        g_thread_init(NULL);

	/** This thread is used to recieve signals from ConnMan */
	main_gthread_ptr = g_thread_create(network_main_gthread, NULL, FALSE, NULL);
	if (main_gthread_ptr == NULL) {
		debug_print("Error!!! g_thread_create() failed.\n");
		return -1;
	}

	debug_print("Entering gmainloop\n");
	mainloop = g_main_loop_new(NULL, FALSE);
	g_main_loop_run(mainloop);

	debug_print("Returned from gmainloop\n");

	return 0;
}

