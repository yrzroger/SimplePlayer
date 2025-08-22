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

//#define LOG_NDEBUG 0
#define LOG_TAG "simple_player"
#include <inttypes.h>
#include <utils/Log.h>

#include "SimplePlayer.h"

#include <binder/IServiceManager.h>
#include <binder/ProcessState.h>
#include <mediadrm/ICrypto.h>
#include <media/IMediaHTTPService.h>
#include <media/IMediaPlayerService.h>
#include <media/MediaCodecBuffer.h>
#include <media/stagefright/foundation/ABuffer.h>
#include <media/stagefright/foundation/ADebug.h>
#include <media/stagefright/foundation/ALooper.h>
#include <media/stagefright/foundation/AMessage.h>
#include <media/stagefright/foundation/AString.h>
#include <media/stagefright/MediaCodec.h>
#include <media/stagefright/MediaCodecList.h>
#include <media/stagefright/MediaDefs.h>
#include <media/stagefright/NuMediaExtractor.h>
#include <gui/ISurfaceComposer.h>
#include <gui/SurfaceComposerClient.h>
#include <gui/Surface.h>
#include <ui/DisplayMode.h>

using namespace android;

static void usage(const char *me) {
    fprintf(stderr, "usage: %s \n"
                    "\tsimple_player /sdcard/video.mp4 \n",
                    me);
    exit(1);
}

int main(int argc, char **argv) {
    ALOGD("start playback ...");
    const char *me = argv[0];

    int res;
    while ((res = getopt(argc, argv, "h")) >= 0) {
        switch (res) {
            case '?':
            case 'h':
            default:
            {
                usage(me);
            }
        }
    }

    argc -= optind;
    argv += optind;

    if (argc != 1) {
        usage(me);
    }

    ProcessState::self()->startThreadPool();

    sp<android::ALooper> looper = new android::ALooper;
    looper->start();

    sp<SurfaceComposerClient> composerClient;
    sp<SurfaceControl> control;
    sp<Surface> surface;

    composerClient = new SurfaceComposerClient;
    CHECK_EQ(composerClient->initCheck(), (status_t)OK);

    const std::vector<PhysicalDisplayId> ids = SurfaceComposerClient::getPhysicalDisplayIds();
    CHECK(!ids.empty());

    const sp<IBinder> display = SurfaceComposerClient::getPhysicalDisplayToken(ids.front());
    CHECK(display != nullptr);

    ui::DisplayMode mode;
    CHECK_EQ(SurfaceComposerClient::getActiveDisplayMode(display, &mode), NO_ERROR);

    const ui::Size& resolution = mode.resolution;
    const ssize_t displayWidth = resolution.getWidth();
    const ssize_t displayHeight = resolution.getHeight();

    ALOGD("display is %zd x %zd\n", displayWidth, displayHeight);

    control = composerClient->createSurface(
            String8("A Surface"),
            displayWidth,
            displayHeight,
            PIXEL_FORMAT_RGB_565,
            0);

    CHECK(control != NULL);
    CHECK(control->isValid());

    SurfaceComposerClient::Transaction{}
             .setLayer(control, INT_MAX)
             .show(control)
             .apply();

    surface = control->getSurface();
    CHECK(surface != NULL);

    sp<SimplePlayer> player = new SimplePlayer;
    looper->registerHandler(player);

    class CodecListener : public CodecEventListener {
    public:
        CodecListener() {}
        virtual ~CodecListener() {}
        virtual void onFirstFrameAvailable() {
            ALOGD("onFirstFrameAvailable");
        }
    };
    sp<CodecListener> listener = new CodecListener;
    player->registerListener(listener);
    player->setDataSource(argv[0]);
    player->setSurface(surface->getIGraphicBufferProducer());
    player->start();
    while(player->isPlaying())
        usleep(50000);
    player->stop();
    player->reset();

    composerClient->dispose();

    looper->stop();

    return 0;
}
