/*
 * To change this license header, choose License Headers in Project Properties.
 * To change this template file, choose Tools | Templates
 * and open the template in the editor.
 */

/* 
 * File:   queueManager.h
 * Author: david
 *
 * Created on 18 November 2016, 10:16 AM
 */

#ifndef QUEUEMANAGER_H
#define QUEUEMANAGER_H

#ifdef __cplusplus
extern "C" {
#endif

void startThread ();
void *queueWatcher (void *dummy);


#ifdef __cplusplus
}
#endif

#endif /* QUEUEMANAGER_H */

