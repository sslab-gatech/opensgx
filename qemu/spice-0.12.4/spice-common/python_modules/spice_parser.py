try:
    from pyparsing import Literal, CaselessLiteral, Word, OneOrMore, ZeroOrMore, \
            Forward, delimitedList, Group, Optional, Combine, alphas, nums, restOfLine, cStyleComment, \
            alphanums, ParseException, ParseResults, Keyword, StringEnd, replaceWith
except ImportError:
    print "Module pyparsing not found."
    exit(1)


import ptypes
import sys

cvtInt = lambda toks: int(toks[0])

def parseVariableDef(toks):
    t = toks[0][0]
    pointer = toks[0][1]
    name = toks[0][2]
    array_size = toks[0][3]
    attributes = toks[0][4]

    if array_size != None:
        t = ptypes.ArrayType(t, array_size)

    if pointer != None:
        t = ptypes.PointerType(t)

    return ptypes.Member(name, t, attributes)

bnf = None
def SPICE_BNF():
    global bnf

    if not bnf:

        # punctuation
        colon  = Literal(":").suppress()
        lbrace = Literal("{").suppress()
        rbrace = Literal("}").suppress()
        lbrack = Literal("[").suppress()
        rbrack = Literal("]").suppress()
        lparen = Literal("(").suppress()
        rparen = Literal(")").suppress()
        equals = Literal("=").suppress()
        comma  = Literal(",").suppress()
        semi   = Literal(";").suppress()

        # primitive types
        int8_      = Keyword("int8").setParseAction(replaceWith(ptypes.int8))
        uint8_     = Keyword("uint8").setParseAction(replaceWith(ptypes.uint8))
        int16_     = Keyword("int16").setParseAction(replaceWith(ptypes.int16))
        uint16_    = Keyword("uint16").setParseAction(replaceWith(ptypes.uint16))
        int32_     = Keyword("int32").setParseAction(replaceWith(ptypes.int32))
        uint32_    = Keyword("uint32").setParseAction(replaceWith(ptypes.uint32))
        int64_     = Keyword("int64").setParseAction(replaceWith(ptypes.int64))
        uint64_    = Keyword("uint64").setParseAction(replaceWith(ptypes.uint64))

        # keywords
        channel_   = Keyword("channel")
        enum32_    = Keyword("enum32").setParseAction(replaceWith(32))
        enum16_    = Keyword("enum16").setParseAction(replaceWith(16))
        enum8_     = Keyword("enum8").setParseAction(replaceWith(8))
        flags32_   = Keyword("flags32").setParseAction(replaceWith(32))
        flags16_   = Keyword("flags16").setParseAction(replaceWith(16))
        flags8_    = Keyword("flags8").setParseAction(replaceWith(8))
        channel_   = Keyword("channel")
        server_    = Keyword("server")
        client_    = Keyword("client")
        protocol_  = Keyword("protocol")
        typedef_   = Keyword("typedef")
        struct_    = Keyword("struct")
        message_   = Keyword("message")
        image_size_ = Keyword("image_size")
        bytes_     = Keyword("bytes")
        cstring_   = Keyword("cstring")
        switch_    = Keyword("switch")
        default_   = Keyword("default")
        case_      = Keyword("case")

        identifier = Word( alphas, alphanums + "_" )
        enumname = Word( alphanums + "_" )

        integer = ( Combine( CaselessLiteral("0x") + Word( nums+"abcdefABCDEF" ) ) |
                    Word( nums+"+-", nums ) ).setName("int").setParseAction(cvtInt)

        typename = identifier.copy().setParseAction(lambda toks : ptypes.TypeRef(str(toks[0])))

        # This is just normal "types", i.e. not channels or messages
        typeSpec = Forward()

        attributeValue = integer ^ identifier
        attribute = Group(Combine ("@" + identifier) + Optional(lparen + delimitedList(attributeValue) + rparen))
        attributes = Group(ZeroOrMore(attribute))
        arraySizeSpecImage = Group(image_size_ + lparen + integer + comma + identifier + comma + identifier + rparen)
        arraySizeSpecBytes = Group(bytes_ + lparen + identifier + comma + identifier + rparen)
        arraySizeSpecCString = Group(cstring_ + lparen + rparen)
        arraySizeSpec = lbrack + Optional(identifier ^ integer ^ arraySizeSpecImage ^ arraySizeSpecBytes ^arraySizeSpecCString, default="") + rbrack
        variableDef = Group(typeSpec + Optional("*", default=None) + identifier + Optional(arraySizeSpec, default=None) + attributes - semi) \
            .setParseAction(parseVariableDef)

        switchCase = Group(Group(OneOrMore(default_.setParseAction(replaceWith(None)) + colon | Group(case_.suppress() + Optional("!", default="") + identifier) + colon)) + variableDef) \
            .setParseAction(lambda toks: ptypes.SwitchCase(toks[0][0], toks[0][1]))
        switchBody = Group(switch_ + lparen + delimitedList(identifier,delim='.', combine=True) + rparen + lbrace + Group(OneOrMore(switchCase)) + rbrace + identifier + attributes - semi) \
            .setParseAction(lambda toks: ptypes.Switch(toks[0][1], toks[0][2], toks[0][3], toks[0][4]))
        messageBody = structBody = Group(lbrace + ZeroOrMore(variableDef | switchBody)  + rbrace)
        structSpec = Group(struct_ + identifier + structBody + attributes).setParseAction(lambda toks: ptypes.StructType(toks[0][1], toks[0][2], toks[0][3]))

        # have to use longest match for type, in case a user-defined type name starts with a keyword type, like "channel_type"
        typeSpec << ( structSpec ^ int8_ ^ uint8_ ^ int16_ ^ uint16_ ^
                     int32_ ^ uint32_ ^ int64_ ^ uint64_ ^
                     typename).setName("type")

        flagsBody = enumBody = Group(lbrace + delimitedList(Group (enumname + Optional(equals + integer))) + Optional(comma) + rbrace)

        messageSpec = Group(message_ + messageBody + attributes).setParseAction(lambda toks: ptypes.MessageType(None, toks[0][1], toks[0][2])) | typename

        channelParent = Optional(colon + typename, default=None)
        channelMessage = Group(messageSpec + identifier + Optional(equals + integer, default=None) + semi) \
            .setParseAction(lambda toks: ptypes.ChannelMember(toks[0][1], toks[0][0], toks[0][2]))
        channelBody = channelParent + Group(lbrace + ZeroOrMore( server_ + colon | client_ + colon | channelMessage)  + rbrace)

        enum_ = (enum32_ | enum16_ | enum8_)
        flags_ = (flags32_ | flags16_ | flags8_)
        enumDef = Group(enum_ + identifier + enumBody + attributes - semi).setParseAction(lambda toks: ptypes.EnumType(toks[0][0], toks[0][1], toks[0][2], toks[0][3]))
        flagsDef = Group(flags_ + identifier + flagsBody + attributes  - semi).setParseAction(lambda toks: ptypes.FlagsType(toks[0][0], toks[0][1], toks[0][2], toks[0][3]))
        messageDef = Group(message_ + identifier + messageBody + attributes - semi).setParseAction(lambda toks: ptypes.MessageType(toks[0][1], toks[0][2], toks[0][3]))
        channelDef = Group(channel_ + identifier + channelBody + attributes - semi).setParseAction(lambda toks: ptypes.ChannelType(toks[0][1], toks[0][2], toks[0][3], toks[0][4]))
        structDef = Group(struct_ + identifier + structBody + attributes - semi).setParseAction(lambda toks: ptypes.StructType(toks[0][1], toks[0][2], toks[0][3]))
        typedefDef = Group(typedef_ + identifier  + typeSpec + attributes - semi).setParseAction(lambda toks: ptypes.TypeAlias(toks[0][1], toks[0][2], toks[0][3]))

        definitions = typedefDef | structDef | enumDef | flagsDef | messageDef | channelDef

        protocolChannel = Group(typename + identifier +  Optional(equals + integer, default=None) + semi) \
            .setParseAction(lambda toks: ptypes.ProtocolMember(toks[0][1], toks[0][0], toks[0][2]))
        protocolDef = Group(protocol_ + identifier + Group(lbrace + ZeroOrMore(protocolChannel) + rbrace) + semi) \
            .setParseAction(lambda toks: ptypes.ProtocolType(toks[0][1], toks[0][2]))

        bnf = ZeroOrMore (definitions) +  protocolDef + StringEnd()

        singleLineComment = "//" + restOfLine
        bnf.ignore( singleLineComment )
        bnf.ignore( cStyleComment )

    return bnf


def parse(filename):
    try:
        bnf = SPICE_BNF()
        types = bnf.parseFile(filename)
    except ParseException, err:
        print >> sys.stderr, err.line
        print >> sys.stderr, " "*(err.column-1) + "^"
        print >> sys.stderr, err
        return None

    for t in types:
        t.resolve()
        t.register()
    protocol = types[-1]
    return protocol
