#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>

class OTAManager {
private:
    bool updating;
    int progress;

public:
    OTAManager();

    void begin();
    void loop();

    bool startUpdateFromUrl(const String &url);

    bool isUpdating() const;
    int getProgress() const;
};

#endif
