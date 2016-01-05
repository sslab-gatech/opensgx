TOR := tor-0.2.5.10

all: $(TOR)
	(cd chutney; ./chutney configure networks/basic-sgx-tor; \
       ./chutney start networks/basic-sgx-tor;)

$(TOR): FORCE
	(cd tor-0.2.5.10; make; sudo make install)

.PHONY: FORCE

