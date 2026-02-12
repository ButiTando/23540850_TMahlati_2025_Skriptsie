/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include "message.h"
#include "CMDqueue.h"

MSG_RAPI actionQueue[MAX];

int front = 0;
int rear = -1;
int itemCount = 0;


//MSG_RAPI *peek() {
//   return actionQueue[front];
//}

bool isEmpty() {
   return itemCount == 0;
}

bool isFull() {
   return itemCount == MAX;
}

int size() {
   return itemCount;
}  

void insert(MSG_RAPI *data) {
   if(!isFull()) {
	
      if(rear == MAX-1) {
         rear = -1;            
      }       \
      ++rear;
//inserting data into queue
      actionQueue[rear].msgPacket.checksum = data->msgPacket.checksum;
      actionQueue[rear].msgPacket.id = data->msgPacket.id;
      actionQueue[rear].msgPacket.length = data->msgPacket.length;
      actionQueue[rear].msgPacket.syncByte = data->msgPacket.syncByte;
      actionQueue[rear].msgPacket.type = data->msgPacket.type;
      memset(actionQueue[rear].msgPacket.data,0,MSG_MAX_PACKET_LENGTH);
      memcpy(actionQueue[rear].msgPacket.data, data->msgPacket.data, data->msgPacket.length);
      
      itemCount++;
      
      printf("Inserting Item Count %d", itemCount);
   }
}

MSG_RAPI *removeData() 
{
   //MSG_RAPI data = actionQueue[front++];
    int temp = front;
    front++;
   if(front == MAX) {
      front = 0;
   }
	
   itemCount--;
   //return data;  
   printf("Item Count %d", itemCount);
   return &actionQueue[temp];
}
void resetQueue()
{
    front = 0;
    rear = -1;
    itemCount = 0;
    
}
