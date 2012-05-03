PROG=	supervise
MAN=	supervise.8

DPADD=  ${LIBUTIL}
LDADD=  -lutil

# To be removed if accepted for the FreeBSD repo
BINDIR?=	/sbin
WARNS?=	100

.include <bsd.prog.mk>
