#include <stdio.h>
#include <stdlib.h>
#include "id-storage.h"

/*
  Generate a new entry based on the buffer and payload length
*/
struct id_entry* new_id_entry(uint8_t** buffer, int pl_len) {
  struct id_entry* new_entry =
      (struct id_entry*)malloc(sizeof(struct id_entry));

  // get session id
  uint8_t session_id_arr[4];
  for (int i = 0; i < 4; i++) {
    session_id_arr[i] = (*buffer)[3 - i + 9];
  }
  uint32_t session_id = *((uint32_t*)session_id_arr);

  // get starting offset
  uint8_t sta_off_arr[8];
  for (int i = 0; i < 8; i++) {
    sta_off_arr[i] = (*buffer)[11 - i + 9];
  }
  uint64_t starting_offset = *((uint64_t*)sta_off_arr);

  // get data length
  uint8_t data_len_arr[8];
  for (int i = 0; i < 8; i++) {
    data_len_arr[i] = (*buffer)[19 - i + 9];
  }
  uint64_t len_to_read = *((uint64_t*)data_len_arr);

  // get file name
  char filename[FILENAME_LEN];
  for (int i = 20; i < pl_len; i++) {
    filename[i - 20] = (*buffer)[i + 9];
  }

  // assign values
  new_entry->data_len = len_to_read;
  new_entry->filename = filename;
  new_entry->session_id = session_id;
  new_entry->start_offset = starting_offset;

  new_entry->left = NULL;
  new_entry->right = NULL;

  return new_entry;
}

/*
  initilize a session id storage tree
*/
struct sessions* session_id_storage_init() {
  struct sessions* session = (struct sessions*)malloc(sizeof(struct sessions));
  session->root = NULL;

  return session;
}

/*
  Use recursion to add a storage entry into storage tree
  return -1 if found an entry with same session id
  return 1 if suessfully add entry into tree
*/
int session_id_storage_add(struct id_entry** root, struct id_entry* entry) {
  if ((*root) == NULL) {
    (*root) = entry;
    return 1;
  }

  if ((*root)->session_id < entry->session_id) {
    return session_id_storage_add(&((*root)->left), entry);
  } else if ((*root)->session_id > entry->session_id) {
    return session_id_storage_add(&((*root)->right), entry);
  } else {
    return -1;
  }
}

/*
  Use recursion to remove a entry with the same session id
  return -1 if not found such entry
  return 1 if found and remove such entry
*/
int session_id_storage_remove(struct id_entry** root, struct id_entry* entry) {
  if (root == NULL) {
    return -1;  // not found
  }

  if ((*root)->session_id < entry->session_id) {
    return session_id_storage_remove(&((*root)->left), entry);
  } else if ((*root)->session_id > entry->session_id) {
    return session_id_storage_remove(&((*root)->right), entry);
  } else {
    free((*root));
    (*root) = NULL;
    return 1;  // found and removed
  }
}

/*
  Use recursion to get the entry which a specific session id
  return NULL if not found 
  return pointer to the entry if found
*/
struct id_entry* session_id_storage_get(struct id_entry** root,
                                        uint64_t session_id) {
  if ((*root) == NULL) {
    return NULL;  // not found
  }

  if ((*root)->session_id < session_id) {
    return session_id_storage_get(&((*root)->left), session_id);
  } else if ((*root)->session_id > session_id) {
    return session_id_storage_get(&((*root)->right), session_id);
  } else {
    return (*root);  // found and return
  }
}

/*
  A recursion function to recursively free memory
  from root of tree
*/
void destory_helper(struct id_entry* root) {
  if (root == NULL) {
    return;
  }

  destory_helper(root->left);
  destory_helper(root->right);

  free(root);
}

/*
  Free all memory usage of session id tree
*/
void session_id_storage_destory(struct sessions* session) {
  destory_helper(session->root);
}