/**************************************************************************
 * The reference implementation of the EE122 Project #1
 *
 * Author: Junda Liu (liujd@cs)
 * Author: DK Moon (dkmoon@cs)
 * Author: David Zats (dzats@cs)
**************************************************************************/

#ifndef _constants_h_
#define _constants_h_

#define EE122_VALID_VERSION 4
#define EE122_MAX_NAME_LENGTH 9
#define EE122_SIZE_X 100
#define EE122_SIZE_Y 100
#define EE122_VISION_RANGE 10
#define EE122_MAX_MSG_LENGTH 255

// Size of UDP package, in bytes
#define STORAGE_LOCATION_REQUEST_SIZE 16
#define STORAGE_LOCATION_RESPONSE_SIZE 12
#define SERVER_AREA_REQUEST_SIZE 8
#define SERVER_AREA_RESPONSE_SIZE 16
#define PLAYER_STATE_REQUEST_SIZE 16
#define PLAYER_STATE_RESPONSE_SIZE 28
#define SAVE_STATE_REQUEST_SIZE 28
#define SAVE_STATE_RESPONSE_SIZE 8

#define LOGIN_REPLY_SIZE 16
#define MOVE_NOTIFY_SIZE 24  
#define ATTACK_NOTIFY_SIZE 32
#define SPEAK_NOTIFY_SIZE 20
#define LOGOUT_NOTIFY_SIZE 24
#define INVALID_STATE_SIZE 8

enum messages {
  LOGIN_REQUEST = 1,
  LOGIN_REPLY,
  MOVE,
  MOVE_NOTIFY,
  ATTACK,
  ATTACK_NOTIFY,
  SPEAK,
  SPEAK_NOTIFY,
  LOGOUT,
  LOGOUT_NOTIFY,
  INVALID_STATE,

  MAX_MESSAGE,
};

enum udpmessages {
  STORAGE_LOCATION_REQUEST = 0,
  STORAGE_LOCATION_RESPONSE,
  SERVER_AREA_REQUEST,
  SERVER_AREA_RESPONSE,
  PLAYER_STATE_REQUEST,
  PLAYER_STATE_RESPONSE,
  SAVE_STATE_REQUEST,
  SAVE_STATE_RESPONSE,
};

enum direction {NORTH = 0, SOUTH, EAST, WEST};

#endif /* _constants_h_ */
