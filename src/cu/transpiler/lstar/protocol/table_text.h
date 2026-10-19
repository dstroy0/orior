// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// table_text.h: a text a row of a table of the run names, numbered in the run's table of texts (texts.oracle.tsv)
#ifndef TABLE_TEXT_H
#define TABLE_TEXT_H

#ifdef __cplusplus
extern "C"
{
#endif

// the table of texts, the name its rows are appended under
#define TABLE_TEXTS "texts"

// the table of the files the run's programs wrote (files.oracle.tsv)
#define TABLE_FILES "files"

// the table of links, each a relation between rows that one column of a row cannot hold (links.oracle.tsv)
#define TABLE_LINKS "links"

// a file's bytes, the record of a file written whole, written from the rows of a table keyed to the head row of the
// file's records
#define TABLE_LINK_WRITTEN_FROM 0x1u

// the most columns a row holds
#define TABLE_ROW_COLUMNS 32u

// the folder of the oracles of the run's own tables, from the top of the tree
#define TABLE_ORACLES "src/cu/transpiler/lstar/protocol"

// the oracle of the rows of a k-file's table, from the top of the tree
#define TABLE_KFILE_ROWS "src/cu/includes/file_defs/krep/krep_records.oracle.tsv"

// the folder of the k-files' oracles, each `<kind>/<kind>.oracle.tsv` in it, from the top of the tree
#define TABLE_KFILE_ORACLES "src/cu/includes/file_defs"

    // the number of the `length` bytes at `bytes` in the table of texts: their 64-bit FNV-1a hash, 1 for a hash of
    // 0; the text appended as a row of the table the first time this process numbers it. An empty text has no
    // number: 0, the number a row gives where it holds no text, and no row
    unsigned long long table_text(const void *bytes, unsigned long long length);

#ifdef __cplusplus
}
#endif

#endif
