import gdb
numEpcm = 64

class InfoSecsCommand (gdb.Command):
    """For debugging Secs data structue"""

    PT_SECS = 0
    secsType = 0

    def __init__ (self):
        super (InfoSecsCommand, self).__init__ ("info secs",
                                                gdb.COMMAND_STATUS,
                                                gdb.COMPLETE_NONE)

    def invoke (self, arg, from_tty):
	# secs_t symbol check
	try:
	    self.secsType = gdb.lookup_type('secs_t')
            pageType = gdb.lookup_type('page_type_t')
            self.PT_SECS  = (pageType.fields())[0].enumval
	except RuntimeError, e:
	    print "type secs_t nof found"
	    return 	

	# get args, arg_list[0] indicate eid of enclave
	arg_list = gdb.string_to_argv(arg)
        
        # find secs 
        secs = self._find_secs(arg_list[0])
        if secs == -1 :
            print "secs not found"
            return;

        # print sec info
        self._print_secs(secs)

    #uint32_t            reserved1[7];
    #attributes_t        attributes;    // Attributes of Enclave: (pg 2-4)
    #uint8_t             mrEnclave[32]; // Measurement Reg of encl. build process
    #uint8_t             reserved2[32];
    #uint8_t             mrSigner[32];  // Measurement Reg extended with pub key that verified the enclave
    #uint8_t             reserved3[96];
    #uint16_t            isvprodID;     // Product ID of enclave
    #uint16_t            isvsvn;        // Security Version Number (SVN) of enclave
    #uint64_t            mrEnclaveUpdateCounter; // Hack: place update counter here


    #find secs of enclave[eid]
    def _find_secs(self, eid):
        #type case eid to int
        eid = int(eid, 10)
        epcmObj = gdb.parse_and_eval("epcm")
        for i in range (0, numEpcm):
            if epcmObj[i]['valid'] == 1 and epcmObj[i]['page_type'] == self.PT_SECS :
                #get secs from the epcmObj
                secs = (epcmObj[i])['epcPageAddress']
                #typecast to secs_t
                secs = (secs).cast(self.secsType.pointer())
                #now get eid field in secs_t
                secs_eid = ((secs['eid_reserved'])['eid_pad'])['eid']
                if secs_eid == eid :
                    #Match Found!!
                    print "epcm[%s]:%s is PT_SECS for eid:%s" % (i, epcmObj[i].address, eid)
                    return secs
        return -1
                
  
    def _print_secs (self, secs):
        gdb.write("size            : %x\n" % secs['size'])
        gdb.write("baseAddr        : %x\n" % secs['baseAddr'])
        gdb.write("ssaFrameSize    : %x\n" % secs['ssaFrameSize'])
        print(    "reserved1       : %s" % secs['reserved1'])
        gdb.write("isvprodID       : %x\n" % secs['isvprodID'])
        gdb.write("isvsvn          : %x\n" % secs['isvsvn'])
        gdb.write("mrEncUpdateCount: %x\n" % secs['ssaFrameSize'])
        gdb.write("eid             : %x\n" % secs['eid_reserved']['eid_pad']['eid'])
        print(    "attributes      : %s" % secs['attributes'])
        print "mrEnclave"
        self._hexdump(secs['mrEnclave'], 32)
        print "reserved2"
        self._hexdump(secs['reserved2'], 32)
        print "mrSigner"
        self._hexdump(secs['mrSigner' ], 32)
        print "reserved3"
        self._hexdump(secs['reserved3'], 96)


    def _hexdump (self, addr, len):
        buff = []
        for k in range(16 + 1):
            buff.append(0)
        #process every byte in the data.
        for i in range(0, len):
            if i % 16 == 0 :
                if i != 0 :
                    print ""
                print "  %04x " % i,
            print " %02x" % addr[i],
            #store a printable ASCII character for later.
            #if addr[i] < 0x20 or addr[i] > 0x7e :
                #buff[i % 16] = '.'
            #else :
                #addr[i] -> gdbValue
                #print addr[i].dynamic_type
                #buff[i % 16] = addr[i].string()
            #buff[(i % 16) + 1] = '\0'

        while i % 16 != 0 :
            print "   ",
            i = i + 1
        print""
        #print "  %s" % buff

InfoSecsCommand()



