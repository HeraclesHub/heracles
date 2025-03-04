/**
 * This file is part of Hercules.
 * http://herc.ws - http://github.com/HerculesWS/Hercules
 *
 * Copyright (C) 2012-2025 Hercules Dev Team
 * Copyright (C) Athena Dev Teams
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
#ifndef MAP_PARTY_H
#define MAP_PARTY_H

#include "common/hercules.h"
#include "common/db.h"
#include "common/mmo.h" // struct party

#include <stdarg.h>

#ifndef MAX_PARTY_BOOKING_JOBS
#define MAX_PARTY_BOOKING_JOBS 6
#endif
#ifndef MAX_PARTY_BOOKING_RESULTS
#define MAX_PARTY_BOOKING_RESULTS 10
#endif

struct block_list;
struct hplugin_data_store;
struct map_session_data;

struct party_member_data {
	struct map_session_data *sd;
	unsigned int hp; //For HP,x,y refreshing.
	unsigned short x, y;
};

struct party_data {
	struct party party;
	struct party_member_data data[MAX_PARTY];
	uint8 itemc; //For item distribution, position of last picker in party
	short *instance;
	unsigned short instances;
	struct {
		unsigned monk : 1;   ///< There's at least one monk in party?
		unsigned sg : 1;     ///< There's at least one Star Gladiator in party?
		unsigned snovice :1; ///< There's a Super Novice
		unsigned tk : 1;     ///< There's a taekwon
		unsigned option_auto_changed : 1;  ///< Party options were changed automatically. (inter_party_check_lv())
		unsigned member_level_changed : 1; ///< A party member's level has changed.
	} state;
	struct hplugin_data_store *hdata; ///< HPM Plugin Data Store
};

#define PB_NOTICE_LENGTH (36 + 1)

struct party_booking_detail {
	short level;
#ifdef PARTY_RECRUIT
	char notice[PB_NOTICE_LENGTH];
#else // not PARTY_RECRUIT
	short mapid;
	short job[MAX_PARTY_BOOKING_JOBS];
#endif // PARTY_RECRUIT
};

struct party_booking_ad_info {
	unsigned int index;
#ifdef PARTY_BOOKING
	int expiretime;
	char charname[NAME_LENGTH];
#else // not PARTY_BOOKING
	char charname[NAME_LENGTH];
	int expiretime;
#endif // PARTY_BOOKING
	struct party_booking_detail p_detail;
};

/*=====================================
* Interface : party.h
* Generated by HerculesInterfaceMaker
* created by Susu
*-------------------------------------*/
struct party_interface {
	struct DBMap *db; // int party_id -> struct party_data* (releases data)
	struct DBMap *booking_db; // int char_id -> struct party_booking_ad_info* (releases data) // Party Booking [Spiria]
	unsigned int booking_nextid;
	/* funcs */
	void (*init) (bool minimal);
	void (*final) (void);
	/* */
	struct party_data* (*search) (int party_id);
	struct party_data* (*searchname) (const char* str);
	int (*getmemberid) (struct party_data* p, struct map_session_data* sd);
	struct map_session_data* (*getavailablesd) (struct party_data *p);

	int (*create) (struct map_session_data *sd, const char *name, int item, int item2);
	void (*created) (int account_id, int char_id, int fail, int party_id, const char *name);
	int (*request_info) (int party_id, int char_id);
	int (*invite) (struct map_session_data *sd,struct map_session_data *tsd);
	void (*member_joined) (struct map_session_data *sd);
	int (*member_added) (int party_id,int account_id,int char_id,int flag);
	int (*leave) (struct map_session_data *sd);
	int (*removemember) (struct map_session_data *sd, int account_id, const char *name);
	int (*member_withdraw) (int party_id,int account_id,int char_id);
	void (*reply_invite) (struct map_session_data *sd,int party_id,int flag);
	int (*recv_noinfo) (int party_id, int char_id);
	int (*recv_info) (const struct party *sp, int char_id);
	int (*recv_movemap) (int party_id,int account_id,int char_id, unsigned short mapid,int online,int lv);
	int (*broken) (int party_id);
	int (*optionchanged) (int party_id,int account_id,int exp,int item,int flag);
	int (*changeoption) (struct map_session_data *sd,int exp,int item);
	bool (*changeleader) (struct map_session_data *sd, struct map_session_data *t_sd);
	void (*send_movemap) (struct map_session_data *sd);
	void (*send_levelup) (struct map_session_data *sd);
	int (*send_logout) (struct map_session_data *sd);
	int (*send_message) (struct map_session_data *sd, const char *mes);
	int (*skill_check) (struct map_session_data *sd, int party_id, uint16 skill_id, uint16 skill_lv);
	int (*send_xy_clear) (struct party_data *p);
	int (*exp_share) (struct party_data *p,struct block_list *src,unsigned int base_exp,unsigned int job_exp,int zeny);
	int (*share_loot) (struct party_data* p, struct map_session_data* sd, struct item* item_data, int first_charid);
	int (*send_dot_remove) (struct map_session_data *sd);
	int (*sub_count) (struct block_list *bl, va_list ap);
	int (*sub_count_chorus) (struct block_list *bl, va_list ap);
	/*==========================================
	 * Party Booking in KRO [Spiria]
	 *------------------------------------------*/
	void (*booking_register) (struct map_session_data *sd, short level, short mapid, short* job);
	void (*booking_update) (struct map_session_data *sd, short* job);
	void (*booking_search) (struct map_session_data *sd, short level, short mapid, short job, unsigned long lastindex, short resultcount);
	/* PARTY_RECRUIT */
	void (*recruit_register) (struct map_session_data *sd, short level, const char *notice);
	void (*recruit_update) (struct map_session_data *sd, const char *notice);
	void (*recruit_search) (struct map_session_data *sd, short level, short mapid, unsigned long lastindex, short resultcount);
	bool (*booking_delete) (struct map_session_data *sd);
	/* */
	int (*vforeachsamemap) (int (*func)(struct block_list *,va_list),struct map_session_data *sd,int range, va_list ap);
	int (*foreachsamemap) (int (*func)(struct block_list *,va_list),struct map_session_data *sd,int range,...);
	int (*send_xy_timer) (int tid, int64 tick, int id, intptr_t data);
	void (*fill_member) (struct party_member* member, struct map_session_data* sd, unsigned int leader);
	struct map_session_data *(*sd_check) (int party_id, int account_id, int char_id);
	void (*check_state) (struct party_data *p);
	struct party_booking_ad_info* (*create_booking_data) (void);
	int (*db_final) (union DBKey key, struct DBData *data, va_list ap);

	bool (*is_leader) (struct map_session_data *sd, const struct party_data *p);
	void (*agency_request_join)(struct map_session_data *sd, struct map_session_data *tsd);
};

#ifdef HERCULES_CORE
void party_defaults(void);
#endif // HERCULES_CORE

HPShared struct party_interface *party;

#endif /* MAP_PARTY_H */
