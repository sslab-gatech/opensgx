from __future__ import with_statement
import ptypes
import codegen

def write_includes(writer):
    writer.header.writeln("#include <spice/protocol.h>")
    writer.header.writeln('#include "marshaller.h"')
    writer.header.newline()
    writer.header.writeln("#ifndef _GENERATED_HEADERS_H")
    writer.header.writeln("#define _GENERATED_HEADERS_H")

    writer.writeln("#include <string.h>")
    writer.writeln("#include <assert.h>")
    writer.writeln("#include <stdlib.h>")
    writer.writeln("#include <stdio.h>")
    writer.writeln("#include <spice/protocol.h>")
    writer.writeln("#include <spice/macros.h>")
    writer.writeln('#include "marshaller.h"')
    writer.newline()
    writer.writeln("#ifdef _MSC_VER")
    writer.writeln("#pragma warning(disable:4101)")
    writer.writeln("#pragma warning(disable:4018)")
    writer.writeln("#endif")
    writer.newline()

class MarshallingSource:
    def __init__(self):
        pass

    def child_at_end(self, t):
        return RootMarshallingSource(self, t.c_type(), t.sizeof())

    def child_sub(self, containee):
        return SubMarshallingSource(self, containee)

    def declare(self, writer):
        return writer.optional_block(self.reuse_scope)

    def is_toplevel(self):
        return self.parent_src == None and not self.is_helper

class RootMarshallingSource(MarshallingSource):
    def __init__(self, parent_src, c_type, sizeof, pointer = None):
        self.is_helper = False
        self.reuse_scope = None
        self.parent_src = parent_src
        if parent_src:
            self.base_var = codegen.increment_identifier(parent_src.base_var)
        else:
            self.base_var = "src"
        self.c_type = c_type
        self.sizeof = sizeof
        self.pointer = pointer
        assert pointer != None

    def get_self_ref(self):
        return self.base_var

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

        writer.assign(self.base_var, "(%s *)%s" % (self.c_type, self.pointer))
        writer.newline()

        if self.reuse_scope:
            return writer.no_block(self.reuse_scope)
        else:
            return writer.partial_block(scope)

class SubMarshallingSource(MarshallingSource):
    def __init__(self, parent_src, containee):
        self.reuse_scope = None
        self.parent_src = parent_src
        self.base_var = parent_src.base_var
        self.containee = containee
        self.name = containee.name
        self.is_helper = False

    def get_self_ref(self):
        if self.containee.has_attr("to_ptr"):
            return "%s" % self.parent_src.get_ref(self.name)
        else:
            return "&%s" % self.parent_src.get_ref(self.name)

    def get_ref(self, member):
        if self.containee.has_attr("to_ptr"):
            return self.parent_src.get_ref(self.name) + "->" + member
        else:
            return self.parent_src.get_ref(self.name) + "." + member

def write_marshal_ptr_function(writer, target_type, is_helper=True):
    if target_type.is_array():
        marshal_function = "spice_marshall_array_%s" % target_type.element_type.primitive_type()
    else:
        marshal_function = "spice_marshall_%s" % target_type.name
    if writer.is_generated("marshaller", marshal_function):
        return marshal_function

    writer.set_is_generated("marshaller", marshal_function)

    names = target_type.get_pointer_names(False)
    names_args = ""
    if len(names) > 0:
        n = map(lambda name: ", SpiceMarshaller **%s_out" % name, names)
        names_args = "".join(n)

    header = writer.header
    if is_helper:
        writer = writer.function_helper()
    writer.header = header
    writer.out_prefix = ""
    if target_type.is_array():
        scope = writer.function(marshal_function, "SPICE_GNUC_UNUSED static void", "SpiceMarshaller *m, %s_t *ptr, unsigned count" % target_type.element_type.primitive_type() + names_args)
    else:
        scope = writer.function(marshal_function, "void", "SpiceMarshaller *m, %s *ptr" % target_type.c_type() + names_args)
        header.writeln("void " + marshal_function + "(SpiceMarshaller *m, %s *msg" % target_type.c_type() + names_args + ");")
    scope.variable_def("SPICE_GNUC_UNUSED SpiceMarshaller *", "m2")

    for n in names:
        writer.assign("*%s_out" % n, "NULL")

    writer.newline()

    if target_type.is_struct():
        src = RootMarshallingSource(None, target_type.c_type(), target_type.sizeof(), "ptr")
        src.reuse_scope = scope
        write_container_marshaller(writer, target_type, src)
    elif target_type.is_array() and target_type.element_type.is_primitive():
        with writer.index() as index:
            with writer.for_loop(index, "count") as array_scope:
                writer.statement("spice_marshaller_add_%s(m, *ptr++)" % (target_type.element_type.primitive_type()))
    else:
        writer.todo("Unsuppored pointer marshaller type")

    writer.end_block()

    return marshal_function

def get_array_size(array, container_src):
    if array.is_constant_length():
        return array.size
    elif array.is_identifier_length():
        return container_src.get_ref(array.size)
    elif array.is_remaining_length():
        raise NotImplementedError("remaining size array sizes marshalling not supported")
    elif array.is_image_size_length():
        bpp = array.size[1]
        width = array.size[2]
        rows = array.size[3]
        width_v = container_src.get_ref(width)
        rows_v = container_src.get_ref(rows)
        # TODO: Handle multiplication overflow
        if bpp == 8:
            return "(%s * %s)" % (width_v, rows_v)
        elif bpp == 1:
            return "(((%s + 7) / 8 ) * %s)" % (width_v, rows_v)
        else:
            return "(((%s * %s + 7) / 8 ) * %s)" % (bpp, width_v, rows_v)
    elif array.is_bytes_length():
        return container_src.get_ref(array.size[2])
    else:
        raise NotImplementedError("TODO array size type not handled yet: %s"  % array)

def write_array_marshaller(writer, member, array, container_src, scope):
    element_type = array.element_type

    if array.is_remaining_length():
        writer.comment("Remaining data must be appended manually").newline()
        return

    nelements = get_array_size(array, container_src)
    is_byte_size = array.is_bytes_length()

    element = "%s__element" % member.name

    if not scope.variable_defined(element):
        if array.has_attr("ptr_array"):
            stars = " **"
        else:
            stars = " *"
        scope.variable_def(element_type.c_type() + stars, element)
    element_array = element
    if array.has_attr("ptr_array"):
        element = "*" + element

    writer.assign(element_array, container_src.get_ref(member.name))

    if is_byte_size:
        size_start_var = "%s__size_start" % member.name
        scope.variable_def("size_t", size_start_var)
        writer.assign(size_start_var, "spice_marshaller_get_size(m)")

    with writer.index() as index:
        with writer.for_loop(index, nelements) as array_scope:
            if element_type.is_primitive():
                writer.statement("spice_marshaller_add_%s(m, *%s)" % (element_type.primitive_type(), element))
            elif element_type.is_struct():
                src2 = RootMarshallingSource(container_src, element_type.c_type(), element_type.sizeof(), element)
                src2.reuse_scope = array_scope
                write_container_marshaller(writer, element_type, src2)
            else:
                writer.todo("array element unhandled type").newline()

            writer.statement("%s++" % element_array)

    if is_byte_size:
        size_var = member.container.lookup_member(array.size[1])
        size_var_type = size_var.member_type
        var = "%s__ref" % array.size[1]
        writer.statement("spice_marshaller_set_%s(m, %s, spice_marshaller_get_size(m) - %s)" % (size_var_type.primitive_type(), var, size_start_var))

def write_pointer_marshaller(writer, member, src):
    t = member.member_type
    ptr_func = write_marshal_ptr_function(writer, t.target_type)
    submarshaller = "spice_marshaller_get_ptr_submarshaller(m, %d)" % (1 if member.get_fixed_nw_size() == 8 else 0)
    if member.has_attr("marshall"):
        rest_args = ""
        if t.target_type.is_array():
            rest_args = ", %s" % get_array_size(t.target_type, src)
        writer.assign("m2", submarshaller)
        if t.has_attr("nonnull"):
            writer.statement("%s(m2, %s%s)" % (ptr_func, src.get_ref(member.name), rest_args))
        else:
            with writer.if_block("%s != NULL" % src.get_ref(member.name)) as block:
                writer.statement("%s(m2, %s%s)" % (ptr_func, src.get_ref(member.name), rest_args))
    else:
        writer.assign("*%s_out" % (writer.out_prefix + member.name), submarshaller)

def write_switch_marshaller(writer, container, switch, src, scope):
    var = container.lookup_member(switch.variable)
    var_type = var.member_type

    saved_out_prefix = writer.out_prefix
    first = True
    for c in switch.cases:
        check = c.get_check(src.get_ref(switch.variable), var_type)
        m = c.member
        writer.out_prefix = saved_out_prefix
        if m.has_attr("outvar"):
            writer.out_prefix = "%s_%s" % (m.attributes["outvar"][0], writer.out_prefix)
        with writer.if_block(check, not first, False) as block:
            t = m.member_type
            if switch.has_attr("anon"):
                if t.is_struct():
                    src2 = src.child_sub(m)
                else:
                    src2 = src
            else:
                if t.is_struct():
                    src2 = src.child_sub(switch).child_sub(m)
                else:
                    src2 = src.child_sub(switch)
            src2.reuse_scope = block

            if t.is_struct():
                write_container_marshaller(writer, t, src2)
            elif t.is_pointer():
                write_pointer_marshaller(writer, m, src2)
            elif t.is_primitive():
                if m.has_attr("zero"):
                    writer.statement("spice_marshaller_add_%s(m, 0)" % (t.primitive_type()))
                else:
                    writer.statement("spice_marshaller_add_%s(m, %s)" % (t.primitive_type(), src2.get_ref(m.name)))
                #TODO validate e.g. flags and enums
            elif t.is_array():
                write_array_marshaller(writer, m, t, src2, scope)
            else:
                writer.todo("Can't handle type %s" % m.member_type)

            if switch.has_attr("fixedsize"):
                remaining = switch.get_fixed_nw_size() - t.get_fixed_nw_size()
                if remaining != 0:
                    writer.statement("spice_marshaller_reserve_space(m, %s)" % remaining)

        first = False
    if switch.has_attr("fixedsize"):
        with writer.block(" else"):
            writer.statement("spice_marshaller_reserve_space(m, %s)" % switch.get_fixed_nw_size())

    writer.newline()

def write_member_marshaller(writer, container, member, src, scope):
    if member.has_attr("outvar"):
        writer.out_prefix = "%s_%s" % (member.attributes["outvar"][0], writer.out_prefix)
    if member.has_attr("virtual"):
        writer.comment("Don't marshall @virtual %s" % member.name).newline()
        return
    if member.has_attr("nomarshal"):
        writer.comment("Don't marshall @nomarshal %s" % member.name).newline()
        return
    if member.is_switch():
        write_switch_marshaller(writer, container, member, src, scope)
        return

    t = member.member_type

    if t.is_pointer():
        write_pointer_marshaller(writer, member, src)
    elif t.is_primitive():
        if member.has_attr("zero"):
            writer.statement("spice_marshaller_add_%s(m, 0)" % (t.primitive_type()))
        if member.has_attr("bytes_count"):
            var = "%s__ref" % member.name
            scope.variable_def("void *", var)
            writer.statement("%s = spice_marshaller_add_%s(m, %s)" % (var, t.primitive_type(), 0))

        else:
            writer.statement("spice_marshaller_add_%s(m, %s)" % (t.primitive_type(), src.get_ref(member.name)))
    elif t.is_array():
        write_array_marshaller(writer, member, t, src, scope)
    elif t.is_struct():
        src2 = src.child_sub(member)
        writer.comment(member.name)
        write_container_marshaller(writer, t, src2)
    else:
        raise NotImplementedError("TODO can't handle parsing of %s" % t)

def write_container_marshaller(writer, container, src):
    saved_out_prefix = writer.out_prefix
    with src.declare(writer) as scope:
        for m in container.members:
            writer.out_prefix = saved_out_prefix
            write_member_marshaller(writer, container, m, src, scope)

def write_message_marshaller(writer, message, is_server, private):
    if message.has_attr("ifdef"):
        writer.ifdef(message.attributes["ifdef"][0])
    writer.out_prefix = ""
    function_name = "spice_marshall_" + message.c_name()
    if writer.is_generated("marshaller", function_name):
        return function_name
    writer.set_is_generated("marshaller", function_name)

    names = message.get_pointer_names(False)
    names_args = ""
    if len(names) > 0:
        n = map(lambda name: ", SpiceMarshaller **%s_out" % name, names)
        names_args = "".join(n)

    if not private:
        writer.header.writeln("void " + function_name + "(SpiceMarshaller *m, %s *msg" % message.c_type() + names_args + ");")

    scope = writer.function(function_name,
                            "static void" if private else "void",
                            "SpiceMarshaller *m, %s *msg" % message.c_type() + names_args)
    scope.variable_def("SPICE_GNUC_UNUSED SpiceMarshaller *", "m2")

    for n in names:
        writer.assign("*%s_out" % n, "NULL")

    # fix warnings about unused variables by not creating body if no members to parse
    if any(x.is_fixed_nw_size() for x in message.members):
        src = RootMarshallingSource(None, message.c_type(), message.sizeof(), "msg")
        src.reuse_scope = scope

        write_container_marshaller(writer, message, src)

    writer.end_block()
    if message.has_attr("ifdef"):
        writer.endif(message.attributes["ifdef"][0])
    writer.newline()
    return function_name

def write_protocol_marshaller(writer, proto, is_server, private_marshallers):
    functions = {}
    for c in proto.channels:
        channel = c.channel_type
        if channel.has_attr("ifdef"):
            writer.ifdef(channel.attributes["ifdef"][0])
            writer.header.ifdef(channel.attributes["ifdef"][0])
        if is_server:
            for m in channel.client_messages:
                message = m.message_type
                f = write_message_marshaller(writer, message, is_server, private_marshallers)
                if channel.has_attr("ifdef") and not functions.has_key(f):
                    functions[f] = channel.attributes["ifdef"][0]
                elif message.has_attr("ifdef") and not functions.has_key(f):
                    functions[f] = message.attributes["ifdef"][0]
                else:
                    functions[f] = True
        else:
            for m in channel.server_messages:
                message = m.message_type
                f = write_message_marshaller(writer, message, is_server, private_marshallers)
                if channel.has_attr("ifdef") and not functions.has_key(f):
                    functions[f] = channel.attributes["ifdef"][0]
                elif message.has_attr("ifdef") and not functions.has_key(f):
                    functions[f] = message.attributes["ifdef"][0]
                else:
                    functions[f] = True
        if channel.has_attr("ifdef"):
            writer.endif(channel.attributes["ifdef"][0])
            writer.header.endif(channel.attributes["ifdef"][0])

    if private_marshallers:
        scope = writer.function("spice_message_marshallers_get" +  writer.public_prefix,
                                "SpiceMessageMarshallers *",
                                "void")
        writer.writeln("static SpiceMessageMarshallers marshallers = {NULL};").newline()
        for f in sorted(functions.keys()):
            member = f[len("spice_marshall_"):]
            if not member.startswith("msg"):
                member = "msg_" + member
            if functions[f] != True:
                writer.ifdef(functions[f])
            writer.assign("marshallers.%s" % member, f)
            if functions[f] != True:
                writer.endif(functions[f])

        writer.newline()
        writer.statement("return &marshallers")
        writer.end_block()
        writer.newline()

def write_trailer(writer):
    writer.header.writeln("#endif")
