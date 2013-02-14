/*
 *  Network Client Library
 *
 * Copyright 2011-2013 Samsung Electronics Co., Ltd

 * Licensed under the Flora License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at

 * http://floralicense.org/license/

 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */


#ifndef __NETWORK_PM_WLAN_H__
#define __NETWORK_PM_WLAN_H__


#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

/**  \file network-pm-wlan.h
     \brief This file contains constants, enums, tructs, and function prototypes that are used by Wlan related sources internally. This File defines the WLAN exported Data Structures.

*/

/**
 * \addtogroup  profile_managing
 * \{
*/

/*
==================================================================================================
                                         INCLUDE FILES
==================================================================================================
*/

#include "network-pm-config.h"

/*
==================================================================================================
                                           CONSTANTS
==================================================================================================
*/

/** Length of essid */
#define NET_WLAN_ESSID_LEN      128


/** 
 * Length of WPS PIN code 
 * WPS PIN code should be 4 or 8 digits
 */
#define NET_WLAN_MAX_WPSPIN_LEN		8	

/**
 * Passphrase length should be between 8..63,
 * If we plan to use encrypted key(hex value generated by wpa_passphrase),
 * then we have to set this value to some higher number
 *
 */
#define NETPM_WLAN_MAX_PSK_PASSPHRASE_LEN 65

/**
 * Length of WEP Key
 * Max of 10 Hex digits allowed in case of 64 bit encryption
 * Max of 26 Hex digits allowed in case of 128 bit encryption
 */
#define NETPM_WLAN_MAX_WEP_KEY_LEN        26


/**
 * These lengths depends on authentication server being used,
 * In case of freeradius server Max allowed length for username/password is 255
 * Let us restrict this value to some optimal value say 50.
 * Used by EAP-TLS, optional for EAP-TTLS and EAP-PEAP
 */
#define NETPM_WLAN_USERNAME_LEN               50
/**
 * These lengths depends on authentication server being used,
 * In case of freeradius server Max allowed length for username/password is 255
 * Let us restrict this value to some optimal value say 50.
 * Used by EAP-TLS, optional for EAP-TTLS and EAP-PEAP
 */
#define NETPM_WLAN_PASSWORD_LEN               50

/**
 * length of CA Cert file name
 * Used by EAP-TLS, optional for EAP-TTLS and EAP-PEAP
 */
#define NETPM_WLAN_CA_CERT_FILENAME_LEN       50
/**
 * length of Client Cert file name
 * Used by EAP-TLS, optional for EAP-TTLS and EAP-PEAP
 */
#define NETPM_WLAN_CLIENT_CERT_FILENAME_LEN   50
/**
 * length of private key file name
 * Used by EAP-TLS, optional for EAP-TTLS and EAP-PEAP
 */
#define NETPM_WLAN_PRIVATE_KEY_FILENAME_LEN   50
/**
 * length of Private key password
 * Used by EAP-TLS, optional for EAP-TTLS and EAP-PEAP
 */
#define NETPM_WLAN_PRIVATE_KEY_PASSWD_LEN     50

/*==================================================================================================
                                             ENUMS
==================================================================================================*/

/**
 * @enum wlan_security_mode_type_t
 * Below security modes are used in infrastructure and ad-hoc mode
 * For now all EAP security mechanisms are provided only in infrastructure mode
 */
typedef enum
{
	/** Security disabled */
	WLAN_SEC_MODE_NONE = 0x01,
	/** WEP */
	WLAN_SEC_MODE_WEP,
	/** EAP */
	WLAN_SEC_MODE_IEEE8021X,
	/** WPA-PSK */
	WLAN_SEC_MODE_WPA_PSK,
	/** WPA2-PSK */
	WLAN_SEC_MODE_WPA2_PSK,
} wlan_security_mode_type_t;

/**
 * @enum wlan_encryption_mode_type_t
 * Below encryption modes are used in infrastructure and ad-hoc mode
 */
typedef enum
{
	/** Encryption disabled */
	WLAN_ENC_MODE_NONE = 0x01,
	/** WEP */
	WLAN_ENC_MODE_WEP,
	/** TKIP */
	WLAN_ENC_MODE_TKIP,
	/** AES */
	WLAN_ENC_MODE_AES,
	/** TKIP and AES are both supported */
	WLAN_ENC_MODE_TKIP_AES_MIXED,
} wlan_encryption_mode_type_t;

/**
 * @enum wlan_connection_mode_type_t
 * WLAN Operation Mode
 * @see net_pm_wlan_profile_info_t
 */
typedef enum
{
	/** auto connection mode */
	NETPM_WLAN_CONNMODE_AUTO = 0x01,
	/** Connection mode Adhoc  */
	NETPM_WLAN_CONNMODE_ADHOC,
	/** Infra connection mode */
	NETPM_WLAN_CONNMODE_INFRA,
} wlan_connection_mode_type_t;


/**
 * @enum wlan_eap_type_t
 * EAP type
 * @see wlan_eap_info_t
 */
typedef enum {
	/** EAP PEAP type */
	WLAN_SEC_EAP_TYPE_PEAP = 0x01,
	/** EAP TLS type */
	WLAN_SEC_EAP_TYPE_TLS,
	/** EAP TTLS type */
	WLAN_SEC_EAP_TYPE_TTLS,
	/** EAP SIM type */
	WLAN_SEC_EAP_TYPE_SIM,
	/** EAP AKA type */
	WLAN_SEC_EAP_TYPE_AKA,
} wlan_eap_type_t;

/**
 * @enum wlan_eap_auth_type_t
 * EAP phase2 authentication type
 * @see wlan_eap_info_t
 */
typedef enum {
	/** EAP phase2 authentication none */
	WLAN_SEC_EAP_AUTH_NONE = 0x01,
	/** EAP phase2 authentication PAP */
	WLAN_SEC_EAP_AUTH_PAP,
	/** EAP phase2 authentication MSCHAP */
	WLAN_SEC_EAP_AUTH_MSCHAP,
	/** EAP phase2 authentication MSCHAPv2 */
	WLAN_SEC_EAP_AUTH_MSCHAPV2,
	/** EAP phase2 authentication GTC */
	WLAN_SEC_EAP_AUTH_GTC,
	/** EAP phase2 authentication MD5 */
	WLAN_SEC_EAP_AUTH_MD5,
} wlan_eap_auth_type_t;

/*
==================================================================================================
                                            MACROS
==================================================================================================
*/


/*==================================================================================================
                                 STRUCTURES AND OTHER TYPEDEFS
==================================================================================================*/

/**
 * Below structure is used to export essid
 */
typedef struct
{
	/** ESSID */
	char essid[NET_WLAN_ESSID_LEN+1];
} net_essid_t;


/**
 * Below structure is used by WPA-PSK or WPA2-PSK
 * @remark To see the maximum length of PSK passphrase key.
 * @see wlan_auth_info_t
 */
typedef struct
{
	/** key value for WPA-PSK or WPA2-PSK */
	char pskKey[NETPM_WLAN_MAX_PSK_PASSPHRASE_LEN + 1];
} wlan_psk_info_t;


/**
 * Below structure is used by WEP
 * @remark To see the maximum length of WEP key.
 * @see wlan_auth_info_t
 */
typedef struct
{
	/** key value for WEP */
	char wepKey[NETPM_WLAN_MAX_WEP_KEY_LEN + 1];
} wlan_wep_info_t;


/**
 * Below structure is used by EAP
 * @see wlan_auth_info_t
 */
typedef struct
{
	/** User name */
	char username[NETPM_WLAN_USERNAME_LEN+1];
	/** Password */
	char password[NETPM_WLAN_PASSWORD_LEN+1];

	/**
	 * Following fields are mandatory for EAP-TLS,
	 * Optional for EAP-TTLS and EAP-PEAP
	 */
	/**
	 * For EAP-TTLS and EAP-PEAP only ca_cert_filename[] can also be provided
	 */
	char ca_cert_filename[NETPM_WLAN_CA_CERT_FILENAME_LEN+1]; /* Used to authenticate server */
	/** client certificate file name */
	char client_cert_filename[NETPM_WLAN_CLIENT_CERT_FILENAME_LEN+1];
	/** private key file name */
	char private_key_filename[NETPM_WLAN_PRIVATE_KEY_FILENAME_LEN+1];
	/** private key password */
	char private_key_passwd[NETPM_WLAN_PRIVATE_KEY_PASSWD_LEN+1];

	/** eap type */
	wlan_eap_type_t eap_type;
	/** eap phase2 authentication type */
	wlan_eap_auth_type_t eap_auth;
} wlan_eap_info_t;


/**
 * At any point of time only one security mechanism is supported
 * @see wlan_security_info_t
 */
typedef union
{
	/** Wep Authentication */
	wlan_wep_info_t wep;
	/** psk Authentication */
	wlan_psk_info_t psk;
	/** eap Authentication */
	wlan_eap_info_t eap;
} wlan_auth_info_t;


/**
 * This is main security information structure
 * @see net_pm_wlan_profile_info_t
 */
typedef struct
{
	/** security mode type */
	wlan_security_mode_type_t sec_mode;
	/** encryption mode type */
	wlan_encryption_mode_type_t enc_mode;
	/** authentication information */
	wlan_auth_info_t authentication;
	/** If WPS is supported, then this property will be set to TRUE */
	char wps_support;
} wlan_security_info_t;


/**
 * AP Profile information
 */
typedef struct
{
	/** ESSID */
	char essid[NET_WLAN_ESSID_LEN+1];
	/** Basic service set identification */
	char bssid[NET_MAX_MAC_ADDR_LEN+1];
	/**  Strength : between 0 and 100 */
	unsigned char Strength;
	/** Frequency band(MHz) */
	unsigned int frequency;
	/** Maximum speed of the line(bps) */
	unsigned int max_rate;
	/** If a passphrase has been set already or if no
		passphrase is needed, then this property will
		be set to FALSE. */
	char PassphraseRequired;
	/** Infrastucture / ad-hoc / auto mode */
	wlan_connection_mode_type_t wlan_mode;
	/** Security mode and authentication info */
	wlan_security_info_t security_info;

	/** network information */
	net_dev_info_t net_info;
} net_wifi_profile_info_t;

/**
 * \}
 */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __NETPM_WLAN_H__ */


