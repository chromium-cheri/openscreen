// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cast/streaming/offer_messages.h"

#include "gtest/gtest.h"
#include "util/json/json_reader.h"

namespace cast {
namespace streaming {

const std::string kValidOffer = R"(
 {
  "castMode": "mirroring",
 	"receiverGetStatus": true,
 	"supportedStreams": [{
 			"index": 0,
 			"type": "video_source",
 			"codecName": "h264",
 			"rtpProfile": "cast",
 			"rtpPayloadType": 127,
 			"ssrc": 19088743,
 			"maxFrameRate": "60000/1000",
 			"timeBase": "1/90000",
 			"maxBitRate": 5000000,
 			"profile": "main",
 			"level": "4",
      "aesKey": "040d756791711fd3adb939066e6d8690",
      "aesIvMask": "9ff0f022a959150e70a2d05a6c184aed",
 			"resolutions": [{
 					"width": 1280,
 					"height": 720
 				},
 				{
 					"width": 640,
 					"height": 360
 				},
 				{
 					"width": 640,
 					"height": 480
 				}
 			]
 		},
 		{
 			"index": 1,
 			"type": "video_source",
 			"codecName": "vp8",
 			"rtpProfile": "cast",
 			"rtpPayloadType": 127,
 			"ssrc": 19088743,
 			"resolutions": [{
 				"width": 1920,
 				"height": 1080
 			}],
 			"maxFrameRate": "30000/1001",
 			"timeBase": "1/90000",
 			"maxBitRate": 5000000,
 			"profile": "main",
 			"level": "5",
      "aesKey": "bbf109bf84513b456b13a184453b66ce",
      "aesIvMask": "edaf9e4536e2b66191f560d9c04b2a69"
 		},
 		{
 			"index": 2,
 			"type": "audio_source",
 			"codecName": "opus",
 			"rtpProfile": "cast",
 			"rtpPayloadType": 97,
 			"ssrc": 19088743,
 			"bitRate": 124000,
 			"timeBase": "1/48000",
 			"channels": 2,
      "aesKey": "51027e4e2347cbcb49d57ef10177aebc",
      "aesIvMask": "7f12a19be62a36c04ae4116caaeff6d1"
 		}
 	]
 }
)";

TEST(OfferTest, CanParseValidOffer) {
  openscreen::JsonReader reader;
  openscreen::ErrorOr<Json::Value> root = reader.Read(kValidOffer);
  ASSERT_TRUE(root.is_value());
  openscreen::ErrorOr<Offer> offer = Offer::Parse(std::move(root.value()));

  EXPECT_EQ(Offer::CastMode::kMirroring, offer.value().cast_mode());

  // Verify list of video streams.
  EXPECT_EQ(2u, offer.value().video_streams().size());
  auto video_streams = offer.value().video_streams();
  VideoStream vs_one = video_streams[0];
  EXPECT_EQ(0, vs_one.index());
  EXPECT_EQ("video_source", vs_one.type());
  EXPECT_EQ("h264", vs_one.codec_name());
  EXPECT_EQ("cast", vs_one.rtp_profile());
  EXPECT_EQ(127, vs_one.rtp_payload_type());
  EXPECT_EQ(19088743u, vs_one.ssrc());
  EXPECT_EQ("60000/1000", vs_one.max_frame_rate());
  EXPECT_EQ(90000, vs_one.time_base());
  EXPECT_EQ(5000000, vs_one.max_bit_rate());
  EXPECT_EQ("main", vs_one.profile());
  EXPECT_EQ("4", vs_one.level());
  EXPECT_EQ("040d756791711fd3adb939066e6d8690", vs_one.aes_key());
  EXPECT_EQ("9ff0f022a959150e70a2d05a6c184aed", vs_one.aes_iv_mask());

  ResolutionList resolutions = vs_one.resolutions();
  EXPECT_EQ(3, resolutions.size());

  Resolution r_one = resolutions[0];
  EXPECT_EQ(1280, r_one.width());
  EXPECT_EQ(720, r_one.height());

  Resolution r_two = resolutions[1];
  EXPECT_EQ(640, r_two.width());
  EXPECT_EQ(360, r_two.height());

  Resolution r_three = resolutions[2];
  EXPECT_EQ(640, r_three.width());
  EXPECT_EQ(480, r_three.height());

  VideoStream vs_two = video_streams[1];
  EXPECT_EQ(1, vs_two.index());
  EXPECT_EQ("video_source", vs_two.type());
  EXPECT_EQ("vp8", vs_two.codec_name());
  EXPECT_EQ("cast", vs_two.rtp_profile());
  EXPECT_EQ(127, vs_two.rtp_payload_type());
  EXPECT_EQ(19088743u, vs_two.ssrc());
  EXPECT_EQ("30000/1001", vs_two.max_frame_rate());
  EXPECT_EQ(90000, vs_two.time_base());
  EXPECT_EQ(5000000, vs_two.max_bit_rate());
  EXPECT_EQ("main", vs_two.profile());
  EXPECT_EQ("5", vs_two.level());
  EXPECT_EQ("bbf109bf84513b456b13a184453b66ce", vs_two.aes_key());
  EXPECT_EQ("edaf9e4536e2b66191f560d9c04b2a69", vs_two.aes_iv_mask());

  ResolutionList resolutions_two = vs_two.resolutions();
  EXPECT_EQ(1, resolutions_two.size());

  Resolution r = resolutions_two[0];
  EXPECT_EQ(1920, r.width());
  EXPECT_EQ(1080, r.height());

  // Verify list of audio streams.
  EXPECT_EQ(1u, offer.value().audio_streams().size());
  AudioStream as = offer.value().audio_streams()[0];
  EXPECT_EQ(2, as.index());
  EXPECT_EQ("audio_source", as.type());
  EXPECT_EQ("opus", as.codec_name());
  EXPECT_EQ("cast", as.rtp_profile());
  EXPECT_EQ(97, as.rtp_payload_type());
  EXPECT_EQ(19088743u, as.ssrc());
  EXPECT_EQ(124000, as.bit_rate());
  EXPECT_EQ(2, as.channels());
  EXPECT_EQ("51027e4e2347cbcb49d57ef10177aebc", as.aes_key());
  EXPECT_EQ("7f12a19be62a36c04ae4116caaeff6d1", as.aes_iv_mask());
}

}  // namespace streaming
}  // namespace cast
