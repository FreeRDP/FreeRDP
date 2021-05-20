--[[
  RDP UDP transport dissector for wireshark
 
  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at
 
      http://www.apache.org/licenses/LICENSE-2.0
 
  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
  
  Copyright 2021 David Fort <contact@hardening-consulting.com>
--]]

local sslDissector = Dissector.get ("tls")
local dtlsDissector = Dissector.get ("dtls")

local dprint = function(...)
	print(table.concat({"Lua:", ...}," "))
end

local dprint2 = dprint

dprint2("loading RDP-UDP with wireshark=", get_version())

local rdpudp = Proto("rdpudp", "UDP transport for RDP")

-- UDP1 fields
local pf_udp_snSourceAck = ProtoField.uint32("rdpudp.snsourceack", "snSourceAck", base.HEX)
local pf_udp_ReceiveWindowSize = ProtoField.uint16("rdpudp.receivewindowsize", "ReceiveWindowSize", base.DEC)
local pf_udp_flags = ProtoField.uint16("rdpudp.flags", "Flags", base.HEX)

RDPUDP_SYN = 0x0001
RDPUDP_FIN = 0x0002
RDPUDP_ACK = 0x0004
RDPUDP_DATA = 0x0008
RDPUDP_FEC = 0x0010
RDPUDP_CN = 0x0020
RDPUDP_CWR = 0x0040
RDPUDP_AOA = 0x0100
RDPUDP_SYNLOSSY = 0x0200
RDPUDP_ACKDELAYED = 0x0400
RDPUDP_CORRELATIONID = 0x0800
RDPUDP_SYNEX = 0x1000

local pf_udp_flag_syn = ProtoField.bool("rdpudp.flags.syn", "Syn", base.HEX, nil, RDPUDP_SYN)
local pf_udp_flag_fin = ProtoField.bool("rdpudp.flags.fin", "Fin", base.HEX, nil, RDPUDP_FIN)
local pf_udp_flag_ack = ProtoField.bool("rdpudp.flags.ack", "Ack", base.HEX, nil, RDPUDP_ACK)
local pf_udp_flag_data = ProtoField.bool("rdpudp.flags.data", "Data", base.HEX, nil, RDPUDP_DATA)
local pf_udp_flag_fec = ProtoField.bool("rdpudp.flags.fec", "FECData", base.HEX, nil, RDPUDP_FEC)
local pf_udp_flag_cn = ProtoField.bool("rdpudp.flags.cn", "CN", base.HEX, nil, RDPUDP_CN)
local pf_udp_flag_cwr = ProtoField.bool("rdpudp.flags.cwr", "CWR", base.HEX, nil, RDPUDP_CWR)
local pf_udp_flag_aoa = ProtoField.bool("rdpudp.flags.aoa", "Ack of Acks", base.HEX, nil, RDPUDP_AOA)
local pf_udp_flag_synlossy = ProtoField.bool("rdpudp.flags.synlossy", "Syn lossy", base.HEX, nil, RDPUDP_SYNLOSSY)
local pf_udp_flag_ackdelayed = ProtoField.bool("rdpudp.flags.ackdelayed", "Ack delayed", base.HEX, nil, RDPUDP_ACKDELAYED)
local pf_udp_flag_correlationId = ProtoField.bool("rdpudp.flags.correlationid", "Correlation id", base.HEX, nil, RDPUDP_CORRELATIONID)
local pf_udp_flag_synex = ProtoField.bool("rdpudp.flags.synex", "SynEx", base.HEX, nil, RDPUDP_SYNEX)

local pf_udp_snInitialSequenceNumber = ProtoField.uint32("rdpudp.initialsequencenumber", "Initial SequenceNumber", base.HEX)
local pf_udp_upstreamMtu = ProtoField.uint16("rdpudp.upstreammtu", "Upstream MTU", base.DEC)
local pf_udp_downstreamMtu = ProtoField.uint16("rdpudp.downstreammtu", "DownStream MTU", base.DEC)

local pf_udp_correlationId = ProtoField.new("Correlation Id", "rdpudp.correlationid", ftypes.BYTES)

local pf_udp_synex_flags = ProtoField.uint16("rdpudp.synex.flags", "Flags", base.HEX)
local pf_udp_synex_flag_version = ProtoField.bool("rdpudp.synex.flags.versioninfo", "Version info", base.HEX, nil, 0x0001)
local pf_udp_synex_version = ProtoField.uint16("rdpudp.synex.version", "Version", base.HEX, {[1]="Version 1", [2]="Version 2", [0x101]="Version 3"})
local pf_udp_synex_cookiehash = ProtoField.new("Cookie Hash", "rdpudp.synex.cookiehash", ftypes.BYTES)

local pf_udp_ack_vectorsize = ProtoField.uint16("rdpudp.ack.vectorsize", "uAckVectorSize", base.DEC)
local pf_udp_ack_item = ProtoField.uint8("rdpudp.ack.item", "Ack item", base.HEX)
local pf_udp_ack_item_state = ProtoField.uint8("rdpudp.ack.item.state", "VECTOR_ELEMENT_STATE", base.HEX, {[0]="Received", [1]="Reserved 1", [2]="Reserved 2", [3]="Pending"}, 0xc0)
local pf_udp_ack_item_rle = ProtoField.uint8("rdpudp.ack.item.rle", "Run length", base.DEC, nil, 0x3f)

local pf_udp_fec_coded = ProtoField.uint32("rdpudp.fec.coded", "snCoded", base.HEX)
local pf_udp_fec_sourcestart = ProtoField.uint32("rdpudp.fec.sourcestart", "snSourceStart", base.HEX)
local pf_udp_fec_range = ProtoField.uint8("rdpudp.fec.range", "Range", base.DEC)
local pf_udp_fec_fecindex = ProtoField.uint8("rdpudp.fec.fecindex", "Fec index", base.HEX)

local pf_udp_resetseqenum = ProtoField.uint32("rdpudp.resetSeqNum", "snResetSeqNum", base.HEX)

local pf_udp_source_sncoded = ProtoField.uint32("rdpudp.data.sncoded", "snCoded", base.HEX)
local pf_udp_source_snSourceStart = ProtoField.uint32("rdpudp.data.sourceStart", "snSourceStart", base.HEX)


-- UDP2 fields
local pf_PacketPrefixByte = ProtoField.new("PacketPrefixByte", "rdpudp2.prefixbyte", ftypes.UINT8, nil, base.HEX)

local pf_packetType = ProtoField.uint8("rdpudp2.packetType", "PacketType", base.HEX, {[0] = "Data", [8] = "Dummy"}, 0x1e, "type of packet")

RDPUDP2_ACK = 0x0001
RDPUDP2_DATA = 0x0004
RDPUDP2_ACKVEC = 0x0008
RDPUDP2_AOA = 0x0010
RDPUDP2_OVERHEAD = 0x0040
RDPUDP2_DELAYACK = 0x00100

local pf_flags = ProtoField.uint16("rdpudp2.flags", "Flags", base.HEX, nil, 0xfff, "flags")
local pf_flag_ack = ProtoField.bool("rdpudp2.flags.ack", "Ack", base.HEX, nil, RDPUDP2_ACK, "packet contains Ack payload")
local pf_flag_data = ProtoField.bool("rdpudp2.flags.data", "Data", base.HEX, nil, RDPUDP2_DATA, "packet contains Data payload")
local pf_flag_ackvec = ProtoField.bool("rdpudp2.flags.ackvec", "AckVec", base.HEX, nil, RDPUDP2_ACKVEC, "packet contains AckVec payload")
local pf_flag_aoa = ProtoField.bool("rdpudp2.flags.ackofacks", "AckOfAcks", base.HEX, nil, RDPUDP2_AOA, "packet contains AckOfAcks payload")
local pf_flag_overhead = ProtoField.bool("rdpudp2.flags.overheadsize", "OverheadSize", base.HEX, nil, RDPUDP2_OVERHEAD, "packet contains OverheadSize payload")
local pf_flag_delayackinfo = ProtoField.bool("rdpudp2.flags.delayackinfo", "DelayedAckInfo", base.HEX, nil, RDPUDP2_DELAYACK, "packet contains DelayedAckInfo payload")

local pf_logWindow = ProtoField.uint16("rdpudp2.logWindow", "LogWindow", base.DEC, nil, 0xf000, "flags")

local pf_AckSeq = ProtoField.uint16("rdpudp2.ack.seqnum", "Base Seq", base.HEX)
local pf_AckTs = ProtoField.uint24("rdpudp2.ack.ts", "receivedTS", base.DEC)
local pf_AckSendTimeGap = ProtoField.uint8("rdpudp2.ack.sendTimeGap", "sendTimeGap", base.DEC)
local pf_ndelayedAcks = ProtoField.uint8("rdpudp2.ack.numDelayedAcks", "NumDelayedAcks", base.DEC, nil, 0x0f)
local pf_delayedTimeScale = ProtoField.uint8("rdpudp2.ack.delayedTimeScale", "delayedTimeScale", base.DEC, nil, 0xf0)
local pf_delayedAcks = ProtoField.new("Delayed acks", "rdpudp2.ack.delayedAcks", ftypes.BYTES)
local pf_delayedAck = ProtoField.uint8("rdpudp2.ack.delayedAck", "Delayed ack", base.DEC)

local pf_OverHeadSize = ProtoField.uint8("rdpudp2.overheadsize", "Overhead size", base.DEC)

local pf_DelayAckMax = ProtoField.uint8("rdpudp2.delayackinfo.max", "MaxDelayedAcks", base.DEC)
local pf_DelayAckTimeout = ProtoField.uint8("rdpudp2.delayackinfo.timeout", "DelayedAckTimeoutInMs", base.DEC)

local pf_AckOfAcks = ProtoField.uint16("rdpudp2.ackofacks", "Ack of Acks", base.HEX)

local pf_DataSeqNumber = ProtoField.uint16("rdpudp2.data.seqnum", "sequence number", base.HEX)
local pf_DataChannelSeqNumber = ProtoField.uint16("rdpudp2.data.channelseqnumber", "Channel sequence number", base.HEX)
local pf_Data = ProtoField.new("Data", "rdpudp2.data", ftypes.BYTES)

local pf_AckvecBaseSeq = ProtoField.uint16("rdpudp2.ackvec.baseseqnum", "Base sequence number", base.HEX)
local pf_AckvecCodecAckVecSize = ProtoField.uint16("rdpudp2.ackvec.codecackvecsize", "Codec ackvec size", base.DEC, nil, 0x7f)
local pf_AckvecHaveTs = ProtoField.bool("rdpudp2.ackvec.havets", "have timestamp", base.DEC, nil, 0x80)
local pf_AckvecTimeStamp = ProtoField.uint24("rdpudp2.ackvec.timestamp", "Timestamp", base.HEX)
local pf_AckvecCodedAck = ProtoField.uint8("rdpudp2.ackvec.codecAck", "Coded Ack", base.HEX)
local pf_AckvecCodedAckMode = ProtoField.uint8("rdpudp2.ackvec.codecAckMode", "Mode", base.HEX, {[0]="Bitmap", [1]="Run length"}, 0x80)
local pf_AckvecCodedAckRleState = ProtoField.uint8("rdpudp2.ackvec.codecAckRleState", "State", base.HEX, {[0]="lost",[1]="received"}, 0x40)
local pf_AckvecCodedAckRleLen = ProtoField.uint8("rdpudp2.ackvec.codecAckRleLen", "Length", base.DEC, nil, 0x3f)

rdpudp.fields = {
	-- UDP1
	pf_udp_snSourceAck, pf_udp_ReceiveWindowSize, pf_udp_flags, pf_udp_flag_syn,
	pf_udp_flag_fin, pf_udp_flag_ack, pf_udp_flag_data, pf_udp_flag_fec, pf_udp_flag_cn,
	pf_udp_flag_cwr, pf_udp_flag_aoa, pf_udp_flag_synlossy, pf_udp_flag_ackdelayed,
	pf_udp_flag_correlationId, pf_udp_flag_synex,
	pf_udp_snInitialSequenceNumber, pf_udp_upstreamMtu, pf_udp_downstreamMtu,
	pf_udp_correlationId,
	pf_udp_synex_flags, pf_udp_synex_flag_version, pf_udp_synex_version, pf_udp_synex_cookiehash,
	pf_udp_ack_vectorsize, pf_udp_ack_item, pf_udp_ack_item_state, pf_udp_ack_item_rle,
	pf_udp_fec_coded, pf_udp_fec_sourcestart, pf_udp_fec_range, pf_udp_fec_fecindex,
	pf_udp_resetseqenum,
	pf_udp_source_sncoded, pf_udp_source_snSourceStart,
	
	-- UDP2
	pf_PacketPrefixByte, pf_packetType, 
	pf_flags, pf_flag_ack, pf_flag_data, pf_flag_ackvec, pf_flag_aoa, pf_flag_overhead, pf_flag_delayackinfo,
	pf_logWindow,
	pf_Ack, pf_AckSeq, pf_AckTs, pf_AckSendTimeGap, pf_ndelayedAcks, pf_delayedTimeScale, pf_delayedAcks,
	pf_OverHeadSize,
	pf_DelayAckMax, pf_DelayAckTimeout, 
	pf_AckOfAcks,
	pf_DataSeqNumber, pf_Data, pf_DataChannelSeqNumber,
	pf_Ackvec, pf_AckvecBaseSeq, pf_AckvecCodecAckVecSize, pf_AckvecHaveTs, pf_AckvecTimeStamp, pf_AckvecCodedAck, pf_AckvecCodedAckMode,
	pf_AckvecCodedAckRleState, pf_AckvecCodedAckRleLen
}

rdpudp.prefs.track_udp2_peer_states = Pref.bool("Track state of UDP2 peers", true, "Keep track of state of UDP2 peers (receiver and sender windows")
rdpudp.prefs.debug_ssl = Pref.bool("SSL debug message", false, "print verbose message of the SSL fragments reassembly")
	


local field_rdpudp_flags = Field.new("rdpudp.flags")
local field_rdpudp2_packetType = Field.new("rdpudp2.packetType")
local field_rdpudp2_channelSeqNumber = Field.new("rdpudp2.data.channelseqnumber")
local field_rdpudp2_ackvec_base = Field.new("rdpudp2.ackvec.baseseqnum")

function unwrapPacket(tvbuf)
	local len = tvbuf:reported_length_remaining()
	local ret = tvbuf:bytes(7, 1) .. tvbuf:bytes(1, 6) .. tvbuf:bytes(0, 1) .. tvbuf:bytes(8, len-8)
	--dprint2("iput first bytes=", tvbuf:bytes(0, 9):tohex(true, " "))
	--dprint2("oput first bytes=", ret:subset(0, 9):tohex(true, " "))
	return ret:tvb("RDP-UDP unwrapped")
end

function rdpudp.init()
	udpComms = {}
end

function computePacketKey(pktinfo)
	local addr_lo = pktinfo.net_src
	local addr_hi = pktinfo.net_dst
	local port_lo = pktinfo.src_port
	local port_hi = pktinfo.dst_port

	if addr_lo > addr_hi then
		addr_hi, addr_lo = addr_lo, addr_hi
		port_hi, port_lo = port_lo, port_hi
	end

	return tostring(addr_lo) .. ":" .. tostring(port_lo) .. " -> " .. tostring(addr_hi) .. ":" .. tostring(port_hi)
end

function tableItem(pktinfo)	
	local key = computePacketKey(pktinfo)
	local ret = udpComms[key]
	if ret == nil then
		dprint2(pktinfo.number .. " creating entry for " .. key)
		udpComms[key] = { isLossy = false, switchToUdp2 = nil, sslFragments = {}, 
			serverAddr = nil, clientAddr = nil,
			serverState = { receiveLow = nil, receiveHigh = nil, senderLow = nil, senderHigh = nil },
			clientState = { receiveLow = nil, receiveHigh = nil, senderLow = nil, senderHigh = nil }
		}
		ret = udpComms[key]
	end
	return ret
end

function doAlign(v, alignment)
	local rest = v % alignment
	if rest ~= 0 then
		return v + alignment - rest
	end
	return v
end

function dissectV1(tvbuf, pktinfo, tree)
	--dprint2("dissecting in UDP1 mode")
	local pktlen = tvbuf:reported_length_remaining()
	
	tree:add(pf_udp_snSourceAck, tvbuf:range(0, 4))
	tree:add(pf_udp_ReceiveWindowSize, tvbuf:range(4, 2))
	local flagsRange = tvbuf:range(6, 2)
	local flagsItem = tree:add(pf_udp_flags, flagsRange)
		--
		flagsItem:add(pf_udp_flag_syn, flagsRange)
		flagsItem:add(pf_udp_flag_fin, flagsRange)
		flagsItem:add(pf_udp_flag_ack, flagsRange)
		flagsItem:add(pf_udp_flag_data, flagsRange)
		flagsItem:add(pf_udp_flag_fec, flagsRange)
		flagsItem:add(pf_udp_flag_cn, flagsRange)
		flagsItem:add(pf_udp_flag_cwr, flagsRange)
		flagsItem:add(pf_udp_flag_aoa, flagsRange)
		flagsItem:add(pf_udp_flag_synlossy, flagsRange)
		flagsItem:add(pf_udp_flag_ackdelayed, flagsRange)
		flagsItem:add(pf_udp_flag_correlationId, flagsRange)
		flagsItem:add(pf_udp_flag_synex, flagsRange)

	startAt = 8
	local flags = flagsRange:uint()
	local haveSyn = bit32.band(flags, RDPUDP_SYN) ~= 0
	local haveAck = bit32.band(flags, RDPUDP_ACK) ~= 0
	local isLossySyn = bit32.band(flags, RDPUDP_SYNLOSSY) ~= 0
	local tableRecord = tableItem(pktinfo)

	
	if isLossySyn then
		tableRecord.isLossy = true
	end
	
	if haveSyn then
		-- dprint2("rdpudp - SYN")
		local synItem = tree:add("Syn", tvbuf:range(startAt, 8))
		
		synItem:add(pf_udp_snInitialSequenceNumber, tvbuf:range(startAt, 4))
		synItem:add(pf_udp_upstreamMtu, tvbuf:range(startAt+4, 2))
		synItem:add(pf_udp_downstreamMtu, tvbuf:range(startAt+6, 2))
		
		startAt = startAt + 8
	end

	if bit32.band(flags, RDPUDP_CORRELATIONID) ~= 0 then
		-- dprint2("rdpudp - CorrelationId")
		tree:add(pf_udp_correlationId, tvbuf:range(startAt, 16))
		startAt = startAt + 32
	end

	if bit32.band(flags, RDPUDP_SYNEX) ~= 0 then
		-- dprint2("rdpudp - SynEx")
		local synexItem = tree:add("SynEx")		
		
		local synexFlagsRange = tvbuf:range(startAt, 2)
		local synexFlags = synexItem:add(pf_udp_synex_flags, synexFlagsRange);
			--
			synexFlags:add(pf_udp_synex_flag_version, synexFlagsRange)
		local exflags = synexFlagsRange:uint()
		startAt = startAt + 2
		if bit32.band(exflags, 1) ~= 0 then
			synexItem:add(pf_udp_synex_version, tvbuf:range(startAt, 2))
			local versionVal = tvbuf:range(startAt, 2):uint()
			startAt = startAt + 2
			
			if versionVal == 0x101 then
				if not haveAck	then
					synexItem:add(pf_udp_synex_cookiehash, tvbuf:range(startAt, 32))
					startAt = startAt + 32
				else
					-- switch to UDP2
					tableRecord.switchToUdp2 = pktinfo.number
				end
			end
		end
		
		local mask = RDPUDP_SYN + RDPUDP_ACK
		if bit32.band(flags, mask) == mask then
			tableRecord.serverAddr = tostring(pktinfo.net_src)
			tableRecord.clientAddr = tostring(pktinfo.net_dst)
			-- dprint2(pktinfo.number .. ": key='" .. computePacketKey(pktinfo) ..
			--	"' setting server=" .. tableRecord.serverAddr .. " client=" .. tableRecord.clientAddr)
		end
	end
	
	if haveAck and not haveSyn then
		-- dprint2("rdpudp - Ack")
		local ackItem = tree:add("Ack")
		ackItem:add(pf_udp_ack_vectorsize, tvbuf:range(startAt, 2))

		local i = 0
		uAckVectorSize = tvbuf:range(startAt, 2):uint()
		while i < uAckVectorSize do
			local ackRange = tvbuf:range(startAt + 2 + i, 1)
			local ack = ackItem:add(pf_udp_ack_item, ackRange)
			ack:add(pf_udp_ack_item_state, ackRange)
			ack:add(pf_udp_ack_item_rle, ackRange)
			i = i + 1
		end -- while
		
		-- aligned on a dword (4 bytes) boundary
		-- dprint2("pktinfo=",pktinfo.number," blockSz=",doAlign(2 + uAckVectorSize, 4))
		startAt = startAt + doAlign(2 + uAckVectorSize, 4)
	end		

	if bit32.band(flags, RDPUDP_FEC) ~= 0 then
		-- dprint2("rdpudp - FEC header")
		local fecItem = tree:add("FEC", tvbuf:range(startAt, 12))
		fecItem:add(pf_udp_fec_coded, tvbuf:range(startAt, 4))
		fecItem:add(pf_udp_fec_sourcestart, tvbuf:range(startAt+4, 4))
		fecItem:add(pf_udp_fec_range, tvbuf:range(startAt+8, 1))
		fecItem:add(pf_udp_fec_fecindex, tvbuf:range(startAt+9, 1))
		
		startAt = startAt + (4 * 3)
	end
	
	if bit32.band(flags, RDPUDP_AOA) ~= 0 then
		-- dprint2("rdpudp - AOA")
		tree:add(pf_udp_resetseqenum, tvbuf:range(startAt, 4))
		startAt = startAt + 4
	end
	
	if bit32.band(flags, RDPUDP_DATA) ~= 0 then
		-- dprint2("rdpudp - Data")
		local dataItem = tree:add("Data")
		dataItem:add(pf_udp_source_sncoded, tvbuf:range(startAt, 4))
		dataItem:add(pf_udp_source_snSourceStart, tvbuf:range(startAt+4, 4))
		startAt = startAt + 8
		
		local payload = tvbuf:range(startAt)
		local subTvb = payload:tvb("payload")
		if tableRecord.isLossy then
			dtlsDissector:call(subTvb, pktinfo, dataItem)
		else
			sslDissector:call(subTvb, pktinfo, dataItem)
		end
	end
	
	return pktlen
end

-- given a tvb containing SSL records returns the part of the buffer that has complete
-- SSL records
function getCompleteSslRecordsLen(tvb)
	local startAt = 0
	local remLen = tvb:reported_length_remaining()
	
	while remLen > 5 do
		local recordLen = 5 + tvb:range(startAt+3, 2):uint()
		if remLen < recordLen then
			break
		end
		startAt = startAt + recordLen
		remLen = remLen - recordLen
	end -- while
	
	return startAt;
end

TLS_OK = 0
TLS_SHORT = 1
TLS_NOT_TLS = 2
TLS_NOT_COMPLETE = 3
sslResNames = {[0]="TLS_OK", [1]="TLS_SHORT", [2]="TLS_NOT_TLS", [3]="TLS_NOT_COMPLETE"}

function checkSslRecord(tvb)	
	local remLen = tvb:reported_length_remaining()
	
	if remLen <= 5 then
		return TLS_SHORT, 0
	end

	local b0 = tvb:range(0, 1):uint()
	if b0 < 0x14 or b0 > 0x17 then
		-- dprint2("doesn't look like a SSL record, b0=",b0)
		return TLS_NOT_TLS, 0
	end
	
	local recordLen = 5 + tvb:range(3, 2):uint()		
	if remLen < recordLen then
		return TLS_NOT_COMPLETE, recordLen
	end
	return TLS_OK, recordLen
end


function getSslFragments(pktinfo)
	local addr0 = pktinfo.net_src
	local addr1 = pktinfo.net_dst
	local port0 = pktinfo.src_port
	local port1 = pktinfo.dst_port
	local key = tostring(addr0) .. ":" .. tostring(port0) .. "->" .. tostring(addr1) .. ":" .. tostring(port1)
	
	local tableRecord = tableItem(pktinfo)
	if tableRecord.sslFragments[key] == nil then
		tableRecord.sslFragments[key] = {}
	end
	
	return tableRecord.sslFragments[key]
end

function dissectV2(in_tvbuf, pktinfo, tree)
	-- dprint2("dissecting in UDP2 mode")
	local pktlen = in_tvbuf:reported_length_remaining()
	if pktlen < 7 then
		dprint2("packet ", pktinfo.number, " too short, len=", pktlen)
		return
	end
	
	local conversation = tableItem(pktinfo)
	local sourceState = nil
	local targetState = nil
	if rdpudp.prefs.track_udp2_peer_states then
		if tostring(pktinfo.net_dst) == conversation.serverAddr then
			sourceState = conversation.clientState
			targetState = conversation.serverState
		else
			sourceState = conversation.serverState
			targetState = conversation.clientState
		end
	end
	
	pktinfo.cols.info = ""
	local info = "("
	local tvbuf = unwrapPacket(in_tvbuf)
	local prefixRange = tvbuf:range(0, 1)
	local prefix_tree = tree:add(pf_PacketPrefixByte, prefixRange)
		--
		local packetType = prefix_tree:add(pf_packetType, prefixRange)
		
	local flagsRange = tvbuf:range(1,2)
	local flagsTree = tree:add_le(pf_flags, flagsRange)
		--
		flagsTree:add_packet_field(pf_flag_ack, flagsRange, ENC_LITTLE_ENDIAN)
		flagsTree:add_le(pf_flag_data, flagsRange)
		flagsTree:add_le(pf_flag_ackvec, flagsRange)
		flagsTree:add_le(pf_flag_aoa, flagsRange)
		flagsTree:add_le(pf_flag_overhead, flagsRange)
		flagsTree:add_le(pf_flag_delayackinfo, flagsRange)
		
	tree:add_le(pf_logWindow, flagsRange)
	
	local flags = tvbuf:range(1,2):le_uint()

	local startAt = 3
	if bit32.band(flags, RDPUDP2_ACK) ~= 0 then
		-- dprint2("got ACK payload")
		info = info .. "ACK," 
		local ackTree = tree:add("Ack") 
		
		ackTree:add_le(pf_AckSeq, tvbuf:range(startAt, 2))
		ackTree:add_le(pf_AckTs, tvbuf:range(startAt+2, 3))
		ackTree:add(pf_AckSendTimeGap, tvbuf:range(startAt+5, 1))
		ackTree:add(pf_ndelayedAcks, tvbuf:range(startAt+6, 1))
		ackTree:add(pf_delayedTimeScale, tvbuf:range(startAt+6, 1))
		
		local ackSeq = tvbuf:range(startAt, 2):le_uint()
		local ackTs = tvbuf:range(startAt+2, 3):le_uint()
		local nacks = bit32.band(tvbuf:range(startAt+6, 1):le_uint(), 0xf)
		local delayAckTimeScale = bit32.rshift(bit32.band(tvbuf:range(startAt+6, 1):le_uint(), 0xf0), 4)
		-- dprint2(pktinfo.number,": nACKs=", nacks, "delayAckTS=", bit32.rshift(delayAckTimeScale, 4))
		
		if rdpudp.prefs.track_udp2_peer_states then
			targetState.senderLow = ackSeq
		end
		
		startAt = startAt + 7
		if nacks ~= 0 then
			local acksItem = ackTree:add(pf_delayedAcks, tvbuf:range(startAt, nacks))
			local i
			for i = nacks-1, 0, -1 do
				local ackDelay = tvbuf:range(startAt+i, 1):le_uint() * bit32.lshift(1, delayAckTimeScale)
				acksItem:add(pf_delayedAck, tvbuf:range(startAt+i, 1), "seq=0x" .. string.format("%0.4x", ackSeq-i-1) .. " ts=" .. ackTs-ackDelay)
			end
			acksItem:add(pf_delayedAck, tvbuf:range(startAt, nacks), "seq=0x" .. string.format("%0.4x", ackSeq) .. " ts=" .. ackTs):set_generated()
		end
		startAt = startAt + nacks
	end

	if bit32.band(flags, RDPUDP2_OVERHEAD) ~= 0 then
		info = info .. "OVERHEAD," 

		tree:add_le(pf_OverHeadSize, tvbuf:range(startAt, 1))
		startAt = startAt + 1
	end

	if bit32.band(flags, RDPUDP2_DELAYACK) ~= 0 then
		info = info .. "DELAYEDACK," 

		local delayAckItem = tree:add("DelayAckInfo", tvbuf:range(startAt, 3))
		delayAckItem:add_le(pf_DelayAckMax, tvbuf:range(startAt, 1))
		delayAckItem:add_le(pf_DelayAckTimeout, tvbuf:range(startAt+1, 2))
		startAt = startAt + 3
	end

	if bit32.band(flags, RDPUDP2_AOA) ~= 0 then
		info = info .. "AOA,"
		tree:add_le(pf_AckOfAcks, tvbuf:range(startAt, 2))
		startAt = startAt + 2
	end

	local dataTree
	local isDummy = (field_rdpudp2_packetType()() == 0x8)
	if bit32.band(flags, RDPUDP2_DATA) ~= 0 then
		if isDummy then
			info = info .. "DUMMY,"
		else
			info = info .. "DATA,"
		end
		dataTree = tree:add(isDummy and "Dummy Data" or "Data")
		dataTree:add_le(pf_DataSeqNumber, tvbuf:range(startAt, 2))
		startAt = startAt + 2
	end

	if bit32.band(flags, RDPUDP2_ACKVEC) ~= 0 then
		-- dprint2("got ACKVEC payload")
		info = info .. "ACKVEC,"

		local codedAckVecSizeA = tvbuf:range(startAt+2, 1):le_uint()
		local codedAckVecSize = bit32.band(codedAckVecSizeA, 0x7f)
		local haveTs = bit32.band(codedAckVecSizeA, 0x80) ~= 0
		
		local ackVecTree = tree:add("AckVec")
		ackVecTree:add_le(pf_AckvecBaseSeq, tvbuf:range(startAt, 2))
		ackVecTree:add(pf_AckvecCodecAckVecSize, tvbuf:range(startAt+2, 1))
		ackVecTree:add(pf_AckvecHaveTs, tvbuf:range(startAt+2, 1))
		startAt = startAt + 3
		if haveTs then
			ackVecTree:add_le(pf_AckvecTimeStamp, tvbuf:range(startAt, 4))
			startAt = startAt + 4
		end
		local codedAckVector = ackVecTree:add("Vector", tvbuf:range(startAt, codedAckVecSize))
		local seqNumber = field_rdpudp2_ackvec_base()()
		for i = 0, codedAckVecSize-1, 1 do
			local bRange = tvbuf:range(startAt + i, 1)
			local b = bRange:uint()
			
			local codedAck = codedAckVector:add(pf_AckvecCodedAck, bRange, b)
			codedAck:add(pf_AckvecCodedAckMode, bRange)
			
			local itemString = "";
			if bit32.band(b, 0x80) == 0 then
				-- bitmap length mode
				itemString = string.format("bitmap(0x%0.2x): ", b) 
				local mask = 0x1
				for j = 0, 7-1 do
					flag = "!"
					if bit32.band(b, mask) ~= 0 then
						flag = ""
					end
					
					itemString = itemString .. " " .. flag .. string.format("%0.4x", seqNumber)
					mask = mask * 2
					seqNumber = seqNumber + 1
				end
			else
				-- run length mode
				codedAck:add(pf_AckvecCodedAckRleState, bRange)
				codedAck:add(pf_AckvecCodedAckRleLen, bRange)

				local rleLen = bit32.band(b, 0x3f)		
				itemString = "rle(len=" .. rleLen .. "): ".. (bit32.band(b, 0x40) and "received" or "lost") .. 
					string.format(" %0.4x -> %0.4x", seqNumber, seqNumber + rleLen)
				seqNumber = seqNumber + rleLen							
			end
			
			codedAck:set_text(itemString)
			
		end
		
		startAt = startAt + codedAckVecSize
	end
	
	if not isDummy and bit32.band(flags, RDPUDP2_DATA) ~= 0 then
		dataTree:add_le(pf_DataChannelSeqNumber, tvbuf:range(startAt, 2))
		local payload = tvbuf:range(startAt + 2)
		local subTvb = payload:tvb("payload")
		
		local channelSeqId = field_rdpudp2_channelSeqNumber()()
		local sslFragments = getSslFragments(pktinfo)
		local workTvb = nil

		local sslRes, recordLen = checkSslRecord(subTvb)
		if rdpudp.prefs.debug_ssl then	
			dprint2("packet=", pktinfo.number, " channelSeq=", channelSeqId, 
				" dataLen=", subTvb:reported_length_remaining(),
				" sslRes=", sslResNames[sslRes], 
				" recordLen=", recordLen)
		end
		if sslRes == TLS_OK then
			workTvb = subTvb
		elseif sslRes == TLS_SHORT or sslRes == TLS_NOT_COMPLETE then
			if rdpudp.prefs.debug_ssl then
				dprint2("packet=", pktinfo.number, " recording fragment len=", subTvb:len())
			end
			
			local frag = ByteArray.new()
			frag:append(subTvb:bytes())			
			sslFragments[channelSeqId] = frag
			
		elseif sslRes == TLS_NOT_TLS then
			local prevFragment = sslFragments[channelSeqId-1]
			if rdpudp.prefs.debug_ssl then
				dprint2("packet=",pktinfo.number," picking channelSeq=", channelSeqId-1, " havePrevFragment=", prevFragment ~= nil and "ok" or "no")
			end
			if prevFragment ~= nil then
				-- dprint2("prevLen=",prevFragment:len(), " subTvbLen=",subTvb:len())
				local testBytes = prevFragment .. subTvb:bytes()
				local testTvb = ByteArray.tvb(testBytes, "reassembled fragment")
				
				sslRes, recordLen = checkSslRecord(testTvb)
				if rdpudp.prefs.debug_ssl then
					dprint2("packet=", pktinfo.number, 
						" reassembled len=", testTvb:reported_length_remaining(),
						" sslRes=", sslResNames[sslRes], 
						" recordLen=", recordLen)
				end
				if sslRes == TLS_OK then
					workTvb = testTvb
				end
			end	
		end
				
		if workTvb ~= nil then		
			repeat
				if rdpudp.prefs.debug_ssl then
					dprint2("treating workTvbLen=", workTvb:reported_length_remaining(), " recordLen=",recordLen)
				end
				local sslFragment = workTvb:range(0, recordLen):tvb("SSL fragment")
				sslDissector:call(sslFragment, pktinfo, dataTree)
				
				workTvb = workTvb:range(recordLen):tvb()
				sslRes, recordLen = checkSslRecord(workTvb)

				if sslRes == TLS_SHORT or sslRes == TLS_NOT_COMPLETE then
					if rdpudp.prefs.debug_ssl then
						dprint2("packet=", pktinfo.number, " recording fragment len=", subTvb:len())
					end
					
					local frag = ByteArray.new()
					frag:append(workTvb:bytes())
					sslFragments[channelSeqId] = frag
				end
				
			until sslRes ~= TLS_OK or workTvb:reported_length_remaining() == 0
		else		
			dataTree:add_le(pf_Data, payload)
		end
	end
	
	info = string.sub(info, 0, -2) .. ")"
	pktinfo.cols.info = info -- .. tostring(pktinfo.cols.info)
	if rdpudp.prefs.track_udp2_peer_states then
		local stateTrackItem = tree:add("UDP2 state tracking")
		stateTrackItem:set_generated()
		stateTrackItem:add(tostring(pktinfo.net_dst) == conversation.serverAddr and "Client -> Server" or "Server -> Client")
	end
	
	
	return pktlen
end

function rdpudp.dissector(in_tvbuf, pktinfo, root)
	-- dprint2("rdpudp.dissector called")
	pktinfo.cols.protocol:set("RDP-UDP")
	
	local pktlen = in_tvbuf:reported_length_remaining()
	local tree = root:add(rdpudp, in_tvbuf:range(0,pktlen))
	
	local tableRecord = tableItem(pktinfo)
	local doDissectV1 = true
	if tableRecord.switchToUdp2 ~= nil and tableRecord.switchToUdp2 < pktinfo.number then
		doDissectV1 = false
	end
	
	if doDissectV1 then
		return dissectV1(in_tvbuf, pktinfo, tree)
	end
	
	return dissectV2(in_tvbuf, pktinfo, tree)	
end

DissectorTable.get("udp.port"):add(3389, rdpudp)
