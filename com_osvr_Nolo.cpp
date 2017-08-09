/** @file
    @brief OSVR plugin for LYRobotix Nolo trackers

    @date 2017

    @author
    Yann Vernier
    <http://yann.vernier.se>
*/

// Copyright 2014 Sensics, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// Internal Includes
#include <osvr/PluginKit/PluginKit.h>
#include <osvr/PluginKit/AnalogInterfaceC.h>
#include <osvr/PluginKit/ButtonInterfaceC.h>
#include <osvr/PluginKit/TrackerInterfaceC.h>

// Generated JSON header file
#include "com_osvr_Nolo_json.h"

// Library/third-party includes
#include <hidapi.h>

// Standard includes
#include <iostream>
#include <wchar.h>

// Anonymous namespace to avoid symbol collision
namespace {

  // btea_decrypt function
  #include "btea.c"

  #if 0
  void hexdump(unsigned char *data, int len) {
    char fill = std::cout.fill();
    std::streamsize w = std::cout.width();
    std::ios_base::fmtflags f = std::cout.flags();
    for (int i=0; i<len; i++) {
      std::cout << std::setw(2) << std::setfill('0')
		<< std::hex << (int)data[i];
      if (i%8==7)
	std::cout << " ";
    }
    std::cout << std::endl;
    std::cout.fill(fill);
    std::cout.width(w);
    std::cout.flags(f);
  };
  #endif
  
class NoloDevice {
  public:
    NoloDevice(OSVR_PluginRegContext ctx,
	       char *path) {
        /// Create the initialization options
        OSVR_DeviceInitOptions opts = osvrDeviceCreateInitOptions(ctx);

	std::cout << "Nolo: opening " << path <<
	  " for " << this << std::endl;
	m_hid = hid_open_path(path);

        /// Indicate that we'll want 7 analog channels.
        //osvrDeviceAnalogConfigure(opts, &m_analog, 2*3+1);
	// update this to 9 analog channels to include the trigger
        osvrDeviceAnalogConfigure(opts, &m_analog, 9);
	/// And 6 buttons per controller
        osvrDeviceButtonConfigure(opts, &m_button, 2*6);
	/// And tracker capability
        osvrDeviceTrackerConfigure(opts, &m_tracker);

        /// Create the device token with the options
        m_dev.initAsync(ctx, "LYRobotix Nolo", opts);

        /// Send JSON descriptor
        m_dev.sendJsonDescriptor(com_osvr_Nolo_json);

        /// Register update callback
        m_dev.registerUpdateCallback(this);
    }
    ~NoloDevice() {
      std::cout << "Nolo: deleting " << this << std::endl;
      hid_close(m_hid);
    }
  
    OSVR_ReturnCode update() {
      unsigned char buf[64];
      const int cryptwords = (64-4)/4, cryptoffset=1;
      static const uint32_t key[4] =
	{0x875bcc51, 0xa7637a66, 0x50960967, 0xf8536c51};
      uint32_t cryptpart[cryptwords];
      int i;

      // TODO: timestamp frame received?
      if (!m_hid)
	 OSVR_RETURN_FAILURE;
      //std::cout << "Reading HID data from " << m_hid << std::endl;
      if(hid_read(m_hid, buf, sizeof buf) != sizeof buf)
	return OSVR_RETURN_FAILURE;
      osvrTimeValueGetNow(&m_lastreport_time);

      //std::cout << "R ";
      //hexdump(buf, sizeof buf);
      // Decrypt encrypted portion
      for (i=0; i<cryptwords; i++) {
	cryptpart[i] =
	  ((uint32_t)buf[cryptoffset+4*i  ])<<0  |
	  ((uint32_t)buf[cryptoffset+4*i+1])<<8  |
	  ((uint32_t)buf[cryptoffset+4*i+2])<<16 |
	  ((uint32_t)buf[cryptoffset+4*i+3])<<24;
      }
      btea_decrypt(cryptpart, cryptwords, 1, key);
      for (i=0; i<cryptwords; i++) {
	buf[cryptoffset+4*i  ] = cryptpart[i]>>0;
	buf[cryptoffset+4*i+1] = cryptpart[i]>>8;
	buf[cryptoffset+4*i+2] = cryptpart[i]>>16;
	buf[cryptoffset+4*i+3] = cryptpart[i]>>24;
      }
      //std::cout << "D ";
      //hexdump(buf, sizeof buf);
      
      switch (buf[0]) {
      case 0xa5:  // controllers frame
	decodeControllerCV1(0, buf+1);
	decodeControllerCV1(1, buf+64-controllerLength);
	break;
      case 0xa6:
	decodeHeadsetMarkerCV1(buf+0x15);
	decodeBaseStationCV1(buf+0x36);
	break;
      }
      return OSVR_RETURN_SUCCESS;
    }

    // Sets vibration strength in percent
    int setVibration(unsigned char left,
		     unsigned char right) {
      unsigned char data[4] = {0xaa, 0x66, left, right};
      return hid_write(m_hid, data, sizeof data);
    }
  
  private:
    void decodePosition(const unsigned char *data,
			OSVR_PositionState *pos) {
      const double scale = 0.0001;
      osvrVec3SetX(pos, scale * (int16_t)(data[0]<<8 | data[1]));
      osvrVec3SetY(pos, scale * (int16_t)(data[2]<<8 | data[3]));
      osvrVec3SetZ(pos, scale *          (data[4]<<8 | data[5]));
    }
    void decodeOrientation(const unsigned char *data,
			   OSVR_OrientationState *quat) {
      double w,i,j,k, scale;
      // CV1 order
      w = (int16_t)(data[0]<<8 | data[1]);
      i = (int16_t)(data[2]<<8 | data[3]);
      j = (int16_t)(data[4]<<8 | data[5]);
      k = (int16_t)(data[6]<<8 | data[7]);
      // Normalize (unknown if scale is constant)
      //scale = 1.0/sqrt(i*i+j*j+k*k+w*w);
      // Turns out it is fixed point. But the android driver author
      // either didn't know, or didn't trust it.
      // Unknown if normalizing it helps OSVR!
      scale = 1.0 / 16384;
      //std::cout << "Scale: " << scale << std::endl;
      w *= scale;
      i *= scale;
      j *= scale;
      k *= scale;
      // Reorder
      osvrQuatSetW(quat, w);
      osvrQuatSetX(quat, i);
      osvrQuatSetY(quat, k);
      osvrQuatSetZ(quat, -j);
    }
    void decodeControllerCV1(int idx, unsigned char *data) {
      OSVR_PoseState pose;
      uint8_t buttons, bit;

      if (data[0] != 2 || data[1] != 1) {
	// Unknown version
	/* Happens when controllers aren't on. 
	std::cout << "Nolo: Unknown controller " << idx
		  << " version " << (int)data[0] << " " << (int)data[1]
		  << std::endl;
	*/
	return;
      }

      decodePosition(data+3, &pose.translation);
      decodeOrientation(data+3+3*2, &pose.rotation);

      osvrDeviceTrackerSendPoseTimestamped(m_dev, m_tracker, &pose, 2+idx, &m_lastreport_time);
      
      buttons = data[3+3*2+4*2];
      // TODO: report buttons for both controllers in one call?
      for (bit=0; bit<6; bit++)
	osvrDeviceButtonSetValueTimestamped(m_dev, m_button,
				 (buttons & 1<<bit ? OSVR_BUTTON_PRESSED
				  : OSVR_BUTTON_NOT_PRESSED), idx*6+bit,
				 &m_lastreport_time);
      // next byte is touch ID bitmask (identical to buttons bit 5)

      // Touch X and Y coordinates
      // //assumes 0 to 255
      // normalize from -1 to 1
      // z = 2*[x - min / (max - min) - 1]
      // z = 2*(x - 0 / (255 - 0) - 1]
      // z = 2*(x/255) -1
      // normalize 0 to 1
      // x/255

      double axis_value = 2*data[3+3*2+4*2+2]/255.0 - 1;
      // invert axis
      axis_value *= -1;
      osvrDeviceAnalogSetValueTimestamped(m_dev, m_analog, axis_value,   idx*3+0, &m_lastreport_time);
      axis_value = 2*((int)data[3+3*2+4*2+2+1])/255.0 -1;
      axis_value *= -1;
      osvrDeviceAnalogSetValueTimestamped(m_dev, m_analog, axis_value, idx*3+1, &m_lastreport_time);
      // battery level
      axis_value = data[3+3*2+4*2+2+2]/255.0; 
      osvrDeviceAnalogSetValueTimestamped(m_dev, m_analog, axis_value, idx*3+2, &m_lastreport_time);
      // trigger
      axis_value = data[3+3*2+4*2+2+3]/255.0; 
      osvrDeviceAnalogSetValueTimestamped(m_dev, m_analog, axis_value, idx*3+3, &m_lastreport_time);
      /*
      osvrDeviceAnalogSetValueTimestamped(m_dev, m_analog, data[3+3*2+4*2+2],   idx*3+0, &m_lastreport_time);
      osvrDeviceAnalogSetValueTimestamped(m_dev, m_analog, data[3+3*2+4*2+2+1], idx*3+1, &m_lastreport_time);
      // battery level
      osvrDeviceAnalogSetValueTimestamped(m_dev, m_analog, data[3+3*2+4*2+2+2], idx*3+2, &m_lastreport_time);
      */
    }
    void decodeHeadsetMarkerCV1(unsigned char *data) {
      if (data[0] != 2 || data[1] != 1) {
	/* Happens with corrupt packages (mixed with controller data)
	std::cout << "Nolo: Unknown headset marker"
		  << " version " << (int)data[0] << " " << (int)data[1]
		  << std::endl;
	*/
	// Unknown version
	return;
      }

      OSVR_PositionState home;
      OSVR_PoseState hmd;
      
      decodePosition(data+3, &hmd.translation);
      decodePosition(data+3+3*2, &home);
      decodeOrientation(data+3+2*3*2+1, &hmd.rotation);

      // Tracker viewer kept using the home for head.
      // Something wrong with how they handle the descriptors. 
      osvrDeviceTrackerSendPositionTimestamped(m_dev, m_tracker, &home, 0,
		      &m_lastreport_time);

      osvrDeviceTrackerSendPoseTimestamped(m_dev, m_tracker, &hmd, 1,
		      &m_lastreport_time);
    }
    void decodeBaseStationCV1(unsigned char *data) {
      if (data[0] != 2 || data[1] != 1)
	// Unknown version
	return;

      osvrDeviceAnalogSetValueTimestamped(m_dev, m_analog, data[2], 2*3,
		      &m_lastreport_time);
    }
  
    static const int controllerLength = 3 + (3+4)*2 + 2 + 2 + 1;
    osvr::pluginkit::DeviceToken m_dev;
    hid_device* m_hid;
    OSVR_AnalogDeviceInterface m_analog;
    OSVR_ButtonDeviceInterface m_button;
    OSVR_TrackerDeviceInterface m_tracker;
    OSVR_TimeValue m_lastreport_time;
};

class HardwareDetection {
  public:
    HardwareDetection() : m_found(false) {}
    OSVR_ReturnCode operator()(OSVR_PluginRegContext ctx) {

      // TODO: probe for new devices only
      if (m_found)
	return OSVR_RETURN_SUCCESS;

      struct hid_device_info *hid_devices, *cur_dev;
      hid_devices = hid_enumerate(0x0483, 0x5750);
      if (!hid_devices)
	return OSVR_RETURN_FAILURE;

      for (cur_dev = hid_devices; cur_dev; cur_dev = cur_dev->next) {
	if (wcscmp(cur_dev->manufacturer_string, L"LYRobotix")==0 &&
	    wcscmp(cur_dev->product_string, L"NOLO")==0) {
	  /// TODO: Distinguish Headset Marker from other parts?
	  /// Create our device object
	  osvr::pluginkit::registerObjectForDeletion
	    (ctx, new NoloDevice(ctx, cur_dev->path));
	  m_found = true;
	}
      }
      
      hid_free_enumeration(hid_devices);
      return OSVR_RETURN_SUCCESS;
    }

  private:
    /// @brief Have we found our device yet? (this limits the plugin to one
    /// instance)
    bool m_found;
};
} // namespace

OSVR_PLUGIN(com_osvr_Nolo) {
    hid_init();
    osvr::pluginkit::PluginContext context(ctx);

    /// Register a detection callback function object.
    context.registerHardwareDetectCallback(new HardwareDetection());

    return OSVR_RETURN_SUCCESS;
}
