import codegen
import types

_types_by_name = {}
_types = []

default_pointer_size = 4

def type_exists(name):
    return _types_by_name.has_key(name)

def lookup_type(name):
    return _types_by_name[name]

def get_named_types():
    return _types

class FixedSize:
    def __init__(self, val = 0, minor = 0):
        if isinstance(val, FixedSize):
            self.vals = val.vals
        else:
            self.vals = [0] * (minor + 1)
            self.vals[minor] = val

    def __add__(self, other):
        if isinstance(other, types.IntType):
            other = FixedSize(other)

        new = FixedSize()
        l = max(len(self.vals), len(other.vals))
        shared = min(len(self.vals), len(other.vals))

        new.vals = [0] * l

        for i in range(shared):
            new.vals[i] = self.vals[i] + other.vals[i]

        for i in range(shared,len(self.vals)):
            new.vals[i] = self.vals[i]

        for i in range(shared,len(other.vals)):
            new.vals[i] = new.vals[i] + other.vals[i]

        return new

    def __radd__(self, other):
        return self.__add__(other)

    def __str__(self):
        s = "%d" % (self.vals[0])

        for i in range(1,len(self.vals)):
            if self.vals[i] > 0:
                s = s + " + ((minor >= %d)?%d:0)" % (i, self.vals[i])
        return s

# Some attribute are propagated from member to the type as they really
# are part of the type definition, rather than the member. This applies
# only to attributes that affect pointer or array attributes, as these
# are member local types, unlike e.g. a Struct that may be used by
# other members
propagated_attributes=["ptr_array", "nonnull", "chunk"]

class Type:
    def __init__(self):
        self.attributes = {}
        self.registred = False
        self.name = None

    def has_name(self):
        return self.name != None

    def get_type(self, recursive=False):
        return self

    def is_primitive(self):
        return False

    def is_fixed_sizeof(self):
        return True

    def is_extra_size(self):
        return False

    def contains_extra_size(self):
        return False

    def is_fixed_nw_size(self):
        return True

    def is_array(self):
        return isinstance(self, ArrayType)

    def contains_member(self, member):
        return False

    def is_struct(self):
        return isinstance(self, StructType)

    def is_pointer(self):
        return isinstance(self, PointerType)

    def get_num_pointers(self):
        return 0

    def get_pointer_names(self, marshalled):
        return []

    def sizeof(self):
        return "sizeof(%s)" % (self.c_type())

    def __repr__(self):
        return self.__str__()

    def __str__(self):
        if self.name != None:
            return self.name
        return "anonymous type"

    def resolve(self):
        return self

    def register(self):
        if self.registred or self.name == None:
            return
        self.registred = True
        if _types_by_name.has_key(self.name):
            raise Exception, "Type %s already defined" % self.name
        _types.append(self)
        _types_by_name[self.name] = self

    def has_attr(self, name):
        return self.attributes.has_key(name)

class TypeRef(Type):
    def __init__(self, name):
        Type.__init__(self)
        self.name = name

    def __str__(self):
        return "ref to %s" % (self.name)

    def resolve(self):
        if not _types_by_name.has_key(self.name):
            raise Exception, "Unknown type %s" % self.name
        return _types_by_name[self.name]

    def register(self):
        assert True, "Can't register TypeRef!"


class IntegerType(Type):
    def __init__(self, bits, signed):
        Type.__init__(self)
        self.bits = bits
        self.signed = signed

        if signed:
            self.name = "int%d" % bits
        else:
            self.name = "uint%d" % bits

    def primitive_type(self):
        return self.name

    def c_type(self):
        return self.name + "_t"

    def get_fixed_nw_size(self):
        return self.bits / 8

    def is_primitive(self):
        return True

class TypeAlias(Type):
    def __init__(self, name, the_type, attribute_list):
        Type.__init__(self)
        self.name = name
        self.the_type = the_type
        for attr in attribute_list:
            self.attributes[attr[0][1:]] = attr[1:]

    def get_type(self, recursive=False):
        if recursive:
            return self.the_type.get_type(True)
        else:
            return self.the_type

    def primitive_type(self):
        return self.the_type.primitive_type()

    def resolve(self):
        self.the_type = self.the_type.resolve()
        return self

    def __str__(self):
        return "alias %s" % self.name

    def is_primitive(self):
        return self.the_type.is_primitive()

    def is_fixed_sizeof(self):
        return self.the_type.is_fixed_sizeof()

    def is_fixed_nw_size(self):
        return self.the_type.is_fixed_nw_size()

    def get_fixed_nw_size(self):
        return self.the_type.get_fixed_nw_size()

    def get_num_pointers(self):
        return self.the_type.get_num_pointers()

    def get_pointer_names(self, marshalled):
        return self.the_type.get_pointer_names(marshalled)

    def c_type(self):
        if self.has_attr("ctype"):
            return self.attributes["ctype"][0]
        return self.name

class EnumBaseType(Type):
    def is_enum(self):
        return isinstance(self, EnumType)

    def primitive_type(self):
        return "uint%d" % (self.bits)

    def c_type(self):
        return "uint%d_t" % (self.bits)

    def c_name(self):
        return codegen.prefix_camel(self.name)

    def c_enumname(self, value):
        return self.c_enumname_by_name(self.names[value])

    def c_enumname_by_name(self, name):
        if self.has_attr("prefix"):
            return self.attributes["prefix"][0] + name
        return codegen.prefix_underscore_upper(self.name.upper(), name)

    def is_primitive(self):
        return True

    def get_fixed_nw_size(self):
        return self.bits / 8

class EnumType(EnumBaseType):
    def __init__(self, bits, name, enums, attribute_list):
        Type.__init__(self)
        self.bits = bits
        self.name = name

        last = -1
        names = {}
        values = {}
        for v in enums:
            name = v[0]
            if len(v) > 1:
                value = v[1]
            else:
                value = last + 1
            last = value

            assert not names.has_key(value)
            names[value] = name
            values[name] = value

        self.names = names
        self.values = values

        for attr in attribute_list:
            self.attributes[attr[0][1:]] = attr[1:]

    def __str__(self):
        return "enum %s" % self.name

    def c_define(self, writer):
        writer.write("typedef enum ")
        writer.write(self.c_name())
        writer.begin_block()
        values = self.names.keys()
        values.sort()
        current_default = 0
        for i in values:
            writer.write(self.c_enumname(i))
            if i != current_default:
                writer.write(" = %d" % (i))
            writer.write(",")
            writer.newline()
            current_default = i + 1
        writer.newline()
        writer.write(codegen.prefix_underscore_upper(self.name.upper(), "ENUM_END"))
        writer.newline()
        writer.end_block(newline=False)
        writer.write(" ")
        writer.write(self.c_name())
        writer.write(";")
        writer.newline()
        writer.newline()

class FlagsType(EnumBaseType):
    def __init__(self, bits, name, flags, attribute_list):
        Type.__init__(self)
        self.bits = bits
        self.name = name

        last = -1
        names = {}
        values = {}
        for v in flags:
            name = v[0]
            if len(v) > 1:
                value = v[1]
            else:
                value = last + 1
            last = value

            assert not names.has_key(value)
            names[value] = name
            values[name] = value

        self.names = names
        self.values = values

        for attr in attribute_list:
            self.attributes[attr[0][1:]] = attr[1:]

    def __str__(self):
        return "flags %s" % self.name

    def c_define(self, writer):
        writer.write("typedef enum ")
        writer.write(self.c_name())
        writer.begin_block()
        values = self.names.keys()
        values.sort()
        mask = 0
        for i in values:
            writer.write(self.c_enumname(i))
            mask = mask |  (1<<i)
            writer.write(" = (1 << %d)" % (i))
            writer.write(",")
            writer.newline()
            current_default = i + 1
        writer.newline()
        writer.write(codegen.prefix_underscore_upper(self.name.upper(), "MASK"))
        writer.write(" = 0x%x" % (mask))
        writer.newline()
        writer.end_block(newline=False)
        writer.write(" ")
        writer.write(self.c_name())
        writer.write(";")
        writer.newline()
        writer.newline()

class ArrayType(Type):
    def __init__(self, element_type, size):
        Type.__init__(self)
        self.name = None

        self.element_type = element_type
        self.size = size

    def __str__(self):
        if self.size == None:
            return "%s[]" % (str(self.element_type))
        else:
            return "%s[%s]" % (str(self.element_type), str(self.size))

    def resolve(self):
        self.element_type = self.element_type.resolve()
        return self

    def is_constant_length(self):
        return isinstance(self.size, types.IntType)

    def is_remaining_length(self):
        return isinstance(self.size, types.StringType) and len(self.size) == 0

    def is_identifier_length(self):
        return isinstance(self.size, types.StringType) and len(self.size) > 0

    def is_image_size_length(self):
        if isinstance(self.size, types.IntType) or isinstance(self.size, types.StringType):
            return False
        return self.size[0] == "image_size"

    def is_bytes_length(self):
        if isinstance(self.size, types.IntType) or isinstance(self.size, types.StringType):
            return False
        return self.size[0] == "bytes"

    def is_cstring_length(self):
        if isinstance(self.size, types.IntType) or isinstance(self.size, types.StringType):
            return False
        return self.size[0] == "cstring"

    def is_fixed_sizeof(self):
        return self.is_constant_length() and self.element_type.is_fixed_sizeof()

    def is_fixed_nw_size(self):
        return self.is_constant_length() and self.element_type.is_fixed_nw_size()

    def get_fixed_nw_size(self):
        if not self.is_fixed_nw_size():
            raise Exception, "Not a fixed size type"

        return self.element_type.get_fixed_nw_size() * self.size

    def get_num_pointers(self):
        element_count = self.element_type.get_num_pointers()
        if element_count  == 0:
            return 0
        if self.is_constant_length(self):
            return element_count * self.size
        raise Exception, "Pointers in dynamic arrays not supported"

    def get_pointer_names(self, marshalled):
        element_count = self.element_type.get_num_pointers()
        if element_count  == 0:
            return []
        raise Exception, "Pointer names in arrays not supported"

    def is_extra_size(self):
        return self.has_attr("ptr_array")

    def contains_extra_size(self):
        return self.element_type.contains_extra_size() or self.has_attr("chunk")

    def sizeof(self):
        return "%s * %s" % (self.element_type.sizeof(), self.size)

    def c_type(self):
        return self.element_type.c_type()

class PointerType(Type):
    def __init__(self, target_type):
        Type.__init__(self)
        self.name = None
        self.target_type = target_type
        self.pointer_size = default_pointer_size

    def __str__(self):
        return "%s*" % (str(self.target_type))

    def resolve(self):
        self.target_type = self.target_type.resolve()
        return self

    def set_ptr_size(self, new_size):
        self.pointer_size = new_size

    def is_fixed_nw_size(self):
        return True

    def is_primitive(self):
        return True

    def primitive_type(self):
        if self.pointer_size == 4:
            return "uint32"
        else:
            return "uint64"

    def get_fixed_nw_size(self):
        return self.pointer_size

    def c_type(self):
        if self.pointer_size == 4:
            return "uint32_t"
        else:
            return "uint64_t"

    def contains_extra_size(self):
        return True

    def get_num_pointers(self):
        return 1

class Containee:
    def __init__(self):
        self.attributes = {}

    def is_switch(self):
        return False

    def is_pointer(self):
        return not self.is_switch() and self.member_type.is_pointer()

    def is_array(self):
        return not self.is_switch() and self.member_type.is_array()

    def is_struct(self):
        return not self.is_switch() and self.member_type.is_struct()

    def is_primitive(self):
        return not self.is_switch() and self.member_type.is_primitive()

    def has_attr(self, name):
        return self.attributes.has_key(name)

    def has_minor_attr(self):
        return self.has_attr("minor")

    def has_end_attr(self):
        return self.has_attr("end")

    def get_minor_attr(self):
        return self.attributes["minor"][0]

class Member(Containee):
    def __init__(self, name, member_type, attribute_list):
        Containee.__init__(self)
        self.name = name
        self.member_type = member_type
        for attr in attribute_list:
            self.attributes[attr[0][1:]] = attr[1:]

    def resolve(self, container):
        self.container = container
        self.member_type = self.member_type.resolve()
        self.member_type.register()
        if self.has_attr("ptr32") and self.member_type.is_pointer():
            self.member_type.set_ptr_size(4)
        for i in propagated_attributes:
            if self.has_attr(i):
                self.member_type.attributes[i] = self.attributes[i]
        return self

    def contains_member(self, member):
        return self.member_type.contains_member(member)

    def is_primitive(self):
        return self.member_type.is_primitive()

    def is_fixed_sizeof(self):
        if self.has_end_attr():
            return False
        return self.member_type.is_fixed_sizeof()

    def is_extra_size(self):
        return self.has_end_attr() or self.has_attr("to_ptr") or self.member_type.is_extra_size()

    def is_fixed_nw_size(self):
        if self.has_attr("virtual"):
            return True
        return self.member_type.is_fixed_nw_size()

    def get_fixed_nw_size(self):
        if self.has_attr("virtual"):
            return 0
        size = self.member_type.get_fixed_nw_size()
        if self.has_minor_attr():
            minor = self.get_minor_attr()
            size = FixedSize(size, minor)
        return size

    def contains_extra_size(self):
        return self.member_type.contains_extra_size()

    def sizeof(self):
        return self.member_type.sizeof()

    def __repr__(self):
        return "%s (%s)" % (str(self.name), str(self.member_type))

    def get_num_pointers(self):
        if self.has_attr("to_ptr"):
            return 1
        return self.member_type.get_num_pointers()

    def get_pointer_names(self, marshalled):
        if self.member_type.is_pointer():
            if self.has_attr("marshall") == marshalled:
                names = [self.name]
            else:
                names = []
        else:
            names = self.member_type.get_pointer_names(marshalled)
        if self.has_attr("outvar"):
            prefix = self.attributes["outvar"][0]
            names = map(lambda name: prefix + "_" + name, names)
        return names

class SwitchCase:
    def __init__(self, values, member):
        self.values = values
        self.member = member
        self.members = [member]

    def get_check(self, var_cname, var_type):
        checks = []
        for v in self.values:
            if v == None:
                return "1"
            elif var_type.is_enum():
                checks.append("%s == %s" % (var_cname, var_type.c_enumname_by_name(v[1])))
            else:
                checks.append("%s(%s & %s)" % (v[0], var_cname, var_type.c_enumname_by_name(v[1])))
        return " || ".join(checks)

    def resolve(self, container):
        self.switch = container
        self.member = self.member.resolve(self)
        return self

    def get_num_pointers(self):
        return self.member.get_num_pointers()

    def get_pointer_names(self, marshalled):
        return self.member.get_pointer_names(marshalled)

class Switch(Containee):
    def __init__(self, variable, cases, name, attribute_list):
        Containee.__init__(self)
        self.variable = variable
        self.name = name
        self.cases = cases
        for attr in attribute_list:
            self.attributes[attr[0][1:]] = attr[1:]

    def is_switch(self):
        return True

    def lookup_case_member(self, name):
        for c in self.cases:
            if c.member.name == name:
                return c.member
        return None

    def has_switch_member(self, member):
        for c in self.cases:
            if c.member == member:
                return True
        return False

    def resolve(self, container):
        self.container = container
        self.cases = map(lambda c : c.resolve(self), self.cases)
        return self

    def __repr__(self):
        return "switch on %s %s" % (str(self.variable),str(self.name))

    def is_fixed_sizeof(self):
        # Kinda weird, but we're unlikely to have a real struct if there is an @end
        if self.has_end_attr():
            return False
        return True

    def is_fixed_nw_size(self):
        if self.has_attr("fixedsize"):
            return True

        size = None
        has_default = False
        for c in self.cases:
            for v in c.values:
                if v == None:
                    has_default = True
            if not c.member.is_fixed_nw_size():
                return False
            if size == None:
                size = c.member.get_fixed_nw_size()
            elif size != c.member.get_fixed_nw_size():
                return False
        # Fixed size if all elements listed, or has default
        if has_default:
            return True
        key = self.container.lookup_member(self.variable)
        return len(self.cases) == len(key.member_type.values)

    def is_extra_size(self):
        return self.has_end_attr()

    def contains_extra_size(self):
        for c in self.cases:
            if c.member.is_extra_size():
                return True
            if c.member.contains_extra_size():
                return True
        return False

    def get_fixed_nw_size(self):
        if not self.is_fixed_nw_size():
            raise Exception, "Not a fixed size type"
        size = 0
        for c in self.cases:
            size = max(size, c.member.get_fixed_nw_size())
        return size

    def sizeof(self):
        return "sizeof(((%s *)NULL)->%s)" % (self.container.c_type(),
                                             self.name)

    def contains_member(self, member):
        return False # TODO: Don't support switch deep member lookup yet

    def get_num_pointers(self):
        count = 0
        for c in self.cases:
            count = max(count, c.get_num_pointers())
        return count

    def get_pointer_names(self, marshalled):
        names = []
        for c in self.cases:
            names = names + c.get_pointer_names(marshalled)
        return names

class ContainerType(Type):
    def is_fixed_sizeof(self):
        for m in self.members:
            if not m.is_fixed_sizeof():
                return False
        return True

    def contains_extra_size(self):
        for m in self.members:
            if m.is_extra_size():
                return True
            if m.contains_extra_size():
                return True
        return False

    def is_fixed_nw_size(self):
        for i in self.members:
            if not i.is_fixed_nw_size():
                return False
        return True

    def get_fixed_nw_size(self):
        size = 0
        for i in self.members:
            size = size + i.get_fixed_nw_size()
        return size

    def contains_member(self, member):
        for m in self.members:
            if m == member or m.contains_member(member):
                return True
        return False

    def get_fixed_nw_offset(self, member):
        size = 0
        for i in self.members:
            if i == member:
                break
            if i.contains_member(member):
                size = size  + i.member_type.get_fixed_nw_offset(member)
                break
            if i.is_fixed_nw_size():
                size = size + i.get_fixed_nw_size()
        return size

    def resolve(self):
        self.members = map(lambda m : m.resolve(self), self.members)
        return self

    def get_num_pointers(self):
        count = 0
        for m in self.members:
            count = count + m.get_num_pointers()
        return count

    def get_pointer_names(self, marshalled):
        names = []
        for m in self.members:
            names = names + m.get_pointer_names(marshalled)
        return names

    def get_nw_offset(self, member, prefix = "", postfix = ""):
        fixed = self.get_fixed_nw_offset(member)
        v = []
        container = self
        while container != None:
            members = container.members
            container = None
            for m in members:
                if m == member:
                    break
                if m.contains_member(member):
                    container = m.member_type
                    break
                if m.is_switch() and m.has_switch_member(member):
                    break
                if not m.is_fixed_nw_size():
                    v.append(prefix + m.name + postfix)
        if len(v) > 0:
            return str(fixed) + " + " + (" + ".join(v))
        else:
            return str(fixed)

    def lookup_member(self, name):
        dot = name.find('.')
        rest = None
        if dot >= 0:
            rest = name[dot+1:]
            name = name[:dot]

        member = None
        if self.members_by_name.has_key(name):
            member = self.members_by_name[name]
        else:
            for m in self.members:
                if m.is_switch():
                    member = m.lookup_case_member(name)
                    if member != None:
                        break
                if member != None:
                    break

        if member == None:
            raise Exception, "No member called %s found" % name

        if rest != None:
            return member.member_type.lookup_member(rest)

        return member

class StructType(ContainerType):
    def __init__(self, name, members, attribute_list):
        Type.__init__(self)
        self.name = name
        self.members = members
        self.members_by_name = {}
        for m in members:
            self.members_by_name[m.name] = m
        for attr in attribute_list:
            self.attributes[attr[0][1:]] = attr[1:]

    def __str__(self):
        if self.name == None:
            return "anonymous struct"
        else:
            return "struct %s" % self.name

    def c_type(self):
        if self.has_attr("ctype"):
            return self.attributes["ctype"][0]
        return codegen.prefix_camel(self.name)

class MessageType(ContainerType):
    def __init__(self, name, members, attribute_list):
        Type.__init__(self)
        self.name = name
        self.members = members
        self.members_by_name = {}
        for m in members:
            self.members_by_name[m.name] = m
        self.reverse_members = {} # ChannelMembers referencing this message
        for attr in attribute_list:
            self.attributes[attr[0][1:]] = attr[1:]

    def __str__(self):
        if self.name == None:
            return "anonymous message"
        else:
            return "message %s" % self.name

    def c_name(self):
        if self.name == None:
            cms = self.reverse_members.keys()
            if len(cms) != 1:
                raise "Unknown typename for message"
            cm = cms[0]
            channelname = cm.channel.member_name
            if channelname == None:
                channelname = ""
            else:
                channelname = channelname + "_"
            if cm.is_server:
                return "msg_" + channelname +  cm.name
            else:
                return "msgc_" + channelname +  cm.name
        else:
            return codegen.prefix_camel("Msg", self.name)

    def c_type(self):
        if self.has_attr("ctype"):
            return self.attributes["ctype"][0]
        if self.name == None:
            cms = self.reverse_members.keys()
            if len(cms) != 1:
                raise "Unknown typename for message"
            cm = cms[0]
            channelname = cm.channel.member_name
            if channelname == None:
                channelname = ""
            if cm.is_server:
                return codegen.prefix_camel("Msg", channelname, cm.name)
            else:
                return codegen.prefix_camel("Msgc", channelname, cm.name)
        else:
            return codegen.prefix_camel("Msg", self.name)

class ChannelMember(Containee):
    def __init__(self, name, message_type, value):
        Containee.__init__(self)
        self.name = name
        self.message_type = message_type
        self.value = value

    def resolve(self, channel):
        self.channel = channel
        self.message_type = self.message_type.resolve()
        self.message_type.reverse_members[self] = 1

        return self

    def __repr__(self):
        return "%s (%s)" % (str(self.name), str(self.message_type))

class ChannelType(Type):
    def __init__(self, name, base, members, attribute_list):
        Type.__init__(self)
        self.name = name
        self.base = base
        self.member_name = None
        self.members = members
        for attr in attribute_list:
            self.attributes[attr[0][1:]] = attr[1:]

    def __str__(self):
        if self.name == None:
            return "anonymous channel"
        else:
            return "channel %s" % self.name

    def is_fixed_nw_size(self):
        return False

    def get_client_message(self, name):
        return self.client_messages_byname[name]

    def get_server_message(self, name):
        return self.server_messages_byname[name]

    def resolve(self):
        if self.base != None:
            self.base = self.base.resolve()

            server_messages = self.base.server_messages[:]
            server_messages_byname = self.base.server_messages_byname.copy()
            client_messages = self.base.client_messages[:]
            client_messages_byname = self.base.client_messages_byname.copy()

            # Set default member_name, FooChannel -> foo
            self.member_name = self.name[:-7].lower()
        else:
            server_messages = []
            server_messages_byname = {}
            client_messages = []
            client_messages_byname = {}

        server_count = 1
        client_count = 1

        server = True
        for m in self.members:
            if m == "server":
                server = True
            elif m == "client":
                server = False
            elif server:
                m.is_server = True
                m = m.resolve(self)
                if m.value:
                    server_count = m.value + 1
                else:
                    m.value = server_count
                    server_count = server_count + 1
                server_messages.append(m)
                server_messages_byname[m.name] = m
            else:
                m.is_server = False
                m = m.resolve(self)
                if m.value:
                    client_count = m.value + 1
                else:
                    m.value = client_count
                    client_count = client_count + 1
                client_messages.append(m)
                client_messages_byname[m.name] = m

        self.server_messages = server_messages
        self.server_messages_byname = server_messages_byname
        self.client_messages = client_messages
        self.client_messages_byname = client_messages_byname

        return self

class ProtocolMember:
    def __init__(self, name, channel_type, value):
        self.name = name
        self.channel_type = channel_type
        self.value = value

    def resolve(self, protocol):
        self.channel_type = self.channel_type.resolve()
        self.channel_type.member_name = self.name
        return self

    def __repr__(self):
        return "%s (%s)" % (str(self.name), str(self.channel_type))

class ProtocolType(Type):
    def __init__(self, name, channels):
        Type.__init__(self)
        self.name = name
        self.channels = channels

    def __str__(self):
        if self.name == None:
            return "anonymous protocol"
        else:
            return "protocol %s" % self.name

    def is_fixed_nw_size(self):
        return False

    def resolve(self):
        count = 1
        for m in self.channels:
            m = m.resolve(self)
            if m.value:
                count = m.value + 1
            else:
                m.value = count
                count = count + 1

        return self

int8 = IntegerType(8, True)
uint8 = IntegerType(8, False)
int16 = IntegerType(16, True)
uint16 = IntegerType(16, False)
int32 = IntegerType(32, True)
uint32 = IntegerType(32, False)
int64 = IntegerType(64, True)
uint64 = IntegerType(64, False)
