import gdb

class InfoEpcCommand (gdb.Command):
    """For debugging EPC data structue
    Just calling "info epc" prints address of all epc page.
    To print specific epc page, pass its index as a parameter
    e.g.) info epc 0 """

    epcType = 0

    def __init__ (self):
        super (InfoEpcCommand, self).__init__ ("info epc",
                                                gdb.COMMAND_STATUS,
                                                gdb.COMPLETE_NONE)

    def invoke (self, arg, from_tty):
	# EPC symbol check
	try:
	    self.epcType = gdb.lookup_type('epc_t')
	except RuntimeError, e:
	    print "type epc_t nof found"
	    return 	

	# get args, arg_list[0] indicate index
	arg_list = gdb.string_to_argv(arg)

	# get epc first & end page
	epcBase = gdb.parse_and_eval("EPC_BaseAddr")
	epcEnd = gdb.parse_and_eval("EPC_EndAddr")
	if (epcBase == 0 and epcEnd == 0):
	    print "EPC is not allocated yet"
	    return

	epcBase += 1

	# print epc info
	if (len(arg_list) == 1):
	    self._print_epc(epcBase, arg_list[0])
	else:
	    self._print_epc_list(epcBase, epcEnd)
	
    def _print_epc_list (self, epcBase, epcEnd):
	endIndex = (epcEnd- epcBase) / self.epcType.sizeof
	for i in range (0, endIndex):
	    epc = (epcBase + (self.epcType.sizeof * i)).cast(self.epcType.pointer())
	    print "epc[%2d]: %s" % (i, epc)

    def _print_epc (self, epcBase, index):
	index = int(index, 10)
	epc = (epcBase + (self.epcType.sizeof * index)).cast(self.epcType.pointer())
	print "epc[%2d]:%s" % (index, epc)

InfoEpcCommand()
