import gdb
numEpcm = 64

class InfoEpcmCommand (gdb.Command):
    """For debugging EPCM data structue
    Calling info epcm prints EPCM address and its contents.
    To designate start and end index, type "info epcm start end".
    e.g.) info epcm 0 3 will print epcm[0]~ epcm[3]. """

    def __init__ (self):
        super (InfoEpcmCommand, self).__init__ ("info epcm",
                                                gdb.COMMAND_STATUS,
                                                gdb.COMPLETE_NONE)

    def invoke (self, arg, from_tty):
	# EPCM symbol check
	try:
	    epcmType = gdb.lookup_type('epcm_entry_t')
	except RuntimeError, e:
	    print "type epcm_entry_t nof found"
	    return 	

	# get args
	arg_list = gdb.string_to_argv(arg)
	if len(arg_list) < 2:
	    start = 0
	    end = numEpcm
	else:
	    start = int(arg_list[0], 10)
	    end = int(arg_list[1], 10)
	    if end > numEpcm:
	    	end = numEpcm
	    if start < 0:
		start = 0

	# get epcm object
	epcmObj = gdb.parse_and_eval("epcm")
	# print epcm info
	self._print_epcm(epcmObj, start, end)

	
    def _print_epcm (self, obj, start, end):

	for i in range (start, end + 1):
	    print "EPCM[%s]:%s" % (i, obj[i].address)
	    print obj[i]

InfoEpcmCommand()
