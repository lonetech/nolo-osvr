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

class NoloDevice {
  public:
    NoloDevice(OSVR_PluginRegContext ctx,
	       char *path) {
        /// Create the initialization options
        OSVR_DeviceInitOptions opts = osvrDeviceCreateInitOptions(ctx);

	m_hid = hid_open_path(path);

        /// Indicate that we'll want 7 analog channels.
        osvrDeviceAnalogConfigure(opts, &m_analog, 2*3+1);
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

    OSVR_ReturnCode update() {
      unsigned char buf[64];
      const int cryptwords = (64-4)/4;
      static const uint32_t key[4] =
	{0x875bcc51, 0xa7637a66, 0x50960967, 0xf8536c51};
      uint32_t cryptpart[cryptwords];
      int i;

      // TODO: timestamp frame received?
      if(hid_read(m_hid, buf, sizeof buf) != sizeof buf)
	return OSVR_RETURN_FAILURE;
	
      // Decrypt encrypted portion
      for (i=0; i<cryptwords; i++) {
	cryptpart[i] =
	  buf[3+4*i  ]<<24 |
	  buf[3+4*i+1]<<16 |
	  buf[3+4*i+2]<<8 |
	  buf[3+4*i+3]<<0;
      }
      btea_decrypt(cryptpart, cryptwords, 1, key);
      for (i=0; i<cryptwords; i++) {
	buf[3+4*i  ] = cryptpart[i]>>24;
	buf[3+4*i+1] = cryptpart[i]>>16;
	buf[3+4*i+2] = cryptpart[i]>>8;
	buf[3+4*i+3] = cryptpart[i]>>0;
      }
      
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
      osvrVec3SetX(pos, (int16_t)(data[0]<<8 | data[1]));
      osvrVec3SetY(pos, (int16_t)(data[2]<<8 | data[3]));
      osvrVec3SetZ(pos,           data[4]<<8 | data[5]);
    }
    void decodeOrientation(const unsigned char *data,
			   OSVR_OrientationState *quat) {
      // TODO: normalize or rescale. Fix order.
      osvrQuatSetW(quat, (int16_t)(data[0]<<8 | data[1]));
      osvrQuatSetX(quat, (int16_t)(data[2]<<8 | data[3]));
      osvrQuatSetY(quat, (int16_t)(data[4]<<8 | data[5]));
      osvrQuatSetZ(quat, (int16_t)(data[6]<<8 | data[7]));
    }
    void decodeControllerCV1(int idx, unsigned char *data) {
      OSVR_PoseState pose;
      uint8_t buttons, bit;

      if (data[0] != 2 || data[1] != 1)
	// Unknown version
	return;

      decodePosition(data+2, &pose.translation);
      decodeOrientation(data+2+3*2, &pose.rotation);

      osvrDeviceTrackerSendPose(m_dev, m_tracker, &pose, 2+idx);
      
      buttons = data[2+3*2+4*2];
      // TODO: report buttons for both controllers in one call?
      for (bit=0; bit<6; bit++)
	osvrDeviceButtonSetValue(m_dev, m_button,
				 (buttons & 1<<bit ? OSVR_BUTTON_PRESSED
				  : OSVR_BUTTON_NOT_PRESSED), idx*6+bit);
      // next byte is touch ID bitmask (identical to buttons bit 5)

      // Touch X and Y coordinates
      osvrDeviceAnalogSetValue(m_dev, m_analog, data[2+3*2+4*2+2],   idx*3+0);
      osvrDeviceAnalogSetValue(m_dev, m_analog, data[2+3*2+4*2+2+1], idx*3+1);
      // battery level
      osvrDeviceAnalogSetValue(m_dev, m_analog, data[2+3*2+4*2+2+2], idx*3+2);
    }
    void decodeHeadsetMarkerCV1(unsigned char *data) {
      OSVR_PoseState pose;
      OSVR_PositionState home;

      if (data[0] != 2 || data[1] != 1)
	// Unknown version
	return;

      decodePosition(data+3, &home);
      decodePosition(data+3+3*2, &pose.translation);
      decodeOrientation(data+3+2*3*2, &pose.rotation);

      osvrDeviceTrackerSendPosition(m_dev, m_tracker, &home, 0);

      osvrDeviceTrackerSendPose(m_dev, m_tracker, &pose, 1);
    }
    void decodeBaseStationCV1(unsigned char *data) {
      if (data[0] != 2 || data[1] != 1)
	// Unknown version
	return;

      osvrDeviceAnalogSetValue(m_dev, m_analog, data[2], 2*3);
    }
  
    static const int controllerLength = 3 + (3+4)*2 + 2 + 2 + 1;
    osvr::pluginkit::DeviceToken m_dev;
    hid_device* m_hid;
    OSVR_AnalogDeviceInterface m_analog;
    OSVR_ButtonDeviceInterface m_button;
    OSVR_TrackerDeviceInterface m_tracker;
};

class HardwareDetection {
  public:
    HardwareDetection() : m_found(false) {}
    OSVR_ReturnCode operator()(OSVR_PluginRegContext ctx) {

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
    osvr::pluginkit::PluginContext context(ctx);

    /// Register a detection callback function object.
    context.registerHardwareDetectCallback(new HardwareDetection());

    return OSVR_RETURN_SUCCESS;
}
