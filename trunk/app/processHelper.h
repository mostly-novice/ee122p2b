#include <time.h>
#include "sendhelper.h"


//player = process_login_request(0,i,fdmax,login,lr->name,lr->hp,lr->exp,lr->x,lr->y,mylist);
Player * process_login_request(char errorcode, int sock, int fdmax, fd_set login, unsigned char * n,
			       int hp, int exp, int x, int y, LinkedList * activeList){

  char * name = (char *) malloc(sizeof(char)*(strlen(n)+1));
  strcpy(name,n);
  strcat(name,"\0");
  // make a new instance of Player
  Player * newplayer = (Player *) malloc(sizeof(Player));
  
  
  memcpy(newplayer->name,name,10);
  newplayer->hp = ntohl(hp);
  newplayer->exp = ntohl(exp);
  newplayer->x = x;
  newplayer->y = y;

  unsigned char lrtosent[LOGIN_REPLY_SIZE];
  unsigned char mntosent[MOVE_NOTIFY_SIZE];
  createloginreply(errorcode,
		   newplayer->hp,
		   newplayer->exp,
		   newplayer->x,
		   newplayer->y,
		   lrtosent);
  unicast(sock,lrtosent,LOGIN_REPLY_SIZE);

  if(errorcode == 0){
    Node * p;
    for(p = activeList->head; p != NULL; p = p->next){
      if (strcmp(p->datum->name,newplayer->name)!=0){
	Player * player = p->datum;
	createmovenotify(player->name,
			 player->hp,
			 player->exp,
			 player->x,
			 player->y,
			 mntosent);
	unicast(sock,mntosent,MOVE_NOTIFY_SIZE);
      }
    }

    memset(mntosent,0,MOVE_NOTIFY_SIZE);
    createmovenotify(name,newplayer->hp,
		     newplayer->exp,
		     newplayer->x,
		     newplayer->y,
		     mntosent);
    broadcast(login,sock,fdmax,mntosent,MOVE_NOTIFY_SIZE);
    return newplayer;
  } else {
    return NULL;
  }
}
int process_move(int listener,
		int sock,
		int fdmax,
		fd_set master,
		Player * player,
		char direction,
		LinkedList * mylist){
}

int process_attack(int sock,
		int fdmax,
		fd_set login,
		char * attackername,
		char * victimname,
		LinkedList * mylist){
	if(strcmp(attackername,victimname)){ // If the attacker is different than the victim
		Player * victim = findPlayer(victimname,mylist);
		Player * attacker = findPlayer(attackername,mylist);

		if(victim){ // If victim is not in the list
			int damage = 10+rand()%11; // Randomize the damage from 10 to 20
			if(damage > victim->hp){
				damage = victim->hp;
			}

			victim->hp -= damage;
			attacker->exp += damage;


			// Broadcasting attack_notify
			unsigned char antosent[ATTACK_NOTIFY_SIZE];
			createattacknotify(attacker->name,
					victimname,
					victim->hp,
					damage,
					antosent);
			printMessage(antosent,ATTACK_NOTIFY_SIZE);
			broadcast(login,sock,fdmax,antosent,ATTACK_NOTIFY_SIZE);

			if(victim->hp == 0){
				victim->hp = 30+rand()%21;
				//                We don't change the (x,y) coordinates when a victim dies.
				//                victim->x = rand()%100;
				//                victim->y = rand()%100;


				// Broadcasting move notify
				unsigned char mntosent[MOVE_NOTIFY_SIZE];
				createmovenotify(victim,
						victim->hp,
						victim->exp,
						victim->x,
						victim->y,
						mntosent);
				broadcast(login,sock,fdmax,mntosent,MOVE_NOTIFY_SIZE);
			}
		} else {
		}
	}
}

int process_speak(char * broadcaster,char*message){
}

int process_logout(int sock,
		int fdmax,
		char * name,
		LinkedList * mylist){
}

int processError(int i,
		fd_set login,
		fd_set master,
		LinkedList * mylist,
		char ** fdnamemap,
		bufferdata ** fdbuffermap,
		int fdmax){
	close(i);
	FD_CLR(i,&login);
	FD_CLR(i,&master);
	Player * player = findPlayer(fdnamemap[i],mylist);
	if(player){
		removePlayer(fdnamemap[i],mylist);
		FILE *file = fopen(fdnamemap[i],"w+");
		fprintf(file,"%d %d %d %d",player->hp,player->exp,player->x,player->y);
		fclose(file);

		// Broadcast to other clients
		unsigned char lntosent[LOGOUT_NOTIFY_SIZE];
		createlogoutnotify(fdnamemap[i],lntosent);
		broadcast(login,i,fdmax,lntosent,LOGOUT_NOTIFY_SIZE);

	} else {
		fprintf(stderr, "THIS SHOULD NEVER HAPPEN\n");
		exit(-1);
	}
	cleanNameMap(fdnamemap,i);
	cleanBuffer(fdbuffermap,i);
}

int process_invalid_state(char payload_c[]){
}

// Processing PLAYER_STATE_REQUEST
int process_psr(char* name,
		int udpsock,
		struct sockaddr_in targetsin,
		int id,int oldest,
		message_record** mr_array,
		int badMessageTypeFlag){

  chdir("users");
  FILE * file = fopen(name,"r"); // open the file
  int hp;
  int exp;
  int x;
  int y;

  if(file){ // If this file existed
    fscanf(file,"%d%d%d%d",&hp,&exp,&x,&y);
    fclose(file);
  } else{ // If it doesn't
    // Randomize the data
    srand(time(NULL));
    hp = 100 + rand()%21;
    exp = 0;
    y = rand()%100;
    x = rand()%100;

    // Add this entry to the db
    FILE * file2 = fopen(name,"w+"); // open a new file with overwrite
    if(file2 == NULL) perror("file open");

    // Write to file
    fprintf(file2,"%d %d %d %d",hp,exp,x,y);
    fclose(file2);
  }
  chdir("..");

  // At this point, we should have all the data to form the PLAYER_STATE_RESPONSE
  char buffer[PLAYER_STATE_RESPONSE_SIZE];
  createpsr(name,hp,exp,x,y,id,buffer);
  if(badMessageTypeFlag){
    buffer[0]=135;
  }
  udpunicast(udpsock,targetsin,buffer,PLAYER_STATE_RESPONSE_SIZE);

  // Set the last message sent for Dup checking.
  message_record * new_mr = (message_record*) malloc(sizeof(message_record));
  new_mr->ip = targetsin.sin_addr.s_addr;
  new_mr->id = id;
  new_mr->message = (unsigned char*) malloc (sizeof(char)*PLAYER_STATE_RESPONSE_SIZE);
  memcpy(new_mr->message,(char*)buffer,PLAYER_STATE_RESPONSE_SIZE);

  if (mr_array[oldest]){
    free(mr_array[oldest]->message);
    free(mr_array[oldest]);
  }

  mr_array[oldest] = new_mr;
}

// process_ssr : process save state request
int process_ss_request(char* name,
		       int hp,
		       int exp,
		       char x,
		       char y,
		       int udpsock,
		       struct sockaddr_in targetsin,
		       int id,
		       int oldest,message_record** mr_array,int faultyErrorCodeFlag){
	FILE * file = fopen(name,"w+");
	int myhp = ntohl(hp);
	int myexp = ntohl(exp);

	char success = 0;

	// Save the file
	if(file == NULL) success = 1;
	if(faultyErrorCodeFlag){
		success=9;
	}
	fprintf(file,"%d %d %d %d",myhp,myexp,x,y);
	fclose(file);

	// Send the save_state_response
	char buffer[SAVE_STATE_RESPONSE_SIZE];
	create_ss_response(id,success,buffer);
	udpunicast(udpsock,targetsin,buffer,SAVE_STATE_RESPONSE_SIZE);

	// Set the last message sent for Dup checking.
	message_record * new_mr = (message_record*) malloc(sizeof(message_record));
	new_mr->ip = targetsin.sin_addr.s_addr;
	new_mr->id = id;
	new_mr->message = (unsigned char*) malloc (sizeof(char)*SAVE_STATE_RESPONSE_SIZE);
	memcpy(new_mr->message,(char*)buffer,SAVE_STATE_RESPONSE_SIZE);

	if (mr_array[oldest]){
		free(mr_array[oldest]->message);
		free(mr_array[oldest]);
	}

	mr_array[oldest] = new_mr;


}

int updateHP(LinkedList * mylist){
	if(mylist->head == NULL){
		return 0; // nothing updated
	}
	Node * i;
	for(i=mylist->head; i!=NULL;i=i->next){
		i->datum->hp = i->datum->hp + 1;
		return 1;
	}
}
