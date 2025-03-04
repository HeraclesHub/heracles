/**
 * This file is part of Hercules.
 * http://herc.ws - http://github.com/HerculesWS/Hercules
 *
 * Copyright (C) 2020-2025 Hercules Dev Team
 * Copyright (C) 2020-2022 Andrei Karas (4144)
 *
 * Hercules is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#define HERCULES_CORE

#include "config/core.h" // ANTI_MAYAP_CHEAT, RENEWAL, SECURE_NPCTIMEOUT
#include "api/httpsender.h"

#include "common/HPM.h"
#include "common/cbasetypes.h"
#include "common/conf.h"
#include "common/ers.h"
#include "common/grfio.h"
#include "common/memmgr.h"
#include "common/mmo.h" // NEW_CARTS, char_achievements
#include "common/nullpo.h"
#include "common/packets.h"
#include "common/random.h"
#include "common/showmsg.h"
#include "common/socket.h"
#include "common/strlib.h"
#include "common/timer.h"
#include "common/utils.h"
#include "api/aclif.h"
#include "api/apisessiondata.h"
#include "api/http_include.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#define WFIFOADDSTR(fd, str) \
    memcpy(WFIFOP(fd, 0), str, strlen(str)); \
    WFIFOSET(fd, strlen(str));

#define WFIFOADDBUF(fd, buf, buf_size) \
    memcpy(WFIFOP(fd, 0), buf, buf_size); \
    WFIFOSET(fd, buf_size);

static struct httpsender_interface httpsender_s;
struct httpsender_interface *httpsender;
static char tmp_buffer[MAX_RESPONSE_SIZE];

//#define DEBUG_LOG

static int do_init_httpsender(bool minimal)
{
	return 0;
}

static void do_final_httpsender(void)
{
}

/**
 * Converts a HTTP Status code (HTTP_RES_*) into its HTTP Status text
 * @param status status code
 * @returns status text
 */
static const char *httpsender_http_status_name(enum http_status status)
{
	switch (status) {
	#define XX(num, name, string) case HTTP_STATUS_##name: return #string;
	HTTP_STATUS_MAP(XX)
	#undef XX
	default:
		ShowWarning("%s: Invalid http status (%u) received.\n", __func__, status);
		return "Unknown";
	}
}

/**
 * Sends a "100 (Continue)" response to fd.
 * HTTP clients that has the expectation of a 100-Continue usually waits for some time
 * expecting for a "100 (Continue)" (or rejection) response before they start transmitting data.
 * Note that "100 (Continue)" doesn't should not close the connection.
 * @param fd connection to send continue to
 */
static void httpsender_send_continue(int fd)
{
#ifdef DEBUG_LOG
	ShowInfo("httpsender_send_continue\n");
#endif  // DEBUG_LOG

	safestrncpy(tmp_buffer, "HTTP/1.1 100 Continue\n\n", sizeof(tmp_buffer));
	WFIFOHEAD(fd, strlen(tmp_buffer));
	WFIFOADDSTR(fd, tmp_buffer);
	sockt->flush(fd);
}

static bool httpsender_send_html(int fd, const char *data)
{
#ifdef DEBUG_LOG
	ShowInfo("httpsender_send_html\n");
#endif  // DEBUG_LOG

	nullpo_retr(false, data);

	const size_t sz = strlen(data);
	size_t buf_sz = snprintf(tmp_buffer, sizeof(tmp_buffer),
		"HTTP/1.1 200 OK\n"
		"Server: %s\n"
		"Content-Type: text/html\n"
		"Content-Length: %lu\n"
		"\n"
		"%s",
		httpsender->server_name, sz, data);
	WFIFOHEAD(fd, buf_sz);
	WFIFOADDSTR(fd, tmp_buffer);
	sockt->flush(fd);
	return true;
}

static bool httpsender_send_json(int fd, const JsonW *json)
{
#ifdef DEBUG_LOG
	ShowInfo("httpsender_send_json\n");
#endif  // DEBUG_LOG

	char *data = jsonwriter->get_string(json);
	const size_t sz = strlen(data);
	size_t buf_sz = snprintf(tmp_buffer, sizeof(tmp_buffer),
		"HTTP/1.1 200 OK\n"
		"Server: %s\n"
		"Content-Type: application/json; charset=utf-8\n"
		"Content-Length: %lu\n"
		"\n"
		"%s",
		httpsender->server_name, sz, data);
	jsonwriter->free(data);
	WFIFOHEAD(fd, buf_sz);
	WFIFOADDSTR(fd, tmp_buffer);
	sockt->flush(fd);
	return true;
}

/**
 * Sends "json" content to fd.
 * 
 * This is similar to httpsender->send_plain but uses the JSON Content-Type.
 * It doesn't perform any validation over "json" to ensure it is correct.
 * 
 * @param fd connection
 * @param json json text to be sent
 * @param status response HTTP status
 * @return true in case of success, false if something goes wrong
 */
static bool httpsender_send_json_text(int fd, const char *json, enum http_status status)
{
#ifdef DEBUG_LOG
	ShowInfo("httpsender_send_json_text\n");
#endif  // DEBUG_LOG

	nullpo_retr(false, json);

	const size_t sz = strlen(json);
	size_t buf_sz = snprintf(tmp_buffer, sizeof(tmp_buffer),
		"HTTP/1.1 %u %s\n"
		"Server: %s\n"
		"Content-Type: application/json; charset=utf-8\n"
		"Content-Length: %lu\n"
		"\n"
		"%s",
		status, httpsender->http_status_name(status),
		httpsender->server_name, sz, json);
	WFIFOHEAD(fd, buf_sz);
	WFIFOADDSTR(fd, tmp_buffer);
	sockt->flush(fd);

	return true;
}

static bool httpsender_send_plain(int fd, const char *data)
{
#ifdef DEBUG_LOG
	ShowInfo("httpsender_send_plain\n");
#endif  // DEBUG_LOG

	nullpo_retr(false, data);

	const size_t sz = strlen(data);
	size_t buf_sz = snprintf(tmp_buffer, sizeof(tmp_buffer),
		"HTTP/1.1 200 OK\n"
		"Server: %s\n"
		"Content-Type: text/plain; charset=utf-8\n"
		"Content-Length: %lu\n"
		"\n"
		"%s",
		httpsender->server_name, sz, data);
	WFIFOHEAD(fd, buf_sz);
	WFIFOADDSTR(fd, tmp_buffer);
	sockt->flush(fd);
	return true;
}

static bool httpsender_send_binary(int fd, const char *data, const size_t data_len)
{
#ifdef DEBUG_LOG
	ShowInfo("httpsender_send_binary\n");
#endif  // DEBUG_LOG

	nullpo_retr(false, data);

	size_t buf_sz = snprintf(tmp_buffer, sizeof(tmp_buffer),
		"HTTP/1.1 200 OK\n"
		"Server: %s\n"
		"Content-Type: octet-stream\n"
		"Content-Length: %lu\n"
		"\n",
		httpsender->server_name, data_len);
	WFIFOHEAD(fd, buf_sz);
	WFIFOADDSTR(fd, tmp_buffer);
	sockt->flush(fd);
	WFIFOHEAD(fd, data_len);
	WFIFOADDBUF(fd, data, data_len);
	sockt->flush(fd);
	return true;
}

void httpsender_defaults(void)
{
	httpsender = &httpsender_s;

	httpsender->tmp_buffer = tmp_buffer;
	httpsender->server_name = "herc.ws/1.0";

	httpsender->init = do_init_httpsender;
	httpsender->final = do_final_httpsender;

	httpsender->http_status_name = httpsender_http_status_name;

	httpsender->send_continue = httpsender_send_continue;

	httpsender->send_plain = httpsender_send_plain;
	httpsender->send_html = httpsender_send_html;
	httpsender->send_json = httpsender_send_json;
	httpsender->send_json_text = httpsender_send_json_text;
	httpsender->send_binary = httpsender_send_binary;
}
