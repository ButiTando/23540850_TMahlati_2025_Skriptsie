/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   CMDqueue.h
 * Author: david
 *
 * Created on 17 November 2016, 12:14 PM
 */

#ifndef CMDQUEUE_H
#define CMDQUEUE_H

#ifdef __cplusplus
extern "C" {
#endif


#define MAX 5
extern MSG_RAPI actionQueue [MAX];

MSG_RAPI *peek();
bool isEmpty();
bool isFull();
int size();
void insert(MSG_RAPI *data);
MSG_RAPI *removeData();
void resetQueue();



#ifdef __cplusplus
}
#endif

#endif /* CMDQUEUE_H */

