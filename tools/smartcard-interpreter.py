#!/usr/bin/env python3
#
#     Copyright 2022 David Fort <contact@hardening-consulting.com>
#
# This script is meant to parse some FreeRDP logs in DEBUG mode (WLOG_LEVEL=DEBUG) and interpret the
# smartcard traffic, dissecting the PIV or GIDS commands
#
# usage: 
#   * live: WLOG_LEVEL=DEBUG xfreerdp <args with smartcard> | python3 smartcard-interpreter.py
#   * on an existing log file: python3 smartcard-interpreter.py <log file>
#
import sys
import codecs


CMD_NAMES = {
    0x04: "DESACTIVATE FILE",
    0x0C: "ERASE RECORD",
    0x0E: "ERASE BINARY",
    0x0F: "ERASE BINARY",
    0x20: "VERIFY",
    0x21: "VERIFY",
    0x22: "MSE",
    0x24: "CHANGE REFERENCE DATA",
    0x25: "MSE",
    0x26: "DISABLE VERIFICATION REQUIREMENT",
    0x28: "ENABLE VERIFICATION REQUIREMENT",
    0x2A: "PSO",
    0x2C: "RESET RETRY COUNTER",
    0x2D: "RESET RETRY COUNTER",
    0x44: "ACTIVATE FILE",
    0x46: "GENERATE ASYMMETRIC KEY PAIR",
    0x47: "GENERATE ASYMMETRIC KEY PAIR",
    0x84: "GET CHALLENGE",
    0x86: "GENERAL AUTHENTICATE",
    0x87: "GENERAL AUTHENTICATE",
    0x88: "INTERNAL AUTHENTICATE",
    0xA0: "SEARCH BINARY",
    0xA1: "SEARCH BINARY",
    0xA2: "SEARCH RECORD",
    0xA4: "SELECT",
    0xB0: "READ BINARY",
    0xB1: "READ BINARY",
    0xB2: "READ RECORD",
    0xB3: "READ RECORD",
    0xC0: "GET RESPONSE",
    0xC2: "ENVELOPPE",
    0xC3: "ENVELOPPE",
    0xCA: "GET DATA",
    0xCB: "GET DATA",
    0xD0: "WRITE BINARY",
    0xD1: "WRITE BINARY",
    0xD2: "WRITE RECORD",
    0xD6: "UPDATE BINARY",
    0xD7: "UPDATE BINARY",
    0xDA: "PUT DATA",
    0xDB: "PUT DATA",
    0xDC: "UPDATE RECORD",
    0xDD: "UPDATE RECORD",
    0xE0: "CREATE FILE",
    0xE2: "APPEND RECORD",
    0xE4: "DELETE FILE",
    0xE6: "TERMINATE DF",
    0xE8: "TERMINATE EF",
    0xFE: "TERMINATE CARD USAGE",
}

AIDs = {
    "a00000039742544659": "MsGidsAID",
    "a000000308": "PIV",
    "a0000003974349445f0100": "SC PNP",
}

FIDs = {
    0x0000: "Current EF",
    0x2F00: "EF.DIR",
    0x2F01: "EF.ATR",
    0x3FFF: "Current application(ADF)",
}

DOs = {
    "df1f": "DO_FILESYSTEMTABLE", 
    "df20": "DO_CARDID",
    "df21": "DO_CARDAPPS",
    "df22": "DO_CARDCF",
    "df23": "DO_CMAPFILE",
    "df24": "DO_KXC00",
}

ERROR_CODES = {
    0x9000: "success",
    0x6282: "end of file or record",
    0x63C0: "warning counter 0",
    0x63C1: "warning counter 1",
    0x63C2: "warning counter 2",
    0x63C3: "warning counter 3",
    0x63C4: "warning counter 4",
    0x63C5: "warning counter 5",
    0x63C6: "warning counter 6",
    0x63C7: "warning counter 7",
    0x63C8: "warning counter 8",
    0x63C9: "warning counter 9",
    0x6982: "security status not satisfied",
    0x6985: "condition of use not satisfied",
    0x6A80: "incorrect parameter cmd data field",
    0x6A81: "function not suppported",
    0x6A82: "file or application not found",
    0x6A83: "record not found",
    0x6A88: "REFERENCE DATA NOT FOUND",
    0x6D00: "unsupported",
}

PIV_OIDs = {
    "5fc101": "X.509 Certificate for Card Authentication",
    "5fc102": "Card Holder Unique Identifier",
    "5fc103": "Cardholder Fingerprints",
    "5fc105": "X.509 Certificate for PIV Authentication",
    "5fc106": "Security Object",
    "5fc107": "Card Capability Container",
    "5fc108": "Cardholder Facial Image",
    "5fc10a": "X.509 Certificate for Digital Signature",
    "5fc10b": "X.509 Certificate for Key Management",
    "5fc10d": "Retired X.509 Certificate for Key Management 1", 
    "5fc10e": "Retired X.509 Certificate for Key Management 2", 
    "5fc10f": "Retired X.509 Certificate for Key Management 3", 
}

class ApplicationDummy(object):
    def __init__(self, aid):
        self.aid = aid
    
    def getAID(self):
        return self.aid

    def selectResult(self, fci, status, body):
        return 'selectResult(%s, %s, %s)\n' %(fci, status, body.hex())
    
    def getData(self, fileId, bytes):
        return 'getData(0x%x, %s)\n' %(fileId, bytes.hex())
    
    def getDataResult(self, status, body):
        return 'getDataResult(0x%x, %s)\n' %(status, body.hex())
    
    def mse(self, body):
        return body.hex()

    def mseResult(self, status, body):
        return body.hex()
    
    def pso(self, body):
        return body.hex()

    def psoResult(self, status, body):
        return body.hex()


class ApplicationPIV(object):
    def __init__(self, aid):
        self.lastGet = None
        self.aid = aid
    
    def getAID(self):
        return self.aid
    
    def selectResult(self, selectT, status, body):
        ret = ''
        appTag = body[0]
        appLen = body[1]
    
        body = body[2:2+appLen]
        while len(body) > 2:
            tag = body[0]
            tagLen = body[1]
            if selectT == "FCI":
                if tag == 0x4f:
                    ret += "\tpiv version: %s\n" % body[2:2 + tagLen].hex()
                elif tag == 0x79:
                    subBody = body[2:2 + tagLen]                                
                    
                    subTag = subBody[0]
                    subLen = subBody[1]
                    
                    content = subBody.hex()
                    if subTag == 0x4f:
                        v = content[4:]
                        if v.startswith('a000000308'):
                            content = 'NIST RID'                                                                        
                    ret += '\tCoexistent tag allocation authority: %s\n' % content                                
                    
                elif tag == 0x50:
                    ret += '\tapplication label\n'
                elif tag == 0xac:
                    ret += '\tCryptographic algorithms supported\n'
                else:
                    rety += '\tunknown tag 0x%x\n' % tag
                    
            else:
                ret += "\tTODO: selectType %s\n" % selectT
            
            body = body[2+tagLen:]
            
        return ret
    
    def getData(self, fileId, bytes):
        ret = "\tfileId=%s\n" % FIDs.get(fileId, "%0.4x" % fileId)

        lc = bytes[4]
        tag = bytes[5]
        tagLen = bytes[6]

        if lc == 4:
            ret += "\tdoId=%0.4x\n"% (bytes[7] * 256 + bytes[8])
            
        elif lc == 0xa:
            keyStr = ''
            # TLV
            i = 7
            tag = bytes[i]
            tagLen = bytes[i+1]
            keyRef = bytes[i+3]
            keyStr = "key(tag=0x%x len=%d ref=0x%x)=" % (tag, tagLen, keyRef)
            i = i + 2 + tagLen 
            
            tag = bytes[i]
            tagLen = bytes[i+1]
            keyStr += "value(tag=0x%x len=%d)"
        elif lc == 5:
            if tag == 0x5C:
                tagStr = bytes[7:].hex()
                ret += '\ttag: %s(%s)\n' % (tagStr, PIV_OIDs.get(tagStr, '<unknown>'))
                self.lastGet = tagStr
        else:
            ret += "\tunknown key access\n"

        return ret
    
    def getDataResult(self, status, body):
        ret = ''
        if not len(body):
            return ''
        appTag = body[0]
        appLen = body[1]

        body = body[2:2+appLen]
        while len(body) > 2:
            tag = body[0]
            tagLen = body[1]
            tagBody = body[2:2+tagLen]
            
            if self.lastGet in ('5fc102',):
                # Card holder Unique Identifier
                if tag == 0x30:
                    ret += '\tFASC-N: %s\n' % tagBody.hex()
                elif tag == 0x34:
                    ret += '\tGUID: %s\n' % tagBody.hex()
                elif tag == 0x35:
                    ret += '\texpirationDate: %s\n' % tagBody.decode('utf8')
                elif tag == 0x3e:
                    ret += '\tIssuer Asymmetric Signature: %s\n' % tagBody.hex()
                else:
                    ret += "\tunknown tag=0x%x len=%d content=%s\n" % (tag, tagLen, tagBody.hex())
            else:
                ret += "\t%s: unknown tag=0x%x len=%d content=%s\n" % (self.lastGet, tag, tagLen, tagBody.hex())
            
            body = body[2+tagLen:]

        return ret
    
    def mse(self, body):
        return body.hex()

    def mseResult(self, status, body):
        return body.hex()
    
    def pso(self, body):
        return body.hex()

    def psoResult(self, status, body):
        return body.hex()


  
class ApplicationGids(object):
    def __init__(self, aid):
        self.aid = aid
        self.lastDo = None
    
    def getAID(self):
        return self.aid

    def parseFcp(self, bytes):
        ret = ''
        tag = bytes[0]
        tagLen = bytes[1]
        
        body = bytes[2:2+tagLen]
                
        if tag == 0x62:
            ret += '\tFCP\n'

            while len(body) > 2:
                tag2 = body[0]
                tag2Len = body[1]
                tag2Body = body[2:2+tag2Len] 
                
                if tag2 == 0x82:
                    ret += '\t\tFileDescriptor: %s\n' % tag2Body.hex() 
                elif tag2 == 0x8a:
                    ret += '\t\tLifeCycleByte: %s\n' % tag2Body.hex()
                elif tag2 == 0x84:
                    ret += '\t\tDF name: %s\n' % tag2Body.encode('utf8')
                elif tag2 == 0x8C:
                    ret += '\t\tSecurityAttributes: %s\n' % tag2Body.hex()
                else:
                    ret += '\t\tunhandled tag=0x%x body=%s\n' % (tag2, tag2Body.hex())
                
                body = body[2+tag2Len:]
        
        return ret
    
    def parseFci(self, bytes):
        ret = ''
        tag = bytes[0]
        tagLen = bytes[1]
        
        body = bytes[2:2+tagLen]
                
        if tag == 0x61:
            ret += '\tFCI\n'
            
            while len(body) > 2:
                tag2 = body[0]
                tag2Len = body[1]
                tag2Body = body[2:2+tag2Len]
                
                if tag2 == 0x4F:
                    ret += '\t\tApplication AID: %s\n' % tag2Body.hex()
                    
                elif tag2 == 0x50:
                    ret += '\t\tApplication label: %s\n' % tag2Body.encode('utf8')
                    
                elif tag2 == 0x73:                    
                    body2 = tag2Body
                    tokens = []
                    while len(body2) > 2:
                        tag3 = body2[0]
                        tag3Len = body2[1]
                        
                        if tag3 == 0x40:
                            v = body2[2] 
                            if v & 0x80:
                                tokens.append('mutualAuthSymAlgo')
                            if v & 0x40:
                                tokens.append('extAuthSymAlgo')
                            if v & 0x20:
                                tokens.append('keyEstabIntAuthECC')

                        
                        body2 = body2[2+tag3Len:]
                    
                    ret += '\t\tDiscretionary data objects: %s\n' % ",".join(tokens)
                else:
                    ret += '\t\tunhandled tag=0x%x body=%s\n' % (tag2, tag2Body.hex())
                
                body = body[2+tag2Len:]

        return ret

    
    def selectResult(self, selectT, status, body):
        if not len(body):
            return ''
        
        if selectT == 'FCP':
            return self.parseFcp(body)
        elif selectT == 'FCI':
            return self.parseFci(body)
        else:
            return '\tselectResult(%s, %s, %s)\n' % (selectT, status, body.hex())
    
    def getData(self, fileId, bytes):
        lc = bytes[4]
        tag = bytes[5]
        tagLen = bytes[6]        

        if tag == 0x5c:
            doStr = bytes[7:7+tagLen].hex()
            ret = '\tDO=%s\n' % DOs.get(doStr, "<%s>" % doStr)
            self.lastDo = doStr
        else:
            ret = '\tunknown tag=0%x len=%d v=%s' % (tag, tagLen, bytes[7:7+tagLen].hex())

        return ret
    
    def getDataResult(self, status, body):
        ret = ''
        '''
        while len(body) > 2:
            tag = body[0]
            tagLen = body[1]
            
            ret += '\ttag=0x%x len=%d content=%s\n' % (tag, tagLen, body[2:2+tagLen].hex())
            
            body = body[2+tagLen:]
        '''
        return ret
    
    def mse(self, body):
        return body.hex()

    def mseResult(self, status, body):
        return body.hex()
    
    def pso(self, body):
        return body.hex()

    def psoResult(self, status, body):
        return body.hex()



def createAppByAid(aid):
    if aid == "a000000308":
        return ApplicationPIV(aid)
    
    elif aid in ('a00000039742544659',):
        return ApplicationGids(aid)
    
    return ApplicationDummy(aid)


if __name__ == '__main__':
    if len(sys.argv) > 1:
        fin = open(sys.argv[1], "r")
    else:
        fin = sys.stdin
    
    lineno = 0
    lastCmd = 0
    lastSelect = None
    lastSelectFCI = False
    lastGetItem = None
    currentApp = None
    
    for l in fin.readlines():
        lineno += 1
        
        if not len(l):
            continue
        
        # smartcard loggers have changed
        #if l.find("[DEBUG][com.freerdp.channels.smartcard.client]") == -1:
        #    continue

        body = ''
        recvKey = 'pbRecvBuffer: { '
        
        pos = l.find(recvKey)
        if pos != -1:
            toCard = False
            
            pos += len(recvKey)
            pos2 = l.find(' }', pos)
            if pos2 == -1:
                print("line %d: invalid recvBuffer")
                continue
                        
        else:
            toCard = True
            sendKey = 'pbSendBuffer: { '
            pos = l.find(sendKey)
            if pos == -1:
                continue
            pos += len(sendKey)
            
            pos2 = l.find(' }', pos)
            if pos2 == -1:
                print("line %d: invalid sendBuffer")
                continue

        body = l[pos:pos2]
        
        print(l[0:-1])
        bytes = codecs.decode(body, 'hex')
        if toCard:
            (cla, ins, p1, p2) = bytes[0:4]            
            cmdName = CMD_NAMES.get(ins, "<COMMAND 0x%x>" % ins)
            print(cmdName + ":")
            
            if cmdName == "SELECT":
                lc = bytes[4]
                i = 5
                
                if p1 == 0x00:
                    print("\tselectByFID: %0.2x%0.2x" % (bytes[i], bytes[i+1]))
                    i = i + lc
                     
                elif p1 == 0x4:                    
                    aid = bytes[i:i+lc].hex()
                    lastSelect = AIDs.get(aid, '')
                    print("\tselectByAID: %s(%s)" % (aid, lastSelect))
                                        
                    if p2 == 0x00:
                        lastSelectT = "FCI"
                        print('\tFCI')
                    elif p2 == 0x04:
                        print('\tFCP')
                        lastSelectT = "FCP"
                    elif p2 == 0x08:
                        print('\tFMD')
                        lastSelectT = "FMD"
                        
                    if not currentApp or currentApp.getAID() != aid:
                        currentApp = createAppByAid(aid)
                
                    
            elif cmdName == "VERIFY":
                lc = bytes[4]
                P2_DATA_QUALIFIER = {
                    0x00: "Card global password",
                    0x01: "RFU",
                    0x80: "Application password",
                    0x81: "Application resetting password",
                    0x82: "Application security status resetting code",
                }
                
                pin=''
                if lc:
                    pin = ", pin='" + bytes[5:5+lc-2].decode('utf8)') + "'"
                
                print("\t%s%s" % (P2_DATA_QUALIFIER.get(p2, "<unknown>"), pin))

            elif cmdName == "GET DATA":
                lc = bytes[4]
                fileId = p1 * 256 + p2
                
                ret = currentApp.getData(fileId, bytes)
                print("%s" % ret)
                
            elif cmdName == "MSE":
                ret = currentApp.mse(bytes[5:5+lc])
                print("%s" % ret)
                
            elif cmdName == "PSO":
                ret = currentApp.pso(bytes[5:5+lc])
                print("%s" % ret)
            else:
                print('handle %s' % cmdName)    

            lastCmd = cmdName
            
        else:
            # Responses
            status = bytes[-1] + bytes[-2] * 256
            body = bytes[0:-2]
            print("status=0x%0.4x(%s)" %  (status, ERROR_CODES.get(status, "<unknown>")))

            if not len(body):
                continue
            
            ret = ''
            if lastCmd == "SELECT":                
                ret = currentApp.selectResult(lastSelectT, status, body)
            elif lastCmd == "GET DATA":
                ret = currentApp.getDataResult(status, body)                
            elif lastCmd == "MSE":
                ret = currentApp.mseResult(status, body)
            elif lastCmd == "PSO":
                ret = currentApp.psoResult(status, body)
            
            if ret:
                print("%s" % ret)