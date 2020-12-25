#include <stdio.h>
#include <stdlib.h>
#include "bitwise.h"
#include "compression.h"

#define DICT_SIZE (256)
#define BUF_INITIAL_LEN (1024)

/*
 * given a path to dictionary,
 * generate a pointer to dict
 */
struct dict* generate_dict(char* dict_path) {
  if (dict_path == NULL) {
    return NULL;
  }

  struct dict* dict = (struct dict*)malloc(sizeof(struct dict));

  /* generate an array to store all the bits */
  char bit_arr[8000];
  FILE* fp = fopen(dict_path, "rb");
  int i, j, bit_num, bit_ctr;
  bit_num = 0;
  unsigned int c;
  while ((c = fgetc(fp)) != EOF) {
    for (j = 0; j < 8; j++) {
      bit_arr[bit_num] = ith_bit(c, 7 - j);
      bit_num++;
    }
  }
  fclose(fp);

  /* set all bits as 0 */
  for (i = 0; i < DICT_SIZE; i++) {
    dict->code[i] = 0;
  }

  /* iterates 256 times to store length and code into dict*/
  bit_ctr = 0;
  for (i = 0; i < DICT_SIZE; i++) {
    // read length
    for (j = 0; j < 8; j++) {
      dict->len[i] = modify_bit(dict->len[i], 7 - j, bit_arr[bit_ctr++]);
    }

    // read code
    for (j = 0; j < dict->len[i]; j++) {
      dict->code[i] =
          modify_bit(dict->code[i], dict->len[i] - 1 - j, bit_arr[bit_ctr++]);
    }
  }
  return dict;
}

/*
 * the helper function using recursion to generate decode tree.
 */
void generate_decode_tree_helper(struct node* root,
                                 uint32_t code,
                                 int code_len,
                                 int decode) {
  if (code_len == 0) {
    root->decode = decode;
    return;
  }

  int bit = ith_bit(code, code_len - 1);  // leftmost
  if (bit == 1) {
    if (root->one == NULL) {
      root->one = (struct node*)malloc(sizeof(struct node));
      root->one->decode = -1;
      root->one->one = NULL;
      root->one->zero = NULL;
    }
    generate_decode_tree_helper(root->one, code, code_len - 1, decode);
  } else {
    if (root->zero == NULL) {
      root->zero = (struct node*)malloc(sizeof(struct node));
      root->zero->decode = -1;
      root->zero->zero = NULL;
      root->zero->one = NULL;
    }
    generate_decode_tree_helper(root->zero, code, code_len - 1, decode);
  }
}

/*
 * given a dict, return a decode tree to do uncompression
 */
struct decode_tree* generate_decode_tree(struct dict* dict) {
  if (dict == NULL) {
    return NULL;
  }

  struct decode_tree* tree =
      (struct decode_tree*)malloc(sizeof(struct decode_tree));

  tree->root = (struct node*)malloc(sizeof(struct node));
  tree->root->decode = -1;
  tree->root->one = NULL;
  tree->root->zero = NULL;

  /*iterate 256 times, store all decode into decode tree */
  for (int i = 0; i < DICT_SIZE; i++) {
    generate_decode_tree_helper(tree->root, dict->code[i], (int)dict->len[i],
                                i);
  }
  return tree;
}

/*
 * given dict, buffers, and payload length,
 * generate compressed payload into buffer_send,
 * retrun the payload length after compress
 */
int compress(struct dict* dict,
             uint8_t** buffer_send,
             uint8_t** buffer_recv,
             int payload_len) {
                 
  int bit_ctr = 0;
  int send_pl_len = payload_len;
  memset((*buffer_send), 0x00, send_pl_len + 9);
  int index, bit, pos;
  for (int i = 0; i < payload_len; i++) {
    uint8_t byte = (*buffer_recv)[i + 9];  //  byte in recveved payload
    uint32_t code = dict->code[byte];      //  corrsponding code in dict
    uint8_t code_len = dict->len[byte];    // code length

    for (int j = 0; j < code_len; j++) {
      bit = ith_bit(code, code_len - 1 - j);  //  bit from leftmost to the right
      index = bit_ctr / 8 + 9;                //  index of buffer_send
      pos = 7 - bit_ctr % 8;                  //  position to modify bit

      (*buffer_send)[index] = modify_bit((*buffer_send)[index], pos, bit);

      bit_ctr++;

      /* check buffer size, if buffer is full, realloc it*/
      if (bit_ctr == send_pl_len) {
        send_pl_len *= 2;
        *buffer_send =
            realloc(*buffer_send, sizeof(uint8_t) * (send_pl_len + 9));
        for (int k = send_pl_len / 2 + 9; k < send_pl_len + 9; k++) {
          (*buffer_send)[k] = 0x00;
        }
      }
    }
  }

  /*modify header*/
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 4, 1);  // type
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 3, 1);  // compreesed
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 2, 0);  // req compress

  /* calculate payload length and padding size.
  store padding size into last byte*/
  uint8_t padding_size = 8 - bit_ctr % 8;
  if (padding_size == 8) {
    padding_size = 0;
  }
  int padding_size_index = 0;
  if (padding_size == 0) {
    padding_size_index = bit_ctr / 8;
  } else {
    padding_size_index = (bit_ctr / 8) + 1;
  }

  (*buffer_send)[padding_size_index + 9] = padding_size;

  /* length of payload is larger than index by 1,
   and store payload it to buffer*/
  padding_size_index += 1;

  uint64_t pl_len_in64 = htobe64(padding_size_index);
  uint8_t* ptr = (uint8_t*)&pl_len_in64;
  for (int i = 1; i < 9; i++) {
    (*buffer_send)[i] = ptr[i - 1];
  }

  return padding_size_index;
}

/*
 * The helper function to decompress.
 * Recuresion is used in helper function
 * return the decode(the index of dictionary)
 */
int decompress_helper(struct node* root,
                      uint32_t buffer,
                      int start_pos,
                      int buffer_len) {
  if (buffer_len == 0) {
    return root->decode;
  }

  int bit = ith_bit(buffer, start_pos);
  if (bit == 1) {
    return decompress_helper(root->one, buffer, start_pos - 1, buffer_len - 1);
  } else {
    return decompress_helper(root->zero, buffer, start_pos - 1, buffer_len - 1);
  }
}

/*
 * given the decode tree, buffers, and payload length,
 * decompress the payload in src, and store in dest,
 * return the payload length after decompress
 */
int decompress(struct decode_tree* tree,
               uint8_t** dest,
               uint8_t** src,
               int src_pl_len) {
  int buffer_size = src_pl_len; //record current buffer size
  uint32_t buffer = 0;    // store the bits from the start of payload
  int buffer_index = 31;  // record index and length
  int dest_index = 9;   //the index of dest to store decode
  for (int i = 9; i < src_pl_len + 9; i++) {
    for (int j = 0; j < 8; j++) {
      buffer = modify_bit(buffer, buffer_index, ith_bit((*src)[i], 7 - j));

      /* use recursion to find decode in tree */
      int buffer_len = 32 - buffer_index;
      int decode = decompress_helper(tree->root, buffer, 31, buffer_len);
      buffer_index--;

      if (decode != -1) {
        (*dest)[dest_index++] = decode;
        buffer = 0;
        buffer_index = 31;

        if (dest_index == buffer_size) {
          buffer_size *= 2;
          *dest = realloc(*dest, sizeof(uint8_t) * (buffer_size + 9));
        }
      }
    }
  }

  (*dest)[0] = modify_bit((*dest)[0], 3, 0);
  return dest_index - 9;  // payload length
}

/* free the memory usage of dict */
void destory_dict(struct dict* dict) {
  free(dict);
}

/* helper function to recursively free the memory usage of decode tree */
void destory_decode_tree_helper(struct node* node) {
  if (node == NULL) {
    return;
  }

  destory_decode_tree_helper(node->one);
  destory_decode_tree_helper(node->zero);

  free(node);
}

/* free the memory usage of decode tree */
void destory_decode_tree(struct decode_tree* tree) {
  destory_decode_tree_helper(tree->root);
  free(tree);
}