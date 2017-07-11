// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include "iot_client.h"
#include "Arduino.h"
#include <json.h>
#include <stdlib.h>
#include "azure_c_shared_utility/sastoken.h"
#include "http_client.h"

static char *hostNameString = NULL;
static char *deviceIdString = NULL;
static char *deviceKeyString = NULL;
static size_t currentExpiry = 0;
static char *currentToken = NULL;
static char *sasUri = NULL;
static char temp[1024];

void setString(char **p, const char *value, int length)
{
    if (*p != NULL)
    {
        free(*p);
    }
    *p = (char *)malloc(length + 1);
    strcpy(*p, value);
}

int setConnectionString(const char *conn_str)
{
    int len = strlen(conn_str);
    const char hostNameToken[] = "HostName";
    const char deviceToken[] = "DeviceId";
    const char deviceKeyToken[] = "SharedAccessKey";
    strcpy(temp, conn_str);

    char *pch;
    pch = strtok(temp, SEMICOLON);

    while (pch != NULL)
    {
        String keyValuePair(pch);
        int equalPos = keyValuePair.indexOf(EQUAL_CHARACTOR);
        if (equalPos > 0 && equalPos < keyValuePair.length() - 1)
        {
            String key = keyValuePair.substring(0, equalPos);
            String value = keyValuePair.substring(equalPos + 1);
            key.trim();
            value.trim();
            if (strcmp(key.c_str(), hostNameToken) == 0)
            {
                setString(&hostNameString, value.c_str(), value.length());
            }
            else if (strcmp(key.c_str(), deviceToken) == 0)
            {
                setString(&deviceIdString, value.c_str(), value.length());
            }
            else if (strcmp(key.c_str(), deviceKeyToken) == 0)
            {
                setString(&deviceKeyString, value.c_str(), value.length());
            }
            else
            {
                Serial.printf("Invalid connection string property: %s", key);
                return -1;
            }
        }
        else
        {
            Serial.printf("Invalid connection string: ", conn_str);
            return -1;
        }

        pch = strtok(NULL, SEMICOLON);
    }
    return 0;
}

int getSASToken()
{
    time_t currentTime = time(NULL);
    if (currentTime == (time_t)-1 || currentTime < 1492333149)
    {
        Serial.println("Time does not appear to be working.");
        return -1;
    }
    size_t expiry = (size_t)(difftime(currentTime, 0) + 3600);
    if (currentExpiry > (size_t)(difftime(currentTime, 0)))
    {
        return 0;
    }
    currentExpiry = expiry;
    STRING_HANDLE keyString = STRING_construct(deviceKeyString);
    sprintf(temp, "%s/devices/%s", hostNameString, deviceIdString);
    STRING_HANDLE uriResource = STRING_construct(temp);
    STRING_HANDLE empty = STRING_new();
    STRING_HANDLE newSASToken = SASToken_Create(keyString, uriResource, empty, expiry);
    setString(&currentToken, STRING_c_str(newSASToken), STRING_length(newSASToken));
    STRING_delete(newSASToken);
    STRING_delete(keyString);
    STRING_delete(uriResource);
    STRING_delete(empty);
    return 0;
}

int validateIoT()
{
    if (hostNameString == NULL || deviceIdString == NULL || deviceKeyString == NULL)
    {
        Serial.println("Iot hub connection string is not initialized");
        return -1;
    }

    if (getSASToken() != 0)
    {
        Serial.println("Cannot generate sas token.");
        return -1;
    }
    return 0;
}

int completeC2DMessage(char *etag)
{
    if (etag == NULL || strlen(etag) < 2)
    {
        Serial.println("Invalid etag.");
        return -1;
    }
    if (validateIoT() != 0)
    {
        return -1;
    }
    etag[strlen(etag) - 1] = '\0';
    sprintf(temp, C2D_CB_ENDPOINT, hostNameString, deviceIdString, etag + 1);
    HTTPClient request = HTTPClient(HTTP_DELETE, temp);
    request.set_header("Authorization", currentToken);
    request.set_header("Accept", "application/json");
    const Http_Response *response = request.send();

    if (response == NULL)
    {
        Serial.println("Cannot delete message(Null Response).");
        return -1;
    }

    return !(response->status_code >= 200 && response->status_code < 300);
}

const char *getC2DMessage(char *etag)
{
    if (validateIoT() != 0)
    {
        return NULL;
    }
    sprintf(temp, C2D_ENDPOINT, hostNameString, deviceIdString);
    HTTPClient request = HTTPClient(HTTP_GET, temp);
    request.set_header("Authorization", currentToken);
    request.set_header("Accept", "application/json");
    const Http_Response *response = request.send();

    if (response == NULL)
    {
        Serial.println("Cannot get message(Null Response).");
        return NULL;
    }

    KEYVALUE *header = (KEYVALUE *)response->headers;
    while (header->prev != NULL)
    {
        if (strcmp("ETag", header->prev->key) == 0)
        {
            setString(&etag, header->value, strlen(header->value));
        }

        header = header->prev;
    }
    return response->body != NULL ? strdup(response->body) : NULL;
}