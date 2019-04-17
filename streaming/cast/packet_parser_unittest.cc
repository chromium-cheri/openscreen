// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "streaming/cast/packet_parser.h"

#include "streaming/cast/rtp_defines.h"
#include "third_party/googletest/src/googletest/include/gtest/gtest.h"

namespace openscreen {
namespace cast_streaming {
namespace {

// clang-format off
// ...because it makes the test input data unreadable!

// Tests that a simple packet for a key frame can be parsed.
TEST(PacketParserTest, ParsesRtpPacketForKeyFrame) {
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    static_cast<uint8_t>(RtpPayloadType::kAudioOpus),  // Payload type byte.
    0xbe, 0xef,  // Sequence number.
    9, 8, 7, 6,  // RTP timestamp.
    1, 2, 3, 4,  // SSRC.
    0b10000000,  // Is key frame, no extensions.
    5,  // Frame ID.
    0xa, 0xb,  // Packet ID.
    0xa, 0xc,  // Max packet ID.
    0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,  // Payload.
  };
  const Ssrc kRemoteSsrc = 0x01020304;

  PacketParser parser(Ssrc{0}, kRemoteSsrc);
  ASSERT_TRUE(PacketParser::IsPacketFromPeer(kInput, kRemoteSsrc));
  ASSERT_TRUE(parser.Parse(kInput));
  EXPECT_TRUE(parser.has_rtp_header_and_payload());
  const RtpCastHeader& header = parser.rtp_header();
  EXPECT_EQ(RtpPayloadType::kAudioOpus, header.payload_type);
  EXPECT_EQ(UINT16_C(0xbeef), header.sequence_number);
  EXPECT_EQ(RtpTimeTicks() + RtpTimeDelta::FromTicks(0x09080706),
            header.rtp_timestamp);
  EXPECT_TRUE(header.is_key_frame);
  EXPECT_EQ(FrameId::first() + 5, header.frame_id);
  EXPECT_EQ(UINT16_C(0x0a0b), header.packet_id);
  EXPECT_EQ(UINT16_C(0x0a0c), header.max_packet_id);
  EXPECT_EQ(FrameId::first() + 5, header.referenced_frame_id);
  EXPECT_EQ(0, header.new_playout_delay.count());
  const absl::Span<const uint8_t> expected_payload(kInput + 18, 8);
  ASSERT_EQ(expected_payload, parser.rtp_payload());
  EXPECT_TRUE(expected_payload == parser.rtp_payload());
}

// Tests that a packet which includes a "referenced frame ID" can be parsed.
TEST(PacketParserTest, ParsesRtpPacketForNonKeyFrameWithReferenceFrameId) {
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    static_cast<uint8_t>(RtpPayloadType::kAudioOpus),  // Payload type byte.
    0xde, 0xad,  // Sequence number.
    2, 4, 6, 8,  // RTP timestamp.
    0, 0, 1, 1,  // SSRC.
    0b01000000,  // Not a key frame, but has ref frame ID; no extensions.
    42,  // Frame ID.
    0x0, 0xb,  // Packet ID.
    0x0, 0xc,  // Max packet ID.
    39,  // Reference Frame ID.
    1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29  // Payload.
  };
  const Ssrc kRemoteSsrc = 0x00000101;

  PacketParser parser(Ssrc{0}, kRemoteSsrc);
  ASSERT_TRUE(PacketParser::IsPacketFromPeer(kInput, kRemoteSsrc));
  ASSERT_TRUE(parser.Parse(kInput));
  EXPECT_TRUE(parser.has_rtp_header_and_payload());
  const RtpCastHeader& header = parser.rtp_header();
  EXPECT_EQ(RtpPayloadType::kAudioOpus, header.payload_type);
  EXPECT_EQ(UINT16_C(0xdead), header.sequence_number);
  EXPECT_EQ(RtpTimeTicks() + RtpTimeDelta::FromTicks(0x02040608),
            header.rtp_timestamp);
  EXPECT_FALSE(header.is_key_frame);
  EXPECT_EQ(FrameId::first() + 42, header.frame_id);
  EXPECT_EQ(UINT16_C(0x000b), header.packet_id);
  EXPECT_EQ(UINT16_C(0x000c), header.max_packet_id);
  EXPECT_EQ(FrameId::first() + 39, header.referenced_frame_id);
  EXPECT_EQ(0, header.new_playout_delay.count());
  const absl::Span<const uint8_t> expected_payload(kInput + 19, 15);
  ASSERT_EQ(expected_payload, parser.rtp_payload());
  EXPECT_TRUE(expected_payload == parser.rtp_payload());
}

// Tests that a packet which lacks a "referenced frame ID" field can be parsed,
// but the parser will provide the implied referenced_frame_id value in the
// result.
TEST(PacketParserTest, ParsesRtpPacketForNonKeyFrameWithoutReferenceFrameId) {
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    static_cast<uint8_t>(RtpPayloadType::kAudioOpus),  // Payload type byte.
    0xde, 0xad,  // Sequence number.
    2, 4, 6, 8,  // RTP timestamp.
    0, 0, 1, 1,  // SSRC.
    0b00000000,  // Not a key frame, no ref frame ID; no extensions.
    42,  // Frame ID.
    0x0, 0xb,  // Packet ID.
    0x0, 0xc,  // Max packet ID.
    1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29  // Payload.
  };
  const Ssrc kRemoteSsrc = 0x00000101;

  PacketParser parser(Ssrc{0}, kRemoteSsrc);
  ASSERT_TRUE(PacketParser::IsPacketFromPeer(kInput, kRemoteSsrc));
  ASSERT_TRUE(parser.Parse(kInput));
  EXPECT_TRUE(parser.has_rtp_header_and_payload());
  const RtpCastHeader& header = parser.rtp_header();
  EXPECT_EQ(RtpPayloadType::kAudioOpus, header.payload_type);
  EXPECT_EQ(UINT16_C(0xdead), header.sequence_number);
  EXPECT_EQ(RtpTimeTicks() + RtpTimeDelta::FromTicks(0x02040608),
            header.rtp_timestamp);
  EXPECT_FALSE(header.is_key_frame);
  EXPECT_EQ(FrameId::first() + 42, header.frame_id);
  EXPECT_EQ(UINT16_C(0x000b), header.packet_id);
  EXPECT_EQ(UINT16_C(0x000c), header.max_packet_id);
  EXPECT_EQ(FrameId::first() + 41, header.referenced_frame_id);
  EXPECT_EQ(0, header.new_playout_delay.count());
  const absl::Span<const uint8_t> expected_payload(kInput + 18, 15);
  ASSERT_EQ(expected_payload, parser.rtp_payload());
  EXPECT_TRUE(expected_payload == parser.rtp_payload());
}

// Tests that a packet indicating a new playout delay can be parsed.
TEST(PacketParserTest, ParsesRtpPacketWithAdaptiveLatencyExtension) {
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    static_cast<uint8_t>(RtpPayloadType::kAudioOpus),  // Payload type byte.
    0xde, 0xad,  // Sequence number.
    2, 4, 6, 8,  // RTP timestamp.
    0, 0, 1, 1,  // SSRC.
    0b11000001,  // Is key frame, has ref frame ID; has one extension.
    64,  // Frame ID.
    0x0, 0x0,  // Packet ID.
    0x0, 0xc,  // Max packet ID.
    64,  // Reference Frame ID.
    4, 2, 1, 14,  // Cast Adaptive Latency Extension data.
    1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29  // Payload.
  };
  const Ssrc kRemoteSsrc = 0x00000101;

  PacketParser parser(Ssrc{0}, kRemoteSsrc);
  ASSERT_TRUE(PacketParser::IsPacketFromPeer(kInput, kRemoteSsrc));
  ASSERT_TRUE(parser.Parse(kInput));
  EXPECT_TRUE(parser.has_rtp_header_and_payload());
  const RtpCastHeader& header = parser.rtp_header();
  EXPECT_EQ(RtpPayloadType::kAudioOpus, header.payload_type);
  EXPECT_EQ(UINT16_C(0xdead), header.sequence_number);
  EXPECT_EQ(RtpTimeTicks() + RtpTimeDelta::FromTicks(0x02040608),
            header.rtp_timestamp);
  EXPECT_TRUE(header.is_key_frame);
  EXPECT_EQ(FrameId::first() + 64, header.frame_id);
  EXPECT_EQ(UINT16_C(0x0000), header.packet_id);
  EXPECT_EQ(UINT16_C(0x000c), header.max_packet_id);
  EXPECT_EQ(FrameId::first() + 64, header.referenced_frame_id);
  EXPECT_EQ(270, header.new_playout_delay.count());
  const absl::Span<const uint8_t> expected_payload(kInput + 23, 15);
  ASSERT_EQ(expected_payload, parser.rtp_payload());
  EXPECT_TRUE(expected_payload == parser.rtp_payload());
}

// Tests that the parser can handle multiple Cast Header Extensions in a RTP
// packet, and ignores all but the one (Adaptive Latency) that it understands.
TEST(PacketParserTest, ParsesRtpPacketWithMultipleExtensions) {
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    static_cast<uint8_t>(RtpPayloadType::kAudioOpus),  // Payload type byte.
    0xde, 0xad,  // Sequence number.
    2, 4, 6, 8,  // RTP timestamp.
    0, 0, 1, 1,  // SSRC.
    0b11000011,  // Is key frame, has ref frame ID; has 3 extensions.
    64,  // Frame ID.
    0x0, 0xb,  // Packet ID.
    0x0, 0xc,  // Max packet ID.
    64,  // Reference Frame ID.
    8, 2, 0, 0,  // Unknown extension with 2 bytes of data.
    4, 2, 1, 14,  // Cast Adaptive Latency Extension data.
    16, 5, 0, 0, 0, 0, 0,  // Unknown extension with 5 bytes of data.
    1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29  // Payload.
  };
  const Ssrc kRemoteSsrc = 0x00000101;

  PacketParser parser(Ssrc{0}, kRemoteSsrc);
  ASSERT_TRUE(PacketParser::IsPacketFromPeer(kInput, kRemoteSsrc));
  ASSERT_TRUE(parser.Parse(kInput));
  EXPECT_TRUE(parser.has_rtp_header_and_payload());
  const RtpCastHeader& header = parser.rtp_header();
  EXPECT_EQ(RtpPayloadType::kAudioOpus, header.payload_type);
  EXPECT_EQ(UINT16_C(0xdead), header.sequence_number);
  EXPECT_EQ(RtpTimeTicks() + RtpTimeDelta::FromTicks(0x02040608),
            header.rtp_timestamp);
  EXPECT_TRUE(header.is_key_frame);
  EXPECT_EQ(FrameId::first() + 64, header.frame_id);
  EXPECT_EQ(UINT16_C(0x000b), header.packet_id);
  EXPECT_EQ(UINT16_C(0x000c), header.max_packet_id);
  EXPECT_EQ(FrameId::first() + 64, header.referenced_frame_id);
  EXPECT_EQ(270, header.new_playout_delay.count());
  const absl::Span<const uint8_t> expected_payload(kInput + 34, 15);
  ASSERT_EQ(expected_payload, parser.rtp_payload());
  EXPECT_TRUE(expected_payload == parser.rtp_payload());
}

// Tests that the parser ignores packets from an unknown source.
TEST(PacketParserTest, IgnoresRtpPacketWithWrongSsrc) {
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    static_cast<uint8_t>(RtpPayloadType::kAudioOpus),  // Payload type byte.
    0xbe, 0xef,  // Sequence number.
    9, 8, 7, 6,  // RTP timestamp.
    4, 3, 2, 1,  // SSRC.
    0b10000000,  // Is key frame, no extensions.
    5,  // Frame ID.
    0xa, 0xb,  // Packet ID.
    0xa, 0xc,  // Max packet ID.
    0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,  // Payload.
  };
  const Ssrc kRemoteSsrc = 0x01020304;

  PacketParser parser(Ssrc{0}, kRemoteSsrc);
  ASSERT_FALSE(PacketParser::IsPacketFromPeer(kInput, kRemoteSsrc));
  ASSERT_TRUE(parser.Parse(kInput));
  EXPECT_FALSE(parser.has_rtp_header_and_payload());
  EXPECT_EQ(absl::Span<const uint8_t>{}, parser.rtp_payload());
}

// Tests that unexpected truncations in the RTP packets does not crash the
// parser, and that it correctly errors-out.
TEST(PacketParserTest, RejectsTruncatedRtpPackets) {
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    static_cast<uint8_t>(RtpPayloadType::kAudioOpus),  // Payload type byte.
    0xde, 0xad,  // Sequence number.
    2, 4, 6, 8,  // RTP timestamp.
    0, 0, 1, 1,  // SSRC.
    0b11000011,  // Is key frame, has ref frame ID; has 3 extensions.
    64,  // Frame ID.
    0x0, 0xb,  // Packet ID.
    0x0, 0xc,  // Max packet ID.
    64,  // Reference Frame ID.
    8, 2, 0, 0,  // Unknown extension with 2 bytes of data.
    4, 2, 1, 14,  // Cast Adaptive Latency Extension data.
    16, 5, 0, 0, 0, 0, 0,  // Unknown extension with 5 bytes of data.
    1, 3, 5, 7, 9, 11, 13, 15, 17, 19, 21, 23, 25, 27, 29  // Payload.
  };
  const Ssrc kRemoteSsrc = 0x00000101;

  PacketParser parser(Ssrc{0}, kRemoteSsrc);
  ASSERT_FALSE(parser.Parse({kInput, 1}));
  ASSERT_FALSE(parser.Parse({kInput, 18}));
  ASSERT_FALSE(parser.Parse({kInput, 22}));
  ASSERT_FALSE(parser.Parse({kInput, 33}));

  // When truncated to 34 bytes, the parser should see it as a packet with zero
  // payload bytes.
  ASSERT_TRUE(parser.Parse({kInput, 34}));

  // And, of course, with the entire kInput available, the parser should see it
  // as a packet with 15 bytes of payload.
  ASSERT_TRUE(parser.Parse({kInput, sizeof(kInput)}));
}

// Tests that the parser rejects invalid packet ID values.
TEST(PacketParserTest, RejectsRtpPacketWithBadPacketId) {
  const uint8_t kInput[] = {
    0b10000000,  // Version/Padding byte.
    static_cast<uint8_t>(RtpPayloadType::kAudioOpus),  // Payload type byte.
    0xbe, 0xef,  // Sequence number.
    9, 8, 7, 6,  // RTP timestamp.
    1, 2, 3, 4,  // SSRC.
    0b10000000,  // Is key frame, no extensions.
    5,  // Frame ID.
    0xa, 0xb,  // Packet ID (which is GREATER than the max packet ID).
    0x0, 0x1,  // Max packet ID.
    0xf, 0xe, 0xd, 0xc, 0xb, 0xa, 0x9, 0x8,  // Payload.
  };
  const Ssrc kRemoteSsrc = 0x01020304;

  PacketParser parser(Ssrc{0}, kRemoteSsrc);
  ASSERT_FALSE(parser.Parse(kInput));
}

// clang-format on

}  // namespace
}  // namespace cast_streaming
}  // namespace openscreen
