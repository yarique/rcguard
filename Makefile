PROG=	supervise
MAN=	supervise.8

DPADD=  ${LIBUTIL}
LDADD=  -lutil

BINDIR?=	/sbin

WARNS?=	100

.include <bsd.prog.mk>
