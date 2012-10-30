/**
 * FreeRDP: A Remote Desktop Protocol Implementation
 * Terminal Server Gateway (TSG)
 *
 * Copyright 2012 Fujitsu Technology Solutions GmbH
 * Copyright 2012 Dmitrij Jasnov <dmitrij.jasnov@ts.fujitsu.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <freerdp/utils/sleep.h>
#include <freerdp/utils/stream.h>
#include <freerdp/utils/memory.h>
#include <freerdp/utils/hexdump.h>
#include <freerdp/utils/unicode.h>

#include <winpr/crt.h>
#include <winpr/ndr.h>

#include "tsg.h"

/**
 * RPC Functions: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378623/
 * Remote Procedure Call: http://msdn.microsoft.com/en-us/library/windows/desktop/aa378651/
 * RPC NDR Interface Reference: http://msdn.microsoft.com/en-us/library/windows/desktop/hh802752/
 */

BYTE tsg_packet1[108] =
{
	0x43, 0x56, 0x00, 0x00, 0x43, 0x56, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x52, 0x54, 0x43, 0x56,
	0x04, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00,
	0x8A, 0xE3, 0x13, 0x71, 0x02, 0xF4, 0x36, 0x71, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x02, 0x40, 0x28, 0x00, 0xDD, 0x65, 0xE2, 0x44, 0xAF, 0x7D, 0xCD, 0x42, 0x85, 0x60, 0x3C, 0xDB,
	0x6E, 0x7A, 0x27, 0x29, 0x01, 0x00, 0x03, 0x00, 0x04, 0x5D, 0x88, 0x8A, 0xEB, 0x1C, 0xC9, 0x11,
	0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60, 0x02, 0x00, 0x00, 0x00
};

/**
	TsProxyCreateTunnel

	0x43, 0x56, 0x00, 0x00, packetId (TSG_PACKET_TYPE_VERSIONCAPS)

	TSG_PACKET
	0x43, 0x56, 0x00, 0x00, SwitchValue (TSG_PACKET_TYPE_VERSIONCAPS)

	0x00, 0x00, 0x02, 0x00, NdrPtr

	0x52, 0x54, componentId
	0x43, 0x56, packetId

	0x04, 0x00, 0x02, 0x00, NdrPtr TsgCapsPtr
	0x01, 0x00, 0x00, 0x00, numCapabilities

	0x01, 0x00, MajorVersion
	0x01, 0x00, MinorVersion
	0x00, 0x00, QuarantineCapabilities

	0x00, 0x00, alignment pad?

	0x01, 0x00, 0x00, 0x00, MaximumCount
	0x01, 0x00, 0x00, 0x00, TSG_CAPABILITY_TYPE_NAP

	0x01, 0x00, 0x00, 0x00, SwitchValue (TSG_NAP_CAPABILITY_QUAR_SOH)
	0x1F, 0x00, 0x00, 0x00, idle value in minutes?

	0x8A, 0xE3, 0x13, 0x71, 0x02, 0xF4, 0x36, 0x71, 0x01, 0x00, 0x04, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x02, 0x40, 0x28, 0x00, 0xDD, 0x65, 0xE2, 0x44, 0xAF, 0x7D, 0xCD, 0x42, 0x85, 0x60, 0x3C, 0xDB,
	0x6E, 0x7A, 0x27, 0x29, 0x01, 0x00, 0x03, 0x00, 0x04, 0x5D, 0x88, 0x8A, 0xEB, 0x1C, 0xC9, 0x11,
	0x9F, 0xE8, 0x08, 0x00, 0x2B, 0x10, 0x48, 0x60, 0x02, 0x00, 0x00, 0x00
 */

/**
 * TsProxyCreateTunnelResponse
 *
	05 00 02 03 10 00 00 00 50 09 10 00 01 00 00 00
	14 09 00 00 00 00 00 00 00 00 02 00

	50 43 00 00 TSG_PACKET_TYPE_CAPS_RESPONSE
	50 43 00 00 TSG_PACKET_TYPE_CAPS_RESPONSE
	Ptr:		04 00 02 00
	flags:		00 00 00 00
	certChainLen:	39 04 00 00 (1081 * 2 = 2162)
	certChainDataPtr: 08 00 02 00

	GUID?
	95 87 a1 e6 51 6e 6e 4f 90 66 49 4f 9b c2 40 ff
	0c 00 02 00 01 00 00 00 01 00 00 00 00 00 00 00
	01 00 00 00

	Ptr:		14 00 02 00
	MaxCount:	39 04 00 00
	Offset:		00 00 00 00
	ActualCount:	39 04 00 00

	end offset = 105 + 2162 = 2267 (0x08DB)

	                             2d 00 2d 00 2d 00 2d 00 ....9...-.-.-.-.
	0070 2d 00 42 00 45 00 47 00 49 00 4e 00 20 00 43 00 -.B.E.G.I.N. .C.
	0080 45 00 52 00 54 00 49 00 46 00 49 00 43 00 41 00 E.R.T.I.F.I.C.A.
	0090 54 00 45 00 2d 00 2d 00 2d 00 2d 00 2d 00 0d 00 T.E.-.-.-.-.-...
	00a0 0a 00 4d 00 49 00 49 00 43 00 34 00 6a 00 43 00 ..M.I.I.C.4.j.C.
	00b0 43 00 41 00 63 00 71 00 67 00 41 00 77 00 49 00 C.A.c.q.g.A.w.I.
	00c0 42 00 41 00 67 00 49 00 51 00 4a 00 38 00 4f 00 B.A.g.I.Q.J.8.O.
	00d0 48 00 6a 00 65 00 66 00 37 00 4e 00 34 00 78 00 H.j.e.f.7.N.4.x.
	00e0 43 00 39 00 30 00 36 00 49 00 53 00 33 00 34 00 C.9.0.6.I.S.3.4.
	00f0 75 00 76 00 54 00 41 00 4e 00 42 00 67 00 6b 00 u.v.T.A.N.B.g.k.
	0100 71 00 68 00 6b 00 69 00 47 00 39 00 77 00 30 00 q.h.k.i.G.9.w.0.
	0110 42 00 41 00 51 00 55 00 46 00 41 00 44 00 41 00 B.A.Q.U.F.A.D.A.
	0120 61 00 0d 00 0a 00 4d 00 52 00 67 00 77 00 46 00 a.....M.R.g.w.F.
	0130 67 00 59 00 44 00 56 00 51 00 51 00 44 00 45 00 g.Y.D.V.Q.Q.D.E.
	0140 77 00 39 00 58 00 53 00 55 00 34 00 74 00 4e 00 w.9.X.S.U.4.t.N.
	0150 7a 00 56 00 46 00 52 00 45 00 4e 00 51 00 52 00 z.V.F.R.E.N.Q.R.
	0160 30 00 64 00 4e 00 52 00 7a 00 6b 00 77 00 48 00 0.d.N.R.z.k.w.H.
	0170 68 00 63 00 4e 00 4d 00 54 00 49 00 77 00 4d 00 h.c.N.M.T.I.w.M.
	0180 7a 00 49 00 34 00 4d 00 44 00 4d 00 77 00 4d 00 z.I.4.M.D.M.w.M.
	0190 54 00 45 00 32 00 57 00 68 00 63 00 4e 00 4d 00 T.E.2.W.h.c.N.M.
	01a0 6a 00 49 00 77 00 0d 00 0a 00 4d 00 7a 00 49 00 j.I.w.....M.z.I.
	01b0 33 00 4d 00 44 00 41 00 77 00 4d 00 44 00 41 00 3.M.D.A.w.M.D.A.
	01c0 77 00 57 00 6a 00 41 00 61 00 4d 00 52 00 67 00 w.W.j.A.a.M.R.g.
	01d0 77 00 46 00 67 00 59 00 44 00 56 00 51 00 51 00 w.F.g.Y.D.V.Q.Q.
	01e0 44 00 45 00 77 00 39 00 58 00 53 00 55 00 34 00 D.E.w.9.X.S.U.4.
	01f0 74 00 4e 00 7a 00 56 00 46 00 52 00 45 00 4e 00 t.N.z.V.F.R.E.N.
	0200 51 00 52 00 30 00 64 00 4e 00 52 00 7a 00 6b 00 Q.R.0.d.N.R.z.k.
	0210 77 00 67 00 67 00 45 00 69 00 4d 00 41 00 30 00 w.g.g.E.i.M.A.0.
	0220 47 00 43 00 53 00 71 00 47 00 0d 00 0a 00 53 00 G.C.S.q.G.....S.
	0230 49 00 62 00 33 00 44 00 51 00 45 00 42 00 41 00 I.b.3.D.Q.E.B.A.
	0240 51 00 55 00 41 00 41 00 34 00 49 00 42 00 44 00 Q.U.A.A.4.I.B.D.
	0250 77 00 41 00 77 00 67 00 67 00 45 00 4b 00 41 00 w.A.w.g.g.E.K.A.
	0260 6f 00 49 00 42 00 41 00 51 00 43 00 6e 00 78 00 o.I.B.A.Q.C.n.x.
	0270 55 00 63 00 32 00 68 00 2b 00 4c 00 5a 00 64 00 U.c.2.h.+.L.Z.d.
	0280 42 00 65 00 56 00 61 00 79 00 35 00 52 00 36 00 B.e.V.a.y.5.R.6.
	0290 68 00 70 00 36 00 47 00 31 00 2b 00 75 00 2f 00 h.p.6.G.1.+.u./.
	02a0 59 00 33 00 7a 00 63 00 33 00 33 00 47 00 0d 00 Y.3.z.c.3.3.G...
	02b0 0a 00 66 00 4c 00 62 00 4f 00 53 00 62 00 48 00 ..f.L.b.O.S.b.H.
	02c0 71 00 6d 00 4e 00 31 00 42 00 4e 00 57 00 79 00 q.m.N.1.B.N.W.y.
	02d0 44 00 7a 00 51 00 66 00 5a 00 71 00 30 00 54 00 D.z.Q.f.Z.q.0.T.
	02e0 35 00 30 00 4b 00 70 00 54 00 61 00 49 00 71 00 5.0.K.p.T.a.I.q.
	02f0 65 00 33 00 58 00 65 00 51 00 4f 00 45 00 63 00 e.3.X.e.Q.O.E.c.
	0300 42 00 33 00 4b 00 78 00 56 00 6a 00 78 00 46 00 B.3.K.x.V.j.x.F.
	0310 46 00 75 00 47 00 67 00 6d 00 57 00 4d 00 55 00 F.u.G.g.m.W.M.U.
	0320 6d 00 7a 00 37 00 79 00 77 00 49 00 49 00 75 00 m.z.7.y.w.I.I.u.
	0330 38 00 0d 00 0a 00 33 00 52 00 56 00 44 00 39 00 8.....3.R.V.D.9.
	0340 36 00 73 00 42 00 30 00 6b 00 31 00 6a 00 37 00 6.s.B.0.k.1.j.7.
	0350 70 00 30 00 4d 00 54 00 6f 00 4a 00 6a 00 71 00 p.0.M.T.o.J.j.q.
	0360 4a 00 45 00 78 00 51 00 56 00 6d 00 48 00 44 00 J.E.x.Q.V.m.H.D.
	0370 72 00 56 00 46 00 2f 00 63 00 4f 00 77 00 6a 00 r.V.F./.c.O.w.j.
	0380 35 00 59 00 69 00 35 00 42 00 33 00 47 00 57 00 5.Y.i.5.B.3.G.W.
	0390 38 00 65 00 65 00 37 00 5a 00 45 00 52 00 30 00 8.e.e.7.Z.E.R.0.
	03a0 76 00 63 00 62 00 4f 00 34 00 59 00 70 00 4c 00 v.c.b.O.4.Y.p.L.
	03b0 64 00 58 00 41 00 0d 00 0a 00 65 00 6f 00 6f 00 d.X.A.....e.o.o.
	03c0 62 00 48 00 6f 00 37 00 7a 00 73 00 65 00 59 00 b.H.o.7.z.s.e.Y.
	03d0 57 00 31 00 37 00 72 00 54 00 4b 00 79 00 73 00 W.1.7.r.T.K.y.s.
	03e0 65 00 59 00 52 00 69 00 6a 00 32 00 6a 00 76 00 e.Y.R.i.j.2.j.v.
	03f0 63 00 6e 00 75 00 6f 00 52 00 72 00 4a 00 48 00 c.n.u.o.R.r.J.H.
	0400 58 00 78 00 36 00 41 00 44 00 64 00 6f 00 57 00 X.x.6.A.D.d.o.W.
	0410 37 00 58 00 4e 00 69 00 39 00 59 00 75 00 55 00 7.X.N.i.9.Y.u.U.
	0420 4a 00 46 00 35 00 6b 00 51 00 46 00 34 00 64 00 J.F.5.k.Q.F.4.d.
	0430 6b 00 73 00 6c 00 5a 00 72 00 0d 00 0a 00 49 00 k.s.l.Z.r.....I.
	0440 44 00 50 00 50 00 6b 00 30 00 68 00 44 00 78 00 D.P.P.k.0.h.D.x.
	0450 6d 00 61 00 49 00 6c 00 5a 00 6a 00 47 00 6a 00 m.a.I.l.Z.j.G.j.
	0460 70 00 55 00 65 00 69 00 47 00 50 00 2b 00 57 00 p.U.e.i.G.P.+.W.
	0470 46 00 68 00 72 00 4d 00 6d 00 6f 00 6b 00 6f 00 F.h.r.M.m.o.k.o.
	0480 46 00 78 00 7a 00 2f 00 70 00 7a 00 61 00 38 00 F.x.z./.p.z.a.8.
	0490 5a 00 4c 00 50 00 4f 00 4a 00 64 00 51 00 76 00 Z.L.P.O.J.d.Q.v.
	04a0 6c 00 31 00 52 00 78 00 34 00 61 00 6e 00 64 00 l.1.R.x.4.a.n.d.
	04b0 43 00 38 00 4d 00 79 00 59 00 47 00 2b 00 0d 00 C.8.M.y.Y.G.+...
	04c0 0a 00 50 00 57 00 62 00 74 00 62 00 44 00 43 00 ..P.W.b.t.b.D.C.
	04d0 31 00 33 00 41 00 71 00 2f 00 44 00 4d 00 4c 00 1.3.A.q./.D.M.L.
	04e0 49 00 56 00 6b 00 6c 00 41 00 65 00 4e 00 6f 00 I.V.k.l.A.e.N.o.
	04f0 78 00 32 00 43 00 61 00 4a 00 65 00 67 00 30 00 x.2.C.a.J.e.g.0.
	0500 56 00 2b 00 48 00 6d 00 46 00 6b 00 70 00 59 00 V.+.H.m.F.k.p.Y.
	0510 68 00 75 00 34 00 6f 00 33 00 6b 00 38 00 6e 00 h.u.4.o.3.k.8.n.
	0520 58 00 5a 00 37 00 7a 00 35 00 41 00 67 00 4d 00 X.Z.7.z.5.A.g.M.
	0530 42 00 41 00 41 00 47 00 6a 00 4a 00 44 00 41 00 B.A.A.G.j.J.D.A.
	0540 69 00 0d 00 0a 00 4d 00 41 00 73 00 47 00 41 00 i.....M.A.s.G.A.
	0550 31 00 55 00 64 00 44 00 77 00 51 00 45 00 41 00 1.U.d.D.w.Q.E.A.
	0560 77 00 49 00 45 00 4d 00 44 00 41 00 54 00 42 00 w.I.E.M.D.A.T.B.
	0570 67 00 4e 00 56 00 48 00 53 00 55 00 45 00 44 00 g.N.V.H.S.U.E.D.
	0580 44 00 41 00 4b 00 42 00 67 00 67 00 72 00 42 00 D.A.K.B.g.g.r.B.
	0590 67 00 45 00 46 00 42 00 51 00 63 00 44 00 41 00 g.E.F.B.Q.c.D.A.
	05a0 54 00 41 00 4e 00 42 00 67 00 6b 00 71 00 68 00 T.A.N.B.g.k.q.h.
	05b0 6b 00 69 00 47 00 39 00 77 00 30 00 42 00 41 00 k.i.G.9.w.0.B.A.
	05c0 51 00 55 00 46 00 0d 00 0a 00 41 00 41 00 4f 00 Q.U.F.....A.A.O.
	05d0 43 00 41 00 51 00 45 00 41 00 52 00 33 00 74 00 C.A.Q.E.A.R.3.t.
	05e0 67 00 2f 00 6e 00 41 00 69 00 73 00 41 00 46 00 g./.n.A.i.s.A.F.
	05f0 42 00 50 00 66 00 5a 00 42 00 68 00 5a 00 31 00 B.P.f.Z.B.h.Z.1.
	0600 71 00 55 00 53 00 74 00 55 00 52 00 32 00 5a 00 q.U.S.t.U.R.2.Z.
	0610 32 00 5a 00 6a 00 55 00 49 00 42 00 70 00 64 00 2.Z.j.U.I.B.p.d.
	0620 68 00 5a 00 4e 00 32 00 64 00 50 00 6b 00 6f 00 h.Z.N.2.d.P.k.o.
	0630 4d 00 6c 00 32 00 4b 00 4f 00 6b 00 66 00 4d 00 M.l.2.K.O.k.f.M.
	0640 4e 00 6d 00 45 00 44 00 45 00 0d 00 0a 00 68 00 N.m.E.D.E.....h.
	0650 72 00 6e 00 56 00 74 00 71 00 54 00 79 00 65 00 r.n.V.t.q.T.y.e.
	0660 50 00 32 00 4e 00 52 00 71 00 78 00 67 00 48 00 P.2.N.R.q.x.g.H.
	0670 46 00 2b 00 48 00 2f 00 6e 00 4f 00 78 00 37 00 F.+.H./.n.O.x.7.
	0680 78 00 6d 00 66 00 49 00 72 00 4c 00 31 00 77 00 x.m.f.I.r.L.1.w.
	0690 45 00 6a 00 63 00 37 00 41 00 50 00 6c 00 37 00 E.j.c.7.A.P.l.7.
	06a0 4b 00 61 00 39 00 4b 00 6a 00 63 00 65 00 6f 00 K.a.9.K.j.c.e.o.
	06b0 4f 00 4f 00 6f 00 68 00 31 00 32 00 6c 00 2f 00 O.O.o.h.1.2.l./.
	06c0 39 00 48 00 53 00 70 00 38 00 30 00 37 00 0d 00 9.H.S.p.8.0.7...
	06d0 0a 00 4f 00 57 00 4d 00 69 00 48 00 41 00 4a 00 ..O.W.M.i.H.A.J.
	06e0 6e 00 66 00 47 00 32 00 46 00 46 00 37 00 6e 00 n.f.G.2.F.F.7.n.
	06f0 51 00 61 00 74 00 63 00 35 00 4e 00 53 00 42 00 Q.a.t.c.5.N.S.B.
	0700 38 00 4e 00 75 00 4f 00 5a 00 67 00 64 00 62 00 8.N.u.O.Z.g.d.b.
	0710 67 00 51 00 2b 00 43 00 42 00 62 00 39 00 76 00 g.Q.+.C.B.b.9.v.
	0720 2b 00 4d 00 56 00 55 00 33 00 43 00 67 00 39 00 +.M.V.U.3.C.g.9.
	0730 4c 00 57 00 65 00 54 00 53 00 2b 00 51 00 78 00 L.W.e.T.S.+.Q.x.
	0740 79 00 59 00 6c 00 37 00 4f 00 30 00 4c 00 43 00 y.Y.l.7.O.0.L.C.
	0750 4d 00 0d 00 0a 00 76 00 45 00 37 00 77 00 4a 00 M.....v.E.7.w.J.
	0760 58 00 53 00 70 00 70 00 4a 00 4c 00 42 00 6b 00 X.S.p.p.J.L.B.k.
	0770 69 00 52 00 72 00 63 00 73 00 51 00 6e 00 34 00 i.R.r.c.s.Q.n.4.
	0780 61 00 39 00 62 00 64 00 67 00 56 00 67 00 6b 00 a.9.b.d.g.V.g.k.
	0790 57 00 32 00 78 00 70 00 7a 00 56 00 4e 00 6d 00 W.2.x.p.z.V.N.m.
	07a0 6e 00 62 00 42 00 35 00 73 00 49 00 49 00 38 00 n.b.B.5.s.I.I.8.
	07b0 35 00 75 00 7a 00 37 00 78 00 76 00 46 00 47 00 5.u.z.7.x.v.F.G.
	07c0 50 00 47 00 65 00 70 00 4c 00 55 00 55 00 55 00 P.G.e.p.L.U.U.U.
	07d0 76 00 66 00 66 00 0d 00 0a 00 36 00 76 00 68 00 v.f.f.....6.v.h.
	07e0 56 00 46 00 5a 00 76 00 62 00 53 00 47 00 73 00 V.F.Z.v.b.S.G.s.
	07f0 77 00 6f 00 72 00 32 00 5a 00 6c 00 54 00 6c 00 w.o.r.2.Z.l.T.l.
	0800 79 00 57 00 70 00 79 00 67 00 5a 00 67 00 71 00 y.W.p.y.g.Z.g.q.
	0810 49 00 46 00 56 00 6f 00 73 00 43 00 4f 00 33 00 I.F.V.o.s.C.O.3.
	0820 34 00 39 00 53 00 65 00 2b 00 55 00 45 00 72 00 4.9.S.e.+.U.E.r.
	0830 72 00 54 00 48 00 46 00 7a 00 71 00 38 00 63 00 r.T.H.F.z.q.8.c.
	0840 76 00 7a 00 4b 00 76 00 63 00 6b 00 4f 00 75 00 v.z.K.v.c.k.O.u.
	0850 4f 00 6b 00 42 00 72 00 42 00 0d 00 0a 00 6c 00 O.k.B.r.B.....l.
	0860 69 00 79 00 4e 00 32 00 47 00 42 00 41 00 6f 00 i.y.N.2.G.B.A.o.
	0870 50 00 45 00 67 00 79 00 4d 00 48 00 41 00 35 00 P.E.g.y.M.H.A.5.
	0880 53 00 78 00 39 00 5a 00 39 00 37 00 35 00 75 00 S.x.9.Z.9.7.5.u.
	0890 6e 00 7a 00 50 00 74 00 77 00 3d 00 3d 00 0d 00 n.z.P.t.w.=.=...
	08a0 0a 00 2d 00 2d 00 2d 00 2d 00 2d 00 45 00 4e 00 ..-.-.-.-.-.E.N.
	08b0 44 00 20 00 43 00 45 00 52 00 54 00 49 00 46 00 D. .C.E.R.T.I.F.
	08c0 49 00 43 00 41 00 54 00 45 00 2d 00 2d 00 2d 00 I.C.A.T.E.-.-.-.
	08d0 2d 00 2d 00 0d 00 0a 00 00 00

	00 00 alignment pad?

	52 54 componentId
	43 56 packetId (TSG_PACKET_TYPE_VERSIONCAPS)
	TsgCapsPtr:		10 00 02 00
	NumCapabilities:	01 00 00 00
	MajorVersion:		01 00
	MinorVersion:		01 00
	quarantineCapabilities:	01 00
	pad:			00 00
	NumCapabilitiesConf:
	MaxCount: 01 00 00 00

		tsgCaps:	01 00 00 00 (TSG_CAPABILITY_TYPE_NAP)
		SwitchValue:	01 00 00 00 (TSG_CAPABILITY_TYPE_NAP)
			capabilities: 1f 00 00 00 =
				TSG_NAP_CAPABILITY_QUAR_SOH |
				TSG_NAP_CAPABILITY_IDLE_TIMEOUT |
				TSG_MESSAGING_CAP_CONSENT_SIGN |
				TSG_MESSAGING_CAP_SERVICE_MSG |
				TSG_MESSAGING_CAP_REAUTH

	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ??

	TunnelContext:
		ContextType: 00 00 00 00
		ContextUuid: 81 1d 32 9f 3f ff 8d 41 ae 54 ba e4 7b b7 ef 43

	30 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 ??
	00 00 00 00 0a 05 0c 00 00 00 00 00 01 00 00 00
	9d 13 89 41

	UINT32 TunnelId:	2e 85 76 3f
	HRESULT ReturnValue:	00 00 00 00
 */

BYTE tsg_packet2[112] =
{
	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x52, 0x51, 0x00, 0x00, 0x52, 0x51, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00, 0x15, 0x00, 0x00, 0x00, 0x08, 0x00, 0x02, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00,
	0x61, 0x00, 0x62, 0x00, 0x63, 0x00, 0x2D, 0x00, 0x4E, 0x00, 0x48, 0x00, 0x35, 0x00, 0x37, 0x00,
	0x30, 0x00, 0x2E, 0x00, 0x43, 0x00, 0x53, 0x00, 0x4F, 0x00, 0x44, 0x00, 0x2E, 0x00, 0x6C, 0x00,
	0x6F, 0x00, 0x63, 0x00, 0x61, 0x00, 0x6C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

/**
	TsProxyAuthorizeTunnel

	TunnelContext:
		ContextType:	0x00, 0x00, 0x00, 0x00,
		ContextUuid:	0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44,
				0x8D, 0x99, 0x29, 0x30, 0x53, 0x6C, 0x04, 0x33,

	TsgPacket:
		PacketId:	0x52, 0x51, 0x00, 0x00,
		SwitchValue:	0x52, 0x51, 0x00, 0x00,

		PacketQuarRequestPtr: 0x00, 0x00, 0x02, 0x00,
		PacketQuarRequest:
			Flags:	0x00, 0x00, 0x00, 0x00,
			MachineNamePtr: 0x04, 0x00, 0x02, 0x00,
			NameLength:	0x15, 0x00, 0x00, 0x00,
			DataPtr:	0x08, 0x00, 0x02, 0x00,
			DataLen:	0x00, 0x00, 0x00, 0x00,
			MachineName:
				MaxCount:	0x15, 0x00, 0x00, 0x00, (21 elements)
				Offset:		0x00, 0x00, 0x00, 0x00,
				ActualCount:	0x15, 0x00, 0x00, 0x00, (21 elements)
				Array:		0x61, 0x00, 0x62, 0x00, 0x63, 0x00, 0x2D, 0x00,
						0x4E, 0x00, 0x48, 0x00, 0x35, 0x00, 0x37, 0x00,
						0x30, 0x00, 0x2E, 0x00, 0x43, 0x00, 0x53, 0x00,
						0x4F, 0x00, 0x44, 0x00, 0x2E, 0x00, 0x6C, 0x00,
						0x6F, 0x00, 0x63, 0x00, 0x61, 0x00, 0x6C, 0x00,
						0x00, 0x00,

			DataLenConf:
				MaxCount: 0x00, 0x00, 0x00, 0x00,
						0x00, 0x00
 */

BYTE tsg_packet3[40] =
{
	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x01, 0x00, 0x00, 0x00, 0x52, 0x47, 0x00, 0x00, 0x52, 0x47, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00
};

/**
	TsProxyMakeTunnelCall

	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x01, 0x00, 0x00, 0x00,

	0x52, 0x47, 0x00, 0x00,
	0x52, 0x47, 0x00, 0x00,
	0x00, 0x00, 0x02, 0x00,
	0x01, 0x00, 0x00, 0x00
 */

BYTE tsg_packet4[48] =
{
	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00
};

/**
	TsProxyCreateChannel

	0x00, 0x00, 0x00, 0x00, 0x6A, 0x78, 0xE9, 0xAB, 0x02, 0x90, 0x1C, 0x44, 0x8D, 0x99, 0x29, 0x30,
	0x53, 0x6C, 0x04, 0x33, 0x00, 0x00, 0x02, 0x00,

	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x04, 0x00, 0x02, 0x00
 */

BYTE tsg_packet5[20] =
{
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
};

DWORD TsProxySendToServer(handle_t IDL_handle, byte pRpcMessage[], UINT32 count, UINT32* lengths)
{
	STREAM* s;
	int status;
	int length;
	rdpTsg* tsg;
	byte* buffer1 = NULL ;
	byte* buffer2 = NULL ;
	byte* buffer3 = NULL ;
	UINT32 buffer1Length;
	UINT32 buffer2Length;
	UINT32 buffer3Length;
	UINT32 numBuffers = 0;
	UINT32 totalDataBytes = 0;

	tsg = (rdpTsg*) IDL_handle;
	buffer1Length = buffer2Length = buffer3Length = 0;

	if (count > 0)
	{
		numBuffers++;
		buffer1 = &pRpcMessage[0];
		buffer1Length = lengths[0];
		totalDataBytes += lengths[0] + 4;
	}

	if (count > 1)
	{
		numBuffers++;
		buffer2 = &pRpcMessage[1];
		buffer2Length = lengths[1];
		totalDataBytes += lengths[1] + 4;
	}

	if (count > 2)
	{
		numBuffers++;
		buffer3 = &pRpcMessage[2];
		buffer3Length = lengths[2];
		totalDataBytes += lengths[2] + 4;
	}

	s = stream_new(28 + totalDataBytes);

	/* PCHANNEL_CONTEXT_HANDLE_NOSERIALIZE_NR (20 bytes) */
	stream_write_UINT32(s, 0); /* ContextType (4 bytes) */
	stream_write(s, tsg->ChannelContext, 16); /* ContextUuid (16 bytes) */

	stream_write_UINT32_be(s, totalDataBytes); /* totalDataBytes (4 bytes) */
	stream_write_UINT32_be(s, numBuffers); /* numBuffers (4 bytes) */

	if (buffer1Length > 0)
		stream_write_UINT32_be(s, buffer1Length); /* buffer1Length (4 bytes) */
	if (buffer2Length > 0)
		stream_write_UINT32_be(s, buffer2Length); /* buffer2Length (4 bytes) */
	if (buffer3Length > 0)
		stream_write_UINT32_be(s, buffer3Length); /* buffer3Length (4 bytes) */

	if (buffer1Length > 0)
		stream_write(s, buffer1, buffer1Length); /* buffer1 (variable) */
	if (buffer2Length > 0)
		stream_write(s, buffer2, buffer2Length); /* buffer2 (variable) */
	if (buffer3Length > 0)
		stream_write(s, buffer3, buffer3Length); /* buffer3 (variable) */

	stream_seal(s);

	length = s->size;
	status = rpc_tsg_write(tsg->rpc, s->data, s->size, 9);

	stream_free(s);

	if (status <= 0)
	{
		printf("rpc_tsg_write failed!\n");
		return -1;
	}

	return length;
}

BOOL tsg_proxy_create_tunnel(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	/**
	 * OpNum = 1
	 *
	 * HRESULT TsProxyCreateTunnel(
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse,
	 * [out] PTUNNEL_CONTEXT_HANDLE_SERIALIZE* tunnelContext,
	 * [out] unsigned long* tunnelId
	 * );
	 */

	DEBUG_TSG("TsProxyCreateTunnel");

	length = sizeof(tsg_packet1);
	buffer = (BYTE*) malloc(length);
	CopyMemory(buffer, tsg_packet1, length);

	status = rpc_tsg_write(rpc, buffer, length, TsProxyCreateTunnelOpnum);

	if (status <= 0)
	{
		printf("TsProxyCreateTunnel write failure\n");
		return FALSE;
	}

	free(buffer);

	status = rpc_recv_pdu(rpc);

	if (status <= 0)
	{
		printf("TsProxyCreateTunnel read failure\n");
		return FALSE;
	}

	CopyMemory(tsg->TunnelContext, rpc->buffer + (status - 48), 16);

#ifdef WITH_DEBUG_TSG
	printf("TSG TunnelContext:\n");
	freerdp_hexdump(tsg->TunnelContext, 16);
	printf("\n");
#endif

	return TRUE;
}

BOOL tsg_proxy_authorize_tunnel(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	/**
	 * OpNum = 2
	 *
	 * HRESULT TsProxyAuthorizeTunnel(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse
	 * );
	 *
	 */

	DEBUG_TSG("TsProxyAuthorizeTunnel");

	length = sizeof(tsg_packet2);
	buffer = (BYTE*) malloc(length);
	CopyMemory(buffer, tsg_packet2, length);
	CopyMemory(&buffer[4], tsg->TunnelContext, 16);

	status = rpc_tsg_write(rpc, buffer, length, TsProxyAuthorizeTunnelOpnum);

	if (status <= 0)
	{
		printf("TsProxyAuthorizeTunnel write failure\n");
		return FALSE;
	}

	free(buffer);

	status = rpc_recv_pdu(rpc);

	if (status <= 0)
	{
		printf("TsProxyAuthorizeTunnel read failure\n");
		return FALSE;
	}

	return TRUE;
}

BOOL tsg_proxy_make_tunnel_call(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	/**
	 * OpNum = 3
	 *
	 * HRESULT TsProxyMakeTunnelCall(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in] unsigned long procId,
	 * [in, ref] PTSG_PACKET tsgPacket,
	 * [out, ref] PTSG_PACKET* tsgPacketResponse
	 * );
	 */

	DEBUG_TSG("TsProxyMakeTunnelCall");

	length = sizeof(tsg_packet3);
	buffer = (BYTE*) malloc(length);
	CopyMemory(buffer, tsg_packet3, length);
	CopyMemory(&buffer[4], tsg->TunnelContext, 16);

	status = rpc_tsg_write(rpc, buffer, length, TsProxyMakeTunnelCallOpnum);

	if (status <= 0)
	{
		printf("TsProxyMakeTunnelCall write failure\n");
		return FALSE;
	}

	/* read? */

	free(buffer);

	return TRUE;
}

BOOL tsg_proxy_create_channel(rdpTsg* tsg)
{
	int status;
	UINT32 count;
	BYTE* buffer;
	UINT32 length;
	UINT32 offset;
	rdpRpc* rpc = tsg->rpc;

	/**
	 * OpNum = 4
	 *
	 * HRESULT TsProxyCreateChannel(
	 * [in] PTUNNEL_CONTEXT_HANDLE_NOSERIALIZE tunnelContext,
	 * [in, ref] PTSENDPOINTINFO tsEndPointInfo,
	 * [out] PCHANNEL_CONTEXT_HANDLE_SERIALIZE* channelContext,
	 * [out] unsigned long* channelId
	 * );
	 */

	DEBUG_TSG("TsProxyCreateChannel");

	offset = 0;
	count = _wcslen(tsg->hostname) + 1;

	length = 48 + 12 + (count * 2);
	buffer = (BYTE*) malloc(length);
	CopyMemory(buffer, tsg_packet4, 48);
	CopyMemory(&buffer[4], tsg->TunnelContext, 16);
	CopyMemory(&buffer[38], &tsg->port, 2);

	CopyMemory(&buffer[48], &count, 4); /* MaximumCount */
	CopyMemory(&buffer[52], &offset, 4); /* Offset */
	CopyMemory(&buffer[56], &count, 4); /* ActualCount */
	CopyMemory(&buffer[60], &tsg->hostname, count);

	status = rpc_tsg_write(rpc, buffer, length, TsProxyCreateChannelOpnum);

	if (status <= 0)
	{
		printf("TsProxyCreateChannel write failure\n");
		return FALSE;
	}

	free(buffer);

	status = rpc_recv_pdu(rpc);

	if (status < 0)
	{
		printf("TsProxyCreateChannel read failure\n");
		return FALSE;
	}

	CopyMemory(tsg->ChannelContext, &rpc->buffer[4], 16);

#ifdef WITH_DEBUG_TSG
	printf("TSG ChannelContext:\n");
	freerdp_hexdump(tsg->ChannelContext, 16);
	printf("\n");
#endif

	return TRUE;
}

BOOL tsg_proxy_setup_receive_pipe(rdpTsg* tsg)
{
	int status;
	BYTE* buffer;
	UINT32 length;
	rdpRpc* rpc = tsg->rpc;

	/**
	 * OpNum = 8
	 *
	 * DWORD TsProxySetupReceivePipe(
	 * [in, max_is(32767)] byte pRpcMessage[]
	 * );
	 */

	DEBUG_TSG("TsProxySetupReceivePipe");

	length = sizeof(tsg_packet5);
	buffer = (BYTE*) malloc(length);
	CopyMemory(buffer, tsg_packet5, length);
	CopyMemory(&buffer[4], tsg->ChannelContext, 16);

	status = rpc_tsg_write(rpc, buffer, length, TsProxySetupReceivePipeOpnum);

	if (status <= 0)
	{
		printf("TsProxySetupReceivePipe write failure\n");
		return FALSE;
	}

	free(buffer);

	/* read? */

	return TRUE;
}

BOOL tsg_connect(rdpTsg* tsg, const char* hostname, UINT16 port)
{
	rdpRpc* rpc = tsg->rpc;

	tsg->port = port;
	freerdp_AsciiToUnicodeAlloc(hostname, &tsg->hostname, 0);

	if (!rpc_connect(rpc))
	{
		printf("rpc_connect failed!\n");
		return FALSE;
	}

	DEBUG_TSG("rpc_connect success");

	tsg_proxy_create_tunnel(tsg);

	tsg_proxy_authorize_tunnel(tsg);

	tsg_proxy_make_tunnel_call(tsg);

	tsg_proxy_create_channel(tsg);

	tsg_proxy_setup_receive_pipe(tsg);

	return TRUE;
}

int tsg_read(rdpTsg* tsg, BYTE* data, UINT32 length)
{
	int status;
	int copyLength;
	RPC_PDU_HEADER* header;
	rdpRpc* rpc = tsg->rpc;

	printf("tsg_read: %d, pending: %d\n", length, tsg->pendingPdu);

	if (tsg->pendingPdu)
	{
		header = (RPC_PDU_HEADER*) rpc->buffer;

		copyLength = (tsg->bytesAvailable > length) ? length : tsg->bytesAvailable;

		CopyMemory(data, &rpc->buffer[tsg->bytesRead], copyLength);
		tsg->bytesAvailable -= copyLength;
		tsg->bytesRead += copyLength;

		if (tsg->bytesAvailable < 1)
			tsg->pendingPdu = FALSE;

		return copyLength;
	}
	else
	{
		status = rpc_recv_pdu(rpc);
		tsg->pendingPdu = TRUE;

		header = (RPC_PDU_HEADER*) rpc->buffer;
		tsg->bytesAvailable = header->frag_length;
		tsg->bytesRead = 0;

		copyLength = (tsg->bytesAvailable > length) ? length : tsg->bytesAvailable;

		CopyMemory(data, &rpc->buffer[tsg->bytesRead], copyLength);
		tsg->bytesAvailable -= copyLength;
		tsg->bytesRead += copyLength;

		return copyLength;
	}
}

int tsg_write(rdpTsg* tsg, BYTE* data, UINT32 length)
{
	return TsProxySendToServer((handle_t) tsg, data, 1, &length);
}

rdpTsg* tsg_new(rdpTransport* transport)
{
	rdpTsg* tsg;

	tsg = xnew(rdpTsg);

	if (tsg != NULL)
	{
		tsg->transport = transport;
		tsg->settings = transport->settings;
		tsg->rpc = rpc_new(tsg->transport);
		tsg->pendingPdu = FALSE;
	}

	return tsg;
}

void tsg_free(rdpTsg* tsg)
{
	if (tsg != NULL)
	{
		rpc_free(tsg->rpc);
		free(tsg);
	}
}
