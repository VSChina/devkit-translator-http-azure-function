// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#ifndef _IOT_HUB_CLIENT_H
#define _IOT_HUB_CLIENT_H

#define C2D_ENDPOINT "https://%s/devices/%s/messages/deviceBound?api-version=2016-11-14"
#define C2D_CB_ENDPOINT "https://%s/devices/%s/messages/deviceBound/%s?api-version=2016-11-14"
#define SEMICOLON ";"
#define EQUAL_CHARACTOR '='

int setConnectionString(const char*conn_str);
const char* getC2DMessage(char * etag);
int completeC2DMessage(char *etag);

#endif