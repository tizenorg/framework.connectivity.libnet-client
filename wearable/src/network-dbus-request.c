/*
 * Network Client Library
 *
 * Copyright 2012 Samsung Electronics Co., Ltd
 *
 * Licensed under the Flora License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.tizenopensource.org/license
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <arpa/inet.h>
#include <netinet/in.h>

#include "network-internal.h"
#include "network-dbus-request.h"

/*****************************************************************************
 * 	Macros and Typedefs
 *****************************************************************************/
#define DBUS_REPLY_TIMEOUT (120 * 1000)

/*****************************************************************************
 * 	Extern Variables
 *****************************************************************************/
extern __thread network_info_t NetworkInfo;
extern __thread network_request_table_t request_table[NETWORK_REQUEST_TYPE_MAX];

static int __net_error_string_to_enum(const char* error)
{
	NETWORK_LOG(NETWORK_HIGH, "Passed error value [%s]\n", error);

	if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".NoReply"))
		return NET_ERR_TIME_OUT;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".Failed"))
		return NET_ERR_UNKNOWN;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".UnknownMethod"))
		return NET_ERR_UNKNOWN_METHOD;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".InvalidArguments"))
		return NET_ERR_INVALID_PARAM;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".PermissionDenied"))
		return NET_ERR_ACCESS_DENIED;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".PassphraseRequired"))
		return NET_ERR_INVALID_OPERATION;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".NotRegistered"))
		return NET_ERR_INVALID_OPERATION;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".NotUnique"))
		return NET_ERR_INVALID_OPERATION;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".NotSupported"))
		return NET_ERR_NOT_SUPPORTED;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".NotImplemented"))
		return NET_ERR_NOT_SUPPORTED;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".NotFound"))
		return NET_ERR_NOT_SUPPORTED;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".NoCarrier"))
		return NET_ERR_NOT_SUPPORTED;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".InProgress"))
		return NET_ERR_IN_PROGRESS;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".AlreadyExists"))
		return NET_ERR_INVALID_OPERATION;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".AlreadyEnabled"))
		return NET_ERR_INVALID_OPERATION;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".AlreadyDisabled"))
		return NET_ERR_INVALID_OPERATION;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".AlreadyConnected"))
		return NET_ERR_ACTIVE_CONNECTION_EXISTS;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".NotConnected"))
		return NET_ERR_NO_ACTIVE_CONNECTIONS;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".OperationAborted"))
		return NET_ERR_OPERATION_ABORTED;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".OperationTimeout"))
		return NET_ERR_TIME_OUT;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".InvalidService"))
		return NET_ERR_NO_SERVICE;
	else if (NULL != strstr(error, CONNMAN_ERROR_INTERFACE ".InvalidProperty"))
		return NET_ERR_INVALID_OPERATION;
	return NET_ERR_UNKNOWN;
}

static int __net_netconfig_error_string_to_enum(const char* error)
{
	NETWORK_LOG(NETWORK_HIGH, "Passed error value [%s]\n", error);

	if (error == NULL)
		return NET_ERR_UNKNOWN;

	if (NULL != strstr(error, ".WifiDriverFailed"))
		return NET_ERR_WIFI_DRIVER_FAILURE;
	else if (NULL != strstr(error, ".PermissionDenied"))
		return NET_ERR_SECURITY_RESTRICTED;
	else if (NULL != strstr(error, ".InProgress"))
		return NET_ERR_WIFI_DRIVER_LOAD_INPROGRESS;
	return NET_ERR_UNKNOWN;
}

static void __net_open_connection_reply(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	__NETWORK_FUNC_ENTER__;

	net_event_info_t event_data = { 0, };
	net_profile_info_t prof_info;
	network_request_table_t *open_info =
			&request_table[NETWORK_REQUEST_TYPE_OPEN_CONNECTION];
	network_request_table_t *wps_info =
			&request_table[NETWORK_REQUEST_TYPE_ENROLL_WPS];
	GDBusConnection *conn = NULL;
	GError *error = NULL;
	net_err_t Error = NET_ERR_NONE;

	NETWORK_LOG(NETWORK_LOW, "__net_open_connection_reply() called\n");

	conn = G_DBUS_CONNECTION (source_object);
	g_dbus_connection_call_finish(conn, res, &error);

	if (error != NULL) {
		NETWORK_LOG(NETWORK_LOW, "error msg - [%s]\n", error->message);
		Error = __net_error_string_to_enum(error->message);
		g_error_free(error);
	} else
		NETWORK_LOG(NETWORK_LOW, "error msg is NULL\n");

	if (Error == NET_ERR_NONE)
		goto done;

	NETWORK_LOG(NETWORK_ERROR, "Connection open failed. Error [%d]\n", Error);

	if (open_info->flag == TRUE) {
		net_device_t device_type =
				_net_get_tech_type_from_path(open_info->ProfileName);

		if (Error == NET_ERR_IN_PROGRESS && device_type == NET_DEVICE_CELLULAR)
			goto done;

		g_strlcpy(event_data.ProfileName, open_info->ProfileName,
				NET_PROFILE_NAME_LEN_MAX + 1);
		memset(open_info, 0, sizeof(network_request_table_t));

		event_data.Error = Error;

		if (Error == NET_ERR_ACTIVE_CONNECTION_EXISTS) {
			Error = net_get_profile_info(event_data.ProfileName, &prof_info);

			if (device_type == NET_DEVICE_CELLULAR)
				event_data.Error = NET_ERR_NONE;

			if (Error != NET_ERR_NONE) {
				NETWORK_LOG(NETWORK_ERROR, "Fail to get profile info [%s]\n",
						_net_print_error(Error));

				event_data.Datalength = 0;
				event_data.Data = NULL;
			} else {
				event_data.Datalength = sizeof(net_profile_info_t);
				event_data.Data = &prof_info;
			}
		}

		event_data.Event = NET_EVENT_OPEN_RSP;
		NETWORK_LOG(NETWORK_LOW, "Sending NET_EVENT_OPEN_RSP Error = %s\n",
				_net_print_error(event_data.Error));
	} else if (wps_info->flag == TRUE) {
		g_strlcpy(event_data.ProfileName, wps_info->ProfileName,
				NET_PROFILE_NAME_LEN_MAX + 1);
		memset(wps_info, 0, sizeof(network_request_table_t));

		event_data.Error = Error;

		if (Error == NET_ERR_ACTIVE_CONNECTION_EXISTS) {
			Error = net_get_profile_info(event_data.ProfileName, &prof_info);
			if (Error != NET_ERR_NONE) {
				NETWORK_LOG(NETWORK_ERROR, "Fail to get profile info [%s]\n",
						_net_print_error(Error));

				event_data.Datalength = 0;
				event_data.Data = NULL;
			} else {
				event_data.Datalength = sizeof(net_profile_info_t);
				event_data.Data = &prof_info;
			}
		}

		event_data.Event = NET_EVENT_WIFI_WPS_RSP;

		NETWORK_LOG(NETWORK_LOW, "Sending NET_EVENT_WIFI_WPS_RSP Error = %s\n",
				_net_print_error(event_data.Error));
	} else {
		_net_dbus_pending_call_unref();

		__NETWORK_FUNC_EXIT__;
		return;
	}

done:
	_net_dbus_pending_call_unref();

	_net_client_callback(&event_data);

	__NETWORK_FUNC_EXIT__;
}

static void __net_close_connection_reply(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	__NETWORK_FUNC_ENTER__;

	NETWORK_LOG(NETWORK_LOW, "__net_close_connection_reply() called\n");

	net_event_info_t event_data = { 0, };
	network_request_table_t *close_info =
			&request_table[NETWORK_REQUEST_TYPE_CLOSE_CONNECTION];

	GDBusConnection *conn = NULL;
	GError *error = NULL;
	net_err_t Error = NET_ERR_NONE;

	conn = G_DBUS_CONNECTION (source_object);
	g_dbus_connection_call_finish(conn, res, &error);
	if (error != NULL) {
		Error = __net_netconfig_error_string_to_enum(error->message);
		g_error_free(error);
	}

	if (Error == NET_ERR_NONE)
		goto done;

	NETWORK_LOG(NETWORK_ERROR, "Connection close failed. Error [%d]\n", Error);

	if (close_info->flag == TRUE) {
		net_device_t device_type =
				_net_get_tech_type_from_path(close_info->ProfileName);

		if (Error == NET_ERR_ACTIVE_CONNECTION_EXISTS &&
				device_type == NET_DEVICE_CELLULAR)
			Error = NET_ERR_NONE;

		g_strlcpy(event_data.ProfileName, close_info->ProfileName,
				NET_PROFILE_NAME_LEN_MAX + 1);
		memset(close_info, 0, sizeof(network_request_table_t));

		event_data.Error = Error;
		event_data.Datalength = 0;
		event_data.Data = NULL;
		event_data.Event = NET_EVENT_CLOSE_RSP;

		NETWORK_LOG(NETWORK_LOW, "Sending NET_EVENT_CLOSE_RSP Error = %s\n",
				_net_print_error(event_data.Error));
	} else {
		_net_dbus_pending_call_unref();

		__NETWORK_FUNC_EXIT__;
		return;
	}

done:
	_net_dbus_pending_call_unref();

	_net_client_callback(&event_data);

	__NETWORK_FUNC_EXIT__;
}

static void __net_wifi_power_reply(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	__NETWORK_FUNC_ENTER__;

	GDBusConnection *conn = NULL;
	GError *error = NULL;
	net_err_t Error = NET_ERR_NONE;
	net_event_info_t event_data = { 0, };

	NETWORK_LOG(NETWORK_LOW, "__net_wifi_power_reply() called\n");

	conn = G_DBUS_CONNECTION (source_object);
	g_dbus_connection_call_finish(conn, res, &error);
	if (error != NULL) {
		Error = __net_netconfig_error_string_to_enum(error->message);
		g_error_free(error);
	}

	if (Error != NET_ERR_NONE) {
		NETWORK_LOG(NETWORK_ERROR,
				"Wi-Fi power operation failed. Error [%d]\n", Error);

		if (Error != NET_ERR_WIFI_DRIVER_LOAD_INPROGRESS) {
			if (request_table[NETWORK_REQUEST_TYPE_WIFI_POWER].flag == TRUE) {
				memset(&request_table[NETWORK_REQUEST_TYPE_WIFI_POWER],
						0, sizeof(network_request_table_t));

				event_data.Event = NET_EVENT_WIFI_POWER_RSP;

				NETWORK_LOG(NETWORK_LOW,
						"Sending NET_EVENT_WIFI_POWER_RSP Wi-Fi: %d Error = %d\n",
						NetworkInfo.wifi_state, Error);

				event_data.Datalength = sizeof(net_wifi_state_t);
				event_data.Data = &(NetworkInfo.wifi_state);
				event_data.Error = Error;
			}
		}
	} else {
		_net_dbus_pending_call_unref();

		__NETWORK_FUNC_EXIT__;
		return;
	}

	_net_dbus_pending_call_unref();

	_net_client_callback(&event_data);

	__NETWORK_FUNC_EXIT__;
}

static void __net_specific_scan_wifi_reply(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	__NETWORK_FUNC_ENTER__;

	GDBusConnection *conn = NULL;
	GError *error = NULL;
	net_err_t Error = NET_ERR_NONE;
	net_event_info_t event_data = { 0, };

	conn = G_DBUS_CONNECTION (source_object);
	g_dbus_connection_call_finish(conn, res, &error);
	if (error != NULL) {
		Error = __net_netconfig_error_string_to_enum(error->message);
		g_error_free(error);
	}

	if (Error != NET_ERR_NONE)
		NETWORK_LOG(NETWORK_ERROR, "Find hidden AP failed. Error [%d]\n", Error);
	else
		NETWORK_LOG(NETWORK_LOW, "Hidden AP found\n");

	if (request_table[NETWORK_REQUEST_TYPE_SPECIFIC_SCAN].flag == TRUE) {
		if (NET_ERR_NONE != Error) {
			/* An error occurred.
			 * So lets reset specific scan request entry in the request table */
			memset(&request_table[NETWORK_REQUEST_TYPE_SPECIFIC_SCAN],
					0, sizeof(network_request_table_t));
		}

		event_data.Event = NET_EVENT_SPECIFIC_SCAN_RSP;

		NETWORK_LOG(NETWORK_LOW,
				"Sending NET_EVENT_SPECIFIC_SCAN_RSP Wi-Fi: %d Error = %d\n",
				NetworkInfo.wifi_state, Error);

		event_data.Datalength = sizeof(net_wifi_state_t);
		event_data.Data = &(NetworkInfo.wifi_state);
		event_data.Error = Error;
	} else {
		_net_dbus_pending_call_unref();

		__NETWORK_FUNC_EXIT__;
		return;
	}

	_net_dbus_pending_call_unref();

	_net_client_callback(&event_data);

	__NETWORK_FUNC_EXIT__;
}

static void __net_wps_scan_wifi_reply(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	__NETWORK_FUNC_ENTER__;

	GDBusConnection *conn = NULL;
	GError *error = NULL;
	net_err_t Error = NET_ERR_NONE;
	net_event_info_t event_data = { 0, };

	conn = G_DBUS_CONNECTION (source_object);
	g_dbus_connection_call_finish(conn, res, &error);
	if (error != NULL) {
		Error = __net_netconfig_error_string_to_enum(error->message);
		g_error_free(error);
	}

	if (Error != NET_ERR_NONE) {
		NETWORK_LOG(NETWORK_ERROR, "WPS scan failed. Error [%d]\n", Error);

		if (request_table[NETWORK_REQUEST_TYPE_WPS_SCAN].flag == TRUE) {
			memset(&request_table[NETWORK_REQUEST_TYPE_WPS_SCAN],
					0, sizeof(network_request_table_t));

			event_data.Event = NET_EVENT_WPS_SCAN_IND;
			event_data.Datalength = 0;
			event_data.Data = NULL;
			event_data.Error = Error;

			_net_dbus_pending_call_unref();

			_net_client_callback(&event_data);

			__NETWORK_FUNC_EXIT__;
			return;
		}
	} else
		NETWORK_LOG(NETWORK_LOW, "WPS scan succeed\n");

	_net_dbus_pending_call_unref();

	__NETWORK_FUNC_EXIT__;
}

static void __net_set_passpoint_reply(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	__NETWORK_FUNC_ENTER__;

	GDBusConnection *conn = NULL;
	GError *error = NULL;
	net_err_t Error = NET_ERR_NONE;

	conn = G_DBUS_CONNECTION (source_object);
	g_dbus_connection_call_finish(conn, res, &error);
	if (error != NULL) {
		Error = __net_netconfig_error_string_to_enum(error->message);
		g_error_free(error);
	}

	if (Error != NET_ERR_NONE)
		NETWORK_LOG(NETWORK_ERROR, "set passpoint failed. Error [%d]\n", Error);
	else
		NETWORK_LOG(NETWORK_LOW, "set passpoint succeed\n");

	_net_dbus_pending_call_unref();

	__NETWORK_FUNC_EXIT__;
}

static void __net_set_default_reply(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
	__NETWORK_FUNC_ENTER__;

	int rv;
	net_event_info_t event_data = { 0, };
	GDBusConnection *conn = NULL;
	GVariant *dbus_result;
	GError *error = NULL;
	net_err_t Error = NET_ERR_NONE;

	NETWORK_LOG(NETWORK_LOW, "__net_set_default_reply() called\n");

	conn = G_DBUS_CONNECTION (source_object);
	dbus_result = g_dbus_connection_call_finish(conn, res, &error);
	if (error != NULL) {
		Error = __net_netconfig_error_string_to_enum(error->message);
		g_error_free(error);

		NETWORK_LOG(NETWORK_ERROR, "Error code: [%d]\n", Error);
	}

	if (request_table[NETWORK_REQUEST_TYPE_SET_DEFAULT].flag == TRUE) {
		memset(&request_table[NETWORK_REQUEST_TYPE_SET_DEFAULT],
						0, sizeof(network_request_table_t));
		event_data.Event = NET_EVENT_CELLULAR_SET_DEFAULT_RSP;

		if (Error == NET_ERR_NONE) {
			g_variant_get(dbus_result, "(b)", &rv);

			NETWORK_LOG(NETWORK_LOW, "Reply: %s\n", rv ? "TRUE" : "FALSE");

			if (rv)
				event_data.Error = NET_ERR_NONE;
			else
				event_data.Error = NET_ERR_UNKNOWN;
		} else
			event_data.Error = Error;

		NETWORK_LOG(NETWORK_LOW, "Sending NET_EVENT_CELLULAR_SET_DEFAULT_RSP Error = %s\n",
				_net_print_error(event_data.Error));
	} else {
		_net_dbus_pending_call_unref();

		__NETWORK_FUNC_EXIT__;
		return;
	}

	_net_dbus_pending_call_unref();

	_net_client_callback(&event_data);

	__NETWORK_FUNC_EXIT__;
}

static char *__net_make_group_name(const char *ssid,
		const char *net_mode, const char *sec)
{
	char *buf = NULL;
	char *pbuf = NULL;
	const char *hidden_str = "hidden";
	const char *g_sec;
	char buf_tmp[32] = { 0, };
	int i;
	int ssid_len = 0;
	int actual_len = 0;

	if (net_mode == NULL || sec == NULL)
		return NULL;

	if (NULL != ssid) {
		ssid_len = strlen(ssid);
		actual_len = ssid_len * 2;
	} else {
		ssid_len = strlen(hidden_str);
		actual_len = ssid_len;
	}

	if (g_strcmp0(net_mode, "managed") != 0)
		return NULL;

	if (!g_strcmp0(sec, "wpa") || !g_strcmp0(sec, "rsn"))
		g_sec = "psk";
	else
		g_sec = sec;

	buf = g_try_malloc0(actual_len + strlen(net_mode) + strlen(sec) + 3);
	if (buf == NULL)
		return NULL;

	if (NULL != ssid) {
		pbuf = buf;
		for (i = 0; i < ssid_len; i++) {
			g_snprintf(pbuf, 3, "%02x", ssid[i]);
			pbuf += 2;
		}
	} else
		g_strlcat(buf, hidden_str,
				actual_len + strlen(net_mode) + strlen(sec) + 3);

	g_snprintf(buf_tmp, 32, "_%s_%s", net_mode, g_sec);
	g_strlcat(buf, buf_tmp,
			actual_len + strlen(net_mode) + strlen(sec) + 3);

	NETWORK_LOG(NETWORK_HIGH, "Group name: %s\n", buf);

	return buf;
}

static int __net_dbus_set_agent_field_and_connect(
		const char *key, const char *value, const char *profilename)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	GVariant *params = NULL;
	GVariantBuilder *builder;

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add(builder, "{sv}", key, g_variant_new_string(value));

	params = g_variant_new("(o@a{sv})",
			profilename, g_variant_builder_end(builder));
	g_variant_builder_unref(builder);

	Error = _net_invoke_dbus_method_nonblock(NETCONFIG_SERVICE,
			NETCONFIG_WIFI_PATH,
			CONNMAN_AGENT_INTERFACE, "SetField", params,
			__net_open_connection_reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

/*****************************************************************************
 * 	Global Functions Definition
 *****************************************************************************/
GVariant *_net_invoke_dbus_method(const char* dest, const char* path,
		char* interface_name, char* method,
		GVariant *params, int* dbus_error)
{
	__NETWORK_FUNC_ENTER__;

	GError *error = NULL;
	GVariant *reply = NULL;
	*dbus_error = NET_ERR_NONE;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL) {
		NETWORK_LOG(NETWORK_ERROR, "GDBusconnection is NULL\n");
		return reply;
	}

	reply = g_dbus_connection_call_sync(connection,
										dest,
										path,
										interface_name,
										method,
										params,
										NULL,
										G_DBUS_CALL_FLAGS_NONE,
										DBUS_REPLY_TIMEOUT,
										_net_dbus_get_gdbus_cancellable(),
										&error);
	if (reply == NULL) {
		if (error != NULL) {
			SECURE_NETWORK_LOG(NETWORK_ERROR,
						"g_dbus_connection_call_sync() failed."
						"error [%d: %s]\n", error->code, error->message);
			g_error_free(error);
		} else {
			NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed.\n");
		}

		*dbus_error = NET_ERR_UNKNOWN;

		__NETWORK_FUNC_EXIT__;
		return NULL;
	}

	__NETWORK_FUNC_EXIT__;
	return reply;
}

int _net_invoke_dbus_method_nonblock(const char* dest, const char* path,
		char* interface_name, char* method, GVariant *params,
		GAsyncReadyCallback notify_func)
{
	__NETWORK_FUNC_ENTER__;

	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL) {
		NETWORK_LOG(NETWORK_ERROR, "GDBusconnection is NULL\n");
		return NET_ERR_APP_NOT_REGISTERED;
	}

	g_dbus_connection_call(connection,
							dest,
							path,
							interface_name,
							method,
							params,
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							DBUS_REPLY_TIMEOUT,
							_net_dbus_get_gdbus_cancellable(),
							(GAsyncReadyCallback) notify_func,
							NULL);

	if (notify_func != NULL)
		_net_dbus_pending_call_ref();

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}

int _net_dbus_open_connection(const char* profile_name)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	/* use DBus signal than reply pending because of performance reason */
	Error = _net_invoke_dbus_method_nonblock(CONNMAN_SERVICE, profile_name,
			CONNMAN_SERVICE_INTERFACE, "Connect", NULL,
			__net_open_connection_reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_close_connection(const char* profile_name)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	/* use DBus signal than reply pending because of performance reason */
	Error = _net_invoke_dbus_method_nonblock(CONNMAN_SERVICE, profile_name,
			CONNMAN_SERVICE_INTERFACE, "Disconnect", NULL,
			__net_close_connection_reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_scan_request(void)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	/* use DBus signal than reply pending because of performance reason */
	Error = _net_invoke_dbus_method_nonblock(CONNMAN_SERVICE,
			CONNMAN_WIFI_TECHNOLOGY_PREFIX,
			CONNMAN_TECHNOLOGY_INTERFACE, "Scan", NULL, NULL);

	if (Error == NET_ERR_IN_PROGRESS)
		Error = NET_ERR_NONE;

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_set_default(const char* profile_name)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	Error = _net_invoke_dbus_method_nonblock(TELEPHONY_SERVICE,
			profile_name, TELEPHONY_PROFILE_INTERFACE,
			"SetDefaultConnection", NULL,
			__net_set_default_reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_set_bgscan_mode(net_wifi_background_scan_mode_t mode)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	GVariant *message = NULL;
	GVariant *params;

	char path[CONNMAN_MAX_BUFLEN] = NETCONFIG_WIFI_PATH;

	params = g_variant_new("(u)", mode);

	message = _net_invoke_dbus_method(NETCONFIG_SERVICE, path,
			NETCONFIG_WIFI_INTERFACE, "SetBgscan", params, &Error);

	if (Error != NET_ERR_NONE)
		NETWORK_LOG(NETWORK_ERROR, "_net_invoke_dbus_method failed\n");

	if (message != NULL)
		g_variant_unref(message);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_get_technology_state(network_tech_state_info_t* tech_state)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	GVariant *message = NULL;

	if ((tech_state == NULL) || (strlen(tech_state->technology) == 0)) {
		NETWORK_LOG(NETWORK_ERROR, "Invalid parameter\n");
		__NETWORK_FUNC_EXIT__;
		return NET_ERR_INVALID_PARAM;
	}

	message = _net_invoke_dbus_method(CONNMAN_SERVICE,
			CONNMAN_MANAGER_PATH, CONNMAN_MANAGER_INTERFACE,
			"GetTechnologies", NULL, &Error);
	if (message == NULL) {
		NETWORK_LOG(NETWORK_ERROR, "Failed to get technology info\n");
		goto done;
	}

	Error = _net_get_tech_state(message, tech_state);

	g_variant_unref(message);

done:
	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_get_network_status(net_device_t device_type, net_cm_network_status_t* network_status)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	network_tech_state_info_t tech_state = {{0,},};

	if (device_type == NET_DEVICE_WIFI)
		g_strlcpy(tech_state.technology, "wifi", NET_TECH_LENGTH_MAX);
	else if (device_type == NET_DEVICE_CELLULAR)
		g_strlcpy(tech_state.technology, "cellular", NET_TECH_LENGTH_MAX);
	else if (device_type == NET_DEVICE_ETHERNET)
		g_strlcpy(tech_state.technology, "ethernet", NET_TECH_LENGTH_MAX);
	else if (device_type == NET_DEVICE_BLUETOOTH)
		g_strlcpy(tech_state.technology, "bluetooth", NET_TECH_LENGTH_MAX);
	else {
		Error = NET_ERR_INVALID_PARAM;
		goto done;
	}

	Error = _net_dbus_get_technology_state(&tech_state);
	if (Error != NET_ERR_NONE) {
		NETWORK_LOG(NETWORK_ERROR,
			"_net_dbus_get_technology_state() failed. Error [%s]\n",
			_net_print_error(Error));
		goto done;
	}

	if (tech_state.Connected == TRUE)
		*network_status = NET_STATUS_AVAILABLE;
	else
		*network_status = NET_STATUS_UNAVAILABLE;

done:
	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_get_tech_status(net_device_t device_type, net_tech_info_t* tech_status)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	network_tech_state_info_t tech_state = {{0,},};

	if (device_type == NET_DEVICE_WIFI)
		g_strlcpy(tech_state.technology, "wifi", NET_TECH_LENGTH_MAX);
	else if (device_type == NET_DEVICE_CELLULAR)
		g_strlcpy(tech_state.technology, "cellular", NET_TECH_LENGTH_MAX);
	else if (device_type == NET_DEVICE_ETHERNET)
		g_strlcpy(tech_state.technology, "ethernet", NET_TECH_LENGTH_MAX);
	else if (device_type == NET_DEVICE_BLUETOOTH)
		g_strlcpy(tech_state.technology, "bluetooth", NET_TECH_LENGTH_MAX);
	else {
		Error = NET_ERR_INVALID_PARAM;
		goto done;
	}

	Error = _net_dbus_get_technology_state(&tech_state);
	if (Error != NET_ERR_NONE) {
		NETWORK_LOG(NETWORK_ERROR,
			"_net_dbus_get_technology_state() failed. Error [%s]\n",
			_net_print_error(Error));
		goto done;
	}

	if (tech_state.Powered == TRUE)
		tech_status->powered = TRUE;
	else
		tech_status->powered = FALSE;

	if (tech_state.Connected == TRUE)
		tech_status->connected = TRUE;
	else
		tech_status->connected = FALSE;

done:
	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_get_statistics(net_device_t device_type, net_statistics_type_e statistics_type, unsigned long long *size)
{
	net_err_t Error = NET_ERR_NONE;
	GVariant *message = NULL;
	char *method = NULL;

	if (device_type == NET_DEVICE_WIFI) {
		switch (statistics_type) {
		case NET_STATISTICS_TYPE_LAST_RECEIVED_DATA:
			method = "GetWifiLastRxBytes";
			break;
		case NET_STATISTICS_TYPE_LAST_SENT_DATA:
			method = "GetWifiLastTxBytes";
			break;
		case NET_STATISTICS_TYPE_TOTAL_RECEIVED_DATA:
			method = "GetWifiTotalRxBytes";
			break;
		case NET_STATISTICS_TYPE_TOTAL_SENT_DATA:
			method = "GetWifiTotalTxBytes";
			break;
		default:
			NETWORK_LOG(NETWORK_ERROR, "Invalid profile name\n");
			return NET_ERR_INVALID_PARAM;
		}
	} else {
		NETWORK_LOG(NETWORK_ERROR, "Invalid profile name\n");
		return NET_ERR_INVALID_PARAM;
	}

	message = _net_invoke_dbus_method(
			NETCONFIG_SERVICE, NETCONFIG_STATISTICS_PATH,
			NETCONFIG_STATISTICS_INTERFACE, method, NULL, &Error);
	if (message == NULL) {
		NETWORK_LOG(NETWORK_ERROR, "Failed to get service properties\n");
		return Error;
	}

	g_variant_get(message, "(t)", size);

	NETWORK_LOG(NETWORK_HIGH, "success [%s] statistics size: [%llu]\n", method, *size);
	g_variant_unref(message);

	return Error;
}

int _net_dbus_set_statistics(net_device_t device_type, net_statistics_type_e statistics_type)
{
	net_err_t Error = NET_ERR_NONE;
	GVariant *message = NULL;
	char *method = NULL;

	if (device_type == NET_DEVICE_CELLULAR) {
		switch (statistics_type) {
		case NET_STATISTICS_TYPE_LAST_RECEIVED_DATA:
			method = "ResetCellularLastRxBytes";
			break;
		case NET_STATISTICS_TYPE_LAST_SENT_DATA:
			method = "ResetCellularLastTxBytes";
			break;
		case NET_STATISTICS_TYPE_TOTAL_RECEIVED_DATA:
			method = "ResetCellularTotalRxBytes";
			break;
		case NET_STATISTICS_TYPE_TOTAL_SENT_DATA:
			method = "ResetCellularTotalTxBytes";
			break;
		default:
			NETWORK_LOG(NETWORK_ERROR, "Invalid profile name\n");
			return NET_ERR_INVALID_PARAM;
		}
	} else if (device_type == NET_DEVICE_WIFI) {
		switch (statistics_type) {
		case NET_STATISTICS_TYPE_LAST_RECEIVED_DATA:
			method = "ResetWifiLastRxBytes";
			break;
		case NET_STATISTICS_TYPE_LAST_SENT_DATA:
			method = "ResetWifiLastTxBytes";
			break;
		case NET_STATISTICS_TYPE_TOTAL_RECEIVED_DATA:
			method = "ResetWifiTotalRxBytes";
			break;
		case NET_STATISTICS_TYPE_TOTAL_SENT_DATA:
			method = "ResetWifiTotalTxBytes";
			break;
		default:
			NETWORK_LOG(NETWORK_ERROR, "Invalid profile name\n");
			return NET_ERR_INVALID_PARAM;
		}
	} else {
		NETWORK_LOG(NETWORK_ERROR, "Invalid profile name\n");
		return NET_ERR_INVALID_PARAM;
	}

	message = _net_invoke_dbus_method(
			NETCONFIG_SERVICE, NETCONFIG_STATISTICS_PATH,
			NETCONFIG_STATISTICS_INTERFACE, method, NULL, &Error);
	if (message == NULL) {
		NETWORK_LOG(NETWORK_ERROR, "Failed to get service properties\n");
		return Error;
	}

	NETWORK_LOG(NETWORK_HIGH, "reset [%s] statistics success\n", method);
	g_variant_unref(message);

	return Error;
}

int _net_dbus_get_state(char* state)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	GVariant *message = NULL;
	GVariant *value = NULL;
	GVariantIter *iter = NULL;
	gchar *key = NULL;
	const gchar *net_state = NULL;

	message = _net_invoke_dbus_method(
			CONNMAN_SERVICE, CONNMAN_MANAGER_PATH,
			CONNMAN_MANAGER_INTERFACE, "GetProperties", NULL, &Error);
	if (message == NULL) {
		NETWORK_LOG(NETWORK_ERROR, "Failed to get service properties\n");

		__NETWORK_FUNC_EXIT__;
		return Error;
	}

	g_variant_get(message, "(a{sv})", &iter);

	while (g_variant_iter_loop(iter, "{sv}", &key, &value)) {
		if (g_strcmp0(key, "State") == 0) {
			net_state = g_variant_get_string(value, NULL);
			g_strlcpy(state, net_state, CONNMAN_STATE_STRLEN);
			g_variant_unref(value);
			g_free(key);
			break;
		}
	}

	NETWORK_LOG(NETWORK_HIGH, "State: %s\n", state);

	g_variant_iter_free(iter);
	g_variant_unref(message);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_set_eap_config_fields_and_connect(
		const net_wifi_connect_service_info_t *wifi_info,
		const char *profilename)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	GVariant *params = NULL;
	GVariantBuilder *builder;

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{ss}"));
	g_variant_builder_add(builder, "{ss}", CONNMAN_CONFIG_FIELD_TYPE, "wifi");

	if (wifi_info->ssid)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_NAME, wifi_info->ssid);

	if (wifi_info->eap_type)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_EAP_METHOD, wifi_info->eap_type);

	if (wifi_info->identity)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_IDENTITY, wifi_info->identity);

	if (wifi_info->password)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_PASSPHRASE, wifi_info->password);

	if (wifi_info->eap_auth &&
			g_strcmp0(wifi_info->eap_auth, "NONE") != 0)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_PHASE2, wifi_info->eap_auth);

	if (wifi_info->ca_cert_file)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_CA_CERT_FILE, wifi_info->ca_cert_file);

	if (wifi_info->client_cert_file)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_CLIENT_CERT_FILE,
				wifi_info->client_cert_file);

	if (wifi_info->private_key_file)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_PVT_KEY_FILE,
				wifi_info->private_key_file);

	if (wifi_info->private_key_password)
		g_variant_builder_add(builder, "{ss}",
				CONNMAN_CONFIG_FIELD_PVT_KEY_PASSPHRASE,
				wifi_info->private_key_password);

	params = g_variant_new("(o@a{ss})",
			profilename, g_variant_builder_end(builder));
	g_variant_builder_unref(builder);

	Error = _net_invoke_dbus_method_nonblock(NETCONFIG_SERVICE,
			NETCONFIG_WIFI_PATH,
			NETCONFIG_WIFI_INTERFACE, "CreateConfig", params,
			__net_open_connection_reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_set_agent_passphrase_and_connect(
		const char *passphrase, const char *profilename)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	if (NULL == passphrase || strlen(passphrase) <= 0 || NULL == profilename) {
		NETWORK_LOG(NETWORK_ERROR, "Invalid parameter\n");
		return NET_ERR_INVALID_PARAM;
	}

	Error = __net_dbus_set_agent_field_and_connect(
			NETCONFIG_AGENT_FIELD_PASSPHRASE,
			passphrase,
			profilename);
	if (NET_ERR_NONE != Error) {
		NETWORK_LOG(NETWORK_ERROR, "Configuration failed(%d)\n", Error);
		return Error;
	}

	NETWORK_LOG(NETWORK_HIGH, "Successfully configured\n");

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_set_agent_fields_and_connect(const char *ssid,
		const char *passphrase, const char *profilename)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	/* If OPEN network, passphrase can be NULL */
	if (NULL == ssid || strlen(ssid) <= 0 || NULL == profilename) {
		NETWORK_LOG(NETWORK_ERROR, "Invalid parameter\n");
		return NET_ERR_INVALID_PARAM;
	}

	GVariant *params = NULL;
	GVariantBuilder *builder;

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));
	g_variant_builder_add(builder, "{sv}", NETCONFIG_AGENT_FIELD_SSID,
			g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, ssid,
					strlen(ssid), sizeof(guchar)));

	if (passphrase)
		g_variant_builder_add(builder, "{sv}", NETCONFIG_AGENT_FIELD_PASSPHRASE,
				g_variant_new_string(passphrase));

	params = g_variant_new("(o@a{sv})",
			profilename, g_variant_builder_end(builder));
	g_variant_builder_unref(builder);

	Error = _net_invoke_dbus_method_nonblock(NETCONFIG_SERVICE,
			NETCONFIG_WIFI_PATH,
			CONNMAN_AGENT_INTERFACE, "SetField", params,
			__net_open_connection_reply);
	if (NET_ERR_NONE != Error) {
		NETWORK_LOG(NETWORK_ERROR, "Configuration failed(%d)\n", Error);
		return Error;
	}

	NETWORK_LOG(NETWORK_HIGH, "Successfully configured\n");

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_set_agent_wps_pbc_and_connect(const char *profilename)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	Error = __net_dbus_set_agent_field_and_connect(
			NETCONFIG_AGENT_FIELD_WPS_PBC,
			"enable",
			profilename);
	if (NET_ERR_NONE != Error) {
		NETWORK_LOG(NETWORK_ERROR, "PBC configuration failed(%d)\n", Error);
		return Error;
	}

	NETWORK_LOG(NETWORK_HIGH, "Successfully PBC configured\n");

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_set_agent_wps_pin_and_connect(
		const char *wps_pin, const char *profilename)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	if (NULL == wps_pin || strlen(wps_pin) <= 0) {
		NETWORK_LOG(NETWORK_ERROR, "Invalid param \n");
		return NET_ERR_INVALID_PARAM;
	}

	Error = __net_dbus_set_agent_field_and_connect(
			NETCONFIG_AGENT_FIELD_WPS_PIN,
			wps_pin,
			profilename);
	if (NET_ERR_NONE != Error) {
		NETWORK_LOG(NETWORK_ERROR, "PIN configuration failed(%d)\n", Error);
		return Error;
	}

	NETWORK_LOG(NETWORK_HIGH, "Successfully PIN configured\n");

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_connect_service(const net_wifi_connect_service_info_t *wifi_connection_info)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	char *grp_name = NULL;
	int profile_count = 0;
	net_profile_info_t* profile_info = NULL;
	int i = 0;

	/* Get group name with prefix 'ssid' in hex */
	grp_name = __net_make_group_name(wifi_connection_info->is_hidden == TRUE ?
				NULL : wifi_connection_info->ssid,
			wifi_connection_info->mode,
			wifi_connection_info->security);
	if (NULL == grp_name) {
		NETWORK_LOG(NETWORK_ERROR, "Failed to make a group name\n");

		__NETWORK_FUNC_EXIT__;
		return NET_ERR_UNKNOWN;
	}

	Error = _net_get_profile_list(NET_DEVICE_WIFI, &profile_info, &profile_count);
	if (NET_ERR_NONE != Error) {
		NETWORK_LOG(NETWORK_ERROR,
				"_net_get_profile_list fail. Error [%s]\n",
				_net_print_error(Error));

		goto error;
	}

	for (i = 0; i < profile_count; i++) {
		if (g_strstr_len(profile_info[i].ProfileName,
				NET_PROFILE_NAME_LEN_MAX+1, grp_name) != NULL) {
			NETWORK_LOG(NETWORK_ERROR, "Found profile %s\n",
					profile_info[i].ProfileName);

			if (profile_info[i].ProfileState == NET_STATE_TYPE_READY ||
					profile_info[i].ProfileState == NET_STATE_TYPE_ONLINE) {
				NETWORK_LOG(NETWORK_ERROR, "Already profile is connected\n");
				Error = NET_ERR_ACTIVE_CONNECTION_EXISTS;

				goto error;
			}

			break;
		}
	}

	if (i >= profile_count) {
		NETWORK_LOG(NETWORK_ERROR, "No matching profile found\n");
		Error = NET_ERR_NO_SERVICE;

		goto error;
	}

	if (wifi_connection_info->is_hidden == TRUE) {
		int target = 0;
		char *target_name = __net_make_group_name(wifi_connection_info->ssid,
				wifi_connection_info->mode,
				wifi_connection_info->security);

		for (target = 0; target < profile_count; target++) {
			if (g_strstr_len(profile_info[target].ProfileName,
					NET_PROFILE_NAME_LEN_MAX+1, target_name) != NULL) {
				g_strlcpy(request_table[NETWORK_REQUEST_TYPE_OPEN_CONNECTION].ProfileName,
						profile_info[target].ProfileName, NET_PROFILE_NAME_LEN_MAX+1);

				break;
			}
		}

		g_free(target_name);
	} else {
		g_strlcpy(request_table[NETWORK_REQUEST_TYPE_OPEN_CONNECTION].ProfileName,
				profile_info[i].ProfileName, NET_PROFILE_NAME_LEN_MAX+1);
	}

	if (g_strcmp0(wifi_connection_info->security, "ieee8021x") == 0) {
		/* Create the EAP config file */
		Error = _net_dbus_set_eap_config_fields_and_connect(
				wifi_connection_info, profile_info[i].ProfileName);
		if (NET_ERR_NONE != Error) {
			NETWORK_LOG(NETWORK_ERROR, "Fail to create eap_config\n");

			goto error;
		}
	} else if (wifi_connection_info->is_hidden == TRUE) {
		Error = _net_dbus_set_agent_fields_and_connect(
				wifi_connection_info->ssid,
				wifi_connection_info->passphrase,
				profile_info[i].ProfileName);
	} else if (g_strcmp0(wifi_connection_info->security, "none") != 0) {
		Error = _net_dbus_set_agent_passphrase_and_connect(
				wifi_connection_info->passphrase, profile_info[i].ProfileName);
		if (NET_ERR_NONE != Error) {
			NETWORK_LOG(NETWORK_ERROR, "Fail to set agent_passphrase\n");

			goto error;
		}
	} else
		Error = _net_dbus_open_connection(profile_info[i].ProfileName);

error:
	if (NET_ERR_NONE != Error) {
		NETWORK_LOG(NETWORK_ERROR,
				"Failed to request open connection, Error [%s]\n",
				_net_print_error(Error));

		memset(&request_table[NETWORK_REQUEST_TYPE_OPEN_CONNECTION],
				0, sizeof(network_request_table_t));
	} else
		NETWORK_LOG(NETWORK_HIGH, "Sent Connect request\n");

	NET_MEMFREE(profile_info);
	g_free(grp_name);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_set_profile_ipv4(net_profile_info_t* prof_info, char* profile_name)
{
	__NETWORK_FUNC_ENTER__;

	const char *manual_method = "manual";
	const char *dhcp_method = "dhcp";
	const char *off_method = "off";

	const char *prop_ipv4_configuration = "IPv4.Configuration";
	const char *prop_method = "Method";
	const char *prop_address = "Address";
	const char *prop_gateway = "Gateway";
	const char *prop_netmask = "Netmask";

	char ip_buffer[NETPM_IPV4_STR_LEN_MAX+1] = "";
	char netmask_buffer[NETPM_IPV4_STR_LEN_MAX+1] = "";
	char gateway_buffer[NETPM_IPV4_STR_LEN_MAX+1] = "";

	char *ipaddress = ip_buffer;
	char *netmask = netmask_buffer;
	char *gateway = gateway_buffer;

	GError *error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	GVariantBuilder *builder;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	if ((prof_info == NULL) || (profile_name == NULL) || (strlen(profile_name) == 0)) {
		NETWORK_LOG(NETWORK_ERROR, "Invalid argument\n");
		__NETWORK_FUNC_EXIT__;
		return NET_ERR_INVALID_PARAM;
	}

	g_strlcpy(ip_buffer,
			inet_ntoa(prof_info->ProfileInfo.Wlan.net_info.IpAddr.Data.Ipv4),
			NETPM_IPV4_STR_LEN_MAX + 1);

	g_strlcpy(netmask_buffer,
			inet_ntoa(prof_info->ProfileInfo.Wlan.net_info.SubnetMask.Data.Ipv4),
			NETPM_IPV4_STR_LEN_MAX + 1);

	g_strlcpy(gateway_buffer,
			inet_ntoa(prof_info->ProfileInfo.Wlan.net_info.GatewayAddr.Data.Ipv4),
			NETPM_IPV4_STR_LEN_MAX + 1);

	NETWORK_LOG(NETWORK_HIGH, "ipaddress: %s, netmask: %s, gateway: %s\n",
			ipaddress, netmask, gateway);

	NETWORK_LOG(NETWORK_HIGH, "DBus Message 1/2: %s %s %s %s\n", CONNMAN_SERVICE,
			profile_name, CONNMAN_SERVICE_INTERFACE, "SetProperty");

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));

	if (prof_info->ProfileInfo.Wlan.net_info.IpConfigType == NET_IP_CONFIG_TYPE_DYNAMIC ||
	    prof_info->ProfileInfo.Wlan.net_info.IpConfigType == NET_IP_CONFIG_TYPE_AUTO_IP) {

		g_variant_builder_add(builder, "{sv}", prop_method, g_variant_new_string(dhcp_method));

	} else if (prof_info->ProfileInfo.Wlan.net_info.IpConfigType == NET_IP_CONFIG_TYPE_OFF) {

		g_variant_builder_add(builder, "{sv}", prop_method, g_variant_new_string(off_method));

		NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2: %s %s\n", prop_method, off_method);
	} else if (prof_info->ProfileInfo.Wlan.net_info.IpConfigType == NET_IP_CONFIG_TYPE_STATIC) {

		g_variant_builder_add(builder, "{sv}", prop_method, g_variant_new_string(manual_method));

		if (strlen(ipaddress) >= NETPM_IPV4_STR_LEN_MIN) {
			g_variant_builder_add(builder, "{sv}", prop_address, g_variant_new_string(ipaddress));
		}

		if (strlen(netmask) >= NETPM_IPV4_STR_LEN_MIN) {
			g_variant_builder_add(builder, "{sv}", prop_netmask, g_variant_new_string(netmask));
		}

		if (strlen(gateway) >= NETPM_IPV4_STR_LEN_MIN) {
			g_variant_builder_add(builder, "{sv}", prop_gateway, g_variant_new_string(gateway));
		}
		NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2: %s %s %s %s %s %s %s %s\n",
				prop_method, manual_method, prop_address, ipaddress,
				prop_netmask, netmask, prop_gateway, gateway);
	} else {
		NETWORK_LOG(NETWORK_ERROR, "Invalid argument\n");
		__NETWORK_FUNC_EXIT__;
		return NET_ERR_INVALID_PARAM;
	}

	params = g_variant_new("(sv)",
			prop_ipv4_configuration, g_variant_builder_end(builder));
	g_variant_builder_unref(builder);

	reply = g_dbus_connection_call_sync(connection,
										CONNMAN_SERVICE,
										profile_name,
										CONNMAN_SERVICE_INTERFACE,
										"SetProperty",
										params,
										NULL,
										G_DBUS_CALL_FLAGS_NONE,
										DBUS_REPLY_TIMEOUT,
										_net_dbus_get_gdbus_cancellable(),
										&error);
	if (reply == NULL) {
		if (error != NULL) {
			SECURE_NETWORK_LOG(NETWORK_ERROR,
						"g_dbus_connection_call_sync() failed."
						"error [%d: %s]\n", error->code, error->message);
			g_error_free(error);
		} else {
			NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed.\n");
		}

		__NETWORK_FUNC_EXIT__;
		return NET_ERR_UNKNOWN;
	}

	NETWORK_LOG(NETWORK_HIGH, "Successfully configured IPv4.Configuration\n");
	g_variant_unref(reply);

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}

int _net_dbus_set_profile_dns(net_profile_info_t* prof_info, char* profile_name)
{
	__NETWORK_FUNC_ENTER__;

	const char *prop_nameserver_configuration = "Nameservers.Configuration";
	char dns_buffer[NET_DNS_ADDR_MAX][NETPM_IPV4_STR_LEN_MAX+1];
	char *dns_address[NET_DNS_ADDR_MAX];

	GError *error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	GVariantBuilder *builder;
	int i = 0;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	if ((prof_info == NULL) || (profile_name == NULL) || (strlen(profile_name) == 0) ||
	    (prof_info->ProfileInfo.Wlan.net_info.DnsCount > NET_DNS_ADDR_MAX))	{
		NETWORK_LOG(NETWORK_ERROR, "Invalid parameter\n");
		__NETWORK_FUNC_EXIT__;
		return NET_ERR_INVALID_PARAM;
	}

	for (i = 0; i < prof_info->ProfileInfo.Wlan.net_info.DnsCount; i++) {
		dns_buffer[i][0] = '\0';
		dns_address[i] = NULL;

		if (prof_info->ProfileInfo.Wlan.net_info.DnsAddr[i].Data.Ipv4.s_addr != 0)
			g_strlcpy(dns_buffer[i],
					inet_ntoa(prof_info->ProfileInfo.Wlan.net_info.DnsAddr[i].Data.Ipv4),
					NETPM_IPV4_STR_LEN_MAX + 1);

		dns_address[i] = dns_buffer[i];
	}

	if (prof_info->ProfileInfo.Wlan.net_info.IpConfigType == NET_IP_CONFIG_TYPE_STATIC) {

		builder = g_variant_builder_new(G_VARIANT_TYPE ("as"));
		for (i = 0; i < prof_info->ProfileInfo.Wlan.net_info.DnsCount; i++) {
			g_variant_builder_add(builder, "s", dns_address[i]);
		}

		params = g_variant_new("(sv)", prop_nameserver_configuration, g_variant_builder_end(builder));
		g_variant_builder_unref(builder);

		reply = g_dbus_connection_call_sync(connection,
											CONNMAN_SERVICE,
											profile_name,
											CONNMAN_SERVICE_INTERFACE,
											"SetProperty",
											params,
											NULL,
											G_DBUS_CALL_FLAGS_NONE,
											DBUS_REPLY_TIMEOUT,
											_net_dbus_get_gdbus_cancellable(),
											&error);
		if (reply == NULL) {
			if (error != NULL) {
				SECURE_NETWORK_LOG(NETWORK_ERROR,
							"g_dbus_connection_call_sync() failed."
							"error [%d: %s]\n", error->code, error->message);
				g_error_free(error);
			} else {
				NETWORK_LOG(NETWORK_ERROR,
						"g_dbus_connection_call_sync() failed.\n");
			}

			__NETWORK_FUNC_EXIT__;
			return NET_ERR_UNKNOWN;
		}

		NETWORK_LOG(NETWORK_HIGH, "Successfully configured Nameservers.Configuration\n");
		g_variant_unref(reply);
	}

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}

int _net_dbus_set_proxy(net_profile_info_t* prof_info, char* profile_name)
{
	__NETWORK_FUNC_ENTER__;

	const char *direct_method = "direct";
	const char *auto_method = "auto";
	const char *manual_method = "manual";

	const char *prop_proxy_configuration = "Proxy.Configuration";
	const char *prop_method = "Method";
	const char *prop_url = "URL";
	const char *prop_servers = "Servers";

	char proxy_buffer[NET_PROXY_LEN_MAX+1] = "";
	char *proxy_address = proxy_buffer;

	GError *error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	GVariantBuilder *builder;
	GVariantBuilder *builder_sub;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	if ((prof_info == NULL) || (profile_name == NULL) || (strlen(profile_name) == 0)) {
		NETWORK_LOG(NETWORK_ERROR, "Invalid argument\n");
		__NETWORK_FUNC_EXIT__;
		return NET_ERR_INVALID_PARAM;
	}

	g_strlcpy(proxy_buffer,
			prof_info->ProfileInfo.Wlan.net_info.ProxyAddr, NET_PROXY_LEN_MAX+1);

	NETWORK_LOG(NETWORK_HIGH, "method: %d, proxy address: %s\n",
			prof_info->ProfileInfo.Wlan.net_info.ProxyMethod, proxy_address);

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{sv}"));

	switch (prof_info->ProfileInfo.Wlan.net_info.ProxyMethod) {
	case NET_PROXY_TYPE_AUTO:
		g_variant_builder_add(builder, "{sv}", prop_method, g_variant_new_string(auto_method));
		break;
	case NET_PROXY_TYPE_MANUAL:
		g_variant_builder_add(builder, "{sv}", prop_method, g_variant_new_string(manual_method));
		break;
	default:
		g_variant_builder_add(builder, "{sv}", prop_method, g_variant_new_string(direct_method));
		break;
	}

	if (prof_info->ProfileInfo.Wlan.net_info.ProxyMethod == NET_PROXY_TYPE_AUTO &&
			proxy_address[0] != '\0') {
		g_variant_builder_add(builder, "{sv}", prop_url, g_variant_new_string(proxy_address));
	}

	if (prof_info->ProfileInfo.Wlan.net_info.ProxyMethod == NET_PROXY_TYPE_MANUAL &&
			proxy_address[0] != '\0') {
		builder_sub = g_variant_builder_new(G_VARIANT_TYPE ("as"));
		g_variant_builder_add(builder_sub, "s", proxy_address);
		g_variant_builder_add(builder, "{sv}", prop_servers, g_variant_builder_end(builder_sub));
		g_variant_builder_unref(builder_sub);
	}

	params = g_variant_new("(sv)", prop_proxy_configuration, g_variant_builder_end(builder));
	g_variant_builder_unref(builder);

	reply = g_dbus_connection_call_sync(connection,
										CONNMAN_SERVICE,
										profile_name,
										CONNMAN_SERVICE_INTERFACE,
										"SetProperty",
										params,
										NULL,
										G_DBUS_CALL_FLAGS_NONE,
										DBUS_REPLY_TIMEOUT,
										_net_dbus_get_gdbus_cancellable(),
										&error);
	if (reply == NULL) {
		if (error != NULL) {
			SECURE_NETWORK_LOG(NETWORK_ERROR,
						"g_dbus_connection_call_sync() failed."
						"error [%d: %s]\n", error->code, error->message);
			g_error_free(error);
		} else {
			NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed.\n");
		}

		__NETWORK_FUNC_EXIT__;
		return NET_ERR_UNKNOWN;
	}

	NETWORK_LOG(NETWORK_HIGH, "Successfully configured Proxy.Configuration\n");
	g_variant_unref(reply);

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}


int _net_dbus_add_pdp_profile(net_profile_info_t *prof_info)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	const char *service_type = "svc_ctg_id";
	const char *home_url = "home_url";
	const char *proxy_addr = "proxy_addr";
	const char *auth_pwd = "auth_pwd";
	const char *auth_type = "auth_type";
	const char *auth_id = "auth_id";
	const char *apn = "apn";
	const char *keyword = "keyword";

	char buff_svc_type[10] = "";
	char buff_auth_type[10] = "";
	char *temp_ptr = NULL;

	GError *error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	GVariantBuilder *builder;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	if (prof_info == NULL) {
		NETWORK_LOG(NETWORK_ERROR, "Invalid argument\n");
		__NETWORK_FUNC_EXIT__;
		return NET_ERR_INVALID_PARAM;
	}

	NETWORK_LOG(NETWORK_HIGH, "DBus Message 1/2 : %s %s %s %s\n", TELEPHONY_SERVICE,
			TELEPHONY_MASTER_PATH, TELEPHONY_MASTER_INTERFACE, ".AddProfile");

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{ss}"));

	g_snprintf(buff_svc_type, 10, "%d", prof_info->ProfileInfo.Pdp.ServiceType);
	temp_ptr = buff_svc_type;

	NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2 : %s : %s\n",
			service_type, temp_ptr);

	g_variant_builder_add(builder, "{ss}", service_type, temp_ptr);

	if (strlen(prof_info->ProfileInfo.Pdp.HomeURL) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.HomeURL;

		NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2 : %s : %s\n",
				home_url, temp_ptr);

		g_variant_builder_add(builder, "{ss}", home_url, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.net_info.ProxyAddr) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.net_info.ProxyAddr;

		NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2 : %s : %s\n",
				proxy_addr, temp_ptr);

		g_variant_builder_add(builder, "{ss}", proxy_addr, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.AuthInfo.Password) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.AuthInfo.Password;

		g_variant_builder_add(builder, "{ss}", auth_pwd, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.AuthInfo.UserName) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.AuthInfo.UserName;

		g_variant_builder_add(builder, "{ss}", auth_id, temp_ptr);
	}

	if (prof_info->ProfileInfo.Pdp.AuthInfo.AuthType >= NET_PDP_AUTH_NONE &&
			prof_info->ProfileInfo.Pdp.AuthInfo.AuthType <= NET_PDP_AUTH_CHAP) {
		g_snprintf(buff_auth_type, 10, "%d",
				prof_info->ProfileInfo.Pdp.AuthInfo.AuthType);

		temp_ptr = buff_auth_type;

		g_variant_builder_add(builder, "{ss}", auth_type, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.Apn) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.Apn;

		g_variant_builder_add(builder, "{ss}", apn, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.Keyword) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.Keyword;

		g_variant_builder_add(builder, "{ss}", keyword, temp_ptr);
	}

	params = g_variant_new("(@a{ss})", g_variant_builder_end(builder));
	g_variant_builder_unref(builder);

	reply = g_dbus_connection_call_sync(connection,
										TELEPHONY_SERVICE,
										TELEPHONY_MASTER_PATH,
										TELEPHONY_MASTER_INTERFACE,
										"AddProfile",
										params,
										NULL,
										G_DBUS_CALL_FLAGS_NONE,
										DBUS_REPLY_TIMEOUT,
										_net_dbus_get_gdbus_cancellable(),
										&error);
	if (reply == NULL) {
		if (error != NULL) {
			SECURE_NETWORK_LOG(NETWORK_ERROR,
						"g_dbus_connection_call_sync() failed."
						"error [%d: %s]\n", error->code, error->message);
			Error = __net_error_string_to_enum(error->message);
			g_error_free(error);
		} else {
			NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed.\n");
		}

		__NETWORK_FUNC_EXIT__;
		return NET_ERR_UNKNOWN;
	}

	NETWORK_LOG(NETWORK_HIGH, "Successfully requested : Add PDP profile\n");

	/** Check Reply */
	int add_result = 0;

	g_variant_get(reply, "(b)", &add_result);
	NETWORK_LOG(NETWORK_HIGH, "Profile add result: %d\n", add_result);

	if (add_result)
		Error = NET_ERR_NONE;
	else
		Error = NET_ERR_UNKNOWN;

	g_variant_unref(reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}


int _net_dbus_modify_pdp_profile(net_profile_info_t *prof_info, const char *profile_name)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	const char *service_type = "svc_ctg_id";
	const char *home_url = "home_url";
	const char *proxy_addr = "proxy_addr";
	const char *auth_pwd = "auth_pwd";
	const char *auth_type = "auth_type";
	const char *auth_id = "auth_id";
	const char *apn = "apn";
	const char *keyword = "keyword";
	const char *default_conn = "default_internet_conn";
	const char *hidden = "hidden";
	const char *editable = "editable";

	char buff_svc_type[10] = "";
	char buff_auth_type[10] = "";
	char *temp_ptr = NULL;

	GError *error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	GVariantBuilder *builder;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	if ((prof_info == NULL) || (profile_name == NULL)) {
		NETWORK_LOG(NETWORK_ERROR, "Invalid argument\n");
		__NETWORK_FUNC_EXIT__;
		return NET_ERR_INVALID_PARAM;
	}

	NETWORK_LOG(NETWORK_HIGH, "DBus Message 1/2 : %s %s %s %s\n", TELEPHONY_SERVICE,
			profile_name, TELEPHONY_PROFILE_INTERFACE, ".ModifyProfile");

	builder = g_variant_builder_new(G_VARIANT_TYPE ("a{ss}"));


	g_snprintf(buff_svc_type, 10, "%d", prof_info->ProfileInfo.Pdp.ServiceType);
	temp_ptr = buff_svc_type;

	NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2 : %s : %s\n",
			service_type, temp_ptr);

	g_variant_builder_add(builder, "{ss}", service_type, temp_ptr);

	if (strlen(prof_info->ProfileInfo.Pdp.HomeURL) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.HomeURL;

		g_variant_builder_add(builder, "{ss}", home_url, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.net_info.ProxyAddr) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.net_info.ProxyAddr;

		g_variant_builder_add(builder, "{ss}", proxy_addr, temp_ptr);
	} else
		g_variant_builder_add(builder, "{ss}", proxy_addr, "");

	if (strlen(prof_info->ProfileInfo.Pdp.AuthInfo.Password) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.AuthInfo.Password;

		g_variant_builder_add(builder, "{ss}", auth_pwd, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.AuthInfo.UserName) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.AuthInfo.UserName;

		g_variant_builder_add(builder, "{ss}", auth_id, temp_ptr);
	}

	if (prof_info->ProfileInfo.Pdp.AuthInfo.AuthType >= NET_PDP_AUTH_NONE &&
	    prof_info->ProfileInfo.Pdp.AuthInfo.AuthType <= NET_PDP_AUTH_CHAP) {
		g_snprintf(buff_auth_type, 10, "%d",
				prof_info->ProfileInfo.Pdp.AuthInfo.AuthType);
		temp_ptr = buff_auth_type;

		g_variant_builder_add(builder, "{ss}", auth_type, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.Apn) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.Apn;

		g_variant_builder_add(builder, "{ss}", apn, temp_ptr);
	}

	if (strlen(prof_info->ProfileInfo.Pdp.Keyword) > 0) {
		temp_ptr = prof_info->ProfileInfo.Pdp.Keyword;

		g_variant_builder_add(builder, "{ss}", keyword, temp_ptr);
	}

	if (prof_info->ProfileInfo.Pdp.Hidden)
		temp_ptr = "TRUE";
	else
		temp_ptr = "FALSE";

	NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2 : %s : %s\n",
			hidden, temp_ptr);

	g_variant_builder_add(builder, "{ss}", hidden, temp_ptr);

	if (prof_info->ProfileInfo.Pdp.Editable)
		temp_ptr = "TRUE";
	else
		temp_ptr = "FALSE";

	NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2 : %s : %s\n",
			editable, temp_ptr);

	g_variant_builder_add(builder, "{ss}", editable, temp_ptr);

	if (prof_info->ProfileInfo.Pdp.DefaultConn)
		temp_ptr = "TRUE";
	else
		temp_ptr = "FALSE";

	NETWORK_LOG(NETWORK_HIGH, "DBus Message 2/2 : %s : %s\n",
			default_conn, temp_ptr);

	g_variant_builder_add(builder, "{ss}", default_conn, temp_ptr);

	params = g_variant_new("(@a{ss})", g_variant_builder_end(builder));
	g_variant_builder_unref(builder);

	reply = g_dbus_connection_call_sync(connection,
										TELEPHONY_SERVICE,
										profile_name,
										TELEPHONY_PROFILE_INTERFACE,
										"ModifyProfile",
										params,
										NULL,
										G_DBUS_CALL_FLAGS_NONE,
										DBUS_REPLY_TIMEOUT,
										_net_dbus_get_gdbus_cancellable(),
										&error);
	if (reply == NULL) {
		if (error != NULL) {
			SECURE_NETWORK_LOG(NETWORK_ERROR,
						"g_dbus_connection_call_sync() failed."
						"error [%d: %s]\n", error->code, error->message);
			Error = __net_error_string_to_enum(error->message);
			g_error_free(error);
		} else {
			NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed.\n");
		}

		__NETWORK_FUNC_EXIT__;
		return NET_ERR_UNKNOWN;
	}

	NETWORK_LOG(NETWORK_HIGH, "Successfully requested to modify PDP profile\n");

	/** Check Reply */
	int add_result = 0;
	g_variant_get(reply, "(b)", &add_result);
	NETWORK_LOG(NETWORK_HIGH, "Profile modify result: %d\n", add_result);

	if (add_result)
		Error = NET_ERR_NONE;
	else
		Error = NET_ERR_UNKNOWN;

	g_variant_unref(reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_load_wifi_driver(gboolean wifi_picker_test)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;
	GVariant *params;

	params = g_variant_new("(b)", wifi_picker_test);

	/* use DBus signal than reply pending because of performance reason */
	Error = _net_invoke_dbus_method_nonblock(NETCONFIG_SERVICE,
			NETCONFIG_WIFI_PATH, NETCONFIG_WIFI_INTERFACE,
			"LoadDriver", params,
			__net_wifi_power_reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_remove_wifi_driver(void)
{
	__NETWORK_FUNC_ENTER__;

	net_err_t Error = NET_ERR_NONE;

	/* use DBus signal than reply pending because of performance reason */
	Error = _net_invoke_dbus_method_nonblock(NETCONFIG_SERVICE,
			NETCONFIG_WIFI_PATH, NETCONFIG_WIFI_INTERFACE,
			"RemoveDriver", NULL,
			__net_wifi_power_reply);

	__NETWORK_FUNC_EXIT__;
	return Error;
}

int _net_dbus_specific_scan_request(const char *ssid)
{
	__NETWORK_FUNC_ENTER__;

	GVariant *params = NULL;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	params = g_variant_new("(s)", ssid);

	g_dbus_connection_call(connection,
							NETCONFIG_SERVICE,
							NETCONFIG_WIFI_PATH,
							NETCONFIG_WIFI_INTERFACE,
							"RequestSpecificScan",
							params,
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							6 * DBUS_REPLY_TIMEOUT,
							_net_dbus_get_gdbus_cancellable(),
							(GAsyncReadyCallback) __net_specific_scan_wifi_reply,
							NULL);

	NETWORK_LOG(NETWORK_HIGH, "Successfully configured\n");

	_net_dbus_pending_call_ref();

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}

int _net_dbus_wps_scan_request(void)
{
	__NETWORK_FUNC_ENTER__;

	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	g_dbus_connection_call(connection,
							NETCONFIG_SERVICE,
							NETCONFIG_WIFI_PATH,
							NETCONFIG_WIFI_INTERFACE,
							"RequestWpsScan",
							NULL,
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							6 * DBUS_REPLY_TIMEOUT,
							_net_dbus_get_gdbus_cancellable(),
							(GAsyncReadyCallback) __net_wps_scan_wifi_reply,
							NULL);

	NETWORK_LOG(NETWORK_HIGH, "Successfully configured\n");

	_net_dbus_pending_call_ref();

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}

int _net_dbus_get_passpoint(int *enabled)
{
	__NETWORK_FUNC_ENTER__;

	GDBusConnection *connection;
	GError *error = NULL;
	GVariant *reply = NULL;
	net_err_t Error = NET_ERR_NONE;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	reply = g_dbus_connection_call_sync(connection,
										NETCONFIG_SERVICE,
										NETCONFIG_WIFI_PATH,
										NETCONFIG_WIFI_INTERFACE,
										"GetPasspoint",
										NULL,
										NULL,
										G_DBUS_CALL_FLAGS_NONE,
										DBUS_REPLY_TIMEOUT,
										_net_dbus_get_gdbus_cancellable(),
										&error);
	if (reply == NULL) {
		if (error != NULL) {
			SECURE_NETWORK_LOG(NETWORK_ERROR,
						"g_dbus_connection_call_sync() failed."
						"error [%d: %s]\n", error->code, error->message);
			Error = __net_error_string_to_enum(error->message);
			g_error_free(error);
		} else {
			NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed.\n");
			Error = NET_ERR_UNKNOWN;
		}

		__NETWORK_FUNC_EXIT__;
		return Error;
	}

	/** Check Reply */
	int result = 0;
	g_variant_get(reply, "(i)", &result);
	*enabled = result;

	NETWORK_LOG(NETWORK_HIGH, "Get passpoint result: %d\n", result);

	g_variant_unref(reply);

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}

int _net_dbus_set_passpoint(int enable)
{
	__NETWORK_FUNC_ENTER__;

	GDBusConnection *connection;
	GVariant *params;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	params = g_variant_new("(i)", enable);

	g_dbus_connection_call(connection,
							NETCONFIG_SERVICE,
							NETCONFIG_WIFI_PATH,
							NETCONFIG_WIFI_INTERFACE,
							"SetPasspoint",
							params,
							NULL,
							G_DBUS_CALL_FLAGS_NONE,
							6 * DBUS_REPLY_TIMEOUT,
							_net_dbus_get_gdbus_cancellable(),
							(GAsyncReadyCallback) __net_set_passpoint_reply,
							NULL);

	NETWORK_LOG(NETWORK_HIGH, "Successfully configured\n");

	_net_dbus_pending_call_ref();

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}

int _net_dbus_wifi_set_autoconnect(const char *profile_name, gboolean autoconnect)
{
	__NETWORK_FUNC_ENTER__;

	const char *auto_connect = "AutoConnect";
	GError *error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	params = g_variant_new("(sv)", auto_connect, g_variant_new_boolean(autoconnect));

	reply = g_dbus_connection_call_sync(connection,
										CONNMAN_SERVICE,
										profile_name,
										CONNMAN_SERVICE_INTERFACE,
										"SetProperty",
										params,
										NULL,
										G_DBUS_CALL_FLAGS_NONE,
										DBUS_REPLY_TIMEOUT,
										_net_dbus_get_gdbus_cancellable(),
										&error);
	if (reply == NULL) {
		if (error != NULL) {
			SECURE_NETWORK_LOG(NETWORK_ERROR,
						"g_dbus_connection_call_sync() failed."
						"error [%d: %s]\n", error->code, error->message);
			g_error_free(error);
		} else {
			NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed.\n");
		}

		__NETWORK_FUNC_EXIT__;
		return NET_ERR_UNKNOWN;
	}

	g_variant_unref(reply);

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}

int _net_dbus_wifi_set_local_only_network(const char *profile_name, gboolean value)
{
	__NETWORK_FUNC_ENTER__;

	const char *local_network = "LocalNetwork";
	GError *error = NULL;
	GVariant *reply = NULL;
	GVariant *params = NULL;
	GDBusConnection *connection;

	connection = _net_dbus_get_gdbus_conn();
	if (connection == NULL)
		return NET_ERR_APP_NOT_REGISTERED;

	params = g_variant_new("(sv)", local_network, g_variant_new_boolean(value));

	reply = g_dbus_connection_call_sync(connection,
			CONNMAN_SERVICE,
			profile_name,
			CONNMAN_SERVICE_INTERFACE,
			"SetProperty",
			params,
			NULL,
			G_DBUS_CALL_FLAGS_NONE,
			DBUS_REPLY_TIMEOUT,
			_net_dbus_get_gdbus_cancellable(),
			&error);
	if (reply == NULL) {
		if (error != NULL) {
			SECURE_NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed."
					"error [%d: %s]\n", error->code, error->message);
			g_error_free(error);
		} else {
			NETWORK_LOG(NETWORK_ERROR,
					"g_dbus_connection_call_sync() failed.\n");
		}

		__NETWORK_FUNC_EXIT__;
		return NET_ERR_UNKNOWN;
	}

	g_variant_unref(reply);

	__NETWORK_FUNC_EXIT__;
	return NET_ERR_NONE;
}