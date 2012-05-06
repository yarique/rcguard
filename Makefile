PROG=	rcguard
MAN=	rcguard.8

DPADD=  ${LIBUTIL}
LDADD=  -lutil

BINDIR?=	/libexec

# To be removed if accepted for the FreeBSD repo
WARNS?=	100

.include <bsd.prog.mk>
