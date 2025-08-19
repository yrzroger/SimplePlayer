//#define LOG_NDEBUG 0
#define LOG_TAG "AudioPlay"

#include "AudioPlay.h"
#include <cstring>
#include <utils/Log.h>

AudioPlay::AudioPlay() {}
AudioPlay::~AudioPlay() { stop(); }

int AudioPlay::init(int sampleRate, int channels) {
    SLresult result;
    ALOGD("init 3");
    // 创建引擎
    result = slCreateEngine(&engineObject_, 0, nullptr, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) return -1;
    result = (*engineObject_)->Realize(engineObject_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return -1;
    result = (*engineObject_)->GetInterface(engineObject_, SL_IID_ENGINE, &engineEngine_);
    if (result != SL_RESULT_SUCCESS) return -1;

    // 创建输出混音器
    result = (*engineEngine_)->CreateOutputMix(engineEngine_, &outputMixObject_, 0, 0, 0);
    if (result != SL_RESULT_SUCCESS) return -1;
    result = (*outputMixObject_)->Realize(outputMixObject_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return -1;

    // 创建播放器
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
    };
    SLDataFormat_PCM format_pcm = {
        SL_DATAFORMAT_PCM,
        (SLuint32)channels,
        (SLuint32)sampleRate * 1000,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        SL_PCMSAMPLEFORMAT_FIXED_16,
        (channels == 2) ? SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT : SL_SPEAKER_FRONT_CENTER,
        SL_BYTEORDER_LITTLEENDIAN
    };
    SLDataSource audioSrc = {&loc_bufq, &format_pcm};
    SLDataLocator_OutputMix loc_outmix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject_};
    SLDataSink audioSnk = {&loc_outmix, nullptr};

    const SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_VOLUME, SL_IID_ANDROIDCONFIGURATION};
    const SLboolean req[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    result = (*engineEngine_)->CreateAudioPlayer(
        engineEngine_,
        &playerObject_,
        &audioSrc,
        &audioSnk,
        3, ids, req
    ); 

    // Use the System stream for  sound playback.
    SLAndroidConfigurationItf playerConfig;
    result = (*playerObject_)->GetInterface(playerObject_,
        SL_IID_ANDROIDCONFIGURATION, &playerConfig);
    if (result != SL_RESULT_SUCCESS) {
        ALOGE("config GetInterface failed with result %d", result);
        return -1;
    }
    SLint32 streamType = SL_ANDROID_STREAM_SYSTEM;
    result = (*playerConfig)->SetConfiguration(playerConfig,
        SL_ANDROID_KEY_STREAM_TYPE, &streamType, sizeof(SLint32));
    if (result != SL_RESULT_SUCCESS) {
        ALOGE("SetConfiguration failed with result %d", result);
        return -1;
    }
    // use normal performance mode as low latency is not needed. This is not mandatory so
    // do not bail if we fail
    SLuint32 performanceMode = SL_ANDROID_PERFORMANCE_NONE;
    result = (*playerConfig)->SetConfiguration(
           playerConfig, SL_ANDROID_KEY_PERFORMANCE_MODE, &performanceMode, sizeof(SLuint32));

    if (result != SL_RESULT_SUCCESS) return -1;
    result = (*playerObject_)->Realize(playerObject_, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) return -1;
    result = (*playerObject_)->GetInterface(playerObject_, SL_IID_PLAY, &playerPlay_);
    if (result != SL_RESULT_SUCCESS) return -1;
    result = (*playerObject_)->GetInterface(playerObject_, SL_IID_BUFFERQUEUE, &bufferQueue_);
    if (result != SL_RESULT_SUCCESS) return -1;

    // get the volume interface
    result = (*playerObject_)->GetInterface(playerObject_, SL_IID_VOLUME, &playerVolume_);
    if (result != SL_RESULT_SUCCESS) {
        ALOGE("sl volume GetInterface failed with result %d", result);
        return -1;
    }


    // 注册回调
    result = (*bufferQueue_)->RegisterCallback(bufferQueue_, bufferQueueCallback, this);
    if (result != SL_RESULT_SUCCESS) return -1;

    // 设置初始状态为停止
    (*playerPlay_)->SetPlayState(playerPlay_, SL_PLAYSTATE_STOPPED);
    ALOGD("init 1");

    return 0;
}

int AudioPlay::write(const uint8_t* pData, size_t size) {
    if(pData == nullptr || size <= 0) return -1;

    std::vector<uint8_t> frame(size);
    memcpy(&frame[0], pData, size);

    audioFrames_.enqueue(frame);
    
    std::unique_lock<std::mutex> lock(stateLock_);
    stateCondition_.notify_one();

    return size;
}

int AudioPlay::start() {
    if (!playerPlay_ || !bufferQueue_) return -1;

    // 先停止队列并清空
    (*playerPlay_)->SetPlayState(playerPlay_, SL_PLAYSTATE_STOPPED);
    (*bufferQueue_)->Clear(bufferQueue_);

    // 设置为播放
    (*playerPlay_)->SetPlayState(playerPlay_, SL_PLAYSTATE_PLAYING);
    playState_ = PLAYING;
    ALOGD("start");
    
    // 主动激活bufferQueueCallback
    std::vector<uint8_t> frame(8);
    audioFrames_.enqueue(frame);
    render();

    return 0;
}

void AudioPlay::stop() {
    std::unique_lock<std::mutex> lock(stateLock_);
    if (playerObject_) {
        (*playerPlay_)->SetPlayState(playerPlay_, SL_PLAYSTATE_STOPPED);
        (*playerObject_)->Destroy(playerObject_);
        playerObject_ = nullptr;
        playerPlay_ = nullptr;
        bufferQueue_ = nullptr;
    }
    if (outputMixObject_) {
        (*outputMixObject_)->Destroy(outputMixObject_);
        outputMixObject_ = nullptr;
    }
    if (engineObject_) {
        (*engineObject_)->Destroy(engineObject_);
        engineObject_ = nullptr;
        engineEngine_ = nullptr;
    }
    playState_ = STOPPED;
    stateCondition_.notify_one();
}

void AudioPlay::render() {
    std::unique_lock<std::mutex> lock(stateLock_);
    if(audioFrames_.empty()) {
        stateCondition_.wait(lock);
    }
    lock.unlock();

    if(playState_ != PLAYING) return;
    
    std::vector<uint8_t> frame = audioFrames_.dequeue();
    (*bufferQueue_)->Enqueue(bufferQueue_, &frame[0], frame.size());
    ALOGD("render 3 %d", frame.size());
}

// 简单地把数据循环播放
void AudioPlay::bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    ALOGD("bufferQueueCallback");
    AudioPlay* self = static_cast<AudioPlay*>(context);
    self->render();
}
