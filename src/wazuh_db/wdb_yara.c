/*
 * Wazuh SQLite integration
 * Copyright (C) 2015-2019, Wazuh Inc.
 * June 06, 2016.
 *
 * This program is a free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation.
 */

#include "wdb.h"
#include "os_crypto/sha256/sha256_op.h"

/* Insert yara set data. Returns ID on success or -1 on error */
int wdb_yara_save_set_data(wdb_t * wdb, char *name, char *description) {
    if (!wdb->transaction && wdb_begin2(wdb) < 0){
        mdebug1("at wdb_yara_save_set_data(): cannot begin transaction");
        return -1;
    }

    sqlite3_stmt *stmt = NULL;

    if (wdb_stmt_cache(wdb, WDB_STMT_YARA_INSERT_SET) < 0) {
        mdebug1("at wdb_yara_save_set_data(): cannot cache statement");
        return -1;
    }

    stmt = wdb->stmt[WDB_STMT_YARA_INSERT_SET];

    sqlite3_bind_text(stmt, 1, name,-1, NULL);
    sqlite3_bind_text(stmt, 2, description, -1, NULL);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return sqlite3_changes(wdb->db);
    } else {
        merror("sqlite3_step(): %s", sqlite3_errmsg(wdb->db));
        return -1;
    }
}

/* Find yara set data. Returns NAME on success or -1 on error */
int wdb_yara_find_set_data(wdb_t * wdb, char *name, char *output) {
    if (!wdb->transaction && wdb_begin2(wdb) < 0){
        mdebug1("cannot begin transaction");
        return -1;
    }

    sqlite3_stmt *stmt = NULL;

    if (wdb_stmt_cache(wdb, WDB_STMT_YARA_SELECT_SET) < 0) {
        mdebug1("cannot cache statement");
        return -1;
    }

    stmt = wdb->stmt[WDB_STMT_YARA_SELECT_SET];

    sqlite3_bind_text(stmt, 1, name, -1, NULL);

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            snprintf(output, OS_MAXSTR - WDB_RESPONSE_BEGIN_SIZE, "%s", sqlite3_column_text(stmt, 1));
            return 1;
            break;
        case SQLITE_DONE:
            return 0;
            break;
        default:
            merror(" at sqlite3_step(): %s", sqlite3_errmsg(wdb->db));
            return -1;
    }
}


/* Insert yara set rule data. Returns ID on success or -1 on error */
int wdb_yara_save_set_rule_data(wdb_t * wdb, char *set_name, char *path, char *description) {
    if (!wdb->transaction && wdb_begin2(wdb) < 0){
        mdebug1("at wdb_yara_save_set_rule_data(): cannot begin transaction");
        return -1;
    }

    sqlite3_stmt *stmt = NULL;

    if (wdb_stmt_cache(wdb, WDB_STMT_YARA_INSERT_SET_RULE) < 0) {
        mdebug1("at wdb_yara_save_set_rule_data(): cannot cache statement");
        return -1;
    }

    stmt = wdb->stmt[WDB_STMT_YARA_INSERT_SET_RULE];

    sqlite3_bind_text(stmt, 1, set_name,-1, NULL);
    sqlite3_bind_text(stmt, 2, path, -1, NULL);
    sqlite3_bind_text(stmt, 3, description, -1, NULL);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return sqlite3_changes(wdb->db);
    } else {
        merror("sqlite3_step(): %s", sqlite3_errmsg(wdb->db));
        return -1;
    }
}

/* Find yara set data rule. Returns ID on success or -1 on error */
int wdb_yara_find_set_rule_data(wdb_t * wdb, char *set_name, char *path, char *output) {
    if (!wdb->transaction && wdb_begin2(wdb) < 0){
        mdebug1("cannot begin transaction");
        return -1;
    }

    sqlite3_stmt *stmt = NULL;

    if (wdb_stmt_cache(wdb, WDB_STMT_YARA_SELECT_SET_RULE) < 0) {
        mdebug1("cannot cache statement");
        return -1;
    }

    stmt = wdb->stmt[WDB_STMT_YARA_SELECT_SET_RULE];

    sqlite3_bind_text(stmt, 1, set_name, -1, NULL);
    sqlite3_bind_text(stmt, 2, path, -1, NULL);

    switch (sqlite3_step(stmt)) {
        case SQLITE_ROW:
            snprintf(output, OS_MAXSTR - WDB_RESPONSE_BEGIN_SIZE, "%s", sqlite3_column_text(stmt, 1));
            return 1;
            break;
        case SQLITE_DONE:
            return 0;
            break;
        default:
            merror(" at sqlite3_step(): %s", sqlite3_errmsg(wdb->db));
            return -1;
    }
}

/* Delete yara set rule. Returns ID on success or -1 on error */
int wdb_yara_delete_set_rule_data(wdb_t * wdb, char *set_name) {
    if (!wdb->transaction && wdb_begin2(wdb) < 0){
        mdebug1("at wdb_yara_delete_set_rule_data(): cannot begin transaction");
        return -1;
    }

    sqlite3_stmt *stmt = NULL;

    if (wdb_stmt_cache(wdb, WDB_STMT_YARA_DELETE_SET_RULES) < 0) {
        mdebug1("at wdb_yara_delete_set_rule_data(): cannot cache statement");
        return -1;
    }

    stmt = wdb->stmt[WDB_STMT_YARA_DELETE_SET_RULES];

    sqlite3_bind_text(stmt, 1, set_name, -1, NULL);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return 0;
    } else {
        merror("sqlite3_step(): %s", sqlite3_errmsg(wdb->db));
        return -1;
    }
}

/* Updates yara set data. Returns ID on success or -1 on error */
int wdb_yara_update_set_data(wdb_t * wdb, char *name, char *description) {
    if (!wdb->transaction && wdb_begin2(wdb) < 0){
        mdebug1("at wdb_yara_update_set_data(): cannot begin transaction");
        return -1;
    }

    sqlite3_stmt *stmt = NULL;

    if (wdb_stmt_cache(wdb, WDB_STMT_YARA_UPDATE_SET) < 0) {
        mdebug1("at wdb_yara_update_set_data(): cannot cache statement");
        return -1;
    }

    stmt = wdb->stmt[WDB_STMT_YARA_UPDATE_SET];

    sqlite3_bind_text(stmt, 1, name, -1, NULL);
    sqlite3_bind_text(stmt, 2, description, -1, NULL);
    sqlite3_bind_text(stmt, 3, name, -1, NULL);

    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return sqlite3_changes(wdb->db);
    } else {
        merror("sqlite3_step(): %s", sqlite3_errmsg(wdb->db));
        return -1;
    }
}

int wdb_yara_get_sets(wdb_t * wdb, char *output) {
    if (!wdb->transaction && wdb_begin2(wdb) < 0){
        mdebug1("cannot begin transaction");
        return -1;
    }

    sqlite3_stmt *stmt = NULL;

    if (wdb_stmt_cache(wdb, WDB_STMT_YARA_GET_SETS) < 0) {
        mdebug1("cannot cache statement");
        return -1;
    }

    stmt = wdb->stmt[WDB_STMT_YARA_GET_SETS];

    char *str = NULL;
    int has_result = 0;

    while(1) {
        switch (sqlite3_step(stmt)) {
            case SQLITE_ROW:
                has_result = 1;
                wm_strcat(&str,(const char *)sqlite3_column_text(stmt, 0),',');
                break;
            case SQLITE_DONE:
                goto end;
                break;
            default:
                merror(" at sqlite3_step(): %s", sqlite3_errmsg(wdb->db));
                os_free(str);
                return -1;
        }
    }

end:
    if(has_result) {
        snprintf(output, OS_MAXSTR - WDB_RESPONSE_BEGIN_SIZE, "%s", str);
        os_free(str);
        return 1;
    }
    return 0;
}

int wdb_yara_delete_set(wdb_t * wdb, char *set_name) {
    if (!wdb->transaction && wdb_begin2(wdb) < 0){
        mdebug1("at wdb_yara_delete_set(): cannot begin transaction");
        return -1;
    }

    sqlite3_stmt *stmt = NULL;

    if (wdb_stmt_cache(wdb, WDB_STMT_YARA_DELETE_SET) < 0) {
        mdebug1("at wdb_yara_delete_set(): cannot cache statement");
        return -1;
    }

    stmt = wdb->stmt[WDB_STMT_YARA_DELETE_SET];

    sqlite3_bind_text(stmt, 1, set_name, -1, NULL);
    
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        return 0;
    } else {
        merror("sqlite3_step(): %s", sqlite3_errmsg(wdb->db));
        return -1;
    }
}