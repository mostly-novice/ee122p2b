#include <stdio.h>
#include <stdlib.h>
#include "model.h"
#include "constants.h"
#include "messages.h"

int process_login_reply(char payload_c[],Player * self){
  struct login_reply * lreply = (struct login_reply *) payload_c;
  check_malformed_stats(lreply->x,lreply->x,ntohl(lreply->hp),ntohl(lreply->exp));
  on_login_reply(lreply->error_code);
  if(lreply->error_code == 0){
    self->hp = ntohl(lreply->hp);
    self->exp = ntohl(lreply->exp);
    self->x = lreply->x;
    self->y = lreply->y;
    return 1;
  }
  return 0;
}

int process_move_notify(char payload_c[], Player * self, LinkedList * mylist){
  struct move_notify * mn = (struct move_notify *) payload_c;
  Node *p;
  check_malformed_move(mn->name,mn->x,mn->y,ntohl(mn->hp),ntohl(mn->exp));

  if (strcmp(mn->name,self->name)==0){ // If the guy is self
    self->hp = ntohl(mn->hp);
    self->exp = ntohl(mn->exp);
    self->x = mn->x;
    self->y = mn->y;
    on_move_notify(self->name, self->x, self->y, self->hp,self->exp);
  } else { // The guy is someone else
    p = findPlayer(mn->name,mylist);
    if(p == NULL){ // Not in the list
      Node * node = (Node*) malloc(sizeof(Node)); // TODO: remember to free this
      Player * newplayer = (Player*) malloc(sizeof(Player)); // TODO: remember to free this first
      initialize(newplayer,mn->name,ntohl(mn->hp),ntohl(mn->exp),mn->x,mn->y);
      node->datum = newplayer;
      node->next = NULL;
      if(mylist->tail == NULL && mylist->head == NULL){ // First player
	mylist->tail = node;
	mylist->head = node;
      } else {
	mylist->tail->next = node;
	mylist->tail = node;
      }
    } else { // The guy is in the list
      int oldv = isVisible(self->x,self->y,p->datum->x,p->datum->y);
      int newv = isVisible(self->x,self->y,mn->x,mn->y);
      initialize(p->datum,mn->name,ntohl(mn->hp),ntohl(mn->exp),mn->x,mn->y);
      if(oldv || newv){on_move_notify(p->datum->name, p->datum->x, p->datum->y, p->datum->hp,p->datum->exp);}
    } // End inner if
  }
  checkSelfVision(self->x,self->y,mylist);
}

int process_attack_notify(char payload_c[], Player * self, LinkedList * mylist){
  struct attack_notify * an = (struct attack_notify *)payload_c;
  check_malformed_attack(an->attacker_name,an->victim_name,an->damage,ntohl(an->hp));
  Player * att;
  Player * vic;

  if (strcmp(an->attacker_name,self->name)==0){
    att = self;
  } else {
    Node * att_node = findPlayer(an->attacker_name,mylist);
    att = att_node->datum;
  }

  if (strcmp(an->victim_name,self->name)==0){
    vic = self;
  } else {
    Node * vic_node = findPlayer(an->victim_name,mylist);
    vic = vic_node->datum;
  }

  int updated_hp = ntohl(an->hp);
  char damage = an->damage;

  // Check the visibility
  int attVisible = isVisible(self->x,self->y,att->x,att->y);
  int vicVisible = isVisible(self->x,self->y,vic->x,vic->y);

  if (attVisible && vicVisible){ on_attack_notify(att->name,vic->name,damage,updated_hp); }
}

int process_speak_notify(char payload_c[]){
  struct speak_notify* sreply = (struct speak_notify*) payload_c;
  unsigned char * broadcaster = sreply->broadcaster;

  char * start = ((char*)payload_c)+10;
  check_malformed_speak(broadcaster,start);

  // null terminated & no longer than 255
  if(!check_player_message(start)){ printf("! Invalid format\n"); return 0;}		
  printf("%s: %s\n",broadcaster,start);
  show_prompt();
  return 1;
}
int process_logout_notify(char payload_c[], LinkedList * mylist){
  struct logout_reply * loreply = (struct logout_reply *) payload_c;
  check_malformed_logout(loreply->name);
  if (!removePlayer(loreply->name, mylist)){
    perror("LOGOUT_NOTIFY - remove player");
    exit(-1);
  }
  
  printf("Player %s has left the tiny world of warcraft.\n",loreply->name);
  show_prompt();
}

int process_invalid_state(char payload_c[]){
  struct invalid_state * is = (struct invalid_state *) payload_c;
  on_invalid_state(is->error_code);
}
