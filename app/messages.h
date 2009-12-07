/**************************************************************************
 * The reference server implementation of the EE122 Project #1
 *
 * Author: Junda Liu (liujd@cs)
 * Author: DK Moon (dkmoon@cs)
 * Author: David Zats (dzats@cs)
**************************************************************************/

#ifndef _messages_h_
#define _messages_h_

#include <assert.h>

#include "constants.h"

/**************************************************************************
  * Message functions for the client
  ************************************************************************/
static inline void show_prompt()
{
  fprintf(stdout, "command> ");
  fflush(stdout);
}

/* Error messages for the client. */
static inline void on_client_connect_failure()
{
  fprintf(stdout,
          "The gate to the tiny world of warcraft is not ready.\n");
}

static inline void on_malformed_message_from_server()
{
  fprintf(stdout, "Meteor is striking the world.\n");
  abort();
}

static inline void on_disconnection_from_server()
{
  fprintf(stdout,
          "The gate to the tiny world of warcraft has disappeared.\n");
}

static inline void on_not_visible()
{
  fprintf(stdout, "The target is not visible.\n");
  show_prompt();
}

static inline void on_login_reply(const int error_code)
{
  if (error_code == 0) {
    fprintf(stdout, "Welcome to the tiny world of warcraft.\n");
  } else if (error_code == 1) {
    fprintf(stdout, "A player with the same name is already in the game.\n");
  } else {
    /* This must not happen. The caller of this function must check the
       error code is either 0 or 1 and must perform malformed message
       handling before this function. We simply die here. */
    assert(0);
  }
  show_prompt();
}

static inline void on_move_notify(const char *player_name,
                                  const uint8_t x,
                                  const uint8_t y,
                                  const uint32_t hp,
                                  const uint32_t exp)
{
  fprintf(stdout, "%s: location=(%u,%u), HP=%u, EXP=%u\n",
          player_name, x, y, hp, exp);
  show_prompt();
}

static inline void on_attack_notify(const char *attacker_name,
                                    const char *victim_name,
                                    const int damage,
                                    const int updated_hp)
{
  if (updated_hp <= 0) {
    fprintf(stdout, "%s killed %s\n", attacker_name, victim_name);
  } else {
    fprintf(stdout, "%s damaged %s by %d. %s's HP is now %d\n",
            attacker_name, victim_name, damage, victim_name, updated_hp);
  }
  show_prompt();
}

static inline void on_speak_notify(const char *speaker, const char *message)
{
  fprintf(stdout, "%s: %s\n", speaker, message);
  show_prompt();
}

static inline void on_exit_notify(const char *player_name)
{
  fprintf(stdout, "Player %s has left the tiny world of warcraft.\n",
          player_name);
  show_prompt();
}

static inline void on_invalid_state(const int error_code)
{
  if (error_code == 0) {
    fprintf(stdout, "You must log in first.\n");
  } else if (error_code == 1) {
    fprintf(stdout, "You already logged in.\n");
  } else {
    /* This must not happen. The caller of this function must check the
       error code is either 0 or 1, and must perform malformed message
       handling before this function. We simply die here. */
    assert(0);
  }
  show_prompt();
}


/**************************************************************************
  * Message functions for the server
  ************************************************************************/
/* Error messages for the server. */
static inline const char *message_on_server_port_bind_failure()
{
  return "The gate cannot be opened there.";
}



/**************************************************************************
  * Utility functions
  ************************************************************************/
/* Returns 1 if the name is valid. */
static inline int check_player_name(const char *name)
{
  size_t i;
  /* Must not be null. */
  if (!name[0]){
	  return 0;
	  printf("broke test 1\n");
  }
  for (i = 0; i <= EE122_MAX_NAME_LENGTH; ++ i) {
    if (!name[i]) return 1;
    /* Must not contain a non-alphanumeric character. */
    if (!isalnum(name[i])){
	    return 0;
	    printf("broke test 2\n");
    }
  }
  /* Must be null terminated. */
  printf("broke test 3\n");
  return 0;
}


/* Returns if 1 if the text message is valid. */
static inline int check_player_message(const char *message)
{
  size_t i;
  /* Must not be null. */
  if (!message[0]){
    printf("Message must not be null.\n");
    return 0;
  }
  for (i = 0; i <= EE122_MAX_MSG_LENGTH; ++ i) {
    if (!message[i]) return 1;
    /* Must not contain a non-printable character. */
    if (!isprint(message[i])){
      printf("Message must not contain a non-printable character.\n");
      return 0;
    }
  }
  /* Must be null terminated. */
  printf("Message must not be null terminated.\n");
  return 0;
}

static inline int check_malformed_header(int version, int len, int msgtype){
  if(version != 4){
    printf("Version is invalid - %d\n", version);
    return -1;
  }

  if(len%4 != 0 || len > 260){
    printf("Len is invalid - %d\n", len);
    return -1;
  }
  if(msgtype<0 || msgtype>0xb){
    printf("Msgtype is invalid - %d\n", msgtype);
    return -1; 
  }
  return 1;
}

static inline int check_malformed_stats(int x, int y, int hp, int exp){
  if(x < 0 || x > 99){ printf("BAD X/Y\n"); on_malformed_message_from_server(); }
  if(y < 0 || y > 99){ printf("BAD X/Y\n"); on_malformed_message_from_server();}
  if(hp < 1){
	  printf("BAD HP\n");
	  on_malformed_message_from_server();
  }
  if(exp < 0){
	  printf("BAD EXP\n");
	  on_malformed_message_from_server();
  }
  return 0;
}

static inline int check_malformed_move(char*name,int x, int y, int hp, int exp){
  if (!check_player_name(name)) on_malformed_message_from_server();
  if(x < 0 || x > 99){ on_malformed_message_from_server(); }
  if(y < 0 || y > 99){ on_malformed_message_from_server();}
  if(hp < 1){on_malformed_message_from_server();}
  if(exp < 0){on_malformed_message_from_server();}
  return 0;
}

static inline int check_malformed_attack(char*name1,char*name2,char damage, int updatedhp){ 
  if (!check_player_name(name1)) on_malformed_message_from_server();
  if (!check_player_name(name2)) on_malformed_message_from_server();
  if (damage < 10 || damage > 20) on_malformed_message_from_server();
  if (updatedhp < 0){ on_malformed_message_from_server(); }
  return 0;
}

static inline int check_malformed_speak(char*name,char*msg){
  if (!check_player_name(name)){
    printf("Malformed Speak: bad name\n");
    return -1;
  }
  if (!check_player_message(msg)){
    printf("Malformed Speak: bad msg\n");
    return -1;
  }
  return 0;
}

static inline int check_malformed_logout(char*name){
  if (!check_player_name(name)) on_malformed_message_from_server();
  return 0;
}


#endif /* _messages_h_ */
