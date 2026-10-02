// Copyright (c) 2025 Naemon team - license: GPL-2.0
// This file is part of the naemon project: https://www.naemon.io
// See LICENSE file in the project root for details.

#ifndef Column_h
#define Column_h

#include "config.h"

#include <stdio.h>
#include <string>
using namespace std;

#define COLTYPE_INT     0
#define COLTYPE_DOUBLE  1
#define COLTYPE_STRING  2
#define COLTYPE_LIST    3
#define COLTYPE_TIME    4
#define COLTYPE_DICT    5

class Filter;
class Query;

class Column
{
    string _name;
    string _description;
public:
    // _indirect_offset locates a pointer field inside the row object
    // that pointer leads to the nested object whose fields this column addresses.
    // if the Object has its fields, normal "offset" is used e.g:
    // Object field: char* name , offset = (char *) &object.name - (char *) &object
    // if the Object refers to another subobject, the "_indirect_offset" is used to get the nested object, the pointer is resolved, and then the offset is used, e.g:
    // Object field: hostgroup *_hostgroup , _indirect_offset = (char *)&(ref._hostgroup) - (char *)&ref
    // once the "_indirect_offset" is added to the base address, the pointer has to be resolved first time to get pointer to the subobject,
    // and then a second time to get to the target field that this column addresses, to read/write to it.
    int _indirect_offset;

public:
    Column(string name, string description, int indirect_offset);
    virtual ~Column() {}
    const char *name() const { return _name.c_str(); }
    const char *description() const { return _description.c_str(); }
    virtual string valueAsString(void *data __attribute__ ((__unused__)), Query *)
        { return "invalid"; }
    virtual int type() = 0;
    virtual void output(void *data, Query *) = 0;
    virtual Filter *createFilter(int opid __attribute__ ((__unused__)), char *value __attribute__ ((__unused__))) { return 0; }
    void *shiftPointer(void *data);
    virtual int compare(void *dataa, void *datab, Query *query);
};

#endif // Column_h
