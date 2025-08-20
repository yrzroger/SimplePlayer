/*
 * Copyright (C) 二的次方
 *
 */

#ifndef AUDIO_PLAY_H
#define AUDIO_PLAY_H

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
#include <cstdint>
#include <SafeQueue.h>
#include <vector>
#include <utils/RefBase.h>

namespace android {

class AudioPlay : public RefBase {
public:
    AudioPlay();
    ~AudioPlay();

    // 初始化播放器，采样率单位Hz。返回0表示成功
    int init(int sampleRate = 44100, int channels = 2);

    // 写入待播放的PCM数据：pData为数据指针，size为字节数
    int write(const uint8_t* pData, size_t size);
    
    // 开始播放
    int start();

    // 停止并释放资源
    void stop();
    
    // 判断当前是否stopped状态
    bool stopped() { return playState_ == STOPPED; }

private:
    enum PlayState {
        STOPPED,
        PLAYING
    };

    SLObjectItf engineObject_ = nullptr;
    SLEngineItf engineEngine_ = nullptr;
    SLObjectItf outputMixObject_ = nullptr;
    SLObjectItf playerObject_ = nullptr;
    SLPlayItf playerPlay_ = nullptr;
    SLAndroidSimpleBufferQueueItf bufferQueue_ = nullptr;
    SLVolumeItf playerVolume_ = nullptr;
    PlayState playState_ = STOPPED;

    // PCM数据队列
    SafeQueue<std::vector<uint8_t>> audioFrames_;
    mutable std::mutex stateLock_;
    std::condition_variable stateCondition_;

    // 静态回调
    static void bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context);

    // 播放一组音频数据
    void renderFrame();
};

}  // namespace android

#endif // AUDIO_PLAY_H
