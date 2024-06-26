/**
 * frame.cpp: handles the sending of "pwnagotchi" beacon frames
 */

#include "frame.h"

/** developer note:
 *
 * when it comes to detecting a pwnagotchi, this is done with pwngrid/opwngrid.
 * essentially pwngrid looks for the numbers 222-226 in payloads, and if they
 * aren't there, it ignores it. these need to be put into the frames!!!
 *
 * note that these frames aren't just normal beacon frames, rather a modified
 * one with data, additional ids, etc. frames are dynamically constructed,
 * headers are included like a normal frame. by far this is the most memory
 * heaviest part of the minigotchi, the reason is
 *
 */

// initializing
size_t Frame::frameSize = 0;
std::vector<uint8_t> Frame::beaconFrame;
size_t Frame::payloadSize = 0;
const size_t Frame::chunkSize = 0xFF;
bool Frame::sent = false;

// payload ID's according to pwngrid
const uint8_t Frame::IDWhisperPayload = 0xDE;
const uint8_t Frame::IDWhisperCompression = 0xDF;
const uint8_t Frame::IDWhisperIdentity = 0xE0;
const uint8_t Frame::IDWhisperSignature = 0xE1;
const uint8_t Frame::IDWhisperStreamHeader = 0xE2;

// other addresses
const uint8_t Frame::SignatureAddr[] = {0xde, 0xad, 0xbe, 0xef, 0xde, 0xad};
const uint8_t Frame::BroadcastAddr[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
const uint16_t Frame::wpaFlags = 0x0411;

const uint8_t Frame::header[]{
    /*  0 - 1  */ 0x80,
    0x00, // frame control, beacon frame
    /*  2 - 3  */ 0x00,
    0x00, // duration
    /*  4 - 9  */ 0xff,
    0xff,
    0xff,
    0xff,
    0xff,
    0xff, // broadcast address
    /* 10 - 15 */ 0xde,
    0xad,
    0xbe,
    0xef,
    0xde,
    0xad, // source address
    /* 16 - 21 */ 0xa1,
    0x00,
    0x64,
    0xe6,
    0x0b,
    0x8b, // bssid
    /* 22 - 23 */ 0x40,
    0x43, // fragment and sequence number
    /* 24 - 32 */ 0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // timestamp
    /* 33 - 34 */ 0x64,
    0x00, // interval
    /* 35 - 36 */ 0x11,
    0x04, // capability info
};

/** developer note:
 *
 * according to pack.go:
 * we build the frame like so
 *
 * func PackOneOf(from, to net.HardwareAddr, peerID []byte, signature []byte,
 *streamID uint64, seqNum uint64, seqTot uint64, payload []byte, compress bool)
 *(error, []byte) { stack := []gopacket.SerializableLayer{ &layers.RadioTap{},
 *		&layers.Dot11{
 *			Address1: to,
 *			Address2: SignatureAddr,
 *			Address3: from,
 *			Type:     layers.Dot11TypeMgmtBeacon,
 *		},
 * 		&layers.Dot11MgmtBeacon{
 *			Flags:    uint16(wpaFlags),
 *			Interval: 100,
 *		},
 *	}
 *
 *	if peerID != nil {
 *		stack = append(stack, Info(IDWhisperIdentity, peerID))
 *	}
 *
 *	if signature != nil {
 *		stack = append(stack, Info(IDWhisperSignature, signature))
 *	}
 *
 *	if streamID > 0 {
 *		streamBuf := new(bytes.Buffer)
 *		if err := binary.Write(streamBuf, binary.LittleEndian,
 *streamID); err != nil { return err, nil } else if err =
 *binary.Write(streamBuf, binary.LittleEndian, seqNum); err != nil { return err,
 *nil } else if err = binary.Write(streamBuf, binary.LittleEndian, seqTot); err
 *!= nil { return err, nil
 *		}
 *		stack = append(stack, Info(IDWhisperStreamHeader,
 *streamBuf.Bytes()))
 *	}
 *
 *	if compress {
 *		if didCompress, compressed, err := Compress(payload); err != nil
 *{ return err, nil } else if didCompress { stack = append(stack,
 *Info(IDWhisperCompression, []byte{1})) payload = compressed
 *		}
 *	}
 *
 *	dataSize := len(payload)
 *	dataLeft := dataSize
 *	dataOff := 0
 *	chunkSize := 0xff
 *
 *	for dataLeft > 0 {
 *		sz := chunkSize
 *		if dataLeft < chunkSize {
 *			sz = dataLeft
 *		}
 *
 *		chunk := payload[dataOff : dataOff+sz]
 *		stack = append(stack, Info(IDWhisperPayload, chunk))
 *
 *		dataOff += sz
 *		dataLeft -= sz
 *	}
 *
 *	return Serialize(stack...)
 * }
 *
 * ofc, when it comes to any new programming language such as Go, i am pretty
 *clueless as how to interpret it so this is all my best try
 *
 */

/** developer note:
 *
 * we're relying off of the frame structure from pack.go
 *
 * func PackOneOf(from, to net.HardwareAddr, peerID []byte, signature []byte,
 *streamID uint64, seqNum uint64, seqTot uint64, payload []byte, compress bool)
 *(error, []byte) { stack := []gopacket.SerializableLayer{ &layers.RadioTap{},
 *		&layers.Dot11{
 *			Address1: to,
 *			Address2: SignatureAddr,
 *			Address3: from,
 *			Type:     layers.Dot11TypeMgmtBeacon,
 *		},
 * 		&layers.Dot11MgmtBeacon{
 *			Flags:    uint16(wpaFlags),
 *			Interval: 100,
 *		},
 *	}
 *
 * see wifi_ieee80211_mac_hdr_t in structs.h for the frame structure...
 *
 */

void Frame::init() {
  // insert header
  Frame::beaconFrame.reserve(Frame::beaconFrame.size() + 2 + sizeof(header));
  Frame::beaconFrame.insert(Frame::beaconFrame.end(), std::begin(header),
                            std::end(header));
}

void Frame::essid() {
  // make a json doc
  String jsonString;
  DynamicJsonDocument doc(1024);

  doc["epoch"] = Config::epoch;
  doc["face"] = Config::face;
  doc["identity"] = Config::identity;
  doc["name"] = Config::name;

  JsonObject policy = doc.createNestedObject("policy");
  policy["advertise"] = Config::advertise;
  policy["ap_ttl"] = Config::ap_ttl;
  policy["associate"] = Config::associate;
  policy["bored_num_epochs"] = Config::bored_num_epochs;

  JsonArray channels = policy.createNestedArray("channels");
  for (size_t i = 0; i < sizeof(Config::channels) / sizeof(Config::channels[0]);
       ++i) {
    channels.add(Config::channels[i]);
  }

  policy["deauth"] = Config::deauth;
  policy["excited_num_epochs"] = Config::excited_num_epochs;
  policy["hop_recon_time"] = Config::hop_recon_time;
  policy["max_inactive_scale"] = Config::max_inactive_scale;
  policy["max_interactions"] = Config::max_interactions;
  policy["max_misses_for_recon"] = Config::max_misses_for_recon;
  policy["min_recon_time"] = Config::min_rssi;
  policy["min_rssi"] = Config::min_rssi;
  policy["recon_inactive_multiplier"] = Config::recon_inactive_multiplier;
  policy["recon_time"] = Config::recon_time;
  policy["sad_num_epochs"] = Config::sad_num_epochs;
  policy["sta_ttl"] = Config::sta_ttl;

  doc["pwnd_run"] = Config::pwnd_run;
  doc["pwnd_tot"] = Config::pwnd_tot;
  doc["session_id"] = Config::session_id;
  doc["uptime"] = Config::uptime;
  doc["version"] = Config::version;

  // serialize then put into beacon frame
  serializeJson(doc, jsonString);
  uint8_t essidLength = jsonString.length();
  Frame::beaconFrame.reserve(Frame::beaconFrame.size() + 2 + essidLength);
  Frame::beaconFrame.push_back(Frame::IDWhisperCompression);
  Frame::beaconFrame.push_back(essidLength);
  Frame::beaconFrame.insert(Frame::beaconFrame.end(), jsonString.begin(),
                            jsonString.end());

  /** developer note:
   *
   * if you literally want to check the json everytime you send a packet(non
   * serialized ofc)
   *
   * Serial.println(jsonString);
   */
}

/** developer note:
 *
 * frame structure based on how it was built here
 *
 * 1. header
 * 2. payload id's
 * 3. (chunked) pwnagotchi data
 *
 */

void Frame::pack() {
  // clear frame before constructing
  Frame::beaconFrame.clear();

  // add the header and essid
  init();
  essid();

  // payload size
  Frame::payloadSize = Frame::beaconFrame.size();

  // store for later
  std::vector<uint8_t> originalBeaconFrame = Frame::beaconFrame;

  for (size_t i = 0; i < payloadSize; i += Frame::chunkSize) {
    Frame::beaconFrame.push_back(Frame::IDWhisperPayload);

    size_t chunkEnd = std::min(i + Frame::chunkSize, Frame::payloadSize);
    for (size_t j = i; j < chunkEnd; ++j) {
      Frame::beaconFrame.push_back(originalBeaconFrame[j]);
    }
  }

  // update size since we changed the frame
  Frame::frameSize = Frame::beaconFrame.size();

  /** developer note:
   *
   * we can print the beacon frame like so...
   *
   * Serial.println("('-') Full Beacon Frame:");
   * for (size_t i = 0; i < beaconFrame.size(); ++i) {
   *     Serial.print(beaconFrame[i], HEX);
   *     Serial.print(" ");
   * }
   * Serial.println(" ");
   *
   */
}

bool Frame::send() {
  // build frame
  Frame::pack();

  // send full frame
  // we dont use raw80211 since it sends a header(which we don't need), although
  // we do use it for monitoring, etc.
  Frame::sent =
      wifi_send_pkt_freedom(Frame::beaconFrame.data(), Frame::frameSize, 0);
  delay(102);
  return (Frame::sent == 0);
}

void Frame::advertise() {
  int packets = 0;
  unsigned long startTime = millis();

  if (Config::advertise) {
    Serial.println("(>-<) Starting advertisment...");
    Serial.println(" ");
    Display::cleanDisplayFace("(>-<)");
    Display::attachSmallText("Starting advertisment...");
    Minigotchi::monStart();
    Parasite::sendAdvertising();
    delay(Config::shortDelay);
    for (int i = 0; i < 150; ++i) {
      if (Frame::send()) {
        packets++;

        // calculate packets per second
        float pps = packets / (float)(millis() - startTime) * 1000;

        // show pps
        if (!isinf(pps)) {
          Serial.print("(>-<) Packets per second: ");
          Serial.print(pps);
          Serial.print(" pkt/s (Channel: ");
          Serial.print(Channel::getChannel());
          Serial.println(")");
          Display::cleanDisplayFace("(>-<)");
          Display::attachSmallText(
              "Packets per second: " + (String)pps + " pkt/s" +
              "(Channel: " + (String)Channel::getChannel() + ")");
        }
      } else {
        Serial.println("(X-X) Advertisment failed to send!");
      }
    }

    Serial.println(" ");
    Serial.println("(^-^) Advertisment finished!");
    Serial.println(" ");
    Display::cleanDisplayFace("(^-^)");
    Display::attachSmallText("Advertisment finished!");
  } else {
    // do nothing but still idle
  }
}
