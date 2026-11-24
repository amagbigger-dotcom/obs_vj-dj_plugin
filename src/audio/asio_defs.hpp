#pragma once

#include <windows.h>
#include <cstdint>

// =============================================================================
// Steinberg ASIO 2.3 C++ Interfész Definíciók (Külső SDK függőség nélkül)
// =============================================================================

static const GUID IID_IASIO = { 0x7a297200, 0xfe21, 0x11ce, { 0x82, 0xe6, 0x0, 0xaa, 0x0, 0x5c, 0x94, 0x23 } };

using ASIOBool = long;
enum {
    ASIOFalse = 0,
    ASIOTrue = 1
};

using ASIOError = long;
enum {
    ASE_OK = 0,
    ASE_SUCCESS = 0x3f484c27,
    ASE_NotPresent = -1000,
    ASE_HWMalfunction = -999,
    ASE_InvalidParameter = -998,
    ASE_InvalidMode = -997,
    ASE_SPNotAdvancing = -996,
    ASE_NoClock = -995,
    ASE_NoMemory = -994
};

using ASIOSampleRate = double;

struct ASIOSamples {
    unsigned long lo;
    unsigned long hi;
};

struct ASIOTimeStamp {
    unsigned long lo;
    unsigned long hi;
};

enum ASIOSampleType {
    ASIOSTInt16MSB = 0,
    ASIOSTInt24MSB = 1,
    ASIOSTInt32MSB = 2,
    ASIOSTFloat32MSB = 3,
    ASIOSTFloat64MSB = 4,

    ASIOSTInt16LSB = 8,
    ASIOSTInt24LSB = 9,
    ASIOSTInt32LSB = 10,
    ASIOSTFloat32LSB = 11,
    ASIOSTFloat64LSB = 12,

    ASIOSTInt32LSB16 = 16,
    ASIOSTInt32LSB18 = 17,
    ASIOSTInt32LSB20 = 18,
    ASIOSTInt32LSB24 = 19,

    ASIOSTInt32MSB16 = 24,
    ASIOSTInt32MSB18 = 25,
    ASIOSTInt32MSB20 = 26,
    ASIOSTInt32MSB24 = 27
};

struct ASIOClockSource {
    long index;
    long associatedChannel;
    long associatedGroup;
    ASIOBool isCurrentSource;
    char name[32];
};

struct ASIOChannelInfo {
    long channel;
    ASIOBool isInput;
    ASIOBool isActive;
    long channelGroup;
    long type;
    char name[32];
};

struct ASIOBufferInfo {
    ASIOBool isInput;
    long channelNum;
    void *buffers[2];
};

struct ASIOTimeCode {
    double speed;
    ASIOSamples timeCodeSamples;
    unsigned long flags;
    char future[64];
};

struct ASIOTimeInfo {
    double speed;
    ASIOTimeStamp systemTime;
    ASIOSamples samplePosition;
    ASIOSampleRate sampleRate;
    unsigned long flags;
    char reserved[12];
};

struct ASIOTime {
    long reserved[4];
    ASIOTimeInfo timeInfo;
    ASIOTimeCode timeCode;
};

struct ASIOCallbacks {
    void (*bufferSwitch)(long doubleBufferIndex, ASIOBool directProcess);
    void (*sampleRateDidChange)(ASIOSampleRate sRate);
    long (*asioMessage)(long selector, long value, void *message, double *opt);
    ASIOTime *(*bufferSwitchTimeInfo)(ASIOTime *params, long doubleBufferIndex, ASIOBool directProcess);
};

// =============================================================================
// IASIO COM Vtable Interfész
// =============================================================================
#undef INTERFACE
#define INTERFACE IASIO
DECLARE_INTERFACE_(IASIO, IUnknown) {
    // IUnknown
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void **ppvObject) PURE;
    STDMETHOD_(ULONG, AddRef)(THIS) PURE;
    STDMETHOD_(ULONG, Release)(THIS) PURE;

    // IASIO
    STDMETHOD_(ASIOBool, init)(THIS_ void *sysHandle) PURE;
    STDMETHOD_(void, getDriverName)(THIS_ char *name) PURE;
    STDMETHOD_(long, getDriverVersion)(THIS) PURE;
    STDMETHOD_(void, getErrorMessage)(THIS_ char *string) PURE;
    STDMETHOD_(ASIOError, start)(THIS) PURE;
    STDMETHOD_(ASIOError, stop)(THIS) PURE;
    STDMETHOD_(ASIOError, getChannels)(THIS_ long *numInputChannels, long *numOutputChannels) PURE;
    STDMETHOD_(ASIOError, getLatencies)(THIS_ long *inputLatency, long *outputLatency) PURE;
    STDMETHOD_(ASIOError, getBufferSize)(THIS_ long *minSize, long *maxSize, long *preferredSize, long *granularity) PURE;
    STDMETHOD_(ASIOError, canSampleRate)(THIS_ ASIOSampleRate sampleRate) PURE;
    STDMETHOD_(ASIOError, getSampleRate)(THIS_ ASIOSampleRate *sampleRate) PURE;
    STDMETHOD_(ASIOError, setSampleRate)(THIS_ ASIOSampleRate sampleRate) PURE;
    STDMETHOD_(ASIOError, getClockSources)(THIS_ ASIOClockSource *clocks, long *numSources) PURE;
    STDMETHOD_(ASIOError, setClockSource)(THIS_ long reference) PURE;
    STDMETHOD_(ASIOError, getSamplePosition)(THIS_ ASIOSamples *sPos, ASIOTimeStamp *tStamp) PURE;
    STDMETHOD_(ASIOError, getChannelInfo)(THIS_ ASIOChannelInfo *info) PURE;
    STDMETHOD_(ASIOError, createBuffers)(THIS_ ASIOBufferInfo *bufferInfos, long numChannels, long bufferSize, ASIOCallbacks *callbacks) PURE;
    STDMETHOD_(ASIOError, disposeBuffers)(THIS) PURE;
    STDMETHOD_(ASIOError, controlPanel)(THIS) PURE;
    STDMETHOD_(ASIOError, future)(THIS_ long selector, void *opt) PURE;
    STDMETHOD_(ASIOError, outputReady)(THIS) PURE;
};
