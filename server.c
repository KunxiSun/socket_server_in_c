/*
    C  socket server!!

    Handler multiple connection useing threads

    Provide main operations including: 
        echo,
        directory listing,
        file size query,
        retrieve file
*/

#include <arpa/inet.h>
#include <dirent.h>
#include <math.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

#include "bitwise.h"
#include "compression.h"
#include "id-storage.h"

#define BUFLEN (1024)                   // initial buffer length
#define PTHREAD_N (100)                 // maximum thread nunmber
#define DIRECTORY_PATH_LEN (50)         // direction path length
#define DICT_PATH ("compression.dict")  // the path of dictionary

/*
 * this is all the configruation needed by the server
 * to handle request
 */
struct configuration {
  uint16_t port;
  uint32_t ip;
  char directory_path[DIRECTORY_PATH_LEN];

  struct dict* dict;
  struct decode_tree* decode_tree;
  struct sessions* sessions;
};

/*
 * this store all infomation received from recv()
 */
struct conc_data {
  int type;      // type
  int compd;     // compressed
  int req_comp;  // required compresse

  uint8_t* payload;      // payload content
  uint64_t payload_len;  // payload length
  uint64_t total_len;    // buffer total length
};

/*
  Global configuration variable,
  will only be initilized ONCE in main
 */
struct configuration* config;

/*
 * Given a buffer, return the payload length
 */
int get_payload_length(uint8_t* buffer) {
  uint8_t arr[8];
  for (int i = 0; i < 8; i++) {
    arr[i] = buffer[8 - i];
  }

  return *((uint64_t*)arr);
}

/*
 * Given a size, set up payload length into buffer
 * which is the 1st byte to 9th byte
 */
void modify_payload_len(uint8_t* buffer, int size) {
  uint64_t pl_len_in64 = htobe64(size);
  uint8_t* ptr = (uint8_t*)&pl_len_in64;
  for (int i = 1; i < 9; i++) {
    buffer[i] = ptr[i - 1];
  }
}

/*
 * Given a buffer, set up all info into data
 */
void setup_recv_size(struct conc_data* data, uint8_t* buffer) {
  data->type = buffer[0] >> 4;

  data->payload_len = get_payload_length(buffer);
  if (data->type != (int)0x0 && data->type != (int)0x2 &&
      data->type != (int)0x4 && data->type != (int)0x6 &&
      data->type != (int)0x8) {
    data->payload_len = 0;
  }

  data->compd = ith_bit(buffer[0], 3);     // 5th bit (8-5)
  data->req_comp = ith_bit(buffer[0], 2);  // 6th bit (8-6)

  data->total_len = data->payload_len + 9;
}

/*
 * Given the received buffer, set up payload content into data
 */
void setup_recv_payload(struct conc_data* data, uint8_t* buffer) {
  data->payload = (uint8_t*)malloc(sizeof(uint8_t) * (data->payload_len));
  for (int i = 0; i < data->payload_len; i++) {
    data->payload[i] = buffer[i + 9];
  }
}

/*
 * Read all the configurations into config argument
 */
void read_config(char* arg, struct configuration* config) {
  config = (struct configuration*)config;

  FILE* fp = fopen(arg, "rb");

  /* init ip and port*/
  fread(&config->ip, sizeof(uint32_t), 1, fp);
  fread(&config->port, sizeof(uint16_t), 1, fp);
  config->port = htons(config->port);

  /* init dictrectory path*/
  char c;
  int i = 0;
  while ((c = fgetc(fp)) != EOF) {
    config->directory_path[i++] = c;
  }
  config->directory_path[i] = '\0';
  fclose(fp);

  /* init dict*/
  struct dict* dict = generate_dict(DICT_PATH);
  config->dict = dict;

  /*init decode treee*/
  struct decode_tree* tree = generate_decode_tree(dict);
  config->decode_tree = tree;

  /*init decode treee*/
  struct sessions* sessions = session_id_storage_init();
  config->sessions = sessions;
}

/*
 *  Provide echo operation in thread handler
 *  Modify the buffer to send
 *  return the new payload length as int
 */
int echo(uint8_t** buffer_send,
         uint8_t** buffer_recv,
         struct conc_data* recv_data) {

  // check if request compression
  if (recv_data->compd == 0 && recv_data->req_comp == 1) {
    int pl_len = compress(config->dict, buffer_send, buffer_recv,
                          recv_data->payload_len);

    return pl_len;
  }

  // send back the same message, but change the type
  (*buffer_send) =
      realloc((*buffer_send), sizeof(uint8_t) * recv_data->total_len);
  memcpy(*buffer_send, *buffer_recv, sizeof(uint8_t) * recv_data->total_len);

  // modify type
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 4, 1);
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 5, 0);
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 6, 0);
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 7, 0);

  // modify require compression
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 2, 0);

  return recv_data->payload_len;
}

/*
 *  Provide directory listing operation in thread handler
 *  Modify the buffer to send
 *  return the new payload length as int
 */
int directory_listing(uint8_t** buffer_send,
                      uint8_t** buffer_recv,
                      char* directory_path) {
  DIR* d;
  struct dirent* dir;
  int ctr = 0;  // count regular file number
  int payload_index = 0;

  // read file name
  d = opendir(directory_path);
  if (d) {
    while ((dir = readdir(d)) != NULL) {
      if (dir->d_type == DT_REG) {
        for (int i = 0; i < strlen(dir->d_name); i++) {
          (*buffer_send)[payload_index + 9] = dir->d_name[i];
          payload_index++;
        }
        (*buffer_send)[payload_index + 9] = 0x00;
        payload_index++;
        ctr++;
      }
    }
    closedir(d);
  }

  // directory is empty, return a signle null payload
  if (ctr == 0) {
    modify_payload_len((*buffer_send), 1);
    (*buffer_send)[9] = 0x00;

    // modify type
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 4, 1);
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 5, 1);

    // modify compression state
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 3, 0);

    // modify require compression state
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 2, 0);
    return 1;
  }

  // modify buffer payload len
  modify_payload_len((*buffer_send), payload_index);

  // require compression
  if ((ith_bit((*buffer_recv[0]), 2)) == 1) {
    uint8_t* copy = (uint8_t*)malloc(payload_index + 9);
    memcpy(copy, (*buffer_send), payload_index + 9);
    payload_index = compress(config->dict, buffer_send, &copy,
                             get_payload_length(*buffer_send));
    free(copy);

    // modify compression state
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 3, 1);

    // modify require compression state
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 2, 0);
  }

  // modify type
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 4, 1);
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 5, 1);

  return payload_index;
}

/*
 *  The helper function of size query
 *   return file size: if found the file
 *          0: if file not found
 */
int size_query_helper(char* base_dir_path, char* filename) {
  // generate file path
  char file_path[FILENAME_LEN];
  strcpy(file_path, base_dir_path);
  strcat(file_path, "/");
  strcat(file_path, filename);
  FILE* fp = fopen(file_path, "r");
  if (fp) {
    // found file! calculate size
    fseek(fp, 0, SEEK_END);
    int size = ftell(fp);

    fclose(fp);
    return size;
  }

  return -1;  // not found
}

/*
 *  Provide size query operation in thread handler
 *  Modify the buffer to send
 *  return the new payload length as int
 */
int size_query(uint8_t** buffer_send,
               uint8_t** buffer_recv,
               struct conc_data* recv_data) {
  char* filename = (char*)recv_data->payload;

  // Use helper function to find file,
  // if not found, modify buffer to error
  int size = size_query_helper(config->directory_path, filename);
  if (size < 0) {
    (*buffer_send)[0] = 0xf0;
    return 0;
  }

  // update payload len
  modify_payload_len((*buffer_send), 8);

  // update file size into buffer
  uint64_t pl_in64 = htobe64(size);
  uint8_t* ptr = (uint8_t*)&pl_in64;
  for (int i = 0; i < 8; i++) {
    (*buffer_send)[i + 9] = ptr[i];
  }

  // payload size is defaultly 8 byte
  int pl_size = 8;

  // check compression request
  if (recv_data->req_comp == 1) {
    uint8_t* copy = (uint8_t*)malloc(pl_size + 9);
    memcpy(copy, *buffer_send, pl_size + 9);

    // update payload length after compressed
    pl_size = compress(config->dict, buffer_send, &copy,
                       get_payload_length(*buffer_send));
    free(copy);

    // modify compression state
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 2, 0);
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 3, 1);
  }

  // modify type
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 6, 1);
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 4, 1);

  return pl_size;
}

/*
 *  Provide retrieve file operation in thread handler
 *  Modify the buffer to send
 *  return the new payload length as int
 */
int retrieve_file(uint8_t** buffer_send,
                  uint8_t** buffer_recv,
                  struct conc_data* recv_data) {
  if (recv_data->payload_len < 20) {
    return -1;
  }
  int pl_len = (int)recv_data->payload_len;

  // decompress
  if (recv_data->compd == 1) {
    uint8_t* copy =
        (uint8_t*)malloc(sizeof(uint8_t) * (recv_data->payload_len + 9));
    memcpy(copy, *buffer_recv, recv_data->payload_len + 9);
    pl_len = decompress(config->decode_tree, buffer_recv, &copy,
                        recv_data->payload_len);
    setup_recv_size(recv_data, *buffer_recv);
    setup_recv_payload(recv_data, *buffer_recv);
  }

  // create new session entry
  struct id_entry* session =
      new_id_entry(buffer_recv, (int)get_payload_length(*buffer_recv));
  int res = session_id_storage_add(&(config->sessions->root), session);
  if (res < 0) {
    (*buffer_send)[0] = 0x70;
    modify_payload_len(*buffer_send, 0);
    return 0;
  }

  // adjust buffer size
  *buffer_send = realloc(*buffer_send, session->data_len + 20 + 9);

  // generate file path
  char file_path[FILENAME_LEN];
  strcpy(file_path, config->directory_path);
  strcat(file_path, "/");
  strcat(file_path, session->filename);

  // write data into buffer_send,
  // change buffer to error type if  file not found,
  FILE* fp = fopen(file_path, "r");
  if (!fp) {
    (*buffer_send)[0] = 0xf0;
    return 0;
  }
  fseek(fp, session->start_offset, SEEK_SET);
  uint8_t* ptr = &(*buffer_send)[20 + 9];
  uint64_t bytes_read = fread(ptr, sizeof(uint8_t), session->data_len, fp);
  fclose(fp);

  // send error type if bad range
  if (bytes_read != session->data_len) {
    (*buffer_send)[0] = 0xf0;
    return 0;
  }
  pl_len = bytes_read + 20;

  // copy id, star_offs, data_len into buffer_send,
  // and overwrite payload length
  memcpy(*buffer_send, *buffer_recv, 20 + 9);
  modify_payload_len((*buffer_send), bytes_read + 20);

  // compress
  if (recv_data->req_comp == 1) {
    uint8_t* copy = (uint8_t*)malloc(bytes_read + 20 + 9);
    memcpy(copy, (*buffer_send), bytes_read + 20 + 9);

    // update payload length
    pl_len = compress(config->dict, buffer_send, &copy,
                      get_payload_length(*buffer_send));
    free(copy);
    // modify compression state
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 2, 0);  // compressed
    (*buffer_send)[0] = modify_bit((*buffer_send)[0], 3, 1);  // req_compr
  }

  // modify type
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 6, 1);
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 5, 1);
  (*buffer_send)[0] = modify_bit((*buffer_send)[0], 4, 1);

  return pl_len;
}

/*
 *  Thread handler
 * agr - client socket or new socket generated from accept()
 */
void* connection_handler(void* arg) {
  uint8_t buffer[9];

  int client_sock = *(int*)arg;

  while (1) {
    ssize_t to_read;
    ssize_t recvd;
    uint8_t* ptr = &buffer[0];

    // Get 9 bytes header
    recvd = recv(client_sock, ptr, 9, 0);
    if (recvd <= 0) {
      break;
    }

    // malloc some sapce to store recv info
    struct conc_data* recv_data =
        (struct conc_data*)malloc(sizeof(struct conc_data));

    // read and payload length
    setup_recv_size(recv_data, buffer);

    // now we know the length, generate a speicic size of buffer
    uint8_t* buffer_recv =
        (uint8_t*)malloc(sizeof(uint8_t) * recv_data->total_len);

    // copy the first 9 byte
    memcpy(buffer_recv, buffer, 9);

    // Get the rest of all data
    ptr = &buffer_recv[9];
    to_read = recv_data->payload_len;
    while (to_read) {
      recvd = recv(client_sock, ptr, to_read, 0);

      if (recvd < 0) {
        free(buffer_recv);
        free(recv_data);
        close(client_sock);
        pthread_exit(NULL);
        return NULL;
      } else if (recvd == 0) {
        break;
      }
      to_read -= recvd;
      ptr += recvd;
    }

    // read and store payload
    setup_recv_payload(recv_data, buffer_recv);

    // generate a send buffer and initilize all as 0
    uint8_t* buffer_send = (uint8_t*)malloc(sizeof(uint8_t) * (BUFLEN + 9));
    memset(buffer_send, 0x00, 1024 + 9);

    int send_payload_len;  // length of payload to send

    switch (recv_data->type) {
      case (int)0x0:
        // echo
        send_payload_len = echo(&buffer_send, &buffer_recv, recv_data);
        send(client_sock, buffer_send, send_payload_len + 9, 0);

        break;
      case (int)0x2:
        // directory listing
        send_payload_len = directory_listing(&buffer_send, &buffer_recv,
                                             config->directory_path);

        send(client_sock, buffer_send, send_payload_len + 9, 0);

        break;
      case (int)0x4:
        // file size query
        send_payload_len = size_query(&buffer_send, &buffer_recv, recv_data);

        send(client_sock, buffer_send, send_payload_len + 9, 0);

        break;
        break;
      case (int)0x6:
        // retrieve file
        send_payload_len = retrieve_file(&buffer_send, &buffer_recv, recv_data);
        send(client_sock, buffer_send, send_payload_len + 9, 0);
        break;
      case (int)0x8:
        // shutdown
        free(recv_data->payload);
        free(recv_data);
        free(buffer_send);
        free(buffer_recv);
        close(client_sock);
        pthread_exit(NULL);
        exit(0);
        break;
      default:
        // error
        buffer[0] = 0xf0;  // or 0xf<<4
        for (int i = 1; i < 9; i++) {
          buffer[i] = 0x00;
        }
        // send error type
        send(client_sock, buffer, recv_data->total_len, 0);
        close(client_sock);
        break;
    }

    //free memory
    free(recv_data->payload);
    free(recv_data);
    free(buffer_send);
    free(buffer_recv);
  }
  close(client_sock);
  pthread_exit(NULL);
  return NULL;
}

int main(int argc, char** argv) {
  // There should be a configuration file at argv[1]
  if (argc != 2) {
    puts("Invalid input");
    exit(1);
  }

  // read config file
  config = (struct configuration*)malloc(sizeof(struct configuration));
  read_config(argv[1], config);

  // socket
  int serverSock = -1;
  int option = 1;
  struct sockaddr_in address;
  serverSock = socket(AF_INET, SOCK_STREAM, 0);
  if (serverSock < 0) {
    puts("Sock failed!");
    exit(1);
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = config->ip;
  address.sin_port = htons(config->port);

  setsockopt(serverSock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &option,
             sizeof(int));

  // bind
  if (bind(serverSock, (struct sockaddr*)&address, sizeof(address)) < 0) {
    puts("Bind failed!");
    exit(1);
  }

  // listen
  listen(serverSock, 10);

  // thread index
  int tid_idx = 0;

  // thread array
  pthread_t tids[PTHREAD_N];

  // client sockat array
  int client_socks[PTHREAD_N];

  while (1) {
    // accept
    uint32_t addrlen = sizeof(struct sockaddr_in);
    client_socks[tid_idx] =
        accept(serverSock, (struct sockaddr*)&address, &addrlen);

    pthread_create(&tids[tid_idx], NULL, connection_handler,
                   (void*)(&client_socks[tid_idx]));
    pthread_detach(tids[tid_idx]);
    tid_idx += 1;
  }

  // free memopoy, but this part will not be reached
  session_id_storage_destory(config->sessions);
  destory_decode_tree(config->decode_tree);
  free(config->dict);
  free(config);

  return 0;
}