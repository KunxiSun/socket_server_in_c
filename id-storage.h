#ifndef ID_STORAGE_H /* guard */
#define ID_STORAGE_H

/*
  Session id storage.
  The storage is a binary tree data structure.

  Session id and some transfer information can be
  globally stored in the tree
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>


#define FILENAME_LEN (200)

/*the entry of tree*/
struct id_entry {
  char* filename;
  uint32_t session_id;
  uint64_t start_offset;
  uint64_t data_len;

  struct id_entry* left;
  struct id_entry* right;
};

/*binary tree*/
struct sessions {
  struct id_entry* root;
};

/*
  Generate a new entry based on the buffer and payload length
*/
struct id_entry* new_id_entry(uint8_t** buffer, int pl_len);

/*
  initilize a session id storage tree
*/
struct sessions* session_id_storage_init();

/*
  Use recursion to add a storage entry into storage tree
  return -1 if found an entry with same session id
  return 1 if suessfully add entry into tree
*/
int session_id_storage_add(struct id_entry** root, struct id_entry* entry);

/*
  Use recursion to remove a entry with the same session id
  return -1 if not found such entry
  return 1 if found and remove such entry
*/
int session_id_storage_remove(struct id_entry** root, struct id_entry* entry);

/*
  Use recursion to get the entry which a specific session id
  return NULL if not found 
  return pointer to the entry if found
*/
struct id_entry* session_id_storage_get(struct id_entry** root,
                                        uint64_t session_id);

/*
  Free all memory usage of session id tree
*/
void session_id_storage_destory(struct sessions* session);

#endif //ID_STORAGE_H