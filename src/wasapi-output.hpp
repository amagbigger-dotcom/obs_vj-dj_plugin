#pragma once

#include <string>
#include <vector>
#include <memory>

class WasapiOutput {
public:
    WasapiOutput();
    ~WasapiOutput();

    bool open(const std::string &device_id);
    void close();
    bool writeFrames(const float *l, const float *r, size_t frames);
    bool isRunning() const;
    std::string currentDeviceId() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
