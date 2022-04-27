#!/usr/bin/env python3

import sys
import getopt
import os.path


def compressTokens(tokens):
    ret = []
    for t in tokens:
        if t:
            ret.append(t)
    return ret

class AsnField(object):
    '''
    '''
    def __init__(self, name, seqIndex, optional, asnType, subType):
        self.name = name
        self.seqIndex = seqIndex
        self.optional = optional
        self.asnType = asnType
        self.asnTypePointer = None
        self.subType = subType
        self.outputType = None
        self.lenFunc = None
        self.writeFunc = None
        self.readFunc = None
        self.cleanupFunc = None
        self.freeFunc = None
        self.options = []

        if asnType.upper() in ['OCTET STRING', 'INTEGER']:
            self.asnTypePointer = self.asnType

    def __str__(self):
        return "{0} [{1}] {2}{3}".format(self.name, self.seqIndex, self.asnType, self.optional and " OPTIONAL" or "")

class AsnSequence(object):
    '''
    '''

    def __init__(self, name):
        self.name = name
        self.fields = []


FIELD_OPTION = 'fieldOption'
CHAR_TO_UNICODE = 'charInMemorySerializeToUnicode'
UNICODE = "unicode"
KNOWN_FIELD_OPTIONS = (CHAR_TO_UNICODE, UNICODE,)

class AsnParser(object):
    KNOWN_OPTIONS = (FIELD_OPTION, 'prefix', )

    STATE_ROOT, STATE_IN_ITEM, STATE_IN_OPTIONS = range(0, 3)

    def __init__(self):
        self.state = AsnParser.STATE_ROOT
        self.defs = {}
        self.currentItem = None
        self.currentName = None
        self.emitArraycode = []

        # options
        self.options = {
            'prefix': '',
            'octetStringLen': {
                'char': 'strlen(item->{fieldName})',
                'WCHAR': '_wcslen(item->{fieldName}) * 2',
                CHAR_TO_UNICODE: 'strlen(item->{fieldName}) * 2',
            }
        }


    def parse(self, content):
        for l in content.split("\n"):
            if l.startswith('#'):
                continue

            tokens = compressTokens(l.lstrip().rstrip().split(' '))

            if self.state == AsnParser.STATE_ROOT:
                if not len(tokens):
                    continue

                if tokens[0] == "%options" and tokens[1] == "{":
                    self.state = AsnParser.STATE_IN_OPTIONS
                    continue

                if tokens[1] != "::=":
                    continue

                if tokens[2] != "SEQUENCE":
                    raise Exception("ERROR: not handling non sequence items for now")

                self.currentName = tokens[0]
                self.currentItem = AsnSequence(tokens[0])
                self.state = AsnParser.STATE_IN_ITEM

            elif self.state == AsnParser.STATE_IN_ITEM:
                if tokens[0] == '}':
                    self.defs[self.currentName] = self.currentItem
                    self.state = AsnParser.STATE_ROOT
                    continue

                optional = tokens[-1] in ["OPTIONAL", "OPTIONAL,"]
                fieldIndex = int(tokens[1][1:-1])

                if optional:
                    typeTokens = tokens[2:-1]
                else:
                    typeTokens = tokens[2:]

                asnType = " ".join(typeTokens)
                if asnType[-1] == ',':
                    asnType = asnType[0:-1]

                subType = None
                if asnType.startswith("SEQUENCE OF"):
                    subType = typeTokens[-1]
                    asnType = "SEQUENCE OF"

                self.currentItem.fields.append(AsnField(tokens[0], fieldIndex, optional, asnType, subType))

            elif self.state == AsnParser.STATE_IN_OPTIONS:
                if not len(tokens) or l.startswith("#"):
                    continue

                if tokens[0] == "}":
                    self.state == AsnParser.STATE_ROOT
                    continue

                option = tokens[0]
                if option not in AsnParser.KNOWN_OPTIONS:
                    raise Exception("unknown option '{0}'".format(option))

                if option == FIELD_OPTION:
                    target = tokens[1]
                    objName, fieldName = target.split(".", 2)

                    obj = self.defs.get(objName, None)
                    if not obj:
                        raise Exception("object type {0} unknown".format(objName))

                    found = False
                    for field in obj.fields:
                        if field.name == fieldName:
                            found = True
                            break
                    if not found:
                        raise Exception("object {0} has no field {1}".format(objName, fieldName))

                    if tokens[2] not in KNOWN_FIELD_OPTIONS:
                        raise Exception("unknown field option {0}".format(objName, tokens[2]))

                    field.options.append(tokens[2])

                elif option == "prefix":
                    self.options['prefix'] = tokens[1]



        # try to resolve custom types in fields
        for typeDef in self.defs.values():
            for field in typeDef.fields:
                if field.asnTypePointer is None:
                    field.asnTypePointer = self.defs.get(field.asnType, None)

                if field.asnType == "SEQUENCE OF":
                    self.emitArraycode.append(field.subType)

        # adjust AsnField fields
        for typeDef in self.defs.values():
            for field in typeDef.fields:
                if field.asnType == "OCTET STRING":
                    fieldType = field.outputType
                    if not fieldType:
                        fieldType = "WCHAR"

                    if CHAR_TO_UNICODE in field.options:
                        fieldType = "char"
                        field.outputType = "char"
                        field.writeFunc = "ber_write_contextual_char_to_unicode_octet_string(s, {fieldIndex}, item->{fieldName})"
                        field.lenFunc = "ber_sizeof_contextual_octet_string(strlen(item->{fieldName}) * 2)"
                        field.readFunc = "ber_read_char_from_unicode_octet_string({stream}, &item->{fieldName})"
                    elif UNICODE in field.options:
                        fieldType = "WCHAR"
                        field.outputType = "WCHAR"
                        field.writeFunc = "ber_write_contextual_unicode_octet_string(s, {fieldIndex}, item->{fieldName})"
                        field.lenFunc = "ber_sizeof_contextual_octet_string(" + self.options['octetStringLen'][fieldType] + ")"
                        field.readFunc = "ber_read_unicode_octet_string({stream}, &item->{fieldName})"
                    else:
                        field.writeFunc = "ber_write_contextual_octet_string(s, {fieldIndex}, item->{fieldName}, item->{fieldName}Len)"
                        field.lenFunc = "ber_sizeof_contextual_octet_string(item->{fieldName}Len)"
                        field.readFunc = "ber_read_octet_string({stream}, &item->{fieldName}, &item->{fieldName}Len)"
                    field.cleanupFunc = "free(item->{fieldName});"


                elif field.asnType == "INTEGER":
                    field.lenFunc = "ber_sizeof_contextual_integer(item->{fieldName})"
                    field.writeFunc = "ber_write_contextual_integer(s, {fieldIndex}, item->{fieldName})"
                    field.readFunc = "ber_read_integer({stream}, &item->{fieldName})"
                    field.cleanupFunc = ""

                elif field.asnType == "SEQUENCE OF":
                    field.lenFunc = "ber_sizeof_contextual_{prefix}{fieldSubType}_array(item->{fieldName}, item->{fieldName}Items)"
                    field.writeFunc = "ber_write_contextual_{prefix}{fieldSubType}_array(s, {fieldIndex}, item->{fieldName}, item->{fieldName}Items)"
                    field.readFunc = "ber_read_{prefix}{fieldSubType}_array({stream}, &item->{fieldName}, &item->{fieldName}Items)"
                    field.cleanupFunc = ""

                else:
                    field.lenFunc = "ber_sizeof_contextual_{prefix}{fieldType}(item->{fieldName})"
                    field.writeFunc = "ber_write_contextual_{prefix}{fieldType}(s, {fieldIndex}, item->{fieldName})"
                    field.readFunc = "ber_read_{prefix}{fieldType}({stream}, &item->{fieldName})"
                    field.cleanupFunc = "{prefix}{fieldType}_free(&item->{fieldName});"

        return True


    def emitStructDefs(self):
        ret = ''
        for defName, seq in self.defs.items():
            h = { 'prefix': self.options['prefix'], 'defName': defName }

            ret += 'typedef struct {\n'
            for field in seq.fields:
                if field.asnType == "INTEGER":
                    ret += "\tUINT32 {fieldName};\n".format(fieldName=field.name)

                elif field.asnType == "OCTET STRING":
                    fieldType = field.outputType
                    if CHAR_TO_UNICODE in field.options:
                        fieldType = 'char'
                    elif fieldType == 'WCHAR':
                        pass
                    else:
                        fieldType = 'BYTE'
                        ret += "\tsize_t {fieldName}Len;\n".format(fieldName=field.name, fieldType=fieldType)

                    ret += "\t{fieldType}* {fieldName};\n".format(fieldName=field.name, fieldType=fieldType)

                elif field.asnType == "SEQUENCE OF":
                    ret += "\tsize_t {fieldName}Items;\n".format(fieldName=field.name, fieldType=field.subType)
                    ret += "\t{fieldType}_t* {fieldName};\n".format(fieldName=field.name, fieldType=field.subType)
                else:
                    ret += "\t{typeName}_t* {fieldName};\n".format(fieldName=field.name, typeName=field.asnType)
            ret += '}} {defName}_t;\n\n'.format(**h)

        return ret


    def emitPrototypes(self):
        ret = ''
        for defName, seq in self.defs.items():
            h = { 'prefix': self.options['prefix'], 'defName': defName }

            ret += "size_t ber_sizeof_{prefix}{defName}_content(const {defName}_t* item);\n".format(**h)
            ret += "size_t ber_sizeof_{prefix}{defName}(const {defName}_t* item);\n".format(**h)
            ret += "size_t ber_sizeof_contextual_{prefix}{defName}(const {defName}_t* item);\n".format(**h)
            ret += "void {prefix}{defName}_free({defName}_t** pitem);\n".format(**h)
            ret += "size_t ber_write_{prefix}{defName}(wStream *s, const {defName}_t* item);\n".format(**h)
            ret += "size_t ber_write_contextual_{prefix}{defName}(wStream *s, BYTE tag, const {defName}_t* item);\n".format(**h)
            ret += 'BOOL ber_read_{prefix}{defName}(wStream *s, {defName}_t** pret);\n'.format(**h)

            if defName in self.emitArraycode:
                ret += "size_t ber_sizeof_{prefix}{defName}_array_content(const {defName}_t* item, size_t nitems);\n".format(**h)
                ret += "size_t ber_sizeof_{prefix}{defName}_array(const {defName}_t* item, size_t nitems);\n".format(**h)
                ret += "size_t ber_sizeof_contextual_{prefix}{defName}_array(const {defName}_t* item, size_t nitems);\n".format(**h)
                ret += "size_t ber_write_{prefix}{defName}_array(wStream* s, const {defName}_t* item, size_t nitems);\n".format(**h)
                ret += "size_t ber_write_contextual_{prefix}{defName}_array(wStream* s, BYTE tag, const {defName}_t* item, size_t nitems);\n".format(**h)
                ret += "BOOL ber_read_{prefix}{defName}_array(wStream* s, {defName}_t** item, size_t* nitems);\n".format(**h)
            ret += '\n'

        return ret


    def emitImpl(self):
        ret = ''
        for defName, seq in self.defs.items():
            h = { 'prefix': self.options['prefix'], 'defName': defName }

            # ================= ber_sizeof_ =========================================
            ret += "size_t ber_sizeof_{prefix}{defName}_content(const {defName}_t* item) {{\n".format(**h)
            ret += "\tsize_t ret = 0;\n\n"

            for field in seq.fields:
                h2 = {'fieldName': field.name, 'fieldIndex': field.seqIndex, 'fieldType':field.asnType,
                      'fieldSubType': field.subType }
                shift = '\t'

                ret += shift + "/* [{fieldIndex}] {fieldName} ({fieldType}){optional}*/\n".format(**h2, optional=field.optional and " OPTIONAL" or "")
                if field.optional:
                    ret += shift + "if (item->{fieldName}) {{\n".format(fieldName=field.name)
                    shift = '\t\t'

                ret += shift + "ret += " + field.lenFunc.format(**h2, **h) + ";\n"

                if field.optional:
                    ret += "\t}\n"
                ret += '\n'

            ret += '\treturn ret;\n'
            ret += '}\n\n'

            ret += '''size_t ber_sizeof_{prefix}{defName}(const {defName}_t* item)
{{
    size_t ret = ber_sizeof_{prefix}{defName}_content(item);
    return ber_sizeof_sequence(ret);
}}
'''.format(**h)

            ret += "size_t ber_sizeof_contextual_{prefix}{defName}(const {defName}_t* item) {{\n".format(**h)
            ret += "\tsize_t innerSz = ber_sizeof_{prefix}{defName}(item);\n".format(**h)
            ret += "\treturn ber_sizeof_contextual_tag(innerSz) + innerSz;\n"
            ret += "}\n\n"


            # ================= free_ =========================================
            ret += "void {prefix}{defName}_free({defName}_t** pitem) {{\n".format(**h)
            ret += "\t{defName}_t* item;\n\n".format(**h)
            ret += "\tWINPR_ASSERT(pitem);\n"
            ret += "\titem = *pitem;\n"
            ret += "\tif (!item)\n"
            ret += "\t\treturn;\n\n"

            for field in seq.fields:
                if field.cleanupFunc:
                    h2 = { 'fieldName': field.name, 'fieldIndex': field.seqIndex, 'fieldType':field.asnType }
                    ret += "\t" + field.cleanupFunc.format(**h2, **h) + "\n"
            ret += "\tfree(item);\n"
            ret += "\t*pitem = NULL;\n"
            ret += "}\n\n"

            # ================= ber_write_ =========================================
            ret += '''size_t ber_write_{prefix}{defName}(wStream *s, const {defName}_t* item)
{{
    size_t content_size = ber_sizeof_{prefix}{defName}_content(item);
    size_t ret = 0;

    ret = ber_write_sequence_tag(s, content_size);
'''.format(**h)

            for field in seq.fields:
                h2 = { 'fieldName': field.name, 'fieldIndex': field.seqIndex, 'fieldType':field.asnType,
                      'fieldSubType': field.subType }
                shift = "    "

                ret += shift + "/* [{fieldIndex}] {fieldName} ({fieldType}){optional} */\n".format(**h2, optional=field.optional and " OPTIONAL" or "")
                if field.optional:
                    ret += shift + "if (item->{fieldName}) {{\n" .format(**h2)
                    shift += '    '

                ret += shift + "if (!" + field.writeFunc.format(**h2, **h) + ")\n"
                ret += shift + '    return 0;\n'

                if field.optional:
                    ret += "    }\n"
                ret += "\n"

            ret += '    return ret + content_size;\n'
            ret += '}\n\n'

            ret += '''size_t ber_write_contextual_{prefix}{defName}(wStream *s, BYTE tag, const {defName}_t* item)
{{
    size_t ret;
    size_t inner = ber_sizeof_{prefix}{defName}(item);

    ret = ber_write_contextual_tag(s, tag, inner, TRUE);
    ber_write_{prefix}{defName}(s, item);
    return ret + inner;
}}

'''.format(**h)


            # ================= ber_read_ =========================================
            ret += '''BOOL ber_read_{prefix}{defName}(wStream *s, {defName}_t** pret) {{
    wStream seqstream;
    size_t seqLength;
    size_t inner_size;
    wStream fieldStream;
    {defName}_t* item;
    BOOL ret;

    if (!ber_read_sequence_tag(s, &seqLength) || !Stream_CheckAndLogRequiredLength(TAG,s, seqLength))
        return FALSE;
    Stream_StaticInit(&seqstream, Stream_Pointer(s), seqLength);

    item = calloc(1, sizeof(*item));
    if (!item)
        return FALSE;

'''.format(**h)
            shiftLevel = 1
            shift = ' ' * 4 * shiftLevel

            cleanupLabels = []
            for field in seq.fields:
                h2 = { 'fieldName': field.name, 'fieldIndex': field.seqIndex, 'fieldType':field.asnType,
                      'stream': '&fieldStream', 'fieldSubType': field.subType }

                cleanupLabels.insert(0, "\t" + field.cleanupFunc.format(**h2, **h))
                cleanupLabels.insert(1, "out_fail_{fieldName}:".format(**h2, **h))

                ret += shift + "/* [{fieldIndex}] {fieldName} ({fieldType}){optional} */\n".format(**h2, optional=field.optional and " OPTIONAL" or "")
                ret += shift + 'ret = ber_read_contextual_tag(&seqstream, {fieldIndex}, &inner_size, TRUE);\n'.format(**h2)

                if not field.optional:
                    ret += shift + "if (!ret) \n"
                    ret += shift + '\tgoto out_fail_{fieldName};\n'.format(**h2)
                else:
                    ret += shift + "if (ret) { \n"
                    shiftLevel += 1
                    shift = ' ' * 4 * shiftLevel

                ret += shift + "Stream_StaticInit(&fieldStream, Stream_Pointer(&seqstream), inner_size);\n"
                ret += shift + "Stream_Seek(&seqstream, inner_size);\n"
                ret += '\n'
                ret += shift + "ret = " + field.readFunc.format(**h2, **h) + ";\n"
                ret += shift + "if (!ret)\n"
                ret += shift + '\tgoto out_fail_{fieldName};\n'.format(**h2)

                if field.optional:
                    shiftLevel -= 1
                    shift = ' ' * 4 * shiftLevel
                    ret += shift + '}'

                ret += '\n'

            ret += shift + "*pret = item;\n"
            ret += shift + "return TRUE;\n"
            ret += '\n'

            cleanupLabels = cleanupLabels[1:]
            ret += "\n".join(cleanupLabels)

            ret += '\n'
            ret += shift + 'free(item);\n'
            ret += shift + 'return FALSE;\n'
            ret += '}\n\n'

            # ====================== code for handling arrays ====================================
            if defName in self.emitArraycode:
                ret += '''size_t ber_sizeof_{prefix}{defName}_array_content(const {defName}_t* item, size_t nitems)
{{
    size_t i, ret = 0;
    for (i = 0; i < nitems; i++, item++)
        ret += ber_sizeof_{prefix}{defName}(item);

    return ber_sizeof_sequence(ret);
}}

size_t ber_sizeof_{prefix}{defName}_array(const {defName}_t* item, size_t nitems)
{{
    return ber_sizeof_sequence( ber_sizeof_{prefix}{defName}_array_content(item, nitems) );
}}

size_t ber_sizeof_contextual_{prefix}{defName}_array(const {defName}_t* item, size_t nitems)
{{
    size_t inner = ber_sizeof_{prefix}{defName}_array(item, nitems);
    return ber_sizeof_contextual_tag(inner) + inner;
}}

size_t ber_write_{prefix}{defName}_array(wStream* s, const {defName}_t* item, size_t nitems)
{{
    size_t i, r, ret;
    size_t inner_len = ber_sizeof_{prefix}{defName}_array_content(item, nitems);

    ret = ber_write_sequence_tag(s, inner_len);

    for (i = 0; i < nitems; i++, item++)
    {{
        r = ber_write_{prefix}{defName}(s, item);
        if (!r)
            return 0;
        ret += r;
    }}

    return ret;
}}

size_t ber_write_contextual_{prefix}{defName}_array(wStream* s, BYTE tag, const {defName}_t* item, size_t nitems)
{{
    size_t ret;
    size_t inner = ber_sizeof_{prefix}{defName}_array(item, nitems);

    ret = ber_write_contextual_tag(s, tag, inner, TRUE);
    ber_write_{prefix}{defName}_array(s, item, nitems);
    return ret + inner;
}}



BOOL ber_read_{prefix}{defName}_array(wStream* s, {defName}_t** pitems, size_t* nitems)
{{
    size_t subLen;
    wStream subStream;
    {defName}_t* retItems = NULL;
    size_t ret = 0;

    if (!ber_read_sequence_tag(s, &subLen) || !Stream_CheckAndLogRequiredLength(TAG,s, subLen))
        return FALSE;

    Stream_StaticInit(&subStream, Stream_Pointer(s), subLen);
    while (Stream_GetRemainingLength(&subStream))
    {{
        {defName}_t *item;
        {defName}_t* tmpRet;

        if (!ber_read_{prefix}{defName}(&subStream, &item))
        {{
            free(retItems);
            return FALSE;
        }}

        tmpRet = realloc(retItems, (ret+1) * sizeof({defName}_t));
        if (!tmpRet)
        {{
            free(retItems);
            return FALSE;
        }}
        retItems = tmpRet;

        memcpy(&retItems[ret], item, sizeof(*item));
        free(item);
        ret++;
    }}

    *pitems = retItems;
    *nitems = ret;
    return TRUE;
}}

'''.format(**h)


        return ret


if __name__ == '__main__':
    opts, extraArgs = getopt.getopt(sys.argv[1:], "hi:o:t:", ['input=', 'output=', 'output-kind=', 'help'])


    ALLOWED_OUTPUTS = ('headers', 'impls',)

    inputStream = sys.stdin
    outputStream = sys.stdout
    outputs = ALLOWED_OUTPUTS
    inputHeaderName = "ASN1_HEADER_H"
    headerName = "tscredentials"

    for option, value in opts:
        if option in ('-h', '--help',):
            print("usage: {0} [-i|--input]=<file> [-o|--output]=<file>")
            print("\t[-i|--input] <file>: input file")
            print("\t[-o|--output] <file>: output file")
            print("\t[-t|--output-kind] [header|impl]: the kind of output")
        elif option in ('-o', '--output',):
            outputStream = open(value, "w")

            # libfreerdp/core/credssp.c => credssp.h
            headerName = os.path.splitext(os.path.basename(value))[0]
        elif option in ('-i', '--input',):
            inputStream = open(value, "r")

            # libfreerdp/core/credssp.asn1 =>  LIBFREERDP_CORE_CREDSSP_ASN1_H
            inputHeaderName = os.path.normpath(value).replace(os.path.sep, '_').replace('.', '_').upper() + '_H'

        elif option in ('-t', '--output-kind',):
            if value not in ALLOWED_OUTPUTS:
                raise Exception("unknown output kind '{0}'".format(value))
            outputs = value
        else:
            raise Exception("unknown option {0}".format(option))

    input = inputStream.read()

    parser = AsnParser()
    parser.parse(input)

    h = {'programName': os.path.basename(sys.argv[0]), 'cmdLine': ' '.join(sys.argv), 'programPath': sys.argv[0],
         'inputHeaderName': inputHeaderName, 'headerName': headerName }

    outputStream.write('''/* ============================================================================================================
 * this file has been generated using
 * {cmdLine}
 *
 * /!\\ If you want to modify this file you'd probably better change {programName} or the corresponding ASN1
 *     definition file
 *
 * ============================================================================================================
 */
'''.format(**h))

    if outputs == 'headers':
        outputStream.write('''#ifndef {inputHeaderName}
#define {inputHeaderName}

#include <winpr/stream.h>

'''.format(**h))
        outputStream.write(parser.emitStructDefs())
        outputStream.write(parser.emitPrototypes())
        outputStream.write('#endif /* {inputHeaderName} */\n'.format(**h))

    elif outputs == "impls":
        outputStream.write('''
#include <winpr/string.h>
#include <freerdp/crypto/ber.h>

#include "{headerName}.h"

#include <freerdp/log.h>

#define TAG FREERDP_TAG("core.{headerName}")

'''.format(**h))
        outputStream.write(parser.emitImpl())

