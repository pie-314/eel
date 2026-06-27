#ifndef HASHTABLE_H
#define HASHTABLE_H
#include <time.h>

#define HASHTABLE_H

typedef struct {
  char name[64];
} Symbol;

typedef struct Entry {
  char *key;
  Symbol *symbol;
  struct Entry *next;
} Entry;

typedef struct SymbolTable {
  Entry **buckets;
  size_t size;
  int count;
  int resizes;
} SymbolTable;

SymbolTable *st_create(int size);
unsigned long hash(const char *str);
void st_set(SymbolTable *st, const char *key, const char *value);
Symbol *st_get(SymbolTable *st, const char *key);
int st_delete(SymbolTable *st, const char *key);
void st_free(SymbolTable *st);
void st_resize(SymbolTable *st, int new_size);
void symbol_insert(SymbolTable *table, const char *name);

#endif
