
# An exit policy that allows exiting to IPv6 localhost
#ExitPolicy accept6 [::1]:*
IPv6Exit 1

# An exit policy that allows exiting to the entire internet on HTTP(S)
# This may be required to work around #11264 with microdescriptors enabled
# "The core of this issue appears to be that the Exit flag code is
#  optimistic (just needs a /8 [IP6?]  and 2 ports), but the microdescriptor
#  exit policy summary code is pessimistic (needs the entire internet)."
# An alternative is to disable microdescriptors and use regular
# descriptors, as they do not suffer from this issue.
#ExitPolicy accept6 *:80
#ExitPolicy accept6 *:443

#ExitPolicy reject6 *:*
# OR
ExitPolicy accept6 *:*
