// SPDX-License-Identifier: AGPL-3.0-or-later OR LicenseRef-Commercial OR LicenseRef-Educational
// class.c: an instruction's class read off the vendor's instruction set for rules 2 and 3: whether the operation the
// class is named for transfers control or waits, whatever name the machine file's form gives the encoding
#include "cubin_safe_internal.h"

#include "../tables/read.h"

#include <ctype.h>

int cubin_class_control_or_wait(const struct TablesSetTag *tables, unsigned long long low, unsigned long long high)
{
    const TablesClass *const class_ = tables_class_of(tables, low, high);
    if (class_ == NULL)
    {
        return 0;
    }
    // the class is named for its operation, the mnemonic in lower case before the first underscore: `bra_`,
    // `exit_`, `iadd3_noimm__RRR_RRR`
    char operation[TABLES_NAME];
    unsigned int length = 0u;
    while ((length + 1u < sizeof(operation)) && (class_->name[length] != '\0') && (class_->name[length] != '_'))
    {
        // the name's bytes are ASCII letters and digits, which toupper takes as unsigned char
        operation[length] = (char)toupper((unsigned char)class_->name[length]);
        length += 1u;
    }
    operation[length] = '\0';
    // an EXIT is the walk's own to read: the one with no guard ends the code, and the others fall through
    if (strcmp(operation, "EXIT") == 0)
    {
        return 0;
    }
    return sass_operation_control_or_wait(operation);
}

int cubin_class_illegal(const struct TablesSetTag *tables, unsigned long long low, unsigned long long high)
{
    const TablesClass *const class_ = tables_class_of(tables, low, high);
    return (class_ != NULL) && !tables_class_legal(class_, low, high, NULL);
}
