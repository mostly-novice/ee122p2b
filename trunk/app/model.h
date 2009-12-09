// Model handling
typedef struct P{
  char name[10];
  int hp;
  int exp;
  int x;
  int y;
  int visible;
  unsigned int p2p_id;
} Player;

typedef struct si{
  unsigned int p2p_id;
  unsigned short port;
  unsigned char *ip;
  Range* backup;
  Range* primary;
} serverInstance;

struct list_el {
  Player * datum;
  struct list_el * next;
};

typedef struct list_el Node;

typedef struct ll{
  Node * head;
  Node * tail;
} LinkedList;


// globals
int fc=0;

void stats(Player * p){
  fprintf(stdout, "%s: location=(%u,%u), HP=%u, EXP=%u\n",
	  p->name, p->x, p->y, p->hp, p->exp);
}

void printPlayers(LinkedList * list){
  Node * n;
  printf("People in the list:\n");
  for(n = list->head; n != NULL; n = n->next){
    stats(n->datum);
  }
  printf("---------------------\n");
}

Player * findPlayer(char * name, LinkedList * list){
  Node * p;
  for(p = list->head; p != NULL; p = p->next){
    if (strcmp(p->datum->name,name)==0){
      return p->datum;
    }
  }
  return NULL;
}

unsigned int removePlayer(char * name, LinkedList * list)
{
  Node * prev = NULL;
  Node * curr = list->head ;
  Node * temp;

  /* check for empty list */
  if(!curr){ printf("The list is empty"); return 0; }

  /* check if datum is in head of list */
  if(strcmp(list->head->datum->name,name)==0){
    if(strcmp(list->tail->datum->name,name)==0){ // if tail == head, this is the only guy
      free(list->head->datum);
      free(list->head);

      list->head = NULL;
      list->tail = NULL;
      return 1;
    }

    curr = list->head;
    free(curr->datum);
    free(curr);
    list->head = list->head->next;
    return 1;
  }

  while( curr->next ){
    prev = curr;
    curr = curr->next;
    if(strcmp( curr->datum->name,name )==0){
      if(strcmp(list->tail->datum->name,name)==0){
	list->tail = prev;}
      prev->next = curr->next;
      free(curr->datum);
      free(curr);
    }
  }
  return 1;
}

Node * freePlayers(LinkedList * list)
{
  // To free all the players after log out
  Node * curr;
  while(list->head)
    {
      curr = list->head;
      list->head = curr->next;
      free(curr->datum);
      fc++;
      free(curr);
      fc++;
    }

}

void initialize(Player * object,char * name, int hp, int exp, int x, int y){
  strcpy(object->name,name);
  object->hp = hp;
  object->exp = exp;
  object->x = x;
  object->y = y;
}

void addPlayer(Player * newplayer, LinkedList * list){
  Node * node = (Node*) malloc(sizeof(Node)); // TODO: remember to free this
  node->datum = newplayer;
  node->next = NULL;
  if(list->head == NULL){
    list->head = node;
    list->tail = node;
  }else {
    list->tail->next = node;
    list->tail = node;
  }
}
