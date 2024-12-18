/*
 * Copyright (C) 2012 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <media/stagefright/foundation/AHandler.h>
#include <media/stagefright/foundation/AString.h>
#include <utils/KeyedVector.h>

namespace android {

struct ABuffer;
struct ALooper;
class AudioTrack;
class IGraphicBufferProducer;
struct MediaCodec;
class MediaCodecBuffer;
struct NuMediaExtractor;
class Surface;

struct CodecEventListener: virtual public RefBase {
    virtual void onFirstFrameAvailable() = 0;
};

struct SimplePlayer : public AHandler {
    SimplePlayer();

    status_t setDataSource(const char *path);
    status_t setSurface(const sp<IGraphicBufferProducer> &bufferProducer);
    status_t prepare();
    status_t start();
    status_t stop();
    status_t reset();
    bool isPlaying();
    void registerListener(const wp<CodecEventListener>& listener) { mListener = listener; }

protected:
    virtual ~SimplePlayer();

    virtual void onMessageReceived(const sp<AMessage> &msg);

private:
    enum State {
        UNINITIALIZED,
        UNPREPARED,
        STOPPED,
        STARTED
    };

    enum {
        kWhatSetDataSource,
        kWhatSetSurface,
        kWhatPrepare,
        kWhatStart,
        kWhatStop,
        kWhatReset,
        kWhatDoMoreStuff,
    };

    enum SourceType {
        VIDEO = 0,
        AUDIO = 1,
        NUM_SOURCE_TYPES = 2
    };

    struct BufferInfo {
        size_t mIndex;
        size_t mOffset;
        size_t mSize;
        int64_t mPresentationTimeUs;
        uint32_t mFlags;
    };

    struct CodecState
    {
        sp<MediaCodec> mCodec;
        Vector<sp<ABuffer> > mCSD;
        Vector<sp<MediaCodecBuffer> > mBuffers[2];
        Vector<sp<ABuffer> > mSampleData;

        List<size_t> mAvailInputBufferIndices;
        List<BufferInfo> mAvailOutputBufferInfos;
        SourceType mType;

        sp<AudioTrack> mAudioTrack;
        uint32_t mNumFramesWritten;
    };

    State mState;
    AString mPath;
    sp<Surface> mSurface;

    sp<NuMediaExtractor> mExtractor;
    sp<ALooper> mCodecLooper;
    KeyedVector<size_t, CodecState> mStateByTrackIndex;
    int32_t mDoMoreStuffGeneration;
    int32_t mEndOfStream;

    int64_t mStartTimeRealUs;
    bool mEncounteredInputEOS;
    bool firstFrameObserved;
    wp<CodecEventListener> mListener;

    status_t onPrepare();
    status_t onStart();
    status_t onStop();
    status_t onReset();
    status_t onDoMoreStuff();
    status_t onOutputFormatChanged(size_t trackIndex, CodecState *state);

    void renderAudio(
            CodecState *state, BufferInfo *info, const sp<MediaCodecBuffer> &buffer);

    DISALLOW_EVIL_CONSTRUCTORS(SimplePlayer);
};

}  // namespace android
