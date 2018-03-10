#include <csignal>
#include <cstdint>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include "IAgoraRecordingEngine.h"

#include "base/atomic.h"
#include "base/log.h"
#include "base/opt_parser.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

using agora::base::opt_parser;
using agora::recording::VideoFrame;
using agora::recording::AudioFrame;

struct MixModeSettings {
    int m_height;
    int m_width;
    bool m_videoMix;
    MixModeSettings():
        m_height(0),
        m_width(0),
        m_videoMix(false)
    {};
};


class AgoraRecorder : public agora::recording::IRecordingEngineEventHandler {
 public:
 // appId
  AgoraRecorder();
  ~AgoraRecorder();

  bool createChannel(const string &appid, const string &channelKey, const string &name,  uid_t uid,
          agora::recording::RecordingConfig &config);

  int setVideoMixLayout();

  bool leaveChannel();
  bool release();

  bool stopped() const;
  inline void updateMixModeSetting(int width, int height, bool isVideoMix) {
      m_mixRes.m_width = width;
      m_mixRes.m_height = height;
      m_mixRes.m_videoMix = isVideoMix;
  }
  
 private:
  virtual void onError(int error);
  virtual void onWarning(int warn);

  virtual void onJoinChannelSuccess(const char * channelId, uid_t uid);
  virtual void onLeaveChannel();

  virtual void onUserJoined(uid_t uid, agora::recording::UserJoinInfos &infos);
  virtual void onUserOffline(uid_t uid, agora::recording::USER_OFFLINE_REASON_TYPE reason);

  virtual const void audioFrameReceived(unsigned int uid, const AudioFrame *frame);
  virtual const void videoFrameReceived(unsigned int uid, const VideoFrame *frame);
 private:
  atomic_bool_t m_stopped;
  agora::recording::IRecordingEngine *m_recorder;
  std::vector<agora::recording::uid_t> m_peers;
  std::string m_recordingDir;
  std::string m_logdir;
  MixModeSettings m_mixRes;
};

AgoraRecorder::AgoraRecorder(): IRecordingEngineEventHandler() {
  m_recorder = NULL;
  m_stopped.store(false);
  m_recordingDir = "./";
}

AgoraRecorder::~AgoraRecorder() {
  if (m_recorder) {
    m_recorder->release();
  }
}

bool AgoraRecorder::stopped() const {
  return m_stopped;
}

bool AgoraRecorder::release() {
  if (m_recorder) {
    m_recorder->release();
    m_recorder = NULL;
  }

  return true;
}

bool AgoraRecorder::createChannel(const string &appid, const string &channelKey, const string &name,
    uint32_t uid, 
    agora::recording::RecordingConfig &config) 
{
  if ((m_recorder = agora::recording::IRecordingEngine::createAgoraRecordingEngine(appid.c_str(), this)) == NULL)
    return false;

  
  return 0 == m_recorder->joinChannel(channelKey.c_str(), name.c_str(), uid, config);
}

bool AgoraRecorder::leaveChannel() {
  if (m_recorder) {
    m_recorder->leaveChannel();
    m_stopped = true;
  }

  return true;
}

int AgoraRecorder::setVideoMixLayout()
{
    if(!m_mixRes.m_videoMix) return 0;

    LOG_DIR(m_logdir.c_str(), INFO, "setVideoMixLayout: user size: %d", m_peers.size());

    agora::recording::VideoMixingLayout layout;
    layout.canvasWidth = m_mixRes.m_width;
    layout.canvasHeight = m_mixRes.m_height;
    layout.backgroundColor = "#23b9dc";

    layout.regionCount = m_peers.size();

    if (!m_peers.empty()) {
        LOG_DIR(m_logdir.c_str(), INFO, "setVideoMixLayout: peers not empty");
        agora::recording::VideoMixingLayout::Region * regionList = new agora::recording::VideoMixingLayout::Region[m_peers.size()];

        regionList[0].uid = m_peers[0];
        regionList[0].x = 0.f;
        regionList[0].y = 0.f;
        regionList[0].width = 1.f;
        regionList[0].height = 1.f;
        regionList[0].zOrder = 0;
        regionList[0].alpha = 1.f;
        regionList[0].renderMode = 0;

        LOG_DIR(m_logdir.c_str(), INFO, "region 0 uid: %d, x: %f, y: %f, width: %f, height: %f, zOrder: %d, alpha: %f", regionList[0].uid, regionList[0].x, regionList[0].y, regionList[0].width, regionList[0].height, regionList[0].zOrder, regionList[0].alpha);


        float canvasWidth = m_mixRes.m_width;
        float canvasHeight = m_mixRes.m_height;

        float viewWidth = 0.3;
        float viewHEdge = 0.025;
        float viewHeight = viewWidth * (canvasWidth / canvasHeight);
        float viewVEdge = viewHEdge * (canvasWidth / canvasHeight);

        for (int i=1; i<m_peers.size(); i++) {
            if (i >= 7)
                break;

            regionList[i].uid = m_peers[i];

            float xIndex = i % 3;
            float yIndex = i / 3;
            regionList[i].x = xIndex * (viewWidth + viewHEdge) + viewHEdge;
            regionList[i].y = 1 - (yIndex + 1) * (viewHeight + viewVEdge);
            regionList[i].width = viewWidth;
            regionList[i].height = viewHeight;
            regionList[i].zOrder = 0;
            regionList[i].alpha = i + 1;
            regionList[i].renderMode = 0;
        }

        layout.regions = regionList;
//    LOG_DIR(m_logdir.c_str(), INFO, "region 0 uid: %d, x: %f, y: %f, width: %f, height: %f, zOrder: %d, alpha: %f", regionList[0].uid, regionList[0].x, regionList[0].y, regionList[0].width, regionList[0].height, regionList[0].zOrder, regionList[0].alpha);
    }
    else {
        layout.regions = NULL;
    }

    m_recorder->setVideoMixingLayout(layout);
}

void AgoraRecorder::onError(int error) {
    cerr << "Error: " << error << endl;
    leaveChannel();
}

void AgoraRecorder::onWarning(int warn) {
    cerr << "warn: " << warn << endl;
    //  leaveChannel();
}

void AgoraRecorder::onJoinChannelSuccess(const char * channelId, uid_t uid) {
    cout << "join channel Id: " << channelId << ", with uid: " << uid << endl;
}

void AgoraRecorder::onLeaveChannel() {
    cout << "leave channel" << endl;
}

void AgoraRecorder::onUserJoined(unsigned uid, agora::recording::UserJoinInfos &infos) {
    cout << "User " << uid << " joined, RecordingDir:" << (infos.recordingDir? infos.recordingDir:"NULL") <<endl;
    if(infos.recordingDir)
    {
        m_recordingDir = std::string(infos.recordingDir);
        m_logdir = m_recordingDir;
    }

    m_peers.push_back(uid);

    setVideoMixLayout();
}

void AgoraRecorder::onUserOffline(unsigned uid, agora::recording::USER_OFFLINE_REASON_TYPE reason) {
    cout << "User " << uid << " offline, reason: " << reason << endl;
    m_peers.erase(std::remove(m_peers.begin(), m_peers.end(), uid), m_peers.end());

    setVideoMixLayout();
}

const void AgoraRecorder::audioFrameReceived(unsigned int uid, const AudioFrame *pframe) {
  char uidbuf[65];
  snprintf(uidbuf, sizeof(uidbuf),"%u", uid);
  std::string info_name = m_recordingDir + std::string(uidbuf) /*+ timestamp_per_join_*/ +".pcm";
  FILE *fp = fopen(info_name.c_str(), "a+b");

  if (pframe->type == agora::recording::AUDIO_FRAME_RAW_PCM) {
    cout << "User " << uid << ", received a raw PCM frame" << endl;
    //cout << "User " << uid << ", received a raw PCM frame, timestamp: " << f->frame_ms_ << endl;
    agora::recording::AudioPcmFrame *f = pframe->frame.pcm;
    ::fwrite(f->pcmBuf_, 1, f->pcmBufSize_, fp);

  } else if (pframe->type == agora::recording::AUDIO_FRAME_AAC) {
    cout << "User " << uid << ", received an AAC frame" << endl;
  }
  ::fclose(fp);
}

const void AgoraRecorder::videoFrameReceived(unsigned int uid, const VideoFrame *pframe) {
  char uidbuf[65];
  snprintf(uidbuf, sizeof(uidbuf),"%u", uid);
  char * suffix=".vtmp";
  if (pframe->type == agora::recording::VIDEO_FRAME_RAW_YUV) {
    agora::recording::VideoYuvFrame *f = pframe->frame.yuv;
    suffix=".yuv";

    cout << "User " << uid << ", received a yuv frame, width: "
        << f->width_ << ", height: " << f->height_ ;
    cout<<",ystide:"<<f->ystride_<< ",ustride:"<<f->ustride_<<",vstride:"<<f->vstride_;
    cout<< endl;
  } else {
    suffix=".h264";
    agora::recording::VideoH264Frame *f = pframe->frame.h264;

    cout << "User " << uid << ", received an h264 frame, timestamp: "
        << f->frame_ms << ", frame no: " << f->frame_num << endl;
  }

  std::string info_name = m_recordingDir + std::string(uidbuf) /*+ timestamp_per_join_ */+ std::string(suffix);
  FILE *fp = fopen(info_name.c_str(), "a+b");

  //store it as file
  if (pframe->type == agora::recording::VIDEO_FRAME_RAW_YUV) {
      agora::recording::VideoYuvFrame *f = pframe->frame.yuv;
      ::fwrite(f->buf_, 1, f->bufSize_, fp);
  }
  else {
      agora::recording::VideoH264Frame *f = pframe->frame.h264;
      ::fwrite(f->buf_, 1, f->bufSize_, fp);
  }
  ::fclose(fp);

}



atomic_bool_t g_bSignalStop;

void signal_handler(int signo) {
  (void)signo;

  // cerr << "Signal " << signo << endl;
  g_bSignalStop = true;
}

int main(int argc, char * const argv[]) {
  uint32_t uid = 0;
  string appId;
  string channelKey;
  string name;
  uint32_t channelProfile;

  string decryptionMode;
  string secret;
  string mixResolution("360,640,15,500");

  int idleLimitSec=30*60;//30min

  string applitePath;
  string appliteLogPath;
  string recordFileRootDir=".";

  int lowUdpPort = 0;//40000;
  int highUdpPort = 0;//40004;

  bool isAudioOnly=0;
  bool isMixingEnabled=0;

  bool getAudioFrame = false;
  bool getVideoFrame = false;
  int width, height, fps, kbps;


  g_bSignalStop = false;
  signal(SIGQUIT, signal_handler);
  signal(SIGABRT, signal_handler);
  signal(SIGINT, signal_handler);
  signal(SIGPIPE, SIG_IGN);

  opt_parser parser;

  parser.add_long_opt("appId", &appId, "App Id/must", agora::base::opt_parser::require_argu);
  parser.add_long_opt("uid", &uid, "User Id default is 0/must", agora::base::opt_parser::require_argu);

  parser.add_long_opt("channel", &name, "Channel Id/must", agora::base::opt_parser::require_argu);
  parser.add_long_opt("appliteDir", &applitePath, "directory of app lite 'video_recorder', Must pointer to 'Agora_Recording_SDK_for_Linux_FULL/bin/' folder/must",
          agora::base::opt_parser::require_argu);

  parser.add_long_opt("channelKey", &channelKey, "channelKey/option");
  parser.add_long_opt("channelProfile", &channelProfile, "channel_profile:(0:COMMUNICATION),(1:broadcast) default is 0/option");

  parser.add_long_opt("isAudioOnly", &isAudioOnly, "Default 0:ARS (0:1)/option");
  parser.add_long_opt("isMixingEnabled", &isMixingEnabled, "Mixing Enable? (0:1)/option");
  parser.add_long_opt("mixResolution", &mixResolution, "change default resolution for vdieo mix mode/option");

  parser.add_long_opt("decryptionMode", &decryptionMode, "decryption Mode, default is NULL/option");
  parser.add_long_opt("secret", &secret, "input secret when enable decryptionMode/option");

  parser.add_long_opt("idle", &idleLimitSec, "Default 300s/option");
  parser.add_long_opt("recordFileRootDir", &recordFileRootDir, "recording file root dir/option");

  parser.add_long_opt("lowUdpPort", &lowUdpPort, "default is random value/option");
  parser.add_long_opt("highUdpPort", &highUdpPort, "default is random value/option");

  parser.add_long_opt("getAudioFrame", &getAudioFrame, "default 0 (0:save as file, 1:get framebuffer) /option");
  parser.add_long_opt("getVideoFrame", &getVideoFrame, "default 0 (0:save as file, 1:get framebuffer) /option");



  if (!parser.parse_opts(argc, argv) || appId.empty() || name.empty()) {
      std::string usage = "Usage: \n\
      ./RECORD_APP --appId STRING --uid UINTEGER32 --channel STRING --appliteDir STRING --channelKey STRING --channelProfile UINTEGER32 --isAudioOnly 0/1 --isMixingEnabled 0/1 --decryptionMode STRING --secret STRING --idle INTEGER32 --recordFileRootDir STRING --lowUdpPort INTEGER32 --highUdpPort INTEGER32\n     \
      --appId     (App Id/must) \n     \
      --uid     (User Id default is 0/must)  \n     \
      --channel     (Channel Id/must) \n     \
      --appliteDir     (directory of app lite 'video_recorder', Must pointer to 'Agora_Recording_SDK_for_Linux_FULL/bin/' folder/must) \n     \
      --channelKey     (channelKey/option) \n     \
      --channelProfile     (channel_profile:(0:COMMUNICATION),(1:broadcast) default is 0/option) \n     \
      --isAudioOnly     (Default 0:ARS (0:1)/option) \n     \
      --isMixingEnabled     (Mixing Enable? (0:1)/option) \n     \
      --mixResolution      (format:width,high,fps,kbps 'change default resolution for video mix mode/option') \n     \
      --decryptionMode     (decryption Mode, default is NULL/option) \n     \
      --secret     (input secret when enable decryptionMode/option) \n     \
      --idle     (Default 300s/option) \n     \
      --recordFileRootDir     (recording file root dir/option) \n      \
      --lowUdpPort            (default is random value/option) \n      \
      --highUdpPort            (default is random value/option) \n      \
      --getAudioFrame          (default 0 (0:save as file, 1:get framebuffer) /option) \n      \
      --getVideoFrame          (default 0 (0:save as file, 1:get framebuffer) /option)";

      std::cerr << usage << std::endl;
    return -1;
  }

  if(isMixingEnabled && !isAudioOnly) {
     if(4 != sscanf(mixResolution.c_str(), "%d,%d,%d,%d", &width,
                  &height, &fps, &kbps)) {
        LOG(ERROR, "Illegal resolution: %s", mixResolution.c_str());
        return -1;
     }
  }

  LOG(INFO, "uid %" PRIu32 " from vendor %s is joining channel %s",
          uid, appId.c_str(), name.c_str());

  AgoraRecorder recorder;
  agora::recording::RecordingConfig config;
  config.idleLimitSec = idleLimitSec;
  config.channelProfile = static_cast<agora::recording::CHANNEL_PROFILE_TYPE>(channelProfile);

  config.isAudioOnly = isAudioOnly;
  config.isMixingEnabled = isMixingEnabled;
  config.mixResolution = (isMixingEnabled && !isAudioOnly)? const_cast<char*>(mixResolution.c_str()):NULL;

  config.appliteDir = const_cast<char*>(applitePath.c_str());
  config.recordFileRootDir = const_cast<char*>(recordFileRootDir.c_str());

  config.secret = secret.empty()? NULL:const_cast<char*>(secret.c_str());
  config.decryptionMode = decryptionMode.empty()? NULL:const_cast<char*>(decryptionMode.c_str());

  config.lowUdpPort = lowUdpPort;
  config.highUdpPort = highUdpPort;

  config.decodeAudio = getAudioFrame;
  config.decodeVideo = getVideoFrame;

  recorder.updateMixModeSetting(width, height, isMixingEnabled ? !isAudioOnly:false);

  if (!recorder.createChannel(appId, channelKey, name, uid, config)) {
    cerr << "Failed to create agora channel: " << name << endl;
    return -1;
  }

  while (!recorder.stopped() && !g_bSignalStop) {
    sleep(1);
  }

  if (g_bSignalStop) {
    recorder.leaveChannel();
    recorder.release();
  }

  cerr << "Stopped \n";
  return 0;
}

