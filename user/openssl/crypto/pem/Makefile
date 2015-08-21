#
# OpenSSL/crypto/pem/Makefile
#

DIR=	pem
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= pem_sign.c pem_seal.c pem_info.c pem_lib.c pem_all.c pem_err.c \
	pem_x509.c pem_xaux.c pem_oth.c pem_pk8.c pem_pkey.c pvkfmt.c

LIBOBJ=	pem_sign.o pem_seal.o pem_info.o pem_lib.o pem_all.o pem_err.o \
	pem_x509.o pem_xaux.o pem_oth.o pem_pk8.o pem_pkey.o pvkfmt.o

SRC= $(LIBSRC)

EXHEADER= pem.h pem2.h
HEADER=	$(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links: $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../../include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../../test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl ../../apps $(APPS)

install:
	@[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	@headerlist="$(EXHEADER)"; for i in $$headerlist ; \
	do  \
	(cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	done;

tags:
	ctags $(SRC)

tests:

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	@[ -n "$(MAKEDEPEND)" ] # should be set by upper Makefile...
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.

pem_all.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_all.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_all.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_all.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
pem_all.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pem_all.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pem_all.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pem_all.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pem_all.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pem_all.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_all.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_all.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
pem_all.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pem_all.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pem_all.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pem_all.o: ../cryptlib.h ../sgx.h pem_all.c
pem_err.o: ../../include/openssl/../sgx.h ../../include/openssl/asn1.h
pem_err.o: ../../include/openssl/bio.h ../../include/openssl/buffer.h
pem_err.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
pem_err.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
pem_err.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
pem_err.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
pem_err.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pem_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
pem_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem.h
pem_err.o: ../../include/openssl/pem2.h ../../include/openssl/pkcs7.h
pem_err.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pem_err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pem_err.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pem_err.o: pem_err.c
pem_info.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_info.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_info.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_info.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
pem_info.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
pem_info.o: ../../include/openssl/ecdsa.h ../../include/openssl/err.h
pem_info.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
pem_info.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pem_info.o: ../../include/openssl/opensslconf.h
pem_info.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_info.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_info.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
pem_info.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pem_info.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pem_info.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pem_info.o: ../cryptlib.h ../sgx.h pem_info.c
pem_lib.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_lib.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_lib.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
pem_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pem_lib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pem_lib.o: ../../include/openssl/engine.h ../../include/openssl/err.h
pem_lib.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
pem_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pem_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
pem_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pem.h
pem_lib.o: ../../include/openssl/pem2.h ../../include/openssl/pkcs12.h
pem_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
pem_lib.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pem_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pem_lib.o: ../../include/openssl/ui.h ../../include/openssl/ui_compat.h
pem_lib.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pem_lib.o: ../asn1/asn1_locl.h ../cryptlib.h ../sgx.h pem_lib.c
pem_oth.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_oth.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_oth.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_oth.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pem_oth.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pem_oth.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pem_oth.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pem_oth.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pem_oth.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_oth.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_oth.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
pem_oth.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pem_oth.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pem_oth.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pem_oth.o: ../cryptlib.h ../sgx.h pem_oth.c
pem_pk8.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_pk8.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_pk8.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_pk8.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pem_pk8.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pem_pk8.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pem_pk8.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pem_pk8.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pem_pk8.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_pk8.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_pk8.o: ../../include/openssl/pkcs12.h ../../include/openssl/pkcs7.h
pem_pk8.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
pem_pk8.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pem_pk8.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pem_pk8.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h pem_pk8.c
pem_pkey.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_pkey.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_pkey.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_pkey.o: ../../include/openssl/dh.h ../../include/openssl/e_os2.h
pem_pkey.o: ../../include/openssl/ec.h ../../include/openssl/ecdh.h
pem_pkey.o: ../../include/openssl/ecdsa.h ../../include/openssl/engine.h
pem_pkey.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pem_pkey.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pem_pkey.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pem_pkey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_pkey.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_pkey.o: ../../include/openssl/pkcs12.h ../../include/openssl/pkcs7.h
pem_pkey.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
pem_pkey.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pem_pkey.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pem_pkey.o: ../../include/openssl/x509_vfy.h ../asn1/asn1_locl.h ../cryptlib.h
pem_pkey.o: ../sgx.h pem_pkey.c
pem_seal.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_seal.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_seal.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_seal.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pem_seal.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pem_seal.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pem_seal.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pem_seal.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pem_seal.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_seal.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_seal.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
pem_seal.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
pem_seal.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pem_seal.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pem_seal.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h pem_seal.c
pem_sign.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_sign.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_sign.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_sign.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pem_sign.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pem_sign.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pem_sign.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pem_sign.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pem_sign.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_sign.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_sign.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
pem_sign.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
pem_sign.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
pem_sign.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
pem_sign.o: ../cryptlib.h ../sgx.h pem_sign.c
pem_x509.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_x509.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_x509.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_x509.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pem_x509.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pem_x509.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pem_x509.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pem_x509.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pem_x509.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_x509.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_x509.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pem_x509.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pem_x509.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pem_x509.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h pem_x509.c
pem_xaux.o: ../../e_os.h ../../include/openssl/../sgx.h
pem_xaux.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pem_xaux.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pem_xaux.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pem_xaux.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pem_xaux.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pem_xaux.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pem_xaux.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pem_xaux.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pem_xaux.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pem_xaux.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pem_xaux.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pem_xaux.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pem_xaux.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h pem_xaux.c
pvkfmt.o: ../../e_os.h ../../include/openssl/../sgx.h
pvkfmt.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pvkfmt.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
pvkfmt.o: ../../include/openssl/crypto.h ../../include/openssl/dsa.h
pvkfmt.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pvkfmt.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pvkfmt.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pvkfmt.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pvkfmt.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pvkfmt.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pvkfmt.o: ../../include/openssl/pem.h ../../include/openssl/pem2.h
pvkfmt.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
pvkfmt.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
pvkfmt.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pvkfmt.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pvkfmt.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h pvkfmt.c
