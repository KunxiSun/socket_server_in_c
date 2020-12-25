#ifndef COMPRESSION_H /* guard */
#define COMPRESSION_H

/*
    Compression handler.
    Contain two main operations: compress and decompress
    Contain two struct: dict and decode tree
    Contain functions to generate and destory these two sturct
    Dict is used to compress; decode tree is used to decompress
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define DICT_SIZE (256)
#define BUF_INITIAL_LEN (1024)

/*dictionary structure, including the code and corresponding length*/
struct dict {
  uint8_t len[DICT_SIZE];
  uint32_t code[DICT_SIZE];
};

/* the node of decode tree, store the decode of dictionary*/
struct node {
  int decode;

  struct node* one;   // 1 bit
  struct node* zero;  // 0 bit
};

/* decode tree, it is a binary tree data sturcture*/
struct decode_tree {
  struct node* root;
};

/*
 * given a path to dictionary,
 * generate a pointer to dict
 */
struct dict* generate_dict(char* dict_path);

/*
 * the helper function using recursion to generate decode tree.
 * note: this should be be encapsulated as a private method in
 * this file
 */
void generate_decode_tree_helper(struct node* root,
                                 uint32_t code,
                                 int code_len,
                                 int decode);

/*
 * given a dict, return a decode tree to do uncompression
 */
struct decode_tree* generate_decode_tree(struct dict* dict);

/*
 * given dict, buffers, and payload length,
 * generate compressed payload into buffer_send,
 * retrun the payload length after compress
 */
int compress(struct dict* dict,
             uint8_t** buffer_send,
             uint8_t** buffer_recv,
             int payload_len);


/*
 * given the decode tree, buffers, and payload length,
 * decompress the payload in src, and store in dest,
 * return the payload length after decompress
 */
int decompress(struct decode_tree* tree,
               uint8_t** dest,
               uint8_t** src,
               int src_pl_len);

/* free the memory usage of dict */
void destory_dict(struct dict* dict);

/* free the memory usage of decode tree */
void destory_decode_tree(struct decode_tree* tree);

#endif //ID_STORAGE_H