/* The implementation of symbol table is taken from :
 * https://github.com/pie-314/RadishDB */

#include "symbol_table.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
    SymbolTable table;

    symbol_table_init(&table);

    symbol_insert(&table, "x");
    symbol_insert(&table, "y");

    printf("%d\n", symbol_exists(&table, "x"));   // 1
    printf("%d\n", symbol_exists(&table, "foo")); // 0
 */

// for initial version key and value are same
void symbol_insert(SymbolTable *table, const char *name) {
  Symbol *sym = malloc(sizeof(Symbol));
  strcpy(sym->name, name);
  st_set(table, name, sym);
}

// need to update this to look across symbol scope
bool symbol_exists(SymbolTable *table, const char *name) {
  SymbolTable *current = table;

  while (current != NULL) {
    if (st_get(current, name) != NULL) {
      return true;
    }
    current = current->parent;
  }

  return false;
}

SymbolTable *st_create(int size) {

  SymbolTable *st =
      malloc(sizeof(SymbolTable)); // allot memory for one SymbolTable struct

  st->size = size;
  st->count = 0;
  st->resizes = 0;
  st->buckets = malloc(size * sizeof(Entry *)); // multiply that one SymbolTable
                                                // with total enteries and allot
  st->parent = NULL;
  for (int i = 0; i < size; i++) {
    st->buckets[i] = NULL; // set all values to NULL (initialization)
  }
  return st;
}

void st_resize(SymbolTable *st, int new_size) {
  Entry **new_buckets = malloc(new_size * sizeof(Entry *));
  for (int i = 0; i < new_size; i++) {
    new_buckets[i] = NULL;
  }

  for (size_t i = 0; i < st->size; i++) {
    Entry *entry = st->buckets[i];
    while (entry) {
      Entry *next = entry->next;

      /*
       * ALGORITHMIC/MATH OPTIMIZATIONS:
       * 1. Cached Hash: entry->hash is reused. Calculating string hashes is O(length),
       *    so reusing the cached hash avoids redundant CPU work during resizing.
       * 2. Bitwise Modulo: Since new_size is guaranteed to be a power of two,
       *    h % new_size is mathematically equivalent to h & (new_size - 1).
       *    Bitwise AND is a single-cycle CPU operation, while integer modulo division (%)
       *    can take up to 40 CPU cycles.
       */
      unsigned long h = entry->hash;
      int index = h & (new_size - 1);

      entry->next = new_buckets[index];
      new_buckets[index] = entry;

      entry = next;
    }
  }

  free(st->buckets);
  st->buckets = new_buckets;
  st->size = new_size;
  st->resizes++;
}

void st_set(SymbolTable *st, const char *key, Symbol *value) {
  // resizing check
  float load = (float)st->count / st->size;
  if (load > 0.75f) {
    st_resize(st, st->size * 2);
  }

  unsigned long h = hash(key);
  /* Bitwise Modulo optimization: h & (st->size - 1) instead of h % st->size */
  int index = h & (st->size - 1);

  Entry *entry = st->buckets[index];
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      free(entry->symbol);
      entry->symbol = value;
      return;
    }
    entry = entry->next;
  }

  Entry *new_entry = malloc(sizeof(Entry));
  if (!new_entry)
    return;

  new_entry->key = strdup(key);
  new_entry->hash = h; /* Cache the hash for future resize operations */
  new_entry->symbol = value;

  new_entry->next = st->buckets[index];

  st->buckets[index] = new_entry;
  st->count++;
}

Symbol *st_get(SymbolTable *st, const char *key) {
  unsigned long h = hash(key);
  /* Bitwise Modulo optimization: h & (st->size - 1) instead of h % st->size */
  int index = h & (st->size - 1);

  Entry *entry = st->buckets[index];
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      return entry->symbol;
    }
    entry = entry->next;
  }
  return NULL;
}

int st_delete(SymbolTable *st, const char *key) {
  unsigned long h = hash(key);
  /* Bitwise Modulo optimization: h & (st->size - 1) instead of h % st->size */
  int index = (int)(h & (st->size - 1));

  Entry *entry = st->buckets[index];
  Entry *prev = NULL;
  while (entry != NULL) {
    if (strcmp(entry->key, key) == 0) {
      if (prev == NULL) {
        st->buckets[index] = entry->next;
      } else {
        prev->next = entry->next;
      }
      free(entry->symbol);
      free(entry->key);
      free(entry);
      st->count--;
      return 1;
    }
    prev = entry;
    entry = entry->next;
  }
  return 0;
}

void st_free(SymbolTable *st) {
  int size = st->size;
  for (int index = 0; index < size; index++) {
    Entry *entry = st->buckets[index];
    while (entry != NULL) {
      Entry *next = entry->next;
      free(entry->key);
      free(entry->symbol);
      free(entry);
      entry = next;
    }
  }
  free(st->buckets);
  free(st);
}

unsigned long hash(const char *str) {
  unsigned long hash = 5381;
  for (int i = 0; str[i] != '\0'; i++) {
    hash = hash * 33 + str[i];
  }
  return hash;
}
