import gdb

class InfoEpcCommand (gdb.Command):
    """For debugging EPC data structue"""

    def __init__ (self):
        super (InfoEpcCommand, self).__init__ ("info epc",
                                                gdb.COMMAND_STATUS,
                                                gdb.COMPLETE_NONE)

    def invoke (self, arg, from_tty):
	# EPC symbol check
	try:
	    epcType = gdb.lookup_type('epc_t')
	except RuntimeError, e:
	    print "type epc_t nof found"
	    return 	

	# get args
	arg_list = gdb.string_to_argv(arg)

	# get epc first & end page
	epcBase = gdb.parse_and_eval("EPC_BaseAddr")
	epcEnd = gdb.parse_and_eval("EPC_EndAddr")
	if (epcBase == 0 and epcEnd == 0):
	    print "EPC is not allocated yet"
	    return

	epcBase += 1
	# print epc info
	self._print_epc(epcType, epcBase, epcEnd)
	
    def _print_epc (self, epcType, start, end):
	    endIndex = (end - start) / epcType.sizeof
	    for i in range (0, endIndex):
	        epc = (start + epcType.sizeof * i).cast( epcType.pointer() )
		print "epc[%d]: %s" % (i, epc)

InfoEpcCommand()
