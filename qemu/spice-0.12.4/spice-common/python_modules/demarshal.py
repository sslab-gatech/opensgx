from __future__ import with_statement
import ptypes
import codegen

# The handling of sizes is somewhat complex, as there are several types of size:
# * nw_size
#   This is the network size, i.e. the number of bytes on the network
#
# * mem_size
#   The total amount of memory used for the representation of something inside
#   spice. This is generally sizeof(C struct), but can be larger if for instance
#   the type has a variable size array at the end or has a pointer in it that
#   points to another data chunk (which will be allocated after the main
#   data chunk). This is essentially how much memory you need to allocate to
#   contain the data type.
#
# * extra_size
#   This is the size of anything that is not part of the containing structure.
#   For instance, a primitive (say uint32_t) member has no extra size, because
#   when allocating its part of the sizeof(MessageStructType) struct. However
#   a variable array can be places at the end of a structure (@end) and its
#   size is then extra_size. Note that this extra_size is included in the
#   mem_size of the enclosing struct, and even if you request the mem_size
#   of the array itself. However, extra_size is typically not requested
#   when the full mem_size is also requested.
#
#   extra sizes come in two flavours. contains_extra_size means that the item
#   has a normal presence in the parent container, but has some additional
#   extra_size it references. For instance via a pointer somewhere in it.
#   There is also is_extra_size(). This indicates that the whole elements
#   "normal" mem size should be considered extra size for the container, so
#   when computing the parent mem_size you should add the mem_size of this
#   part as extra_size

def write_parser_helpers(writer):
    if writer.is_generated("helper", "demarshaller"):
        return

    writer.set_is_generated("helper", "demarshaller")

    writer = writer.function_helper()

    writer.writeln("#ifdef WORDS_BIGENDIAN")
    for size in [8, 16, 32, 64]:
        for sign in ["", "u"]:
            utype = "uint%d" % (size)
            type = "%sint%d" % (sign, size)
            swap = "SPICE_BYTESWAP%d" % size
            if size == 8:
                writer.macro("read_%s" % type, "ptr", "(*((%s_t *)(ptr)))" % type)
                writer.macro("write_%s" % type, "ptr, val", "*(%s_t *)(ptr) = val" % (type))
            else:
                writer.macro("read_%s" % type, "ptr", "((%s_t)%s(*((%s_t *)(ptr))))" % (type, swap, utype))
                writer.macro("write_%s" % type, "ptr, val", "*(%s_t *)(ptr) = %s((%s_t)val)" % (utype, swap, utype))
    writer.writeln("#else")
    for size in [8, 16, 32, 64]:
        for sign in ["", "u"]:
            type = "%sint%d" % (sign, size)
            writer.macro("read_%s" % type, "ptr", "(*((%s_t *)(ptr)))" % type)
            writer.macro("write_%s" % type, "ptr, val", "(*((%s_t *)(ptr))) = val" % type)
    writer.writeln("#endif")

    for size in [8, 16, 32, 64]:
        for sign in ["", "u"]:
            writer.newline()
            type = "%sint%d" % (sign, size)
            ctype = "%s_t" % type
            scope = writer.function("SPICE_GNUC_UNUSED consume_%s" % type, ctype, "uint8_t **ptr", True)
            scope.variable_def(ctype, "val")
            writer.assign("val", "read_%s(*ptr)" % type)
            writer.increment("*ptr", size / 8)
            writer.statement("return val")
            writer.end_block()

    writer.newline()
    writer.statement("typedef struct PointerInfo PointerInfo")
    writer.statement("typedef void (*message_destructor_t)(uint8_t *message)")
    writer.statement("typedef uint8_t * (*parse_func_t)(uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *ptr_info, int minor)")
    writer.statement("typedef uint8_t * (*parse_msg_func_t)(uint8_t *message_start, uint8_t *message_end, int minor, size_t *size_out, message_destructor_t *free_message)")
    writer.statement("typedef uint8_t * (*spice_parse_channel_func_t)(uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message)")

    writer.newline()
    writer.begin_block("struct PointerInfo")
    writer.variable_def("uint64_t", "offset")
    writer.variable_def("parse_func_t", "parse")
    writer.variable_def("void **", "dest")
    writer.variable_def("uint32_t", "nelements")
    writer.end_block(semicolon=True)

def write_read_primitive(writer, start, container, name, scope):
    m = container.lookup_member(name)
    assert(m.is_primitive())
    writer.assign("pos", start + " + " + container.get_nw_offset(m, "", "__nw_size"))
    writer.error_check("pos + %s > message_end" % m.member_type.get_fixed_nw_size())

    var = "%s__value" % (name.replace(".", "_"))
    if not scope.variable_defined(var):
        scope.variable_def(m.member_type.c_type(), var)
    writer.assign(var, "read_%s(pos)" % (m.member_type.primitive_type()))
    return var

def write_write_primitive(writer, start, container, name, val):
    m = container.lookup_member(name)
    assert(m.is_primitive())
    writer.assign("pos", start + " + " + container.get_nw_offset(m, "", "__nw_size"))

    var = "%s__value" % (name)
    writer.statement("write_%s(pos, %s)" % (m.member_type.primitive_type(), val))
    return var

def write_read_primitive_item(writer, item, scope):
    assert(item.type.is_primitive())
    writer.assign("pos", item.get_position())
    writer.error_check("pos + %s > message_end" % item.type.get_fixed_nw_size())
    var = "%s__value" % (item.subprefix.replace(".", "_"))
    scope.variable_def(item.type.c_type(), var)
    writer.assign(var, "read_%s(pos)" % (item.type.primitive_type()))
    return var

class ItemInfo:
    def __init__(self, type, prefix, position):
        self.type = type
        self.prefix = prefix
        self.subprefix = prefix
        self.position = position
        self.member = None

    def nw_size(self):
        return self.prefix + "__nw_size"

    def mem_size(self):
        return self.prefix + "__mem_size"

    def extra_size(self):
        return self.prefix + "__extra_size"

    def get_position(self):
        return self.position

class MemberItemInfo(ItemInfo):
    def __init__(self, member, container, start):
        if not member.is_switch():
            self.type = member.member_type
        self.prefix = member.name
        self.subprefix = member.name
        self.position = "(%s + %s)" % (start, container.get_nw_offset(member, "", "__nw_size"))
        self.member = member

def write_validate_switch_member(writer, container, switch_member, scope, parent_scope, start,
                                 want_nw_size, want_mem_size, want_extra_size):
    var = container.lookup_member(switch_member.variable)
    var_type = var.member_type

    v = write_read_primitive(writer, start, container, switch_member.variable, parent_scope)

    item = MemberItemInfo(switch_member, container, start)

    first = True
    for c in switch_member.cases:
        check = c.get_check(v, var_type)
        m = c.member
        with writer.if_block(check, not first, False) as if_scope:
            item.type = c.member.member_type
            item.subprefix = item.prefix + "_" + m.name
            item.member = c.member

            all_as_extra_size = m.is_extra_size() and want_extra_size
            if not want_mem_size and all_as_extra_size and not scope.variable_defined(item.mem_size()):
                scope.variable_def("uint32_t", item.mem_size())

            sub_want_mem_size = want_mem_size or all_as_extra_size
            sub_want_extra_size = want_extra_size and not all_as_extra_size

            write_validate_item(writer, container, item, if_scope, scope, start,
                                want_nw_size, sub_want_mem_size, sub_want_extra_size)

            if all_as_extra_size:
                writer.assign(item.extra_size(), item.mem_size())

        first = False

    with writer.block(" else"):
        if want_nw_size:
            writer.assign(item.nw_size(), 0)
        if want_mem_size:
            writer.assign(item.mem_size(), 0)
        if want_extra_size:
            writer.assign(item.extra_size(), 0)

    writer.newline()

def write_validate_struct_function(writer, struct):
    validate_function = "validate_%s" % struct.c_type()
    if writer.is_generated("validator", validate_function):
        return validate_function

    writer.set_is_generated("validator", validate_function)
    writer = writer.function_helper()
    scope = writer.function(validate_function, "static intptr_t", "uint8_t *message_start, uint8_t *message_end, uint64_t offset, int minor")
    scope.variable_def("uint8_t *", "start = message_start + offset")
    scope.variable_def("SPICE_GNUC_UNUSED uint8_t *", "pos")
    scope.variable_def("size_t", "mem_size", "nw_size")
    num_pointers = struct.get_num_pointers()
    if  num_pointers != 0:
        scope.variable_def("SPICE_GNUC_UNUSED intptr_t", "ptr_size")

    writer.newline()
    with writer.if_block("offset == 0"):
        writer.statement("return 0")

    writer.newline()
    writer.error_check("start >= message_end")

    writer.newline()
    write_validate_container(writer, None, struct, "start", scope, True, True, False)

    writer.newline()
    writer.comment("Check if struct fits in reported side").newline()
    writer.error_check("start + nw_size > message_end")

    writer.statement("return mem_size")

    writer.newline()
    writer.label("error")
    writer.statement("return -1")

    writer.end_block()

    return validate_function

def write_validate_pointer_item(writer, container, item, scope, parent_scope, start,
                                want_nw_size, want_mem_size, want_extra_size):
    if want_nw_size:
        writer.assign(item.nw_size(), item.type.get_fixed_nw_size())

    if want_mem_size or want_extra_size:
        target_type = item.type.target_type

        v = write_read_primitive_item(writer, item, scope)
        if item.type.has_attr("nonnull"):
            writer.error_check("%s == 0" % v)

        # pointer target is struct, or array of primitives
        # if array, need no function check

        if target_type.is_array():
            writer.error_check("message_start + %s >= message_end" % v)


            assert target_type.element_type.is_primitive()

            array_item = ItemInfo(target_type, "%s__array" % item.prefix, start)
            scope.variable_def("uint32_t", array_item.nw_size())
            # don't create a variable that isn't used, fixes -Werror=unused-but-set-variable
            need_mem_size = want_mem_size or (
                want_extra_size and not item.member.has_attr("chunk")
                and not target_type.is_cstring_length())
            if need_mem_size:
                scope.variable_def("uint32_t", array_item.mem_size())
            if target_type.is_cstring_length():
                writer.assign(array_item.nw_size(), "spice_strnlen((char *)message_start + %s, message_end - (message_start + %s))" % (v, v))
                writer.error_check("*(message_start + %s + %s) != 0" % (v, array_item.nw_size()))
            else:
                write_validate_array_item(writer, container, array_item, scope, parent_scope, start,
                                          True, want_mem_size=need_mem_size, want_extra_size=False)
                writer.error_check("message_start + %s + %s > message_end" % (v, array_item.nw_size()))

            if want_extra_size:
                if item.member and item.member.has_attr("chunk"):
                    writer.assign(item.extra_size(), "sizeof(SpiceChunks) + sizeof(SpiceChunk)")
                elif item.member and item.member.has_attr("nocopy"):
                    writer.comment("@nocopy, so no extra size").newline()
                    writer.assign(item.extra_size(), 0)
                elif target_type.element_type.get_fixed_nw_size == 1:
                    writer.assign(item.extra_size(), array_item.mem_size())
                # If not bytes or zero, add padding needed for alignment
                else:
                    writer.assign(item.extra_size(), "%s + /* for alignment */ 3" % array_item.mem_size())
            if want_mem_size:
                writer.assign(item.mem_size(), "sizeof(void *) + %s" % array_item.mem_size())

        elif target_type.is_struct():
            validate_function = write_validate_struct_function(writer, target_type)
            writer.assign("ptr_size", "%s(message_start, message_end, %s, minor)" % (validate_function, v))
            writer.error_check("ptr_size < 0")

            if want_extra_size:
                writer.assign(item.extra_size(), "ptr_size + /* for alignment */ 3")
            if want_mem_size:
                writer.assign(item.mem_size(), "sizeof(void *) + ptr_size")
        else:
            raise NotImplementedError("pointer to unsupported type %s" % target_type)


def write_validate_array_item(writer, container, item, scope, parent_scope, start,
                              want_nw_size, want_mem_size, want_extra_size):
    array = item.type
    is_byte_size = False
    element_type = array.element_type
    if array.is_bytes_length():
        nelements = "%s__nbytes" %(item.prefix)
        real_nelements = "%s__nelements" %(item.prefix)
        if not parent_scope.variable_defined(real_nelements):
            parent_scope.variable_def("uint32_t", real_nelements)
    else:
        nelements = "%s__nelements" %(item.prefix)
    if not parent_scope.variable_defined(nelements):
        parent_scope.variable_def("uint32_t", nelements)

    if array.is_constant_length():
        writer.assign(nelements, array.size)
    elif array.is_remaining_length():
        if element_type.is_fixed_nw_size():
            if element_type.get_fixed_nw_size() == 1:
                writer.assign(nelements, "message_end - %s" % item.get_position())
            else:
                writer.assign(nelements, "(message_end - %s) / (%s)" %(item.get_position(), element_type.get_fixed_nw_size()))
        else:
            raise NotImplementedError("TODO array[] of dynamic element size not done yet")
    elif array.is_identifier_length():
        v = write_read_primitive(writer, start, container, array.size, scope)
        writer.assign(nelements, v)
    elif array.is_image_size_length():
        bpp = array.size[1]
        width = array.size[2]
        rows = array.size[3]
        width_v = write_read_primitive(writer, start, container, width, scope)
        rows_v = write_read_primitive(writer, start, container, rows, scope)
        # TODO: Handle multiplication overflow
        if bpp == 8:
            writer.assign(nelements, "%s * %s" % (width_v, rows_v))
        elif bpp == 1:
            writer.assign(nelements, "((%s + 7) / 8 ) * %s" % (width_v, rows_v))
        else:
            writer.assign(nelements, "((%s * %s + 7) / 8 ) * %s" % (bpp, width_v, rows_v))
    elif array.is_bytes_length():
        is_byte_size = True
        v = write_read_primitive(writer, start, container, array.size[1], scope)
        writer.assign(nelements, v)
        writer.assign(real_nelements, 0)
    elif array.is_cstring_length():
        writer.todo("cstring array size type not handled yet")
    else:
        writer.todo("array size type not handled yet")

    writer.newline()

    nw_size = item.nw_size()
    mem_size = item.mem_size()
    extra_size = item.extra_size()

    if is_byte_size and want_nw_size:
        writer.assign(nw_size, nelements)
        want_nw_size = False

    if element_type.is_fixed_nw_size() and want_nw_size:
        element_size = element_type.get_fixed_nw_size()
        # TODO: Overflow check the multiplication
        if element_size == 1:
            writer.assign(nw_size, nelements)
        else:
            writer.assign(nw_size, "(%s) * %s" % (element_size, nelements))
        want_nw_size = False

    if array.has_attr("as_ptr") and want_mem_size:
        writer.assign(mem_size, "sizeof(void *)")
        want_mem_size = False

    if array.has_attr("chunk"):
        if want_mem_size:
            writer.assign(extra_size, "sizeof(SpiceChunks *)")
            want_mem_size = False
        if want_extra_size:
            writer.assign(extra_size, "sizeof(SpiceChunks) + sizeof(SpiceChunk)")
            want_extra_size = False

    if element_type.is_fixed_sizeof() and want_mem_size and not is_byte_size:
        # TODO: Overflow check the multiplication
        if array.has_attr("ptr_array"):
            writer.assign(mem_size, "sizeof(void *) + SPICE_ALIGN(%s * %s, 4)" % (element_type.sizeof(), nelements))
        else:
            writer.assign(mem_size, "%s * %s" % (element_type.sizeof(), nelements))
        want_mem_size = False

    if not element_type.contains_extra_size() and want_extra_size:
        writer.assign(extra_size, 0)
        want_extra_size = False

    if not (want_mem_size or want_nw_size or want_extra_size):
        return

    start2 = codegen.increment_identifier(start)
    scope.variable_def("uint8_t *", "%s = %s" % (start2, item.get_position()))
    if is_byte_size:
        start2_end = "%s_array_end" % start2
        scope.variable_def("uint8_t *", start2_end)

    element_item = ItemInfo(element_type, "%s__element" % item.prefix, start2)

    element_nw_size = element_item.nw_size()
    element_mem_size = element_item.mem_size()
    element_extra_size = element_item.extra_size()
    scope.variable_def("uint32_t", element_nw_size)
    scope.variable_def("uint32_t", element_mem_size)
    if want_extra_size:
        scope.variable_def("uint32_t", element_extra_size)

    if want_nw_size:
        writer.assign(nw_size, 0)
    if want_mem_size:
        writer.assign(mem_size, 0)
    if want_extra_size:
        writer.assign(extra_size, 0)

    want_element_nw_size = want_nw_size
    if element_type.is_fixed_nw_size():
        start_increment = element_type.get_fixed_nw_size()
    else:
        want_element_nw_size = True
        start_increment = element_nw_size

    if is_byte_size:
        writer.assign(start2_end, "%s + %s" % (start2, nelements))

    with writer.index(no_block = is_byte_size) as index:
        with writer.while_loop("%s < %s" % (start2, start2_end) ) if is_byte_size else writer.for_loop(index, nelements) as scope:
            if is_byte_size:
                writer.increment(real_nelements, 1)
            write_validate_item(writer, container, element_item, scope, parent_scope, start2,
                                want_element_nw_size, want_mem_size, want_extra_size)

            if want_nw_size:
                writer.increment(nw_size, element_nw_size)
            if want_mem_size:
                if array.has_attr("ptr_array"):
                    writer.increment(mem_size, "sizeof(void *) + SPICE_ALIGN(%s, 4)" % element_mem_size)
                else:
                    writer.increment(mem_size, element_mem_size)
            if want_extra_size:
                writer.increment(extra_size, element_extra_size)

            writer.increment(start2, start_increment)
    if is_byte_size:
        writer.error_check("%s != %s" % (start2, start2_end))
        write_write_primitive(writer, start, container, array.size[1], real_nelements)

def write_validate_struct_item(writer, container, item, scope, parent_scope, start,
                               want_nw_size, want_mem_size, want_extra_size):
    struct = item.type
    start2 = codegen.increment_identifier(start)
    scope.variable_def("SPICE_GNUC_UNUSED uint8_t *", start2 + " = %s" % (item.get_position()))

    write_validate_container(writer, item.prefix, struct, start2, scope, want_nw_size, want_mem_size, want_extra_size)

def write_validate_primitive_item(writer, container, item, scope, parent_scope, start,
                                  want_nw_size, want_mem_size, want_extra_size):
    if want_nw_size:
        nw_size = item.nw_size()
        writer.assign(nw_size, item.type.get_fixed_nw_size())
    if want_mem_size:
        mem_size = item.mem_size()
        writer.assign(mem_size, item.type.sizeof())
    if want_extra_size:
        writer.assign(item.extra_size(), 0)

def write_validate_item(writer, container, item, scope, parent_scope, start,
                        want_nw_size, want_mem_size, want_extra_size):
    if item.member and item.member.has_attr("to_ptr"):
        want_nw_size = True
    if item.type.is_pointer():
        write_validate_pointer_item(writer, container, item, scope, parent_scope, start,
                                    want_nw_size, want_mem_size, want_extra_size)
    elif item.type.is_array():
        write_validate_array_item(writer, container, item, scope, parent_scope, start,
                                  want_nw_size, want_mem_size, want_extra_size)
    elif item.type.is_struct():
        write_validate_struct_item(writer, container, item, scope, parent_scope, start,
                                   want_nw_size, want_mem_size, want_extra_size)
    elif item.type.is_primitive():
        write_validate_primitive_item(writer, container, item, scope, parent_scope, start,
                                      want_nw_size, want_mem_size, want_extra_size)
    else:
        writer.todo("Implement validation of %s" % item.type)

    if item.member and item.member.has_attr("to_ptr"):
        saved_size = "%s__saved_size" % item.member.name
        writer.add_function_variable("uint32_t", saved_size + " = 0")
        writer.assign(saved_size, item.nw_size())

def write_validate_member(writer, container, member, parent_scope, start,
                          want_nw_size, want_mem_size, want_extra_size):
    if member.has_attr("virtual"):
        return

    if member.has_minor_attr():
        prefix = "if (minor >= %s)" % (member.get_minor_attr())
        newline = False
    else:
        prefix = ""
        newline = True
    item = MemberItemInfo(member, container, start)
    with writer.block(prefix, newline=newline, comment=member.name) as scope:
        if member.is_switch():
            write_validate_switch_member(writer, container, member, scope, parent_scope, start,
                                         want_nw_size, want_mem_size, want_extra_size)
        else:
            write_validate_item(writer, container, item, scope, parent_scope, start,
                                want_nw_size, want_mem_size, want_extra_size)

    if member.has_minor_attr():
        with writer.block(" else", comment = "minor < %s" % (member.get_minor_attr())):
            if member.is_array():
                nelements = "%s__nelements" %(item.prefix)
                writer.assign(nelements, 0)
            if want_nw_size:
                writer.assign(item.nw_size(), 0)

            if want_mem_size:
                if member.is_fixed_sizeof():
                    writer.assign(item.mem_size(), member.sizeof())
                elif member.is_array():
                    writer.assign(item.mem_size(), 0)
                else:
                    raise NotImplementedError("TODO minor check for non-constant items")

            assert not want_extra_size

def write_validate_container(writer, prefix, container, start, parent_scope, want_nw_size, want_mem_size, want_extra_size):
    for m in container.members:
        sub_want_nw_size = want_nw_size and not m.is_fixed_nw_size()
        sub_want_mem_size = m.is_extra_size() and want_mem_size
        sub_want_extra_size = not m.is_extra_size() and m.contains_extra_size()

        defs = ["size_t"]
        if sub_want_nw_size:
            defs.append (m.name + "__nw_size")
        if sub_want_mem_size:
            defs.append (m.name + "__mem_size")
        if sub_want_extra_size:
            defs.append (m.name + "__extra_size")

        if sub_want_nw_size or sub_want_mem_size or sub_want_extra_size:
            parent_scope.variable_def(*defs)
            write_validate_member(writer, container, m, parent_scope, start,
                                  sub_want_nw_size, sub_want_mem_size, sub_want_extra_size)
            writer.newline()

    if want_nw_size:
        if prefix:
            nw_size = prefix + "__nw_size"
        else:
            nw_size = "nw_size"

        size = 0
        for m in container.members:
            if m.is_fixed_nw_size():
                size = size + m.get_fixed_nw_size()

        nm_sum = str(size)
        for m in container.members:
            if not m.is_fixed_nw_size():
                nm_sum = nm_sum + " + " + m.name + "__nw_size"

        writer.assign(nw_size, nm_sum)

    if want_mem_size:
        if prefix:
            mem_size = prefix + "__mem_size"
        else:
            mem_size = "mem_size"

        mem_sum = container.sizeof()
        for m in container.members:
            if m.is_extra_size():
                mem_sum = mem_sum + " + " + m.name + "__mem_size"
            elif m.contains_extra_size():
                mem_sum = mem_sum + " + " + m.name + "__extra_size"

        writer.assign(mem_size, mem_sum)

    if want_extra_size:
        if prefix:
            extra_size = prefix + "__extra_size"
        else:
            extra_size = "extra_size"

        extra_sum = []
        for m in container.members:
            if m.is_extra_size():
                extra_sum.append(m.name + "__mem_size")
            elif m.contains_extra_size():
                extra_sum.append(m.name + "__extra_size")
        writer.assign(extra_size, codegen.sum_array(extra_sum))

class DemarshallingDestination:
    def __init__(self):
        pass

    def child_at_end(self, writer, t):
        return RootDemarshallingDestination(self, t.c_type(), t.sizeof())

    def child_sub(self, member):
        return SubDemarshallingDestination(self, member)

    def declare(self, writer):
        return writer.optional_block(self.reuse_scope)

    def is_toplevel(self):
        return self.parent_dest == None and not self.is_helper

class RootDemarshallingDestination(DemarshallingDestination):
    def __init__(self, parent_dest, c_type, sizeof, pointer = None):
        self.is_helper = False
        self.reuse_scope = None
        self.parent_dest = parent_dest
        if parent_dest:
            self.base_var = codegen.increment_identifier(parent_dest.base_var)
        else:
            self.base_var = "out"
        self.c_type = c_type
        self.sizeof = sizeof
        self.pointer = pointer # None == at "end"

    def get_ref(self, member):
        return self.base_var + "->" + member

    def declare(self, writer):
        if self.reuse_scope:
            scope = self.reuse_scope
        else:
            writer.begin_block()
            scope = writer.get_subwriter()

        scope.variable_def(self.c_type + " *", self.base_var)
        if not self.reuse_scope:
            scope.newline()

        if self.pointer:
            writer.assign(self.base_var, "(%s *)%s" % (self.c_type, self.pointer))
        else:
            writer.assign(self.base_var, "(%s *)end" % (self.c_type))
            writer.increment("end", self.sizeof)
        writer.newline()

        if self.reuse_scope:
            return writer.no_block(self.reuse_scope)
        else:
            return writer.partial_block(scope)

class SubDemarshallingDestination(DemarshallingDestination):
    def __init__(self, parent_dest, member):
        self.reuse_scope = None
        self.parent_dest = parent_dest
        self.base_var = parent_dest.base_var
        self.member = member
        self.is_helper = False

    def get_ref(self, member):
        return self.parent_dest.get_ref(self.member) + "." + member

# Note: during parsing, byte_size types have been converted to count during validation
def read_array_len(writer, prefix, array, dest, scope, is_ptr):
    if is_ptr:
        nelements = "%s__array__nelements" % prefix
    else:
        nelements = "%s__nelements" % prefix
    if dest.is_toplevel() and scope.variable_defined(nelements):
        return nelements # Already there for toplevel, need not recalculate
    element_type = array.element_type
    scope.variable_def("uint32_t", nelements)
    if array.is_constant_length():
        writer.assign(nelements, array.size)
    elif array.is_identifier_length():
        writer.assign(nelements, dest.get_ref(array.size))
    elif array.is_remaining_length():
        if element_type.is_fixed_nw_size():
            writer.assign(nelements, "(message_end - in) / (%s)" %(element_type.get_fixed_nw_size()))
        else:
            raise NotImplementedError("TODO array[] of dynamic element size not done yet")
    elif array.is_image_size_length():
        bpp = array.size[1]
        width = array.size[2]
        rows = array.size[3]
        width_v = dest.get_ref(width)
        rows_v = dest.get_ref(rows)
        # TODO: Handle multiplication overflow
        if bpp == 8:
            writer.assign(nelements, "%s * %s" % (width_v, rows_v))
        elif bpp == 1:
            writer.assign(nelements, "((%s + 7) / 8 ) * %s" % (width_v, rows_v))
        else:
            writer.assign(nelements, "((%s * %s + 7) / 8 ) * %s" % (bpp, width_v, rows_v))
    elif array.is_bytes_length():
        writer.assign(nelements, dest.get_ref(array.size[2]))
    else:
        raise NotImplementedError("TODO array size type not handled yet")
    return nelements

def write_switch_parser(writer, container, switch, dest, scope):
    var = container.lookup_member(switch.variable)
    var_type = var.member_type

    if switch.has_attr("fixedsize"):
        scope.variable_def("uint8_t *", "in_save")
        writer.assign("in_save", "in")

    first = True
    for c in switch.cases:
        check = c.get_check(dest.get_ref(switch.variable), var_type)
        m = c.member
        with writer.if_block(check, not first, False) as block:
            t = m.member_type
            if switch.has_end_attr():
                dest2 = dest.child_at_end(writer, m.member_type)
            elif switch.has_attr("anon"):
                if t.is_struct() and not m.has_attr("to_ptr"):
                    dest2 = dest.child_sub(m.name)
                else:
                    dest2 = dest
            else:
                if t.is_struct():
                    dest2 = dest.child_sub(switch.name + "." + m.name)
                else:
                    dest2 = dest.child_sub(switch.name)
            dest2.reuse_scope = block

            if m.has_attr("to_ptr"):
                write_parse_to_pointer(writer, t, False, dest2, m.name, block)
            elif t.is_pointer():
                write_parse_pointer(writer, t, False, dest2, m.name, block)
            elif t.is_struct():
                write_container_parser(writer, t, dest2)
            elif t.is_primitive():
                if m.has_attr("zero"):
                    writer.statement("consume_%s(&in)" % (t.primitive_type()))
                else:
                    writer.assign(dest2.get_ref(m.name), "consume_%s(&in)" % (t.primitive_type()))
                #TODO validate e.g. flags and enums
            elif t.is_array():
                nelements = read_array_len(writer, m.name, t, dest, block, False)
                write_array_parser(writer, m, nelements, t, dest2, block)
            else:
                writer.todo("Can't handle type %s" % m.member_type)

        first = False

    writer.newline()

    if switch.has_attr("fixedsize"):
        writer.assign("in", "in_save + %s" % switch.get_fixed_nw_size())

def write_parse_ptr_function(writer, target_type):
    if target_type.is_array():
        parse_function = "parse_array_%s" % target_type.element_type.primitive_type()
    else:
        parse_function = "parse_struct_%s" % target_type.c_type()
    if writer.is_generated("parser", parse_function):
        return parse_function

    writer.set_is_generated("parser", parse_function)

    writer = writer.function_helper()
    scope = writer.function(parse_function, "static uint8_t *", "uint8_t *message_start, uint8_t *message_end, uint8_t *struct_data, PointerInfo *this_ptr_info, int minor")
    scope.variable_def("uint8_t *", "in = message_start + this_ptr_info->offset")
    scope.variable_def("uint8_t *", "end")

    num_pointers = target_type.get_num_pointers()
    if  num_pointers != 0:
        scope.variable_def("SPICE_GNUC_UNUSED intptr_t", "ptr_size")
        scope.variable_def("uint32_t", "n_ptr=0")
        scope.variable_def("PointerInfo", "ptr_info[%s]" % num_pointers)

    writer.newline()
    if target_type.is_array():
        writer.assign("end", "struct_data")
    else:
        writer.assign("end", "struct_data + %s" % (target_type.sizeof()))

    dest = RootDemarshallingDestination(None, target_type.c_type(), target_type.sizeof(), "struct_data")
    dest.is_helper = True
    dest.reuse_scope = scope
    if target_type.is_array():
        write_array_parser(writer, None, "this_ptr_info->nelements", target_type, dest, scope)
    else:
        write_container_parser(writer, target_type, dest)

    if num_pointers != 0:
        write_ptr_info_check(writer)

    writer.statement("return end")

    if writer.has_error_check:
        writer.newline()
        writer.label("error")
        writer.statement("return NULL")

    writer.end_block()

    return parse_function

def write_array_parser(writer, member, nelements, array, dest, scope):
    is_byte_size = array.is_bytes_length()

    element_type = array.element_type
    if member:
        array_start = dest.get_ref(member.name)
        at_end = member.has_attr("end")
    else:
        array_start = "end"
        at_end = True

    if element_type == ptypes.uint8 or element_type == ptypes.int8:
        writer.statement("memcpy(%s, in, %s)" % (array_start, nelements))
        writer.increment("in", nelements)
        if at_end:
            writer.increment("end", nelements)
    else:
        with writer.index() as index:
            if member:
                array_pos = "%s[%s]" % (array_start, index)
            else:
                array_pos = "*(%s *)end" % (element_type.c_type())

            if array.has_attr("ptr_array"):
                scope.variable_def("void **", "ptr_array")
                scope.variable_def("int", "ptr_array_index")
                writer.assign("ptr_array_index", 0)
                writer.assign("ptr_array", "(void **)%s" % array_start)
                writer.increment("end", "sizeof(void *) * %s" % nelements)
                array_start = "end"
                array_pos = "*(%s *)end" % (element_type.c_type())
                at_end = True

            with writer.for_loop(index, nelements) as array_scope:
                if array.has_attr("ptr_array"):
                    writer.statement("ptr_array[ptr_array_index++] = end")
                if element_type.is_primitive():
                    writer.statement("%s = consume_%s(&in)" % (array_pos, element_type.primitive_type()))
                    if at_end:
                        writer.increment("end", element_type.sizeof())
                else:
                    if at_end:
                        dest2 = dest.child_at_end(writer, element_type)
                    else:
                        dest2 = RootDemarshallingDestination(dest, element_type.c_type(), element_type.c_type(), array_pos)
                    dest2.reuse_scope = array_scope
                    write_container_parser(writer, element_type, dest2)
                if array.has_attr("ptr_array"):
                    writer.comment("Align ptr_array element to 4 bytes").newline()
                    writer.assign("end", "(uint8_t *)SPICE_ALIGN((size_t)end, 4)")

def write_parse_pointer_core(writer, target_type, offset, at_end, dest, member_name, scope):
    writer.assign("ptr_info[n_ptr].offset", offset)
    writer.assign("ptr_info[n_ptr].parse", write_parse_ptr_function(writer, target_type))
    if at_end:
        writer.assign("ptr_info[n_ptr].dest", "(void **)end")
        writer.increment("end", "sizeof(void *)")
    else:
        writer.assign("ptr_info[n_ptr].dest", "(void **)&%s" % dest.get_ref(member_name))
    if target_type.is_array():
        nelements = read_array_len(writer, member_name, target_type, dest, scope, True)
        writer.assign("ptr_info[n_ptr].nelements", nelements)

    writer.statement("n_ptr++")

def write_parse_pointer(writer, t, at_end, dest, member_name, scope):
    write_parse_pointer_core(writer, t.target_type, "consume_%s(&in)" % t.primitive_type(),
                             at_end, dest, member_name, scope)

def write_parse_to_pointer(writer, t, at_end, dest, member_name, scope):
    write_parse_pointer_core(writer, t, "in - start",
                             at_end, dest, member_name, scope)
    writer.increment("in", "%s__saved_size" % member_name)

def write_member_parser(writer, container, member, dest, scope):
    if member.has_attr("virtual"):
        writer.assign(dest.get_ref(member.name), member.attributes["virtual"][0])
        return

    if member.is_switch():
        write_switch_parser(writer, container, member, dest, scope)
        return

    t = member.member_type

    if member.has_attr("to_ptr"):
        write_parse_to_pointer(writer, t, member.has_end_attr(), dest, member.name, scope)
    elif t.is_pointer():
        if member.has_attr("chunk"):
            assert(t.target_type.is_array())
            nelements = read_array_len(writer, member.name, t.target_type, dest, scope, True)
            writer.comment("Reuse data from network message as chunk").newline()
            scope.variable_def("SpiceChunks *", "chunks")
            writer.assign("chunks", "(SpiceChunks *)end")
            writer.increment("end", "sizeof(SpiceChunks) + sizeof(SpiceChunk)")
            writer.assign(dest.get_ref(member.name), "chunks")
            writer.assign("chunks->data_size", nelements)
            writer.assign("chunks->flags", 0)
            writer.assign("chunks->num_chunks", 1)
            writer.assign("chunks->chunk[0].len", nelements)
            writer.assign("chunks->chunk[0].data", "message_start + consume_%s(&in)" % t.primitive_type())
        elif member.has_attr("nocopy"):
            writer.comment("Reuse data from network message").newline()
            writer.assign(dest.get_ref(member.name), "(size_t)(message_start + consume_%s(&in))" % t.primitive_type())
        else:
            write_parse_pointer(writer, t, member.has_end_attr(), dest, member.name, scope)
    elif t.is_primitive():
        if member.has_attr("zero"):
            writer.statement("consume_%s(&in)" % t.primitive_type())
        elif member.has_end_attr():
            writer.statement("*(%s *)end = consume_%s(&in)" % (t.c_type(), t.primitive_type()))
            writer.increment("end", t.sizeof())
        else:
            if member.has_attr("bytes_count"):
                dest_var = dest.get_ref(member.attributes["bytes_count"][0])
            else:
                dest_var = dest.get_ref(member.name)
            writer.assign(dest_var, "consume_%s(&in)" % (t.primitive_type()))
        #TODO validate e.g. flags and enums
    elif t.is_array():
        nelements = read_array_len(writer, member.name, t, dest, scope, False)
        if member.has_attr("chunk") and t.element_type.is_fixed_nw_size() and t.element_type.get_fixed_nw_size() == 1:
            writer.comment("use array as chunk").newline()

            scope.variable_def("SpiceChunks *", "chunks")
            writer.assign("chunks", "(SpiceChunks *)end")
            writer.increment("end", "sizeof(SpiceChunks) + sizeof(SpiceChunk)")
            writer.assign(dest.get_ref(member.name), "chunks")
            writer.assign("chunks->data_size", nelements)
            writer.assign("chunks->flags", 0)
            writer.assign("chunks->num_chunks", 1)
            writer.assign("chunks->chunk[0].len", nelements)
            writer.assign("chunks->chunk[0].data", "in")
            writer.increment("in", "%s" % (nelements))
        elif member.has_attr("as_ptr") and t.element_type.is_fixed_nw_size():
            writer.comment("use array as pointer").newline()
            writer.assign(dest.get_ref(member.name), "(%s *)in" % t.element_type.c_type())
            len_var = member.attributes["as_ptr"]
            if len(len_var) > 0:
                writer.assign(dest.get_ref(len_var[0]), nelements)
            el_size = t.element_type.get_fixed_nw_size()
            if el_size != 1:
                writer.increment("in", "%s * %s" % (nelements, el_size))
            else:
                writer.increment("in", "%s" % (nelements))
        else:
            write_array_parser(writer, member, nelements, t, dest, scope)
    elif t.is_struct():
        if member.has_end_attr():
            dest2 = dest.child_at_end(writer, t)
        else:
            dest2 = dest.child_sub(member.name)
        writer.comment(member.name)
        write_container_parser(writer, t, dest2)
    else:
        raise NotImplementedError("TODO can't handle parsing of %s" % t)

def write_container_parser(writer, container, dest):
    with dest.declare(writer) as scope:
        for m in container.members:
            if m.has_minor_attr():
                writer.begin_block("if (minor >= %s)" % m.get_minor_attr())
            write_member_parser(writer, container, m, dest, scope)
            if m.has_minor_attr():
                # We need to zero out the fixed part of all optional fields
                if not m.member_type.is_array():
                    writer.end_block(newline=False)
                    writer.begin_block(" else")
                    # TODO: This is not right for fields that don't exist in the struct
                    if m.has_attr("zero"):
                        pass
                    elif m.member_type.is_primitive():
                        writer.assign(dest.get_ref(m.name), "0")
                    elif m.is_fixed_sizeof():
                        writer.statement("memset ((char *)&%s, 0, %s)" % (dest.get_ref(m.name), m.sizeof()))
                    else:
                        raise NotImplementedError("TODO Clear optional dynamic fields")
                writer.end_block()

def write_ptr_info_check(writer):
    writer.newline()
    with writer.index() as index:
        with writer.for_loop(index, "n_ptr") as scope:
            offset = "ptr_info[%s].offset" % index
            function = "ptr_info[%s].parse" % index
            dest = "ptr_info[%s].dest" % index
            with writer.if_block("%s == 0" % offset, newline=False):
                writer.assign("*%s" % dest, "NULL")
            with writer.block(" else"):
                writer.comment("Align to 32 bit").newline()
                writer.assign("end", "(uint8_t *)SPICE_ALIGN((size_t)end, 4)")
                writer.assign("*%s" % dest, "(void *)end")
                writer.assign("end", "%s(message_start, message_end, end, &ptr_info[%s], minor)" % (function, index))
                writer.error_check("end == NULL")
    writer.newline()

def write_nofree(writer):
    if writer.is_generated("helper", "nofree"):
        return
    writer = writer.function_helper()
    scope = writer.function("nofree", "static void", "uint8_t *data")
    writer.end_block()

def write_msg_parser(writer, message):
    msg_name = message.c_name()
    function_name = "parse_%s" % msg_name
    if writer.is_generated("demarshaller", function_name):
        return function_name
    writer.set_is_generated("demarshaller", function_name)

    msg_type = message.c_type()
    msg_sizeof = message.sizeof()

    want_mem_size = (len(message.members) != 1 or message.members[0].is_fixed_nw_size()
                         or not message.members[0].is_array())

    writer.newline()
    if message.has_attr("ifdef"):
        writer.ifdef(message.attributes["ifdef"][0])
    parent_scope = writer.function(function_name,
                                   "uint8_t *",
                                   "uint8_t *message_start, uint8_t *message_end, int minor, size_t *size, message_destructor_t *free_message", True)
    parent_scope.variable_def("SPICE_GNUC_UNUSED uint8_t *", "pos")
    parent_scope.variable_def("uint8_t *", "start = message_start")
    parent_scope.variable_def("uint8_t *", "data = NULL")
    parent_scope.variable_def("size_t", "nw_size")
    if want_mem_size:
        parent_scope.variable_def("size_t", "mem_size")
    if not message.has_attr("nocopy"):
        parent_scope.variable_def("uint8_t *", "in", "end")
    num_pointers = message.get_num_pointers()
    if  num_pointers != 0:
        parent_scope.variable_def("SPICE_GNUC_UNUSED intptr_t", "ptr_size")
        parent_scope.variable_def("uint32_t", "n_ptr=0")
        parent_scope.variable_def("PointerInfo", "ptr_info[%s]" % num_pointers)
    writer.newline()

    write_parser_helpers(writer)

    write_validate_container(writer, None, message, "start", parent_scope, True,
                             want_mem_size=want_mem_size, want_extra_size=False)

    writer.newline()

    writer.comment("Check if message fits in reported side").newline()
    with writer.block("if (start + nw_size > message_end)"):
        writer.statement("return NULL")

    writer.newline().comment("Validated extents and calculated size").newline()

    if message.has_attr("nocopy"):
        write_nofree(writer)
        writer.assign("data", "message_start")
        writer.assign("*size", "message_end - message_start")
        writer.assign("*free_message", "nofree")
    else:
        writer.assign("data", "(uint8_t *)malloc(mem_size)")
        writer.error_check("data == NULL")
        writer.assign("end", "data + %s" % (msg_sizeof))
        writer.assign("in", "start").newline()

        # avoid defined and assigned but not used warnings of gcc 4.6.0+
        if message.is_extra_size() or not message.is_fixed_nw_size() or message.get_fixed_nw_size() > 0:
            dest = RootDemarshallingDestination(None, msg_type, msg_sizeof, "data")
            dest.reuse_scope = parent_scope
            write_container_parser(writer, message, dest)

        writer.newline()
        writer.statement("assert(in <= message_end)")

        if num_pointers != 0:
            write_ptr_info_check(writer)

        writer.statement("assert(end <= data + mem_size)")

        writer.newline()
        writer.assign("*size", "end - data")
        writer.assign("*free_message", "(message_destructor_t) free")

    writer.statement("return data")
    writer.newline()
    if writer.has_error_check:
        writer.label("error")
        with writer.block("if (data != NULL)"):
            writer.statement("free(data)")
        writer.statement("return NULL")
    writer.end_block()

    if message.has_attr("ifdef"):
        writer.endif(message.attributes["ifdef"][0])

    return function_name

def write_channel_parser(writer, channel, server):
    writer.newline()
    ids = {}
    min_id = 1000000
    if server:
        messages = channel.server_messages
    else:
        messages = channel.client_messages
    for m in messages:
        ids[m.value] = m

    ranges = []
    ids2 = ids.copy()
    while len(ids2) > 0:
        end = start = min(ids2.keys())
        while ids2.has_key(end):
            del ids2[end]
            end = end + 1

        ranges.append( (start, end) )

    if server:
        function_name = "parse_%s_msg" % channel.name
    else:
        function_name = "parse_%s_msgc" % channel.name
    writer.newline()
    if channel.has_attr("ifdef"):
        writer.ifdef(channel.attributes["ifdef"][0])
    scope = writer.function(function_name,
                            "static uint8_t *",
                            "uint8_t *message_start, uint8_t *message_end, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message")

    helpers = writer.function_helper()

    d = 0
    for r in ranges:
        d = d + 1
        writer.write("static parse_msg_func_t funcs%d[%d] = " % (d, r[1] - r[0]))
        writer.begin_block()
        for i in range(r[0], r[1]):
            func = write_msg_parser(helpers, ids[i].message_type)
            writer.write(func)
            if i != r[1] -1:
                writer.write(",")
            writer.newline()

        writer.end_block(semicolon = True)

    d = 0
    for r in ranges:
        d = d + 1
        with writer.if_block("message_type >= %d && message_type < %d" % (r[0], r[1]), d > 1, False):
            writer.statement("return funcs%d[message_type-%d](message_start, message_end, minor, size_out, free_message)" % (d, r[0]))
    writer.newline()

    writer.statement("return NULL")
    writer.end_block()
    if channel.has_attr("ifdef"):
        writer.endif(channel.attributes["ifdef"][0])

    return function_name

def write_get_channel_parser(writer, channel_parsers, max_channel, is_server):
    writer.newline()
    if is_server:
        function_name = "spice_get_server_channel_parser" + writer.public_prefix
    else:
        function_name = "spice_get_client_channel_parser" + writer.public_prefix

    scope = writer.function(function_name,
                            "spice_parse_channel_func_t",
                            "uint32_t channel, unsigned int *max_message_type")

    writer.write("static struct {spice_parse_channel_func_t func; unsigned int max_messages; } channels[%d] = " % (max_channel+1))
    writer.begin_block()
    channel = None
    for i in range(0, max_channel + 1):
        if channel_parsers.has_key(i):
            channel = channel_parsers[i][0]
            if channel.has_attr("ifdef"):
                writer.ifdef(channel.attributes["ifdef"][0])
            writer.write("{ ")
            writer.write(channel_parsers[i][1])
            writer.write(", ")

            max_msg = 0
            if is_server:
                messages = channel.server_messages
            else:
                messages = channel.client_messages
            for m in messages:
                max_msg = max(max_msg, m.value)
            writer.write(max_msg)
            writer.write("}")
        else:
            writer.write("{ NULL, 0 }")

        if i != max_channel:
            writer.write(",")
        writer.newline()
        if channel and channel.has_attr("ifdef"):
            writer.ifdef_else(channel.attributes["ifdef"][0])
            writer.write("{ NULL, 0 }")
            if i != max_channel:
                writer.write(",")
            writer.newline()
            writer.endif(channel.attributes["ifdef"][0])
    writer.end_block(semicolon = True)

    with writer.if_block("channel < %d" % (max_channel + 1)):
        with writer.if_block("max_message_type != NULL"):
            writer.assign("*max_message_type", "channels[channel].max_messages")
        writer.statement("return channels[channel].func")

    writer.statement("return NULL")
    writer.end_block()


def write_full_protocol_parser(writer, is_server):
    writer.newline()
    if is_server:
        function_name = "spice_parse_msg"
    else:
        function_name = "spice_parse_reply"
    scope = writer.function(function_name + writer.public_prefix,
                            "uint8_t *",
                            "uint8_t *message_start, uint8_t *message_end, uint32_t channel, uint16_t message_type, int minor, size_t *size_out, message_destructor_t *free_message")
    scope.variable_def("spice_parse_channel_func_t", "func" )

    if is_server:
        writer.assign("func", "spice_get_server_channel_parser%s(channel, NULL)" % writer.public_prefix)
    else:
        writer.assign("func", "spice_get_client_channel_parser%s(channel, NULL)" % writer.public_prefix)

    with writer.if_block("func != NULL"):
        writer.statement("return func(message_start, message_end, message_type, minor, size_out, free_message)")

    writer.statement("return NULL")
    writer.end_block()

def write_protocol_parser(writer, proto, is_server):
    max_channel = 0
    parsers = {}

    for channel in proto.channels:
        max_channel = max(max_channel, channel.value)

        parsers[channel.value] = (channel.channel_type, write_channel_parser(writer, channel.channel_type, is_server))

    write_get_channel_parser(writer, parsers, max_channel, is_server)
    write_full_protocol_parser(writer, is_server)

def write_includes(writer):
    writer.writeln("#include <string.h>")
    writer.writeln("#include <assert.h>")
    writer.writeln("#include <stdlib.h>")
    writer.writeln("#include <stdio.h>")
    writer.writeln("#include <spice/protocol.h>")
    writer.writeln("#include <spice/macros.h>")
    writer.writeln('#include "mem.h"')
    writer.newline()
    writer.writeln("#ifdef _MSC_VER")
    writer.writeln("#pragma warning(disable:4101)")
    writer.writeln("#endif")
