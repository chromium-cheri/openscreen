Problem:

Playback is complete crap.

Looks like the sender has enqueued much more than the receiver has/is willing
to play?

```
[INFO:../../cast/streaming/sender.cc(108):T0] Enqueueing frame with id: F54
[INFO:../../cast/streaming/sender.cc(121):T0] MAX INFLIGHT: 240710, INFLIGHT: 1440000
[INFO:../../cast/standalone_sender/streaming_opus_encoder.cc(132):T0] AUDIO[26018] Dropping: In-flight duration would be too high.
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F53
[INFO:../../cast/streaming/sender_packet_router.cc(171):T0] Sent packets over sender packet router: RTCP 0, RCP: 1
[INFO:../../cast/streaming/sender_packet_router.cc(151):T0] Scheduling packet burst for 129194271224 μs-ticks, first allowable: 129194118384 μs-ticks
[WARNING:../../cast/standalone_sender/streaming_vp8_encoder.cc(160):T0] VIDEO[51460] Dropping: In-flight media duration would be too high.
```

And receiver:

```
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(127):T0] Decoded frame_id F0
[VERBOSE:../../cast/streaming/receiver.cc(136):T0] [SSRC:51461] AdvanceToNextFrame: No frames ready. Last consumed was F7.
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(180):T0] Dropping frame with presentation time: 129188971316 μs-ticks
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(192):T0] Set current frame to present at: 129188971316 μs-ticks, start time at: 129189000001 μs-ticks
[INFO:../../cast/standalone_receiver/sdl_video_player.cc(106):T0] Creating SDL texture for SDL_PIXELFORMAT_IYUV at 1920×1080
[INFO:../../cast/streaming/receiver.cc(72):T0] Set player processing time: 547101µs
[VERBOSE:../../cast/streaming/receiver.cc(136):T0] [SSRC:51461] AdvanceToNextFrame: No frames ready. Last consumed was F7.
[INFO:../../cast/streaming/receiver.cc(200):T0] Updating max allowed frame id: F127
[VERBOSE:../../cast/streaming/receiver.cc(211):T0] [SSRC:51461] Advanced latest frame expected to F8
[VERBOSE:../../cast/streaming/receiver.cc(136):T0] [SSRC:51461] AdvanceToNextFrame: No frames ready. Last consumed was F7.
[INFO:../../cast/streaming/receiver.cc(351):T0] Frame acks are: (1)
[INFO:../../cast/streaming/receiver.cc(353):T0] "F28"
[VERBOSE:../../cast/streaming/receiver.cc(364):T0] [SSRC:26019] Sent RTCP packet.
[VERBOSE:../../cast/streaming/receiver.cc(441):T0] [SSRC:51461] Advancing checkpoint to F8
[INFO:../../cast/streaming/receiver.cc(351):T0] Frame acks are: (1)
[INFO:../../cast/streaming/receiver.cc(353):T0] "F8"
[VERBOSE:../../cast/streaming/receiver.cc(364):T0] [SSRC:51461] Sent RTCP packet.
[VERBOSE:../../cast/streaming/receiver.cc(101):T0] [SSRC:51461] AdvanceToNextFrame: Next in sequence (F8)
[VERBOSE:../../cast/streaming/receiver.cc(157):T0] [SSRC:51461] ConsumeNextFrame → F8: 11095 payload bytes, RTP Timestamp 300000 µs, to play-out -4456814 µs from now.
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(127):T0] Decoded frame_id F1
[VERBOSE:../../cast/streaming/receiver.cc(136):T0] [SSRC:51461] AdvanceToNextFrame: No frames ready. Last consumed was F8.
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(180):T0] Dropping frame with presentation time: 129189004649 μs-ticks
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(192):T0] Set current frame to present at: 129189004649 μs-ticks, start time at: 129189694811 μs-ticks
[INFO:../../cast/streaming/receiver.cc(72):T0] Set player processing time: 991017µs
[VERBOSE:../../cast/streaming/receiver.cc(136):T0] [SSRC:51461] AdvanceToNextFrame: No frames ready. Last consumed was F8.
```

So, options are:

1. Packet never sent over network
2. Packet never received
3. Packet is not "ready"


NOTES: also reproes with dummy player, so not an SDL-specific thing. Could be on the sender encoder though?

Sender is WAY ahead.
```
[INFO:../../cast/streaming/sender.cc(108):T0] Enqueueing frame with id: F5
[INFO:../../cast/streaming/sender_packet_router.cc(151):T0] Scheduling packet burst for 129936566415 μs-ticks, first allowable: 129936566415 μs-ticks
[INFO:../../cast/streaming/sender_packet_router.cc(151):T0] Scheduling packet burst for 129936566415 μs-ticks, first allowable: 129936566415 μs-ticks
[INFO:../../cast/streaming/compound_rtcp_parser.cc(91):T0] Parsing RTCP event.
[INFO:../../cast/streaming/compound_rtcp_parser.cc(122):T0] Received receiver report
[INFO:../../cast/streaming/compound_rtcp_parser.cc(151):T0] Received extended report
[INFO:../../cast/streaming/compound_rtcp_parser.cc(130):T0] Received payload specific
[WARNING:../../cast/streaming/sender.cc(301):T0] Invalidating a round-trip time measurement (470474 μs) since it exceeds the current target playout delay (400 ms).
[INFO:../../cast/streaming/sender.cc(352):T0] received a receiver checkpoint for frame id: F-1
[INFO:../../cast/streaming/sender.cc(369):T0] updating latest expected to: F-1
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F18
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F5
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F5
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F5
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F5
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F5
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F5
[INFO:../../cast/streaming/sender_packet_router.cc(172):T0] Sent packets over sender packet router: RTCP 2, RCP: 7
[INFO:../../cast/streaming/sender_packet_router.cc(151):T0] Scheduling packet burst for 129936662961 μs-ticks, first allowable: 129936652961 μs-ticks
[INFO:../../cast/streaming/sender.cc(108):T0] Enqueueing frame with id: F19
[INFO:../../cast/streaming/sender_packet_router.cc(151):T0] Scheduling packet burst for 129936652961 μs-ticks, first allowable: 129936652961 μs-ticks
[INFO:../../cast/streaming/sender.cc(108):T0] Enqueueing frame with id: F20
[INFO:../../cast/streaming/sender_packet_router.cc(151):T0] Scheduling packet burst for 129936652961 μs-ticks, first allowable: 129936652961 μs-ticks
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F19
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F20
[INFO:../../cast/streaming/sender.cc(239):T0] Generating packet from frame F5
```

Sender is now on like 38 though.

```
[INFO:../../cast/streaming/frame_collector.cc(58):T0] Collecting a packet for frame [F18]. Packet count: 1, max packet ID: 0 chunks size: 1 packet ID: 0
[VERBOSE:../../cast/streaming/receiver.cc(447):T0] [SSRC:15133] Advancing checkpoint to F18
[INFO:../../cast/streaming/receiver.cc(357):T0] Frame acks are: (1)
[INFO:../../cast/streaming/receiver.cc(359):T0] "F18"
[VERBOSE:../../cast/streaming/receiver.cc(370):T0] [SSRC:15133] Sent RTCP packet.
[VERBOSE:../../cast/streaming/receiver.cc(102):T0] [SSRC:15133] AdvanceToNextFrame: Next in sequence (F18)
[VERBOSE:../../cast/streaming/receiver.cc(158):T0] [SSRC:15133] ConsumeNextFrame → F18: 3 payload bytes, RTP Timestamp 180000 µs, to play-out -2462148 µs from now.
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(128):T0] Decoded frame_id F18
[VERBOSE:../../cast/streaming/receiver.cc(137):T0] [SSRC:15133] AdvanceToNextFrame: No frames ready. Last consumed was F18.
[INFO:../../cast/streaming/receiver.cc(182):T0] received rtp packet with frame id F4
[ERROR:../../cast/streaming/receiver.cc(193):T0] Frame id is no longer interesting... F4<=F4
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(181):T0] Dropping frame with presentation time: 129936698546 μs-ticks
[INFO:../../cast/standalone_receiver/sdl_player_base.cc(194):T0] Set current frame to present at: 129936698546 μs-ticks, start time at: 129939160687 μs-ticks
[INFO:../../cast/streaming/receiver.cc(72):T0] Set player processing time: 32100µs
[VERBOSE:../../cast/streaming/receiver.cc(137):T0] [SSRC:15133] AdvanceToNextFrame: No frames ready. Last consumed was F18.
[VERBOSE:../../cast/streaming/receiver.cc(326):T0] [SSRC:15133] Received Sender Report: Local clock is ahead of Sender's by -640625 µs (minus one-way network transit time).
[INFO:../../cast/streaming/receiver.cc(357):T0] Frame acks are: (1)
[INFO:../../cast/streaming/receiver.cc(359):T0] "F18"
[VERBOSE:../../cast/streaming/receiver.cc(370):T0] [SSRC:15133] Sent RTCP packet.
[VERBOSE:../../cast/streaming/receiver.cc(326):T0] [SSRC:60394] Received Sender Report: Local clock is ahead of Sender's by -508510 µs (minus one-way network transit time).
[INFO:../../cast/streaming/receiver.cc(357):T0] Frame acks are: (1)
[INFO:../../cast/streaming/receiver.cc(359):T0] "F4"
[VERBOSE:../../cast/streaming/receiver.cc(370):T0] [SSRC:60394] Sent RTCP packet.
[INFO:../../cast/streaming/receiver.cc(182):T0] received rtp packet with frame id F18
[ERROR:../../cast/streaming/receiver.cc(193):T0] Frame id is no longer interesting... F18<=F18
[INFO:../../cast/streaming/receiver.cc(182):T0] received rtp packet with frame id F5
[INFO:../../cast/streaming/receiver.cc(204):T0] Updating max allowed frame id: F124
[VERBOSE:../../cast/streaming/receiver.cc(215):T0] [SSRC:60394] Advanced latest frame expected to F5
```

This looks like the receiver hasn't gotten the packets yet. But the sender
definitely thinks it has sent them.

Packet loss? If I repeat the environment send packet call 10 times,
I get 9 no longer interestings. Not the UDP socket's fault looks like?

```
[WARNING:../../cast/streaming/sender.cc(301):T0] Invalidating a round-trip time measurement (6132654 μs) since it exceeds the current target playout delay (400 ms).
```

Is it a timing problem??

Sender report:

```
[INFO:../../cast/streaming/sender.cc(296):T0] Timing notes: total delay 2470137 μs, arrival time 131373107472 μs-ticks, non network delay 30 μs, measurement: 2470107 μs
```

Delay is terrible, but appears to be due to network according to this?

Upping the playout delay to four seconds:

```
[INFO:../../cast/streaming/sender.cc(296):T0] Timing notes: total delay 8694414 μs, arrival time 131582137751 μs-ticks, non network delay 30 μs, measurement: 8694384 μs
```

```
[INFO:../../cast/streaming/sender.cc(296):T0] Timing notes: total delay 10816805 μs, arrival time 131584443145 μs-ticks, non network delay 15 μs, measurement: 10816790 μs
```

```
[INFO:../../cast/streaming/sender.cc(296):T0] Timing notes: total delay 13608648 μs, arrival time 131587308393 μs-ticks, non network delay 30 μs, measurement: 13608618 μs
```

Total delay seems much worse... and appears to get worse over time. This definitely implies some tomfoolery calculations.

Dummy player doesn't rule out decoder...

Hmmm.... now seeing too many frames in pipeline already??

Receiver has decoded F8 and F26, Sender has sent F20 and and F56 and
enqueued F22 and F129.

So... what's the gap? Looks like all UDP packets do get sent and received--
is it bottled up somewhere?

NOTE: still occurs with dummy players, so not the fault of SDL taking too
long to initialize or anything similar to that.

Steady state is receiver thinks things are fine, sender is dropping packets
because the media duration in flight is too long and/or FrameId span limit
has been reached--meaning too many frames enqueued?

Too many frames enqueued== the current frame ID minus the checkpoint (where the
receiver has ACKed to) is greater than 120. Does that many all of these have been sent?

Sender::EnqueueFrame enqueues a frame (DUH) by checking how many are in flight,
breaking down to a set of packets, then requesting RTP sending.

Sender thinks everything has been sent already--the packet router agrees.

But then continuously drops new packets due to high in flight duration.

What happens after an ACK? A new send? YES

```
12:26:36.272870 [INFO:../../cast/streaming/sender.cc(358):T0] received a receiver checkpoint for frame id: F50
12:26:36.272886 [INFO:../../cast/streaming/sender.cc(375):T0] updating latest expected to: F50
12:26:36.274823 [INFO:../../cast/streaming/sender.cc(108):T0] Enqueueing frame with id: F170
12:26:36.274880 [INFO:../../cast/streaming/sender_packet_router.cc(151):T0] Scheduling packet burst for 180228655761 μs-ticks, first allowable: 180228655761 μs-ticks
12:26:36.274999 [INFO:../../cast/streaming/sender.cc(108):T0] Enqueueing frame with id: F171
12:26:36.275015 [WARNING:../../cast/standalone_sender/streaming_opus_encoder.cc(128):T0] AUDIO[41045] Dropping: FrameId span limit reached.
```

This implies that the acking and sending piece is good.

Receiver checkpoint is at F50, and receiver says last consumed was F63?

So there is definitely a gap, Sender thinks it has sent all the way to F170.

Does the receiver packet router have F170?

How is the clock drift getting calculating?
```
12:26:36.176034 [VERBOSE:../../cast/streaming/receiver.cc(326):T0] [SSRC:41046] Received Sender Report: Local clock is ahead of Sender's by 1817294 µs (minus one-way network transit time).
```

```
12:26:36.432588 [WARNING:../../cast/streaming/sender.cc(307):T0] Invalidating a round-trip time measurement (4505030 μs) since it exceeds the current target playout delay (4000 ms).
```

Audio is the packet with higher count...

Sender: acks F50 and F21

Receiver: acks F63 and F21

So stuff is wonky. Could be UDP socket? I don't think anyone else is using
it. ReadyHandle may be tricky?

Well, if I process ready handles all the freaking time it doesn't change anything.

It is NOT a problem of checking the UDP sockets more frequently--scheduling
checks doesn't improve performance at all.

