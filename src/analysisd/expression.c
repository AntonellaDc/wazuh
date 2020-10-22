/* Copyright (C) 2015-2020, Wazuh Inc.
 * All right reserved.
 *
 * This program is free software; you can redistribute it
 * and/or modify it under the terms of the GNU General Public
 * License (version 2) as published by the FSF - Free Software
 * Foundation
 */

#include "expression.h"


void w_calloc_expression_t(w_expression_t ** var, w_exp_type_t type) {

    os_calloc(1, sizeof(w_expression_t), *var);
    (*var)->exp_type = type;

    switch (type) {

        case EXP_TYPE_OSMATCH:
            os_calloc(1, sizeof(OSMatch), (*var)->match);
            break;

        case EXP_TYPE_OSREGEX:
            os_calloc(1, sizeof(OSRegex), (*var)->regex);
            break;

        default:
            break;
    }
}

void w_free_expression_t(w_expression_t ** var) {

    if (var == NULL || *var == NULL) {
        return;
    }

    switch ((*var)->exp_type) {

        case EXP_TYPE_OSMATCH:
            OSMatch_FreePattern((*var)->match);
            os_free((*var)->match);
            break;

        case EXP_TYPE_OSREGEX:
            OSRegex_FreePattern((*var)->regex);
            os_free((*var)->regex);
            break;

        case EXP_TYPE_STRING:
            os_free((*var)->string);
            break;

        case EXP_TYPE_OSIP_ARRAY:

            if((*var)->ips == NULL) {
                break;
            }

            for (int i = 0; (*var)->ips[i]; i++) {
                w_free_os_ip((*var)->ips[i]);
            }
            os_free((*var)->ips);
            break;

        case EXP_TYPE_PCRE2:
             pcre2_code_free((*var)->pcre2);
             break;

        default:
            break;
    }
    os_free(*var);
}

bool w_expression_add_osip(w_expression_t ** var, char * ip) {

    unsigned int ip_s = 0;

    if ((*var) == NULL) {
        w_calloc_expression_t(var, EXP_TYPE_OSIP_ARRAY);
    }

    while ((*var)->ips && (*var)->ips[ip_s]) {
        ip_s++;
    }

    os_realloc((*var)->ips, (ip_s + 2) * sizeof(os_ip *), (*var)->ips);
    os_calloc(1, sizeof(os_ip), (*var)->ips[ip_s]);
    (*var)->ips[ip_s + 1] = NULL;

    if (!OS_IsValidIP(ip, (*var)->ips[ip_s])) {
        w_free_expression_t(var);
        return false;
    }

    return true;
}

bool w_expression_compile(w_expression_t * expression, char * pattern, int flags) {

    bool retval = true;

    int errornumber = 0;
    PCRE2_SIZE erroroffset = 0;

    switch (expression->exp_type) {

        case EXP_TYPE_OSREGEX:
            if (!OSRegex_Compile(pattern, expression->regex, flags)) {
                merror(REGEX_COMPILE, pattern, expression->regex->error);
                retval = false;
            }
            break;

        case EXP_TYPE_OSMATCH:
            if (!OSMatch_Compile(pattern, expression->match, flags)) {
                merror(REGEX_COMPILE, pattern, expression->match->error);
                retval = false;
            }
            break;

        case EXP_TYPE_PCRE2:
            expression->pcre2 = pcre2_compile((PCRE2_SPTR) pattern, PCRE2_ZERO_TERMINATED,
                                               0, &errornumber, &erroroffset, NULL);

            if (!expression->pcre2) {
                PCRE2_UCHAR error_message[OS_SIZE_256];
                pcre2_get_error_message(errornumber, error_message, OS_SIZE_256);
                merror(REGEX_PCRE2_COMPILE, (int)erroroffset, error_message);
                retval = false;
            }

            break;

        default:
            break;
    }

    return retval;
}

bool w_expression_test(w_expression_t * expression, const char * str_test) {

    bool retval = false;

    /* OSRegex */
    regex_matching decoder_match;
    /* PCRE2 */
    pcre2_match_data * match_data;
    int rc = 0;

    if (expression == NULL || str_test == NULL) {
        return retval;
    }

    switch (expression->exp_type) {

        case EXP_TYPE_OSMATCH:
            retval = (OSMatch_Execute(str_test, sizeof(str_test), expression->match) == 0) ? false : true;
            break;

        case EXP_TYPE_OSREGEX:
            memset(&decoder_match, 0, sizeof(regex_matching));
            retval = (OSRegex_Execute_ex(str_test, expression->regex, &decoder_match) == NULL) ? false : true;
            break;

        case EXP_TYPE_STRING:
            
            break;

        case EXP_TYPE_OSIP_ARRAY:

            break;

        case EXP_TYPE_PCRE2:
            match_data = pcre2_match_data_create_from_pattern(expression->pcre2, NULL);
            rc = pcre2_match(expression->pcre2, (PCRE2_SPTR) str_test, sizeof(str_test), 0, 0, match_data, NULL);
            pcre2_match_data_free(match_data);
            retval = (rc > 0) ? true : false;
            break;

        default:
            break;
    }

    return retval;
}

const char * w_expression_execute(w_expression_t * expression, const char * str_test, regex_matching * regex_match) {

    const char * retval = NULL;

    pcre2_match_data * match_data;
    PCRE2_SIZE * ovector;
    int rc = 0;

    if (expression == NULL || str_test == NULL || regex_match == NULL) {
        return retval;
    }

    switch (expression->exp_type) {

        case EXP_TYPE_OSREGEX:
            retval = OSRegex_Execute_ex(str_test, expression->regex, regex_match);
            break;

        case EXP_TYPE_PCRE2:
            match_data = pcre2_match_data_create_from_pattern(expression->pcre2, NULL);
            rc = pcre2_match(expression->pcre2, (PCRE2_SPTR) str_test, strlen(str_test), 0, 0, match_data, NULL);

            if (rc > 0) {
                ovector = pcre2_get_ovector_pointer(match_data);
                retval = str_test + ovector[1] - 1;
                w_expression_PCRE2_fill_regex_match(rc, str_test, match_data, regex_match);
            }
            pcre2_match_data_free(match_data);
            break;

        default:
            break;
    }

    return retval;
}

void w_expression_PCRE2_fill_regex_match(int rc, const char * str_test, pcre2_match_data * match_data,
                                         regex_matching * regex_match) {

    PCRE2_SIZE * ovector;
    char ***sub_strings;
    regex_dynamic_size *str_sizes;

    if (rc < 2) {
        return;
    }

    sub_strings = &regex_match->sub_strings;
    str_sizes = &regex_match->d_size;

    w_FreeArray(*sub_strings);
    os_realloc(*sub_strings, sizeof(char *) * rc, *sub_strings);
    memset((void*)*sub_strings, 0, sizeof(char *) * rc);
    str_sizes->sub_strings_size = rc;


    ovector = pcre2_get_ovector_pointer(match_data);
    for (int i = 1; i < rc; i++) {
        size_t substring_length = ovector[2 * i + 1] - ovector[2 * i];
        regex_match->sub_strings[i - 1] = strndup(str_test + ovector[2 * i], substring_length);
    }
    regex_match->sub_strings[rc - 1] = NULL;
}