#include "Arduino.h"
#include "AudioClass.h"
#include "AZ3166WiFi.h"
#include "OLEDDisplay.h"
#include "http_client.h"
#include "iot_client.h"
#include "mbed_memory_status.h"

#define MAX_UPLOAD_SIZE (64 * 1024)

static boolean hasWifi = false;
static const int recordedDuration = 3;
static char *waveFile = NULL;
static int wavFileSize;
static const uint32_t delayTimes = 1000;
static AudioClass Audio;
static const int audioSize = ((32000 * recordedDuration) + 44);
static bool translated = false;
static bool validParameters = false;
static const char *deviceConnectionString = "";
static const char *azureFunctionUri = "";

enum STATUS
{
    Idle,
    Recorded,
    WavReady,
    Uploaded
};

static STATUS status;

static void log_time(const char *event)
{
    time_t t = time(NULL);
    Serial.printf("%s: %s", event, ctime(&t));
}

static int httpTriggerTranslator(const char *content, int length)
{
    if (content == NULL || length <= 0 || length > MAX_UPLOAD_SIZE)
    {
        Serial.println("Content not valid");
        return -1;
    }
    log_time("begin httppost");
    HTTPClient client = HTTPClient(HTTP_POST, azureFunctionUri);
    const Http_Response *response = client.send(content, length);
    log_time("response back");
    if (response != NULL && response->status_code == 200)
    {
        return 0;
    }
    return -1;
}

static void EnterIdleState()
{
    status = Idle;
    Screen.clean();
    Screen.print(0, "Hold B to talk");
}

void setup()
{
    Screen.clean();
    Screen.print(0, "DevKitTranslator");
    Screen.print(2, "Initializing...");
    Screen.print(3, " > WiFi");

    hasWifi = (WiFi.begin() == WL_CONNECTED);
    if (!hasWifi)
    {
        Screen.print(2, "No Wifi");
        return;
    }
    validParameters = (deviceConnectionString != NULL && *deviceConnectionString != '\0' && azureFunctionUri != NULL && *azureFunctionUri != '\0');
    if (!validParameters)
    {
        Screen.print(2, "Please check parameters", true);
        return;
    }
    setConnectionString(deviceConnectionString);
    EnterIdleState();
}

void freeWavFile()
{
    if (waveFile != NULL)
    {
        free(waveFile);
        waveFile = NULL;
    }
}

void loop()
{
    if (!hasWifi || !validParameters)
    {
        return;
    }

    uint32_t curr = millis();
    switch (status)
    {
    case Idle:
        if (digitalRead(USER_BUTTON_B) == LOW)
        {
            waveFile = (char *)malloc(audioSize + 1);
            if (waveFile == NULL)
            {
                Serial.println("No enough Memory! ");
                EnterIdleState();
                return;
            }
            memset(waveFile, 0, audioSize + 1);
            Audio.format(8000, 16);
            Audio.startRecord(waveFile, audioSize, recordedDuration);
            status = Recorded;
            Screen.clean();
            Screen.print(0, "Release to send\r\nMax duraion: 3 sec");
        }
        break;
    case Recorded:
        if (digitalRead(USER_BUTTON_B) == HIGH)
        {
            Audio.getWav(&wavFileSize);
            if (wavFileSize > 0)
            {
                wavFileSize = Audio.convertToMono(waveFile, wavFileSize, 16);
                if (wavFileSize <= 0)
                {
                    Serial.println("ConvertToMono failed! ");
                    EnterIdleState();
                    freeWavFile();
                }
                else
                {
                    status = WavReady;
                    Screen.clean();
                    Screen.print(0, "Processing...");
                    Screen.print(1, "Uploading...");
                }
            }
            else
            {
                Serial.println("No Data Recorded! ");
                freeWavFile();
                EnterIdleState();
            }
        }
        break;
    case WavReady:
        if (wavFileSize > 0 && waveFile != NULL)
        {
            if (0 == httpTriggerTranslator(waveFile, wavFileSize))
            {
                status = Uploaded;
            }
            else
            {
                Serial.println("Error happened when translating");
                freeWavFile();
                EnterIdleState();
                Screen.print(2, "azure function failed", true);
            }
        }
        else
        {
            freeWavFile();
            EnterIdleState();
            Screen.print(1, "wav not ready");
        }
        break;
    case Uploaded:
        Screen.print(1, "Receiving...");
        char *etag = (char *)malloc(40);
        while (!translated)
        {
            const char *p = getC2DMessage(etag);
            if (p != NULL)
            {
                if (strlen(p) == 0)
                {
                    free((void *)p);
                    break;
                }
                Screen.clean();
                Screen.print(1, "Translation: ");
                Screen.print(2, p, true);
                log_time("Translated");
                completeC2DMessage((char *)etag);
                translated = true;
                free((void *)p);
            }
        }
        translated = false;
        status = Idle;
        Screen.print(0, "Hold B to talk");
        free(etag);
        freeWavFile();
        break;
    }

    curr = millis() - curr;
    if (curr < delayTimes)
    {
        delay(delayTimes - curr);
    }
}
