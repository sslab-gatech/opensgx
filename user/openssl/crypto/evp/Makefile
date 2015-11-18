#
# OpenSSL/crypto/evp/Makefile
#

DIR=	evp
TOP=	../..
CC=	cc
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

CFLAGS= $(INCLUDES) $(CFLAG)

GENERAL=Makefile
TEST=evp_test.c
TESTDATA=evptests.txt
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC= encode.c digest.c evp_enc.c evp_key.c evp_acnf.c evp_cnf.c \
	e_des.c e_bf.c e_idea.c e_des3.c e_camellia.c\
	e_rc4.c e_aes.c names.c e_seed.c \
	e_xcbc_d.c e_rc2.c e_cast.c e_rc5.c \
	m_null.c m_md2.c m_md4.c m_md5.c m_sha.c m_sha1.c m_wp.c \
	m_dss.c m_dss1.c m_mdc2.c m_ripemd.c m_ecdsa.c\
	p_open.c p_seal.c p_sign.c p_verify.c p_lib.c p_enc.c p_dec.c \
	bio_md.c bio_b64.c bio_enc.c evp_err.c e_null.c \
	c_all.c c_allc.c c_alld.c evp_lib.c bio_ok.c \
	evp_pkey.c evp_pbe.c p5_crpt.c p5_crpt2.c \
	e_old.c pmeth_lib.c pmeth_fn.c pmeth_gn.c m_sigver.c \
	e_aes_cbc_hmac_sha1.c e_aes_cbc_hmac_sha256.c e_rc4_hmac_md5.c

LIBOBJ=	encode.o digest.o evp_enc.o evp_key.o evp_acnf.o evp_cnf.o \
	e_des.o e_bf.o e_idea.o e_des3.o e_camellia.o\
	e_rc4.o e_aes.o names.o e_seed.o \
	e_xcbc_d.o e_rc2.o e_cast.o e_rc5.o \
	m_null.o m_md2.o m_md4.o m_md5.o m_sha.o m_sha1.o m_wp.o \
	m_dss.o m_dss1.o m_mdc2.o m_ripemd.o m_ecdsa.o\
	p_open.o p_seal.o p_sign.o p_verify.o p_lib.o p_enc.o p_dec.o \
	bio_md.o bio_b64.o bio_enc.o evp_err.o e_null.o \
	c_all.o c_allc.o c_alld.o evp_lib.o bio_ok.o \
	evp_pkey.o evp_pbe.o p5_crpt.o p5_crpt2.o \
	e_old.o pmeth_lib.o pmeth_fn.o pmeth_gn.o m_sigver.o \
	e_aes_cbc_hmac_sha1.o e_aes_cbc_hmac_sha256.o e_rc4_hmac_md5.o

SRC= $(LIBSRC)

EXHEADER= evp.h
HEADER=	evp_locl.h $(EXHEADER)

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

links:
	@$(PERL) $(TOP)/util/mklink.pl ../../include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../../test $(TEST)
	@[ -f $(TESTDATA) ] && cp $(TESTDATA) ../../test && echo "$(TESTDATA) -> ../../test/$(TESTDATA)"
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

bio_b64.o: ../../e_os.h ../../include/openssl/../sgx.h
bio_b64.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
bio_b64.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bio_b64.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_b64.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
bio_b64.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
bio_b64.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bio_b64.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
bio_b64.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bio_b64.o: ../cryptlib.h ../sgx.h bio_b64.c
bio_enc.o: ../../e_os.h ../../include/openssl/../sgx.h
bio_enc.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
bio_enc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bio_enc.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_enc.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
bio_enc.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
bio_enc.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bio_enc.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
bio_enc.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bio_enc.o: ../cryptlib.h ../sgx.h bio_enc.c
bio_md.o: ../../e_os.h ../../include/openssl/../sgx.h
bio_md.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
bio_md.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bio_md.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_md.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
bio_md.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
bio_md.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bio_md.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
bio_md.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
bio_md.o: ../cryptlib.h ../sgx.h bio_md.c
bio_ok.o: ../../e_os.h ../../include/openssl/../sgx.h
bio_ok.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
bio_ok.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
bio_ok.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
bio_ok.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
bio_ok.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
bio_ok.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
bio_ok.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
bio_ok.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
bio_ok.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h bio_ok.c
c_all.o: ../../e_os.h ../../include/openssl/../sgx.h
c_all.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
c_all.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
c_all.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
c_all.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
c_all.o: ../../include/openssl/engine.h ../../include/openssl/err.h
c_all.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
c_all.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
c_all.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
c_all.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
c_all.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
c_all.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
c_all.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
c_all.o: ../cryptlib.h ../sgx.h c_all.c
c_allc.o: ../../e_os.h ../../include/openssl/../sgx.h
c_allc.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
c_allc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
c_allc.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
c_allc.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
c_allc.o: ../../include/openssl/err.h ../../include/openssl/evp.h
c_allc.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
c_allc.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
c_allc.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
c_allc.o: ../../include/openssl/pkcs12.h ../../include/openssl/pkcs7.h
c_allc.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
c_allc.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
c_allc.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
c_allc.o: ../cryptlib.h ../sgx.h c_allc.c
c_alld.o: ../../e_os.h ../../include/openssl/../sgx.h
c_alld.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
c_alld.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
c_alld.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
c_alld.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
c_alld.o: ../../include/openssl/err.h ../../include/openssl/evp.h
c_alld.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
c_alld.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
c_alld.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
c_alld.o: ../../include/openssl/pkcs12.h ../../include/openssl/pkcs7.h
c_alld.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
c_alld.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
c_alld.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
c_alld.o: ../cryptlib.h ../sgx.h c_alld.c
digest.o: ../../e_os.h ../../include/openssl/../sgx.h
digest.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
digest.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
digest.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
digest.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
digest.o: ../../include/openssl/engine.h ../../include/openssl/err.h
digest.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
digest.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
digest.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
digest.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
digest.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
digest.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
digest.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
digest.o: ../cryptlib.h ../sgx.h digest.c
e_aes.o: ../../include/openssl/../sgx.h ../../include/openssl/aes.h
e_aes.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_aes.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
e_aes.o: ../../include/openssl/err.h ../../include/openssl/evp.h
e_aes.o: ../../include/openssl/lhash.h ../../include/openssl/modes.h
e_aes.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_aes.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_aes.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
e_aes.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_aes.o: ../../include/openssl/symhacks.h ../modes/modes_lcl.h e_aes.c
e_aes.o: evp_locl.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/../sgx.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/aes.h ../../include/openssl/asn1.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/bio.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/crypto.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/e_os2.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/evp.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/modes.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/obj_mac.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/objects.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/opensslconf.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/opensslv.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/ossl_typ.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/rand.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/safestack.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/sha.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/stack.h
e_aes_cbc_hmac_sha1.o: ../../include/openssl/symhacks.h ../modes/modes_lcl.h
e_aes_cbc_hmac_sha1.o: ../sgx.h e_aes_cbc_hmac_sha1.c
e_aes_cbc_hmac_sha256.o: ../../include/openssl/../sgx.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/aes.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/asn1.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/bio.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/crypto.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/e_os2.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/evp.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/modes.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/obj_mac.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/objects.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/opensslconf.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/opensslv.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/ossl_typ.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/rand.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/safestack.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/sha.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/stack.h
e_aes_cbc_hmac_sha256.o: ../../include/openssl/symhacks.h ../modes/modes_lcl.h
e_aes_cbc_hmac_sha256.o: ../sgx.h e_aes_cbc_hmac_sha256.c
e_bf.o: ../../e_os.h ../../include/openssl/../sgx.h
e_bf.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_bf.o: ../../include/openssl/blowfish.h ../../include/openssl/buffer.h
e_bf.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
e_bf.o: ../../include/openssl/err.h ../../include/openssl/evp.h
e_bf.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_bf.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_bf.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_bf.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_bf.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h e_bf.c
e_bf.o: evp_locl.h
e_camellia.o: ../../include/openssl/../sgx.h ../../include/openssl/asn1.h
e_camellia.o: ../../include/openssl/bio.h ../../include/openssl/camellia.h
e_camellia.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
e_camellia.o: ../../include/openssl/err.h ../../include/openssl/evp.h
e_camellia.o: ../../include/openssl/lhash.h ../../include/openssl/modes.h
e_camellia.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_camellia.o: ../../include/openssl/opensslconf.h
e_camellia.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_camellia.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_camellia.o: ../../include/openssl/symhacks.h ../modes/modes_lcl.h
e_camellia.o: e_camellia.c evp_locl.h
e_cast.o: ../../e_os.h ../../include/openssl/../sgx.h
e_cast.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_cast.o: ../../include/openssl/buffer.h ../../include/openssl/cast.h
e_cast.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
e_cast.o: ../../include/openssl/err.h ../../include/openssl/evp.h
e_cast.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_cast.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_cast.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_cast.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_cast.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h e_cast.c
e_cast.o: evp_locl.h
e_des.o: ../../e_os.h ../../include/openssl/../sgx.h
e_des.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_des.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_des.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
e_des.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_des.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
e_des.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_des.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_des.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
e_des.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_des.o: ../../include/openssl/symhacks.h ../../include/openssl/ui.h
e_des.o: ../../include/openssl/ui_compat.h ../cryptlib.h ../sgx.h e_des.c
e_des.o: evp_locl.h
e_des3.o: ../../e_os.h ../../include/openssl/../sgx.h
e_des3.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_des3.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_des3.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
e_des3.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_des3.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
e_des3.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_des3.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_des3.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rand.h
e_des3.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
e_des3.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_des3.o: ../../include/openssl/ui.h ../../include/openssl/ui_compat.h
e_des3.o: ../cryptlib.h ../sgx.h e_des3.c evp_locl.h
e_idea.o: ../../e_os.h ../../include/openssl/../sgx.h
e_idea.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_idea.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_idea.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_idea.o: ../../include/openssl/evp.h ../../include/openssl/idea.h
e_idea.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
e_idea.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
e_idea.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_idea.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_idea.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h e_idea.c
e_idea.o: evp_locl.h
e_null.o: ../../e_os.h ../../include/openssl/../sgx.h
e_null.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_null.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_null.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_null.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
e_null.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_null.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_null.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
e_null.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
e_null.o: ../cryptlib.h ../sgx.h e_null.c
e_old.o: e_old.c
e_rc2.o: ../../e_os.h ../../include/openssl/../sgx.h
e_rc2.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_rc2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_rc2.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_rc2.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
e_rc2.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_rc2.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_rc2.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rc2.h
e_rc2.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_rc2.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h e_rc2.c
e_rc2.o: evp_locl.h
e_rc4.o: ../../e_os.h ../../include/openssl/../sgx.h
e_rc4.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_rc4.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_rc4.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_rc4.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
e_rc4.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_rc4.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_rc4.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rc4.h
e_rc4.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_rc4.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h e_rc4.c
e_rc4.o: evp_locl.h
e_rc4_hmac_md5.o: ../../include/openssl/../sgx.h ../../include/openssl/asn1.h
e_rc4_hmac_md5.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
e_rc4_hmac_md5.o: ../../include/openssl/e_os2.h ../../include/openssl/evp.h
e_rc4_hmac_md5.o: ../../include/openssl/md5.h ../../include/openssl/obj_mac.h
e_rc4_hmac_md5.o: ../../include/openssl/objects.h
e_rc4_hmac_md5.o: ../../include/openssl/opensslconf.h
e_rc4_hmac_md5.o: ../../include/openssl/opensslv.h
e_rc4_hmac_md5.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rc4.h
e_rc4_hmac_md5.o: ../../include/openssl/safestack.h
e_rc4_hmac_md5.o: ../../include/openssl/stack.h
e_rc4_hmac_md5.o: ../../include/openssl/symhacks.h ../sgx.h e_rc4_hmac_md5.c
e_rc5.o: ../../e_os.h ../../include/openssl/bio.h
e_rc5.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_rc5.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_rc5.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
e_rc5.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_rc5.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_rc5.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h e_rc5.c
e_seed.o: ../../include/openssl/../sgx.h ../../include/openssl/asn1.h
e_seed.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
e_seed.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_seed.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
e_seed.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_seed.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
e_seed.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
e_seed.o: ../../include/openssl/seed.h ../../include/openssl/stack.h
e_seed.o: ../../include/openssl/symhacks.h e_seed.c evp_locl.h
e_xcbc_d.o: ../../e_os.h ../../include/openssl/../sgx.h
e_xcbc_d.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
e_xcbc_d.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
e_xcbc_d.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
e_xcbc_d.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
e_xcbc_d.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
e_xcbc_d.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
e_xcbc_d.o: ../../include/openssl/opensslconf.h
e_xcbc_d.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
e_xcbc_d.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
e_xcbc_d.o: ../../include/openssl/symhacks.h ../../include/openssl/ui.h
e_xcbc_d.o: ../../include/openssl/ui_compat.h ../cryptlib.h ../sgx.h e_xcbc_d.c
e_xcbc_d.o: evp_locl.h
encode.o: ../../e_os.h ../../include/openssl/../sgx.h
encode.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
encode.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
encode.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
encode.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
encode.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
encode.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
encode.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
encode.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
encode.o: ../cryptlib.h ../sgx.h encode.c
evp_acnf.o: ../../e_os.h ../../include/openssl/../sgx.h
evp_acnf.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
evp_acnf.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
evp_acnf.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
evp_acnf.o: ../../include/openssl/err.h ../../include/openssl/evp.h
evp_acnf.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_acnf.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_acnf.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_acnf.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
evp_acnf.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h evp_acnf.c
evp_cnf.o: ../../e_os.h ../../include/openssl/../sgx.h
evp_cnf.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
evp_cnf.o: ../../include/openssl/buffer.h ../../include/openssl/conf.h
evp_cnf.o: ../../include/openssl/crypto.h ../../include/openssl/dso.h
evp_cnf.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
evp_cnf.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
evp_cnf.o: ../../include/openssl/err.h ../../include/openssl/evp.h
evp_cnf.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_cnf.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_cnf.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_cnf.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
evp_cnf.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
evp_cnf.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
evp_cnf.o: ../../include/openssl/x509_vfy.h ../../include/openssl/x509v3.h
evp_cnf.o: ../cryptlib.h ../sgx.h evp_cnf.c
evp_enc.o: ../../e_os.h ../../include/openssl/../sgx.h
evp_enc.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
evp_enc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
evp_enc.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
evp_enc.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
evp_enc.o: ../../include/openssl/engine.h ../../include/openssl/err.h
evp_enc.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
evp_enc.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
evp_enc.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
evp_enc.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
evp_enc.o: ../../include/openssl/rand.h ../../include/openssl/safestack.h
evp_enc.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
evp_enc.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
evp_enc.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h evp_enc.c
evp_enc.o: evp_locl.h
evp_err.o: ../../include/openssl/../sgx.h ../../include/openssl/asn1.h
evp_err.o: ../../include/openssl/bio.h ../../include/openssl/crypto.h
evp_err.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
evp_err.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
evp_err.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
evp_err.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
evp_err.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
evp_err.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
evp_err.o: evp_err.c
evp_key.o: ../../e_os.h ../../include/openssl/../sgx.h
evp_key.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
evp_key.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
evp_key.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
evp_key.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
evp_key.o: ../../include/openssl/err.h ../../include/openssl/evp.h
evp_key.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_key.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_key.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_key.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
evp_key.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
evp_key.o: ../../include/openssl/symhacks.h ../../include/openssl/ui.h
evp_key.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
evp_key.o: ../cryptlib.h ../sgx.h evp_key.c
evp_lib.o: ../../e_os.h ../../include/openssl/../sgx.h
evp_lib.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
evp_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
evp_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
evp_lib.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
evp_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
evp_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
evp_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/safestack.h
evp_lib.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
evp_lib.o: ../cryptlib.h ../sgx.h evp_lib.c
evp_pbe.o: ../../e_os.h ../../include/openssl/../sgx.h
evp_pbe.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
evp_pbe.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
evp_pbe.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
evp_pbe.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
evp_pbe.o: ../../include/openssl/err.h ../../include/openssl/evp.h
evp_pbe.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_pbe.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_pbe.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_pbe.o: ../../include/openssl/pkcs12.h ../../include/openssl/pkcs7.h
evp_pbe.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
evp_pbe.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
evp_pbe.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
evp_pbe.o: ../cryptlib.h ../sgx.h evp_locl.h evp_pbe.c
evp_pkey.o: ../../e_os.h ../../include/openssl/../sgx.h
evp_pkey.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
evp_pkey.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
evp_pkey.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
evp_pkey.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
evp_pkey.o: ../../include/openssl/err.h ../../include/openssl/evp.h
evp_pkey.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
evp_pkey.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
evp_pkey.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
evp_pkey.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
evp_pkey.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
evp_pkey.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
evp_pkey.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
evp_pkey.o: ../asn1/asn1_locl.h ../cryptlib.h ../sgx.h evp_pkey.c
m_dss.o: ../../e_os.h ../../include/openssl/../sgx.h
m_dss.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_dss.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_dss.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
m_dss.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_dss.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_dss.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_dss.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_dss.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
m_dss.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
m_dss.o: ../cryptlib.h ../sgx.h m_dss.c
m_dss1.o: ../../e_os.h ../../include/openssl/../sgx.h
m_dss1.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_dss1.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_dss1.o: ../../include/openssl/dsa.h ../../include/openssl/e_os2.h
m_dss1.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_dss1.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_dss1.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_dss1.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_dss1.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
m_dss1.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
m_dss1.o: ../cryptlib.h ../sgx.h m_dss1.c
m_ecdsa.o: ../../e_os.h ../../include/openssl/../sgx.h
m_ecdsa.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_ecdsa.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_ecdsa.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_ecdsa.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_ecdsa.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_ecdsa.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_ecdsa.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_ecdsa.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_ecdsa.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
m_ecdsa.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_ecdsa.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_ecdsa.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h m_ecdsa.c
m_md2.o: ../../e_os.h ../../include/openssl/bio.h
m_md2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_md2.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
m_md2.o: ../../include/openssl/lhash.h ../../include/openssl/opensslconf.h
m_md2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_md2.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
m_md2.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h m_md2.c
m_md4.o: ../../e_os.h ../../include/openssl/../sgx.h
m_md4.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_md4.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_md4.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_md4.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_md4.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_md4.o: ../../include/openssl/lhash.h ../../include/openssl/md4.h
m_md4.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
m_md4.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
m_md4.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
m_md4.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
m_md4.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_md4.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_md4.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h evp_locl.h
m_md4.o: m_md4.c
m_md5.o: ../../e_os.h ../../include/openssl/../sgx.h
m_md5.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_md5.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_md5.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_md5.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_md5.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_md5.o: ../../include/openssl/lhash.h ../../include/openssl/md5.h
m_md5.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
m_md5.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
m_md5.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
m_md5.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
m_md5.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_md5.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_md5.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h evp_locl.h
m_md5.o: m_md5.c
m_mdc2.o: ../../e_os.h ../../include/openssl/../sgx.h
m_mdc2.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_mdc2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_mdc2.o: ../../include/openssl/des.h ../../include/openssl/des_old.h
m_mdc2.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_mdc2.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_mdc2.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_mdc2.o: ../../include/openssl/lhash.h ../../include/openssl/mdc2.h
m_mdc2.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
m_mdc2.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
m_mdc2.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
m_mdc2.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
m_mdc2.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_mdc2.o: ../../include/openssl/symhacks.h ../../include/openssl/ui.h
m_mdc2.o: ../../include/openssl/ui_compat.h ../../include/openssl/x509.h
m_mdc2.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h evp_locl.h
m_mdc2.o: m_mdc2.c
m_null.o: ../../e_os.h ../../include/openssl/../sgx.h
m_null.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_null.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_null.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_null.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_null.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_null.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_null.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_null.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_null.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
m_null.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_null.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_null.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h m_null.c
m_ripemd.o: ../../e_os.h ../../include/openssl/../sgx.h
m_ripemd.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_ripemd.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_ripemd.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_ripemd.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_ripemd.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_ripemd.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_ripemd.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_ripemd.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_ripemd.o: ../../include/openssl/pkcs7.h ../../include/openssl/ripemd.h
m_ripemd.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
m_ripemd.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_ripemd.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_ripemd.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h evp_locl.h
m_ripemd.o: m_ripemd.c
m_sha.o: ../../e_os.h ../../include/openssl/../sgx.h
m_sha.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_sha.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_sha.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_sha.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_sha.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_sha.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_sha.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_sha.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_sha.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
m_sha.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
m_sha.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
m_sha.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
m_sha.o: ../cryptlib.h ../sgx.h evp_locl.h m_sha.c
m_sha1.o: ../../e_os.h ../../include/openssl/../sgx.h
m_sha1.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_sha1.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_sha1.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
m_sha1.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
m_sha1.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
m_sha1.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
m_sha1.o: ../../include/openssl/ossl_typ.h ../../include/openssl/rsa.h
m_sha1.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
m_sha1.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
m_sha1.o: ../cryptlib.h ../sgx.h m_sha1.c
m_sigver.o: ../../e_os.h ../../include/openssl/../sgx.h
m_sigver.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_sigver.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_sigver.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_sigver.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_sigver.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_sigver.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_sigver.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_sigver.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_sigver.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
m_sigver.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_sigver.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
m_sigver.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h evp_locl.h
m_sigver.o: m_sigver.c
m_wp.o: ../../e_os.h ../../include/openssl/../sgx.h
m_wp.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
m_wp.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
m_wp.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
m_wp.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
m_wp.o: ../../include/openssl/err.h ../../include/openssl/evp.h
m_wp.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
m_wp.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
m_wp.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
m_wp.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
m_wp.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
m_wp.o: ../../include/openssl/symhacks.h ../../include/openssl/whrlpool.h
m_wp.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
m_wp.o: ../cryptlib.h ../sgx.h evp_locl.h m_wp.c
names.o: ../../e_os.h ../../include/openssl/../sgx.h
names.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
names.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
names.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
names.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
names.o: ../../include/openssl/err.h ../../include/openssl/evp.h
names.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
names.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
names.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
names.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
names.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
names.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
names.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h names.c
p5_crpt.o: ../../e_os.h ../../include/openssl/../sgx.h
p5_crpt.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
p5_crpt.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p5_crpt.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p5_crpt.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p5_crpt.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p5_crpt.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p5_crpt.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p5_crpt.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p5_crpt.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
p5_crpt.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p5_crpt.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p5_crpt.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h p5_crpt.c
p5_crpt2.o: ../../e_os.h ../../include/openssl/../sgx.h
p5_crpt2.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
p5_crpt2.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p5_crpt2.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p5_crpt2.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p5_crpt2.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p5_crpt2.o: ../../include/openssl/hmac.h ../../include/openssl/lhash.h
p5_crpt2.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
p5_crpt2.o: ../../include/openssl/opensslconf.h
p5_crpt2.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p5_crpt2.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
p5_crpt2.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p5_crpt2.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p5_crpt2.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h evp_locl.h
p5_crpt2.o: p5_crpt2.c
p_dec.o: ../../e_os.h ../../include/openssl/../sgx.h
p_dec.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_dec.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_dec.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_dec.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_dec.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_dec.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p_dec.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p_dec.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p_dec.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
p_dec.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
p_dec.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p_dec.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p_dec.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h p_dec.c
p_enc.o: ../../e_os.h ../../include/openssl/../sgx.h
p_enc.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_enc.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_enc.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_enc.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_enc.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_enc.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p_enc.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p_enc.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p_enc.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
p_enc.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
p_enc.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p_enc.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p_enc.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h p_enc.c
p_lib.o: ../../e_os.h ../../include/openssl/../sgx.h
p_lib.o: ../../include/openssl/asn1.h ../../include/openssl/asn1_mac.h
p_lib.o: ../../include/openssl/bio.h ../../include/openssl/bn.h
p_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_lib.o: ../../include/openssl/dh.h ../../include/openssl/dsa.h
p_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_lib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_lib.o: ../../include/openssl/engine.h ../../include/openssl/err.h
p_lib.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
p_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
p_lib.o: ../../include/openssl/opensslconf.h ../../include/openssl/opensslv.h
p_lib.o: ../../include/openssl/ossl_typ.h ../../include/openssl/pkcs7.h
p_lib.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
p_lib.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p_lib.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p_lib.o: ../../include/openssl/x509_vfy.h ../asn1/asn1_locl.h ../cryptlib.h
p_lib.o: ../sgx.h p_lib.c
p_open.o: ../../e_os.h ../../include/openssl/../sgx.h
p_open.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_open.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_open.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_open.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_open.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_open.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p_open.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p_open.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p_open.o: ../../include/openssl/pkcs7.h ../../include/openssl/rsa.h
p_open.o: ../../include/openssl/safestack.h ../../include/openssl/sha.h
p_open.o: ../../include/openssl/stack.h ../../include/openssl/symhacks.h
p_open.o: ../../include/openssl/x509.h ../../include/openssl/x509_vfy.h
p_open.o: ../cryptlib.h ../sgx.h p_open.c
p_seal.o: ../../e_os.h ../../include/openssl/../sgx.h
p_seal.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_seal.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_seal.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_seal.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_seal.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_seal.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p_seal.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p_seal.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p_seal.o: ../../include/openssl/pkcs7.h ../../include/openssl/rand.h
p_seal.o: ../../include/openssl/rsa.h ../../include/openssl/safestack.h
p_seal.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p_seal.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p_seal.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h p_seal.c
p_sign.o: ../../e_os.h ../../include/openssl/../sgx.h
p_sign.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_sign.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_sign.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_sign.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_sign.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_sign.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p_sign.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p_sign.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p_sign.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
p_sign.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p_sign.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p_sign.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h p_sign.c
p_verify.o: ../../e_os.h ../../include/openssl/../sgx.h
p_verify.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
p_verify.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
p_verify.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
p_verify.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
p_verify.o: ../../include/openssl/err.h ../../include/openssl/evp.h
p_verify.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
p_verify.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
p_verify.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
p_verify.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
p_verify.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
p_verify.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
p_verify.o: ../../include/openssl/x509_vfy.h ../cryptlib.h ../sgx.h p_verify.c
pmeth_fn.o: ../../e_os.h ../../include/openssl/../sgx.h
pmeth_fn.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pmeth_fn.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pmeth_fn.o: ../../include/openssl/e_os2.h ../../include/openssl/err.h
pmeth_fn.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
pmeth_fn.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pmeth_fn.o: ../../include/openssl/opensslconf.h
pmeth_fn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pmeth_fn.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
pmeth_fn.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h evp_locl.h
pmeth_fn.o: pmeth_fn.c
pmeth_gn.o: ../../e_os.h ../../include/openssl/../sgx.h
pmeth_gn.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pmeth_gn.o: ../../include/openssl/bn.h ../../include/openssl/buffer.h
pmeth_gn.o: ../../include/openssl/crypto.h ../../include/openssl/e_os2.h
pmeth_gn.o: ../../include/openssl/err.h ../../include/openssl/evp.h
pmeth_gn.o: ../../include/openssl/lhash.h ../../include/openssl/obj_mac.h
pmeth_gn.o: ../../include/openssl/objects.h ../../include/openssl/opensslconf.h
pmeth_gn.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pmeth_gn.o: ../../include/openssl/safestack.h ../../include/openssl/stack.h
pmeth_gn.o: ../../include/openssl/symhacks.h ../cryptlib.h ../sgx.h evp_locl.h
pmeth_gn.o: pmeth_gn.c
pmeth_lib.o: ../../e_os.h ../../include/openssl/../sgx.h
pmeth_lib.o: ../../include/openssl/asn1.h ../../include/openssl/bio.h
pmeth_lib.o: ../../include/openssl/buffer.h ../../include/openssl/crypto.h
pmeth_lib.o: ../../include/openssl/e_os2.h ../../include/openssl/ec.h
pmeth_lib.o: ../../include/openssl/ecdh.h ../../include/openssl/ecdsa.h
pmeth_lib.o: ../../include/openssl/engine.h ../../include/openssl/err.h
pmeth_lib.o: ../../include/openssl/evp.h ../../include/openssl/lhash.h
pmeth_lib.o: ../../include/openssl/obj_mac.h ../../include/openssl/objects.h
pmeth_lib.o: ../../include/openssl/opensslconf.h
pmeth_lib.o: ../../include/openssl/opensslv.h ../../include/openssl/ossl_typ.h
pmeth_lib.o: ../../include/openssl/pkcs7.h ../../include/openssl/safestack.h
pmeth_lib.o: ../../include/openssl/sha.h ../../include/openssl/stack.h
pmeth_lib.o: ../../include/openssl/symhacks.h ../../include/openssl/x509.h
pmeth_lib.o: ../../include/openssl/x509_vfy.h ../asn1/asn1_locl.h ../cryptlib.h
pmeth_lib.o: ../sgx.h evp_locl.h pmeth_lib.c
