//
//  kinect_interface.h
//
//  Jonathan Tompson
// 

#pragma once

#include <mutex>
#include <condition_variable>
#include <thread>
#include "math/math_types.h"
#include "threading/callback.h" 
#include "data_str/vector_managed.h"
#include "data_str/vector.h"

#define src_width 640
#define src_height 480
#define src_dim (src_width * src_height)

#define SKEL_NJOINTS 25
#define SKELETON_SMOOTHING 0.05f
#define MIRROR true  // true if you want to mirror all kinect data
#define KINECT_INTERFACE_NUM_WORKER_THREADS 4
#define KINECT_INTERFACE_NUM_CONVERTER_THREADS 4  // cannot be > NUM_WORKER_THREADS
#define OPENNI_WAIT_TIMEOUT 50  // Maybe ms?

namespace jtil { namespace clk { class Clk; } }
namespace jtil { namespace threading { class ThreadPool; } }

namespace openni { class Device; }
namespace openni { class VideoStream; }
namespace openni { class VideoFrameRef; }
namespace openni { class VideoMode; }
namespace openni { class SensorInfo; }
		
namespace kinect_interface_primesense {

  typedef enum {
    DEPTH_STREAM = 0,
    RGB_STREAM = 1,
    IR_STREAM = 2,
    NUM_STREAMS = 3
  } OpenNIStreamID;

  class OpenNIFuncs;
  class KinectDeviceListener;

  class KinectInterfacePrimesense {
  public:
    friend class KinectDeviceListener;
    // If device_id == NULL then it will open the first avalible device
    // Otherwise you can use findDevices() to get a list of connected devices
    KinectInterfacePrimesense(const char* device_uri);  // Starts up the kinect update thread
    ~KinectInterfacePrimesense();  // Must not be called until thread is joined
    void shutdownKinect();  // Blocking until the kinect thread has shut down

    static void findDevices(jtil::data_str::VectorManaged<char*>& devices);

    const uint8_t* rgb() const;  // NOT THREAD SAFE!  Use lockData()
    const uint8_t* ir() const;  // NOT THREAD SAFE!  Use lockData()
    const uint8_t* registered_rgb() const;  // NOT THREAD SAFE!  Use lockData()
    const float* xyz() const;  // NOT THREAD SAFE!  Use lockData()
    const uint16_t* depth() const;  // NOT THREAD SAFE!  Use lockData()
    const uint16_t* depth1mm() const;  // NOT THREAD SAFE!  Use lockData()
    const uint8_t* labels() const { return labels_; }  // NOT THREAD SAFE!  Use lockData()
    const uint8_t* filteredDecisionForestLabels() const;  // NOT THREAD SAFE!  Use lockData()
    const uint8_t* rawDecisionForestLabels() const;  // NOT THREAD SAFE!  Use lockData()
    OpenNIFuncs* openni_funcs() { return openni_funcs_; }
    double depth_frame_time() { return depth_frame_time_; }

    inline void lockData() { data_lock_.lock(); };
    inline void unlockData() { data_lock_.unlock(); };

    const uint64_t depth_frame_number() const { return depth_frame_number_; }
    const uint64_t ir_frame_number() const { return ir_frame_number_; }
    const uint64_t rgb_frame_number() const { return rgb_frame_number_; }
    const jtil::math::Int2& depth_dim() const { return depth_dim_; }
    const jtil::math::Int2& rgb_dim() const { return rgb_dim_; }
    const jtil::math::Int2& ir_dim() const { return ir_dim_; }

  private:
    // Kinect nodes
    openni::Device* device_;
    std::string device_uri_;
    static KinectDeviceListener* device_listener_;
    static jtil::data_str::Vector<KinectInterfacePrimesense*> open_kinects_;
    openni::VideoStream* streams_[NUM_STREAMS];
    openni::VideoFrameRef* frames_[NUM_STREAMS];
    static bool openni_init_;
    static std::mutex openni_static_lock_;
    static uint32_t openni_devices_open_;
    static jtil::clk::Clk shared_clock_;
    bool device_initialized_;
   
    // Multi-threading
    uint32_t threads_finished_;
    jtil::threading::ThreadPool* tp_;
    std::mutex thread_update_lock_;  // For workers to communicate with main thread
    std::condition_variable not_finished_;
    jtil::data_str::VectorManaged<jtil::threading::Callback<void>*> pts_world_thread_cbs_;
    jtil::data_str::VectorManaged<jtil::threading::Callback<void>*> rgb_thread_cbs_;
    std::recursive_mutex data_lock_;
    std::thread kinect_thread_;
    jtil::math::Int2 depth_dim_;
    int depth_fps_setting_;
    jtil::math::Int2 rgb_dim_;  // RGB dimension always matches depth
    int rgb_fps_setting_;
    jtil::math::Int2 ir_dim_; 
    int ir_fps_setting_;
    
    // Processed data
    bool depth_format_100um_;
    OpenNIFuncs* openni_funcs_;
    uint16_t* depth_1mm_;
    uint8_t* registered_rgb_;
    float* pts_uvd_;
    float* pts_world_;
    uint8_t* labels_;  // Generated by hand_detector
    uint64_t depth_frame_number_;
    uint64_t rgb_frame_number_;
    uint64_t ir_frame_number_;
    double depth_frame_time_;
    float max_depth_;
    bool sync_ir_stream_;  // We can either sync the IR or RGB but not both
    bool flip_image_;
    
    bool kinect_running_;
    
    // MAIN UPDATE THREAD:
    void kinectUpdateThread();
    
    void init(const char* device_uri);
    void initOpenNI(const char* device_uri);
    void initDepth();
    void initRGB(const bool start);
    void initIR(const bool start);
    void performConversions();  // depth to XYZ, rgb to depth
    void convertDepthToWorld(const uint32_t start, const uint32_t end);
    void convertRGBToDepth(const uint32_t start, const uint32_t end);
    openni::VideoMode findMaxResYFPSMode(const openni::SensorInfo& sensor,
      const int required_format) const;
    openni::VideoMode findMatchingMode(const openni::SensorInfo& sensor,
      const jtil::math::Int2& dim, const int fps, const int format) const;
    static void initOpenNIStatic();
    static void shutdownOpenNIStatic();
    static void checkOpenNIRC(int rc, const char* error_msg);
    static std::string formatToString(const int mode);
    static void printMode(const openni::VideoMode& mode);
    inline bool kinect_running() const { return kinect_running_; }
    void setCropDepthToRGB(const bool crop_depth_to_rgb);  // internal use only
    void setFlipImage(const bool flip_image);  // internal use only
    void setDepthColorSync(const bool depth_color_sync);  // internal use only
    void executeThreadCallbacks(jtil::threading::ThreadPool* tp, 
    jtil::data_str::VectorManaged<jtil::threading::Callback<void>*>& cbs);
  };
  
#ifndef EPSILON
  #define EPSILON 0.000001f
#endif
  
};  // namespace kinect_interface_primesense
