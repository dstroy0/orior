// orior - Copyright (C) 2026 Douglas Quigg (dstroy0) <dquigg123@gmail.com>
// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// forms_blob.h: the forms of the machine file that answered a relation, the blob of a record of the run
// (forms_blob.oracle.tsv): one row a form, rows end to end
#ifndef FORMS_BLOB_H
#define FORMS_BLOB_H

// the record's name the vendor's writer appends the forms under
#define FORMS_BLOB_NAME "forms_blob"

// a form: its encoding's low and high words, 1 where its relation was read signed, its relation and its text as the
// writer places its operands, each ended by a NUL within its room
typedef struct
{
    unsigned long long low;
    unsigned long long high;
    unsigned int signed_read;
    char relation[60];
    char text[176];
} FormsBlobRow;

_Static_assert(sizeof(FormsBlobRow) == 256u, "FormsBlobRow must be the 256 bytes of a row");

#endif
