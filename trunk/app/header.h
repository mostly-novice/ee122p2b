struct header {
  unsigned char version;
  unsigned short len;
  unsigned char msgtype;
}__attribute__((packed));

// Payload Type
struct login_request {
  unsigned char name[10];
  unsigned int hp;
  unsigned int exp;
  unsigned char x;
  unsigned char y;
}__attribute__((packed));

struct logout_reply{
  unsigned  char name [10];
  unsigned char padding[2];
}__attribute__((packed));

struct login_reply {
  unsigned char error_code;
  unsigned int hp;
  unsigned int exp;
  unsigned char x;
  unsigned char y;
  unsigned char padding;
}__attribute__((packed));

struct move_notify {
  unsigned char name[10];
  unsigned char x;
  unsigned char y;
  unsigned int hp;
  unsigned int exp;
}__attribute__((packed));


struct move {
  unsigned char direction;
  unsigned char padding[3];
}__attribute__((packed));

struct attack {
  char victimname[10];
  char padding[2];
}__attribute__((packed));

struct attack_notify {
  unsigned char attacker_name[10];
  unsigned char victim_name[10];
  unsigned char damage;
  unsigned int hp; // Specifies the updated HP of the victim in network-byte-order. Cannot be negative
  char padding[3];
}__attribute__((packed));

struct speak {
  char * msg;
  char * padding;
}__attribute__((packed));

struct speak_notify {
  unsigned char broadcaster[10];
  unsigned char * msg;
}__attribute__((packed));

struct logout_notify {
  unsigned char name[10];
  unsigned int hp;
  unsigned int exp;
  unsigned char x;
  unsigned char y;
  unsigned short padding;
}__attribute__((packed));

struct invalid_state {
  unsigned char error_code;
}__attribute__((packed));

struct storage_location_request {
  unsigned char message_type;
  unsigned int id;
  unsigned char name[10];
  unsigned char padding;
}__attribute__((packed));

struct storage_location_response {
  unsigned char message_type;
  unsigned int id;
  unsigned int server_ip;
  unsigned short udpport;
}__attribute__((packed));

struct server_area_request {
  unsigned char message_type;
  unsigned int id;
  unsigned char x;
  unsigned char y;
  unsigned char paddding;
}__attribute__((packed));

struct server_area_response {
  unsigned char message_type;
  unsigned int id;
  unsigned int server_ip;
  unsigned short tcpport;
  unsigned char min_x;
  unsigned char max_x;
  unsigned char min_y;
  unsigned char max_y;
  unsigned char paddding;
}__attribute__((packed));

struct player_state_request {
  unsigned char message_type;
  unsigned int id;
  unsigned char name[10];
  unsigned char paddding;
}__attribute__((packed));

struct player_state_response {
  unsigned char message_type;
  unsigned int id;
  unsigned char name[10];
  unsigned int hp;
  unsigned int exp;
  unsigned char x;
  unsigned char y;
}__attribute__((packed));

struct save_state_request {
  unsigned char message_type;
  unsigned int id;
  unsigned char name[10];
  unsigned int hp;
  unsigned int exp;
  unsigned char x;
  unsigned char y;
  unsigned char padding[3];
}__attribute__((packed));

struct save_state_response {
  unsigned char message_type;
  unsigned int id;
  unsigned char error_code;
  unsigned short padding;
}__attribute__((packed));

struct p2p_join_request {
  unsigned int p2p_id;
}__attribute__((packed));


struct p2p_join_response {
  unsigned int usernumber;
  unsigned char * userdata;
}__attribute__((packed));

struct p2p_bkup_request {
  unsigned char MsgVer;
  unsigned short MsgLength;
  unsigned char MsgType;
  unsigned char User_Data[20];
}__attribute__((packed));

struct p2p_bkup_response {
  unsigned char MsgVer;
  unsigned short MsgLength;
  unsigned char MsgType;
  unsigned char error_code;
  unsigned char padding[3];
}__attribute__((packed));
