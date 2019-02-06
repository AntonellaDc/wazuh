/*
 * SQL Schema for vulnerability detector module
 * Copyright (C) 2015-2019, Wazuh Inc.
 * January 28, 2018.
 * This program is a free software, you can redistribute it
 * and/or modify it under the terms of GPLv2.
 */

BEGIN;

CREATE TABLE IF NOT EXISTS AGENTS (
         AGENT_ID INT NOT NULL,
         CPE_INDEX_ID INT DEFAULT 0,
         VENDOR TEXT NOT NULL,
         PACKAGE_NAME TEXT NOT NULL,
         VERSION TEXT NOT NULL,
         ARCH TEXT NOT NULL,
         PRIMARY KEY(AGENT_ID, VENDOR, PACKAGE_NAME, VERSION, ARCH)
);

 CREATE TABLE IF NOT EXISTS METADATA (
         OS TEXT PRIMARY KEY NOT NULL,
         PRODUCT_NAME TEXT NOT NULL,
         PRODUCT_VERSION TEXT,
         SCHEMA_VERSION TEXT,
         TIMESTAMP DATE NOT NULL
 );

 CREATE TABLE IF NOT EXISTS DB_METADATA (
         VERSION TEXT PRIMARY KEY NOT NULL
 );

 CREATE TABLE IF NOT EXISTS VULNERABILITIES_INFO (
         ID TEXT NOT NULL,
         TITLE TEXT,
         SEVERITY TEXT,
         PUBLISHED TEXT,
         UPDATED TEXT,
         REFERENCE TEXT,
         OS TEXT NOT NULL,
         RATIONALE TEXT,
         CVSS TEXT,
         CVSS_VECTOR TEXT,
         CVSS3 TEXT,
         BUGZILLA_REFERENCE TEXT,
         CWE TEXT,
         ADVISORIES TEXT,
         PRIMARY KEY(ID, OS)
 );

CREATE TABLE IF NOT EXISTS VULNERABILITIES (
        CVEID TEXT NOT NULL REFERENCES VULNERABILITIES_INFO(ID),
        OS TEXT NOT NULL REFERENCES VULNERABILITIES_INFO(V_OS),
        PACKAGE TEXT NOT NULL,
        PENDING BOOLEAN NOT NULL,
        OPERATION TEXT NOT NULL,
        OPERATION_VALUE TEXT,
        SECOND_OPERATION TEXT,
        SECOND_OPERATION_VALUE TEXT,
        PRIMARY KEY(CVEID, OS, PACKAGE, OPERATION_VALUE)
);

CREATE TABLE IF NOT EXISTS CPE_INDEX (
	ID INTEGER PRIMARY KEY AUTOINCREMENT,
    PART TEXT,
    VENDOR TEXT NOT NULL,
    PRODUCT TEXT NOT NULL,
    VERSION TEXT NOT NULL,
    UPDATEV TEXT,
    EDITION TEXT,
    LANGUAGE TEXT,
    SW_EDITION TEXT,
    TARGET_SW TEXT,
    TARGET_HW TEXT,
    OTHER TEXT
);

CREATE TABLE IF NOT EXISTS NVD_METADATA (
    YEAR INTEGER PRIMARY KEY,
    SIZE INTEGER,
    ZIP_SIZE INTEGER,
    GZ_SIZE INTEGER,
    SHA256 TEXT,
    LAST_MODIFIED INTEGER NOT NULL,
    CVES_NUMBER INTEGER
);

CREATE INDEX IF NOT EXISTS IN_CVE ON VULNERABILITIES_INFO (ID);
CREATE INDEX IF NOT EXISTS IN_PACK ON VULNERABILITIES (PACKAGE);
CREATE INDEX IF NOT EXISTS IN_OP ON VULNERABILITIES (OPERATION);
CREATE INDEX IF NOT EXISTS CPE_ID ON CPE_INDEX(ID);
CREATE INDEX IF NOT EXISTS CPE_VENDOR ON CPE_INDEX(VENDOR);
CREATE INDEX IF NOT EXISTS CPE_PRODUCT ON CPE_INDEX(PRODUCT);
CREATE INDEX IF NOT EXISTS CPE_VERSION ON CPE_INDEX(VERSION);
DELETE FROM DB_METADATA;

END;
